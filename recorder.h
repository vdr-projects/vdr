/*
 * recorder.h: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.h 1.1 2002/06/08 09:35:03 kls Exp $
 */

#ifndef __RECORDER_H
#define __RECORDER_H

#include "receiver.h"
#include "recording.h"
#include "remux.h"
#include "ringbuffer.h"
#include "thread.h"

class cRecorder : public cReceiver, cThread {
private:
  cRingBufferLinear *ringBuffer;
  cRemux *remux;
  cFileName *fileName;
  cIndexFile *index;
  uchar pictureType;
  int fileSize;
  int recordFile;
  bool active;
  time_t lastDiskSpaceCheck;
  bool RunningLowOnDiskSpace(void);
  bool NextFile(void);
protected:
  virtual void Activate(bool On);
  virtual void Receive(uchar *Data, int Length);
  virtual void Action(void);
public:
  cRecorder(const char *FileName, int Ca, int Priority, int VPid, int APid1, int APid2, int DPid1, int DPid2);
               // Creates a new recorder that requires conditional access Ca, has
               // the given Priority and will record the given PIDs into the file FileName.
  virtual ~cRecorder();
  };

#endif //__RECORDER_H
