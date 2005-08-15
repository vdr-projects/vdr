/*
 * recorder.h: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.h 1.4 2005/08/13 11:31:18 kls Exp $
 */

#ifndef __RECORDER_H
#define __RECORDER_H

#include "receiver.h"
#include "recording.h"
#include "remux.h"
#include "ringbuffer.h"
#include "thread.h"

class cFileWriter;

class cRecorder : public cReceiver, cThread {
private:
  cRingBufferLinear *ringBuffer;
  cRemux *remux;
  cFileWriter *writer;
protected:
  virtual void Activate(bool On);
  virtual void Receive(uchar *Data, int Length);
  virtual void Action(void);
public:
  cRecorder(const char *FileName, int Ca, int Priority, int VPid, const int *APids, const int *DPids, const int *SPids);
               // Creates a new recorder that requires conditional access Ca, has
               // the given Priority and will record the given PIDs into the file FileName.
  virtual ~cRecorder();
  };

#endif //__RECORDER_H
