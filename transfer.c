/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 1.2 2002/06/23 12:56:49 kls Exp $
 */

#include "transfer.h"

//XXX+ also used in recorder.c - find a better place???
// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(1)

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cReceiver(0, 0, 5, VPid, APid1, APid2, DPid1, DPid2)
{
  ringBuffer = new cRingBufferLinear(VIDEOBUFSIZE, true);
  remux = new cRemux(VPid, APid1, APid2, DPid1, DPid2);
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
  int p = ringBuffer->Put(Data, Length);
  if (p != Length && active)
     esyslog("ERROR: ring buffer overflow (%d bytes dropped)", Length - p);
}

void cTransfer::Action(void)
{
  dsyslog("transfer thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  int r = 0;
  active = true;
  while (active) {

        //XXX+ Maybe we need this to avoid "buffer empty" log messages from the driver.
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

        int g = ringBuffer->Get(b + r, sizeof(b) - r);
        if (g > 0)
           r += g;

        // Play the data:

        if (r > 0) {
           int Count = r, Result;
           const uchar *p = remux->Process(b, Count, Result);
           if (p) {
              //XXX+ StripAudio???
              while (Result > 0 && active) {
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
           if (Count > 0) {
              r -= Count;
              if (r > 0)
                 memmove(b, b + Count, r);
              }
           }
        }

  dsyslog("transfer thread ended (pid=%d)", getpid());
}

void cTransfer::SetAudioPid(int APid)
{
  /*XXX+
  Clear();
  //XXX we may need to have access to the audio device, too, in order to clear it
  CHECK(ioctl(toDevice, VIDEO_CLEAR_BUFFER));
  gotBufferReserve = false;
  remux.SetAudioPid(APid);
  XXX*/
}

// --- cTransferControl ------------------------------------------------------

cTransferControl::cTransferControl(cDevice *ReceiverDevice, int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cControl(transfer = new cTransfer(VPid, APid1, APid2, DPid1, DPid2), true)
{
  ReceiverDevice->AttachReceiver(transfer);
}

cTransferControl::~cTransferControl()
{
  delete transfer;
}
