/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 5.10 2026/02/01 14:36:14 kls Exp $
 */

#ifndef __MENU_H
#define __MENU_H

#include "ci.h"
#include "device.h"
#include "epg.h"
#include "osdbase.h"
#include "dvbplayer.h"
#include "menuitems.h"
#include "recorder.h"
#include "skins.h"

class cMenuText : public cOsdMenu {
private:
  char *text;
  eDvbFont font;
public:
  cMenuText(const char *Title, const char *Text, eDvbFont Font = fontOsd);
  virtual ~cMenuText() override;
  void SetText(const char *Text);
  virtual void Display(void) override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

class cMenuFolder : public cOsdMenu {
private:
  cNestedItemList *nestedItemList;
  cList<cNestedItem> *list;
  cString dir;
  cOsdItem *firstFolder;
  bool editing;
  int helpKeys;
  void SetHelpKeys(void);
  void Set(const char *CurrentFolder = NULL);
  void DescendPath(const char *Path);
  eOSState SetFolder(void);
  eOSState Select(bool Open);
  eOSState New(void);
  eOSState Delete(void);
  eOSState Edit(void);
  cMenuFolder(const char *Title, cList<cNestedItem> *List, cNestedItemList *NestedItemList, const char *Dir, const char *Path = NULL);
public:
  cMenuFolder(const char *Title, cNestedItemList *NestedItemList, const char *Path = NULL);
  cString GetFolder(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

class cMenuCommands : public cOsdMenu {
private:
  cList<cNestedItem> *commands;
  cString parameters;
  cString title;
  cString command;
  bool confirm;
  char *result;
  bool Parse(const char *s);
  eOSState Execute(void);
public:
  cMenuCommands(const char *Title, cList<cNestedItem> *Commands, const char *Parameters = NULL);
  virtual ~cMenuCommands() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

class cMenuEditTimer : public cOsdMenu {
private:
  static const cTimer *addedTimer;
  cTimer *timer;
  cTimer data;
  int channel;
  bool addIfConfirmed;
  cStringList svdrpServerNames;
  char remote[HOST_NAME_MAX];
  cMenuEditStrItem *pattern;
  cMenuEditStrItem *file;
  cMenuEditDateItem *day;
  cMenuEditDateItem *firstday;
  eOSState SetFolder(void);
  void SetFirstDayItem(void);
  void SetPatternItem(bool Initial = false);
  void SetHelpKeys(void);
public:
  cMenuEditTimer(cTimer *Timer, bool New = false);
  virtual ~cMenuEditTimer() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  static const cTimer *AddedTimer(void);
  };

class cMenuEvent : public cOsdMenu {
private:
  const cEvent *event;
public:
  cMenuEvent(const cTimers *Timers, const cChannels *Channels, const cEvent *Event, bool CanSwitch = false, bool Buttons = false);
  virtual void Display(void) override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

class cMenuMain : public cOsdMenu {
private:
  bool replaying;
  cOsdItem *deletedRecordingsItem;
  cOsdItem *stopReplayItem;
  cOsdItem *cancelEditingItem;
  cOsdItem *stopRecordingItem;
  int recordControlsState;
  static cOsdObject *pluginOsdObject;
  void Set(void);
  bool Update(bool Force = false);
public:
  cMenuMain(eOSState State = osUnknown, bool OpenSubMenus = false);
  virtual eOSState ProcessKey(eKeys Key) override;
  static cOsdObject *PluginOsdObject(void);
  };

class cDisplayChannel : public cOsdObject {
private:
  cSkinDisplayChannel *displayChannel;
  int group;
  bool withInfo;
  cTimeMs lastTime;
  int number;
  bool timeout;
  int osdState;
  const cPositioner *positioner;
  const cChannel *channel;
  const cEvent *lastPresent;
  const cEvent *lastFollowing;
  static cDisplayChannel *currentDisplayChannel;
  void DisplayChannel(void);
  void DisplayInfo(void);
  void Refresh(void);
  const cChannel *NextAvailableChannel(const cChannel *Channel, int Direction);
public:
  cDisplayChannel(int Number, bool Switched);
  cDisplayChannel(eKeys FirstKey);
  virtual ~cDisplayChannel() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  static bool IsOpen(void) { return currentDisplayChannel != NULL; }
  };

class cDisplayVolume : public cOsdObject {
private:
  cSkinDisplayVolume *displayVolume;
  cTimeMs timeout;
  static cDisplayVolume *currentDisplayVolume;
  virtual void Show(void) override;
  cDisplayVolume(void);
public:
  virtual ~cDisplayVolume() override;
  static cDisplayVolume *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

class cDisplayTracks : public cOsdObject {
private:
  cSkinDisplayTracks *displayTracks;
  cTimeMs timeout;
  eTrackType types[ttMaxTrackTypes];
  char *descriptions[ttMaxTrackTypes + 1]; // list is NULL terminated
  int numTracks, track, audioChannel;
  static cDisplayTracks *currentDisplayTracks;
  virtual void Show(void) override;
  cDisplayTracks(void);
public:
  virtual ~cDisplayTracks() override;
  static bool IsOpen(void) { return currentDisplayTracks != NULL; }
  static cDisplayTracks *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

class cDisplaySubtitleTracks : public cOsdObject {
private:
  cSkinDisplayTracks *displayTracks;
  cTimeMs timeout;
  eTrackType types[ttMaxTrackTypes];
  char *descriptions[ttMaxTrackTypes + 1]; // list is NULL terminated
  int numTracks, track;
  static cDisplaySubtitleTracks *currentDisplayTracks;
  virtual void Show(void) override;
  cDisplaySubtitleTracks(void);
public:
  virtual ~cDisplaySubtitleTracks() override;
  static bool IsOpen(void) { return currentDisplayTracks != NULL; }
  static cDisplaySubtitleTracks *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

cOsdObject *CamControl(void);
bool CamMenuActive(void);

class cRecordingFilter {
public:
  virtual ~cRecordingFilter(void) {};
  virtual bool Filter(const cRecording *Recording) const = 0;
      ///< Returns true if the given Recording shall be displayed in the Recordings menu.
  };

class cMenuRecordingItem;

class cMenuRecordings : public cOsdMenu {
private:
  char *base;
  int level;
  cStateKey recordingsStateKey;
  int helpKeys;
  bool delRecMenu;
  const cRecordingFilter *filter;
  static cString path;
  static cString fileName;
  static cString deletedName;
  static time_t toggleDelRec;
  void SetHelpKeys(void);
  void Set(bool Refresh = false);
  bool Open(bool OpenSubMenus = false);
  eOSState AdjustTitle(eOSState State, bool Redisplay = true);
  eOSState Play(void);
  eOSState Rewind(void);
  eOSState Delete(void);
  eOSState Restore(void);
  eOSState Purge(void);
  eOSState Info(void);
  eOSState Sort(void);
  eOSState Commands(eKeys Key = kNone);
  void SetDeleted(const char *FileName);
protected:
  cString DirectoryName(void);
public:
  cMenuRecordings(const char *Base = NULL, int Level = 0, bool OpenSubMenus = false, const cRecordingFilter *Filter = NULL, bool DelRecMenu = false);
  ~cMenuRecordings();
  virtual eOSState ProcessKey(eKeys Key) override;
  static void SetRecording(const char *FileName);
  };

class cRecordControl {
private:
  cDevice *device;
  cTimer *timer;
  cRecorder *recorder;
  const cEvent *event;
  cString instantId;
  char *fileName;
  bool GetEvent(void);
public:
  cRecordControl(cDevice *Device, cTimers *Timers, cTimer *Timer = NULL, bool Pause = false);
  virtual ~cRecordControl();
  bool Process(time_t t);
  cDevice *Device(void) { return device; }
  void Stop(bool ExecuteUserCommand = true);
  const char *InstantId(void) { return instantId; }
  const char *FileName(void) { return fileName; }
  cTimer *Timer(void) { return timer; }
  };

class cRecordControls {
private:
  static cRecordControl *RecordControls[];
  static int state;
public:
  static bool Start(cTimers *Timers, cTimer *Timer, bool Pause = false);
  static bool Start(bool Pause = false);
  static void Stop(const char *InstantId);
  static void Stop(cTimer *Timer);
  static bool PauseLiveVideo(void);
  static const char *GetInstantId(const char *LastInstantId);
  static cRecordControl *GetRecordControl(const char *FileName);
  static cRecordControl *GetRecordControl(const cTimer *Timer);
         ///< Returns the cRecordControl for the given Timer.
         ///< If there is no cRecordControl for Timer, NULL is returned.
  static bool Process(cTimers *Timers, time_t t);
  static void ChannelDataModified(const cChannel *Channel);
  static bool Active(void);
  static void Shutdown(void);
  static void ChangeState(void) { state++; }
  static bool StateChanged(int &State);
  };

class cAdaptiveSkipper {
private:
  int *initialValue;
  int currentValue;
  double framesPerSecond;
  eKeys lastKey;
  cTimeMs timeout;
public:
  cAdaptiveSkipper(void);
  void Initialize(int *InitialValue, double FramesPerSecond);
  int GetValue(eKeys Key);
  };

class cReplayControl : public cDvbPlayerControl {
private:
  static cTimer *timeshiftTimer;
  cSkinDisplayReplay *displayReplay;
  cAdaptiveSkipper adaptiveSkipper;
  cMarks marks;
  bool marksModified;
  bool visible, modeOnly, shown, displayFrames;
  int lastErrors;
  int lastCurrent, lastTotal;
  bool lastPlay, lastForward;
  int lastSpeed;
  time_t timeoutShow;
  cTimeMs updateTimer;
  bool timeSearchActive, timeSearchHide;
  int timeSearchTime, timeSearchPos;
  void TimeSearchDisplay(void);
  void TimeSearchProcess(eKeys Key);
  void TimeSearch(void);
  void ShowTimed(int Seconds = 0);
  static cReplayControl *currentReplayControl;
  static cString fileName;
  void ShowMode(void);
  bool ShowProgress(bool Initial);
  void MarkToggle(void);
  void MarkJump(bool Forward);
  void MarkMove(int Frames, bool MarkRequired);
  void ErrorJump(bool Forward);
  void EditCut(void);
  void EditTest(void);
public:
  cReplayControl(bool PauseLive = false);
  virtual ~cReplayControl() override;
  static void DelTimeshiftTimer(void);
  void Stop(void);
  virtual cOsdObject *GetInfo(void) override;
  virtual const cRecording *GetRecording(void) override;
  virtual eOSState ProcessKey(eKeys Key) override;
  virtual void Show(void) override;
  virtual void Hide(void) override;
  bool Visible(void) { return visible; }
  virtual void ClearEditingMarks(void) override;
  static void SetRecording(const char *FileName);
  static const char *NowReplaying(void);
  static const char *LastReplayed(void);
  static void ClearLastReplayed(const char *FileName);
  };

void SetTrackDescriptions(int LiveChannel);

#endif //__MENU_H
