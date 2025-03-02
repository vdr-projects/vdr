/*
 * nit.h: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.h 5.1 2025/03/02 11:03:35 kls Exp $
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
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length) override;
public:
  cNitFilter(cSdtFilter *SdtFilter);
  virtual void SetStatus(bool On) override;
  };

#endif //__NIT_H
