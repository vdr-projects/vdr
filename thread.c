/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 1.19 2002/03/09 12:05:44 kls Exp $
 */

#include "thread.h"
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
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

bool cCondVar::Wait(cMutex &Mutex)
{
  return pthread_cond_wait(&cond, &Mutex.mutex);
}

/*
bool cCondVar::TimedWait(cMutex &Mutex, unsigned long tmout)
{
  return pthread_cond_timedwait(&cond, &Mutex.mutex, tmout);
}
*/

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

// --- cMutex ----------------------------------------------------------------

cMutex::cMutex(void)
{
  lockingPid = 0;
  locked = 0;
  pthread_mutex_init(&mutex, NULL);
}

cMutex::~cMutex()
{
  pthread_mutex_destroy(&mutex);
}

void cMutex::Lock(void)
{
  if (getpid() != lockingPid || !locked) {
     pthread_mutex_lock(&mutex);
     lockingPid = getpid();
     }
  locked++;
}

void cMutex::Unlock(void)
{
 if (!--locked) {
    lockingPid = 0;
    pthread_mutex_unlock(&mutex);
    }
}

// --- cThread ---------------------------------------------------------------

// The signal handler is necessary to be able to use SIGIO to wake up any
// pending 'select()' call.

time_t cThread::lastPanic = 0;
int cThread::panicLevel = 0;
bool cThread::signalHandlerInstalled = false;
bool cThread::emergencyExitRequested = false;

cThread::cThread(void)
{
  if (!signalHandlerInstalled) {
     signal(SIGIO, SignalHandler);
     signalHandlerInstalled = true;
     }
  running = false;
  parentPid = threadPid = 0;
}

cThread::~cThread()
{
}

void cThread::SignalHandler(int signum)
{
  signal(signum, SignalHandler);
}

void *cThread::StartThread(cThread *Thread)
{
  Thread->threadPid = getpid();
  Thread->Action();
  return NULL;
}

bool cThread::Start(void)
{
  if (!running) {
     running = true;
     parentPid = getpid();
     pthread_create(&thread, NULL, (void *(*) (void *))&StartThread, (void *)this);
     pthread_setschedparam(thread, SCHED_RR, 0);
     usleep(10000); // otherwise calling Active() immediately after Start() causes a "pure virtual method called" error
     }
  return true; //XXX return value of pthread_create()???
}

bool cThread::Active(void)
{
  if (threadPid) {
     if (kill(threadPid, SIGIO) < 0) { // couldn't find another way of checking whether the thread is still running - any ideas?
        if (errno == ESRCH)
           threadPid = 0;
        else
           LOG_ERROR;
        }
     else
        return true;
     }
  return false;
}

void cThread::Cancel(int WaitSeconds)
{
  if (WaitSeconds > 0) {
     for (time_t t0 = time(NULL) + WaitSeconds; time(NULL) < t0; ) {
         if (!Active())
            return;
         usleep(10000);
         }
     esyslog(LOG_ERR, "ERROR: thread %d won't end (waited %d seconds) - cancelling it...", threadPid, WaitSeconds);
     }
  pthread_cancel(thread);
}

void cThread::WakeUp(void)
{
  kill(parentPid, SIGIO); // makes any waiting 'select()' call return immediately
}

#define MAXPANICLEVEL 10

void cThread::RaisePanic(void)
{
  if (lastPanic > 0) {
     if (time(NULL) - lastPanic < 5)
        panicLevel++;
     else if (panicLevel > 0)
        panicLevel--;
     }
  lastPanic = time(NULL);
  if (panicLevel > MAXPANICLEVEL) {
     esyslog(LOG_ERR, "ERROR: max. panic level exceeded");
     EmergencyExit(true);
     }
  else
     dsyslog(LOG_INFO, "panic level: %d", panicLevel);
}

bool cThread::EmergencyExit(bool Request)
{
  if (!Request)
     return emergencyExitRequested;
  esyslog(LOG_ERR, "initiating emergency exit");
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
        for (int i = 0; i <= fd[1]; i++) {
            if (i == STDIN_FILENO || i == STDOUT_FILENO || i == STDERR_FILENO)
               continue;
            close(i); // close all dup'ed filedescriptors
            }
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

