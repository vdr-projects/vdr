/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 1.22 2002/02/03 11:59:49 kls Exp $
 */

#ifndef __RECORDING_H
#define __RECORDING_H

#include <time.h>
#include "config.h"
#include "tools.h"

void RemoveDeletedRecordings(void);
void AssertFreeDiskSpace(int Priority = 0);

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
private:
  int resume;
  char *titleBuffer;
  char *sortBuffer;
  char *fileName;
  char *name;
  char *summary;
  char *StripEpisodeName(char *s);
  char *SortName(void);
  int GetResume(void);
public:
  time_t start;
  int priority;
  int lifetime;
  cRecording(cTimer *Timer, const char *Title, const char *Subtitle, const char *Summary);
  cRecording(const char *FileName);
  ~cRecording();
  virtual bool operator< (const cListObject &ListObject);
  const char *Name(void) { return name; }
  const char *FileName(void);
  const char *Title(char Delimiter = ' ', bool NewIndicator = false, int Level = -1);
  const char *Summary(void) { return summary; }
  const char *PrefixFileName(char Prefix);
  int HierarchyLevels(void);
  bool IsNew(void) { return GetResume() <= 0; }
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
  cRecording *GetByName(const char *FileName);
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

#define RUC_BEFORERECORDING "before"
#define RUC_AFTERRECORDING  "after"
#define RUC_EDITEDRECORDING "edited"

class cRecordingUserCommand {
private:
  static const char *command;
public:
  static void SetCommand(const char *Command) { command = Command; }
  static void InvokeCommand(const char *State, const char *RecordingFileName);
  };

#endif //__RECORDING_H
