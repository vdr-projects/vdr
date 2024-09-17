/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 5.8 2024/09/17 09:39:50 kls Exp $
 */

#include "recorder.h"
#include "shutdown.h"

#define RECORDERBUFSIZE  (MEGABYTE(20) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30000 // milliseconds

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
  oldErrors = max(0, recordingInfo->Errors()); // in case this is a re-started recording
  errors = 0;
  lastErrors = 0;
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
  free(recordingName);
}

#define ERROR_LOG_DELTA 1 // seconds between logging errors

void cRecorder::HandleErrors(bool Force)
{
  // We don't log every single error separately, to avoid spamming the log file:
  if (Force || time(NULL) - lastErrorLog >= ERROR_LOG_DELTA) {
     if (errors > lastErrors) {
        int d = errors - lastErrors;
        esyslog("%s: %d new error%s (total %d)", recordingName, d, d > 1 ? "s" : "", oldErrors + errors);
        recordingInfo->SetErrors(oldErrors + errors);
        recordingInfo->Write();
        LOCK_RECORDINGS_WRITE;
        Recordings->UpdateByName(recordingName);
        lastErrors = errors;
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
                    int PreviousErrors = 0;
                    if (frameDetector->NewFrame(&PreviousErrors)) {
                       if (index)
                          index->Write(frameDetector->IndependentFrame(), fileName->Number(), fileSize);
                       if (PreviousErrors)
                          errors++;
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
           errors += MAXBROKENTIMEOUT / 1000 * frameDetector->FramesPerSecond();
           HandleErrors(true);
           esyslog("ERROR: video data stream broken");
           ShutdownHandler.RequestEmergencyExit();
           t.Set(MAXBROKENTIMEOUT);
           }
        }
  HandleErrors(true);
}
