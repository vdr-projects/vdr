/*
 * eitscan.h: EIT scanner
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eitscan.h 1.3 2003/05/24 13:21:36 kls Exp $
 */

#ifndef __EITSCAN_H
#define __EITSCAN_H

#include <time.h>
#include "config.h"

class cEITScanner {
private:
  enum { ActivityTimeout = 60,
         ScanTimeout = 20
       };
  time_t lastScan, lastActivity;
  int currentChannel;
  int lastChannel[MAXDEVICES];
  int numTransponders, *transponders;
  bool TransponderScanned(cChannel *Channel);
public:
  cEITScanner(void);
  ~cEITScanner();
  bool Active(void) { return currentChannel; }
  void Activity(void);
  void Process(void);
  };

extern cEITScanner EITScanner;

#endif //__EITSCAN_H
