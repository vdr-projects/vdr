/*
 * thread.h: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.h 1.5 2001/05/25 09:36:27 kls Exp $
 */

#ifndef __THREAD_H
#define __THREAD_H

#include <pthread.h>
#include <sys/types.h>

class cMutex {
private:
  pthread_mutex_t mutex;
  pid_t lockingPid;
  int locked;
public:
  cMutex(void);
  ~cMutex();
  void Lock(void);
  void Unlock(void);
  };

class cThread {
  friend class cThreadLock;
private:
  pthread_t thread;
  cMutex Mutex;
  pid_t parentPid, threadPid, lockingPid;
  int locked;
  bool running;
  static bool emergencyExitRequested;
  static bool signalHandlerInstalled;
  static void SignalHandler(int signum);
  static void *StartThread(cThread *Thread);
  bool Lock(void);
  void Unlock(void);
protected:
  void WakeUp(void);
  virtual void Action(void) = 0;
  void Cancel(int WaitSeconds = 0);
public:
  cThread(void);
  virtual ~cThread();
  bool Start(void);
  bool Active(void);
  static bool EmergencyExit(bool Request = false);
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
  cThreadLock(cThread *Thread = NULL);
  ~cThreadLock();
  bool Lock(cThread *Thread);
  bool Locked(void);
  };

#define LOCK_THREAD cThreadLock ThreadLock(this)

#endif //__THREAD_H
