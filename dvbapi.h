/*
 * dvbapi.h: Interface to the DVB driver
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.h 1.2 2000/03/06 19:47:20 kls Exp $
 */

#ifndef __DVBAPI_H
#define __DVBAPI_H

// FIXME: these should be defined in ../DVB/driver/dvb.h!!!
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#if defined(DEBUG_OSD) || defined(DEBUG_REMOTE)
#include <ncurses.h>
#endif
#include "../DVB/driver/dvb.h"

enum eDvbColor { clrBackground,
#ifndef DEBUG_OSD
                 clrOBSOLETE, //FIXME apparently color '1' can't be used as FgColor with e.g. clrRed as BgColor???
                 clrBlack,
#else
                 clrBlack = clrBackground,
#endif
                 clrRed,
                 clrGreen,
                 clrYellow,
                 clrBlue,
                 clrMagenta,
                 clrCyan,
                 clrWhite,
               };
                  
extern const char *DvbQuality; // Low, Medium, High

bool DvbSetChannel(int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid);

class cDvbRecorder {
private:
  bool recording;
public:
  cDvbRecorder(void);
  ~cDvbRecorder();
  bool Recording(void);
       // Returns true if this recorder is currently recording, false if it
       // is playing back or does nothing.
  bool Record(const char *FileName, char Quality);
       // Starts recording the current channel into the given file, with the
       // given quality level. Any existing file will be overwritten.
       // Returns true if recording was started successfully.
       // If there is already a recording session active, false will be
       // returned.
  bool Play(const char *FileName, int Frame = 0);
       // Starts playback of the given file, at the optional Frame (default
       // is the beginning of the file). If Frame is beyond the last recorded
       // frame in the file (or if it is negative), playback will be positioned
       // to the last frame in the file (or the frame with the absolute value of
       // Frame) and will do an implicit Pause() there.
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

class cDvbOsd {
private:
  enum { charWidth  = 12, // average character width
         lineHeight = 27  // smallest text height
       };
#ifdef DEBUG_OSD
  WINDOW *window;
  enum { MaxColorPairs = 16 };
  int colorPairs[MaxColorPairs];
  void SetColor(eDvbColor colorFg, eDvbColor colorBg = clrBackground);
#else
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
#endif
  int cols, rows;
public:
  cDvbOsd(void);
  ~cDvbOsd();
  void Open(int w, int h);
  void Close(void);
  void Clear(void);
  void Fill(int x, int y, int w, int h, eDvbColor color = clrBackground);
  void ClrEol(int x, int y, eDvbColor color = clrBackground);
  void Text(int x, int y, const char *s, eDvbColor colorFg = clrWhite, eDvbColor colorBg = clrBackground);
  };
  
#endif //__DVBAPI_H
