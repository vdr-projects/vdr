/*
 * pat.h: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.h 5.3 2021/06/21 20:13:55 kls Exp $
 */

#ifndef __PAT_H
#define __PAT_H

#include <stdint.h>
#include "filter.h"
#include "thread.h"

class cPmtPidEntry;
class cPmtSidEntry;
class cPmtSidRequest;

class cPatFilter : public cFilter {
private:
  cMutex mutex;
  cTimeMs timer;
  int patVersion;
  cPmtPidEntry *activePmt;
  cList<cPmtPidEntry> pmtPidList;
  cList<cPmtSidEntry> pmtSidList;
  cList<cPmtSidRequest> pmtSidRequestList;
  int source;
  int transponder;
  cSectionSyncer sectionSyncer;
  bool TransponderChanged(void);
  bool PmtPidComplete(int PmtPid);
  void PmtPidReset(int PmtPid);
  bool PmtVersionChanged(int PmtPid, int Sid, int Version, bool SetNewVersion = false);
  int  NumSidRequests(int Sid);
  void SwitchToNextPmtPid(void);
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cPatFilter(void);
  virtual void SetStatus(bool On);
  void Trigger(int = 0); // triggers reading the PMT PIDs that are currently not requested (dummy parameter for backwards compatibility, value is ignored)
  void Request(int Sid); // requests permanent reading of the PMT PID for this SID
  void Release(int Sid); // releases permanent reading of the PMT PID for this SID
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
