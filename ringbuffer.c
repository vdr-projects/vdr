/*
 * ringbuffer.c: A ring buffer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Parts of this file were inspired by the 'ringbuffy.c' from the
 * LinuxDVB driver (see linuxtv.org).
 *
 * $Id: ringbuffer.c 1.17 2003/05/12 17:38:11 kls Exp $
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
  putTimeout = getTimeout = 0;
}

cRingBuffer::~cRingBuffer()
{
  if (statistics)
     dsyslog("buffer stats: %d (%d%%) used", maxFill, maxFill * 100 / (size - 1));
}

void cRingBuffer::WaitForPut(void)
{
  if (putTimeout) {
     putMutex.Lock();
     readyForPut.TimedWait(putMutex, putTimeout);
     putMutex.Unlock();
     }
}

void cRingBuffer::WaitForGet(void)
{
  if (getTimeout) {
     getMutex.Lock();
     readyForGet.TimedWait(getMutex, getTimeout);
     getMutex.Unlock();
     }
}

void cRingBuffer::EnablePut(void)
{
  if (putTimeout)
     readyForPut.Broadcast();
}

void cRingBuffer::EnableGet(void)
{
  if (getTimeout)
     readyForGet.Broadcast();
}

void cRingBuffer::SetTimeouts(int PutTimeout, int GetTimeout)
{
  putTimeout = PutTimeout;
  getTimeout = GetTimeout;
}

// --- cRingBufferLinear -----------------------------------------------------

cRingBufferLinear::cRingBufferLinear(int Size, int Margin, bool Statistics)
:cRingBuffer(Size, Statistics)
{
  margin = Margin;
  buffer = NULL;
  getThreadPid = -1;
  if (Size > 1) { // 'Size - 1' must not be 0!
     buffer = MALLOC(uchar, Size);
     if (!buffer)
        esyslog("ERROR: can't allocate ring buffer (size=%d)", Size);
     Clear();
     }
  else
     esyslog("ERROR: illegal size for ring buffer (%d)", Size);
}

cRingBufferLinear::~cRingBufferLinear()
{
  free(buffer);
}

int cRingBufferLinear::Available(void)
{
  Lock();
  int diff = head - tail;
  Unlock();
  return (diff >= 0) ? diff : Size() + diff - margin;
}

void cRingBufferLinear::Clear(void)
{
  Lock();
  head = tail = margin;
  lastGet = -1;
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
     int free = ((tail < margin) ? rest : (diff > 0) ? diff : Size() + diff - margin) - 1;
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
              memcpy(buffer + margin, Data + rest, Count - rest);
           head = margin + Count - rest;
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
     if (Count == 0)
        WaitForPut();
     }
  return Count;
}

uchar *cRingBufferLinear::Get(int &Count)
{
  uchar *p = NULL;
  Lock();
  if (getThreadPid < 0)
     getThreadPid = getpid();
  int rest = Size() - tail;
  if (rest < margin && head < tail) {
     int t = margin - rest;
     memcpy(buffer + t, buffer + tail, rest);
     tail = t;
     }
  int diff = head - tail;
  int cont = (diff >= 0) ? diff : Size() + diff - margin;
  if (cont > rest)
     cont = rest;
  if (cont >= margin) {
     p = buffer + tail;
     Count = lastGet = cont;
     }
  Unlock();
  if (!p)
     WaitForGet();
  return p;
}

void cRingBufferLinear::Del(int Count)
{
  if (Count > 0 && Count <= lastGet) {
     Lock();
     tail += Count;
     lastGet -= Count;
     if (tail >= Size())
        tail = margin;
     Unlock();
     EnablePut();
     }
  else
     esyslog("ERROR: invalid Count in cRingBufferLinear::Del: %d", Count);
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
     data = MALLOC(uchar, count);
     if (data)
        memcpy(data, Data, count);
     else
        esyslog("ERROR: can't allocate frame buffer (count=%d)", count);
     }
  next = NULL;
}

cFrame::~cFrame()
{
  free(data);
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
  cFrame *p;
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

cFrame *cRingBufferFrame::Get(void)
{
  Lock();
  cFrame *p = head ? head->next : NULL;
  Unlock();
  return p;
}

void cRingBufferFrame::Delete(cFrame *Frame)
{
  currentFill -= Frame->Count();
  delete Frame;
}

void cRingBufferFrame::Drop(cFrame *Frame)
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
