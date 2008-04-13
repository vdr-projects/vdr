/*
 * eitscan.h: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.h 2.0 2005/09/04 10:51:35 kls Exp $
 */

#ifndef __EITSCAN_H
#define __EITSCAN_H

#include <time.h>
#include "channels.h"
#include "config.h"
#include "device.h"

class cScanList;
class cTransponderList;

class cEITScanner {
private:
  enum { ActivityTimeout = 60,
         ScanTimeout = 20
       };
  time_t lastScan, lastActivity;
  cDevice *currentDevice;
  int currentChannel;
  cScanList *scanList;
  cTransponderList *transponderList;
public:
  cEITScanner(void);
  ~cEITScanner();
  bool Active(void) { return currentChannel || lastActivity == 0; }
  bool UsesDevice(const cDevice *Device) { return currentDevice == Device; }
  void AddTransponder(cChannel *Channel);
  void ForceScan(void);
  void Activity(void);
  void Process(void);
  };

extern cEITScanner EITScanner;

#endif //__EITSCAN_H
