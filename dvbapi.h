/*
 * dvbapi.h: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.h 1.56 2001/10/28 15:47:10 kls Exp $
 */

#ifndef __DVBAPI_H
#define __DVBAPI_H

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
#include <ncurses.h>
#endif
#include <stdlib.h> // FIXME: this is apparently necessary for the ost/... header files
                    // FIXME: shouldn't every header file include ALL the other header
                    // FIXME: files it depends on? The sequence in which header files
                    // FIXME: are included here should not matter - and it should NOT
                    // FIXME: be necessary to include <stdlib.h> here!
#include <linux/videodev.h>
#include <ost/dmx.h>
#include <ost/sec.h>
#include <ost/frontend.h>
#include <ost/video.h>
#include <ost/audio.h>
#include <ost/osd.h>
#include <stdio.h>

#include "dvbosd.h"
#ifdef DVDSUPPORT
#include "dvd.h"
#endif //DVDSUPPORT
#include "eit.h"
#include "thread.h"

// Overlay facilities
#define MAXCLIPRECTS 100
typedef struct CRect {
  signed short x, y, width, height;
  };

#define FRAMESPERSEC 25

// The maximum file size is limited by the range that can be covered
// with 'int'. 4GB might be possible (if the range is considered
// 'unsigned'), 2GB should be possible (even if the range is considered
// 'signed'), so let's use 2000MB for absolute safety (the actual file size
// may be slightly higher because we stop recording only before the next
// 'I' frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZE 2000 // MB
#define MINVIDEOFILESIZE  100 // MB

#define MAXVOLUME 255

const char *IndexToHMSF(int Index, bool WithFrame = false);
      // Converts the given index to a string, optionally containing the frame number.
int HMSFToIndex(const char *HMSF);
      // Converts the given string (format: "hh:mm:ss.ff") to an index.

enum eSetChannelResult { scrOk, scrNoTransfer, scrFailed };

class cChannel;

class cRecordBuffer;
class cPlayBuffer;
class cReplayBuffer;
#ifdef DVDSUPPORT
class cDVDplayBuffer;
#endif //DVDSUPPORT
class cTransferBuffer;
class cCuttingBuffer;

class cVideoCutter {
private:
  static char *editedVersionName;
  static cCuttingBuffer *cuttingBuffer;
public:
  static bool Start(const char *FileName);
  static void Stop(void);
  static bool Active(void);
  };

class cDvbApi {
  friend class cRecordBuffer;
  friend class cReplayBuffer;
#ifdef DVDSUPPORT
  friend class cDVDplayBuffer;
#endif //DVDSUPPORT
  friend class cTransferBuffer;
private:
  FrontendType frontendType;
  int videoDev;
  int fd_osd, fd_frontend, fd_sec, fd_dvr, fd_audio, fd_video, fd_demuxa1, fd_demuxa2, fd_demuxd1, fd_demuxd2, fd_demuxv, fd_demuxt;
  int vPid, aPid1, aPid2, dPid1, dPid2;
  bool SetPid(int fd, dmxPesType_t PesType, int Pid, dmxOutput_t Output);
  bool SetVpid(int Vpid, dmxOutput_t Output)  { return SetPid(fd_demuxv,  DMX_PES_VIDEO,    Vpid, Output); }
  bool SetApid1(int Apid, dmxOutput_t Output) { return SetPid(fd_demuxa1, DMX_PES_AUDIO,    Apid, Output); }
  bool SetApid2(int Apid, dmxOutput_t Output) { return SetPid(fd_demuxa2, DMX_PES_OTHER,    Apid, Output); }
  bool SetDpid1(int Dpid, dmxOutput_t Output) { return SetPid(fd_demuxd1, DMX_PES_OTHER,    Dpid, Output); }
  bool SetDpid2(int Dpid, dmxOutput_t Output) { return SetPid(fd_demuxd2, DMX_PES_OTHER,    Dpid, Output); }
  bool SetTpid(int Tpid, dmxOutput_t Output)  { return SetPid(fd_demuxt,  DMX_PES_TELETEXT, Tpid, Output); }
  bool SetPids(bool ForRecording);
  cDvbApi(int n);
public:
  ~cDvbApi();

#define MAXDVBAPI 4
  static int NumDvbApis;
private:
  static cDvbApi *dvbApi[MAXDVBAPI];
  static int useDvbApi;
  int cardIndex;
public:
  static cDvbApi *PrimaryDvbApi;
  static void SetUseDvbApi(int n);
         // Sets the 'useDvbApi' flag of the given DVB device.
         // If this function is not called before Init(), all DVB devices
         // will be used.
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
  int CardIndex(void) { return cardIndex; }
         // Returns the card index of this DvbApi (0 ... MAXDVBAPI - 1).
  static bool Probe(const char *FileName);
         // Probes for existing DVB devices.
  static bool Init(void);
         // Initializes the DVB API.
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
public:
  void Open(int w, int h);
  void Close(void);
  void Clear(void);
  void Fill(int x, int y, int w, int h, eDvbColor color = clrBackground);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  void ClrEol(int x, int y, eDvbColor color = clrBackground);
  int CellWidth(void);
  int LineHeight(void);
  int Width(unsigned char c);
  int WidthInCells(const char *s);
  eDvbFont SetFont(eDvbFont Font);
  void Text(int x, int y, const char *s, eDvbColor colorFg = clrWhite, eDvbColor colorBg = clrBackground);
  void Flush(void);

  // Video format facilities:

  void SetVideoFormat(videoFormat_t Format);

  // Channel facilities

private:
  int currentChannel;
public:
  eSetChannelResult SetChannel(int ChannelNumber, int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid1, int Apid2, int Dpid1, int Dpid2, int Tpid, int Ca, int Pnr);
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
  cPlayBuffer *replayBuffer;
  int ca;
  int priority;
  int  Priority(void) { return priority; }
       // Returns the priority of the current recording session (0..MAXPRIORITY),
       // or -1 if no recording is currently active.
  int  SetModeRecord(void);
       // Initiates recording mode and returns the file handle to read from.
  void SetModeReplay(void);
  void SetModeNormal(bool FromRecording);
public:
  int  Ca(void) { return ca; }
       // Returns the ca of the current recording session (0..MAXDVBAPI).
  int  SecondsToFrames(int Seconds);
       // Returns the number of frames corresponding to the given number of seconds.
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
  bool StartReplay(const char *FileName);
       // Starts replaying the given file.
       // If there is already a replay session active, it will be stopped
       // and the new file will be played back.
#ifdef DVDSUPPORT
  bool StartDVDplay(cDVD *dvd, int TitleID);//XXX dvd parameter necessary???
       // Starts replaying the given TitleID on the DVD.
#endif //DVDSUPPORT
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
  void SkipSeconds(int Seconds);
       // Skips the given number of seconds in the current replay session.
       // The sign of 'Seconds' determines the direction in which to skip.
       // Use a very large negative value to go all the way back to the
       // beginning of the recording.
  int  SkipFrames(int Frames);
       // Returns the new index into the current replay session after skipping
       // the given number of frames (no actual repositioning is done!).
       // The sign of 'Frames' determines the direction in which to skip.
  bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
       // Returns the current and total frame index, optionally snapped to the
       // nearest I-frame.
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
       // Returns the current replay mode (if applicable).
       // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
       // we are going forward or backward and 'Speed' is -1 if this is normal
       // play/pause mode, 0 if it is single speed fast/slow forward/back mode
       // and >0 if this is multi speed mode.
  void Goto(int Index, bool Still = false);
       // Positions to the given index and displays that frame as a still picture
       // if Still is true.

  // Audio track facilities

public:
  bool CanToggleAudioTrack(void);
       // Returns true if we are currently replaying and this recording has two
       // audio tracks, or if the current channel has two audio PIDs.
  bool ToggleAudioTrack(void);
       // Toggles the audio track if possible.

  // Dolby Digital audio facilities

private:
  static char *audioCommand;
public:
  static void SetAudioCommand(const char *Command);
  static const char *AudioCommand(void) { return audioCommand; }

  // Volume facilities:

private:
  bool mute;
  int volume;
public:
  void ToggleMute(void);
       // Turns the volume off or on.
  void SetVolume(int Volume, bool Absolute = false);
       // Sets the volume to the given value, either absolutely or relative to
       // the current volume.
  static int CurrentVolume(void) { return PrimaryDvbApi ? PrimaryDvbApi->volume : 0; }
  };

class cEITScanner {
private:
  enum { ActivityTimeout = 60,
         ScanTimeout = 20
       };
  time_t lastScan, lastActivity;
  int currentChannel, lastChannel;
  int numTransponders, *transponders;
  bool TransponderScanned(cChannel *Channel);
public:
  cEITScanner(void);
  ~cEITScanner();
  bool Active(void) { return currentChannel; }
  void Activity(void);
  void Process(void);
  };

#endif //__DVBAPI_H
