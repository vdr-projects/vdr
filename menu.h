/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.17 2001/02/04 11:47:21 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#define _GNU_SOURCE

#include "dvbapi.h"
#include "osd.h"
#include "recording.h"

class cMenuMain : public cOsdMenu {
private:
  time_t lastActivity;
public:
  cMenuMain(bool Replaying);
  virtual eOSState ProcessKey(eKeys Key);
  };
  
class cDisplayChannel : public cOsdBase {
private:
  bool withInfo, group;
  int lines;
  int lastTime;
  int oldNumber, number;
  void DisplayChannel(const cChannel *Channel);
  void DisplayInfo(void);
public:
  cDisplayChannel(int Number, bool Switched, bool Group = false);
  cDisplayChannel(eKeys FirstKey);
  virtual ~cDisplayChannel();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuRecordings : public cOsdMenu {
private:
  cRecordings Recordings;
  eOSState Play(void);
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
  char *instantId;
public:
  cRecordControl(cDvbApi *DvbApi, cTimer *Timer = NULL);
  virtual ~cRecordControl();
  bool Process(void);
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
  static void Process(void);
  static bool Active(void);
  };

class cReplayControl : public cOsdBase {
private:
  cDvbApi *dvbApi;
  cMarks marks;
  bool visible, shown, displayFrames;
  void Show(void);
  void Hide(void);
  static char *fileName;
  static char *title;
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
