/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 1.5 2000/11/22 17:11:04 kls Exp $
 */

#include "thread.h"
#include <signal.h>
#include <unistd.h>

// --- cThread ---------------------------------------------------------------

// The signal handler is necessary to be able to use SIGIO to wake up any
// pending 'select()' call.

bool cThread::signalHandlerInstalled = false;

cThread::cThread(void)
{
  if (!signalHandlerInstalled) {
     signal(SIGIO, SignalHandler);
     signalHandlerInstalled = true;
     }
  running = false;
  parentPid = lockingPid = 0;
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
  Thread->Action();
  return NULL;
}

bool cThread::Start(void)
{
  if (!running) {
     running = true;
     parentPid = getpid();
     pthread_create(&thread, NULL, (void *(*) (void *))&StartThread, (void *)this);
     }
  return true; //XXX return value of pthread_create()???
}

void cThread::Stop(void)
{
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

