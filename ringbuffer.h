/*
 * ringbuffer.h: A ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ringbuffer.h 1.6 2002/06/16 11:30:07 kls Exp $
 */

#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include "thread.h"

typedef unsigned char uchar;//XXX+

class cRingBuffer {
private:
  cMutex mutex;
  cCondVar readyForPut, readyForGet;
  cMutex putMutex, getMutex;
  int size;
protected:
  int maxFill;//XXX
  int lastPercent;
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
public:
  cRingBuffer(int Size, bool Statistics = false);
  virtual ~cRingBuffer();
  };

class cRingBufferLinear : public cRingBuffer {
private:
  int head, tail;
  uchar *buffer;
  pid_t getThreadPid;
public:
  cRingBufferLinear(int Size, bool Statistics = false);
  virtual ~cRingBufferLinear();
  virtual int Available(void);
  virtual void Clear(void);
    // Immediately clears the ring buffer.
  int Put(const uchar *Data, int Count);
    // Puts at most Count bytes of Data into the ring buffer.
    // Returns the number of bytes actually stored.
  int Get(uchar *Data, int Count);
    // Gets at most Count bytes of Data from the ring buffer.
    // Returns the number of bytes actually retrieved.
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
public:
  cRingBufferFrame(int Size, bool Statistics = false);
  virtual ~cRingBufferFrame();
  virtual int Available(void);
  virtual void Clear(void);
    // Immediately clears the ring buffer.
  bool Put(cFrame *Frame);
    // Puts the Frame into the ring buffer.
    // Returns true if this was possible.
  const cFrame *Get(void);
    // Gets the next frame from the ring buffer.
    // The actual data still remains in the buffer until Drop() is called.
  void Drop(const cFrame *Frame);
    // Drops the Frame that has just been fetched with Get().
  };

#endif // __RINGBUFFER_H
