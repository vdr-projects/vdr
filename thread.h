/*
 * thread.h: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.h 1.23 2004/10/24 10:28:48 kls Exp $
 */

#ifndef __THREAD_H
#define __THREAD_H

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>

class cCondWait {
private:
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool signaled;
public:
  cCondWait(void);
  ~cCondWait();
  bool Wait(int TimeoutMs = 0);
       ///< Waits at most TimeoutMs milliseconds for a call to Signal(), or
       ///< forever if TimeoutMs is 0.
       ///< \return Returns true if Signal() has been called, false it the given
       ///< timeout has expired.
  void Signal(void);
       ///< Signals a caller of Wait() that the condition it is waiting for is met.
  };

class cMutex;

class cCondVar {
private:
  pthread_cond_t cond;
public:
  cCondVar(void);
  ~cCondVar();
  void Wait(cMutex &Mutex);
  bool TimedWait(cMutex &Mutex, int TimeoutMs);
  void Broadcast(void);
  };

class cRwLock {
private:
  pthread_rwlock_t rwlock;
public:
  cRwLock(bool PreferWriter = false);
  ~cRwLock();
  bool Lock(bool Write, int TimeoutMs = 0);
  void Unlock(void);
  };

class cMutex {
  friend class cCondVar;
private:
  pthread_mutex_t mutex;
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
  pthread_t parentTid, childTid;
  cMutex mutex;
  char *description;
  static bool emergencyExitRequested;
  static void *StartThread(cThread *Thread);
protected:
  void Lock(void) { mutex.Lock(); }
  void Unlock(void) { mutex.Unlock(); }
  virtual void Action(void) = 0;
  void Cancel(int WaitSeconds = 0);
public:
  cThread(const char *Description = NULL);
  virtual ~cThread();
  void SetDescription(const char *Description, ...);
  bool Start(void);
  bool Active(void);
  static bool EmergencyExit(bool Request = false);
  };

// cMutexLock can be used to easily set a lock on mutex and make absolutely
// sure that it will be unlocked when the block will be left. Several locks can
// be stacked, so a function that makes many calls to another function which uses
// cMutexLock may itself use a cMutexLock to make one longer lock instead of many
// short ones.

class cMutexLock {
private:
  cMutex *mutex;
  bool locked;
public:
  cMutexLock(cMutex *Mutex = NULL);
  ~cMutexLock();
  bool Lock(cMutex *Mutex);
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
  };

#define LOCK_THREAD cThreadLock ThreadLock(this)

// cPipe implements a pipe that closes all unnecessary file descriptors in
// the child process.

class cPipe {
private:
  pid_t pid;
  FILE *f;
public:
  cPipe(void);
  ~cPipe();
  operator FILE* () { return f; }
  bool Open(const char *Command, const char *Mode);
  int Close(void);
  };

// SystemExec() implements a 'system()' call that closes all unnecessary file
// descriptors in the child process.

int SystemExec(const char *Command);

#endif //__THREAD_H
