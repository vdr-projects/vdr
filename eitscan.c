/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 1.15 2004/01/04 12:28:00 kls Exp $
 */

#include "eitscan.h"
#include <stdlib.h>
#include "channels.h"
#include "dvbdevice.h"

// --- cScanData -------------------------------------------------------------

class cScanData : public cListObject {
private:
  int source;
  int transponder;
public:
  cScanData(int Source, int Transponder);
  virtual bool operator< (const cListObject &ListObject);
  int Source(void) { return source; }
  int Transponder(void) { return transponder; }
  cChannel *GetChannel(void);
  };

cScanData::cScanData(int Source, int Transponder)
{
  source = Source;
  transponder = Transponder;
}

bool cScanData::operator< (const cListObject &ListObject)
{
  cScanData *sd = (cScanData *)&ListObject;
  return source < sd->source || source == sd->source && transponder < sd->transponder;
}

//XXX this might be done differently later...
cChannel *cScanData::GetChannel(void)
{
  for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
      if (!Channel->GroupSep() && Channel->Source() == source && ISTRANSPONDER(Channel->Transponder(), transponder))
         return Channel;
      }
  return NULL;
}

// --- cScanList -------------------------------------------------------------

class cScanList : public cList<cScanData> {
public:
  cScanList(void);
  void AddTransponder(const cChannel *Channel);
  };

cScanList::cScanList(void)
{
  for (cChannel *ch = Channels.First(); ch; ch = Channels.Next(ch))
      AddTransponder(ch);
  Sort();
}

void cScanList::AddTransponder(const cChannel *Channel)
{
  for (cScanData *sd = First(); sd; sd = Next(sd)) {
      if (sd->Source() == Channel->Source() && sd->Transponder() == Channel->Transponder())
         return;
      }
  Add(new cScanData(Channel->Source(), Channel->Transponder()));
}

// --- cEITScanner -----------------------------------------------------------

cEITScanner EITScanner;

cEITScanner::cEITScanner(void)
{
  lastScan = lastActivity = time(NULL);
  currentDevice = NULL;
  currentChannel = 0;
  memset(lastChannel, 0, sizeof(lastChannel));
  scanList = NULL;
}

cEITScanner::~cEITScanner()
{
  delete scanList;
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
           if (!scanList)
              scanList = new cScanList();
           for (bool AnyDeviceSwitched = false; !AnyDeviceSwitched; ) {
               cScanData *ScanData = NULL;
               for (int i = 0; i < cDevice::NumDevices(); i++) {
                   cDevice *Device = cDevice::GetDevice(i);
                   if (Device) {
                      if (Device != cDevice::PrimaryDevice() || (cDevice::NumDevices() == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                         if (!(Device->Receiving(true) || Device->Replaying())) {
                            if (!ScanData)
                               ScanData = scanList->First();
                            if (ScanData) {
                               cChannel *Channel = ScanData->GetChannel();
                               //XXX if (Device->ProvidesTransponder(Channel)) {
                               if ((!Channel->Ca() || Channel->Ca() == Device->DeviceNumber() + 1 || Channel->Ca() >= 0x0100) && Device->ProvidesTransponder(Channel)) { //XXX temporary for the 'sky' plugin
                                  if (Device == cDevice::PrimaryDevice() && !currentChannel)
                                     currentChannel = Device->CurrentChannel();
                                  currentDevice = Device;//XXX see also dvbdevice.c!!!
                                  Device->SwitchChannel(Channel, false);
                                  currentDevice = NULL;
                                  scanList->Del(ScanData);
                                  ScanData = NULL;
                                  AnyDeviceSwitched = true;
                                  }
                               }
                            else
                               break;
                            }
                         }
                      }
                   }
               if (ScanData && !AnyDeviceSwitched) {
                  scanList->Del(ScanData);
                  ScanData = NULL;
                  }
               if (!scanList->Count()) {
                  delete scanList;
                  scanList = NULL;
                  break;
                  }
               }
           Channels.Unlock();
           lastScan = time(NULL);
           }
        }
     }
}
