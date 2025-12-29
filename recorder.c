/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 5.13 2025/12/29 14:14:05 kls Exp $
 */

#include "recorder.h"
#include "shutdown.h"

#define RECORDERBUFSIZE  (MEGABYTE(20) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30000 // milliseconds
#define LEFTOVERTIMEOUT   2000 // milliseconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

// --- cRecorder -------------------------------------------------------------

cRecorder::cRecorder(const char *FileName, const cChannel *Channel, int Priority)
:cReceiver(Channel, Priority)
,cThread("recording")
{
  recordingName = strdup(FileName);
  recordingInfo = new cRecordingInfo(recordingName);
  recordingInfo->Read();
  tmpErrors = recordingInfo->TmpErrors();
  oldErrors = max(0, recordingInfo->Errors()) - tmpErrors; // in case this is a re-started recording
  errors = 0;
  lastErrors = oldErrors + tmpErrors;
  working = false;
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
  // Check if this is a resumed recording, in which case we definitely missed frames:
  NextFile();
  if (fileName->Number() > 1 || oldErrors)
     GetLastPts(recordingName);
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
  Cancel(3); // in case the caller didn't call Stop()
  Detach();
  delete index;
  delete fileName;
  delete frameDetector;
  delete ringBuffer;
  free(recordingName);
}

void cRecorder::Stop(void)
{
  Cancel(3);
}

#define ERROR_LOG_DELTA 1 // seconds between logging errors

void cRecorder::HandleErrors(bool Force)
{
  // We don't log every single error separately, to avoid spamming the log file:
  if (Force || time(NULL) - lastErrorLog >= ERROR_LOG_DELTA) {
     int AllErrors = oldErrors + errors + tmpErrors;
     if (AllErrors != lastErrors) {
        int d = AllErrors - lastErrors;
        esyslog("%s: %d new error%s (total %d)", recordingName, d, d > 1 ? "s" : "", AllErrors);
        recordingInfo->SetErrors(AllErrors, tmpErrors);
        recordingInfo->Write();
        LOCK_RECORDINGS_WRITE;
        Recordings->UpdateByName(recordingName);
        lastErrors = AllErrors;
        }
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
  if (working) {
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
     if (p != Length && working)
        ringBuffer->ReportOverflow(Length - p);
     }
}

#define MIN_IFRAMES_FOR_LAST_PTS 2

void cRecorder::GetLastPts(const char *RecordingName)
{
  dsyslog("getting last PTS of '%s'", RecordingName);
  if (cIndexFile *Index = new cIndexFile(RecordingName, false)) {
     cFileName *FileName = new cFileName(RecordingName, false);
     uint16_t FileNumber;
     off_t FileOffset;
     bool Independent;
     uchar Buffer[MAXFRAMESIZE];
     int Length;
     int64_t LastPts = -1;
     int IframesSeen = 0;
     cPatPmtParser PatPmtParser;
     bool GotPatPmtVersions = false;
     for (int i = Index->Last(); i >= 0; i--) {
         if (Index->Get(i, &FileNumber, &FileOffset, &Independent, &Length)) {
            if (cUnbufferedFile *f = FileName->SetOffset(FileNumber, FileOffset)) {
               int l = ReadFrame(f, Buffer, Length, sizeof(Buffer));
               if (l > 0) {
                  int64_t Pts = TsGetPts(Buffer, l);
                  if (LastPts < 0 || PtsDiff(LastPts, Pts) > 0) {
                     LastPts = Pts;
                     IframesSeen = 0;
                     }
                  if (Independent) {
                     if (!GotPatPmtVersions && PatPmtParser.ParsePatPmt(Buffer, l)) {
                        int PatVersion;
                        int PmtVersion;
                        if (PatPmtParser.GetVersions(PatVersion, PmtVersion)) {
                           patPmtGenerator.SetVersions(PatVersion + 1, PmtVersion + 1);
                           GotPatPmtVersions = true;
                           }
                        }
                     if (++IframesSeen >= MIN_IFRAMES_FOR_LAST_PTS)
                        break;
                     }
                  }
               else
                  break;
               }
            else
               break;
            }
         else
            break;
         }
     frameDetector->SetLastPts(LastPts);
     delete FileName;
     delete Index;
     }
}

void cRecorder::Action(void)
{
//#define TEST_VDSB 40000 // 40000 to test VDSB without restart, 70000 for VDSB with restart
#ifdef TEST_VDSB
  cTimeMs VdsbTimer;
#endif
  cTimeMs t(MAXBROKENTIMEOUT);
  bool InfoWritten = false;
  bool pendIndependentFrame = false;
  uint16_t pendNumber = 0;
  off_t pendFileSize = 0;
  bool pendMissing = false;
  int NumIframesSeen = 0;
  working = true;
  while (true) {
#ifdef TEST_VDSB
        int Vdsb = VdsbTimer.Elapsed();
        if (Vdsb > 30000 && Vdsb < TEST_VDSB) {
           working = false;
           cCondWait::SleepMs(100);
           }
        working = true;
#endif
        int r;
        uchar *b = ringBuffer->Get(r);
        if (b) {
           int Count = frameDetector->Analyze(b, r);
           if (Count) {
              if (!Running() && frameDetector->IndependentFrame()) { // finish the recording before the next independent frame
                 working = false;
                 break;
                 }
              if (frameDetector->Synced()) {
                 if (!InfoWritten) {
                    if ((frameDetector->FramesPerSecond() > 0 && DoubleEqual(recordingInfo->FramesPerSecond(), DEFAULTFRAMESPERSECOND) && !DoubleEqual(recordingInfo->FramesPerSecond(), frameDetector->FramesPerSecond())) ||
                        frameDetector->FrameWidth()  != recordingInfo->FrameWidth()  ||
                        frameDetector->FrameHeight() != recordingInfo->FrameHeight() ||
                        frameDetector->AspectRatio() != recordingInfo->AspectRatio()) {
                       recordingInfo->SetFramesPerSecond(frameDetector->FramesPerSecond());
                       recordingInfo->SetFrameParams(frameDetector->FrameWidth(), frameDetector->FrameHeight(), frameDetector->ScanType(), frameDetector->AspectRatio());
                       recordingInfo->Write();
                       LOCK_RECORDINGS_WRITE;
                       Recordings->UpdateByName(recordingName);
                       }
                    InfoWritten = true;
                    cRecordingUserCommand::InvokeCommand(RUC_STARTRECORDING, recordingName);
                    }
                 if (firstIframeSeen || frameDetector->IndependentFrame()) {
                    firstIframeSeen = true; // start recording with the first I-frame
                    if (!NextFile())
                       break;
                    bool PreviousErrors = false;
                    bool MissingFrames = false;
                    if (frameDetector->NewFrame(PreviousErrors, MissingFrames)) {
                       if (index) {
                          if (pendNumber > 0)
                             index->Write(pendIndependentFrame, pendNumber, pendFileSize, PreviousErrors, pendMissing);
                          pendIndependentFrame = frameDetector->IndependentFrame();
                          pendNumber = fileName->Number();
                          pendFileSize = fileSize;
                          pendMissing = MissingFrames;
                          }
                       errors = frameDetector->Errors();
                       }
                    if (frameDetector->IndependentFrame()) {
                       NumIframesSeen++;
                       tmpErrors = 0;
                       recordFile->Write(patPmtGenerator.GetPat(), TS_SIZE);
                       fileSize += TS_SIZE;
                       int Index = 0;
                       while (uchar *pmt = patPmtGenerator.GetPmt(Index)) {
                             recordFile->Write(pmt, TS_SIZE);
                             fileSize += TS_SIZE;
                             }
                       t.Reset();
                       }
                    if (recordFile->Write(b, Count) < 0) {
                       LOG_ERROR_STR(fileName->Name());
                       break;
                       }
                    if (NumIframesSeen >= 2) // avoids extra log entry when resuming a recording
                       HandleErrors();
                    fileSize += Count;
                    }
                 }
              ringBuffer->Del(Count);
              }
           }
        if (t.TimedOut()) {
           esyslog("ERROR: video data stream broken");
           tmpErrors += int(round(frameDetector->FramesPerSecond() * t.Elapsed() / 1000));
           if (pendNumber > 0) {
              bool PreviousErrors = false;
              errors = frameDetector->Errors(&PreviousErrors);
              index->Write(pendIndependentFrame, pendNumber, pendFileSize, PreviousErrors, pendMissing);
              pendNumber = 0;
              }
           HandleErrors(true);
           ShutdownHandler.RequestEmergencyExit();
           t.Reset();
           }
        if (!Running() && ShutdownHandler.EmergencyExitRequested())
           break;
        }
  // Estimate the number of missing frames in case the data stream was broken, but the timer
  // didn't reach the timeout, yet:
  int dt = t.Elapsed();
  if (dt > LEFTOVERTIMEOUT)
     tmpErrors += int(round(frameDetector->FramesPerSecond() * dt / 1000));
  if (pendNumber > 0) {
     bool PreviousErrors = false;
     errors = frameDetector->Errors(&PreviousErrors);
     index->Write(pendIndependentFrame, pendNumber, pendFileSize, PreviousErrors, pendMissing);
     }
  HandleErrors(true);
}
