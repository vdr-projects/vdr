/*
 * eitscan.c: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.c 1.1 2002/05/20 10:58:10 kls Exp $
 */

#include "eitscan.h"

cEITScanner::cEITScanner(void)
{
  lastScan = lastActivity = time(NULL);
  currentChannel = 0;
  lastChannel = 0;
  numTransponders = 0;
  transponders = NULL;
}

cEITScanner::~cEITScanner()
{
  delete transponders;
}

bool cEITScanner::TransponderScanned(cChannel *Channel)
{
  for (int i = 0; i < numTransponders; i++) {
      if (transponders[i] == Channel->frequency)
         return true;
      }
  transponders = (int *)realloc(transponders, ++numTransponders * sizeof(int));
  transponders[numTransponders - 1] = Channel->frequency;
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
        for (int i = 0; i < MAXDVBAPI; i++) {
            cDvbApi *DvbApi = cDvbApi::GetDvbApi(i + 1, MAXPRIORITY + 1);
            if (DvbApi) {
               if (DvbApi != cDvbApi::PrimaryDvbApi || (cDvbApi::NumDvbApis == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                  if (!(DvbApi->Recording() || DvbApi->Replaying() || DvbApi->Transferring())) {
                     int oldCh = lastChannel;
                     int ch = oldCh + 1;
                     while (ch != oldCh) {
                           if (ch > Channels.MaxNumber()) {
                              ch = 1;
                              numTransponders = 0;
                              }
                           cChannel *Channel = Channels.GetByNumber(ch);
                           if (Channel) {
                              if (Channel->ca <= MAXDVBAPI && !DvbApi->ProvidesCa(Channel->ca))
                                 break; // the channel says it explicitly needs a different card
                              if (Channel->pnr && !TransponderScanned(Channel)) {
                                 if (DvbApi == cDvbApi::PrimaryDvbApi && !currentChannel)
                                    currentChannel = DvbApi->Channel();
                                 Channel->Switch(DvbApi, false);
                                 lastChannel = ch;
                                 break;
                                 }
                              }
                           ch++;
                           }
                     }
                  }
               }
            }
        lastScan = time(NULL);
        }
     }
}
