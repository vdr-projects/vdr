/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.8 2000/04/30 10:58:49 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include "osd.h"

class cMenuMain : public cOsdMenu {
public:
  cMenuMain(bool Recording);
  virtual eOSState ProcessKey(eKeys Key);
  };
  
class cRecordControl : public cOsdBase {
private:
  cTimer *timer;
  bool isInstant;
public:
  cRecordControl(cTimer *Timer = NULL);
  virtual ~cRecordControl();
  virtual eOSState ProcessKey(eKeys Key);
  void Stop(bool KeepInstant = false);
  bool IsInstant(void) { return isInstant; }
  };

class cReplayControl : public cOsdBase {
private:
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
