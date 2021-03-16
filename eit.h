/*
 * eit.h: EIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eit.h 5.1 2021/03/16 15:10:54 kls Exp $
 */

#ifndef __EIT_H
#define __EIT_H

#include "filter.h"
#include "tools.h"

#define NUM_EIT_TABLES  17

// Event information (or EPG) is broadcast in tables 0x4E and 0x4F for "present/following" events on
// "this transponder" (0x4E) and "other transponders" (0x4F), as well as 0x50-0x5F ("all events on this
// transponder) and 0x60-0x6F ("all events on other transponders). Since it's either "this" or "other",
// we only use one section syncer for 0x4E/0x4F and 16 syncers for either 0x5X or 0x6X.

class cEitTables : public cListObject {
private:
  cSectionSyncerRandom sectionSyncer[NUM_EIT_TABLES]; // for tables 0x4E/0x4F and 0x50-0x5F/0x60-0x6F
  bool complete;
  int Index(uchar TableId) { return (TableId < 0x50) ? 0 : (TableId & 0x0F) + 1; }
public:
  cEitTables(void) { complete = false; }
  bool Check(uchar TableId, uchar Version, int SectionNumber);
  bool Processed(uchar TableId, uchar LastTableId, int SectionNumber, int LastSectionNumber, int SegmentLastSectionNumber = -1);
  bool Complete(void) { return complete; }
  };

class cSectionSyncerHash : public cHash<cEitTables> {
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
