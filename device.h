/*
 * device.h: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.h 1.12 2002/09/06 14:04:52 kls Exp $
 */

#ifndef __DEVICE_H
#define __DEVICE_H

#include "thread.h"
#include "tools.h"

#define MAXDEVICES         16 // the maximum number of devices in the system
#define MAXCACAPS          16 // the maximum number of different CA values per device
#define MAXPIDHANDLES      16 // the maximum number of different PIDs per device
#define MAXRECEIVERS       16 // the maximum number of receivers per device
#define MAXVOLUME         255
#define VOLUMEDELTA         5 // used to increase/decrease the volume

#define TS_SIZE          188
#define TS_SYNC_BYTE     0x47
#define PID_MASK_HI      0x1F

enum eSetChannelResult { scrOk, scrNotAvailable, scrNoTransfer, scrFailed };

enum ePlayMode { pmNone,       // audio/video from decoder
                 pmAudioVideo, // audio/video from player
                 pmAudioOnly,  // audio only from player, video from decoder
                 pmExtern_THIS_SHOULD_BE_AVOIDED
                 // external player (e.g. MPlayer), release the device
                 // WARNING: USE THIS MODE ONLY AS A LAST RESORT, IF YOU
                 // ABSOLUTELY, POSITIVELY CAN'T IMPLEMENT YOUR PLAYER
                 // THE WAY IT IS SUPPOSED TO WORK. FORCING THE DEVICE
                 // TO RELEASE ITS FILES HANDLES (OR WHATEVER RESOURCES
                 // IT MAY USE) TO ALLOW AN EXTERNAL PLAYER TO ACCESS
                 // THEM MEANS THAT SUCH A PLAYER WILL NEED TO HAVE
                 // DETAILED KNOWLEDGE ABOUT THE INTERNALS OF THE DEVICE
                 // IN USE. AS A CONSEQUENCE, YOUR PLAYER MAY NOT WORK
                 // IF A PARTICULAR VDR INSTALLATION USES A DEVICE NOT
                 // KNOWN TO YOUR PLAYER.
               };

class cOsdBase;
class cChannel;
class cPlayer;
class cReceiver;

class cDevice : cThread {
private:
  static int numDevices;
  static int useDevice;
  static cDevice *device[MAXDEVICES];
  static cDevice *primaryDevice;
public:
  static int NumDevices(void) { return numDevices; }
         // Returns the total number of devices.
  static void SetUseDevice(int n);
         // Sets the 'useDevice' flag of the given device.
         // If this function is not called before initializing, all devices
         // will be used.
  static bool UseDevice(int n) { return useDevice == 0 || (useDevice & (1 << n)) != 0; }
         // Tells whether the device with the given card index shall be used in
         // this instance of VDR.
  static bool SetPrimaryDevice(int n);
         // Sets the primary device to 'n' (which must be in the range
         // 1...numDevices) and returns true if this was possible.
  static cDevice *PrimaryDevice(void) { return primaryDevice; }
         // Returns the primary device.
  static cDevice *GetDevice(int Index);
         // Returns the device with the Index (if Index is in the range
         // 0..numDevices-1, NULL otherwise).
  static cDevice *GetDevice(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL);
         // Returns a device that is able to receive the given Channel at the
         // given Priority (see ProvidesChannel() for more information on how
         // priorities are handled, and the meaning of NeedsDetachReceivers).
  static void SetCaCaps(int Index = -1);
         // Sets the CaCaps of the given device according to the Setup data.
         // By default the CaCaps of all devices are set.
  static void Shutdown(void);
         // Closes down all devices.
         // Must be called at the end of the program.
private:
  static int nextCardIndex;
  int cardIndex;
  int caCaps[MAXCACAPS];
protected:
  cDevice(void);
  virtual ~cDevice();
  static int NextCardIndex(int n = 0);
         // Each device in a given machine must have a unique card index, which
         // will be used to identify the device for assigning Ca parameters and
         // deciding whether to actually use that device in this particular
         // instance of VDR. Every time a new cDevice is created, it will be
         // given the current nextCardIndex, and then nextCardIndex will be
         // automatically incremented by 1. A derived class can determine whether
         // a given device shall be used by checking UseDevice(NextCardIndex()).
         // If a device is skipped, or if there are possible device indexes left
         // after a derived class has set up all its devices, NextCardIndex(n)
         // must be called, where n is the number of card indexes to skip.
  virtual void MakePrimaryDevice(bool On);
         // Informs a device that it will be the primary device. If there is
         // anything the device needs to set up when it becomes the primary
         // device (On = true) or to shut down when it no longer is the primary
         // device (On = false), it should do so in this function.
public:
  bool IsPrimaryDevice(void) const { return this == primaryDevice; }
  int CardIndex(void) const { return cardIndex; }
         // Returns the card index of this device (0 ... MAXDEVICES - 1).
  int ProvidesCa(int Ca);
         // Checks whether this device provides the given value in its
         // caCaps. Returns 0 if the value is not provided, 1 if only this
         // value is provided, and > 1 if this and other values are provided.
         // If the given value is equal to the number of this device,
         // 1 is returned. If it is 0 (FTA), 1 plus the number of other values
         // in caCaps is returned.
  virtual bool HasDecoder(void) const;
         // Tells whether this device has an MPEG decoder.

// OSD facilities

public:
  virtual cOsdBase *NewOsd(int x, int y);
         // Creates a new cOsdBase object that can be used by the cOsd class
         // to display information on the screen, with the upper left corner
         // of the OSD at the given coordinates. If a derived cDevice doesn't
         // implement this function, NULL will be returned by default (which
         // means the device has no OSD capabilities).

// Channel facilities

protected:
  int currentChannel;
public:
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL);
         // Returns true if this device can provide the given channel.
         // In case the device has cReceivers attached to it or it is the primary
         // device, Priority is used to decide whether the caller's request can
         // be honored.
         // The special Priority value -1 will tell the caller whether this device
         // is principally able to provide the given Channel, regardless of any
         // attached cReceivers.
         // If NeedsDetachReceivers is given, the resulting value in it will tell the
         // caller whether or not it will have to detach any currently attached
         // receivers from this device before calling SwitchChannel. Note
         // that the return value in NeedsDetachReceivers is only meaningful if the
         // function itself actually returns true.
         // The default implementation always returns false, so a derived cDevice
         // class that can provide channels must implement this function.
  bool SwitchChannel(const cChannel *Channel, bool LiveView);
         // Switches the device to the given Channel, initiating transfer mode
         // if necessary.
private:
  eSetChannelResult SetChannel(const cChannel *Channel, bool LiveView);
         // Sets the device to the given channel (general setup).
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);
         // Sets the device to the given channel (actual physical setup).
public:
  static int CurrentChannel(void) { return primaryDevice ? primaryDevice->currentChannel : 0; }
         // Returns the number of the current channel on the primary device.
  int Channel(void) { return currentChannel; }
         // Returns the number of the current channel on this device.

// PID handle facilities

private:
  bool active;
  virtual void Action(void);
protected:
  enum ePidType { ptVideo, ptAudio, ptTeletext, ptDolby, ptOther };
  class cPidHandle {
  public:
    int pid;
    int handle;
    int used;
    cPidHandle(void) { pid = used = 0; handle = -1; }
    };
  cPidHandle pidHandles[MAXPIDHANDLES];
  bool HasPid(int Pid);
         // Returns true if this device is currently receiving the given PID.
  bool AddPid(int Pid, ePidType PidType = ptOther);
         // Adds a PID to the set of PIDs this device shall receive.
  void DelPid(int Pid);
         // Deletes a PID from the set of PIDs this device shall receive.
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);
         // Does the actual PID setting on this device.
         // On indicates whether the PID shall be added or deleted.
         // Handle->handle can be used by the device to store information it
         // needs to receive this PID (for instance a file handle).
         // Handle->used indicated how many receivers are using this PID.
         // Type indicates some special types of PIDs, which the device may
         // need to set in a specific way.

// Image Grab facilities

public:
  virtual bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);
         // Grabs the currently visible screen image into the given file, with the
         // given parameters.

// Video format facilities

public:
  virtual void SetVideoFormat(bool VideoFormat16_9);
         // Sets the output video format to either 16:9 or 4:3 (only useful
         // if this device has an MPEG decoder).

// Volume facilities

private:
  bool mute;
  int volume;
protected:
  virtual void SetVolumeDevice(int Volume);
       // Sets the audio volume on this device (Volume = 0...255).
public:
  bool IsMute(void) { return mute; }
  bool ToggleMute(void);
       // Turns the volume off or on and returns the new mute state.
  void SetVolume(int Volume, bool Absolute = false);
       // Sets the volume to the given value, either absolutely or relative to
       // the current volume.
  static int CurrentVolume(void) { return primaryDevice ? primaryDevice->volume : 0; }//XXX???

// Player facilities

private:
  cPlayer *player;
protected:
  virtual bool SetPlayMode(ePlayMode PlayMode);
       // Sets the device into the given play mode.
       // Returns true if the operation was successful.
public:
  virtual void TrickSpeed(int Speed);
       // Sets the device into a mode where replay is done slower.
       // Every single frame shall then be displayed the given number of
       // times.
  virtual void Clear(void);
       // Clears all video and audio data from the device.
  virtual void Play(void);
       // Sets the device into play mode (after a previous trick
       // mode).
  virtual void Freeze(void);
       // Puts the device into "freeze frame" mode.
  virtual void Mute(void);
       // Turns off audio while replaying.
  virtual void StillPicture(const uchar *Data, int Length);
       // Displays the given I-frame as a still picture.
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
       // Returns true if the device itself or any of the file handles in
       // Poller is ready for further action.
       // If TimeoutMs is not zero, the device will wait up to the given number
       // of milleseconds before returning in case there is no immediate
       // need for data.
  virtual int PlayVideo(const uchar *Data, int Length);
       // Actually plays the given data block as video. The data must be
       // part of a PES (Packetized Elementary Stream) which can contain
       // one video and one audio strem.
  virtual int PlayAudio(const uchar *Data, int Length);
       // Plays additional audio streams, like Dolby Digital.
  bool Replaying(void);
       // Returns true if we are currently replaying.
  void StopReplay(void);
       // Stops the current replay session (if any).
  bool AttachPlayer(cPlayer *Player);
       // Attaches the given player to this device.
  void Detach(cPlayer *Player);
       // Detaches the given player from this device.

// Receiver facilities

private:
  cReceiver *receiver[MAXRECEIVERS];
  int ca;
  int CanShift(int Ca, int Priority, int UsedCards = 0);
protected:
  int Priority(void);
      // Returns the priority of the current receiving session (0..MAXPRIORITY),
      // or -1 if no receiver is currently active. The primary device will
      // always return at least Setup.PrimaryLimit-1.
  virtual bool OpenDvr(void);
      // Opens the DVR of this device and prepares it to deliver a Transport
      // Stream for use in a cReceiver.
  virtual void CloseDvr(void);
      // Shuts down the DVR.
  virtual int GetTSPacket(uchar *Data);
      // Gets exactly one TS packet from the DVR of this device and copies it
      // into the given memory area (which is exactly 188 bytes in size).
      // Returns the number of bytes copied into Data (which must be 188).
      // If there is currently no TS packet available, 0 should be returned.
      // In case of a non recoverable error, returns -1.
public:
  int  Ca(void) { return ca; }
       // Returns the ca of the current receiving session.
  bool Receiving(void);
       // Returns true if we are currently receiving.
  bool AttachReceiver(cReceiver *Receiver);
       // Attaches the given receiver to this device.
  void Detach(cReceiver *Receiver);
       // Detaches the given receiver from this device.
  };

#endif //__DEVICE_H
