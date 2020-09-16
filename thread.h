/*
 * thread.h: A simple thread base class
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: thread.h 4.6 2020/09/16 13:48:33 kls Exp $
 */

#ifndef __THREAD_H
#define __THREAD_H

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>

typedef pid_t tThreadId;

class cCondWait {
private:
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  bool signaled;
public:
  cCondWait(void);
  ~cCondWait();
  static void SleepMs(int TimeoutMs);
       ///< Creates a cCondWait object and uses it to sleep for TimeoutMs
       ///< milliseconds, immediately giving up the calling thread's time
       ///< slice and thus avoiding a "busy wait".
       ///< In order to avoid a possible busy wait, TimeoutMs will be automatically
       ///< limited to values >2.
  bool Wait(int TimeoutMs = 0);
       ///< Waits at most TimeoutMs milliseconds for a call to Signal(), or
       ///< forever if TimeoutMs is 0.
       ///< Returns true if Signal() has been called, false if the given
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
  int locked;
  tThreadId writeLockThreadId;
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
  bool active;
  bool running;
  pthread_t childTid;
  tThreadId childThreadId;
  cMutex mutex;
  char *description;
  bool lowPriority;
  static tThreadId mainThreadId;
  static void *StartThread(cThread *Thread);
protected:
  void SetPriority(int Priority);
  void SetIOPriority(int Priority);
  void Lock(void) { mutex.Lock(); }
  void Unlock(void) { mutex.Unlock(); }
  virtual void Action(void) = 0;
       ///< A derived cThread class must implement the code it wants to
       ///< execute as a separate thread in this function. If this is
       ///< a loop, it must check Running() repeatedly to see whether
       ///< it's time to stop.
  bool Running(void) { return running; }
       ///< Returns false if a derived cThread object shall leave its Action()
       ///< function.
  void Cancel(int WaitSeconds = 0);
       ///< Cancels the thread by first setting 'running' to false, so that
       ///< the Action() loop can finish in an orderly fashion and then waiting
       ///< up to WaitSeconds seconds for the thread to actually end. If the
       ///< thread doesn't end by itself, it is killed.
       ///< If WaitSeconds is -1, only 'running' is set to false and Cancel()
       ///< returns immediately, without killing the thread.
public:
  cThread(const char *Description = NULL, bool LowPriority = false);
       ///< Creates a new thread.
       ///< If Description is present, a log file entry will be made when
       ///< the thread starts and stops (see SetDescription()).
       ///< The Start() function must be called to actually start the thread.
       ///< LowPriority can be set to true to make this thread run at a lower
       ///< priority.
  virtual ~cThread();
  void SetDescription(const char *Description, ...) __attribute__ ((format (printf, 2, 3)));
       ///< Sets the description of this thread, which will be used when logging
       ///< starting or stopping of the thread. Make sure any important information
       ///< is within the first 15 characters of Description, because only these
       ///< may be displayed in thread listings (like 'htop', for instance).
  bool Start(void);
       ///< Actually starts the thread.
       ///< If the thread is already running, nothing happens.
  bool Active(void);
       ///< Checks whether the thread is still alive.
  static tThreadId ThreadId(void);
  static tThreadId IsMainThread(void) { return ThreadId() == mainThreadId; }
  static void SetMainThreadId(void);
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

class cStateKey;

class cStateLock {
  friend class cStateKey;
private:
  enum { emDisabled = 0, emArmed, emEnabled };
  const char *name;
  tThreadId threadId;
  cRwLock rwLock;
  int state;
  int explicitModify;
  cStateKey *syncStateKey;
  void Unlock(cStateKey &StateKey, bool IncState = true);
       ///< Releases a lock that has been obtained by a previous call to Lock()
       ///< with the given StateKey. If this was a write-lock, and IncState is true,
       ///< the state of the lock will be incremented. In any case, the (new) state
       ///< of the lock will be copied to the StateKey's state.
public:
  cStateLock(const char *Name = NULL);
  bool Lock(cStateKey &StateKey, bool Write = false, int TimeoutMs = 0);
       ///< Tries to get a lock and returns true if successful.
       ///< If TimoutMs is not 0, it waits for the given number of milliseconds
       ///< and returns false if no lock has been obtained within that time.
       ///< Otherwise it waits indefinitely for the lock. The given StateKey
       ///< will store which lock it has been used with, and will use that
       ///< information when its Remove() function is called.
       ///< There are two possible locks, one only for read access, and one
       ///< for reading and writing:
       ///<
       ///< If Write is false (i.e. a read-lock is requested), the lock's state
       ///< is compared to the given StateKey's state, and true is returned if
       ///< they differ.
       ///< If true is returned, the read-lock is still in place and the
       ///< protected data structures can be safely accessed (in read-only mode!).
       ///< Once the necessary operations have been performed, the lock must
       ///< be released by a call to the StateKey's Remove() function.
       ///< If false is returned, the state has not changed since the last check,
       ///< and the read-lock has been released. In that case the protected
       ///< data structures have not changed since the last call, so no action
       ///< is required. Note that if TimeoutMs is used with read-locks, Lock()
       ///< might return false even if the states of lock and key differ, just
       ///< because it was unable to obtain the lock within the given time.
       ///< You can call cStateKey::TimedOut() to detect this.
       ///<
       ///< If Write is true (i.e. a write-lock is requested), the states of the
       ///< lock and the given StateKey don't matter, it will always try to obtain
       ///< a write lock.
  void SetSyncStateKey(cStateKey &StateKey);
       ///< Sets the given StateKey to be synchronized to the state of this lock.
       ///< The caller must currently hold a write lock on this lock, with a cStateKey
       ///< that is different from the given StateKey. If, when removing the key that
       ///< is holding the write lock, the StateKey's current state is the same as that
       ///< of the lock, it will be increased together with the lock's state.
  void SetExplicitModify(void);
       ///< If you have obtained a write lock on this lock, and you don't want its
       ///< state to be automatically incremented when the lock is released, a call to
       ///< this function will disable this, and you can explicitly call SetModified()
       ///< to increment the state.
  void SetModified(void);
       ///< Sets this lock to have its state incremented when the current write lock
       ///< state key is removed. Must have called SetExplicitModify() before calling
       ///< this function.
  };

class cStateKey {
  friend class cStateLock;
private:
  cStateLock *stateLock;
  bool write;
  int state;
  bool timedOut;
public:
  cStateKey(bool IgnoreFirst = false);
       ///< Sets up a new state key. If IgnoreFirst is true, the first use
       ///< of this key with a lock will not return true if the lock's state
       ///< hasn't explicitly changed.
  ~cStateKey();
  void Reset(void);
       ///< Resets the state of this key, so that the next call to a lock's
       ///< Lock() function with this key will return true, even if the
       ///< lock's state hasn't changed.
  void Remove(bool IncState = true);
       ///< Removes this key from the lock it was previously used with.
       ///< If this key was used to obtain a write lock, the state of the lock will
       ///< be incremented and copied to this key. You can set IncState to false
       ///< to prevent this.
  bool StateChanged(void);
       ///< Returns true if this key is used for obtaining a write lock, and the
       ///< lock's state differs from that of the key. When used with a read lock,
       ///< it always returns true, because otherwise the lock wouldn't have been
       ///< obtained in the first place.
  bool InLock(void) { return stateLock; }
       ///< Returns true if this key is currently in a lock.
  bool TimedOut(void) const { return timedOut; }
       ///< Returns true if the last lock attempt this key was used with failed due
       ///< to a timeout.
  };

class cIoThrottle {
private:
  static cMutex mutex;
  static int count;
  bool active;
public:
  cIoThrottle(void);
  ~cIoThrottle();
  void Activate(void);
       ///< Activates the global I/O throttling mechanism.
       ///< This function may be called any number of times, but only
       ///< the first call after an inactive state will have an effect.
  void Release(void);
       ///< Releases the global I/O throttling mechanism.
       ///< This function may be called any number of times, but only
       ///< the first call after an active state will have an effect.
  bool Active(void) { return active; }
       ///< Returns true if this I/O throttling object is currently active.
  static bool Engaged(void);
       ///< Returns true if any I/O throttling object is currently active.
  };

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

// cBackTrace can be used for debugging.

class cStringList;
class cString;

class cBackTrace {
public:
  static cString Demangle(char *s);
         ///< Demangles the function name in the given string and returns the converted
         ///< version of s. s must be one of the strings returned by a call to
         ///< BackTrace() or GetCaller().
         ///< Note that this function works on the given string by inserting '\0'
         ///< characters to separate the individual parts. Therefore the string
         ///< will be modified upon return.
  static void BackTrace(cStringList &StringList, int Level = 0, bool Mangled = false);
         ///< Produces a backtrace and stores it in the given StringList.
         ///< If Level is given, only calls up to the given value are listed.
         ///< If Mangled is true, the raw backtrace will be returned and you can use
         ///< Demangle() to make the function names readable.
  static void BackTrace(FILE *f = NULL, int Level = 0, bool Mangled = false);
         ///< Produces a backtrace beginning at the given Level, and
         ///< writes it to the given file. If no file is given, the backtrace is
         ///< written to the logfile. If Mangled is true, the raw backtrace will
         ///< be printed/logged.
  static cString GetCaller(int Level = 0, bool Mangled = false);
         ///< Returns the caller at the given Level (or the immediate caller,
         ///< if Level is 0).
         ///< If Mangled is true, the raw backtrace will be returned and you can use
         ///< Demangle() to make the function name readable.
  };

// SystemExec() implements a 'system()' call that closes all unnecessary file
// descriptors in the child process.
// With Detached=true, calls command in background and in a separate session,
// with stdin connected to /dev/null.

int SystemExec(const char *Command, bool Detached = false);

#endif //__THREAD_H
