/*
 * pat.h: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.h 2.3.1.1 2014/02/18 14:12:24 kls Exp $
 */

#ifndef __PAT_H
#define __PAT_H

#include <stdint.h>
#include "filter.h"
#include "thread.h"

#define MAXPMTENTRIES 64

class cPatFilter : public cFilter {
private:
  cMutex mutex;
  cTimeMs timer;
  int patVersion;
  int pmtIndex;
  int pmtId[MAXPMTENTRIES];
  int pmtVersion[MAXPMTENTRIES];
  int numPmtEntries;
  int sid;
  int GetPmtPid(int Index) { return pmtId[Index] & 0x0000FFFF; }
  int MakePmtId(int PmtPid, int Sid) { return PmtPid | (Sid << 16); }
  bool PmtVersionChanged(int PmtPid, int Sid, int Version, bool SetNewVersion = false);
  void SwitchToNextPmtPid(void);
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cPatFilter(void);
  virtual void SetStatus(bool On);
  void Trigger(int Sid = -1);
  };

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, uchar *Data, int EsPid);
         ///< Gets all CA descriptors for a given channel.
         ///< Copies all available CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Data (at most BufSize bytes). Only those CA descriptors
         ///< are copied that match one of the given CA system IDs.
         ///< Returns the number of bytes copied into Data (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA descriptors.
         ///< The return value tells whether these CA descriptors are to be used
         ///< for the individual streams.

#endif //__PAT_H
