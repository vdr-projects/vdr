/*
 * recorder.h: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.h 1.5 2007/01/05 10:44:05 kls Exp $
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
  cRecorder(const char *FileName, tChannelID ChannelID, int Priority, int VPid, const int *APids, const int *DPids, const int *SPids);
               // Creates a new recorder for the channel with the given ChannelID and
               // the given Priority that will record the given PIDs into the file FileName.
  virtual ~cRecorder();
  };

#endif //__RECORDER_H
