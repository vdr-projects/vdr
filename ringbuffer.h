/*
 * ringbuffer.h: A threaded ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ringbuffer.h 1.5 2001/11/03 10:41:33 kls Exp $
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
  cCondVar readyForPut, readyForGet;
  cMutex putMutex, getMutex;
  int size;
  bool busy;
protected:
  int maxFill;//XXX
  bool statistics;//XXX
  void WaitForPut(void);
  void WaitForGet(void);
  void EnablePut(void);
  void EnableGet(void);
  virtual void Clear(void) = 0;
  virtual int Available(void) = 0;
  int Free(void) { return size - Available() - 1; }
  void Lock(void) { mutex.Lock(); }
  void Unlock(void) { mutex.Unlock(); }
  int Size(void) { return size; }
  bool Busy(void) { return busy; }
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

class cRingBufferLinear : public cRingBuffer {
private:
  int head, tail;
  uchar *buffer;
protected:
  virtual int Available(void);
  virtual void Clear(void);
    // Immediately clears the ring buffer.
  int Put(const uchar *Data, int Count);
    // Puts at most Count bytes of Data into the ring buffer.
    // Returns the number of bytes actually stored.
  int Get(uchar *Data, int Count);
    // Gets at most Count bytes of Data from the ring buffer.
    // Returns the number of bytes actually retrieved.
public:
  cRingBufferLinear(int Size, bool Statistics = false);
  virtual ~cRingBufferLinear();
  };

enum eFrameType { ftUnknown, ftVideo, ftAudio, ftDolby };

class cFrame {
  friend class cRingBufferFrame;
private:
  cFrame *next;
  uchar *data;
  int count;
  eFrameType type;
  int index;
public:
  cFrame(const uchar *Data, int Count, eFrameType = ftUnknown, int Index = -1);
  ~cFrame();
  const uchar *Data(void) const { return data; }
  int Count(void) const { return count; }
  eFrameType Type(void) const { return type; }
  int Index(void) const { return index; }
  };

class cRingBufferFrame : public cRingBuffer {
private:
  cFrame *head;
  int currentFill;
  void Delete(const cFrame *Frame);
protected:
  virtual int Available(void);
  virtual void Clear(void);
    // Immediately clears the ring buffer.
  bool Put(cFrame *Frame);
    // Puts the Frame into the ring buffer.
    // Returns true if this was possible.
  const cFrame *Get(bool Wait = true);
    // Gets the next frame from the ring buffer.
    // The actual data still remains in the buffer until Drop() is called.
  void Drop(const cFrame *Frame);
    // Drops the Frame that has just been fetched with Get().
public:
  cRingBufferFrame(int Size, bool Statistics = false);
  virtual ~cRingBufferFrame();
  };

#endif // __RINGBUFFER_H
