/*
 * eit.h: EIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eit.h 4.2 2017/05/08 21:10:29 kls Exp $
 */

#ifndef __EIT_H
#define __EIT_H

#include "filter.h"
#include "tools.h"

class cSectionSyncerEntry : public cListObject, public cSectionSyncer {};

class cSectionSyncerHash : public cHash<cSectionSyncerEntry> {
public:
  cSectionSyncerHash(void) : cHash(HASHSIZE, true) {};
  };

class cEitFilter : public cFilter {
private:
  cMutex mutex;
  cSectionSyncerHash sectionSyncerHash;
  static time_t disableUntil;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cEitFilter(void);
  virtual void SetStatus(bool On);
  static void SetDisableUntil(time_t Time);
  };

#endif //__EIT_H
