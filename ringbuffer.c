/*
 * ringbuffer.c: A ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Parts of this file were inspired by the 'ringbuffy.c' from the
 * LinuxDVB driver (see linuxtv.org).
 *
 * $Id: ringbuffer.c 1.11 2003/01/19 15:03:00 kls Exp $
 */

#include "ringbuffer.h"
#include <stdlib.h>
#include <unistd.h>
#include "tools.h"

// --- cRingBuffer -----------------------------------------------------------

cRingBuffer::cRingBuffer(int Size, bool Statistics)
{
  size = Size;
  statistics = Statistics;
  maxFill = 0;
  lastPercent = 0;
}

cRingBuffer::~cRingBuffer()
{
  if (statistics)
     dsyslog("buffer stats: %d (%d%%) used", maxFill, maxFill * 100 / (size - 1));
}

void cRingBuffer::WaitForPut(void)
{
  putMutex.Lock();
  readyForPut.Wait(putMutex);
  putMutex.Unlock();
}

void cRingBuffer::WaitForGet(void)
{
  getMutex.Lock();
  readyForGet.Wait(getMutex);
  getMutex.Unlock();
}

void cRingBuffer::EnablePut(void)
{
  readyForPut.Broadcast();
}

void cRingBuffer::EnableGet(void)
{
  readyForGet.Broadcast();
}

// --- cRingBufferLinear -----------------------------------------------------

cRingBufferLinear::cRingBufferLinear(int Size, bool Statistics)
:cRingBuffer(Size, Statistics)
{
  buffer = NULL;
  getThreadPid = -1;
  if (Size > 1) { // 'Size - 1' must not be 0!
     buffer = new uchar[Size];
     if (!buffer)
        esyslog("ERROR: can't allocate ring buffer (size=%d)", Size);
     Clear();
     }
  else
     esyslog("ERROR: illegal size for ring buffer (%d)", Size);
}

cRingBufferLinear::~cRingBufferLinear()
{
  delete buffer;
}

int cRingBufferLinear::Available(void)
{
  Lock();
  int diff = head - tail;
  Unlock();
  return (diff >= 0) ? diff : Size() + diff;
}

void cRingBufferLinear::Clear(void)
{
  Lock();
  head = tail = 0;
  Unlock();
  EnablePut();
  EnableGet();
}

int cRingBufferLinear::Put(const uchar *Data, int Count)
{
  if (Count > 0) {
     Lock();
     int rest = Size() - head;
     int diff = tail - head;
     int free = (diff > 0) ? diff - 1 : Size() + diff - 1;
     if (statistics) {
        int fill = Size() - free - 1 + Count;
        if (fill >= Size())
           fill = Size() - 1;
        if (fill > maxFill)
           maxFill = fill;
        int percent = maxFill * 100 / (Size() - 1) / 5 * 5;
        if (abs(lastPercent - percent) >= 5) {
           if (percent > 75)
              dsyslog("buffer usage: %d%% (pid=%d)", percent, getThreadPid);
           lastPercent = percent;
           }
        }
     if (free > 0) {
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
     else
        Count = 0;
     Unlock();
     EnableGet();
     }
  return Count;
}

int cRingBufferLinear::Get(uchar *Data, int Count)
{
  if (Count > 0) {
     Lock();
     if (getThreadPid < 0)
        getThreadPid = getpid();
     int rest = Size() - tail;
     int diff = head - tail;
     int cont = (diff >= 0) ? diff : Size() + diff;
     if (rest > 0) {
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
     else
        Count = 0;
     Unlock();
     if (Count == 0)
        WaitForGet();
     }
  return Count;
}

// --- cFrame ----------------------------------------------------------------

cFrame::cFrame(const uchar *Data, int Count, eFrameType Type, int Index)
{
  count = abs(Count);
  type = Type;
  index = Index;
  if (Count < 0)
     data = (uchar *)Data;
  else {
     data = new uchar[count];
     if (data)
        memcpy(data, Data, count);
     else
        esyslog("ERROR: can't allocate frame buffer (count=%d)", count);
     }
  next = NULL;
}

cFrame::~cFrame()
{
  delete data;
}

// --- cRingBufferFrame ------------------------------------------------------

cRingBufferFrame::cRingBufferFrame(int Size, bool Statistics)
:cRingBuffer(Size, Statistics)
{
  head = NULL;
  currentFill = 0;
}

cRingBufferFrame::~cRingBufferFrame()
{
  Clear();
}

void cRingBufferFrame::Clear(void)
{
  Lock();
  const cFrame *p;
  while ((p = Get()) != NULL)
        Drop(p);
  Unlock();
  EnablePut();
  EnableGet();
}

bool cRingBufferFrame::Put(cFrame *Frame)
{
  if (Frame->Count() <= Free()) {
     Lock();
     if (head) {
        Frame->next = head->next;
        head->next = Frame;
        head = Frame;
        }
     else {
        head = Frame->next = Frame;
        }
     currentFill += Frame->Count();
     Unlock();
     EnableGet();
     return true;
     }
  return false;
}

const cFrame *cRingBufferFrame::Get(void)
{
  Lock();
  cFrame *p = head ? head->next : NULL;
  Unlock();
  return p;
}

void cRingBufferFrame::Delete(const cFrame *Frame)
{
  currentFill -= Frame->Count();
  delete Frame;
}

void cRingBufferFrame::Drop(const cFrame *Frame)
{
  Lock();
  if (head) {
     if (Frame == head->next) {
        if (head->next != head) {
           head->next = Frame->next;
           Delete(Frame);
           }
        else {
           Delete(head);
           head = NULL;
           }
        }
     else
        esyslog("ERROR: attempt to drop wrong frame from ring buffer!");
     }
  Unlock();
  EnablePut();
}

int cRingBufferFrame::Available(void)
{
  Lock();
  int av = currentFill;
  Unlock();
  return av;
}
