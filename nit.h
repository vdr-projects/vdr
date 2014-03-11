/*
 * nit.h: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.h 2.0.1.1 2014/03/11 09:30:05 kls Exp $
 */

#ifndef __NIT_H
#define __NIT_H

#include "filter.h"
#include "sdt.h"

#define MAXNITS 16
#define MAXNETWORKNAME Utf8BufSize(256)

class cNitFilter : public cFilter {
private:

  class cNit {
  public:
    u_short networkId;
    char name[MAXNETWORKNAME];
    bool hasTransponder;
    };

  cSectionSyncer sectionSyncer;
  cSdtFilter *sdtFilter;
  cNit nits[MAXNITS];
  u_short networkId;
  int numNits;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cNitFilter(cSdtFilter *SdtFilter);
  virtual void SetStatus(bool On);
  };

#endif //__NIT_H
