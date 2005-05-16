/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.70 2005/05/15 14:34:54 kls Exp $
 */

#ifndef __MENU_H
#define __MENU_H

#include "ci.h"
#include "device.h"
#include "epg.h"
#include "osdbase.h"
#include "dvbplayer.h"
#include "recorder.h"
#include "skins.h"

class cMenuText : public cOsdMenu {
private:
  char *text;
public:
  cMenuText(const char *Title, const char *Text, eDvbFont Font = fontOsd);
  virtual ~cMenuText();
  void SetText(const char *Text);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuMain : public cOsdMenu {
private:
  time_t lastActivity;
  bool replaying;
  static cOsdObject *pluginOsdObject;
  void Set(const char *Plugin = NULL);
public:
  cMenuMain(bool Replaying, eOSState State = osUnknown, const char *Plugin = NULL);
  virtual eOSState ProcessKey(eKeys Key);
  static cOsdObject *PluginOsdObject(void);
  };

class cDisplayChannel : public cOsdObject {
private:
  cSkinDisplayChannel *displayChannel;
  int group;
  bool withInfo;
  cTimeMs lastTime;
  int number;
  cChannel *channel;
  const cEvent *lastPresent;
  const cEvent *lastFollowing;
  void DisplayChannel(void);
  void DisplayInfo(void);
  void Refresh(void);
public:
  cDisplayChannel(int Number, bool Switched);
  cDisplayChannel(eKeys FirstKey);
  virtual ~cDisplayChannel();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cDisplayVolume : public cOsdObject {
private:
  cSkinDisplayVolume *displayVolume;
  cTimeMs timeout;
  static cDisplayVolume *currentDisplayVolume;
  virtual void Show(void);
  cDisplayVolume(void);
public:
  virtual ~cDisplayVolume();
  static cDisplayVolume *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

class cDisplayTracks : public cOsdObject {
private:
  cSkinDisplayTracks *displayTracks;
  cTimeMs timeout;
  eTrackType types[ttMaxTrackTypes];
  char *descriptions[ttMaxTrackTypes];
  int numTracks, track, audioChannel;
  static cDisplayTracks *currentDisplayTracks;
  virtual void Show(void);
  cDisplayTracks(void);
public:
  virtual ~cDisplayTracks();
  static bool IsOpen(void) { return currentDisplayTracks != NULL; }
  static cDisplayTracks *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

class cMenuCam : public cOsdMenu {
private:
  cCiMenu *ciMenu;
  time_t lastActivity;
  bool selected;
  eOSState Select(void);
public:
  cMenuCam(cCiMenu *CiMenu);
  virtual ~cMenuCam();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuCamEnquiry : public cOsdMenu {
private:
  cCiEnquiry *ciEnquiry;
  time_t lastActivity;
  char *input;
  bool replied;
  eOSState Reply(void);
public:
  cMenuCamEnquiry(cCiEnquiry *CiEnquiry);
  virtual ~cMenuCamEnquiry();
  virtual eOSState ProcessKey(eKeys Key);
  };

cOsdObject *CamControl(void);

class cMenuRecordingItem;

class cMenuRecordings : public cOsdMenu {
private:
  char *base;
  int level;
  static int helpKeys;
  void SetHelpKeys(void);
  cRecording *GetRecording(cMenuRecordingItem *Item);
  bool Open(bool OpenSubMenus = false);
  eOSState Play(void);
  eOSState Rewind(void);
  eOSState Delete(void);
  eOSState Info(void);
  eOSState Commands(eKeys Key = kNone);
public:
  cMenuRecordings(const char *Base = NULL, int Level = 0, bool OpenSubMenus = false);
  ~cMenuRecordings();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cRecordControl {
private:
  cDevice *device;
  cTimer *timer;
  cRecorder *recorder;
  const cEvent *event;
  char *instantId;
  char *fileName;
  bool GetEvent(void);
public:
  cRecordControl(cDevice *Device, cTimer *Timer = NULL, bool Pause = false);
  virtual ~cRecordControl();
  bool Process(time_t t);
  cDevice *Device(void) { return device; }
  void Stop(void);
  const char *InstantId(void) { return instantId; }
  const char *FileName(void) { return fileName; }
  cTimer *Timer(void) { return timer; }
  };

class cRecordControls {
private:
  static cRecordControl *RecordControls[];
public:
  static bool Start(cTimer *Timer = NULL, bool Pause = false);
  static void Stop(const char *InstantId);
  static void Stop(cDevice *Device);
  static bool StopPrimary(bool DoIt = false);
  static bool PauseLiveVideo(void);
  static const char *GetInstantId(const char *LastInstantId);
  static cRecordControl *GetRecordControl(const char *FileName);
  static void Process(time_t t);
  static void ChannelDataModified(cChannel *Channel);
  static bool Active(void);
  static void Shutdown(void);
  };

class cReplayControl : public cDvbPlayerControl {
private:
  cSkinDisplayReplay *displayReplay;
  cMarks marks;
  bool visible, modeOnly, shown, displayFrames;
  int lastCurrent, lastTotal;
  bool lastPlay, lastForward;
  int lastSpeed;
  time_t timeoutShow;
  bool timeSearchActive, timeSearchHide;
  int timeSearchTime, timeSearchPos;
  void TimeSearchDisplay(void);
  void TimeSearchProcess(eKeys Key);
  void TimeSearch(void);
  void ShowTimed(int Seconds = 0);
  static char *fileName;
  static char *title;
  void ShowMode(void);
  bool ShowProgress(bool Initial);
  void MarkToggle(void);
  void MarkJump(bool Forward);
  void MarkMove(bool Forward);
  void EditCut(void);
  void EditTest(void);
public:
  cReplayControl(void);
  virtual ~cReplayControl();
  virtual eOSState ProcessKey(eKeys Key);
  virtual void Show(void);
  virtual void Hide(void);
  bool Visible(void) { return visible; }
  static void SetRecording(const char *FileName, const char *Title);
  static const char *LastReplayed(void);
  static void ClearLastReplayed(const char *FileName);
  };

#endif //__MENU_H
