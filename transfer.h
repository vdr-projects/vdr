/*
 * transfer.h: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __TRANSFER_H
#define __TRANSFER_H

#include "player.h"
#include "receiver.h"
#include "remux.h"

class cTransfer : public cReceiver, public cPlayer {
private:
  time_t lastErrorReport;
  int numLostPackets;
  cPatPmtGenerator patPmtGenerator;
protected:
  virtual void Activate(bool On) override;
  virtual void Receive(const uchar *Data, int Length) override;
public:
  cTransfer(const cChannel *Channel);
  virtual ~cTransfer() override;
  };

class cTransferControl : public cControl {
private:
  cTransfer *transfer;
  static cDevice *receiverDevice;
public:
  cTransferControl(cDevice *ReceiverDevice, const cChannel *Channel);
  ~cTransferControl();
  virtual void Hide(void) override {}
  static cDevice *ReceiverDevice(void) { return receiverDevice; }
  };

#endif //__TRANSFER_H
