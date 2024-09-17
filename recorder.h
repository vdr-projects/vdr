/*
 * recorder.h: The actual DVB recorder
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recorder.h 5.3 2024/09/17 09:39:50 kls Exp $
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
  cFrameDetector *frameDetector;
  cPatPmtGenerator patPmtGenerator;
  cFileName *fileName;
  cRecordingInfo *recordingInfo;
  cIndexFile *index;
  cUnbufferedFile *recordFile;
  char *recordingName;
  bool firstIframeSeen;
  off_t fileSize;
  time_t lastDiskSpaceCheck;
  time_t lastErrorLog;
  int oldErrors;
  int errors;
  int lastErrors;
  bool RunningLowOnDiskSpace(void);
  bool NextFile(void);
  void HandleErrors(bool Force = false);
protected:
  virtual void Activate(bool On);
       ///< If you override Activate() you need to call Detach() (which is a
       ///< member of the cReceiver class) from your own destructor in order
       ///< to properly get a call to Activate(false) when your object is
       ///< destroyed.
  virtual void Receive(const uchar *Data, int Length);
  virtual void Action(void);
public:
  cRecorder(const char *FileName, const cChannel *Channel, int Priority);
       ///< Creates a new recorder for the given Channel and
       ///< the given Priority that will record into the file FileName.
  virtual ~cRecorder();
  int Errors(void) { return oldErrors + errors; };
       ///< Returns the number of errors that were detected during recording.
       ///< Each frame that is missing or contains (any number of) errors counts as one error.
       ///< If this is a resumed recording, this includes errors that occurred
       ///< in the previous parts.
  };

#endif //__RECORDER_H
