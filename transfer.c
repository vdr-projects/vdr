/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.18 2004/10/23 13:35:08 kls Exp $
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
  canToggleAudioTrack = false;
  audioTrack = 0xC0;
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
  active = true;
  while (active) {
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
        if (!p && (p = remux->Get(Result)) != NULL)
           StripAudioPackets(p, Result, audioTrack);
        if (p) {
           cPoller Poller;
           if (DevicePoll(Poller, 100)) {
              PollTimeouts = 0;
              int w = PlayVideo(p, Result);
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

void cTransfer::StripAudioPackets(uchar *b, int Length, uchar Except)
{
  for (int i = 0; i < Length - 6; i++) {
      if (b[i] == 0x00 && b[i + 1] == 0x00 && b[i + 2] == 0x01) {
         uchar c = b[i + 3];
         int l = b[i + 4] * 256 + b[i + 5] + 6;
         switch (c) {
           case 0xBD: // dolby
                if (Except)
                   PlayAudio(&b[i], l);
                // continue with deleting the data - otherwise it disturbs DVB replay
           case 0xC0 ... 0xC1: // audio
                if (c == 0xC1)
                   canToggleAudioTrack = true;
                if (!Except || c != Except)
                   memset(&b[i], 0x00, min(l, Length-i));
                break;
           case 0xE0 ... 0xEF: // video
                break;
           default:
                //esyslog("ERROR: unexpected packet id %02X", c);
                l = 0;
           }
         if (l)
            i += l - 1; // the loop increments, too!
         }
      /*XXX
      else
         esyslog("ERROR: broken packet header");
         XXX*/
      }
}

int cTransfer::NumAudioTracks(void) const
{
  return canToggleAudioTrack ? 2 : 1;
}

const char **cTransfer::GetAudioTracks(int *CurrentTrack) const
{
  if (NumAudioTracks()) {
     if (CurrentTrack)
        *CurrentTrack = (audioTrack == 0xC0) ? 0 : 1;
     static const char *audioTracks1[] = { "Audio 1", NULL };
     static const char *audioTracks2[] = { "Audio 1", "Audio 2", NULL };
     return NumAudioTracks() > 1 ? audioTracks2 : audioTracks1;
     }
  return NULL;
}

void cTransfer::SetAudioTrack(int Index)
{
  if ((audioTrack == 0xC0) != (Index == 0)) {
     audioTrack = (Index == 1) ? 0xC1 : 0xC0;
     DeviceClear();
     }
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
