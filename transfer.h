/*
 * transfer.h: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.h 1.4 2003/05/11 08:48:36 kls Exp $
 */

#ifndef __TRANSFER_H
#define __TRANSFER_H

#include "player.h"
#include "receiver.h"
#include "remux.h"
#include "ringbuffer.h"
#include "thread.h"

class cTransfer : public cReceiver, public cPlayer, public cThread {
private:
  cRingBufferLinear *ringBuffer;
  cRemux *remux;
  bool canToggleAudioTrack;
  uchar audioTrack;
  bool gotBufferReserve;
  bool active;
  void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00);
protected:
  virtual void Activate(bool On);
  virtual void Receive(uchar *Data, int Length);
  virtual void Action(void);
public:
  cTransfer(int VPid, int APid1, int APid2, int DPid1, int DPid2);
  virtual ~cTransfer();
  virtual int NumAudioTracks(void) const;
  virtual const char **GetAudioTracks(int *CurrentTrack = NULL) const;
  virtual void SetAudioTrack(int Index);
  };

class cTransferControl : public cControl {
private:
  cTransfer *transfer;
  static cDevice *receiverDevice;
public:
  cTransferControl(cDevice *ReceiverDevice, int VPid, int APid1, int APid2, int DPid1, int DPid2);
  ~cTransferControl();
  virtual void Hide(void) {}
  static cDevice *ReceiverDevice(void) { return receiverDevice; }
  };

#endif //__TRANSFER_H
