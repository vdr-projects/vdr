/*
 * sdt.h: SDT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sdt.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __SDT_H
#define __SDT_H

#include "filter.h"
#include "pat.h"

class cSdtFilter : public cFilter {
private:
  enum eTransponderState { tsUnknown, tsWrong, tsAccepted, tsVerified };
  cMutex mutex;
  cSectionSyncer sectionSyncer;
  int source;
  int lastSource;
  int lastTransponder;
  int lastNid;
  int lastTid;
  cPatFilter *patFilter;
  enum eTransponderState transponderState;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length) override;
public:
  cSdtFilter(cPatFilter *PatFilter);
  virtual void SetStatus(bool On) override;
  void Trigger(int Source);
  bool TransponderVerified(void) const { return transponderState == tsVerified; } // returns true if the expected NIT/TID have been received in the SDT
  bool TransponderWrong(void) const { return transponderState == tsWrong; } // returns true if an expected change of NIT/TID has not happened
  };

#endif //__SDT_H
