/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.13 2003/05/18 15:22:09 kls Exp $
 */

#include "transfer.h"

//XXX+ also used in recorder.c - find a better place???
// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(1)

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cReceiver(0, -1, 5, VPid, APid1, APid2, DPid1, DPid2)
{
  ringBuffer = new cRingBufferLinear(VIDEOBUFSIZE, TS_SIZE * 2, true);
  remux = new cRemux(VPid, APid1, APid2, DPid1, DPid2);
  canToggleAudioTrack = false;
  audioTrack = 0xC0;
  gotBufferReserve = false;
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
  if (IsAttached()) {
     int i = 0;
     while (active && Length > 0) {
           if (i++ > 10) {
              esyslog("ERROR: ring buffer overflow (%d bytes dropped)", Length);
              break;
              }
           int p = ringBuffer->Put(Data, Length);
           Length -= p;
           Data += p;
           }
     }
}

void cTransfer::Action(void)
{
  dsyslog("transfer thread started (pid=%d)", getpid());

  active = true;
  while (active) {

        //XXX+ Maybe we need this to avoid buffer underruns in driver.
        //XXX+ But then again, it appears to play just fine without this...
        /*
        if (!gotBufferReserve) {
           if (ringBuffer->Available() < 4 * MAXFRAMESIZE) {
              usleep(100000); // allow the buffer to collect some reserve
              continue;
              }
           else
              gotBufferReserve = true;
           }
           */

        // Get data from the buffer:

        int r;
        const uchar *b = ringBuffer->Get(r);

        // Play the data:

        if (b) {
           int Count = r, Result;
           uchar *p = remux->Process(b, Count, Result);
           ringBuffer->Del(Count);
           if (p) {
              StripAudioPackets(p, Result, audioTrack);
              while (Result > 0 && active) {
                    cPoller Poller;
                    if (DevicePoll(Poller, 100)) {
                       int w = PlayVideo(p, Result);
                       if (w > 0) {
                          p += w;
                          Result -= w;
                          }
                       else if (w < 0 && FATALERRNO) {
                          LOG_ERROR;
                          break;
                          }
                       }
                    }
              }
           }
        else
           usleep(1); // this keeps the CPU load low
        }

  dsyslog("transfer thread ended (pid=%d)", getpid());
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
