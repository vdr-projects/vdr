/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 1.15 2001/09/01 12:58:02 kls Exp $
 */

#ifndef __RECORDING_H
#define __RECORDING_H

#include <time.h>
#include "config.h"
#include "tools.h"

void RemoveDeletedRecordings(void);
void AssertFreeDiskSpace(int Priority);

class cResumeFile {
private:
  char *fileName;
public:
  cResumeFile(const char *FileName);
  ~cResumeFile();
  int Read(void);
  bool Save(int Index);
  void Delete(void);
  };

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
  const char *Title(char Delimiter = ' ', bool NewIndicator = false);
  const char *Summary(void) { return summary; }
  const char *PrefixFileName(char Prefix);
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

class cMark : public cListObject {
private:
  static char *buffer;
public:
  int position;
  char *comment;
  cMark(int Position = 0, const char *Comment = NULL);
  ~cMark();
  const char *ToText(void);
  bool Parse(const char *s);
  bool Save(FILE *f);
  };

class cMarks : public cConfig<cMark> {
public:
  bool Load(const char *RecordingFileName);
  void Sort(void);
  cMark *Add(int Position);
  cMark *Get(int Position);
  cMark *GetPrev(int Position);
  cMark *GetNext(int Position);
  };

#endif //__RECORDING_H
