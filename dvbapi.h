/*
 * dvbapi.h: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.h 1.27 2000/12/03 13:44:28 kls Exp $
 */

#ifndef __DVBAPI_H
#define __DVBAPI_H

// FIXME: these should be defined in ../DVB/driver/dvb.h!!!
typedef unsigned int __u32;
typedef unsigned short __u16;
typedef unsigned char __u8;

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
#include <ncurses.h>
#endif
#include <stdio.h>
#include <dvb.h>
#include "dvbosd.h"
#include "eit.h"

// Overlay facilities
#define MAXCLIPRECTS 100
typedef struct CRect {
  signed short x, y, width, height;
  };

#define MenuLines   15
#define MenuColumns 40

class cResumeFile {
private:
  char *fileName;
public:
  cResumeFile(const char *FileName);
  ~cResumeFile();
  int Read(void);
  bool Save(int Index);
  };

class cRecordBuffer;
class cReplayBuffer;
class cTransferBuffer;

class cDvbApi {
private:
  int videoDev;
  cDvbApi(const char *VideoFileName, const char *VbiFileName);
public:
  ~cDvbApi();

#define MAXDVBAPI 2
  static int NumDvbApis;
private:
  static cDvbApi *dvbApi[MAXDVBAPI];
public:
  static cDvbApi *PrimaryDvbApi;
  static bool SetPrimaryDvbApi(int n);
         // Sets the primary DVB device to 'n' (which must be in the range
         // 1...NumDvbApis) and returns true if this was possible.
  static cDvbApi *GetDvbApi(int Ca, int Priority);
         // Selects a free DVB device, starting with the highest device number
         // (but avoiding, if possible, the PrimaryDvbApi).
         // If Ca is not 0, the device with the given number will be returned.
         // If all DVB devices are currently recording, the one recording the
         // lowest priority timer (if any) that is lower than the given Priority
         // will be returned.
         // The caller must check whether the returned DVB device is actually
         // recording and stop recording if necessary.
  int Index(void);
         // Returns the index of this DvbApi.
  static bool Init(void);
         // Initializes the DVB API and probes for existing DVB devices.
         // Must be called before accessing any DVB functions.
  static void Cleanup(void);
         // Closes down all DVB devices.
         // Must be called at the end of the program.

  // EIT facilities

private:
  cSIProcessor *siProcessor;
public:
  const cSchedules *Schedules(cThreadLock *ThreadLock) const;
         // Caller must provide a cThreadLock which has to survive the entire
         // time the returned cSchedules is accessed. Once the cSchedules is no
         // longer used, the cThreadLock must be destroyed.
  void SetUseTSTime(bool On) { if (siProcessor) siProcessor->SetUseTSTime(On); }

  // Image Grab facilities

  bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

  // Overlay facilities

private:
  bool ovlStat, ovlGeoSet, ovlFbSet;
  int ovlSizeX, ovlSizeY, ovlPosX, ovlPosY, ovlBpp, ovlPalette, ovlClips, ovlClipCount;
  int ovlFbSizeX, ovlFbSizeY;
  __u16 ovlBrightness, ovlColour, ovlHue, ovlContrast;
  struct video_clip ovlClipRects[MAXCLIPRECTS];
public:
  bool OvlF(int SizeX, int SizeY, int FbAddr, int Bpp, int Palette);
  bool OvlG(int SizeX, int SizeY, int PosX, int PosY);
  bool OvlC(int ClipCount, CRect *Cr);
  bool OvlP(__u16 Brightness, __u16 Color, __u16 Hue, __u16 Contrast);
  bool OvlO(bool Value);

  // On Screen Display facilities

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
  cDvbOsd *osd;
#endif
  int cols, rows;
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
public:
  void Open(int w, int h);
  void Close(void);
  void Clear(void);
  void Fill(int x, int y, int w, int h, eDvbColor color = clrBackground);
  void ClrEol(int x, int y, eDvbColor color = clrBackground);
  int CellWidth(void);
  int Width(unsigned char c);
  int WidthInCells(const char *s);
  eDvbFont SetFont(eDvbFont Font);
  void Text(int x, int y, const char *s, eDvbColor colorFg = clrWhite, eDvbColor colorBg = clrBackground);
  void Flush(void);

  // Progress Display facilities

private:
  int lastProgress, lastTotal;
  char *replayTitle;
public:
  bool ShowProgress(bool Initial = false);

  // Channel facilities

private:
  int currentChannel;
public:
  bool SetChannel(int ChannelNumber, int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid, int Ca, int Pnr);
  static int CurrentChannel(void) { return PrimaryDvbApi ? PrimaryDvbApi->currentChannel : 0; }
  int Channel(void) { return currentChannel; }

  // Transfer facilities

private:
  cTransferBuffer *transferBuffer;
  cDvbApi *transferringFromDvbApi;
public:
  bool Transferring(void);
       // Returns true if we are currently transferring video data.
private:
  cDvbApi *StartTransfer(int TransferToVideoDev);
       // Starts transferring video data from this DVB device to TransferToVideoDev.
  void StopTransfer(void);
       // Stops transferring video data (in case a transfer is currently active).

  // Record/Replay facilities

private:
  cRecordBuffer *recordBuffer;
  cReplayBuffer *replayBuffer;
  int ca;
  int priority;
protected:
  int  Ca(void) { return ca; }
       // Returns the ca of the current recording session (0..MAXDVBAPI).
  int  Priority(void) { return priority; }
       // Returns the priority of the current recording session (0..99),
       // or -1 if no recording is currently active.
public:
  bool Recording(void);
       // Returns true if we are currently recording.
  bool Replaying(void);
       // Returns true if we are currently replaying.
  bool StartRecord(const char *FileName, int Ca, int Priority);
       // Starts recording the current channel into the given file, with
       // the given ca and priority.
       // In order to be able to record longer movies,
       // a numerical suffix will be appended to the file name. The inital
       // value of that suffix will be larger than any existing file under
       // the given name, thus allowing an interrupted recording to continue
       // gracefully.
       // Returns true if recording was started successfully.
       // If there is already a recording session active, false will be
       // returned.
  void StopRecord(void);
       // Stops the current recording session (if any).
  bool StartReplay(const char *FileName, const char *Title = NULL);
       // Starts replaying the given file.
       // If there is already a replay session active, it will be stopped
       // and the new file will be played back.
       // If provided Title will be used in the progress display.
  void StopReplay(void);
       // Stops the current replay session (if any).
  void Pause(void);
       // Pauses the current replay session, or resumes a paused session.
  void Play(void);
       // Resumes normal replay mode.
  void Forward(void);
       // Runs the current replay session forward at a higher speed.
  void Backward(void);
       // Runs the current replay session backwards at a higher speed.
  void Skip(int Seconds);
       // Skips the given number of seconds in the current replay session.
       // The sign of 'Seconds' determines the direction in which to skip.
       // Use a very large negative value to go all the way back to the
       // beginning of the recording.
  bool GetIndex(int &Current, int &Total);
  };

class cEITScanner {
private:
  enum { ActivityTimeout = 60,
         ScanTimeout = 20
       };
  time_t lastScan, lastActivity;
  int currentChannel, lastChannel;
public:
  cEITScanner(void);
  bool Active(void) { return currentChannel; }
  void Activity(void);
  void Process(void);
  };

#endif //__DVBAPI_H
