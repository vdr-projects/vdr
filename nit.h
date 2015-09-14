/*
 * nit.h: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.h 4.1 2015/03/16 12:41:38 kls Exp $
 */

#ifndef __NIT_H
#define __NIT_H

#include "filter.h"
#include "sdt.h"

class cNitFilter : public cFilter {
private:
  cSectionSyncer sectionSyncer;
  cSdtFilter *sdtFilter;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cNitFilter(cSdtFilter *SdtFilter);
  virtual void SetStatus(bool On);
  };

#endif //__NIT_H
