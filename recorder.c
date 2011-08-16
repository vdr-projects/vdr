/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 2.14 2011/08/13 14:56:36 kls Exp $
 */

#include "recorder.h"
#include "shutdown.h"

#define RECORDERBUFSIZE  (MEGABYTE(5) / TS_SIZE * TS_SIZE) // multiple of TS_SIZE

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

// --- cRecorder -------------------------------------------------------------

cRecorder::cRecorder(const char *FileName, const cChannel *Channel, int Priority)
:cReceiver(Channel, Priority)
,cThread("recording")
{
  recordingName = strdup(FileName);

  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  ringBuffer = new cRingBufferLinear(RECORDERBUFSIZE, TS_SIZE, true, "Recorder");
  ringBuffer->SetTimeouts(0, 100);

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
  if (recordFile) {
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

void cRecorder::Receive(uchar *Data, int Length)
{
  if (Running()) {
     int p = ringBuffer->Put(Data, Length);
     if (p != Length && Running())
        ringBuffer->ReportOverflow(Length - p);
     }
}

void cRecorder::Action(void)
{
  time_t t = time(NULL);
  bool InfoWritten = false;
  bool FirstIframeSeen = false;
#define BUFFERSIZE (5 * TS_SIZE)
  bool Buffering = false;
  int BufferIndex = 0;
  int MaxBufferIndex = 0;
  uchar *Buffer = NULL;
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
                    cRecordingInfo RecordingInfo(recordingName);
                    if (RecordingInfo.Read()) {
                       if (frameDetector->FramesPerSecond() > 0 && DoubleEqual(RecordingInfo.FramesPerSecond(), DEFAULTFRAMESPERSECOND) && !DoubleEqual(RecordingInfo.FramesPerSecond(), frameDetector->FramesPerSecond())) {
                          RecordingInfo.SetFramesPerSecond(frameDetector->FramesPerSecond());
                          RecordingInfo.Write();
                          Recordings.UpdateByName(recordingName);
                          }
                       }
                    InfoWritten = true;
                    }
                 if (frameDetector->NewPayload()) { // We're at the first TS packet of a new payload...
                    if (Buffering)
                       esyslog("ERROR: encountered new payload while buffering - dropping some data!");
                    if (!frameDetector->NewFrame()) { // ...but the frame type is yet unknown, so we need to buffer packets until we see the frame type
                       if (!Buffer) {
                          dsyslog("frame type not in first packet of payload - buffering");
                          if (!(Buffer = MALLOC(uchar, BUFFERSIZE))) {
                             esyslog("ERROR: can't allocate frame type buffer");
                             break;
                             }
                          }
                       BufferIndex = 0;
                       Buffering = true;
                       }
                    }
                 else if (frameDetector->NewFrame()) // now we know the frame type, so stop buffering
                    Buffering = false;
                 if (Buffering) {
                    if (BufferIndex + Count <= BUFFERSIZE) {
                       memcpy(Buffer + BufferIndex, b, Count);
                       BufferIndex += Count;
                       }
                    else
                       esyslog("ERROR: too many bytes for frame type buffer (%d > %d) - dropped %d bytes", BufferIndex + Count, int(BUFFERSIZE), Count);
                    }
                 else if (FirstIframeSeen || frameDetector->IndependentFrame()) {
                    FirstIframeSeen = true; // start recording with the first I-frame
                    if (frameDetector->IndependentFrame() && !NextFile()) // every file shall start with an independent frame
                       break;
                    if (index && frameDetector->NewFrame())
                       index->Write(frameDetector->IndependentFrame(), fileName->Number(), fileSize);
                    if (frameDetector->IndependentFrame()) {
                       recordFile->Write(patPmtGenerator.GetPat(), TS_SIZE);
                       fileSize += TS_SIZE;
                       int Index = 0;
                       while (uchar *pmt = patPmtGenerator.GetPmt(Index)) {
                             recordFile->Write(pmt, TS_SIZE);
                             fileSize += TS_SIZE;
                             }
                       }
                    if (BufferIndex) {
                       recordFile->Write(Buffer, BufferIndex); // if an error occurs here, the next write below will catch and report it
                       if (BufferIndex > MaxBufferIndex)
                          MaxBufferIndex = BufferIndex;
                       BufferIndex = 0;
                       }
                    if (recordFile->Write(b, Count) < 0) {
                       LOG_ERROR_STR(fileName->Name());
                       break;
                       }
                    fileSize += Count;
                    t = time(NULL);
                    }
                 }
              ringBuffer->Del(Count);
              }
           }
        if (time(NULL) - t > MAXBROKENTIMEOUT) {
           esyslog("ERROR: video data stream broken");
           ShutdownHandler.RequestEmergencyExit();
           t = time(NULL);
           }
        }
  if (Buffer) {
     free(Buffer);
     dsyslog("frame type buffer used %d bytes", MaxBufferIndex);
     }
}
