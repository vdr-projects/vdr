/*
 * recorder.c: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.c 1.4 2002/12/22 11:33:08 kls Exp $
 */

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "recorder.h"

// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(5)

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

cRecorder::cRecorder(const char *FileName, int Ca, int Priority, int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cReceiver(Ca, Priority, 5, VPid, APid1, APid2, DPid1, DPid2)
{
  ringBuffer = NULL;
  remux = NULL;
  fileName = NULL;
  index = NULL;
  pictureType = NO_PICTURE;
  fileSize = 0;
  active = false;
  lastDiskSpaceCheck = time(NULL);
  isyslog("record %s", FileName);

  // Create directories if necessary:

  if (!MakeDirs(FileName, true))
     return;

  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  ringBuffer = new cRingBufferLinear(VIDEOBUFSIZE, true);
  remux = new cRemux(VPid, APid1, APid2, DPid1, DPid2, true);
  fileName = new cFileName(FileName, true);
  recordFile = fileName->Open();
  if (recordFile < 0)
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
  delete remux;
  delete ringBuffer;
}

void cRecorder::Activate(bool On)
{
  if (On) {
     if (recordFile >= 0)
        Start();
     }
  else if (active) {
     active = false;
     Cancel(3);
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
  if (recordFile >= 0 && pictureType == I_FRAME) { // every file shall start with an I_FRAME
     if (fileSize > MEGABYTE(Setup.MaxVideoFileSize) || RunningLowOnDiskSpace()) {
        recordFile = fileName->NextFile();
        fileSize = 0;
        }
     }
  return recordFile >= 0;
}

void cRecorder::Receive(uchar *Data, int Length)
{
  int p = ringBuffer->Put(Data, Length);
  if (p != Length && active)
     esyslog("ERROR: ring buffer overflow (%d bytes dropped)", Length - p);
}

void cRecorder::Action(void)
{
  dsyslog("recording thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  int r = 0;
  active = true;
  while (active) {
        int g = ringBuffer->Get(b + r, sizeof(b) - r);
        if (g > 0)
           r += g;
        if (r > 0) {
           int Count = r, Result;
           uchar *p = remux->Process(b, Count, Result, &pictureType);
           if (p) {
              //XXX+ active??? see old version (Busy)
              if (!active && pictureType == I_FRAME) // finish the recording before the next 'I' frame
                 break;
              if (NextFile()) {
                 if (index && pictureType != NO_PICTURE)
                    index->Write(pictureType, fileName->Number(), fileSize);
                 if (safe_write(recordFile, p, Result) < 0) {
                    LOG_ERROR_STR(fileName->Name());
                    break;
                    }
                 fileSize += Result;
                 }
              else
                 break;
              }
           if (Count > 0) {
              r -= Count;
              memmove(b, b + Count, r);
              }
           }
        else
           usleep(1); // this keeps the CPU load low
        }

  dsyslog("recording thread ended (pid=%d)", getpid());
}
