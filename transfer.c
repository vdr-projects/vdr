/*
 * transfer.c: Transfer mode
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: transfer.c 2.4 2009/12/06 14:22:23 kls Exp $
 */

#include "transfer.h"

// --- cTransfer -------------------------------------------------------------

cTransfer::cTransfer(tChannelID ChannelID, int VPid, const int *APids, const int *DPids, const int *SPids)
:cReceiver(ChannelID, -1, VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids)
{
  patPmtGenerator.SetChannel(Channels.GetByChannelID(ChannelID));
}

cTransfer::~cTransfer()
{
  cReceiver::Detach();
  cPlayer::Detach();
}

void cTransfer::Activate(bool On)
{
  if (On) {
     PlayTs(patPmtGenerator.GetPat(), TS_SIZE);
     int Index = 0;
     while (uchar *pmt = patPmtGenerator.GetPmt(Index))
           PlayTs(pmt, TS_SIZE);
     }
  else
     cPlayer::Detach();
}

void cTransfer::Receive(uchar *Data, int Length)
{
  if (cPlayer::IsAttached()) {
     // Transfer Mode means "live tv", so there's no point in doing any additional
     // buffering here. The TS packets *must* get through here! However, every
     // now and then there may be conditions where the packet just can't be
     // handled when offered the first time, so that's why we try several times:
     for (int i = 0; i < 100; i++) {
         if (PlayTs(Data, Length) > 0)
            return;
         cCondWait::SleepMs(10);
         }
     esyslog("ERROR: TS packet not accepted in Transfer Mode");
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
