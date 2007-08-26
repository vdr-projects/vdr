/*
 * dvbosd.h: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.19 2007/08/25 13:49:34 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#include "osd.h"

class cDvbOsdProvider : public cOsdProvider {
private:
  int osdDev;
public:
  cDvbOsdProvider(int OsdDev);
  virtual cOsd *CreateOsd(int Left, int Top, uint Level);
  };

#endif //__DVBOSD_H
