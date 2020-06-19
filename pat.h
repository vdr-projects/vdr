/*
 * pat.h: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.h 4.2 2020/06/19 12:19:15 kls Exp $
 */

#ifndef __PAT_H
#define __PAT_H

#include <stdint.h>
#include "filter.h"
#include "thread.h"

class cPmtPidEntry;
class cPmtSidEntry;

class cPatFilter : public cFilter {
private:
  cMutex mutex;
  cTimeMs timer;
  int patVersion;
  int sid;
  cPmtPidEntry *activePmt;
  cList<cPmtPidEntry> pmtPidList;
  cList<cPmtSidEntry> pmtSidList;
  cSectionSyncer sectionSyncer;
  bool PmtPidComplete(int PmtPid);
  void PmtPidReset(int PmtPid);
  bool PmtVersionChanged(int PmtPid, int Sid, int Version, bool SetNewVersion = false);
  void SwitchToNextPmtPid(void);
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cPatFilter(void);
  virtual void SetStatus(bool On);
  void Trigger(int Sid = -1);
  };

void GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid);
         ///< Gets all CA descriptors for a given channel.
         ///< Copies all available CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer. Only those CA descriptors
         ///< are copied that match one of the given CA system IDs (or all of them, if CaSystemIds
         ///< is 0xFFFF).

int GetCaPids(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, int *Pids);
         ///< Gets all CA pids for a given channel.
         ///< Copies all available CA pids from the CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Pids (at most BufSize - 1 entries, the list will be zero-terminated).
         ///< Only the CA pids of those CA descriptors are copied that match one of the given CA system IDs
         ///< (or all of them, if CaSystemIds is 0xFFFF).
         ///< Returns the number of pids copied into Pids (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA pids.

int GetPmtPid(int Source, int Transponder, int ServiceId);
         ///< Gets the Pid of the PMT in which the CA descriptors for this channel are defined.

#endif //__PAT_H
