/*
 * device.h: The basic device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: device.h 1.35 2003/11/07 13:15:45 kls Exp $
 */

#ifndef __DEVICE_H
#define __DEVICE_H

#include "ci.h"
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

enum ePlayMode { pmNone,           // audio/video from decoder
                 pmAudioVideo,     // audio/video from player
                 pmAudioOnly,      // audio only from player, video from decoder
                 pmAudioOnlyBlack, // audio only from player, no video (black screen)
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

enum eVideoSystem { vsPAL,
                    vsNTSC
                  };

class cOsdBase;
class cChannel;
class cPlayer;
class cReceiver;
class cSpuDecoder;

/// The cDevice class is the base from which actual devices can be derived.

class cDevice : public cThread {
private:
  static int numDevices;
  static int useDevice;
  static cDevice *device[MAXDEVICES];
  static cDevice *primaryDevice;
public:
  static int NumDevices(void) { return numDevices; }
         ///< Returns the total number of devices.
  static void SetUseDevice(int n);
         ///< Sets the 'useDevice' flag of the given device.
         ///< If this function is not called before initializing, all devices
         ///< will be used.
  static bool UseDevice(int n) { return useDevice == 0 || (useDevice & (1 << n)) != 0; }
         ///< Tells whether the device with the given card index shall be used in
         ///< this instance of VDR.
  static bool SetPrimaryDevice(int n);
         ///< Sets the primary device to 'n'.
         ///< \param n must be in the range 1...numDevices.
         ///< \return true if this was possible.
  static cDevice *PrimaryDevice(void) { return primaryDevice; }
         ///< Returns the primary device.
  static cDevice *ActualDevice(void);
         ///< Returns the actual receiving device in case of Transfer Mode, or the
         ///< primary device otherwise.
  static cDevice *GetDevice(int Index);
         ///< Gets the device with the given Index.
         ///< \param Index must be in the range 0..numDevices-1.
         ///< \return A pointer to the device, or NULL if the Index was invalid.
  static cDevice *GetDevice(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL);
         ///< Returns a device that is able to receive the given Channel at the
         ///< given Priority.
         ///< See ProvidesChannel() for more information on how
         ///< priorities are handled, and the meaning of NeedsDetachReceivers.
  static void SetCaCaps(int Index = -1);
         ///< Sets the CaCaps of the given device according to the Setup data.
         ///< By default the CaCaps of all devices are set.
  static void Shutdown(void);
         ///< Closes down all devices.
         ///< Must be called at the end of the program.
private:
  static int nextCardIndex;
  int cardIndex;
  int caCaps[MAXCACAPS];
protected:
  cDevice(void);
  virtual ~cDevice();
  static int NextCardIndex(int n = 0);
         ///< Calculates the next card index.
         ///< Each device in a given machine must have a unique card index, which
         ///< will be used to identify the device for assigning Ca parameters and
         ///< deciding whether to actually use that device in this particular
         ///< instance of VDR. Every time a new cDevice is created, it will be
         ///< given the current nextCardIndex, and then nextCardIndex will be
         ///< automatically incremented by 1. A derived class can determine whether
         ///< a given device shall be used by checking UseDevice(NextCardIndex()).
         ///< If a device is skipped, or if there are possible device indexes left
         ///< after a derived class has set up all its devices, NextCardIndex(n)
         ///< must be called, where n is the number of card indexes to skip.
  virtual void MakePrimaryDevice(bool On);
         ///< Informs a device that it will be the primary device. If there is
         ///< anything the device needs to set up when it becomes the primary
         ///< device (On = true) or to shut down when it no longer is the primary
         ///< device (On = false), it should do so in this function.
public:
  bool IsPrimaryDevice(void) const { return this == primaryDevice; }
  int CardIndex(void) const { return cardIndex; }
         ///< Returns the card index of this device (0 ... MAXDEVICES - 1).
  int DeviceNumber(void) const;
         ///< Returns the number of this device (0 ... MAXDEVICES - 1).
  int ProvidesCa(int Ca) const;
         ///< Checks whether this device provides the given value in its
         ///< caCaps. Returns 0 if the value is not provided, 1 if only this
         ///< value is provided, and > 1 if this and other values are provided.
         ///< If the given value is equal to the number of this device,
         ///< 1 is returned. If it is 0 (FTA), 1 plus the number of other values
         ///< in caCaps is returned.
  virtual bool HasDecoder(void) const;
         ///< Tells whether this device has an MPEG decoder.

// OSD facilities

public:
  virtual cOsdBase *NewOsd(int x, int y);
         ///< Creates a new cOsdBase object that can be used by the cOsd class
         ///< to display information on the screen, with the upper left corner
         ///< of the OSD at the given coordinates. If a derived cDevice doesn't
         ///< implement this function, NULL will be returned by default (which
         ///< means the device has no OSD capabilities).
  virtual cSpuDecoder *GetSpuDecoder(void);
         ///< Returns a pointer to the device's SPU decoder (or NULL, if this
         ///< device doesn't have an SPU decoder).

// Channel facilities

protected:
  static int currentChannel;
public:
  virtual bool ProvidesSource(int Source) const;
         ///< Returns true if this device can provide the given source.
  virtual bool ProvidesChannel(const cChannel *Channel, int Priority = -1, bool *NeedsDetachReceivers = NULL) const;
         ///< Returns true if this device can provide the given channel.
         ///< In case the device has cReceivers attached to it or it is the primary
         ///< device, Priority is used to decide whether the caller's request can
         ///< be honored.
         ///< The special Priority value -1 will tell the caller whether this device
         ///< is principally able to provide the given Channel, regardless of any
         ///< attached cReceivers.
         ///< If NeedsDetachReceivers is given, the resulting value in it will tell the
         ///< caller whether or not it will have to detach any currently attached
         ///< receivers from this device before calling SwitchChannel. Note
         ///< that the return value in NeedsDetachReceivers is only meaningful if the
         ///< function itself actually returns true.
         ///< The default implementation always returns false, so a derived cDevice
         ///< class that can provide channels must implement this function.
  bool SwitchChannel(const cChannel *Channel, bool LiveView);
         ///< Switches the device to the given Channel, initiating transfer mode
         ///< if necessary.
  static bool SwitchChannel(int Direction);
         ///< Switches the primary device to the next available channel in the given
         ///< Direction (only the sign of Direction is evaluated, positive values
         ///< switch to higher channel numbers).
private:
  eSetChannelResult SetChannel(const cChannel *Channel, bool LiveView);
         ///< Sets the device to the given channel (general setup).
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);
         ///< Sets the device to the given channel (actual physical setup).
public:
  static int CurrentChannel(void) { return primaryDevice ? currentChannel : 0; }
         ///< Returns the number of the current channel on the primary device.
  virtual bool HasProgramme(void);
         ///< Returns true if the device is currently showing any programme to
         ///< the user, either through replaying or live.

// PID handle facilities

private:
  bool active;
  virtual void Action(void);
protected:
  enum ePidType { ptAudio, ptVideo, ptPcr, ptTeletext, ptDolby, ptOther };
  class cPidHandle {
  public:
    int pid;
    int handle;
    int used;
    cPidHandle(void) { pid = used = 0; handle = -1; }
    };
  cPidHandle pidHandles[MAXPIDHANDLES];
  bool HasPid(int Pid) const;
         ///< Returns true if this device is currently receiving the given PID.
  bool AddPid(int Pid, ePidType PidType = ptOther);
         ///< Adds a PID to the set of PIDs this device shall receive.
  void DelPid(int Pid, ePidType PidType = ptOther);
         ///< Deletes a PID from the set of PIDs this device shall receive.
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);
         ///< Does the actual PID setting on this device.
         ///< On indicates whether the PID shall be added or deleted.
         ///< Handle->handle can be used by the device to store information it
         ///< needs to receive this PID (for instance a file handle).
         ///< Handle->used indicated how many receivers are using this PID.
         ///< Type indicates some special types of PIDs, which the device may
         ///< need to set in a specific way.

// Common Interface facilities:

protected:
  cCiHandler *ciHandler;
public:
  cCiHandler *CiHandler(void) { return ciHandler; }

// Image Grab facilities

public:
  virtual bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);
         ///< Capture a single frame as an image.
         ///< Grabs the currently visible screen image into the given file, with the
         ///< given parameters.
         ///< \param FileName The name of the file to write. Should include the proper extension.
         ///< \param Jpeg If true will write a JPEG file. Otherwise a PNM file will be written.
         ///< \param Quality The compression factor for JPEG. 1 will create a very blocky
         ///<        and small image, 70..80 will yield reasonable quality images while keeping the
         ///<        image file size around 50 KB for a full frame. The default will create a big
         ///<        but very high quality image.
         ///< \param SizeX The number of horizontal pixels in the frame (default is the current screen width).
         ///< \param SizeY The number of vertical pixels in the frame (default is the current screen height).
         ///< \return True if all went well. */

// Video format facilities

public:
  virtual void SetVideoFormat(bool VideoFormat16_9);
         ///< Sets the output video format to either 16:9 or 4:3 (only useful
         ///< if this device has an MPEG decoder).
  virtual eVideoSystem GetVideoSystem(void);
         ///< Returns the video system of the currently displayed material
         ///< (default is PAL).

// Audio facilities

private:
  bool mute;
  int volume;
protected:
  virtual void SetVolumeDevice(int Volume);
       ///< Sets the audio volume on this device (Volume = 0...255).
  virtual int NumAudioTracksDevice(void) const;
       ///< Returns the number of audio tracks that are currently available on this
       ///< device. The default return value is 0, meaning that this device
       ///< doesn't have multiple audio track capabilities. The return value may
       ///< change with every call and need not necessarily be the number of list
       ///< entries returned by GetAudioTracksDevice(). This function is mainly called to
       ///< decide whether there should be an "Audio" button in a menu.
  virtual const char **GetAudioTracksDevice(int *CurrentTrack = NULL) const;
       ///< Returns a list of currently available audio tracks. The last entry in the
       ///< list must be NULL. The number of entries does not necessarily have to be
       ///< the same as returned by a previous call to NumAudioTracksDevice().
       ///< If CurrentTrack is given, it will be set to the index of the current track
       ///< in the returned list. Note that the list must not be changed after it has
       ///< been returned by a call to GetAudioTracksDevice()! The only time the list may
       ///< change is *inside* the GetAudioTracksDevice() function.
       ///< By default the return value is NULL and CurrentTrack, if given, will not
       ///< have any meaning.
  virtual void SetAudioTrackDevice(int Index);
       ///< Sets the current audio track to the given value, which should be within the
       ///< range of the list returned by a previous call to GetAudioTracksDevice()
       ///< (otherwise nothing will happen).
public:
  bool IsMute(void) const { return mute; }
  bool ToggleMute(void);
       ///< Turns the volume off or on and returns the new mute state.
  void SetVolume(int Volume, bool Absolute = false);
       ///< Sets the volume to the given value, either absolutely or relative to
       ///< the current volume.
  static int CurrentVolume(void) { return primaryDevice ? primaryDevice->volume : 0; }//XXX???
  int NumAudioTracks(void) const;
       ///< Returns the number of audio tracks that are currently available on this
       ///< device or a player attached to it.
  const char **GetAudioTracks(int *CurrentTrack = NULL) const;
       ///< Returns a list of currently available audio tracks. The last entry in the
       ///< list is NULL. The number of entries does not necessarily have to be
       ///< the same as returned by a previous call to NumAudioTracks().
       ///< If CurrentTrack is given, it will be set to the index of the current track
       ///< in the returned list.
       ///< By default the return value is NULL and CurrentTrack, if given, will not
       ///< have any meaning.
  void SetAudioTrack(int Index);
       ///< Sets the current audio track to the given value, which should be within the
       ///< range of the list returned by a previous call to GetAudioTracks() (otherwise
       ///< nothing will happen).

// Player facilities

private:
  cPlayer *player;
protected:
  virtual bool CanReplay(void) const;
       ///< Returns true if this device can currently start a replay session.
  virtual bool SetPlayMode(ePlayMode PlayMode);
       ///< Sets the device into the given play mode.
       ///< \return true if the operation was successful.
public:
  virtual int64_t GetSTC(void);
       ///< Gets the current System Time Counter, which can be used to
       ///< synchronize audio and video. If this device is unable to
       ///< provide the STC, -1 will be returned.
  virtual void TrickSpeed(int Speed);
       ///< Sets the device into a mode where replay is done slower.
       ///< Every single frame shall then be displayed the given number of
       ///< times.
  virtual void Clear(void);
       ///< Clears all video and audio data from the device.
       ///< A derived class must call the base class function to make sure
       ///< all registered cAudio objects are notified.
  virtual void Play(void);
       ///< Sets the device into play mode (after a previous trick
       ///< mode).
  virtual void Freeze(void);
       ///< Puts the device into "freeze frame" mode.
  virtual void Mute(void);
       ///< Turns off audio while replaying.
       ///< A derived class must call the base class function to make sure
       ///< all registered cAudio objects are notified.
  virtual void StillPicture(const uchar *Data, int Length);
       ///< Displays the given I-frame as a still picture.
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
       ///< Returns true if the device itself or any of the file handles in
       ///< Poller is ready for further action.
       ///< If TimeoutMs is not zero, the device will wait up to the given number
       ///< of milleseconds before returning in case there is no immediate
       ///< need for data.
  virtual int PlayVideo(const uchar *Data, int Length);
       ///< Actually plays the given data block as video. The data must be
       ///< part of a PES (Packetized Elementary Stream) which can contain
       ///< one video and one audio strem.
  virtual void PlayAudio(const uchar *Data, int Length);
       ///< Plays additional audio streams, like Dolby Digital.
       ///< A derived class must call the base class function to make sure data
       ///< is distributed to all registered cAudio objects.
  bool Replaying(void) const;
       ///< Returns true if we are currently replaying.
  void StopReplay(void);
       ///< Stops the current replay session (if any).
  bool AttachPlayer(cPlayer *Player);
       ///< Attaches the given player to this device.
  void Detach(cPlayer *Player);
       ///< Detaches the given player from this device.

// Receiver facilities

private:
  cReceiver *receiver[MAXRECEIVERS];
  int CanShift(int Ca, int Priority, int UsedCards = 0) const;
protected:
  int Priority(void) const;
      ///< Returns the priority of the current receiving session (0..MAXPRIORITY),
      ///< or -1 if no receiver is currently active. The primary device will
      ///< always return at least Setup.PrimaryLimit-1.
  virtual bool OpenDvr(void);
      ///< Opens the DVR of this device and prepares it to deliver a Transport
      ///< Stream for use in a cReceiver.
  virtual void CloseDvr(void);
      ///< Shuts down the DVR.
  virtual bool GetTSPacket(uchar *&Data);
      ///< Gets exactly one TS packet from the DVR of this device and returns
      ///< a pointer to it in Data. Only the first 188 bytes (TS_SIZE) Data
      ///< points to are valid and may be accessed. If there is currently no
      ///< new data available, Data will be set to NULL. The function returns
      ///< false in case of a non recoverable error, otherwise it returns true,
      ///< even if Data is NULL.
public:
  int  Ca(void) const;
       ///< Returns the ca of the current receiving session(s).
  bool Receiving(bool CheckAny = false) const;
       ///< Returns true if we are currently receiving.
  bool AttachReceiver(cReceiver *Receiver);
       ///< Attaches the given receiver to this device.
  void Detach(cReceiver *Receiver);
       ///< Detaches the given receiver from this device.
  };

/// Derived cDevice classes that can receive channels will have to provide
/// Transport Stream (TS) packets one at a time. cTSBuffer implements a
/// simple buffer that allows the device to read a larger amount of data
/// from the driver with each call to Read(), thus avoiding the overhead
/// of getting each TS packet separately from the driver. It also makes
/// sure the returned data points to a TS packet and automatically
/// re-synchronizes after broken packet.

class cTSBuffer {
private:
  int f;
  int size;
  int cardIndex;
  int tsRead;
  int tsWrite;
  uchar *buf;
  bool firstRead;
  int Used(void) { return tsRead <= tsWrite ? tsWrite - tsRead : size - tsRead + tsWrite; }
public:
  cTSBuffer(int File, int Size, int CardIndex);
  ~cTSBuffer();
  int Read(void);
  uchar *Get(void);
  };

#endif //__DEVICE_H
