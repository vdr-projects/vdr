/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 1.29 2003/12/21 15:17:24 kls Exp $
 */

#include "thread.h"
#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tools.h"

// --- cCondVar --------------------------------------------------------------

cCondVar::cCondVar(void)
{
  pthread_cond_init(&cond, 0);
}

cCondVar::~cCondVar()
{
  pthread_cond_destroy(&cond);
}

void cCondVar::Wait(cMutex &Mutex)
{
  if (Mutex.locked) {
     int locked = Mutex.locked;
     Mutex.locked = 0; // have to clear the locked count here, as pthread_cond_wait
                       // does an implizit unlock of the mutex
     pthread_cond_wait(&cond, &Mutex.mutex);
     Mutex.locked = locked;
     }
}

bool cCondVar::TimedWait(cMutex &Mutex, int TimeoutMs)
{
  bool r = true; // true = condition signaled false = timeout

  if (Mutex.locked) {
     struct timeval now;                   // unfortunately timedwait needs the absolute time, not the delta :-(
     if (gettimeofday(&now, NULL) == 0) {  // get current time
        now.tv_usec += TimeoutMs * 1000;   // add the timeout
        while (now.tv_usec >= 1000000) {   // take care of an overflow
              now.tv_sec++;
              now.tv_usec -= 1000000;
              }
        struct timespec abstime;              // build timespec for timedwait
        abstime.tv_sec = now.tv_sec;          // seconds
        abstime.tv_nsec = now.tv_usec * 1000; // nano seconds

        int locked = Mutex.locked;
        Mutex.locked = 0; // have to clear the locked count here, as pthread_cond_timedwait
                          // does an implizit unlock of the mutex.
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

/*
void cCondVar::Signal(void)
{
  pthread_cond_signal(&cond);
}
*/

// --- cRWlock ---------------------------------------------------------------

cRWlock::cRWlock(bool PreferWriter)
{
  pthread_rwlockattr_t attr = { PreferWriter ? PTHREAD_RWLOCK_PREFER_WRITER_NP : PTHREAD_RWLOCK_PREFER_READER_NP };
  pthread_rwlock_init(&rwlock, &attr);
}

cRWlock::~cRWlock()
{
  pthread_rwlock_destroy(&rwlock);
}

bool cRWlock::Lock(bool Write, int TimeoutMs)
{
  int Result = 0;
  struct timespec abstime;
  if (TimeoutMs) {
     abstime.tv_sec = TimeoutMs / 1000;
     abstime.tv_nsec = (TimeoutMs % 1000) * 1000000;
     }
  if (Write)
     Result = TimeoutMs ? pthread_rwlock_timedwrlock(&rwlock, &abstime) : pthread_rwlock_wrlock(&rwlock);
  else
     Result = TimeoutMs ? pthread_rwlock_timedrdlock(&rwlock, &abstime) : pthread_rwlock_rdlock(&rwlock);
  return Result == 0;
}

void cRWlock::Unlock(void)
{
  pthread_rwlock_unlock(&rwlock);
}

// --- cMutex ----------------------------------------------------------------

cMutex::cMutex(void)
{
  locked = 0;
  pthread_mutexattr_t attr = { PTHREAD_MUTEX_ERRORCHECK_NP };
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

// The signal handler is necessary to be able to use SIGIO to wake up any
// pending 'select()' call.

bool cThread::signalHandlerInstalled = false;
bool cThread::emergencyExitRequested = false;

cThread::cThread(const char *Description)
{
  if (!signalHandlerInstalled) {
     signal(SIGIO, SignalHandler);
     signalHandlerInstalled = true;
     }
  running = false;
  parentTid = childTid = 0;
  description = NULL;
  SetDescription(Description);
}

cThread::~cThread()
{
  free(description);
}

void cThread::SetDescription(const char *Description, ...)
{
  free(description);
  description = NULL;
  if (Description) {
     va_list ap;
     va_start(ap, Description);
     vasprintf(&description, Description, ap);
     va_end(ap);
     }
}

void cThread::SignalHandler(int signum)
{
  signal(signum, SignalHandler);
}

void *cThread::StartThread(cThread *Thread)
{
  Thread->childTid = pthread_self();
  if (Thread->description)
     dsyslog("%s thread started (pid=%d, tid=%ld)", Thread->description, getpid(), Thread->childTid);
  Thread->Action();
  if (Thread->description)
     dsyslog("%s thread ended (pid=%d, tid=%ld)", Thread->description, getpid(), Thread->childTid);
  Thread->childTid = 0;
  return NULL;
}

bool cThread::Start(void)
{
  if (!running) {
     running = true;
     parentTid = pthread_self();
     pthread_create(&childTid, NULL, (void *(*) (void *))&StartThread, (void *)this);
     pthread_setschedparam(childTid, SCHED_RR, 0);
     usleep(10000); // otherwise calling Active() immediately after Start() causes a "pure virtual method called" error
     }
  return true; //XXX return value of pthread_create()???
}

bool cThread::Active(void)
{
  if (childTid) {
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
        }
     else
        return true;
     }
  return false;
}

void cThread::Cancel(int WaitSeconds)
{
  running = false;
  if (WaitSeconds > 0) {
     for (time_t t0 = time(NULL) + WaitSeconds; time(NULL) < t0; ) {
         if (!Active())
            return;
         usleep(10000);
         }
     esyslog("ERROR: thread %ld won't end (waited %d seconds) - cancelling it...", childTid, WaitSeconds);
     }
  pthread_cancel(childTid);
}

void cThread::WakeUp(void)
{
  pthread_kill(parentTid, SIGIO); // makes any waiting 'select()' call return immediately
}

bool cThread::EmergencyExit(bool Request)
{
  if (!Request)
     return emergencyExitRequested;
  esyslog("initiating emergency exit");
  return emergencyExitRequested = true; // yes, it's an assignment, not a comparison!
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

  char *mode = "w";
  int iopipe = 0;

  if (pid > 0) { // parent process
     if (strcmp(Mode, "r") == 0) {
        mode = "r";
        iopipe = 1;
        }
     close(fd[iopipe]);
     f = fdopen(fd[1 - iopipe], mode);
     if ((f = fdopen(fd[1 - iopipe], mode)) == NULL) {
        LOG_ERROR;
        close(fd[1 - iopipe]);
        }
     return f != NULL;
     }
  else { // child process
     int iofd = STDOUT_FILENO;
     if (strcmp(Mode, "w") == 0) {
        mode = "r";
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
           usleep(100000);
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

int SystemExec(const char *Command)
{
  pid_t pid;

  if ((pid = fork()) < 0) { // fork failed
     LOG_ERROR;
     return -1;
     }

  if (pid > 0) { // parent process
     int status;
     if (waitpid(pid, &status, 0) < 0) {
        LOG_ERROR;
        return -1;
        }
     return status;
     }
  else { // child process
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

