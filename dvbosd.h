/*
 * dvbosd.h: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.17 2004/04/30 13:44:16 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#include <linux/dvb/osd.h>
#include "dvbdevice.h"
#include "osd.h"

class cDvbOsd : public cOsd {
private:
  int osdDev;
  bool shown;
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
public:
  cDvbOsd(int Left, int Top, int OsdDev);
  virtual ~cDvbOsd();
  virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
  virtual void Flush(void);
  };

class cDvbOsdProvider : public cOsdProvider {
private:
  int osdDev;
public:
  cDvbOsdProvider(int OsdDev);
  virtual cOsd *CreateOsd(int Left, int Top);
  };

#endif //__DVBOSD_H
