/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 5.1 2022/11/13 15:25:52 kls Exp $
 */

#include "thread.h"
#include <cxxabi.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <linux/unistd.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tools.h"

#define ABORT { dsyslog("ABORT!"); cBackTrace::BackTrace(); abort(); }

//#define DEBUG_LOCKING  // uncomment this line to activate debug output for locking
#define DEBUG_LOCKSEQ  // uncomment this line to activate debug output for invalid locking sequence
//#define DEBUG_LOCKCALL // uncomment this line to activate caller information with DEBUG_LOCKSEQ (WARNING: expensive operation, use only when actually debugging the locking sequence!)

#ifdef DEBUG_LOCKING
#define dbglocking(a...) fprintf(stderr, a)
#else
#define dbglocking(a...)
#endif

static bool GetAbsTime(struct timespec *Abstime, int MillisecondsFromNow)
{
  struct timeval now;
  if (gettimeofday(&now, NULL) == 0) {           // get current time
     now.tv_sec  += MillisecondsFromNow / 1000;  // add full seconds
     now.tv_usec += (MillisecondsFromNow % 1000) * 1000;  // add microseconds
     if (now.tv_usec >= 1000000) {               // take care of an overflow
        now.tv_sec++;
        now.tv_usec -= 1000000;
        }
     Abstime->tv_sec = now.tv_sec;          // seconds
     Abstime->tv_nsec = now.tv_usec * 1000; // nano seconds
     return true;
     }
  return false;
}

// --- cCondWait -------------------------------------------------------------

cCondWait::cCondWait(void)
{
  signaled = false;
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
}

cCondWait::~cCondWait()
{
  pthread_cond_broadcast(&cond); // wake up any sleepers
  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);
}

void cCondWait::SleepMs(int TimeoutMs)
{
  cCondWait w;
  w.Wait(max(TimeoutMs, 3)); // making sure the time is >2ms to avoid a possible busy wait
}

bool cCondWait::Wait(int TimeoutMs)
{
  pthread_mutex_lock(&mutex);
  if (!signaled) {
     if (TimeoutMs) {
        struct timespec abstime;
        if (GetAbsTime(&abstime, TimeoutMs)) {
           while (!signaled) {
                 if (pthread_cond_timedwait(&cond, &mutex, &abstime) == ETIMEDOUT)
                    break;
                 }
           }
        }
     else
        pthread_cond_wait(&cond, &mutex);
     }
  bool r = signaled;
  signaled = false;
  pthread_mutex_unlock(&mutex);
  return r;
}

void cCondWait::Signal(void)
{
  pthread_mutex_lock(&mutex);
  signaled = true;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mutex);
}

// --- cCondVar --------------------------------------------------------------

cCondVar::cCondVar(void)
{
  pthread_cond_init(&cond, 0);
}

cCondVar::~cCondVar()
{
  pthread_cond_broadcast(&cond); // wake up any sleepers
  pthread_cond_destroy(&cond);
}

void cCondVar::Wait(cMutex &Mutex)
{
  if (Mutex.locked) {
     int locked = Mutex.locked;
     Mutex.locked = 0; // have to clear the locked count here, as pthread_cond_wait
                       // does an implicit unlock of the mutex
     pthread_cond_wait(&cond, &Mutex.mutex);
     Mutex.locked = locked;
     }
}

bool cCondVar::TimedWait(cMutex &Mutex, int TimeoutMs)
{
  bool r = true; // true = condition signaled, false = timeout

  if (Mutex.locked) {
     struct timespec abstime;
     if (GetAbsTime(&abstime, TimeoutMs)) {
        int locked = Mutex.locked;
        Mutex.locked = 0; // have to clear the locked count here, as pthread_cond_timedwait
                          // does an implicit unlock of the mutex.
        if (pthread_cond_timedwait(&cond, &Mutex.mutex, &abstime) == ETIMEDOUT)
           r = false;
        Mutex.locked = locked;
        }
     }
  return r;
}

void cCondVar::Broadcast(void)
{
  pthread_cond_broadcast(&cond);
}

// --- cRwLock ---------------------------------------------------------------

cRwLock::cRwLock(bool PreferWriter)
{
  locked = 0;
  writeLockThreadId = 0;
  pthread_rwlockattr_t attr;
  pthread_rwlockattr_init(&attr);
  pthread_rwlockattr_setkind_np(&attr, PreferWriter ? PTHREAD_RWLOCK_PREFER_WRITER_NP : PTHREAD_RWLOCK_PREFER_READER_NP);
  pthread_rwlock_init(&rwlock, &attr);
}

cRwLock::~cRwLock()
{
  pthread_rwlock_destroy(&rwlock);
}

bool cRwLock::Lock(bool Write, int TimeoutMs)
{
  int Result = 0;
  struct timespec abstime;
  if (TimeoutMs) {
     if (!GetAbsTime(&abstime, TimeoutMs))
        TimeoutMs = 0;
     }
  if (Write) {
     Result = TimeoutMs ? pthread_rwlock_timedwrlock(&rwlock, &abstime) : pthread_rwlock_wrlock(&rwlock);
     if (Result == 0)
        writeLockThreadId = cThread::ThreadId();
     }
  else if (writeLockThreadId == cThread::ThreadId()) {
     locked++; // there can be any number of stacked read locks, so we keep track here
     Result = 0; // acquiring a read lock while holding a write lock within the same thread is OK
     }
  else
     Result = TimeoutMs ? pthread_rwlock_timedrdlock(&rwlock, &abstime) : pthread_rwlock_rdlock(&rwlock);
  return Result == 0;
}

void cRwLock::Unlock(void)
{
  if (writeLockThreadId == cThread::ThreadId()) { // this is the thread that obtained the initial write lock
     if (locked) { // this is the unlock of a read lock within the write lock
        locked--;
        return;
        }
     }
  writeLockThreadId = 0;
  pthread_rwlock_unlock(&rwlock);
}

// --- cMutex ----------------------------------------------------------------

cMutex::cMutex(void)
{
  locked = 0;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
  pthread_mutex_init(&mutex, &attr);
}

cMutex::~cMutex()
{
  pthread_mutex_destroy(&mutex);
}

void cMutex::Lock(void)
{
  pthread_mutex_lock(&mutex);
  locked++;
}

void cMutex::Unlock(void)
{
  if (!--locked)
     pthread_mutex_unlock(&mutex);
}

// --- cThread ---------------------------------------------------------------

tThreadId cThread::mainThreadId = 0;

cThread::cThread(const char *Description, bool LowPriority)
{
  active = running = false;
  childTid = 0;
  childThreadId = 0;
  description = NULL;
  if (Description)
     SetDescription("%s", Description);
  lowPriority = LowPriority;
}

cThread::~cThread()
{
  Cancel(); // just in case the derived class didn't call it
  free(description);
}

void cThread::SetPriority(int Priority)
{
  if (setpriority(PRIO_PROCESS, 0, Priority) < 0)
     LOG_ERROR;
}

void cThread::SetIOPriority(int Priority)
{
  if (syscall(SYS_ioprio_set, 1, 0, (Priority & 0xff) | (3 << 13)) < 0) // idle class
     LOG_ERROR;
}

void cThread::SetDescription(const char *Description, ...)
{
  free(description);
  description = NULL;
  if (Description) {
     va_list ap;
     va_start(ap, Description);
     description = strdup(cString::vsprintf(Description, ap));
     va_end(ap);
     }
}

void *cThread::StartThread(cThread *Thread)
{
  Thread->childThreadId = ThreadId();
  if (Thread->description) {
     dsyslog("%s thread started (pid=%d, tid=%d, prio=%s)", Thread->description, getpid(), Thread->childThreadId, Thread->lowPriority ? "low" : "high");
#ifdef PR_SET_NAME
     if (prctl(PR_SET_NAME, Thread->description, 0, 0, 0) < 0)
        esyslog("%s thread naming failed (pid=%d, tid=%d)", Thread->description, getpid(), Thread->childThreadId);
#endif
     }
  if (Thread->lowPriority) {
     Thread->SetPriority(19);
     Thread->SetIOPriority(7);
     }
  Thread->Action();
  if (Thread->description)
     dsyslog("%s thread ended (pid=%d, tid=%d)", Thread->description, getpid(), Thread->childThreadId);
  Thread->running = false;
  Thread->active = false;
  return NULL;
}

#define THREAD_STOP_TIMEOUT  3000 // ms to wait for a thread to stop before newly starting it
#define THREAD_STOP_SLEEP      30 // ms to sleep while waiting for a thread to stop

bool cThread::Start(void)
{
  if (!running) {
     if (active) {
        // Wait until the previous incarnation of this thread has completely ended
        // before starting it newly:
        cTimeMs RestartTimeout;
        while (!running && active && RestartTimeout.Elapsed() < THREAD_STOP_TIMEOUT)
              cCondWait::SleepMs(THREAD_STOP_SLEEP);
        }
     if (!active) {
        active = running = true;
        if (pthread_create(&childTid, NULL, (void *(*) (void *))&StartThread, (void *)this) == 0) {
           pthread_detach(childTid); // auto-reap
           }
        else {
           LOG_ERROR;
           active = running = false;
           return false;
           }
        }
     }
  return true;
}

bool cThread::Active(void)
{
  if (active) {
     //
     // Single UNIX Spec v2 says:
     //
     // The pthread_kill() function is used to request
     // that a signal be delivered to the specified thread.
     //
     // As in kill(), if sig is zero, error checking is
     // performed but no signal is actually sent.
     //
     int err;
     if ((err = pthread_kill(childTid, 0)) != 0) {
        if (err != ESRCH)
           LOG_ERROR;
        childTid = 0;
        active = running = false;
        }
     else
        return true;
     }
  return false;
}

void cThread::Cancel(int WaitSeconds)
{
  running = false;
  if (active && WaitSeconds > -1) {
     if (WaitSeconds > 0) {
        for (time_t t0 = time(NULL) + WaitSeconds; time(NULL) < t0; ) {
            if (!Active())
               return;
            cCondWait::SleepMs(10);
            }
        esyslog("ERROR: %s thread %d won't end (waited %d seconds) - canceling it...", description ? description : "", childThreadId, WaitSeconds);
        }
     pthread_cancel(childTid);
     childTid = 0;
     active = false;
     }
}

tThreadId cThread::ThreadId(void)
{
  return syscall(__NR_gettid);
}

void cThread::SetMainThreadId(void)
{
  if (mainThreadId == 0)
     mainThreadId = ThreadId();
  else
     esyslog("ERROR: attempt to set main thread id to %d while it already is %d", ThreadId(), mainThreadId);
}

// --- cMutexLock ------------------------------------------------------------

cMutexLock::cMutexLock(cMutex *Mutex)
{
  mutex = NULL;
  locked = false;
  Lock(Mutex);
}

cMutexLock::~cMutexLock()
{
  if (mutex && locked)
     mutex->Unlock();
}

bool cMutexLock::Lock(cMutex *Mutex)
{
  if (Mutex && !mutex) {
     mutex = Mutex;
     Mutex->Lock();
     locked = true;
     return true;
     }
  return false;
}

// --- cThreadLock -----------------------------------------------------------

cThreadLock::cThreadLock(cThread *Thread)
{
  thread = NULL;
  locked = false;
  Lock(Thread);
}

cThreadLock::~cThreadLock()
{
  if (thread && locked)
     thread->Unlock();
}

bool cThreadLock::Lock(cThread *Thread)
{
  if (Thread && !thread) {
     thread = Thread;
     Thread->Lock();
     locked = true;
     return true;
     }
  return false;
}

// --- cBackTrace ------------------------------------------------------------

#define BT_BUF_SIZE 100

cString cBackTrace::Demangle(char *s)
{
  char *Module = s;
  char *Function = NULL;
  char *Offset = NULL;
  char *Address = NULL;
  // separate the string:
  for (char *q = Module; *q; q++) {
      if (*q == '(') {
         *q = 0;
         Function = q + 1;
         }
      else if (*q == '+') {
         *q = 0;
         Offset = q + 1;
         }
      else if (*q == ')')
         *q = 0;
      else if (*q == '[')
         Address = q + 1;
      else if (*q == ']') {
         *q = 0;
         break;
         }
      }
  // demangle the function name:
  char *DemangledFunction = NULL;
  if (Function) {
     int status;
     DemangledFunction = abi::__cxa_demangle(Function, NULL, 0, &status);
     if (DemangledFunction)
        Function = DemangledFunction;
     if (!*Function)
        Function = NULL;
     }
  cString d = cString::sprintf("%s%s%s", Module, Function ? " " : "", Function ? Function : "");
  // convert string address to numbers:
  unsigned long long addr = Address ? strtoull(Address, NULL, 0) : 0;
  unsigned long long offs = Offset ? strtoull(Offset, NULL, 0) : 0;
  // for shared libraries we need get the offset inside the library:
  if (Function) {
     // check whether the module name ends with ".so*":
     char *e = Module;
     char *p = NULL;
     while (e = strstr(e, ".so"))
           p = e++;
     if (p && !strchr(p, '/')) {
        Dl_info dlinfo;
        if (dladdr(reinterpret_cast<void*>(addr), &dlinfo)) {
           if ((strcmp(Module, dlinfo.dli_fname) == 0) && dlinfo.dli_fbase) {
              unsigned long long base = reinterpret_cast<unsigned long long>(dlinfo.dli_fbase);
              addr -= base;
              addr &= 0x0FFFFFFFF; // to make it work on both 32 and 64 bit systems
              }
           }
        }
     }
  // determine the file name and line number:
  cString cmd = cString::sprintf("addr2line --functions --demangle --inlines --basename --exe=%s 0x%llx", Module, Function ? addr : offs);
  cPipe p;
  if (p.Open(cmd, "r")) {
     int n = 0;
     cReadLine rl;
     while (char *l = rl.Read(p)) {
           if (n == 0) {
              if (Function && strcmp(l, Function))
                 d = cString::sprintf("%s calling %s", *d, l);
              }
           else
              d = cString::sprintf("%s at %s", *d, l);
           n++;
           }
     p.Close();
     }
  free(DemangledFunction);
  return d;
}

void cBackTrace::BackTrace(cStringList &StringList, int Level, bool Mangled)
{
  void *b[BT_BUF_SIZE];
  int n = backtrace(b, BT_BUF_SIZE);
  if (char **s = backtrace_symbols(b, n)) {
     for (int i = max(Level, 0) + 1; i < n; i++) // 1 is the call to this function itself
         StringList.Append(strdup(Mangled ? s[i] : *Demangle(s[i])));
     free(s);
     }
}

void cBackTrace::BackTrace(FILE *f, int Level, bool Mangled)
{
  cStringList sl;
  BackTrace(sl, Level + 1, Mangled); // 1 is the call to this function itself
  for (int i = 0; i < sl.Size(); i++) {
      if (f)
         fprintf(f, "%s\n", sl[i]);
      else
         dsyslog("%s", sl[i]);
      }
}

cString cBackTrace::GetCaller(int Level, bool Mangled)
{
  cString Caller;
  Level = max(Level, 0) + 1; // 1 is the call to this function itself
  void *b[BT_BUF_SIZE];
  int n = backtrace(b, BT_BUF_SIZE);
  if (char **s = backtrace_symbols(b, n)) {
     if (Level < n)
        Caller = Mangled ? s[Level] : *Demangle(s[Level]);
     free(s);
     }
  return Caller;
}

// --- cStateLockLog ---------------------------------------------------------

#ifdef DEBUG_LOCKSEQ
#define SLL_SIZE     20 // the number of log entries
#define SLL_LENGTH  512 // the maximum length of log entries
#define SLL_THREADS  20 // the maximum number of threads holding locks at the same time (typically well below 10)
#define SLL_MAX_LIST  9 // max. number of lists to log
#define SLL_WRITE_FLAG 0x80000000
#define SLL_LOCK_FLAG  0x40000000

class cStateLockLog {
private:
  cMutex mutex;
  cVector<tThreadId> threadIds;
  cVector<int> flags;
  tThreadId logThreadIds[SLL_SIZE];
  int logFlags[SLL_SIZE];
  uint8_t logCounter[SLL_THREADS][SLL_MAX_LIST];
#ifdef DEBUG_LOCKCALL
  char logCaller[SLL_SIZE][SLL_LENGTH];
#endif
  int logIndex;
  bool dumped;
  void Dump(const char *Name, tThreadId ThreadId);
public:
  cStateLockLog(void);
  void Check(const char *Name, bool Lock, bool Write = false);
  };

cStateLockLog::cStateLockLog(void)
{
  memset(logThreadIds, 0, sizeof(logThreadIds));
  memset(logFlags, 0, sizeof(logFlags));
  memset(logCounter, 0, sizeof(logCounter));
#ifdef DEBUG_LOCKCALL
  memset(logCaller, 0, sizeof(logCaller));
#endif
  logIndex = 0;
  dumped = false;
}

void cStateLockLog::Dump(const char *Name, tThreadId ThreadId)
{
  dsyslog("--- begin invalid lock sequence report");
  int LastFlags = 0;
  for (int i = 0; i < SLL_SIZE; i++) {
      if (tThreadId tid = logThreadIds[logIndex]) {
         char msg[SLL_LENGTH];
         char *q = msg;
         q += sprintf(q, "%5d", tid);
         int Flags = logFlags[logIndex];
         bool Write = Flags & SLL_WRITE_FLAG;
         bool Lock = Flags & SLL_LOCK_FLAG;
         Flags &= ~(SLL_WRITE_FLAG | SLL_LOCK_FLAG);
         int Changed = LastFlags ^ Flags;
         LastFlags = Flags;
         for (int i = 0; i <= SLL_MAX_LIST; i++) {
             char c = '-';
             int b = 1 << i;
             if ((Flags & b) != 0)
                c = '*';
             if ((Changed & b) != 0)
                c = Lock ? Write ? 'W' : 'R' : 'U';
             q += sprintf(q, "  %c", c);
             }
         q += sprintf(q, "  %c", Lock ? 'L' : 'U');
#ifdef DEBUG_LOCKCALL
         if (*logCaller[logIndex]) {
            *q++ = ' ';
            strn0cpy(q, *cBackTrace::Demangle(logCaller[logIndex]), sizeof(msg) - (q - msg));
            }
#endif
         dsyslog("%s", msg);
         }
      if (++logIndex >= SLL_SIZE)
         logIndex = 0;
      }
  dsyslog("%5d invalid lock sequence: %s", ThreadId, Name);
  dsyslog("full backtrace:");
  cBackTrace::BackTrace(NULL, 2);
  dsyslog("--- end invalid lock sequence report");
  dsyslog("--- THERE WILL BE NO FURTHER REPORTS UNTIL VDR IS RESTARTED!");
  fprintf(stderr, "invalid lock sequence at %s\n", *DayDateTime(time(NULL)));
}

void cStateLockLog::Check(const char *Name, bool Lock, bool Write)
{
  if (!dumped && Name) {
     int n = *Name - '0' - 1;
     if (0 <= n && n < SLL_MAX_LIST) {
        int b = 1 << n;
        cMutexLock MutexLock(&mutex);
        tThreadId ThreadId = cThread::ThreadId();
        int Index = -1;
        int AvailableIndex = -1;
        for (int i = 0; i < threadIds.Size(); i++) {
            if (ThreadId == threadIds[i]) {
               Index = i;
               break;
               }
            if (threadIds[i] == 0)
               AvailableIndex = i;
            }
        if (Index < 0) {
           if (AvailableIndex < 0) {
              Index = threadIds.Size();
              threadIds.Append(ThreadId);
              flags.Append(0);
              }
           else {
              Index = AvailableIndex;
              threadIds[Index] = ThreadId;
              }
           }
        if (Index >= SLL_THREADS) {
           // should never happen!
           esyslog("ERROR: too many threads holding list locks at the same time - stopped logging locks!");
           dumped = true;
           return;
           }
        bool DoDump = false;
        if (Lock) {
           if ((flags[Index] & ~b) < b) // thread holds only "smaller" locks -> OK
              ;
           else if ((flags[Index] & b) == 0) // thread already holds "bigger" locks, so it may only re-lock one that it already has!
              DoDump = true;
           logCounter[Index][n]++;
           flags[Index] |= b;
           }
        else if (--logCounter[Index][n] == 0)
           flags[Index] &= ~b;
        logThreadIds[logIndex] = ThreadId;
        logFlags[logIndex] = flags[Index] | (Write ? SLL_WRITE_FLAG : 0) | (Lock ? SLL_LOCK_FLAG : 0);
        if (flags[Index] == 0)
           threadIds[Index] = 0;
#ifdef DEBUG_LOCKCALL
        strn0cpy(logCaller[logIndex], cBackTrace::GetCaller(Lock ? 3 : 5, true), SLL_LENGTH);
#endif
        if (++logIndex >= SLL_SIZE)
           logIndex = 0;
        if (DoDump) {
           Dump(Name, ThreadId);
           dumped = true;
           }
        }
     }
}

static cStateLockLog StateLockLog;

#define dbglockseq(n, l, w) StateLockLog.Check(n, l, w)
#else
#define dbglockseq(n, l, w)
#endif // DEBUG_LOCKSEQ

// --- cStateLock ------------------------------------------------------------

cStateLock::cStateLock(const char *Name)
:rwLock(true)
{
  name = Name;
  threadId = 0;
  state = 0;
  explicitModify = emDisabled;
  syncStateKey = NULL;
}

bool cStateLock::Lock(cStateKey &StateKey, bool Write, int TimeoutMs)
{
  dbglocking("%5d %-12s %10p   lock state = %d/%d write = %d timeout = %d\n", cThread::ThreadId(), name, &StateKey, state, StateKey.state, Write, TimeoutMs);
  StateKey.timedOut = false;
  if (StateKey.stateLock) {
     esyslog("ERROR: StateKey already in use in call to cStateLock::Lock() (tid=%d, lock=%s)", StateKey.stateLock->threadId, name);
     ABORT;
     return false;
     }
  if (rwLock.Lock(Write, TimeoutMs)) {
     dbglockseq(name, true, Write);
     StateKey.stateLock = this;
     if (Write) {
        dbglocking("%5d %-12s %10p   locked write\n", cThread::ThreadId(), name, &StateKey);
        threadId = cThread::ThreadId();
        StateKey.write = true;
        return true;
        }
     else if (state != StateKey.state) {
        dbglocking("%5d %-12s %10p   locked read\n", cThread::ThreadId(), name, &StateKey);
        return true;
        }
     else {
        dbglocking("%5d %-12s %10p   state unchanged\n", cThread::ThreadId(), name, &StateKey);
        StateKey.stateLock = NULL;
        dbglockseq(name, false, false);
        rwLock.Unlock();
        }
     }
  else if (TimeoutMs) {
     dbglocking("%5d %-12s %10p   timeout\n", cThread::ThreadId(), name, &StateKey);
     StateKey.timedOut = true;
     }
  else if (threadId == cThread::ThreadId()) {
     static bool DoubleWriteLockReported = false;
     if (!DoubleWriteLockReported) {
        dsyslog("WARNING: attempt to acquire write lock while already holding a write lock in the same thread - this may crash! (backtrace follows)");
        cBackTrace::BackTrace();
        DoubleWriteLockReported = true;
        }
     }
  return false;
}

void cStateLock::Unlock(cStateKey &StateKey, bool IncState)
{
  dbglocking("%5d %-12s %10p unlock state = %d/%d inc = %d\n", cThread::ThreadId(), name, &StateKey, state, StateKey.state, IncState);
  if (StateKey.stateLock != this) {
     esyslog("ERROR: cStateLock::Unlock() called with an unused key (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  if (StateKey.write && threadId != cThread::ThreadId()) {
     esyslog("ERROR: cStateLock::Unlock() called without holding a write lock (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  if (StateKey.write && (IncState && explicitModify != emArmed || explicitModify == emEnabled)) {
     if (syncStateKey && syncStateKey->state == state)
        syncStateKey->state++;
     state++;
     }
  StateKey.state = state;
  StateKey.stateLock = NULL;
  if (StateKey.write) {
     StateKey.write = false;
     threadId = 0;
     explicitModify = emDisabled;
     syncStateKey = NULL;
     }
  dbglockseq(name, false, false);
  rwLock.Unlock();
}

void cStateLock::SetSyncStateKey(cStateKey &StateKey)
{
  dbglocking("%5d %-12s %10p SetSyncStateKey\n", cThread::ThreadId(), name, &StateKey);
  if (threadId != cThread::ThreadId()) {
     esyslog("ERROR: cStateLock::SetSyncStateKey() called without holding a write lock (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  if (StateKey.stateLock == this) {
     esyslog("ERROR: cStateLock::SetSyncStateKey() called with locked key (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  if (syncStateKey) {
     esyslog("ERROR: cStateLock::SetSyncStateKey() called twice (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  syncStateKey = &StateKey;
}

void cStateLock::SetExplicitModify(void)
{
  if (threadId != cThread::ThreadId()) {
     esyslog("ERROR: cStateLock::SetExplicitModify() called without holding a write lock (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  if (explicitModify != emDisabled) {
     esyslog("ERROR: cStateLock::SetExplicitModify() called twice (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  explicitModify = emArmed;
}

void cStateLock::SetModified(void)
{
  if (threadId != cThread::ThreadId()) {
     esyslog("ERROR: cStateLock::SetModified() called without holding a write lock (tid=%d, lock=%s)", threadId, name);
     ABORT;
     return;
     }
  explicitModify = emEnabled;
}

// --- cStateKey -------------------------------------------------------------

cStateKey::cStateKey(bool IgnoreFirst)
{
  stateLock = NULL;
  write = false;
  state = 0;
  if (!IgnoreFirst)
     Reset();
}

cStateKey::~cStateKey()
{
  if (stateLock) {
     esyslog("ERROR: cStateKey::~cStateKey() called without releasing the lock first (tid=%d, lock=%s, key=%p)", stateLock->threadId, stateLock->name, this);
     ABORT;
     }
}

void cStateKey::Reset(void)
{
  state = -1; // lock and key are initialized differently, to make the first check return true
}

void cStateKey::Remove(bool IncState)
{
  if (stateLock)
     stateLock->Unlock(*this, IncState);
  else {
     esyslog("ERROR: cStateKey::Remove() called without holding a lock (key=%p)", this);
     ABORT;
     }
}

bool cStateKey::StateChanged(void)
{
  if (!stateLock) {
     esyslog("ERROR: cStateKey::StateChanged() called without holding a lock (tid=%d, key=%p)", cThread::ThreadId(), this);
     ABORT;
     }
  else if (write)
     return state != stateLock->state;
  else
     return true;
}

// --- cIoThrottle -----------------------------------------------------------

cMutex cIoThrottle::mutex;
int cIoThrottle::count = 0;

cIoThrottle::cIoThrottle(void)
{
  active = false;
}

cIoThrottle::~cIoThrottle()
{
  Release();
}

void cIoThrottle::Activate(void)
{
  if (!active) {
     mutex.Lock();
     count++;
     active = true;
     dsyslog("i/o throttle activated, count = %d (tid=%d)", count, cThread::ThreadId());
     mutex.Unlock();
     }
}

void cIoThrottle::Release(void)
{
  if (active) {
     mutex.Lock();
     count--;
     active = false;
     dsyslog("i/o throttle released, count = %d (tid=%d)", count, cThread::ThreadId());
     mutex.Unlock();
     }
}

bool cIoThrottle::Engaged(void)
{
  return count > 0;
}

// --- cPipe -----------------------------------------------------------------

// cPipe::Open() and cPipe::Close() are based on code originally received from
// Andreas Vitting <Andreas@huji.de>

cPipe::cPipe(void)
{
  pid = -1;
  f = NULL;
}

cPipe::~cPipe()
{
  Close();
}

bool cPipe::Open(const char *Command, const char *Mode)
{
  int fd[2];

  if (pipe(fd) < 0) {
     LOG_ERROR;
     return false;
     }
  if ((pid = fork()) < 0) { // fork failed
     LOG_ERROR;
     close(fd[0]);
     close(fd[1]);
     return false;
     }

  const char *mode = "w";
  int iopipe = 0;

  if (pid > 0) { // parent process
     if (strcmp(Mode, "r") == 0) {
        mode = "r";
        iopipe = 1;
        }
     close(fd[iopipe]);
     if ((f = fdopen(fd[1 - iopipe], mode)) == NULL) {
        LOG_ERROR;
        close(fd[1 - iopipe]);
        }
     return f != NULL;
     }
  else { // child process
     int iofd = STDOUT_FILENO;
     if (strcmp(Mode, "w") == 0) {
        iopipe = 1;
        iofd = STDIN_FILENO;
        }
     close(fd[iopipe]);
     if (dup2(fd[1 - iopipe], iofd) == -1) { // now redirect
        LOG_ERROR;
        close(fd[1 - iopipe]);
        _exit(-1);
        }
     else {
        int MaxPossibleFileDescriptors = getdtablesize();
        for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
            close(i); //close all dup'ed filedescriptors
        if (execl("/bin/sh", "sh", "-c", Command, NULL) == -1) {
           LOG_ERROR_STR(Command);
           close(fd[1 - iopipe]);
           _exit(-1);
           }
        }
     _exit(0);
     }
}

int cPipe::Close(void)
{
  int ret = -1;

  if (f) {
     fclose(f);
     f = NULL;
     }

  if (pid > 0) {
     int status = 0;
     int i = 5;
     while (i > 0) {
           ret = waitpid(pid, &status, WNOHANG);
           if (ret < 0) {
              if (errno != EINTR && errno != ECHILD) {
                 LOG_ERROR;
                 break;
                 }
              }
           else if (ret == pid)
              break;
           i--;
           cCondWait::SleepMs(100);
           }
     if (!i) {
        kill(pid, SIGKILL);
        ret = -1;
        }
     else if (ret == -1 || !WIFEXITED(status))
        ret = -1;
     pid = -1;
     }

  return ret;
}

// --- SystemExec ------------------------------------------------------------

int SystemExec(const char *Command, bool Detached)
{
  pid_t pid;

  if ((pid = fork()) < 0) { // fork failed
     LOG_ERROR;
     return -1;
     }

  if (pid > 0) { // parent process
     int status = 0;
     if (waitpid(pid, &status, 0) < 0) {
        LOG_ERROR;
        return -1;
        }
     return status;
     }
  else { // child process
     if (Detached) {
        // Fork again and let first child die - grandchild stays alive without parent
        if (fork() > 0)
           _exit(0);
        // Start a new session
        pid_t sid = setsid();
        if (sid < 0)
           LOG_ERROR;
        // close STDIN and re-open as /dev/null
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull < 0 || dup2(devnull, 0) < 0)
           LOG_ERROR;
        }
     int MaxPossibleFileDescriptors = getdtablesize();
     for (int i = STDERR_FILENO + 1; i < MaxPossibleFileDescriptors; i++)
         close(i); //close all dup'ed filedescriptors
     if (execl("/bin/sh", "sh", "-c", Command, NULL) == -1) {
        LOG_ERROR_STR(Command);
        _exit(-1);
        }
     _exit(0);
     }
}
