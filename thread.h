/*
 * thread.h: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.h 1.1 2000/10/08 08:36:21 kls Exp $
 */

#ifndef __THREAD_H
#define __THREAD_H

#include <pthread.h>
#include <sys/types.h>

class cThread {
  friend class cThreadLock;
private:
  pthread_t thread;
  pthread_mutex_t mutex;
  pid_t parentPid, lockingPid;
  int locked;
  bool running;
  static bool signalHandlerInstalled;
  static void SignalHandler(int signum);
  static void *StartThread(cThread *Thread);
  bool Lock(void);
  void Unlock(void);
protected:
  void WakeUp(void);
  virtual void Action(void) = 0;
  void Stop(void);
public:
  cThread(void);
  virtual ~cThread();
  bool Start(void);
  };

// cThreadLock can be used to easily set a lock in a thread and make absolutely
// sure that it will be unlocked when the block will be left. Several locks can
// be stacked, so a function that makes many calls to another function which uses
// cThreadLock may itself use a cThreadLock to make one longer lock instead of many
// short ones.

class cThreadLock {
private:
  cThread *thread;
  bool locked;
public:
  cThreadLock(cThread *Thread);
  ~cThreadLock();
  bool Locked(void);
  };

#define LOCK_THREAD cThreadLock ThreadLock(this)

#endif //__THREAD_H
