/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.30 2001/09/23 10:57:33 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#define _GNU_SOURCE

#include "dvbapi.h"
#ifdef DVDSUPPORT
#include "dvd.h"
#endif //DVDSUPPORT
#include "osd.h"
#include "recording.h"

class cMenuMain : public cOsdMenu {
private:
  time_t lastActivity;
  int digit;
  const char *hk(const char *s);
public:
  cMenuMain(bool Replaying);
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

#ifdef DVDSUPPORT
class cMenuDVD : public cOsdMenu {
private:
  cDVD *dvd;//XXX member really necessary???
  eOSState Play(void);
  eOSState Eject(void);
public:
  cMenuDVD(void);
  virtual eOSState ProcessKey(eKeys Key);
  };
#endif //DVDSUPPORT

class cMenuRecordings : public cOsdMenu {
private:
  cRecordings Recordings;
  eOSState Play(void);
  eOSState Rewind(void);
  eOSState Del(void);
  eOSState Summary(void);
public:
  cMenuRecordings(void);
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
  };

class cRecordControls {
private:
  static cRecordControl *RecordControls[MAXDVBAPI];
public:
  static bool Start(cTimer *Timer = NULL);
  static void Stop(const char *InstantId);
  static void Stop(cDvbApi *DvbApi);
  static const char *GetInstantId(const char *LastInstantId);
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
  int timeSearchHH, timeSearchMM, timeSearchPos;
  void TimeSearchDisplay(void);
  void TimeSearchProcess(eKeys Key);
  void TimeSearch(void);
  void Show(int Seconds = 0);
  void Hide(void);
  static char *fileName;
#ifdef DVDSUPPORT
  static cDVD *dvd;//XXX member really necessary???
  static int titleid;//XXX
#endif //DVDSUPPORT
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
#ifdef DVDSUPPORT
  static void SetDVD(cDVD *DVD, int Title);//XXX
#endif //DVDSUPPORT
  static const char *LastReplayed(void);
  static void ClearLastReplayed(const char *FileName);
  };

#endif //_MENU_H
