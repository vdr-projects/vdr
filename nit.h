/*
 * nit.h: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.h 1.1 2004/01/11 14:31:05 kls Exp $
 */

#ifndef __NIT_H
#define __NIT_H

#include "filter.h"

class cNitFilter : public cFilter {
private:
  cSectionSyncer sectionSyncer;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cNitFilter(void);
  virtual void SetStatus(bool On);
  };

#endif //__NIT_H
