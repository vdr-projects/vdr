/*
 * ringbuffer.h: A ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ringbuffer.h 1.12 2003/05/12 17:35:10 kls Exp $
 */

#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H

#include "thread.h"
#include "tools.h"

class cRingBuffer {
private:
  cMutex mutex;
  cCondVar readyForPut, readyForGet;
  cMutex putMutex, getMutex;
  int putTimeout;
  int getTimeout;
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
  void SetTimeouts(int PutTimeout, int GetTimeout);
  };

class cRingBufferLinear : public cRingBuffer {
private:
  int margin, head, tail;
  int lastGet;
  uchar *buffer;
  pid_t getThreadPid;
public:
  cRingBufferLinear(int Size, int Margin = 0, bool Statistics = false);
    ///< Creates a linear ring buffer.
    ///< The buffer will be able to hold at most Size bytes of data, and will
    ///< be guaranteed to return at least Margin bytes in one consecutive block.
  virtual ~cRingBufferLinear();
  virtual int Available(void);
  virtual void Clear(void);
    ///< Immediately clears the ring buffer.
  int Put(const uchar *Data, int Count);
    ///< Puts at most Count bytes of Data into the ring buffer.
    ///< \return Returns the number of bytes actually stored.
  uchar *Get(int &Count);
    ///< Gets data from the ring buffer.
    ///< The data will remain in the buffer until a call to Del() deletes it.
    ///< \return Returns a pointer to the data, and stores the number of bytes
    ///< actually retrieved in Count. If the returned pointer is NULL, Count has no meaning.
  void Del(int Count);
    ///< Deletes at most Count bytes from the ring buffer.
    ///< Count must be less or equal to the number that was returned by a previous
    ///< call to Get().
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
    ///< Creates a new cFrame object.
    ///< If Count is negative, the cFrame object will take ownership of the given
    ///< Data. Otherwise it will allocate Count bytes of memory and copy Data.
  ~cFrame();
  uchar *Data(void) const { return data; }
  int Count(void) const { return count; }
  eFrameType Type(void) const { return type; }
  int Index(void) const { return index; }
  };

class cRingBufferFrame : public cRingBuffer {
private:
  cFrame *head;
  int currentFill;
  void Delete(cFrame *Frame);
public:
  cRingBufferFrame(int Size, bool Statistics = false);
  virtual ~cRingBufferFrame();
  virtual int Available(void);
  virtual void Clear(void);
    // Immediately clears the ring buffer.
  bool Put(cFrame *Frame);
    // Puts the Frame into the ring buffer.
    // Returns true if this was possible.
  cFrame *Get(void);
    // Gets the next frame from the ring buffer.
    // The actual data still remains in the buffer until Drop() is called.
  void Drop(cFrame *Frame);
    // Drops the Frame that has just been fetched with Get().
  };

#endif // __RINGBUFFER_H
