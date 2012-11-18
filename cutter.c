/*
 * cutter.c: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.c 2.16 2012/11/18 12:09:00 kls Exp $
 */

#include "cutter.h"
#include "menu.h"
#include "recording.h"
#include "remux.h"
#include "videodir.h"

// --- cPacketBuffer ---------------------------------------------------------

class cPacketBuffer {
private:
  uchar *data;
  int size;
  int length;
public:
  cPacketBuffer(void);
  ~cPacketBuffer();
  void Append(uchar *Data, int Length);
       ///< Appends Length bytes of Data to this packet buffer.
  void Flush(uchar *Data, int &Length, int MaxLength);
       ///< Flushes the content of this packet buffer into the given Data, starting
       ///< at position Length, and clears the buffer afterwards. Length will be
       ///< incremented accordingly. If Length plus the total length of the stored
       ///< packets would exceed MaxLength, nothing is copied.
  };

cPacketBuffer::cPacketBuffer(void)
{
  data = NULL;
  size = length = 0;
}

cPacketBuffer::~cPacketBuffer()
{
  free(data);
}

void cPacketBuffer::Append(uchar *Data, int Length)
{
  if (length + Length >= size) {
     int NewSize = (length + Length) * 3 / 2;
     if (uchar *p = (uchar *)realloc(data, NewSize)) {
        data = p;
        size = NewSize;
        }
     else
        return; // out of memory
     }
  memcpy(data + length, Data, Length);
  length += Length;
}

void cPacketBuffer::Flush(uchar *Data, int &Length, int MaxLength)
{
  if (Data && length > 0 && Length + length <= MaxLength) {
     memcpy(Data + Length, data, length);
     Length += length;
     }
  length = 0;
}

// --- cPacketStorage --------------------------------------------------------

class cPacketStorage {
private:
  cPacketBuffer *buffers[MAXPID];
public:
  cPacketStorage(void);
  ~cPacketStorage();
  void Append(int Pid, uchar *Data, int Length);
  void Flush(int Pid, uchar *Data, int &Length, int MaxLength);
  };

cPacketStorage::cPacketStorage(void)
{
  for (int i = 0; i < MAXPID; i++)
      buffers[i] = NULL;
}

cPacketStorage::~cPacketStorage()
{
  for (int i = 0; i < MAXPID; i++)
      delete buffers[i];
}

void cPacketStorage::Append(int Pid, uchar *Data, int Length)
{
  if (!buffers[Pid])
     buffers[Pid] = new cPacketBuffer;
  buffers[Pid]->Append(Data, Length);
}

void cPacketStorage::Flush(int Pid, uchar *Data, int &Length, int MaxLength)
{
  if (buffers[Pid])
     buffers[Pid]->Flush(Data, Length, MaxLength);
}

// --- cDanglingPacketStripper -----------------------------------------------

class cDanglingPacketStripper {
private:
  bool processed[MAXPID];
  cPatPmtParser patPmtParser;
public:
  cDanglingPacketStripper(void);
  bool Process(uchar *Data, int Length, int64_t FirstPts);
       ///< Scans the frame given in Data and hides the payloads of any TS packets
       ///< that either didn't start within this frame, or have a PTS that is
       ///< before FirstPts. The TS packets in question are not physically removed
       ///< from Data in order to keep any frame counts and PCR timestamps intact.
       ///< Returns true if any dangling packets have been found.
  };

cDanglingPacketStripper::cDanglingPacketStripper(void)
{
  memset(processed, 0x00, sizeof(processed));
}

bool cDanglingPacketStripper::Process(uchar *Data, int Length, int64_t FirstPts)
{
  bool Found = false;
  while (Length >= TS_SIZE && *Data == TS_SYNC_BYTE) {
        int Pid = TsPid(Data);
        if (Pid == PATPID)
           patPmtParser.ParsePat(Data, TS_SIZE);
        else if (Pid == patPmtParser.PmtPid())
           patPmtParser.ParsePmt(Data, TS_SIZE);
        else {
           int64_t Pts = TsGetPts(Data, TS_SIZE);
           if (Pts >= 0)
              processed[Pid] = PtsDiff(FirstPts, Pts) >= 0; // Pts is at or after FirstPts
           if (!processed[Pid]) {
              TsHidePayload(Data);
              Found = true;
              }
           }
        Length -= TS_SIZE;
        Data += TS_SIZE;
        }
  return Found;
}

// --- cPtsFixer -------------------------------------------------------------

class cPtsFixer {
private:
  int delta; // time between two frames
  int64_t last; // the last (i.e. highest) video PTS value seen
  int64_t offset; // offset to add to PTS values
  bool fixCounters; // controls fixing the TS continuity counters (only from the second CutIn up)
  uchar counter[MAXPID]; // the TS continuity counter for each PID
  cPatPmtParser patPmtParser;
public:
  cPtsFixer(void);
  void Setup(double FramesPerSecond);
  void Fix(uchar *Data, int Length, bool CutIn);
  };

cPtsFixer::cPtsFixer(void)
{
  delta = 0;
  last = -1;
  offset = -1;
  fixCounters = false;
  memset(counter, 0x00, sizeof(counter));
}

void cPtsFixer::Setup(double FramesPerSecond)
{
  delta = int(round(PTSTICKS / FramesPerSecond));
}

void cPtsFixer::Fix(uchar *Data, int Length, bool CutIn)
{
  if (!patPmtParser.Vpid()) {
     if (!patPmtParser.ParsePatPmt(Data, Length))
        return;
     }
  // Determine the PTS offset at the beginning of each sequence (except the first one):
  if (CutIn && last >= 0) {
     int64_t Pts = TsGetPts(Data, Length);
     if (Pts >= 0) {
        // offset is calculated so that Pts + offset results in last + delta:
        offset = Pts - PtsAdd(last, delta);
        if (offset <= 0)
           offset = -offset;
        else
           offset = MAX33BIT + 1 - offset;
        }
     fixCounters = true;
     }
  // Keep track of the highest video PTS:
  uchar *p = Data;
  int len = Length;
  while (len >= TS_SIZE && *p == TS_SYNC_BYTE) {
        int Pid = TsPid(p);
        if (Pid == patPmtParser.Vpid()) {
           int64_t Pts = PtsAdd(TsGetPts(p, TS_SIZE), offset); // offset is taken into account here, to make last have the "new" value already!
           if (Pts >= 0 && (last < 0 || PtsDiff(last, Pts) > 0))
              last = Pts;
           }
        // Adjust the TS continuity counter:
        if (fixCounters) {
           counter[Pid] = (counter[Pid] + 1) & TS_CONT_CNT_MASK;
           TsSetContinuityCounter(p, counter[Pid]);
           }
        else
           counter[Pid] = TsGetContinuityCounter(p); // collect initial counters
        p += TS_SIZE;
        len -= TS_SIZE;
        }
  // Apply the PTS offset:
  if (offset > 0) {
     uchar *p = Data;
     int len = Length;
     while (len >= TS_SIZE && *p == TS_SYNC_BYTE) {
           // Adjust the various timestamps:
           int64_t Pts = TsGetPts(p, TS_SIZE);
           if (Pts >= 0)
              TsSetPts(p, TS_SIZE, PtsAdd(Pts, offset));
           int64_t Dts = TsGetDts(p, TS_SIZE);
           if (Dts >= 0)
              TsSetDts(p, TS_SIZE, PtsAdd(Dts, offset));
           int64_t Pcr = TsGetPcr(p);
           if (Pcr >= 0) {
              int64_t NewPcr = Pcr + offset * PCRFACTOR;
              if (NewPcr >= MAX27MHZ)
                 NewPcr -= MAX27MHZ + 1;
              TsSetPcr(p, NewPcr);
              }
           p += TS_SIZE;
           len -= TS_SIZE;
           }
     }
}

// --- cCuttingThread --------------------------------------------------------

class cCuttingThread : public cThread {
private:
  const char *error;
  bool isPesRecording;
  double framesPerSecond;
  cUnbufferedFile *fromFile, *toFile;
  cFileName *fromFileName, *toFileName;
  cIndexFile *fromIndex, *toIndex;
  cMarks fromMarks, toMarks;
  int numSequences;
  off_t maxVideoFileSize;
  off_t fileSize;
  cPtsFixer ptsFixer;
  bool suspensionLogged;
  bool Throttled(void);
  bool SwitchFile(bool Force = false);
  bool LoadFrame(int Index, uchar *Buffer, bool &Independent, int &Length);
  bool FramesAreEqual(int Index1, int Index2);
  void GetPendingPackets(uchar *Buffer, int &Length, int Index, int64_t LastPts);
       // Gather all non-video TS packets from Index upward that either belong to
       // payloads that started before Index, or have a PTS that is before LastPts,
       // and add them to the end of the given Data.
  bool ProcessSequence(int LastEndIndex, int BeginIndex, int EndIndex, int NextBeginIndex);
protected:
  virtual void Action(void);
public:
  cCuttingThread(const char *FromFileName, const char *ToFileName);
  virtual ~cCuttingThread();
  const char *Error(void) { return error; }
  };

cCuttingThread::cCuttingThread(const char *FromFileName, const char *ToFileName)
:cThread("video cutting", true)
{
  error = NULL;
  fromFile = toFile = NULL;
  fromFileName = toFileName = NULL;
  fromIndex = toIndex = NULL;
  cRecording Recording(FromFileName);
  isPesRecording = Recording.IsPesRecording();
  framesPerSecond = Recording.FramesPerSecond();
  suspensionLogged = false;
  fileSize = 0;
  ptsFixer.Setup(framesPerSecond);
  if (fromMarks.Load(FromFileName, framesPerSecond, isPesRecording) && fromMarks.Count()) {
     numSequences = fromMarks.GetNumSequences();
     if (numSequences > 0) {
        fromFileName = new cFileName(FromFileName, false, true, isPesRecording);
        toFileName = new cFileName(ToFileName, true, true, isPesRecording);
        fromIndex = new cIndexFile(FromFileName, false, isPesRecording);
        toIndex = new cIndexFile(ToFileName, true, isPesRecording);
        toMarks.Load(ToFileName, framesPerSecond, isPesRecording); // doesn't actually load marks, just sets the file name
        maxVideoFileSize = MEGABYTE(Setup.MaxVideoFileSize);
        if (isPesRecording && maxVideoFileSize > MEGABYTE(MAXVIDEOFILESIZEPES))
           maxVideoFileSize = MEGABYTE(MAXVIDEOFILESIZEPES);
        Start();
        }
     else
        esyslog("no editing sequences found for %s", FromFileName);
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

bool cCuttingThread::Throttled(void)
{
  if (cIoThrottle::Engaged()) {
     if (!suspensionLogged) {
        dsyslog("suspending cutter thread");
        suspensionLogged = true;
        }
     return true;
     }
  else if (suspensionLogged) {
     dsyslog("resuming cutter thread");
     suspensionLogged = false;
     }
  return false;
}

bool cCuttingThread::LoadFrame(int Index, uchar *Buffer, bool &Independent, int &Length)
{
  uint16_t FileNumber;
  off_t FileOffset;
  if (fromIndex->Get(Index, &FileNumber, &FileOffset, &Independent, &Length)) {
     fromFile = fromFileName->SetOffset(FileNumber, FileOffset);
     if (fromFile) {
        fromFile->SetReadAhead(MEGABYTE(20));
        int len = ReadFrame(fromFile, Buffer,  Length, MAXFRAMESIZE);
        if (len < 0)
           error = "ReadFrame";
        else if (len != Length)
           Length = len;
        return error == NULL;
        }
     else
        error = "fromFile";
     }
  return false;
}

bool cCuttingThread::SwitchFile(bool Force)
{
  if (fileSize > maxVideoFileSize || Force) {
     toFile = toFileName->NextFile();
     if (!toFile) {
        error = "toFile";
        return false;
        }
     fileSize = 0;
     }
  return true;
}

bool cCuttingThread::FramesAreEqual(int Index1, int Index2)
{
  bool Independent;
  uchar Buffer1[MAXFRAMESIZE];
  uchar Buffer2[MAXFRAMESIZE];
  int Length1;
  int Length2;
  if (LoadFrame(Index1, Buffer1, Independent, Length1) && LoadFrame(Index2, Buffer2, Independent, Length2)) {
     if (Length1 == Length2) {
        int Diffs = 0;
        for (int i = 0; i < Length1; i++) {
            if (Buffer1[i] != Buffer2[i]) {
               if (Diffs++ > 10) // the continuity counters of the PAT/PMT packets may differ
                  return false;
               }
            }
        return true;
        }
     }
  return false;
}

void cCuttingThread::GetPendingPackets(uchar *Data, int &Length, int Index, int64_t LastPts)
{
  bool Processed[MAXPID] = { false };
  int NumIndependentFrames = 0;
  cPatPmtParser PatPmtParser;
  cPacketStorage PacketStorage;
  for (; NumIndependentFrames < 2; Index++) {
      uchar Buffer[MAXFRAMESIZE];
      bool Independent;
      int len;
      if (LoadFrame(Index, Buffer, Independent, len)) {
         if (Independent)
            NumIndependentFrames++;
         uchar *p = Buffer;
         while (len >= TS_SIZE && *p == TS_SYNC_BYTE) {
               int Pid = TsPid(p);
               if (Pid == PATPID)
                  PatPmtParser.ParsePat(p, TS_SIZE);
               else if (Pid == PatPmtParser.PmtPid())
                  PatPmtParser.ParsePmt(p, TS_SIZE);
               else if (!Processed[Pid]) {
                  int64_t Pts = TsGetPts(p, TS_SIZE);
                  if (Pts >= 0) {
                     int64_t d = PtsDiff(LastPts, Pts);
                     if (d <= 0) // Pts is before or at LastPts
                        PacketStorage.Flush(Pid, Data, Length, MAXFRAMESIZE);
                     if (d >= 0) { // Pts is at or after LastPts
                        NumIndependentFrames = 0; // we search until we find two consecutive I-frames without any more pending packets
                        Processed[Pid] = true;
                        }
                     }
                  if (!Processed[Pid])
                     PacketStorage.Append(Pid, p, TS_SIZE);
                  }
               len -= TS_SIZE;
               p += TS_SIZE;
               }
         }
      else
         break;
      }
}

bool cCuttingThread::ProcessSequence(int LastEndIndex, int BeginIndex, int EndIndex, int NextBeginIndex)
{
  // Check for seamless connections:
  bool SeamlessBegin = LastEndIndex >= 0 && FramesAreEqual(LastEndIndex, BeginIndex);
  bool SeamlessEnd = NextBeginIndex >= 0 && FramesAreEqual(EndIndex, NextBeginIndex);
  // Process all frames from BeginIndex (included) to EndIndex (excluded):
  cDanglingPacketStripper DanglingPacketStripper;
  int NumIndependentFrames = 0;
  int64_t FirstPts = -1;
  int64_t LastPts = -1;
  for (int Index = BeginIndex; Running() && Index < EndIndex; Index++) {
      uchar Buffer[MAXFRAMESIZE];
      bool Independent;
      int Length;
      if (LoadFrame(Index, Buffer, Independent, Length)) {
         if (!isPesRecording) {
            int64_t Pts = TsGetPts(Buffer, Length);
            if (FirstPts < 0)
               FirstPts = Pts; // the PTS of the first frame in the sequence
            else if (LastPts < 0 || PtsDiff(LastPts, Pts) > 0)
               LastPts = Pts; // the PTS of the frame that is displayed as the very last one of the sequence
            }
         // Fixup data at the beginning of the sequence:
         if (!SeamlessBegin) {
            if (isPesRecording) {
               if (Index == BeginIndex)
                  cRemux::SetBrokenLink(Buffer, Length);
               }
            else if (NumIndependentFrames < 2) {
               if (DanglingPacketStripper.Process(Buffer, Length, FirstPts))
                  NumIndependentFrames = 0; // we search until we find two consecutive I-frames without any more dangling packets
               }
            }
         // Fixup data at the end of the sequence:
         if (!SeamlessEnd) {
            if (Index == EndIndex - 1) {
               if (!isPesRecording)
                  GetPendingPackets(Buffer, Length, EndIndex, LastPts + int(round(PTSTICKS / framesPerSecond))); // adding one frame length to fully cover the very last frame
               }
            }
         // Fixup timestamps and continuity counters:
         if (!isPesRecording) {
            if (numSequences > 1)
              ptsFixer.Fix(Buffer, Length, !SeamlessBegin && Index == BeginIndex);
            }
         // Every file shall start with an independent frame:
         if (Independent) {
            NumIndependentFrames++;
            if (!SwitchFile())
               return false;
            }
         // Write index:
         if (!toIndex->Write(Independent, toFileName->Number(), fileSize)) {
            error = "toIndex";
            return false;
            }
         // Write data:
         if (toFile->Write(Buffer, Length) < 0) {
            error = "safe_write";
            return false;
            }
         fileSize += Length;
         // Generate marks at the editing points in the edited recording:
         if (numSequences > 0 && Index == BeginIndex) {
            if (toMarks.Count() > 0)
               toMarks.Add(toIndex->Last());
            toMarks.Add(toIndex->Last());
            toMarks.Save();
            }
         }
      else
         return false;
      }
  return true;
}

void cCuttingThread::Action(void)
{
  if (cMark *BeginMark = fromMarks.GetNextBegin()) {
     fromFile = fromFileName->Open();
     toFile = toFileName->Open();
     if (!fromFile || !toFile)
        return;
     int LastEndIndex = -1;
     while (BeginMark && Running()) {
           // Suspend cutting if we have severe throughput problems:
           if (Throttled()) {
              cCondWait::SleepMs(100);
              continue;
              }
           // Make sure there is enough disk space:
           AssertFreeDiskSpace(-1);
           // Determine the actual begin and end marks, skipping any marks at the same position:
           cMark *EndMark = fromMarks.GetNextEnd(BeginMark);
           // Process the current sequence:
           int EndIndex = EndMark ? EndMark->Position() : fromIndex->Last() + 1;
           int NextBeginIndex = -1;
           if (EndMark) {
              if (cMark *NextBeginMark = fromMarks.GetNextBegin(EndMark))
                 NextBeginIndex = NextBeginMark->Position();
              }
           if (!ProcessSequence(LastEndIndex, BeginMark->Position(), EndIndex, NextBeginIndex))
              break;
           if (!EndMark)
              break; // reached EOF
           LastEndIndex = EndIndex;
           // Switch to the next sequence:
           BeginMark = fromMarks.GetNextBegin(EndMark);
           if (BeginMark) {
              // Split edited files:
              if (Setup.SplitEditedFiles) {
                 if (!SwitchFile(true))
                    break;
                 }
              }
           }
     Recordings.TouchUpdate();
     }
  else
     esyslog("no editing marks found!");
}

// --- cCutter ---------------------------------------------------------------

cMutex cCutter::mutex;
cString cCutter::originalVersionName;
cString cCutter::editedVersionName;
cCuttingThread *cCutter::cuttingThread = NULL;
bool cCutter::error = false;
bool cCutter::ended = false;

bool cCutter::Start(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  if (!cuttingThread) {
     error = false;
     ended = false;
     originalVersionName = FileName;
     cRecording Recording(FileName);

     cMarks FromMarks;
     FromMarks.Load(FileName, Recording.FramesPerSecond(), Recording.IsPesRecording());
     if (cMark *First = FromMarks.GetNextBegin())
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
        editedVersionName = evn;
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
  if ((Interrupted || Error) && *editedVersionName) {
     if (Interrupted)
        isyslog("editing process has been interrupted");
     if (Error)
        esyslog("ERROR: '%s' during editing process", Error);
     if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), editedVersionName) == 0)
        cControl::Shutdown();
     RemoveVideoFile(editedVersionName);
     Recordings.DelByName(editedVersionName);
     }
}

bool cCutter::Active(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  if (cuttingThread) {
     if (cuttingThread->Active())
        return !FileName || strcmp(FileName, originalVersionName) == 0 || strcmp(FileName, editedVersionName) == 0;
     error = cuttingThread->Error();
     Stop();
     if (!error)
        cRecordingUserCommand::InvokeCommand(RUC_EDITEDRECORDING, editedVersionName, originalVersionName);
     originalVersionName = NULL;
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
           if (Marks.GetNumSequences()) {
              if (cCutter::Start(FileName)) {
                 while (cCutter::Active())
                       cCondWait::SleepMs(CUTTINGCHECKINTERVAL);
                 return true;
                 }
              else
                 fprintf(stderr, "can't start editing process\n");
              }
           else
              fprintf(stderr, "'%s' has no editing sequences\n", FileName);
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
