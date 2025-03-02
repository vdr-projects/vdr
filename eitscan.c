/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 5.8 2025/03/02 11:03:35 kls Exp $
 */

#include "eitscan.h"
#include <stdlib.h>
#include "channels.h"
#include "dvbdevice.h"
#include "skins.h"
#include "transfer.h"

// --- cScanData -------------------------------------------------------------

class cScanData : public cListObject {
private:
  cChannel channel;
public:
  cScanData(const cChannel *Channel);
  virtual int Compare(const cListObject &ListObject) const override;
  int Source(void) const { return channel.Source(); }
  int Transponder(void) const { return channel.Transponder(); }
  const cChannel *GetChannel(void) const { return &channel; }
  };

cScanData::cScanData(const cChannel *Channel)
{
  channel = *Channel;
}

int cScanData::Compare(const cListObject &ListObject) const
{
  const cScanData *sd = (const cScanData *)&ListObject;
  int r = Source() - sd->Source();
  if (r == 0)
     r = Transponder() - sd->Transponder();
  return r;
}

// --- cScanList -------------------------------------------------------------

class cScanList : public cList<cScanData> {
private:
  bool HasDeviceForChannelEIT(const cChannel *Channel) const;
public:
  void AddTransponders(const cList<cChannel> *Channels);
  void AddTransponder(const cChannel *Channel);
  };

bool cScanList::HasDeviceForChannelEIT(const cChannel *Channel) const
{
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      cDevice *Device = cDevice::GetDevice(i);
      if (Device && Device->ProvidesEIT() && Device->ProvidesTransponder(Channel))
         return true;
      }
  return false;
}

void cScanList::AddTransponders(const cList<cChannel> *Channels)
{
  for (const cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch))
      AddTransponder(ch);
  Sort();
}

void cScanList::AddTransponder(const cChannel *Channel)
{
  if (Channel->Source() && Channel->Transponder() && (Setup.EPGScanMaxChannel <= 0 || Channel->Number() < Setup.EPGScanMaxChannel)) {
     if (!HasDeviceForChannelEIT(Channel))
        return;
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
  paused = false;
  lastScan = 0;
  lastActivity = time(NULL);
  currentChannel = 0;
  scanList = new cScanList;
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
     LOCK_CHANNELS_READ;
     Channels->SwitchTo(currentChannel);
     currentChannel = 0;
     }
  lastActivity = time(NULL);
}

void cEITScanner::Process(void)
{
  if (Setup.EPGScanTimeout || !lastActivity) { // !lastActivity means a scan was forced
     time_t now = time(NULL);
     if (now - lastScan > ScanTimeout && now - lastActivity > ActivityTimeout) {
        if (Setup.EPGPauseAfterScan && scanList->Count() == 0 && lastActivity && lastScan && now - lastScan < Setup.EPGScanTimeout * 3600) {
           if (!paused) {
              dsyslog("pause EPG scan");
              paused = true;
              }
           // Allow unused devices to go into power save mode:
           if ((now - lastScan) % 10 == 0) { // let's not do this too often
              for (int i = 0; i < cDevice::NumDevices(); i++) {
                  if (cDevice *Device = cDevice::GetDevice(i))
                     Device->SetPowerSaveIfUnused();
                  }
               }
           return; // pause for Setup.EPGScanTimeout hours
           }
        else if (paused) {
           dsyslog("start EPG scan");
           paused = false;
           }
        cStateKey StateKey;
        if (const cChannels *Channels = cChannels::GetChannelsRead(StateKey, 10)) {
           if (scanList->Count() == 0) {
              if (transponderList) {
                 scanList->AddTransponders(transponderList);
                 delete transponderList;
                 transponderList = NULL;
                 }
              scanList->AddTransponders(Channels);
              //dsyslog("EIT scan: %d scanList entries", scanList->Count());
              }
           for (int i = 0; i < cDevice::NumDevices(); i++) {
               cDevice *Device = cDevice::GetDevice(i);
               if (Device && Device->ProvidesEIT()) {
                  cScanData *Next = NULL;
                  for (cScanData *ScanData = scanList->First(); ScanData; ScanData = Next) {
                      Next = scanList->Next(ScanData);
                      const cChannel *Channel = ScanData->GetChannel();
                      if (Channel) {
                         if (Device->IsTunedToTransponder(Channel))
                            scanList->Del(ScanData);
                         else if (!Channel->Ca() || Channel->Ca() == Device->DeviceNumber() + 1 || Channel->Ca() >= CA_ENCRYPTED_MIN) {
                            if (Device->ProvidesTransponder(Channel)) {
                               if (Device->Priority() < 0) {
                                  if (const cPositioner *Positioner = Device->Positioner()) {
                                     if (Positioner->LastLongitude() != cSource::Position(Channel->Source()))
                                        continue;
                                     }
                                  bool MaySwitchTransponder = Device->MaySwitchTransponder(Channel);
                                  if (MaySwitchTransponder || Device->ProvidesTransponderExclusively(Channel) && now - lastActivity > Setup.EPGScanTimeout * 3600) {
                                     if (!MaySwitchTransponder) {
                                        if (Device == cDevice::ActualDevice() && !currentChannel) {
                                           cDevice::PrimaryDevice()->StopReplay(); // stop transfer mode
                                           currentChannel = Device->CurrentChannel();
                                           Skins.Message(mtInfo, tr("Starting EPG scan"));
                                           }
                                        }
                                     //dsyslog("EIT scan: %d device %d  source  %-8s tp %5d", scanList->Count(), Device->DeviceNumber() + 1, *cSource::ToString(Channel->Source()), Channel->Transponder());
                                     Device->SwitchChannel(Channel, false);
                                     scanList->Del(ScanData);
                                     break;
                                     }
                                  }
                               }
                            }
                         }
                      }
                  }
               }
           if (scanList->Count() == 0) {
              if (lastActivity == 0) // this was a triggered scan
                 Activity();
              }
           StateKey.Remove();
           }
        lastScan = time(NULL);
        }
     }
}
