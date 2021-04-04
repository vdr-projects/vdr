/*
 * eit.h: EIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: eit.h 5.2 2021/04/04 11:06:30 kls Exp $
 */

#ifndef __EIT_H
#define __EIT_H

#include "filter.h"
#include "tools.h"

#define NUM_EIT_TABLES  17

// Event information (or EPG) is broadcast in tables 0x4E and 0x4F for "present/following" events on
// "this transponder" (0x4E) and "other transponders" (0x4F), as well as 0x50-0x5F ("all events on this
// transponder") and 0x60-0x6F ("all events on other transponders"). Since it's either "this" or "other",
// we only use one section syncer for 0x4E/0x4F and 16 syncers for either 0x5X or 0x6X.

class cEitTables : public cListObject {
private:
  cSectionSyncerRandom sectionSyncer[NUM_EIT_TABLES]; // for tables 0x4E/0x4F and 0x50-0x5F/0x60-0x6F
  time_t tableStart; // only used for table 0x4E
  time_t tableEnd;
  bool complete;
  int Index(uchar TableId) { return (TableId < 0x50) ? 0 : (TableId & 0x0F) + 1; }
public:
  cEitTables(void);
  void SetTableStart(time_t t) { tableStart = t; }
  void SetTableEnd(time_t t) { tableEnd = t; }
  time_t TableStart(void) { return tableStart; }
  time_t TableEnd(void) { return tableEnd; }
  bool Check(uchar TableId, uchar Version, int SectionNumber);
  bool Processed(uchar TableId, uchar LastTableId, int SectionNumber, int LastSectionNumber, int SegmentLastSectionNumber = -1);
       ///< Returns true if all sections of the table with the given TableId have been processed.
  bool Complete(void) { return complete; }
       ///< Returns true if all sections of all tables have been processed.
  };

class cEitTablesHash : public cHash<cEitTables> {
public:
  cEitTablesHash(void) : cHash(HASHSIZE, true) {};
  };

class cEitFilter : public cFilter {
private:
  cMutex mutex;
  cEitTablesHash eitTablesHash;
  static time_t disableUntil;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cEitFilter(void);
  virtual void SetStatus(bool On);
  static void SetDisableUntil(time_t Time);
  };

#endif //__EIT_H
