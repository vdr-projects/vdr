/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.34 2007/01/05 10:45:28 kls Exp $
 */

#include "transfer.h"

#define TRANSFERBUFSIZE  MEGABYTE(2)
#define POLLTIMEOUTS_BEFORE_DEVICECLEAR 6

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(tChannelID ChannelID, int VPid, const int *APids, const int *DPids, const int *SPids)
:cReceiver(ChannelID, -1, VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids)
,cThread("transfer")
{
  ringBuffer = new cRingBufferLinear(TRANSFERBUFSIZE, TS_SIZE * 2, true, "Transfer");
  remux = new cRemux(VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids);
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
  else {
     Cancel(3);
     cPlayer::Detach();
     }
}

void cTransfer::Receive(uchar *Data, int Length)
{
  if (cPlayer::IsAttached() && Running()) {
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && Running())
        ringBuffer->ReportOverflow(Length - p);
     }
}

void cTransfer::Action(void)
{
  int PollTimeouts = 0;
  uchar *p = NULL;
  int Result = 0;
  while (Running()) {
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
                 }
              }
           }
        }
}

// --- cTransferControl ------------------------------------------------------

cDevice *cTransferControl::receiverDevice = NULL;

cTransferControl::cTransferControl(cDevice *ReceiverDevice, tChannelID ChannelID, int VPid, const int *APids, const int *DPids, const int *SPids)
:cControl(transfer = new cTransfer(ChannelID, VPid, APids, DPids, SPids), true)
{
  ReceiverDevice->AttachReceiver(transfer);
  receiverDevice = ReceiverDevice;
}

cTransferControl::~cTransferControl()
{
  receiverDevice = NULL;
  delete transfer;
}
