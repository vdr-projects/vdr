/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 1.9 2001/06/27 11:34:41 kls Exp $
 */

#include "thread.h"
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "tools.h"

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
  if (getpid() != lockingPid || !locked)
     pthread_mutex_lock(&mutex);
  lockingPid = getpid();
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
  parentPid = threadPid = lockingPid = 0;
  locked = 0;
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

bool cThread::Lock(void)
{
  if (!lockingPid || lockingPid != getpid()) {
     Mutex.Lock();
     lockingPid = getpid();
     }
  locked++;
  return true;
}

void cThread::Unlock(void)
{
  if (!--locked) {
     lockingPid = 0;
     Mutex.Unlock();
     }
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
     locked = Thread->Lock();
     return locked;
     }
  return false;
}

bool cThreadLock::Locked(void)
{
  return locked;
}

