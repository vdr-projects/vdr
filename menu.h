/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.41 2002/03/31 13:53:23 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include "dvbapi.h"
#include "osd.h"
#include "recording.h"

class cMenuMain : public cOsdMenu {
private:
  time_t lastActivity;
  bool replaying;
  void Set(void);
public:
  cMenuMain(bool Replaying, eOSState State = osUnknown);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cDisplayChannel : public cOsdBase {
private:
  int group;
  bool withInfo;
  int lines;
  int lastTime;
  int oldNumber, number;
  void DisplayChannel(const cChannel *Channel);
  void DisplayInfo(void);
public:
  cDisplayChannel(int Number, bool Switched);
  cDisplayChannel(eKeys FirstKey);
  virtual ~cDisplayChannel();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cDisplayVolume : public cOsdBase {
private:
  int timeout;
  static cDisplayVolume *displayVolume;
  void Show(void);
  cDisplayVolume(void);
public:
  virtual ~cDisplayVolume();
  static cDisplayVolume *Create(void);
  static void Process(eKeys Key);
  eOSState ProcessKey(eKeys Key);
  };

class cMenuRecordingItem;

class cMenuRecordings : public cOsdMenu {
private:
  static cRecordings Recordings;
  char *base;
  int level;
  static int helpKeys;
  void SetHelpKeys(void);
  cRecording *GetRecording(cMenuRecordingItem *Item);
  bool Open(bool OpenSubMenus = false);
  eOSState Play(void);
  eOSState Rewind(void);
  eOSState Del(void);
  eOSState Summary(void);
public:
  cMenuRecordings(const char *Base = NULL, int Level = 0, bool OpenSubMenus = false);
  ~cMenuRecordings();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cRecordControl {
private:
  cDvbApi *dvbApi;
  cTimer *timer;
  const cEventInfo *eventInfo;
  char *instantId;
  char *fileName;
  bool GetEventInfo(void);
public:
  cRecordControl(cDvbApi *DvbApi, cTimer *Timer = NULL);
  virtual ~cRecordControl();
  bool Process(time_t t);
  bool Uses(cDvbApi *DvbApi) { return DvbApi == dvbApi; }
  void Stop(bool KeepInstant = false);
  bool IsInstant(void) { return instantId; }
  const char *InstantId(void) { return instantId; }
  const char *FileName(void) { return fileName; }
  cTimer *Timer(void) { return timer; }
  };

class cRecordControls {
private:
  static cRecordControl *RecordControls[MAXDVBAPI];
public:
  static bool Start(cTimer *Timer = NULL);
  static void Stop(const char *InstantId);
  static void Stop(cDvbApi *DvbApi);
  static bool StopPrimary(bool DoIt = false);
  static const char *GetInstantId(const char *LastInstantId);
  static cRecordControl *GetRecordControl(const char *FileName);
  static void Process(time_t t);
  static bool Active(void);
  };

class cReplayControl : public cOsdBase {
private:
  cDvbApi *dvbApi;
  cMarks marks;
  bool visible, modeOnly, shown, displayFrames;
  int lastCurrent, lastTotal;
  time_t timeoutShow;
  bool timeSearchActive, timeSearchHide;  
  int timeSearchTime, timeSearchPos;
  void TimeSearchDisplay(void);
  void TimeSearchProcess(eKeys Key);
  void TimeSearch(void);
  void Show(int Seconds = 0);
  void Hide(void);
  static char *fileName;
  static char *title;
  void DisplayAtBottom(const char *s = NULL);
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
  bool Visible(void) { return visible; }
  static void SetRecording(const char *FileName, const char *Title);
  static const char *LastReplayed(void);
  static void ClearLastReplayed(const char *FileName);
  };

#endif //_MENU_H
