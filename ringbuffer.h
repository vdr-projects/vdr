/*
 * ringbuffer.h: A threaded ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ringbuffer.h 1.3 2001/08/02 13:48:42 kls Exp $
 */

#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include "thread.h"

typedef unsigned char uchar;

class cRingBufferInputThread;
class cRingBufferOutputThread;

class cRingBuffer {
  friend class cRingBufferInputThread;
  friend class cRingBufferOutputThread;
private:
  cRingBufferInputThread *inputThread;
  cRingBufferOutputThread *outputThread;
  cMutex mutex;
  int size, head, tail;
  uchar *buffer;
  int maxFill;
  bool busy;
  bool statistics;
protected:
  void Lock(void) { mutex.Lock(); }
  void Unlock(void) { mutex.Unlock(); }
  int Available(void);
  int Free(void) { return size - Available() - 1; }
  bool Busy(void) { return busy; }
  void Clear(void);
    // Immediately clears the ring buffer.
  int Put(const uchar *Data, int Count);
    // Puts at most Count bytes of Data into the ring buffer.
    // Returns the number of bytes actually stored.
  int Get(uchar *Data, int Count);
    // Gets at most Count bytes of Data from the ring buffer.
    // Returns the number of bytes actually retrieved.
  virtual void Input(void) = 0;
    // Runs as a separate thread and shall continuously read data from
    // a source and call Put() to store the data in the ring buffer.
  virtual void Output(void) = 0;
    // Runs as a separate thread and shall continuously call Get() to
    // retrieve data from the ring buffer and write it to a destination.
public:
  cRingBuffer(int Size, bool Statistics = false);
  virtual ~cRingBuffer();
  bool Start(void);
  bool Active(void);
  void Stop(void);
  };

#endif // __RINGBUFFER_H
