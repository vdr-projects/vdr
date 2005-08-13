/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 1.39 2005/08/13 14:09:50 kls Exp $
 */

#ifndef __RECORDING_H
#define __RECORDING_H

#include <time.h>
#include "channels.h"
#include "config.h"
#include "epg.h"
#include "thread.h"
#include "timers.h"
#include "tools.h"

void RemoveDeletedRecordings(void);
void AssertFreeDiskSpace(int Priority = 0);
     ///< The special Priority value -1 means that we shall get rid of any
     ///< deleted recordings faster than normal (because we're cutting).

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

class cRecordingInfo {
  friend class cRecording;
private:
  tChannelID channelID;
  const cEvent *event;
  cEvent *ownEvent;
  cRecordingInfo(tChannelID ChannelID = tChannelID::InvalidID, const cEvent *Event = NULL);
  void SetData(const char *Title, const char *ShortText, const char *Description);
public:
  ~cRecordingInfo();
  const char *Title(void) const { return event->Title(); }
  const char *ShortText(void) const { return event->ShortText(); }
  const char *Description(void) const { return event->Description(); }
  const cComponents *Components(void) const { return event->Components(); }
  bool Read(FILE *f);
  bool Write(FILE *f, const char *Prefix = "") const;
  };

class cRecording : public cListObject {
private:
  mutable int resume;
  mutable char *titleBuffer;
  mutable char *sortBuffer;
  mutable char *fileName;
  mutable char *name;
  cRecordingInfo *info;
  static char *StripEpisodeName(char *s);
  char *SortName(void) const;
  int GetResume(void) const;
public:
  time_t start;
  int priority;
  int lifetime;
  cRecording(cTimer *Timer, const cEvent *Event);
  cRecording(const char *FileName);
  ~cRecording();
  virtual int Compare(const cListObject &ListObject) const;
  const char *Name(void) const { return name; }
  const char *FileName(void) const;
  const char *Title(char Delimiter = ' ', bool NewIndicator = false, int Level = -1) const;
  const cRecordingInfo *Info(void) const { return info; }
  const char *PrefixFileName(char Prefix);
  int HierarchyLevels(void) const;
  bool IsNew(void) const { return GetResume() <= 0; }
  bool IsEdited(void) const;
  bool WriteInfo(void);
  bool Delete(void);
       // Changes the file name so that it will no longer be visible in the "Recordings" menu
       // Returns false in case of error
  bool Remove(void);
       // Actually removes the file from the disk
       // Returns false in case of error
  };

class cRecordings : public cList<cRecording> {
private:
  bool deleted;
  time_t lastUpdate;
  void ScanVideoDir(const char *DirName);
public:
  cRecordings(bool Deleted = false);
  bool Load(void);
  void TriggerUpdate(void) { lastUpdate = 0; }
  bool NeedsUpdate(void);
  cRecording *GetByName(const char *FileName);
  void AddByName(const char *FileName);
  void DelByName(const char *FileName);
  };

extern cRecordings Recordings;

class cMark : public cListObject {
public:
  int position;
  char *comment;
  cMark(int Position = 0, const char *Comment = NULL);
  ~cMark();
  cString ToText(void);
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

//XXX+
#define FRAMESPERSEC 25

// The maximum size of a single frame (up to HDTV 1920x1080):
#define MAXFRAMESIZE  KILOBYTE(512)

// The maximum file size is limited by the range that can be covered
// with 'int'. 4GB might be possible (if the range is considered
// 'unsigned'), 2GB should be possible (even if the range is considered
// 'signed'), so let's use 2000MB for absolute safety (the actual file size
// may be slightly higher because we stop recording only before the next
// 'I' frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZE 2000 // MB
#define MINVIDEOFILESIZE  100 // MB

class cIndexFile {
private:
  struct tIndex { int offset; uchar type; uchar number; short reserved; };
  int f;
  char *fileName;
  int size, last;
  tIndex *index;
  cResumeFile resumeFile;
  cMutex mutex;
  bool CatchUp(int Index = -1);
public:
  cIndexFile(const char *FileName, bool Record);
  ~cIndexFile();
  bool Ok(void) { return index != NULL; }
  bool Write(uchar PictureType, uchar FileNumber, int FileOffset);
  bool Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType = NULL, int *Length = NULL);
  int GetNextIFrame(int Index, bool Forward, uchar *FileNumber = NULL, int *FileOffset = NULL, int *Length = NULL, bool StayOffEnd = false);
  int Get(uchar FileNumber, int FileOffset);
  int Last(void) { CatchUp(); return last; }
  int GetResume(void) { return resumeFile.Read(); }
  bool StoreResume(int Index) { return resumeFile.Save(Index); }
  };

class cFileName {
private:
  int file;
  int fileNumber;
  char *fileName, *pFileNumber;
  bool record;
  bool blocking;
public:
  cFileName(const char *FileName, bool Record, bool Blocking = false);
  ~cFileName();
  const char *Name(void) { return fileName; }
  int Number(void) { return fileNumber; }
  int Open(void);
  void Close(void);
  int SetOffset(int Number, int Offset = 0);
  int NextFile(void);
  };

cString IndexToHMSF(int Index, bool WithFrame = false);
      // Converts the given index to a string, optionally containing the frame number.
int HMSFToIndex(const char *HMSF);
      // Converts the given string (format: "hh:mm:ss.ff") to an index.
int SecondsToFrames(int Seconds); //XXX+ ->player???
      // Returns the number of frames corresponding to the given number of seconds.

int ReadFrame(int f, uchar *b, int Length, int Max);

#endif //__RECORDING_H
