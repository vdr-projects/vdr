/*
 * ringbuffer.c: A threaded ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Parts of this file were inspired by the 'ringbuffy.c' from the
 * LinuxDVB driver (see linuxtv.org).
 *
 * $Id: ringbuffer.c 1.1 2001/03/10 17:11:34 kls Exp $
 */

#include "ringbuffer.h"
#include "tools.h"

// --- cRingBufferInputThread -------------------------------------------------

class cRingBufferInputThread : public cThread {
private:
  cRingBuffer *ringBuffer;
protected:
  virtual void Action(void) { ringBuffer->Input(); }
public:
  cRingBufferInputThread(cRingBuffer *RingBuffer) { ringBuffer = RingBuffer; }
  };

// --- cRingBufferOutputThread ------------------------------------------------

class cRingBufferOutputThread : public cThread {
private:
  cRingBuffer *ringBuffer;
protected:
  virtual void Action(void) { ringBuffer->Output(); }
public:
  cRingBufferOutputThread(cRingBuffer *RingBuffer) { ringBuffer = RingBuffer; }
  };

// --- cRingBuffer ------------------------------------------------------------

cRingBuffer::cRingBuffer(int Size)
{
  size = Size;
  buffer = NULL;
  inputThread = NULL;
  outputThread = NULL;
  maxFill = 0;
  busy = false;
  if (size > 1) { // 'size - 1' must not be 0!
     buffer = new uchar[size];
     if (!buffer)
        esyslog(LOG_ERR, "ERROR: can't allocate ring buffer (size=%d)", size);
     Clear();
     }
  else
     esyslog(LOG_ERR, "ERROR: illegal size for ring buffer (%d)", size);
}

cRingBuffer::~cRingBuffer()
{
  delete inputThread;
  delete outputThread;
  delete buffer;
  dsyslog(LOG_INFO, "buffer stats: %d (%d%%) used", maxFill, maxFill * 100 / (size - 1));
}

void cRingBuffer::Clear(void)
{
  mutex.Lock();
  head = tail = 0;
  mutex.Unlock();
}

int cRingBuffer::Put(const uchar *Data, int Count)
{
  if (Count > 0) {
     mutex.Lock();
     int rest = size - head;
     int diff = tail - head;
     mutex.Unlock();
     int free = (diff > 0) ? diff - 1 : size + diff - 1;
     // Statistics:
     int fill = size - free - 1 + Count;
     if (fill >= size)
        fill = size - 1;
     if (fill > maxFill) {
        maxFill = fill;
        int percent = maxFill * 100 / (size - 1);
        if (percent > 75)
           dsyslog(LOG_INFO, "buffer usage: %d%%", percent);
        }
     //
     if (free <= 0)
        return 0;
     if (free < Count)
        Count = free;
     if (Count > maxFill)
        maxFill = Count;
     if (Count >= rest) {
        memcpy(buffer + head, Data, rest);
        if (Count - rest)
           memcpy(buffer, Data + rest, Count - rest);
        head = Count - rest;
        }
     else {
        memcpy(buffer + head, Data, Count);
        head += Count;
        }
     }
  return Count;
}

int cRingBuffer::Get(uchar *Data, int Count)
{
  if (Count > 0) {
     mutex.Lock();
     int rest = size - tail;
     int diff = head - tail;
     mutex.Unlock();
     int cont = (diff >= 0) ? diff : size + diff;
     if (rest <= 0)
        return 0;
     if (cont < Count)
        Count = cont;
     if (Count >= rest) {
        memcpy(Data, buffer + tail, rest);
        if (Count - rest)
           memcpy(Data + rest, buffer, Count - rest);
        tail = Count - rest;
        }
     else {
        memcpy(Data, buffer + tail, Count);
        tail += Count;
        }
     }
  return Count;
}

bool cRingBuffer::Start(void)
{
  if (!busy) {
     busy = true;
     outputThread = new cRingBufferOutputThread(this);
     if (!outputThread->Start())
        DELETENULL(outputThread);
     inputThread = new cRingBufferInputThread(this);
     if (!inputThread->Start()) {
        DELETENULL(inputThread);
        DELETENULL(outputThread);
        }
     busy = outputThread && inputThread;
     }
  return busy;
}

bool cRingBuffer::Active(void)
{
  return outputThread && outputThread->Active() && inputThread && inputThread->Active();
}

void cRingBuffer::Stop(void)
{
  busy = false;
  for (time_t t0 = time(NULL) + 3; time(NULL) < t0; ) {
      if (!((outputThread && outputThread->Active()) || (inputThread && inputThread->Active())))
         break;
      }
  DELETENULL(inputThread);
  DELETENULL(outputThread);
}

