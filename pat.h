/*
 * pat.h: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.h 3.2 2014/01/04 11:16:48 kls Exp $
 */

#ifndef __PAT_H
#define __PAT_H

#include <stdint.h>
#include "filter.h"

#define MAXPMTENTRIES 64

class cPatFilter : public cFilter {
private:
  time_t lastPmtScan;
  int pmtIndex;
  int pmtPid;
  int pmtSid;
  uint64_t pmtVersion[MAXPMTENTRIES];
  int numPmtEntries;
  bool PmtVersionChanged(int PmtPid, int Sid, int Version);
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cPatFilter(void);
  virtual void SetStatus(bool On);
  void Trigger(void);
  };

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, uchar *Data, int EsPid);
         ///< Gets all CA descriptors for a given channel.
         ///< Copies all available CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Data (at most BufSize bytes). Only those CA descriptors
         ///< are copied that match one of the given CA system IDs (or all of them, if CaSystemIds
         ///< is 0xFFFF).
         ///< Returns the number of bytes copied into Data (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA descriptors.

int GetCaPids(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, int *Pids);
         ///< Gets all CA pids for a given channel.
         ///< Copies all available CA pids from the CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Pids (at most BufSize - 1 entries, the list will be zero-terminated).
         ///< Only the CA pids of those CA descriptors are copied that match one of the given CA system IDs
         ///< (or all of them, if CaSystemIds is 0xFFFF).
         ///< Returns the number of pids copied into Pids (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA pids.

#endif //__PAT_H
