/*
 * eitscan.h: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.h 1.6 2004/01/11 14:11:25 kls Exp $
 */

#ifndef __EITSCAN_H
#define __EITSCAN_H

#include <time.h>
#include "config.h"

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
  int numScan;
  cScanList *scanList;
  cTransponderList *transponderList;
public:
  cEITScanner(void);
  ~cEITScanner();
  bool Active(void) { return currentChannel; }
  bool UsesDevice(const cDevice *Device) { return currentDevice == Device; }
  void AddTransponder(cChannel *Channel);
  void Activity(void);
  void Process(void);
  };

extern cEITScanner EITScanner;

#endif //__EITSCAN_H
