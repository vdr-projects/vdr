/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.22 2005/01/07 15:44:30 kls Exp $
 */

#include "transfer.h"

#define TRANSFERBUFSIZE  MEGABYTE(2)
#define POLLTIMEOUTS_BEFORE_DEVICECLEAR 3

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cReceiver(0, -1, 5, VPid, APid1, APid2, DPid1, DPid2)
,cThread("transfer")
{
  ringBuffer = new cRingBufferLinear(TRANSFERBUFSIZE, TS_SIZE * 2, true, "Transfer");
  remux = new cRemux(VPid, APid1, APid2, DPid1, DPid2);
  needsBufferReserve = VPid != 0 && DPid1 != 0;
  active = false;
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
  if (On) {
     if (!active)
        Start();
     }
  else if (active) {
     active = false;
     Cancel(3);
     }
}

void cTransfer::Receive(uchar *Data, int Length)
{
  if (IsAttached() && active) {
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && active)
        ringBuffer->ReportOverflow(Length - p);
     return;
     }
}

void cTransfer::Action(void)
{
  int PollTimeouts = 0;
  uchar *p = NULL;
  int Result = 0;
#define FW_NEEDS_BUFFER_RESERVE_FOR_AC3
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
  bool Cleared = false;
  bool GotBufferReserve = false;
#endif
  active = true;
  while (active) {
#ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
        if (needsBufferReserve) {
           if (IsAttached() && !Cleared) {
              PlayPes(NULL, 0);
              Cleared = true;
              }
           //XXX For dolby we've to fill the buffer because the firmware does
           //XXX not decode dolby but use a PCM stream for transport, therefore
           //XXX the firmware has not enough buffer for noiseless skipping early
           //XXX PCM samples (each dolby frame requires 6144 bytes in PCM and
           //XXX audio is mostly to early in comparison to video).
           //XXX To resolve this, the remuxer or PlayPes() should synchronize
           //XXX audio with the video frames. 2004/09/09 Werner
           if (!GotBufferReserve) {
              if (ringBuffer->Available() < 3 * MAXFRAMESIZE / 2) {
                 cCondWait::SleepMs(20); // allow the buffer to collect some reserve
                 continue;
                 }
              else
                 GotBufferReserve = true;
              }
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
              ringBuffer->Clear();
              remux->Clear();
              p = NULL;
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
                 p = NULL;
                 }
              }
           }
        }
  active = false;
}

// --- cTransferControl ------------------------------------------------------

cDevice *cTransferControl::receiverDevice = NULL;

cTransferControl::cTransferControl(cDevice *ReceiverDevice, int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cControl(transfer = new cTransfer(VPid, APid1, APid2, DPid1, DPid2), true)
{
  ReceiverDevice->AttachReceiver(transfer);
  receiverDevice = ReceiverDevice;
}

cTransferControl::~cTransferControl()
{
  receiverDevice = NULL;
  delete transfer;
}
