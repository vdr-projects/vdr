/*
 * dvbosd.h: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.18 2004/06/12 13:09:52 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#include "osd.h"

class cDvbOsdProvider : public cOsdProvider {
private:
  int osdDev;
public:
  cDvbOsdProvider(int OsdDev);
  virtual cOsd *CreateOsd(int Left, int Top);
  };

#endif //__DVBOSD_H
