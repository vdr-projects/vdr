/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 1.18 2004/01/11 15:50:59 kls Exp $
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
  cChannel *GetChannel(cList<cChannel> *Channels);
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

cChannel *cScanData::GetChannel(cList<cChannel> *Channels)
{
  for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
      if (!Channel->GroupSep() && Channel->Source() == source && ISTRANSPONDER(Channel->Transponder(), transponder))
         return Channel;
      }
  return NULL;
}

// --- cScanList -------------------------------------------------------------

class cScanList : public cList<cScanData> {
public:
  cScanList(cList<cChannel> *Channels);
  void AddTransponder(const cChannel *Channel);
  };

cScanList::cScanList(cList<cChannel> *Channels)
{
  for (cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch))
      AddTransponder(ch);
  Sort();
}

void cScanList::AddTransponder(const cChannel *Channel)
{
  for (cScanData *sd = First(); sd; sd = Next(sd)) {
      if (sd->Source() == Channel->Source() && ISTRANSPONDER(sd->Transponder(), Channel->Transponder()))
         return;
      }
  Add(new cScanData(Channel->Source(), Channel->Transponder()));
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
  numScan = 0;
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
           cList<cChannel> *ChannelList = &Channels;
           if (numScan % 2 == 0 && transponderList) // switch between the list of new transponders and the actual channels in case there are transponders w/o any channels
              ChannelList = transponderList;
           if (!scanList)
              scanList = new cScanList(ChannelList);
           for (bool AnyDeviceSwitched = false; !AnyDeviceSwitched; ) {
               cScanData *ScanData = NULL;
               for (int i = 0; i < cDevice::NumDevices(); i++) {
                   if (ScanData || (ScanData = scanList->First()) != NULL) {
                      cDevice *Device = cDevice::GetDevice(i);
                      if (Device) {
                         if (Device != cDevice::PrimaryDevice() || (cDevice::NumDevices() == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                            if (!(Device->Receiving(true) || Device->Replaying())) {
                               cChannel *Channel = ScanData->GetChannel(ChannelList);
                               if (Channel) {
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
                  numScan++;
                  if (ChannelList == transponderList) {
                     delete transponderList;
                     transponderList = NULL;
                     }
                  break;
                  }
               }
           }
        lastScan = time(NULL);
        Channels.Unlock();
        }
     }
}
