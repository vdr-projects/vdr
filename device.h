/*
 * device.h: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.h 1.1 2002/06/10 16:30:00 kls Exp $
 */

#ifndef __DEVICE_H
#define __DEVICE_H

#include <stdlib.h> // FIXME: this is apparently necessary for the ost/... header files
                    // FIXME: shouldn't every header file include ALL the other header
                    // FIXME: files it depends on? The sequence in which header files
                    // FIXME: are included here should not matter - and it should NOT
                    // FIXME: be necessary to include <stdlib.h> here!
#include <ost/dmx.h>
#include <ost/frontend.h>
#include <ost/audio.h>
#include <ost/video.h>
#include "eit.h"
#include "thread.h"

enum eSetChannelResult { scrOk, scrNoTransfer, scrFailed };

#define MAXDEVICES          4 // the maximum number of devices in the system
#define MAXCACAPS          16 // the maximum number of different CA values per DVB device
#define MAXPIDHANDLES      16 // the maximum number of different PIDs per DVB device
#define MAXRECEIVERS       16 // the maximum number of receivers per DVB device
#define MAXVOLUME         255
#define VOLUMEDELTA         5 // used to increase/decrease the volume

class cPlayer;
class cReceiver;

class cDevice : cThread {
  friend class cOsd;//XXX
private:
  static int numDevices;
  static int useDevice;
  static cDevice *device[MAXDEVICES];
  static cDevice *primaryDevice;
public:
  static int NumDevices(void) { return numDevices; }
         // Returns the total number of DVB devices.
  static void SetUseDevice(int n);
         // Sets the 'useDevice' flag of the given DVB device.
         // If this function is not called before Initialize(), all DVB devices
         // will be used.
  static bool SetPrimaryDevice(int n);
         // Sets the primary DVB device to 'n' (which must be in the range
         // 1...numDevices) and returns true if this was possible.
  static cDevice *PrimaryDevice(void) { return primaryDevice; }
         // Returns the primary DVB device.
  static cDevice *GetDevice(int Ca, int Priority, int Frequency = 0, int Vpid = 0, bool *ReUse = NULL);
         // Selects a free DVB device, avoiding the primaryDevice if possible.
         // If Ca is not 0, the device with the given number will be returned
         // in case Ca is <= MAXDEVICES, or the device that provides the given
         // value in its caCaps.
         // If there is a device that is already tuned to the given Frequency,
         // and that device is able to receive multiple channels ("budget" cards),
         // that device will be returned. Else if a ("full featured") device is
         // tuned to Frequency and Vpid, that one will be returned.
         // If all DVB devices are currently receiving, the one receiving the
         // lowest priority timer (if any) that is lower than the given Priority
         // will be returned.
         // If ReUse is given, the caller will be informed whether the device can be re-used
         // for a new recording. If ReUse returns 'true', the caller must NOT switch the channel
         // (the device is already properly tuned). Otherwise the caller MUST switch the channel.
  static void SetCaCaps(void);
         // Sets the CaCaps of all DVB devices according to the Setup data.
  static bool Probe(const char *FileName);
         // Probes for existing DVB devices.
  static bool Initialize(void);
         // Initializes the DVB devices.
         // Must be called before accessing any DVB functions.
  static void Shutdown(void);
         // Closes down all DVB devices.
         // Must be called at the end of the program.
private:
  int cardIndex;
  int caCaps[MAXCACAPS];
  FrontendType frontendType;
  char *dvrFileName;
  bool active;
  int fd_osd, fd_frontend, fd_sec, fd_audio, fd_video;
  int OsdDeviceHandle(void) { return fd_osd; }
public:
  cDevice(int n);
  virtual ~cDevice();
  bool IsPrimaryDevice(void) { return this == primaryDevice; }
  int CardIndex(void) const { return cardIndex; }
         // Returns the card index of this device (0 ... MAXDEVICES - 1).
  int ProvidesCa(int Ca);
         // Checks whether this DVB device provides the given value in its
         // caCaps. Returns 0 if the value is not provided, 1 if only this
         // value is provided, and > 1 if this and other values are provided.
         // If the given value is equal to the number of this DVB device,
         // 1 is returned. If it is 0 (FTA), 1 plus the number of other values
         // in caCaps is returned.
  bool HasDecoder(void) const { return fd_video >= 0 && fd_audio >= 0; }

// Channel facilities

private:
  int currentChannel;
  int frequency;
public:
  eSetChannelResult SetChannel(int ChannelNumber, int Frequency, char Polarization, int Diseqc, int Srate, int Vpid, int Apid, int Tpid, int Ca, int Pnr);
  static int CurrentChannel(void) { return primaryDevice ? primaryDevice->currentChannel : 0; }
  int Channel(void) { return currentChannel; }

// PID handle facilities

private:
  enum ePidType { ptVideo, ptAudio, ptTeletext, ptDolby, ptOther };
  class cPidHandle {
  public:
    int pid;
    int fd;
    int used;
    cPidHandle(void) { pid = used = 0; fd = -1; }
    };
  cPidHandle pidHandles[MAXPIDHANDLES];
  bool AddPid(int Pid, ePidType PidType = ptOther);
  bool DelPid(int Pid);
  bool SetPid(int fd, dmxPesType_t PesType, int Pid, dmxOutput_t Output);
  virtual void Action(void);

// Image Grab facilities

public:
  bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

// Video format facilities

public:
  virtual void SetVideoFormat(videoFormat_t Format);

// Volume facilities

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
  static int CurrentVolume(void) { return primaryDevice ? primaryDevice->volume : 0; }//XXX???

  // EIT facilities

private:
  cSIProcessor *siProcessor;

// Player facilities

private:
  cPlayer *player;
public:
  void TrickSpeed(int Speed);
  void Clear(void);
  void Play(void);
  void Freeze(void);
  void Mute(void);
  void StillPicture(const uchar *Data, int Length);
  bool Replaying(void);
       // Returns true if we are currently replaying.
  void StopReplay(void);
       // Stops the current replay session (if any).
  bool Attach(cPlayer *Player);
  void Detach(cPlayer *Player);
  virtual int PlayVideo(const uchar *Data, int Length);
  virtual int PlayAudio(const uchar *Data, int Length);

// Receiver facilities

private:
  cReceiver *receiver[MAXRECEIVERS];
  int ca;
  int Priority(void);
      // Returns the priority of the current receiving session (0..MAXPRIORITY),
      // or -1 if no receiver is currently active. The primary DVB device will
      // always return at least Setup.PrimaryLimit-1.
  int CanShift(int Ca, int Priority, int UsedCards = 0);
public:
  int  Ca(void) { return ca; }
       // Returns the ca of the current receiving session.
  bool Receiving(void);
       // Returns true if we are currently receiving.
  bool Attach(cReceiver *Receiver);
  void Detach(cReceiver *Receiver);
  };

#endif //__DEVICE_H
