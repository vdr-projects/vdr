/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 1.13 2003/05/24 13:34:59 kls Exp $
 */

#include "eitscan.h"
#include <stdlib.h>
#include "channels.h"
#include "dvbdevice.h"

cEITScanner EITScanner;

cEITScanner::cEITScanner(void)
{
  lastScan = lastActivity = time(NULL);
  currentChannel = 0;
  memset(lastChannel, 0, sizeof(lastChannel));
  numTransponders = 0;
  transponders = NULL;
}

cEITScanner::~cEITScanner()
{
  free(transponders);
}

bool cEITScanner::TransponderScanned(cChannel *Channel)
{
  for (int i = 0; i < numTransponders; i++) {
      if (transponders[i] == Channel->Frequency())
         return true;
      }
  transponders = (int *)realloc(transponders, ++numTransponders * sizeof(int));
  transponders[numTransponders - 1] = Channel->Frequency();
  return false;
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
        for (int i = 0; i < cDevice::NumDevices(); i++) {
            cDevice *Device = cDevice::GetDevice(i);
            if (Device && Device->CardIndex() < MAXDVBDEVICES) {
               if (Device != cDevice::PrimaryDevice() || (cDevice::NumDevices() == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                  if (!(Device->Receiving(true) || Device->Replaying())) {
                     for (;;) {
                         cChannel *Channel = Channels.GetByNumber(lastChannel[Device->DeviceNumber()] + 1, 1);
                         if (Channel) {
                            lastChannel[Device->DeviceNumber()] = Channel->Number();
                            if (Channel->Sid() && Device->ProvidesChannel(Channel) && !TransponderScanned(Channel)) {
                               if (Device == cDevice::PrimaryDevice() && !currentChannel) {
                                  currentChannel = Device->CurrentChannel();
                                  }
                               Device->SwitchChannel(Channel, false);
                               break;
                               }
                            }
                         else {
                            if (lastChannel[Device->DeviceNumber()])
                               numTransponders = 0;
                            lastChannel[Device->DeviceNumber()] = 0;
                            break;
                            }
                         }
                     }
                  }
               }
            }
        lastScan = time(NULL);
        }
     }
}
