/*
 * cutter.c: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.c 2.10 2011/12/04 12:55:53 kls Exp $
 */

#include "cutter.h"
#include "recording.h"
#include "remux.h"
#include "videodir.h"

// --- cCuttingThread --------------------------------------------------------

class cCuttingThread : public cThread {
private:
  const char *error;
  bool isPesRecording;
  cUnbufferedFile *fromFile, *toFile;
  cFileName *fromFileName, *toFileName;
  cIndexFile *fromIndex, *toIndex;
  cMarks fromMarks, toMarks;
  off_t maxVideoFileSize;
protected:
  virtual void Action(void);
public:
  cCuttingThread(const char *FromFileName, const char *ToFileName);
  virtual ~cCuttingThread();
  const char *Error(void) { return error; }
  };

cCuttingThread::cCuttingThread(const char *FromFileName, const char *ToFileName)
:cThread("video cutting")
{
  error = NULL;
  fromFile = toFile = NULL;
  fromFileName = toFileName = NULL;
  fromIndex = toIndex = NULL;
  cRecording Recording(FromFileName);
  isPesRecording = Recording.IsPesRecording();
  if (fromMarks.Load(FromFileName, Recording.FramesPerSecond(), isPesRecording) && fromMarks.Count()) {
     fromFileName = new cFileName(FromFileName, false, true, isPesRecording);
     toFileName = new cFileName(ToFileName, true, true, isPesRecording);
     fromIndex = new cIndexFile(FromFileName, false, isPesRecording);
     toIndex = new cIndexFile(ToFileName, true, isPesRecording);
     toMarks.Load(ToFileName, Recording.FramesPerSecond(), isPesRecording); // doesn't actually load marks, just sets the file name
     maxVideoFileSize = MEGABYTE(Setup.MaxVideoFileSize);
     if (isPesRecording && maxVideoFileSize > MEGABYTE(MAXVIDEOFILESIZEPES))
        maxVideoFileSize = MEGABYTE(MAXVIDEOFILESIZEPES);
     Start();
     }
  else
     esyslog("no editing marks found for %s", FromFileName);
}

cCuttingThread::~cCuttingThread()
{
  Cancel(3);
  delete fromFileName;
  delete toFileName;
  delete fromIndex;
  delete toIndex;
}

void cCuttingThread::Action(void)
{
  cMark *Mark = fromMarks.First();
  if (Mark) {
     SetPriority(19);
     SetIOPriority(7);
     fromFile = fromFileName->Open();
     toFile = toFileName->Open();
     if (!fromFile || !toFile)
        return;
     fromFile->SetReadAhead(MEGABYTE(20));
     int Index = Mark->Position();
     Mark = fromMarks.Next(Mark);
     off_t FileSize = 0;
     int CurrentFileNumber = 0;
     int LastIFrame = 0;
     toMarks.Add(0);
     toMarks.Save();
     uchar buffer[MAXFRAMESIZE];
     bool LastMark = false;
     bool cutIn = true;
     while (Running()) {
           uint16_t FileNumber;
           off_t FileOffset;
           int Length;
           bool Independent;

           // Make sure there is enough disk space:

           AssertFreeDiskSpace(-1);

           // Read one frame:

           if (fromIndex->Get(Index++, &FileNumber, &FileOffset, &Independent, &Length)) {
              if (FileNumber != CurrentFileNumber) {
                 fromFile = fromFileName->SetOffset(FileNumber, FileOffset);
                 if (fromFile)
                    fromFile->SetReadAhead(MEGABYTE(20));
                 CurrentFileNumber = FileNumber;
                 }
              if (fromFile) {
                 int len = ReadFrame(fromFile, buffer,  Length, sizeof(buffer));
                 if (len < 0) {
                    error = "ReadFrame";
                    break;
                    }
                 if (len != Length) {
                    CurrentFileNumber = 0; // this re-syncs in case the frame was larger than the buffer
                    Length = len;
                    }
                 }
              else {
                 error = "fromFile";
                 break;
                 }
              }
           else {
              // Error, unless we're past the last cut-in and there's no cut-out
              if (Mark || LastMark)
                 error = "index";
              break;
              }

           // Write one frame:

           if (Independent) { // every file shall start with an independent frame
              if (LastMark) // edited version shall end before next I-frame
                 break;
              if (FileSize > maxVideoFileSize) {
                 toFile = toFileName->NextFile();
                 if (!toFile) {
                    error = "toFile 1";
                    break;
                    }
                 FileSize = 0;
                 }
              LastIFrame = 0;

              if (cutIn) {
                 if (isPesRecording)
                    cRemux::SetBrokenLink(buffer, Length);
                 else
                    TsSetTeiOnBrokenPackets(buffer, Length);
                 cutIn = false;
                 }
              }
           if (toFile->Write(buffer, Length) < 0) {
              error = "safe_write";
              break;
              }
           if (!toIndex->Write(Independent, toFileName->Number(), FileSize)) {
              error = "toIndex";
              break;
              }
           FileSize += Length;
           if (!LastIFrame)
              LastIFrame = toIndex->Last();

           // Check editing marks:

           if (Mark && Index >= Mark->Position()) {
              Mark = fromMarks.Next(Mark);
              toMarks.Add(LastIFrame);
              if (Mark)
                 toMarks.Add(toIndex->Last() + 1);
              toMarks.Save();
              if (Mark) {
                 Index = Mark->Position();
                 Mark = fromMarks.Next(Mark);
                 CurrentFileNumber = 0; // triggers SetOffset before reading next frame
                 cutIn = true;
                 if (Setup.SplitEditedFiles) {
                    toFile = toFileName->NextFile();
                    if (!toFile) {
                       error = "toFile 2";
                       break;
                       }
                    FileSize = 0;
                    }
                 }
              else
                 LastMark = true;
              }
           }
     Recordings.TouchUpdate();
     }
  else
     esyslog("no editing marks found!");
}

// --- cCutter ---------------------------------------------------------------

cMutex cCutter::mutex;
char *cCutter::editedVersionName = NULL;
cCuttingThread *cCutter::cuttingThread = NULL;
bool cCutter::error = false;
bool cCutter::ended = false;

bool cCutter::Start(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  if (!cuttingThread) {
     error = false;
     ended = false;
     cRecording Recording(FileName);

     cMarks FromMarks;
     FromMarks.Load(FileName, Recording.FramesPerSecond(), Recording.IsPesRecording());
     if (cMark *First = FromMarks.First())
        Recording.SetStartTime(Recording.Start() + (int(First->Position() / Recording.FramesPerSecond() + 30) / 60) * 60);

     const char *evn = Recording.PrefixFileName('%');
     if (evn && RemoveVideoFile(evn) && MakeDirs(evn, true)) {
        // XXX this can be removed once RenameVideoFile() follows symlinks (see videodir.c)
        // remove a possible deleted recording with the same name to avoid symlink mixups:
        char *s = strdup(evn);
        char *e = strrchr(s, '.');
        if (e) {
           if (strcmp(e, ".rec") == 0) {
              strcpy(e, ".del");
              RemoveVideoFile(s);
              }
           }
        free(s);
        // XXX
        editedVersionName = strdup(evn);
        Recording.WriteInfo();
        Recordings.AddByName(editedVersionName, false);
        cuttingThread = new cCuttingThread(FileName, editedVersionName);
        return true;
        }
     }
  return false;
}

void cCutter::Stop(void)
{
  cMutexLock MutexLock(&mutex);
  bool Interrupted = cuttingThread && cuttingThread->Active();
  const char *Error = cuttingThread ? cuttingThread->Error() : NULL;
  delete cuttingThread;
  cuttingThread = NULL;
  if ((Interrupted || Error) && editedVersionName) {
     if (Interrupted)
        isyslog("editing process has been interrupted");
     if (Error)
        esyslog("ERROR: '%s' during editing process", Error);
     RemoveVideoFile(editedVersionName); //XXX what if this file is currently being replayed?
     Recordings.DelByName(editedVersionName);
     }
}

bool cCutter::Active(void)
{
  cMutexLock MutexLock(&mutex);
  if (cuttingThread) {
     if (cuttingThread->Active())
        return true;
     error = cuttingThread->Error();
     Stop();
     if (!error)
        cRecordingUserCommand::InvokeCommand(RUC_EDITEDRECORDING, editedVersionName);
     free(editedVersionName);
     editedVersionName = NULL;
     ended = true;
     }
  return false;
}

bool cCutter::Error(void)
{
  cMutexLock MutexLock(&mutex);
  bool result = error;
  error = false;
  return result;
}

bool cCutter::Ended(void)
{
  cMutexLock MutexLock(&mutex);
  bool result = ended;
  ended = false;
  return result;
}

#define CUTTINGCHECKINTERVAL 500 // ms between checks for the active cutting process

bool CutRecording(const char *FileName)
{
  if (DirectoryOk(FileName)) {
     cRecording Recording(FileName);
     if (Recording.Name()) {
        cMarks Marks;
        if (Marks.Load(FileName, Recording.FramesPerSecond(), Recording.IsPesRecording()) && Marks.Count()) {
           if (cCutter::Start(FileName)) {
              while (cCutter::Active())
                    cCondWait::SleepMs(CUTTINGCHECKINTERVAL);
              return true;
              }
           else
              fprintf(stderr, "can't start editing process\n");
           }
        else
           fprintf(stderr, "'%s' has no editing marks\n", FileName);
        }
     else
        fprintf(stderr, "'%s' is not a recording\n", FileName);
     }
  else
     fprintf(stderr, "'%s' is not a directory\n", FileName);
  return false;
}
