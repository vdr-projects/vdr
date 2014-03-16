/*
 * sdt.h: SDT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sdt.h 3.1 2014/03/10 14:40:54 kls Exp $
 */

#ifndef __SDT_H
#define __SDT_H

#include "filter.h"
#include "pat.h"

class cSdtFilter : public cFilter {
private:
  cMutex mutex;
  cSectionSyncer sectionSyncer;
  int source;
  cPatFilter *patFilter;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cSdtFilter(cPatFilter *PatFilter);
  virtual void SetStatus(bool On);
  void Trigger(int Source);
  };

#endif //__SDT_H
