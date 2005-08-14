/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.30 2005/08/14 10:55:03 kls Exp $
 */

#include "transfer.h"

#define TRANSFERBUFSIZE  MEGABYTE(2)
#define POLLTIMEOUTS_BEFORE_DEVICECLEAR 6

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(int VPid, const int *APids, const int *DPids, const int *SPids)
:cReceiver(0, -1, VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids)
,cThread("transfer")
{
  ringBuffer = new cRingBufferLinear(TRANSFERBUFSIZE, TS_SIZE * 2, true, "Transfer");
  remux = new cRemux(VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids);
  needsBufferReserve = Setup.UseDolbyDigital && VPid != 0 && DPids && DPids[0] != 0;
}

cTransfer::~cTransfer()
{
  cReceiver::Detach();
  cPlayer::Detach();
  delete remux;
  delete ringBuffer;
}

void cTransfer::Activate(bool On)
{
  if (On)
     Start();
  else
     Cancel(3);
}

void cTransfer::Receive(uchar *Data, int Length)
{
  if (IsAttached() && Running()) {
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && Running())
        ringBuffer->ReportOverflow(Length - p);
     return;
     }
}

#define FW_NEEDS_BUFFER_RESERVE_FOR_AC3
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
//XXX This is a very ugly hack to allow cDvbOsd to reduce the buffer
//XXX requirements in cTransfer if it detects a 4MB full featured DVB card.
bool DvbCardWith4MBofSDRAM = false;
#endif

void cTransfer::Action(void)
{
  int PollTimeouts = 0;
  uchar *p = NULL;
  int Result = 0;
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
  bool GotBufferReserve = false;
  int RequiredBufferReserve = KILOBYTE(DvbCardWith4MBofSDRAM ? 288 : 576);
#endif
  while (Running()) {
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
        if (needsBufferReserve && !GotBufferReserve) {
           //XXX For dolby we've to fill the buffer because the firmware does
           //XXX not decode dolby but use a PCM stream for transport, therefore
           //XXX the firmware has not enough buffer for noiseless skipping early
           //XXX PCM samples (each dolby frame requires 6144 bytes in PCM and
           //XXX audio is mostly to early in comparison to video).
           //XXX To resolve this, the remuxer or PlayPes() should synchronize
           //XXX audio with the video frames. 2004/09/09 Werner
           if (ringBuffer->Available() < RequiredBufferReserve) { // used to be MAXFRAMESIZE, but the HDTV value of KILOBYTE(512) is way too much here
              cCondWait::SleepMs(20); // allow the buffer to collect some reserve
              continue;
              }
           else
              GotBufferReserve = true;
           }
#endif
        int Count;
        uchar *b = ringBuffer->Get(Count);
        if (b) {
           if (ringBuffer->Available() > TRANSFERBUFSIZE * 9 / 10) {
              // If the buffer runs full, we have no chance of ever catching up
              // since the data comes in at the same rate as it goes out (it's "live").
              // So let's clear the buffer instead of suffering from permanent
              // overflows.
              dsyslog("clearing transfer buffer to avoid overflows");
              DeviceClear();
              ringBuffer->Clear();
              remux->Clear();
              PlayPes(NULL, 0);
              p = NULL;
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
              GotBufferReserve = false;
#endif
              continue;
              }
           Count = remux->Put(b, Count);
           if (Count)
              ringBuffer->Del(Count);
           }
        if (!p)
           p = remux->Get(Result);
        if (p) {
           cPoller Poller;
           if (DevicePoll(Poller, 100)) {
              PollTimeouts = 0;
              int w = PlayPes(p, Result);
              if (w > 0) {
                 p += w;
                 Result -= w;
                 remux->Del(w);
                 if (Result <= 0)
                    p = NULL;
                 }
              else if (w < 0 && FATALERRNO)
                 LOG_ERROR;
              }
           else {
              PollTimeouts++;
              if (PollTimeouts == POLLTIMEOUTS_BEFORE_DEVICECLEAR) {
                 dsyslog("clearing device because of consecutive poll timeouts");
                 DeviceClear();
                 ringBuffer->Clear();
                 remux->Clear();
                 PlayPes(NULL, 0);
                 p = NULL;
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
                 GotBufferReserve = false;
#endif
                 }
              }
           }
        }
}

// --- cTransferControl ------------------------------------------------------

cDevice *cTransferControl::receiverDevice = NULL;

cTransferControl::cTransferControl(cDevice *ReceiverDevice, int VPid, const int *APids, const int *DPids, const int *SPids)
:cControl(transfer = new cTransfer(VPid, APids, DPids, SPids), true)
{
  ReceiverDevice->AttachReceiver(transfer);
  receiverDevice = ReceiverDevice;
}

cTransferControl::~cTransferControl()
{
  receiverDevice = NULL;
  delete transfer;
}
