/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 1.9 2000/07/28 13:53:54 kls Exp $
 */

#ifndef __RECORDING_H
#define __RECORDING_H

#include <time.h>
#include "config.h"
#include "tools.h"

void AssertFreeDiskSpace(void);

class cRecording : public cListObject {
  friend class cRecordings;
private:
  char *titleBuffer;
  char *fileName;
  char *name;
  char *summary;
public:
  time_t start;
  int priority;
  int lifetime;
  cRecording(cTimer *Timer);
  cRecording(const char *FileName);
  ~cRecording();
  const char *FileName(void);
  const char *Title(char Delimiter = ' ');
  const char *Summary(void) { return summary; }
  bool WriteSummary(void);
  bool Delete(void);
       // Changes the file name so that it will no longer be visible in the "Recordings" menu
       // Returns false in case of error
  bool Remove(void);
       // Actually removes the file from the disk
       // Returns false in case of error
  };

class cRecordings : public cList<cRecording> {
public:
  bool Load(bool Deleted = false);
  };

#endif //__RECORDING_H
