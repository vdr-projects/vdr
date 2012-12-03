/*
 * dvbsdffosd.h: Implementation of the DVB SD Full Featured On Screen Display
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbsdffosd.h 2.2 2012/12/03 13:43:55 kls Exp $
 */

#ifndef __DVBSDFFODF_H
#define __DVBSDFFODF_H

#include <vdr/osd.h>

class cDvbOsdProvider : public cOsdProvider {
private:
  int osdDev;
public:
  cDvbOsdProvider(int OsdDev);
  virtual cOsd *CreateOsd(int Left, int Top, uint Level);
  };

#endif //__DVBSDFFODF_H
