/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 1.21 2004/02/14 13:44:31 kls Exp $
 */

#include "eitscan.h"
#include <stdlib.h>
#include "channels.h"
#include "dvbdevice.h"
#include "interface.h"

// --- cScanData -------------------------------------------------------------

class cScanData : public cListObject {
private:
  cChannel channel;
public:
  cScanData(const cChannel *Channel);
  virtual bool operator< (const cListObject &ListObject);
  int Source(void) { return channel.Source(); }
  int Transponder(void) { return channel.Transponder(); }
  const cChannel *GetChannel(void) { return &channel; }
  };

cScanData::cScanData(const cChannel *Channel)
{
  channel = *Channel;
}

bool cScanData::operator< (const cListObject &ListObject)
{
  cScanData *sd = (cScanData *)&ListObject;
  return Source() < sd->Source() || Source() == sd->Source() && Transponder() < sd->Transponder();
}

// --- cScanList -------------------------------------------------------------

class cScanList : public cList<cScanData> {
public:
  void AddTransponders(cList<cChannel> *Channels);
  void AddTransponder(const cChannel *Channel);
  };

void cScanList::AddTransponders(cList<cChannel> *Channels)
{
  for (cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch))
      AddTransponder(ch);
  Sort();
}

void cScanList::AddTransponder(const cChannel *Channel)
{
  if (Channel->Source() && Channel->Transponder()) {
     for (cScanData *sd = First(); sd; sd = Next(sd)) {
         if (sd->Source() == Channel->Source() && ISTRANSPONDER(sd->Transponder(), Channel->Transponder()))
            return;
         }
     Add(new cScanData(Channel));
     }
}

// --- cTransponderList ------------------------------------------------------

class cTransponderList : public cList<cChannel> {
public:
  void AddTransponder(cChannel *Channel);
  };

void cTransponderList::AddTransponder(cChannel *Channel)
{
  for (cChannel *ch = First(); ch; ch = Next(ch)) {
      if (ch->Source() == Channel->Source() && ch->Transponder() == Channel->Transponder()) {
         delete Channel;
         return;
         }
      }
  Add(Channel);
}

// --- cEITScanner -----------------------------------------------------------

cEITScanner EITScanner;

cEITScanner::cEITScanner(void)
{
  lastScan = lastActivity = time(NULL);
  currentDevice = NULL;
  currentChannel = 0;
  scanList = NULL;
  transponderList = NULL;
}

cEITScanner::~cEITScanner()
{
  delete scanList;
  delete transponderList;
}

void cEITScanner::AddTransponder(cChannel *Channel)
{
  if (!transponderList)
     transponderList = new cTransponderList;
  transponderList->AddTransponder(Channel);
}

void cEITScanner::ForceScan(void)
{
  lastActivity = 0;
}

void cEITScanner::Activity(void)
{
  if (currentChannel) {
     Channels.SwitchTo(currentChannel);
     currentChannel = 0;
     }
  lastActivity = time(NULL);
}

void cEITScanner::Process(void)
{
  if (Setup.EPGScanTimeout && Channels.MaxNumber() > 1) {
     time_t now = time(NULL);
     if (now - lastScan > ScanTimeout && now - lastActivity > ActivityTimeout) {
        if (Channels.Lock(false, 10)) {
           if (!scanList) {
              scanList = new cScanList;
              scanList->AddTransponders(&Channels);
              if (transponderList) {
                 scanList->AddTransponders(transponderList);
                 delete transponderList;
                 transponderList = NULL;
                 }
              }
           for (bool AnyDeviceSwitched = false; !AnyDeviceSwitched; ) {
               cScanData *ScanData = NULL;
               for (int i = 0; i < cDevice::NumDevices(); i++) {
                   if (ScanData || (ScanData = scanList->First()) != NULL) {
                      cDevice *Device = cDevice::GetDevice(i);
                      if (Device) {
                         if (Device != cDevice::PrimaryDevice() || (cDevice::NumDevices() == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                            if (!(Device->Receiving(true) || Device->Replaying())) {
                               const cChannel *Channel = ScanData->GetChannel();
                               if (Channel) {
                                  if ((!Channel->Ca() || Channel->Ca() == Device->DeviceNumber() + 1 || Channel->Ca() >= 0x0100) && Device->ProvidesTransponder(Channel)) {
                                     if (Device == cDevice::PrimaryDevice() && !currentChannel) {
                                        currentChannel = Device->CurrentChannel();
                                        Interface->Info("Starting EPG scan");
                                        }
                                     currentDevice = Device;//XXX see also dvbdevice.c!!!
                                     Device->SwitchChannel(Channel, false);
                                     currentDevice = NULL;
                                     scanList->Del(ScanData);
                                     ScanData = NULL;
                                     AnyDeviceSwitched = true;
                                     }
                                  }
                               }
                            }
                         }
                      }
                   else
                      break;
                   }
               if (ScanData && !AnyDeviceSwitched) {
                  scanList->Del(ScanData);
                  ScanData = NULL;
                  }
               if (!scanList->Count()) {
                  delete scanList;
                  scanList = NULL;
                  if (lastActivity == 0) // this was a triggered scan
                     Activity();
                  break;
                  }
               }
           }
        lastScan = time(NULL);
        Channels.Unlock();
        }
     }
}
