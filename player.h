/*
 * player.h: The basic player interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: player.h 1.4 2002/06/23 12:56:38 kls Exp $
 */

#ifndef __PLAYER_H
#define __PLAYER_H

#include "device.h"
#include "osd.h"

class cPlayer {
  friend class cDevice;
private:
  cDevice *device;
  int deviceFileHandle;
protected:
  int DeviceFileHandle(void) { return deviceFileHandle; } //XXX+ needed for polling
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
  int PlayAudio(const uchar *Data, int Length);
               // XXX+ TODO
public:
  cPlayer(void);
  virtual ~cPlayer();
  bool IsAttached(void) { return device != NULL; }
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
  static void Launch(cControl *Control);
  static void Attach(void);
  static void Shutdown(void);
  static cControl *Control(void);
  };

#endif //__PLAYER_H
