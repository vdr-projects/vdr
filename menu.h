/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.10 2000/09/10 14:42:20 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#define _GNU_SOURCE

#include "dvbapi.h"
#include "osd.h"

class cMenuMain : public cOsdMenu {
private:
  time_t lastActivity;
public:
  cMenuMain(bool Replaying);
  virtual eOSState ProcessKey(eKeys Key);
  };
  
class cDirectChannelSelect : public cOsdBase {
private:
  int oldNumber;
  int number;
  int lastTime;
public:
  cDirectChannelSelect(eKeys FirstKey);
  virtual ~cDirectChannelSelect();
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
  static const char *GetInstantId(const char *LastInstantId);
  static void Process(void);
  };

class cReplayControl : public cOsdBase {
private:
  cDvbApi *dvbApi;
  bool visible, shown;
  void Show(void);
  void Hide(void);
  static char *fileName;
  static char *title;
public:
  cReplayControl(void);
  virtual ~cReplayControl();
  virtual eOSState ProcessKey(eKeys Key);
  bool Visible(void) { return visible; }
  static void SetRecording(const char *FileName, const char *Title);
  };

#endif //_MENU_H
