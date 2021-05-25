/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 5.3 2021/05/25 20:14:06 kls Exp $
 */

#include "recorder.h"
#include "shutdown.h"

#define RECORDERBUFSIZE  (MEGABYTE(20) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30000 // milliseconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

static bool DebugChecks = false;

// cTsChecker and cFrameChecker are used to detect errors in the recorded data stream.
// While cTsChecker checks the continuity counter of the incoming TS packets, cFrameChecker
// works on entire frames, checking their PTS (Presentation Time Stamps) to see whether
// all expected frames arrive. The resulting number of errors is not a precise value.
// If it is zero, the recording can be safely considered error free. The higher the value,
// the more damaged the recording is.

// --- cTsChecker ------------------------------------------------------------

#define TS_CC_UNKNOWN    0xFF

class cTsChecker {
private:
  uchar counter[MAXPID];
  int errors;
  void Report(int Pid, const char *Message);
public:
  cTsChecker(void);
  void CheckTs(const uchar *Data, int Length);
  int Errors(void) { return errors; }
  };

cTsChecker::cTsChecker(void)
{
  memset(counter, TS_CC_UNKNOWN, sizeof(counter));
  errors = 0;
}

void cTsChecker::Report(int Pid, const char *Message)
{
  errors++;
  if (DebugChecks)
     fprintf(stderr, "%s: TS error #%d on PID %d (%s)\n", *TimeToString(time(NULL)), errors, Pid, Message);
}

void cTsChecker::CheckTs(const uchar *Data, int Length)
{
  int Pid = TsPid(Data);
  uchar Cc = TsContinuityCounter(Data);
  if (TsHasPayload(Data)) {
     if (TsError(Data))
        Report(Pid, "tei");
     else if (TsIsScrambled(Data))
        Report(Pid, "scrambled");
     else {
        uchar OldCc = counter[Pid];
        if (OldCc != TS_CC_UNKNOWN) {
           uchar NewCc = (OldCc + 1) & TS_CONT_CNT_MASK;
           if (Cc != NewCc)
              Report(Pid, "continuity");
           }
        }
     }
  counter[Pid] = Cc;
}

// --- cFrameChecker ---------------------------------------------------------

#define MAX_BACK_REFS  32

class cFrameChecker {
private:
  int frameDelta;
  int64_t lastPts;
  uint32_t backRefs;
  int lastFwdRef;
  int errors;
  void Report(const char *Message, int NumErrors = 1);
public:
  cFrameChecker(void);
  void SetFrameDelta(int FrameDelta) { frameDelta = FrameDelta; }
  void CheckFrame(const uchar *Data, int Length);
  void ReportBroken(void);
  int Errors(void) { return errors; }
  };

cFrameChecker::cFrameChecker(void)
{
  frameDelta = PTSTICKS / DEFAULTFRAMESPERSECOND;
  lastPts = -1;
  backRefs = 0;
  lastFwdRef = 0;
  errors = 0;
}

void cFrameChecker::Report(const char *Message, int NumErrors)
{
  errors += NumErrors;
  if (DebugChecks)
     fprintf(stderr, "%s: frame error #%d (%s)\n", *TimeToString(time(NULL)), errors, Message);
}

void cFrameChecker::CheckFrame(const uchar *Data, int Length)
{
  int64_t Pts = TsGetPts(Data, Length);
  if (Pts >= 0) {
     if (lastPts >= 0) {
        int Diff = int(round((PtsDiff(lastPts, Pts) / double(frameDelta))));
        if (Diff > 0) {
           if (Diff <= MAX_BACK_REFS) {
              if (lastFwdRef > 1) {
                 if (backRefs != uint32_t((1 << (lastFwdRef - 1)) - 1))
                    Report("missing backref");
                 }
              }
           else
              Report("missed", Diff);
           backRefs = 0;
           lastFwdRef = Diff;
           lastPts = Pts;
           }
        else if (Diff < 0) {
           Diff = -Diff;
           if (Diff <= MAX_BACK_REFS) {
              int b = 1 << (Diff - 1);
              if ((backRefs & b) != 0)
                 Report("duplicate backref");
              backRefs |= b;
              }
           else
              Report("rev diff too big");
           }
        else
           Report("zero diff");
        }
     else
        lastPts = Pts;
     }
  else
     Report("no PTS");
}

void cFrameChecker::ReportBroken(void)
{
  int MissedFrames = MAXBROKENTIMEOUT / 1000 * PTSTICKS / frameDelta;
  Report("missed", MissedFrames);
}

// --- cRecorder -------------------------------------------------------------

cRecorder::cRecorder(const char *FileName, const cChannel *Channel, int Priority)
:cReceiver(Channel, Priority)
,cThread("recording")
{
  tsChecker = new cTsChecker;
  frameChecker = new cFrameChecker;
  recordingName = strdup(FileName);
  recordingInfo = new cRecordingInfo(recordingName);
  recordingInfo->Read();
  oldErrors = max(0, recordingInfo->Errors()); // in case this is a re-started recording
  errors = oldErrors;
  lastErrors = errors;
  firstIframeSeen = false;

  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  ringBuffer = new cRingBufferLinear(RECORDERBUFSIZE, MIN_TS_PACKETS_FOR_FRAME_DETECTOR * TS_SIZE, true, "Recorder");
  ringBuffer->SetTimeouts(0, 100);
  ringBuffer->SetIoThrottle();

  int Pid = Channel->Vpid();
  int Type = Channel->Vtype();
  if (!Pid && Channel->Apid(0)) {
     Pid = Channel->Apid(0);
     Type = 0x04;
     }
  if (!Pid && Channel->Dpid(0)) {
     Pid = Channel->Dpid(0);
     Type = 0x06;
     }
  frameDetector = new cFrameDetector(Pid, Type);
  index = NULL;
  fileSize = 0;
  lastDiskSpaceCheck = time(NULL);
  lastErrorLog = 0;
  fileName = new cFileName(FileName, true);
  int PatVersion, PmtVersion;
  if (fileName->GetLastPatPmtVersions(PatVersion, PmtVersion))
     patPmtGenerator.SetVersions(PatVersion + 1, PmtVersion + 1);
  patPmtGenerator.SetChannel(Channel);
  recordFile = fileName->Open();
  if (!recordFile)
     return;
  // Create the index file:
  index = new cIndexFile(FileName, true);
  if (!index)
     esyslog("ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
}

cRecorder::~cRecorder()
{
  Detach();
  delete index;
  delete fileName;
  delete frameDetector;
  delete ringBuffer;
  delete frameChecker;
  delete tsChecker;
  free(recordingName);
}

#define ERROR_LOG_DELTA 1 // seconds between logging errors

void cRecorder::HandleErrors(bool Force)
{
  // We don't log every single error separately, to avoid spamming the log file:
  if (Force || time(NULL) - lastErrorLog >= ERROR_LOG_DELTA) {
     errors = tsChecker->Errors() + frameChecker->Errors();
     if (errors > lastErrors) {
        int d = errors - lastErrors;
        if (DebugChecks)
           fprintf(stderr, "%s: %s: %d error%s\n", *TimeToString(time(NULL)), recordingName, d, d > 1 ? "s" : "");
        esyslog("%s: %d error%s", recordingName, d, d > 1 ? "s" : "");
        recordingInfo->SetErrors(oldErrors + errors);
        recordingInfo->Write();
        }
     lastErrors = errors;
     lastErrorLog = time(NULL);
     }
}

bool cRecorder::RunningLowOnDiskSpace(void)
{
  if (time(NULL) > lastDiskSpaceCheck + DISKCHECKINTERVAL) {
     int Free = FreeDiskSpaceMB(fileName->Name());
     lastDiskSpaceCheck = time(NULL);
     if (Free < MINFREEDISKSPACE) {
        dsyslog("low disk space (%d MB, limit is %d MB)", Free, MINFREEDISKSPACE);
        return true;
        }
     }
  return false;
}

bool cRecorder::NextFile(void)
{
  if (recordFile && frameDetector->IndependentFrame()) { // every file shall start with an independent frame
     if (fileSize > MEGABYTE(off_t(Setup.MaxVideoFileSize)) || RunningLowOnDiskSpace()) {
        recordFile = fileName->NextFile();
        fileSize = 0;
        }
     }
  return recordFile != NULL;
}

void cRecorder::Activate(bool On)
{
  if (On)
     Start();
  else
     Cancel(3);
}

void cRecorder::Receive(const uchar *Data, int Length)
{
  if (Running()) {
     static const uchar aff[TS_SIZE - 4] = { 0xB7, 0x00,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF}; // Length is always TS_SIZE!
     if ((Data[3] & 0b00110000) == 0b00100000 && !memcmp(Data + 4, aff, sizeof(aff)))
        return; // Adaptation Field Filler found, skipping
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && Running())
        ringBuffer->ReportOverflow(Length - p);
     else if (firstIframeSeen) // we ignore any garbage before the first I-frame
        tsChecker->CheckTs(Data, Length);
     }
}

void cRecorder::Action(void)
{
  cTimeMs t(MAXBROKENTIMEOUT);
  bool InfoWritten = false;
  while (Running()) {
        int r;
        uchar *b = ringBuffer->Get(r);
        if (b) {
           int Count = frameDetector->Analyze(b, r);
           if (Count) {
              if (!Running() && frameDetector->IndependentFrame()) // finish the recording before the next independent frame
                 break;
              if (frameDetector->Synced()) {
                 if (!InfoWritten) {
                    if (frameDetector->FramesPerSecond() > 0 && DoubleEqual(recordingInfo->FramesPerSecond(), DEFAULTFRAMESPERSECOND) && !DoubleEqual(recordingInfo->FramesPerSecond(), frameDetector->FramesPerSecond())) {
                       recordingInfo->SetFramesPerSecond(frameDetector->FramesPerSecond());
                       recordingInfo->Write();
                       LOCK_RECORDINGS_WRITE;
                       Recordings->UpdateByName(recordingName);
                       }
                    InfoWritten = true;
                    cRecordingUserCommand::InvokeCommand(RUC_STARTRECORDING, recordingName);
                    frameChecker->SetFrameDelta(PTSTICKS / frameDetector->FramesPerSecond());
                    }
                 if (firstIframeSeen || frameDetector->IndependentFrame()) {
                    firstIframeSeen = true; // start recording with the first I-frame
                    if (!NextFile())
                       break;
                    if (frameDetector->NewFrame()) {
                       if (index)
                          index->Write(frameDetector->IndependentFrame(), fileName->Number(), fileSize);
                       if (frameChecker)
                          frameChecker->CheckFrame(b, Count);
                       }
                    if (frameDetector->IndependentFrame()) {
                       recordFile->Write(patPmtGenerator.GetPat(), TS_SIZE);
                       fileSize += TS_SIZE;
                       int Index = 0;
                       while (uchar *pmt = patPmtGenerator.GetPmt(Index)) {
                             recordFile->Write(pmt, TS_SIZE);
                             fileSize += TS_SIZE;
                             }
                       t.Set(MAXBROKENTIMEOUT);
                       }
                    if (recordFile->Write(b, Count) < 0) {
                       LOG_ERROR_STR(fileName->Name());
                       break;
                       }
                    HandleErrors();
                    fileSize += Count;
                    }
                 }
              ringBuffer->Del(Count);
              }
           }
        if (t.TimedOut()) {
           frameChecker->ReportBroken();
           HandleErrors(true);
           esyslog("ERROR: video data stream broken");
           ShutdownHandler.RequestEmergencyExit();
           t.Set(MAXBROKENTIMEOUT);
           }
        }
  HandleErrors(true);
}
