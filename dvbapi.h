/*
 * dvbapi.h: Interface to the DVB driver
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.h 1.1 2000/02/19 13:36:48 kls Exp $
 */

#ifndef __DVBAPI_H
#define __DVBAPI_H

const int DvbOsdCharWidth  = 12; //XXX
const int DvbOsdLineHeight = 25;

extern const char *DvbQuality; // Low, Medium, High

bool DvbSetChannel(int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid);

class cDvbRecorder {
public:
  cDvbRecorder(void);
  ~cDvbRecorder();
  bool Record(const char *FileName, char Quality);
       // Starts recording the current channel into the given file, with the
       // given quality level. Any existing file will be overwritten.
       // Returns true if recording was started successfully.
       // If there is already a recording session active, false will be
       // returned.
  bool Play(const char *FileName, int Frame = 0);
       // Starts playback of the given file, at the optional Frame (default
       // is the beginning of the file). If Frame is beyond the last recorded
       // frame in the file, or if it is negative, playback will be positioned
       // to the last frame in the file and will do an implicit Pause() there.
       // If there is already a playback session active, it will be stopped
       // and the new file or frame (which may be in the same file) will
       // be played back.
  bool FastForward(void);
       // Runs the current playback session forward at a higher speed.
       // TODO allow different fast forward speeds???
  bool FastRewind(void);
       // Runs the current playback session backwards forward at a higher speed.
       // TODO allow different fast rewind speeds???
  bool Pause(void);
       // Pauses the current recording or playback session, or resumes a paused
       // session.
       // Returns true if there is actually a recording or playback session
       // active that was paused/resumed.
  void Stop(void);
       // Stops the current recording or playback session.
  int  Frame(void);
       // Returns the number of the current frame in the current recording or
       // playback session, which can be used to start playback at a given position.
       // The number returned is the actual number of frames counted from the
       // beginning of the current file.
       // The very first frame has the number 1.
  };

void DvbOsdOpen(int x, int y, int w, int h);
void DvbOsdClose(void);
void DvbOsdClear(void);
void DvbOsdClrEol(int x, int y);
void DvbOsdText(int x, int y, char *s);

#endif //__DVBAPI_H
