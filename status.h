/*
 * status.h: Status monitoring
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: status.h 5.2 2025/01/29 11:15:26 kls Exp $
 */

#ifndef __STATUS_H
#define __STATUS_H

#include "config.h"
#include "device.h"
#include "player.h"
#include "tools.h"

// Several member functions of the following classes are called with a pointer to
// an object from a global list (cTimer, cChannel, cRecording or cEvent). In these
// cases the core VDR code holds a lock on the respective list. While in general a
// plugin should only work with the objects and data that is explicitly given to it
// in the function call, the called function may itself set a read lock (not a write
// lock!) on this list, because read locks can be nested. It may also set read locks
// (not write locks!) on higher order lists.
// For instance, a function that is called with a cChannel may lock cRecordings and/or
// cSchedules (which contains cEvent objects), but not cTimers. If a plugin needs to
// set locks of its own (on mutexes defined inside the plugin code), it shall do so
// after setting any locks on VDR's global lists, and it shall always set these
// locks in the same sequence, to avoid deadlocks.

enum eTimerChange { tcMod, tcAdd, tcDel }; // tcMod is obsolete and no longer used!

class cTimer;

class cStatus : public cListObject {
private:
  static cList<cStatus> statusMonitors;
protected:
  // These functions can be implemented by derived classes to receive status information:
  virtual void ChannelChange(const cChannel *Channel) {}
               // Indicates a change in the parameters of the given Channel that may
               // require a retune.
  virtual void TimerChange(const cTimer *Timer, eTimerChange Change) {}
               // Indicates a change in the timer settings.
               // Timer points to the timer that has been added or will be deleted, respectively.
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView) {}
               // Indicates a channel switch on the given DVB device.
               // If ChannelNumber is 0, this is before the channel is being switched,
               // otherwise ChannelNumber is the number of the channel that has been switched to.
               // LiveView tells whether this channel switch is for live viewing.
  virtual void Recording(const cDevice *Device, const char *Name, const char *FileName, bool On) {}
               // The given DVB device has started (On = true) or stopped (On = false) recording Name.
               // Name is the name of the recording, without any directory path. The full file name
               // of the recording is given in FileName, which may be NULL in case there is no
               // actual file involved. If On is false, Name may be NULL.
  virtual void Replaying(const cControl *Control, const char *Name, const char *FileName, bool On) {}
               // The given player control has started (On = true) or stopped (On = false) replaying Name.
               // Name is the name of the recording, without any directory path. In case of a player that can't provide
               // a name, Name can be a string that identifies the player type (like, e.g., "DVD").
               // The full file name of the recording is given in FileName, which may be NULL in case there is no
               // actual file involved. If On is false, Name may be NULL.
  virtual void MarksModified(const cMarks *Marks) {}
               // If the editing marks of the recording that is currently being played
               // are modified in any way, this function is called with the list of
               // Marks. If Marks is NULL, the editing marks for the currently played
               // recording have been deleted entirely.
  virtual void SetVolume(int Volume, bool Absolute) {}
               // The volume has been set to the given value, either
               // absolutely or relative to the current volume.
  virtual void SetAudioTrack(int Index, const char * const *Tracks) {}
               // The audio track has been set to the one given by Index, which
               // points into the Tracks array of strings. Tracks is NULL terminated.
  virtual void SetAudioChannel(int AudioChannel) {}
               // The audio channel has been set to the given value.
               // 0=stereo, 1=left, 2=right, -1=no information available.
  virtual void SetSubtitleTrack(int Index, const char * const *Tracks) {}
               // The subtitle track has been set to the one given by Index, which
               // points into the Tracks array of strings. Tracks is NULL terminated.
  virtual void OsdClear(void) {}
               // The OSD has been cleared.
  virtual void OsdTitle(const char *Title) {}
               // Title has been displayed in the title line of the menu.
  virtual void OsdStatusMessage(const char *Message) {}
               // Message has been displayed in the status line of the menu.
               // If Message is NULL, the status line has been cleared.
  virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue) {}
               // The help keys have been set to the given values (may be NULL).
  virtual void OsdItem(const char *Text, int Index) {}
               // The OSD displays the given single line Text as menu item at Index.
  virtual void OsdItem2(const char *Text, int Index, bool Selectable) { OsdItem(Text, Index); }
               // The OSD displays the given single line Text as menu item at Index.
               // Selectable is true if this item can be selected.
  virtual void OsdCurrentItem(const char *Text) {}
               // The OSD displays the given single line Text as the current menu item.
  virtual void OsdCurrentItem2(const char *Text, int Index) { OsdCurrentItem(Text); }
               // The OSD displays the given single line Text as the current menu item.
               // Index is the one that was given in OsdItem[2]() for this item.
  virtual void OsdTextItem(const char *Text, bool Scroll) {}
               // The OSD displays the given multi line text. If Text points to an
               // actual string, that text shall be displayed and Scroll has no
               // meaning. If Text is NULL, Scroll defines whether the previously
               // received text shall be scrolled up (true) or down (false) and
               // the text shall be redisplayed with the new offset.
  virtual void OsdChannel(const char *Text) {}
               // The OSD displays the single line Text with the current channel information.
  virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle) {}
               // The OSD displays the given programme information.
public:
  cStatus(void);
  virtual ~cStatus();
  // These functions are called whenever the related status information changes:
  static void MsgChannelChange(const cChannel *Channel);
  static void MsgTimerChange(const cTimer *Timer, eTimerChange Change);
  static void MsgChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);
  static void MsgRecording(const cDevice *Device, const char *Name, const char *FileName, bool On);
  static void MsgReplaying(const cControl *Control, const char *Name, const char *FileName, bool On);
  static void MsgMarksModified(const cMarks* Marks);
  static void MsgSetVolume(int Volume, bool Absolute);
  static void MsgSetAudioTrack(int Index, const char * const *Tracks);
  static void MsgSetAudioChannel(int AudioChannel);
  static void MsgSetSubtitleTrack(int Index, const char * const *Tracks);
  static void MsgOsdClear(void);
  static void MsgOsdTitle(const char *Title);
  static void MsgOsdStatusMessage(const char *Message);
  static void MsgOsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue);
  static void MsgOsdItem(const char *Text, int Index, bool Selectable = true);
  static void MsgOsdCurrentItem(const char *Text, int Index = -1);
  static void MsgOsdTextItem(const char *Text,  bool Scroll = false);
  static void MsgOsdChannel(const char *Text);
  static void MsgOsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle);
  };

#endif //__STATUS_H
