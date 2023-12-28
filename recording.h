/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 5.6 2023/12/27 09:21:29 kls Exp $
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
#include "remux.h"

#define FOLDERDELIMCHAR '~'

extern int DirectoryPathMax;
extern int DirectoryNameMax;
extern bool DirectoryEncoding;
extern int InstanceId;

enum eRecordingUsage {
  ruNone     = 0x0000, // the recording is currently unused
  ruTimer    = 0x0001, // the recording is currently written to by a timer
  ruReplay   = 0x0002, // the recording is being replayed
  // mutually exclusive:
  ruCut      = 0x0004, // the recording is being cut
  ruMove     = 0x0008, // the recording is being moved
  ruCopy     = 0x0010, // the recording is being copied
  // mutually exclusive:
  ruSrc      = 0x0020, // the recording is the source of a cut, move or copy process
  ruDst      = 0x0040, // the recording is the destination of a cut, move or copy process
  //
  ruPending  = 0x0080, // the recording is pending a cut, move or copy process
  ruCanceled = 0x8000, // the operation has been canceled, waiting for cleanup
  };

void RemoveDeletedRecordings(void);
void AssertFreeDiskSpace(int Priority = 0, bool Force = false);
     ///< The special Priority value -1 means that we shall get rid of any
     ///< deleted recordings faster than normal (because we're cutting).
     ///< If Force is true, the check will be done even if the timeout
     ///< hasn't expired yet.

class cResumeFile {
private:
  char *fileName;
  bool isPesRecording;
public:
  cResumeFile(const char *FileName, bool IsPesRecording);
  ~cResumeFile();
  int Read(void);
  bool Save(int Index);
  void Delete(void);
  };

class cRecordingInfo {
  friend class cRecording;
private:
  tChannelID channelID;
  char *channelName;
  const cEvent *event;
  cEvent *ownEvent;
  char *aux;
  double framesPerSecond;
  uint16_t frameWidth;
  uint16_t frameHeight;
  eScanType scanType;
  eAspectRatio aspectRatio;
  int priority;
  int lifetime;
  char *fileName;
  int errors;
  cRecordingInfo(const cChannel *Channel = NULL, const cEvent *Event = NULL);
  bool Read(FILE *f);
public:
  cRecordingInfo(const char *FileName);
  ~cRecordingInfo();
  tChannelID ChannelID(void) const { return channelID; }
  const char *ChannelName(void) const { return channelName; }
  const cEvent *GetEvent(void) const { return event; }
  const char *Title(void) const { return event->Title(); }
  const char *ShortText(void) const { return event->ShortText(); }
  const char *Description(void) const { return event->Description(); }
  const cComponents *Components(void) const { return event->Components(); }
  const char *Aux(void) const { return aux; }
  double FramesPerSecond(void) const { return framesPerSecond; }
  uint16_t FrameWidth(void) const { return frameWidth; }
  uint16_t FrameHeight(void) const { return frameHeight; }
  eScanType ScanType(void) const { return scanType; }
  char ScanTypeChar(void) const { return ScanTypeChars[scanType]; }
  eAspectRatio AspectRatio(void) const { return aspectRatio; }
  const char *AspectRatioText(void) const { return AspectRatioTexts[aspectRatio]; }
  void SetFramesPerSecond(double FramesPerSecond);
  void SetFrameParams(uint16_t FrameWidth, uint16_t FrameHeight, eScanType ScanType, eAspectRatio AspectRatio);
  void SetFileName(const char *FileName);
  int Errors(void) const { return errors; } // returns -1 if undefined
  void SetErrors(int Errors);
  bool Write(FILE *f, const char *Prefix = "") const;
  bool Read(void);
  bool Write(void) const;
  void SetData(const char *Title, const char *ShortText, const char *Description);
  void SetAux(const char *Aux);
  };

class cRecording : public cListObject {
  friend class cRecordings;
private:
  int id;
  mutable int resume;
  mutable char *titleBuffer;
  mutable char *sortBufferName;
  mutable char *sortBufferTime;
  mutable char *fileName;
  mutable char *name;
  mutable int fileSizeMB;
  mutable int numFrames;
  int channel;
  int instanceId;
  bool isPesRecording;
  mutable int isOnVideoDirectoryFileSystem; // -1 = unknown, 0 = no, 1 = yes
  double framesPerSecond;
  cRecordingInfo *info;
  cRecording(const cRecording&); // can't copy cRecording
  cRecording &operator=(const cRecording &); // can't assign cRecording
  static char *StripEpisodeName(char *s, bool Strip);
  char *SortName(void) const;
  void ClearSortName(void);
  void SetId(int Id); // should only be set by cRecordings
  time_t start;
  int priority;
  int lifetime;
  time_t deleted;
public:
  cRecording(cTimer *Timer, const cEvent *Event);
  cRecording(const char *FileName);
  virtual ~cRecording();
  int Id(void) const { return id; }
  time_t Start(void) const { return start; }
  int Priority(void) const { return priority; }
  int Lifetime(void) const { return lifetime; }
  time_t Deleted(void) const { return deleted; }
  void SetDeleted(void) { deleted = time(NULL); }
  virtual int Compare(const cListObject &ListObject) const;
  bool IsInPath(const char *Path) const;
       ///< Returns true if this recording is stored anywhere under the given Path.
       ///< If Path is NULL or an empty string, the entire video directory is checked.
  cString Folder(void) const;
       ///< Returns the name of the folder this recording is stored in (without the
       ///< video directory). For use in menus etc.
  cString BaseName(void) const;
       ///< Returns the base name of this recording (without the
       ///< video directory and folder). For use in menus etc.
  const char *Name(void) const { return name; }
       ///< Returns the full name of the recording (without the video directory).
       ///< For use in menus etc.
  const char *FileName(void) const;
       ///< Returns the full path name to the recording directory, including the
       ///< video directory and the actual '*.rec'. For disk file access use.
  const char *Title(char Delimiter = ' ', bool NewIndicator = false, int Level = -1) const;
  cRecordingInfo *Info(void) const { return info; }
  const char *PrefixFileName(char Prefix);
  int HierarchyLevels(void) const;
  void ResetResume(void) const;
  double FramesPerSecond(void) const { return framesPerSecond; }
  int NumFrames(void) const;
       ///< Returns the number of frames in this recording.
       ///< If the number of frames is unknown, -1 will be returned.
  int LengthInSeconds(void) const;
       ///< Returns the length (in seconds) of this recording, or -1 in case of error.
  int FileSizeMB(void) const;
       ///< Returns the total file size of this recording (in MB), or -1 if the file
       ///< size is unknown.
  int GetResume(void) const;
       ///< Returns the index of the frame where replay of this recording shall
       ///< be resumed, or -1 in case of an error.
  bool IsNew(void) const { return GetResume() <= 0; }
  bool IsEdited(void) const;
  bool IsPesRecording(void) const { return isPesRecording; }
  bool IsOnVideoDirectoryFileSystem(void) const;
  bool HasMarks(void) const;
       ///< Returns true if this recording has any editing marks.
  bool DeleteMarks(void);
       ///< Deletes the editing marks from this recording (if any).
       ///< Returns true if the operation was successful. If there is no marks file
       ///< for this recording, it also returns true.
  void ReadInfo(void);
  bool WriteInfo(const char *OtherFileName = NULL);
       ///< Writes in info file of this recording. If OtherFileName is given, the info
       ///< file will be written under that recording file name instead of this
       ///< recording's file name.
  void SetStartTime(time_t Start);
       ///< Sets the start time of this recording to the given value.
       ///< If a filename has already been set for this recording, it will be
       ///< deleted and a new one will be generated (using the new start time)
       ///< at the next call to FileName().
       ///< Use this function with care - it does not check whether a recording with
       ///< this new name already exists, and if there is one, results may be
       ///< unexpected!
  bool ChangePriorityLifetime(int NewPriority, int NewLifetime);
       ///< Changes the priority and lifetime of this recording to the given values.
       ///< If the new values are the same as the old ones, nothing happens.
       ///< Returns false in case of error.
  bool ChangeName(const char *NewName);
       ///< Changes the name of this recording to the given value. NewName is in the
       ///< same format as the one returned by Name(), i.e. without the video directory
       ///< and the actual '*.rec' part, and using FOLDERDELIMCHAR as the directory
       ///< delimiter.
       ///< If the new name is the same as the old one, nothing happens.
       ///< Returns false in case of error.
  bool Delete(void);
       ///< Changes the file name so that it will no longer be visible in the "Recordings" menu
       ///< Returns false in case of error
  bool Remove(void);
       ///< Actually removes the file from the disk
       ///< Returns false in case of error
  bool Undelete(void);
       ///< Changes the file name so that it will be visible in the "Recordings" menu again and
       ///< not processed by cRemoveDeletedRecordingsThread.
       ///< Returns false in case of error
  int IsInUse(void) const;
       ///< Checks whether this recording is currently in use and therefore shall not
       ///< be tampered with. Returns 0 (ruNone) if the recording is not in use.
       ///< The return value may consist of several or'd eRecordingUsage flags. If the
       ///< caller is just interested in whether the recording is in use or not, the
       ///< return value can be used like a boolean value.
       ///< A recording may be in use for several reasons (like being recorded and replayed,
       ///< as in time-shift).
  };

class cVideoDirectoryScannerThread;

class cRecordings : public cList<cRecording> {
private:
  static cRecordings recordings;
  static cRecordings deletedRecordings;
  static int lastRecordingId;
  static char *updateFileName;
  static time_t lastUpdate;
  static cVideoDirectoryScannerThread *videoDirectoryScannerThread;
  static const char *UpdateFileName(void);
public:
  cRecordings(bool Deleted = false);
  virtual ~cRecordings();
  static const cRecordings *GetRecordingsRead(cStateKey &StateKey, int TimeoutMs = 0) { return recordings.Lock(StateKey, false, TimeoutMs) ? &recordings : NULL; }
       ///< Gets the list of recordings for read access.
       ///< See cTimers::GetTimersRead() for details.
  static cRecordings *GetRecordingsWrite(cStateKey &StateKey, int TimeoutMs = 0) { return recordings.Lock(StateKey, true, TimeoutMs) ? &recordings : NULL; }
       ///< Gets the list of recordings for write access.
       ///< See cTimers::GetTimersWrite() for details.
  static const cRecordings *GetDeletedRecordingsRead(cStateKey &StateKey, int TimeoutMs = 0) { return deletedRecordings.Lock(StateKey, false, TimeoutMs) ? &deletedRecordings : NULL; }
       ///< Gets the list of deleted recordings for read access.
       ///< See cTimers::GetTimersRead() for details.
  static cRecordings *GetDeletedRecordingsWrite(cStateKey &StateKey, int TimeoutMs = 0) { return deletedRecordings.Lock(StateKey, true, TimeoutMs) ? &deletedRecordings : NULL; }
       ///< Gets the list of deleted recordings for write access.
       ///< See cTimers::GetTimersWrite() for details.
  static void Update(bool Wait = false);
       ///< Triggers an update of the list of recordings, which will run
       ///< as a separate thread if Wait is false. If Wait is true, the
       ///< function returns only after the update has completed.
  static void TouchUpdate(void);
       ///< Touches the '.update' file in the video directory, so that other
       ///< instances of VDR that access the same video directory can be triggered
       ///< to update their recordings list.
  static bool NeedsUpdate(void);
  void ResetResume(const char *ResumeFileName = NULL);
  void ClearSortNames(void);
  const cRecording *GetById(int Id) const;
  cRecording *GetById(int Id) { return const_cast<cRecording *>(static_cast<const cRecordings *>(this)->GetById(Id)); };
  const cRecording *GetByName(const char *FileName) const;
  cRecording *GetByName(const char *FileName) { return const_cast<cRecording *>(static_cast<const cRecordings *>(this)->GetByName(FileName)); }
  void Add(cRecording *Recording);
  void AddByName(const char *FileName, bool TriggerUpdate = true);
  void DelByName(const char *FileName);
  void UpdateByName(const char *FileName);
  int TotalFileSizeMB(void) const;
  double MBperMinute(void) const;
       ///< Returns the average data rate (in MB/min) of all recordings, or -1 if
       ///< this value is unknown.
  int PathIsInUse(const char *Path) const;
       ///< Checks whether any recording in the given Path is currently in use and therefore
       ///< the whole Path shall not be tampered with. Returns 0 (ruNone) if no recording
       ///< is in use.
       ///< See cRecording::IsInUse() for details about the possible non-zero return values.
       ///< If several recordings in the Path are currently in use, the return value will
       ///< be the combination of all individual recordings' flags.
       ///< If Path is NULL or an empty string, the entire video directory is checked.
  int GetNumRecordingsInPath(const char *Path) const;
       ///< Returns the total number of recordings in the given Path, including all
       ///< sub-folders of Path.
       ///< If Path is NULL or an empty string, the entire video directory is checked.
  bool MoveRecordings(const char *OldPath, const char *NewPath);
       ///< Moves all recordings in OldPath to NewPath.
       ///< Returns true if all recordings were successfully moved.
       ///< As soon as the operation fails for one recording, the whole
       ///< action is aborted and false will be returned. Any recordings that
       ///< have been successfully moved thus far will keep their new name.
       ///< If OldPath and NewPath are on different file systems, the recordings
       ///< will be moved in a background process and this function returns true
       ///< if all recordings have been successfully added to the RecordingsHandler.
  };

// Provide lock controlled access to the list:

DEF_LIST_LOCK(Recordings);
DEF_LIST_LOCK2(Recordings, DeletedRecordings);

// These macros provide a convenient way of locking the global recordings list
// and making sure the lock is released as soon as the current scope is left
// (note that these macros wait forever to obtain the lock!):

#define LOCK_RECORDINGS_READ  USE_LIST_LOCK_READ(Recordings)
#define LOCK_RECORDINGS_WRITE USE_LIST_LOCK_WRITE(Recordings)
#define LOCK_DELETEDRECORDINGS_READ  USE_LIST_LOCK_READ2(Recordings, DeletedRecordings)
#define LOCK_DELETEDRECORDINGS_WRITE USE_LIST_LOCK_WRITE2(Recordings, DeletedRecordings)

class cRecordingsHandlerEntry;

class cRecordingsHandler : public cThread {
private:
  cMutex mutex;
  cList<cRecordingsHandlerEntry> operations;
  bool finished;
  bool error;
  cRecordingsHandlerEntry *Get(const char *FileName);
protected:
  virtual void Action(void);
public:
  cRecordingsHandler(void);
  virtual ~cRecordingsHandler();
  bool Add(int Usage, const char *FileNameSrc, const char *FileNameDst = NULL);
       ///< Adds the given FileNameSrc to the recordings handler for (later)
       ///< processing. Usage can be either ruCut, ruMove or ruCopy. FileNameDst
       ///< is only applicable for ruMove and ruCopy.
       ///< At any given time there can be only one operation for any FileNameSrc
       ///< or FileNameDst in the list. An attempt to add a file name twice will
       ///< result in an error.
       ///< Returns true if the operation was successfully added to the list.
  void Del(const char *FileName);
       ///< Deletes the given FileName from the list of operations.
       ///< If an action is already in progress, it will be terminated.
       ///< FileName can be either the FileNameSrc or FileNameDst (if applicable)
       ///< that was given when the operation was added with Add().
  void DelAll(void);
       ///< Deletes/terminates all operations.
  int GetUsage(const char *FileName);
       ///< Returns the usage type for the given FileName.
  bool Finished(bool &Error);
       ///< Returns true if all operations in the list have been finished.
       ///< If there have been any errors, Errors will be set to true.
       ///< This function will only return true once if the list of operations
       ///< has actually become empty since the last call.
  };

extern cRecordingsHandler RecordingsHandler;

#define DEFAULTFRAMESPERSECOND 25.0

class cMark : public cListObject {
  friend class cMarks; // for sorting
private:
  double framesPerSecond;
  int position;
  cString comment;
public:
  cMark(int Position = 0, const char *Comment = NULL, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
  virtual ~cMark();
  int Position(void) const { return position; }
  const char *Comment(void) const { return comment; }
  void SetPosition(int Position) { position = Position; }
  void SetComment(const char *Comment) { comment = Comment; }
  cString ToText(void);
  bool Parse(const char *s);
  bool Save(FILE *f);
  };

class cMarks : public cConfig<cMark> {
private:
  cString recordingFileName;
  cString fileName;
  double framesPerSecond;
  bool isPesRecording;
  time_t nextUpdate;
  time_t lastFileTime;
  time_t lastChange;
public:
  cMarks(void): cConfig<cMark>("Marks") {};
  static cString MarksFileName(const cRecording *Recording);
       ///< Returns the marks file name for the given Recording (regardless whether such
       ///< a file actually exists).
  static bool DeleteMarksFile(const cRecording *Recording);
  bool Load(const char *RecordingFileName, double FramesPerSecond = DEFAULTFRAMESPERSECOND, bool IsPesRecording = false);
  bool Update(void);
  bool Save(void);
  void Align(void);
  void Sort(void);
  void Add(int Position);
       ///< If this cMarks object is used by multiple threads, the caller must Lock()
       ///< it before calling Add() and Unlock() it afterwards. The same applies to
       ///< calls to Del(), or any of the functions that return a "cMark *", in case
       ///< an other thread might modify the list while the returned pointer is
       ///< considered valid.
  const cMark *Get(int Position) const;
  const cMark *GetPrev(int Position) const;
  const cMark *GetNext(int Position) const;
  const cMark *GetNextBegin(const cMark *EndMark = NULL) const;
       ///< Returns the next "begin" mark after EndMark, skipping any marks at the
       ///< same position as EndMark. If EndMark is NULL, the first actual "begin"
       ///< will be returned (if any).
  const cMark *GetNextEnd(const cMark *BeginMark) const;
       ///< Returns the next "end" mark after BeginMark, skipping any marks at the
       ///< same position as BeginMark.
  int GetNumSequences(void) const;
       ///< Returns the actual number of sequences to be cut from the recording.
       ///< If there is only one actual "begin" mark, and it is positioned at index
       ///< 0 (the beginning of the recording), and there is no "end" mark, the
       ///< return value is 0, which means that the result is the same as the original
       ///< recording.
  cMark *Get(int Position) { return const_cast<cMark *>(static_cast<const cMarks *>(this)->Get(Position)); }
  cMark *GetPrev(int Position) { return const_cast<cMark *>(static_cast<const cMarks *>(this)->GetPrev(Position)); }
  cMark *GetNext(int Position) { return const_cast<cMark *>(static_cast<const cMarks *>(this)->GetNext(Position)); }
  cMark *GetNextBegin(const cMark *EndMark = NULL) { return const_cast<cMark *>(static_cast<const cMarks *>(this)->GetNextBegin(EndMark)); }
  cMark *GetNextEnd(const cMark *BeginMark) { return const_cast<cMark *>(static_cast<const cMarks *>(this)->GetNextEnd(BeginMark)); }
  };

#define RUC_BEFORERECORDING  "before"
#define RUC_STARTRECORDING   "started"
#define RUC_AFTERRECORDING   "after"
#define RUC_EDITINGRECORDING "editing"
#define RUC_EDITEDRECORDING  "edited"
#define RUC_DELETERECORDING  "deleted"
#define RUC_RENAMEDRECORDING "renamed" // same directory, only the base name is changed
#define RUC_MOVEDRECORDING   "moved"   // different directory (and maybe base name), or "copy" to other filesystem + delete original (triggers copying->copied->deleted)
#define RUC_COPYINGRECORDING "copying"
#define RUC_COPIEDRECORDING  "copied"

class cRecordingUserCommand {
private:
  static const char *command;
public:
  static void SetCommand(const char *Command) { command = Command; }
  static void InvokeCommand(const char *State, const char *RecordingFileName, const char *SourceFileName = NULL);
  };

// The maximum size of a single frame (up to HDTV 1920x1080):
#define MAXFRAMESIZE  (KILOBYTE(1024) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE to avoid breaking up TS packets

// The maximum file size is limited by the range that can be covered
// with a 40 bit 'unsigned int', which is 1TB. The actual maximum value
// used is 6MB below the theoretical maximum, to have some safety (the
// actual file size may be slightly higher because we stop recording only
// before the next independent frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZETS  1048570 // MB
#define MAXVIDEOFILESIZEPES    2000 // MB
#define MINVIDEOFILESIZE        100 // MB
#define MAXVIDEOFILESIZEDEFAULT MAXVIDEOFILESIZEPES

struct tIndexTs;
class cIndexFileGenerator;

class cIndexFile {
private:
  int f;
  cString fileName;
  int size, last;
  tIndexTs *index;
  bool isPesRecording;
  cResumeFile resumeFile;
  cIndexFileGenerator *indexFileGenerator;
  cMutex mutex;
  void ConvertFromPes(tIndexTs *IndexTs, int Count);
  void ConvertToPes(tIndexTs *IndexTs, int Count);
  bool CatchUp(int Index = -1);
public:
  cIndexFile(const char *FileName, bool Record, bool IsPesRecording = false, bool PauseLive = false, bool Update = false);
  ~cIndexFile();
  bool Ok(void) { return index != NULL; }
  bool Write(bool Independent, uint16_t FileNumber, off_t FileOffset);
  bool Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent = NULL, int *Length = NULL);
  int GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber = NULL, off_t *FileOffset = NULL, int *Length = NULL);
  int GetClosestIFrame(int Index);
       ///< Returns the index of the I-frame that is closest to the given Index (or Index itself,
       ///< if it already points to an I-frame). Index may be any value, even outside the current
       ///< range of frame indexes.
       ///< If there is no actual index data available, 0 is returned.
  int Get(uint16_t FileNumber, off_t FileOffset);
  int Last(void) { CatchUp(); return last; }
       ///< Returns the index of the last entry in this file, or -1 if the file is empty.
  int GetResume(void) { return resumeFile.Read(); }
  bool StoreResume(int Index) { return resumeFile.Save(Index); }
  bool IsStillRecording(void);
  void Delete(void);
  static int GetLength(const char *FileName, bool IsPesRecording = false);
       ///< Calculates the recording length (number of frames) without actually reading the index file.
       ///< Returns -1 in case of error.
  static cString IndexFileName(const char *FileName, bool IsPesRecording);
  };

class cFileName {
private:
  cUnbufferedFile *file;
  uint16_t fileNumber;
  char *fileName, *pFileNumber;
  bool record;
  bool blocking;
  bool isPesRecording;
public:
  cFileName(const char *FileName, bool Record, bool Blocking = false, bool IsPesRecording = false);
  ~cFileName();
  const char *Name(void) { return fileName; }
  uint16_t Number(void) { return fileNumber; }
  bool GetLastPatPmtVersions(int &PatVersion, int &PmtVersion);
  cUnbufferedFile *Open(void);
  void Close(void);
  cUnbufferedFile *SetOffset(int Number, off_t Offset = 0); // yes, Number is int for easier internal calculating
  cUnbufferedFile *NextFile(void);
  };

class cDoneRecordings {
private:
  cString fileName;
  cStringList doneRecordings;
  void Add(const char *Title);
public:
  bool Load(const char *FileName);
  bool Save(void) const;
  void Append(const char *Title);
  bool Contains(const char *Title) const;
  };

extern cDoneRecordings DoneRecordingsPattern;

cString IndexToHMSF(int Index, bool WithFrame = false, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
      // Converts the given index to a string, optionally containing the frame number.
int HMSFToIndex(const char *HMSF, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
      // Converts the given string (format: "hh:mm:ss.ff") to an index.
int SecondsToFrames(int Seconds, double FramesPerSecond = DEFAULTFRAMESPERSECOND);
      // Returns the number of frames corresponding to the given number of seconds.

int ReadFrame(cUnbufferedFile *f, uchar *b, int Length, int Max);

char *ExchangeChars(char *s, bool ToFileSystem);
      // Exchanges the characters in the given string to or from a file system
      // specific representation (depending on ToFileSystem). The given string will
      // be modified and may be reallocated if more space is needed. The return
      // value points to the resulting string, which may be different from s.

bool GenerateIndex(const char *FileName, bool Update = false);
       ///< Generates the index of the existing recording with the given FileName.
       ///< If Update is true, an existing index file will be checked whether it is
       ///< complete, and will be updated if it isn't. Otherwise an existing index
       ///< file will be removed before a new one is generated.

enum eRecordingsSortDir { rsdAscending, rsdDescending };
enum eRecordingsSortMode { rsmName, rsmTime };
extern eRecordingsSortMode RecordingsSortMode;
bool HasRecordingsSortMode(const char *Directory);
void GetRecordingsSortMode(const char *Directory);
void SetRecordingsSortMode(const char *Directory, eRecordingsSortMode SortMode);
void IncRecordingsSortMode(const char *Directory);

void SetRecordingTimerId(const char *Directory, const char *TimerId);
cString GetRecordingTimerId(const char *Directory);

#endif //__RECORDING_H
