/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 1.17 2006/01/08 11:01:25 kls Exp $
 */

#include "recorder.h"
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define RECORDERBUFSIZE  MEGABYTE(5)

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

class cFileWriter : public cThread {
private:
  cRemux *remux;
  cFileName *fileName;
  cIndexFile *index;
  uchar pictureType;
  int fileSize;
  cUnbufferedFile *recordFile;
  time_t lastDiskSpaceCheck;
  bool RunningLowOnDiskSpace(void);
  bool NextFile(void);
protected:
  virtual void Action(void);
public:
  cFileWriter(const char *FileName, cRemux *Remux);
  virtual ~cFileWriter();
  };

cFileWriter::cFileWriter(const char *FileName, cRemux *Remux)
:cThread("file writer")
{
  fileName = NULL;
  remux = Remux;
  index = NULL;
  pictureType = NO_PICTURE;
  fileSize = 0;
  lastDiskSpaceCheck = time(NULL);
  fileName = new cFileName(FileName, true);
  recordFile = fileName->Open();
  if (!recordFile)
     return;
  // Create the index file:
  index = new cIndexFile(FileName, true);
  if (!index)
     esyslog("ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
}

cFileWriter::~cFileWriter()
{
  Cancel(3);
  delete index;
  delete fileName;
}

bool cFileWriter::RunningLowOnDiskSpace(void)
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

bool cFileWriter::NextFile(void)
{
  if (recordFile && pictureType == I_FRAME) { // every file shall start with an I_FRAME
     if (fileSize > MEGABYTE(Setup.MaxVideoFileSize) || RunningLowOnDiskSpace()) {
        recordFile = fileName->NextFile();
        fileSize = 0;
        }
     }
  return recordFile != NULL;
}

void cFileWriter::Action(void)
{
  time_t t = time(NULL);
  while (Running()) {
        int Count;
        uchar *p = remux->Get(Count, &pictureType);
        if (p) {
           if (!Running() && pictureType == I_FRAME) // finish the recording before the next 'I' frame
              break;
           if (NextFile()) {
              if (index && pictureType != NO_PICTURE)
                 index->Write(pictureType, fileName->Number(), fileSize);
              if (recordFile->Write(p, Count) < 0) {
                 LOG_ERROR_STR(fileName->Name());
                 break;
                 }
              fileSize += Count;
              remux->Del(Count);
              }
           else
              break;
           t = time(NULL);
           }
        else if (time(NULL) - t > MAXBROKENTIMEOUT) {
           esyslog("ERROR: video data stream broken");
           cThread::EmergencyExit(true);
           t = time(NULL);
           }
        }
}

cRecorder::cRecorder(const char *FileName, int Ca, int Priority, int VPid, const int *APids, const int *DPids, const int *SPids)
:cReceiver(Ca, Priority, VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids)
,cThread("recording")
{
  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  ringBuffer = new cRingBufferLinear(RECORDERBUFSIZE, TS_SIZE * 2, true, "Recorder");
  ringBuffer->SetTimeouts(0, 100);
  remux = new cRemux(VPid, APids, Setup.UseDolbyDigital ? DPids : NULL, SPids, true);
  writer = new cFileWriter(FileName, remux);
}

cRecorder::~cRecorder()
{
  Detach();
  delete writer;
  delete remux;
  delete ringBuffer;
}

void cRecorder::Activate(bool On)
{
  if (On) {
     writer->Start();
     Start();
     }
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
  while (Running()) {
        int r;
        uchar *b = ringBuffer->Get(r);
        if (b) {
           int Count = remux->Put(b, r);
           if (Count)
              ringBuffer->Del(Count);
           else
              cCondWait::SleepMs(100); // avoid busy loop when resultBuffer is full in cRemux::Put()
           }
        }
}
