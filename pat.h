/*
 * pat.h: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.h 1.2 2003/12/24 10:08:22 kls Exp $
 */

#ifndef __PAT_H
#define __PAT_H

#include "filter.h"
#include "thread.h"

class cPatFilter : public cFilter {
private:
  time_t lastPmtScan;
  int pmtIndex;
  int pmtPid;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  cPatFilter(void);
  virtual void SetStatus(bool On);
  };

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data, bool &StreamFlag);
         ///< Gets all CA descriptors for a given channel.
         ///< Copies all available CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Data (at most BufSize bytes). Only those CA descriptors
         ///< are copied that match one of the given CA system IDs.
         ///< \return Returns the number of bytes copied into Data (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA descriptors.
         ///< The return value in StreamFlag tells whether these CA descriptors are to be used
         ///< for the individual streams.

#endif //__PAT_H
