/*
 * cutter.c: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.c 5.7 2025/12/31 11:00:06 kls Exp $
 */

#include "cutter.h"
#include "menu.h"
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

// --- cMpeg2Fixer -----------------------------------------------------------

class cMpeg2Fixer : private cTsPayload {
private:
  bool FindHeader(uint32_t Code, const char *Header);
public:
  cMpeg2Fixer(uchar *Data, int Length, int Vpid);
  void SetBrokenLink(void);
  void SetClosedGop(void);
  int GetTref(void);
  void AdjGopTime(int Offset, int FramesPerSecond);
  void AdjTref(int TrefOffset);
  };

cMpeg2Fixer::cMpeg2Fixer(uchar *Data, int Length, int Vpid)
{
  // Go to first video packet:
  for (; Length > 0; Data += TS_SIZE, Length -= TS_SIZE) {
      if (TsPid(Data) == Vpid) {
         Setup(Data, Length, Vpid);
         break;
         }
      }
}

bool cMpeg2Fixer::FindHeader(uint32_t Code, const char *Header)
{
  Reset();
  if (Find(Code))
     return true;
  esyslog("ERROR: %s header not found!", Header);
  return false;
}

void cMpeg2Fixer::SetBrokenLink(void)
{
  if (!FindHeader(0x000001B8, "GOP"))
     return;
  SkipBytes(3);
  uchar b = GetByte();
  if (!(b & 0x40)) { // GOP not closed
     b |= 0x20;
     SetByte(b, GetLastIndex());
     }
}

void cMpeg2Fixer::SetClosedGop(void)
{
  if (!FindHeader(0x000001B8, "GOP"))
     return;
  SkipBytes(3);
  uchar b = GetByte();
  b |= 0x40;
  SetByte(b, GetLastIndex());
}

int cMpeg2Fixer::GetTref(void)
{
  if (!FindHeader(0x00000100, "picture"))
     return 0;
  int Tref = GetByte() << 2;
  Tref |= GetByte() >> 6;
  return Tref;
}

void cMpeg2Fixer::AdjGopTime(int Offset, int FramesPerSecond)
{
  if (!FindHeader(0x000001B8, "GOP"))
     return;
  uchar Byte1 = GetByte();
  int Index1 = GetLastIndex();
  uchar Byte2 =  GetByte();
  int Index2 = GetLastIndex();
  uchar Byte3 = GetByte();
  int Index3 = GetLastIndex();
  uchar Byte4 =  GetByte();
  int Index4 = GetLastIndex();
  uchar Frame = ((Byte3 & 0x1F) << 1) | (Byte4 >> 7);
  uchar Sec   = ((Byte2 & 0x07) << 3) | (Byte3 >> 5);
  uchar Min   = ((Byte1 & 0x03) << 4) | (Byte2 >> 4);
  uchar Hour  = ((Byte1 & 0x7C) >> 2);
  int GopTime = ((Hour * 60 + Min) * 60 + Sec) * FramesPerSecond + Frame;
  if (GopTime) { // do not fix when zero
     GopTime += Offset;
     if (GopTime < 0)
        GopTime += 24 * 60 * 60 * FramesPerSecond;
     Frame = GopTime % FramesPerSecond;
     GopTime = GopTime / FramesPerSecond;
     Sec = GopTime % 60;
     GopTime = GopTime / 60;
     Min = GopTime % 60;
     GopTime = GopTime / 60;
     Hour = GopTime % 24;
     SetByte((Byte1 & 0x80) | (Hour << 2) | (Min >> 4), Index1);
     SetByte(((Min & 0x0F) << 4) | 0x08 | (Sec >> 3), Index2);
     SetByte(((Sec & 0x07) << 3) | (Frame >> 1), Index3);
     SetByte((Byte4 & 0x7F) | ((Frame & 0x01) << 7), Index4);
     }
}

void cMpeg2Fixer::AdjTref(int TrefOffset)
{
  if (!FindHeader(0x00000100, "picture"))
     return;
  int Tref = GetByte() << 2;
  int Index1 = GetLastIndex();
  uchar Byte2 = GetByte();
  int Index2 = GetLastIndex();
  Tref |= Byte2 >> 6;
  Tref -= TrefOffset;
  SetByte(Tref >> 2, Index1);
  SetByte((Tref << 6) | (Byte2 & 0x3F), Index2);
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
  int frameErrors;
  time_t lastErrorHandling;
  cString editedRecordingName;
  cRecordingInfo *recordingInfo;
  bool suspensionLogged;
  int sequence;          // cutting sequence
  int delta;             // time between two frames (PTS ticks)
  int64_t lastVidPts;    // the video PTS of the last frame (in display order)
  bool multiFramePkt;    // multiple frames within one PES packet
  int64_t firstPts;      // first valid PTS, for dangling packet stripping
  int64_t offset;        // offset to add to all timestamps
  int tRefOffset;        // number of stripped frames in GOP
  uchar counter[MAXPID]; // the TS continuity counter for each PID
  bool keepPkt[MAXPID];  // flag for each PID to keep packets, for dangling packet stripping
  int numIFrames;        // number of I-frames without pending packets
  cPatPmtParser patPmtParser;
  bool Throttled(void);
  bool SwitchFile(bool Force = false);
  bool LoadFrame(int Index, uchar *Buffer, bool &Independent, int &Length);
  bool FramesAreEqual(int Index1, int Index2);
  void GetPendingPackets(uchar *Buffer, int &Length, int Index);
       // Gather all non-video TS packets from Index upward that either belong to
       // payloads that started before Index, or have a PTS that is before lastVidPts,
       // and add them to the end of the given Data.
  bool FixFrame(uchar *Data, int &Length, bool Independent, int Index, bool CutIn, bool CutOut);
  bool ProcessSequence(int LastEndIndex, int BeginIndex, int EndIndex, int NextBeginIndex);
  void HandleErrors(bool Force = false);
protected:
  virtual void Action(void) override;
public:
  cCuttingThread(const char *FromFileName, const char *ToFileName, cRecordingInfo *RecordingInfo);
  virtual ~cCuttingThread() override;
  const char *Error(void) { return error; }
  };

cCuttingThread::cCuttingThread(const char *FromFileName, const char *ToFileName, cRecordingInfo *RecordingInfo)
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
  frameErrors = 0;
  lastErrorHandling = 0;
  editedRecordingName = ToFileName;
  recordingInfo = RecordingInfo;
  sequence = 0;
  delta = int(round(PTSTICKS / framesPerSecond));
  lastVidPts = -1;
  multiFramePkt = false;
  firstPts = -1;
  offset = 0;
  tRefOffset = 0;
  memset(counter, 0x00, sizeof(counter));
  numIFrames = 0;
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
        if (fromIndex->GetErrors()->Size() > 0) {
           recordingInfo->SetErrors(0); // the fromIndex has error indicators, so we reset the error count
           recordingInfo->Write();
           }
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

class cHeapBuffer {
private:
  uchar *buffer;
public:
  cHeapBuffer(int Size) { buffer = MALLOC(uchar, Size); }
  ~cHeapBuffer() { free(buffer); }
  operator uchar * () { return buffer; }
  };

bool cCuttingThread::FramesAreEqual(int Index1, int Index2)
{
  cHeapBuffer Buffer1(MAXFRAMESIZE);
  cHeapBuffer Buffer2(MAXFRAMESIZE);
  if (!Buffer1 || !Buffer2)
     return false;
  bool Independent;
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

void cCuttingThread::GetPendingPackets(uchar *Data, int &Length, int Index)
{
  cHeapBuffer Buffer(MAXFRAMESIZE);
  if (!Buffer)
     return;
  bool Processed[MAXPID] = { false };
  cPacketStorage PacketStorage;
  int64_t LastPts = lastVidPts + delta;// adding one frame length to fully cover the very last frame
  Processed[patPmtParser.Vpid()] = true; // we only want non-video packets
  for (int NumIndependentFrames = 0; NumIndependentFrames < 2; Index++) {
      bool Independent;
      int len;
      if (LoadFrame(Index, Buffer, Independent, len)) {
         if (Independent)
            NumIndependentFrames++;
         for (uchar *p = Buffer; len >= TS_SIZE && *p == TS_SYNC_BYTE; len -= TS_SIZE, p += TS_SIZE) {
             int Pid = TsPid(p);
             if (Pid != PATPID && !patPmtParser.IsPmtPid(Pid) && !Processed[Pid]) {
                int64_t Pts = TsGetPts(p, TS_SIZE);
                if (Pts >= 0) {
                   int64_t d = PtsDiff(LastPts, Pts);
                   if (d < 0) // Pts is before LastPts
                      PacketStorage.Flush(Pid, Data, Length, MAXFRAMESIZE);
                   else { // Pts is at or after LastPts
                      NumIndependentFrames = 0; // we search until we find two consecutive I-frames without any more pending packets
                      Processed[Pid] = true;
                      }
                   }
                if (!Processed[Pid])
                   PacketStorage.Append(Pid, p, TS_SIZE);
                }
             }
         }
      else
         break;
      }
}

bool cCuttingThread::FixFrame(uchar *Data, int &Length, bool Independent, int Index, bool CutIn, bool CutOut)
{
  if (!patPmtParser.Vpid()) {
     if (!patPmtParser.ParsePatPmt(Data, Length))
        return false;
     }
  if (CutIn) {
     sequence++;
     memset(keepPkt, false, sizeof(keepPkt));
     numIFrames = 0;
     firstPts = TsGetPts(Data, Length);
     // Determine the PTS offset at the beginning of each sequence (except the first one):
     if (sequence != 1) {
        if (firstPts >= 0)
           offset = (lastVidPts + delta - firstPts) & MAX33BIT;
        }
     }
  if (CutOut)
     GetPendingPackets(Data, Length, Index + 1);
  if (Independent) {
     numIFrames++;
     tRefOffset = 0;
     }
  // Fix MPEG-2:
  if (patPmtParser.Vtype() == 2) {
     cMpeg2Fixer Mpeg2fixer(Data, Length, patPmtParser.Vpid());
     if (CutIn) {
        if (sequence == 1 || multiFramePkt)
           Mpeg2fixer.SetBrokenLink();
        else {
           Mpeg2fixer.SetClosedGop();
           tRefOffset = Mpeg2fixer.GetTref();
           }
        }
     if (tRefOffset)
        Mpeg2fixer.AdjTref(tRefOffset);
     if (sequence > 1 && Independent)
        Mpeg2fixer.AdjGopTime((offset - MAX33BIT) / delta, round(framesPerSecond));
     }
  bool DeletedFrame = false;
  bool GotVidPts = false;
  bool StripVideo = sequence > 1 && tRefOffset;
  uchar *p;
  int len;
  for (p = Data, len = Length; len >= TS_SIZE && *p == TS_SYNC_BYTE; p += TS_SIZE, len -= TS_SIZE) {
      int Pid = TsPid(p);
      // Strip dangling packets:
      if (numIFrames < 2 && Pid != PATPID && !patPmtParser.IsPmtPid(Pid)) {
         if (Pid != patPmtParser.Vpid() || StripVideo) {
            int64_t Pts = TsGetPts(p, TS_SIZE);
            if (Pts >= 0)
               keepPkt[Pid] = PtsDiff(firstPts, Pts) >= 0; // Pts is at or after FirstPts
            if (!keepPkt[Pid]) {
               TsHidePayload(p);
               numIFrames = 0; // we search until we find two consecutive I-frames without any more dangling packets
               if (Pid == patPmtParser.Vpid())
                  DeletedFrame = true;
               }
            }
         }
      // Adjust the TS continuity counter:
      if (sequence > 1) {
         if (TsHasPayload(p))
            counter[Pid] = (counter[Pid] + 1) & TS_CONT_CNT_MASK;
         TsSetContinuityCounter(p, counter[Pid]);
         }
      else
         counter[Pid] = TsContinuityCounter(p); // collect initial counters
      // Adjust PTS:
      int64_t Pts = TsGetPts(p, TS_SIZE);
      if (Pts >= 0) {
         if (sequence > 1) {
            Pts = PtsAdd(Pts, offset);
            TsSetPts(p, TS_SIZE, Pts);
            }
         // Keep track of the highest video PTS - in case of multiple fields per frame, take the first one
         if (!GotVidPts && Pid == patPmtParser.Vpid()) {
            GotVidPts = true;
            if (lastVidPts < 0 || PtsDiff(lastVidPts, Pts) > 0)
               lastVidPts = Pts;
            }
         }
      // Adjust DTS:
      if (sequence > 1) {
         int64_t Dts = TsGetDts(p, TS_SIZE);
         if (Dts >= 0) {
            Dts = PtsAdd(Dts, offset);
            if (CutIn)
               Dts = PtsAdd(Dts, tRefOffset * delta);
            TsSetDts(p, TS_SIZE, Dts);
            }
         int64_t Pcr = TsGetPcr(p);
         if (Pcr >= 0) {
            Pcr = Pcr + offset * PCRFACTOR;
            if (Pcr > MAX27MHZ)
               Pcr -= MAX27MHZ + 1;
            TsSetPcr(p, Pcr);
            }
         }
      }
  if (!DeletedFrame && !GotVidPts) {
     // Adjust PTS for multiple frames within a single PES packet:
     lastVidPts = (lastVidPts + delta) & MAX33BIT;
     multiFramePkt = true;
     }
  return DeletedFrame;
}

bool cCuttingThread::ProcessSequence(int LastEndIndex, int BeginIndex, int EndIndex, int NextBeginIndex)
{
  // Check for seamless connections:
  bool SeamlessBegin = LastEndIndex >= 0 && FramesAreEqual(LastEndIndex, BeginIndex);
  bool SeamlessEnd = NextBeginIndex >= 0 && FramesAreEqual(EndIndex, NextBeginIndex);
  // Process all frames from BeginIndex (included) to EndIndex (excluded):
  cHeapBuffer Buffer(MAXFRAMESIZE);
  if (!Buffer) {
     error = "malloc";
     return false;
     }
  cPatPmtParser PatPmtParser;
  cFrameChecker FrameChecker;
  for (int Index = BeginIndex; Running() && Index < EndIndex; Index++) {
      bool Independent;
      int Length;
      if (LoadFrame(Index, Buffer, Independent, Length)) {
         bool Errors = false;
         bool Missing = false;
         if (!isPesRecording) {
            int OldPatVersion, OldPmtVersion;
            PatPmtParser.GetVersions(OldPatVersion, OldPmtVersion);
            if (PatPmtParser.ParsePatPmt(Buffer, Length)) {
               if (OldPatVersion >= 0 && OldPmtVersion >= 0) {
                  int NewPatVersion, NewPmtVersion;
                  if (PatPmtParser.GetVersions(NewPatVersion, NewPmtVersion)) {
                     if (NewPatVersion != OldPatVersion || NewPmtVersion != OldPmtVersion) {
                        dsyslog("PAT/PMT version change while cutting");
                        FrameChecker.Reset();
                        }
                     }
                  }
               }

            FrameChecker.Check(Buffer, Length, Independent, Errors, Missing, Index == EndIndex - 1);
            }
         // Make sure there is enough disk space:
         AssertFreeDiskSpace(-1);
         bool CutIn = !SeamlessBegin && Index == BeginIndex;
         bool CutOut = !SeamlessEnd && Index == EndIndex - 1;
         bool DeletedFrame = false;
         if (!isPesRecording) {
            DeletedFrame = FixFrame(Buffer, Length, Independent, Index, CutIn, CutOut);
            }
         else if (CutIn)
            cRemux::SetBrokenLink(Buffer, Length);
         // Every file shall start with an independent frame:
         if (Independent) {
            if (!SwitchFile())
               return false;
            }
         // Write index:
         if (!DeletedFrame && !toIndex->Write(Independent, toFileName->Number(), fileSize, Errors, Missing)) {
            error = "toIndex";
            return false;
            }
         frameErrors = FrameChecker.TotalErrors();
         HandleErrors();
         // Write data:
         if (toFile->Write(Buffer, Length) < 0) {
            error = "safe_write";
            return false;
            }
         fileSize += Length;
         // Generate marks at the editing points in the edited recording:
         if (numSequences > 1 && Index == BeginIndex) {
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

#define ERROR_HANDLING_DELTA 1 // seconds between handling errors

void cCuttingThread::HandleErrors(bool Force)
{
  if (Force || time(NULL) - lastErrorHandling >= ERROR_HANDLING_DELTA) {
     if (frameErrors > recordingInfo->Errors()) {
        recordingInfo->SetErrors(frameErrors);
        recordingInfo->Write();
        Force = true;
        }
     if (Force) {
        cStateKey StateKey;
        if (cRecordings *Recordings = cRecordings::GetRecordingsWrite(StateKey, 1)) {
           Recordings->UpdateByName(editedRecordingName);
           StateKey.Remove();
           }
        }
     lastErrorHandling = time(NULL);
     }
}

void cCuttingThread::Action(void)
{
  if (cMark *BeginMark = fromMarks.GetNextBegin()) {
     fromFile = fromFileName->Open();
     toFile = toFileName->Open();
     if (!fromFile || !toFile)
        return;
     int LastEndIndex = -1;
     HandleErrors(true); // to make sure an initially reset error count is displayed correctly
     while (BeginMark && Running()) {
           // Suspend cutting if we have severe throughput problems:
           if (Throttled()) {
              cCondWait::SleepMs(100);
              continue;
              }
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
     HandleErrors(true);
     }
  else
     esyslog("no editing marks found!");
}

// --- cCutter ---------------------------------------------------------------

cCutter::cCutter(const char *FileName)
:recordingInfo(FileName)
{
  cuttingThread = NULL;
  error = false;
  originalVersionName = FileName;
}

cCutter::~cCutter()
{
  Stop();
}

cString cCutter::EditedFileName(const char *FileName)
{
  cRecording Recording(FileName);
  cMarks Marks;
  if (Marks.Load(FileName, Recording.FramesPerSecond(), Recording.IsPesRecording())) {
     if (cMark *First = Marks.GetNextBegin())
        Recording.SetStartTime(Recording.Start() + (int(First->Position() / Recording.FramesPerSecond() + 30) / 60) * 60);
     return Recording.PrefixFileName('%');
     }
  return NULL;
}

bool cCutter::Start(void)
{
  if (!cuttingThread) {
     error = false;
     if (*originalVersionName) {
        cRecording Recording(originalVersionName);
        editedVersionName = EditedFileName(originalVersionName);
        if (*editedVersionName) {
           if (strcmp(originalVersionName, editedVersionName) != 0) { // names must be different!
              cRecordingUserCommand::InvokeCommand(RUC_EDITINGRECORDING, editedVersionName, originalVersionName);
              if (cVideoDirectory::RemoveVideoFile(editedVersionName) && MakeDirs(editedVersionName, true)) {
                 recordingInfo.Read();
                 recordingInfo.SetFileName(editedVersionName);
                 recordingInfo.Write();
                 SetRecordingTimerId(editedVersionName, cString::sprintf("%d@%s", 0, Setup.SVDRPHostName));
                 cuttingThread = new cCuttingThread(originalVersionName, editedVersionName, &recordingInfo);
                 return true;
                 }
              }
           }
        }
     }
  return false;
}

void cCutter::Stop(void)
{
  if (!cuttingThread)
     return;
  bool Interrupted = cuttingThread->Active();
  const char *Error = cuttingThread->Error();
  delete cuttingThread;
  cuttingThread = NULL;
  SetRecordingTimerId(editedVersionName, NULL);
  if ((Interrupted || Error) && *editedVersionName) {
     if (Interrupted)
        isyslog("editing process has been interrupted");
     if (Error)
        esyslog("ERROR: '%s' during editing process", Error);
     if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), editedVersionName) == 0)
        cControl::Shutdown();
     }
}

bool cCutter::Active(void)
{
  if (cuttingThread) {
     if (cuttingThread->Active())
        return true;
     error = cuttingThread->Error();
     Stop();
     if (!error)
        cRecordingUserCommand::InvokeCommand(RUC_EDITEDRECORDING, editedVersionName, originalVersionName);
     }
  return false;
}

bool cCutter::Error(void)
{
  return error;
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
              cCutter Cutter(FileName);
              if (Cutter.Start()) {
                 while (Cutter.Active())
                       cCondWait::SleepMs(CUTTINGCHECKINTERVAL);
                 if (!Cutter.Error())
                    return true;
                 fprintf(stderr, "error while cutting\n");
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
