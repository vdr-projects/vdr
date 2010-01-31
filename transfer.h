/*
 * transfer.h: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.h 2.2 2010/01/29 16:38:09 kls Exp $
 */

#ifndef __TRANSFER_H
#define __TRANSFER_H

#include "player.h"
#include "receiver.h"
#include "remux.h"

class cTransfer : public cReceiver, public cPlayer {
private:
  cPatPmtGenerator patPmtGenerator;
protected:
  virtual void Activate(bool On);
  virtual void Receive(uchar *Data, int Length);
public:
  cTransfer(const cChannel *Channel);
  virtual ~cTransfer();
  };

class cTransferControl : public cControl {
private:
  cTransfer *transfer;
  static cDevice *receiverDevice;
public:
  cTransferControl(cDevice *ReceiverDevice, const cChannel *Channel);
  ~cTransferControl();
  virtual void Hide(void) {}
  static cDevice *ReceiverDevice(void) { return receiverDevice; }
  };

#endif //__TRANSFER_H
