/*
 * thread.c: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.c 1.1 2000/10/07 17:31:39 kls Exp $
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
  pthread_mutex_init(&mutex, NULL);
  running = false;
  parentPid = lockingPid = 0;
  locked = 0;
}

cThread::~cThread()
{
  pthread_mutex_destroy(&mutex);
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
     pthread_create(&thread, NULL, &StartThread, (void *)this);
     }
  return true; //XXX return value of pthread_create()???
}

void cThread::Stop(void)
{
  pthread_exit(NULL);
}

bool cThread::Lock(void)
{
  if (!lockingPid || lockingPid != getpid()) {
     pthread_mutex_lock(&mutex);
     lockingPid = getpid();
     }
  locked++;
  return true;
}

void cThread::Unlock(void)
{
  if (!--locked) {
     lockingPid = 0;
     pthread_mutex_unlock(&mutex);
     }
}

void cThread::WakeUp(void)
{
  kill(parentPid, SIGIO); // makes any waiting 'select()' call return immediately
}

// --- cThreadLock -----------------------------------------------------------

cThreadLock::cThreadLock(cThread *Thread)
{
  thread = Thread;
  locked = Thread->Lock();
}

cThreadLock::~cThreadLock()
{
  if (locked)
     thread->Unlock();
}

bool cThreadLock::Locked(void)
{
  return locked;
}

