/*
 * recording.h: Recording file handling
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.h 1.1 2000/03/05 15:57:27 kls Exp $
 */

#ifndef __RECORDING_H
#define __RECORDING_H

#include <time.h>
#include "config.h"
#include "dvbapi.h"
#include "tools.h"

extern cDvbRecorder *Recorder;

void AssertFreeDiskSpace(void);

class cRecording : public cListObject {
private:
  bool AssertRecorder(void);
public:
  char *name;
  char *fileName;
  time_t start;
  char quality;
  int priority;
  int lifetime;
  cRecording(const char *Name, time_t Start, char Quality, int Priority, int LifeTime);
  cRecording(cTimer *Timer);
  cRecording(const char *FileName);
  ~cRecording();
  const char *FileName(void);
  bool Delete(void);
       // Changes the file name so that it will no longer be visible in the OSM
       // Returns false in case of error
  bool Remove(void);
       // Actually removes the file from the disk
       // Returns false in case of error
  bool Record(void);
       // Starts recording of the file
  bool Play(void);
       // Starts playback of the file
  void Stop(void);
       // Stops recording or playback of the file
  };

class cRecordings : public cList<cRecording> {
public:
  bool Load(bool Deleted = false);
  };

#endif //__RECORDING_H
