/*
 * dvbapi.h: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.h 1.69 2002/04/21 09:49:22 kls Exp $
 */

#ifndef __DVBAPI_H
#define __DVBAPI_H

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
#include <ncurses.h>
#undef ERR //XXX ncurses defines this - but this clashes with newer system header files
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
#include "eit.h"
#include "thread.h"

#define FRAMESPERSEC 25

// The maximum file size is limited by the range that can be covered
// with 'int'. 4GB might be possible (if the range is considered
// 'unsigned'), 2GB should be possible (even if the range is considered
// 'signed'), so let's use 2000MB for absolute safety (the actual file size
// may be slightly higher because we stop recording only before the next
// 'I' frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZE 2000 // MB
#define MINVIDEOFILESIZE  100 // MB

#define MAXVOLUME         255
#define VOLUMEDELTA         5 // used to increase/decrease the volume

const char *IndexToHMSF(int Index, bool WithFrame = false);
      // Converts the given index to a string, optionally containing the frame number.
int HMSFToIndex(const char *HMSF);
      // Converts the given string (format: "hh:mm:ss.ff") to an index.

enum eSetChannelResult { scrOk, scrNoTransfer, scrFailed };

class cChannel;

class cRecordBuffer;
class cPlayBuffer;
class cReplayBuffer;
class cTransferBuffer;
class cCuttingBuffer;

class cVideoCutter {
private:
  static char *editedVersionName;
  static cCuttingBuffer *cuttingBuffer;
  static bool error;
  static bool ended;
public:
  static bool Start(const char *FileName);
  static void Stop(void);
  static bool Active(void);
  static bool Error(void);
  static bool Ended(void);
  };

class cDvbApi {
  friend class cRecordBuffer;
  friend class cReplayBuffer;
  friend class cTransferBuffer;
private:
  FrontendType frontendType;
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

#define MAXDVBAPI  4 // the maximum number of DVB cards in the system
#define MAXCACAPS 16 // the maximum number of different CA values per DVB card

  static int NumDvbApis;
private:
  static cDvbApi *dvbApi[MAXDVBAPI];
  static int useDvbApi;
  int cardIndex;
  int caCaps[MAXCACAPS];
  int CanShift(int Ca, int Priority, int UsedCards = 0);
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
         // Selects a free DVB device, avoiding the PrimaryDvbApi if possible.
         // If Ca is not 0, the device with the given number will be returned
         // in case Ca is <= MAXDVBAPI, or the device that provides the given
         // value in its caCaps.
         // If all DVB devices are currently recording, the one recording the
         // lowest priority timer (if any) that is lower than the given Priority
         // will be returned.
         // The caller must check whether the returned DVB device is actually
         // recording and stop recording if necessary.
  int CardIndex(void) { return cardIndex; }
         // Returns the card index of this DvbApi (0 ... MAXDVBAPI - 1).
  static void SetCaCaps(void);
         // Sets the CaCaps of all DVB devices according to the Setup data.
  int ProvidesCa(int Ca);
         // Checks whether this DVB device provides the given value in its
         // caCaps. Returns 0 if the value is not provided, 1 if only this
         // value is provided, and > 1 if this and other values are provided.
         // If the given value is equal to the number of this DVB device,
         // 1 is returned. If it is 0 (FTA), 1 plus the number of other values
         // in caCaps is returned.
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
  // Image Grab facilities

  bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

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
  eSetChannelResult SetChannel(int ChannelNumber, int Frequency, char Polarization, int Diseqc, int Srate, int Vpid, int Apid1, int Apid2, int Dpid1, int Dpid2, int Tpid, int Ca, int Pnr);
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
  int  Priority(void);
       // Returns the priority of the current recording session (0..MAXPRIORITY),
       // or -1 if no recording is currently active. The primary DVB device will
       // always return at least Setup.PrimaryLimit-1.
  int  SetModeRecord(void);
       // Initiates recording mode and returns the file handle to read from.
  void SetModeReplay(void);
  void SetModeNormal(bool FromRecording);
public:
  int  Ca(void) { return ca; }
       // Returns the ca of the current recording session.
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
  bool IsMute(void) { return mute; }
  bool ToggleMute(void);
       // Turns the volume off or on and returns the new mute state.
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
