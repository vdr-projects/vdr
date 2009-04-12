/*
 * player.h: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.h 2.4 2009/03/08 12:29:10 kls Exp $
 */

#ifndef __PLAYER_H
#define __PLAYER_H

#include "device.h"
#include "osdbase.h"

class cPlayer {
  friend class cDevice;
private:
  cDevice *device;
  ePlayMode playMode;
protected:
  void DeviceClrAvailableTracks(bool DescriptionsOnly = false) { if (device) device->ClrAvailableTracks(DescriptionsOnly); }
  bool DeviceSetAvailableTrack(eTrackType Type, int Index, uint16_t Id, const char *Language = NULL, const char *Description = NULL) { return device ? device->SetAvailableTrack(Type, Index, Id, Language, Description) : false; }
  bool DeviceSetCurrentAudioTrack(eTrackType Type) { return device ? device->SetCurrentAudioTrack(Type) : false; }
  bool DeviceSetCurrentSubtitleTrack(eTrackType Type) { return device ? device->SetCurrentSubtitleTrack(Type) : false; }
  bool DevicePoll(cPoller &Poller, int TimeoutMs = 0) { return device ? device->Poll(Poller, TimeoutMs) : false; }
  bool DeviceFlush(int TimeoutMs = 0) { return device ? device->Flush(TimeoutMs) : true; }
  bool DeviceHasIBPTrickSpeed(void) { return device ? device->HasIBPTrickSpeed() : false; }
  bool DeviceIsPlayingVideo(void) { return device ? device->IsPlayingVideo() : false; }
  void DeviceTrickSpeed(int Speed) { if (device) device->TrickSpeed(Speed); }
  void DeviceClear(void) { if (device) device->Clear(); }
  void DevicePlay(void) { if (device) device->Play(); }
  void DeviceFreeze(void) { if (device) device->Freeze(); }
  void DeviceMute(void) { if (device) device->Mute(); }
  void DeviceSetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat) { if (device) device->SetVideoDisplayFormat(VideoDisplayFormat); }
  void DeviceStillPicture(const uchar *Data, int Length) { if (device) device->StillPicture(Data, Length); }
  uint64_t DeviceGetSTC(void) { return device ? device->GetSTC() : -1; }
  void Detach(void);
  virtual void Activate(bool On) {}
       // This function is called right after the cPlayer has been attached to
       // (On == true) or before it gets detached from (On == false) a cDevice.
       // It can be used to do things like starting/stopping a thread.
  int PlayPes(const uchar *Data, int Length, bool VideoOnly = false);
       // Sends the given PES Data to the device and returns the number of
       // bytes that have actually been accepted by the device (or a
       // negative value in case of an error).
  int PlayTs(const uchar *Data, int Length, bool VideoOnly = false) { return device ? device->PlayTs(Data, Length, VideoOnly) : -1; }
       // Sends the given TS packet to the device and returns a positive number
       // if the packet has been accepted by the device, or a negative value in
       // case of an error.
public:
  cPlayer(ePlayMode PlayMode = pmAudioVideo);
  virtual ~cPlayer();
  bool IsAttached(void) { return device != NULL; }
  virtual double FramesPerSecond(void) { return DEFAULTFRAMESPERSECOND; }
       // Returns the number of frames per second of the currently played material.
  virtual bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false) { return false; }
       // Returns the current and total frame index, optionally snapped to the
       // nearest I-frame.
  virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed) { return false; }
       // Returns the current replay mode (if applicable).
       // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
       // we are going forward or backward and 'Speed' is -1 if this is normal
       // play/pause mode, 0 if it is single speed fast/slow forward/back mode
       // and >0 if this is multi speed mode.
  virtual void SetAudioTrack(eTrackType Type, const tTrackId *TrackId) {}
       // Sets the current audio track to the given value.
       // This is just a virtual hook for players that need to do special things
       // in order to switch audio tracks.
  virtual void SetSubtitleTrack(eTrackType Type, const tTrackId *TrackId) {}
       // Sets the current subtitle track to the given value.
       // This is just a virtual hook for players that need to do special things
       // in order to switch subtitle tracks.
  };

class cControl : public cOsdObject {
private:
  static cControl *control;
  static cMutex mutex;
  bool attached;
  bool hidden;
protected:
  cPlayer *player;
public:
  cControl(cPlayer *Player, bool Hidden = false);
  virtual ~cControl();
  virtual void Hide(void) = 0;
  virtual cOsdObject *GetInfo(void);
  double FramesPerSecond(void) { return player->FramesPerSecond(); }
  bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false) { return player->GetIndex(Current, Total, SnapToIFrame); }
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed) { return player->GetReplayMode(Play, Forward, Speed); }
  static void Launch(cControl *Control);
  static void Attach(void);
  static void Shutdown(void);
  static cControl *Control(void);
  };

#endif //__PLAYER_H
