/*
 * player.h: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.h 1.10 2002/11/03 11:27:30 kls Exp $
 */

#ifndef __PLAYER_H
#define __PLAYER_H

#include "device.h"
#include "osd.h"

class cPlayer {
  friend class cDevice;
private:
  cDevice *device;
  ePlayMode playMode;
protected:
  bool DevicePoll(cPoller &Poller, int TimeoutMs = 0) { return device ? device->Poll(Poller, TimeoutMs) : false; }
  void DeviceTrickSpeed(int Speed) { if (device) device->TrickSpeed(Speed); }
  void DeviceClear(void) { if (device) device->Clear(); }
  void DevicePlay(void) { if (device) device->Play(); }
  void DeviceFreeze(void) { if (device) device->Freeze(); }
  void DeviceMute(void) { if (device) device->Mute(); }
  void DeviceStillPicture(const uchar *Data, int Length) { if (device) device->StillPicture(Data, Length); }
  void Detach(void);
  virtual void Activate(bool On) {}
       // This function is called right after the cPlayer has been attached to
       // (On == true) or before it gets detached from (On == false) a cDevice.
       // It can be used to do things like starting/stopping a thread.
  int PlayVideo(const uchar *Data, int Length);
       // Sends the given Data to the video device and returns the number of
       // bytes that have actually been accepted by the video device (or a
       // negative value in case of an error).
  void PlayAudio(const uchar *Data, int Length);
       // Plays additional audio streams, like Dolby Digital.
public:
  cPlayer(ePlayMode PlayMode = pmAudioVideo);
  virtual ~cPlayer();
  bool IsAttached(void) { return device != NULL; }
  virtual bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false) { return false; }
       // Returns the current and total frame index, optionally snapped to the
       // nearest I-frame.
  virtual bool GetReplayMode(bool &Play, bool &Forward, int &Speed) { return false; }
       // Returns the current replay mode (if applicable).
       // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
       // we are going forward or backward and 'Speed' is -1 if this is normal
       // play/pause mode, 0 if it is single speed fast/slow forward/back mode
       // and >0 if this is multi speed mode.
  virtual int NumAudioTracks(void) const { return 0; }
       // Returns the number of audio tracks that are currently available on this
       // player. The default return value is 0, meaning that this player
       // doesn't have multiple audio track capabilities. The return value may
       // change with every call and need not necessarily be the number of list
       // entries returned by GetAudioTracks(). This function is mainly called to
       // decide whether there should be an "Audio" button in a menu.
  virtual const char **GetAudioTracks(int *CurrentTrack = NULL) const { return NULL; }
       // Returns a list of currently available audio tracks. The last entry in the
       // list must be NULL. The number of entries does not necessarily have to be
       // the same as returned by a previous call to NumAudioTracks().
       // If CurrentTrack is given, it will be set to the index of the current track
       // in the returned list. Note that the list must not be changed after it has
       // been returned by a call to GetAudioTracks()! The only time the list may
       // change is *inside* the GetAudioTracks() function.
       // By default the return value is NULL and CurrentTrack, if given, will not
       // have any meaning.
  virtual void SetAudioTrack(int Index) {}
       // Sets the current audio track to the given value, which should be within the
       // range of the list returned by a previous call to GetAudioTracks()
       // (otherwise nothing will happen).
  };

class cControl : public cOsdObject {
private:
  static cControl *control;
  bool attached;
  bool hidden;
protected:
  cPlayer *player;
public:
  cControl(cPlayer *Player, bool Hidden = false);
  virtual ~cControl();
  virtual void Hide(void) = 0;
  bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false) { return player->GetIndex(Current, Total, SnapToIFrame); }
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed) { return player->GetReplayMode(Play, Forward, Speed); }
  static void Launch(cControl *Control);
  static void Attach(void);
  static void Shutdown(void);
  static cControl *Control(void);
  };

#endif //__PLAYER_H
