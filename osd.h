/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 1.30 2002/05/18 12:36:30 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include "config.h"
#include "interface.h"
#include "tools.h"

#define MAXOSDITEMS (Setup.OSDheight - 4)

enum eOSState { osUnknown,
                osMenu,
                osContinue,
                osSchedule,
                osChannels,
                osTimers,
                osRecordings,
                osPlugin,
                osSetup,
                osCommands,
                osRecord,
                osReplay,
                osStopRecord,
                osStopReplay,
                osCancelEdit,
                osSwitchDvb,
                osBack,
                osEnd,
                os_User, // the following values can be used locally
                osUser1,
                osUser2,
                osUser3,
                osUser4,
                osUser5,
                osUser6,
                osUser7,
                osUser8,
                osUser9,
                osUser10,
              };

class cOsdItem : public cListObject {
private:
  const char *text;
  int offset;
  eOSState state;
protected:
  bool fresh;
  bool userColor;
  eDvbColor fgColor, bgColor;
public:
  cOsdItem(eOSState State = osUnknown);
  cOsdItem(const char *Text, eOSState State = osUnknown);
  virtual ~cOsdItem();
  bool HasUserColor(void) { return userColor; }
  void SetText(const char *Text, bool Copy = true);
  void SetColor(eDvbColor FgColor, eDvbColor BgColor = clrBackground);
  const char *Text(void) { return text; }
  virtual void Display(int Offset = -1, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  virtual void Set(void) {}
  virtual eOSState ProcessKey(eKeys Key);
};

class cOsdObject {
protected:
  bool needsFastResponse;
public:
  cOsdObject(bool FastResponse = false) { needsFastResponse = FastResponse; }
  virtual ~cOsdObject() {}
  int Width(void) { return Interface->Width(); }
  int Height(void) { return Interface->Height(); }
  bool NeedsFastResponse(void) { return needsFastResponse; }
  virtual eOSState ProcessKey(eKeys Key) = 0;
};

class cOsdMenu : public cOsdObject, public cList<cOsdItem> {
private:
  char *title;
  int cols[cInterface::MaxCols];
  int first, current, marked;
  cOsdMenu *subMenu;
  const char *helpRed, *helpGreen, *helpYellow, *helpBlue;
  const char *status;
  int digit;
  bool hasHotkeys;
protected:
  bool visible;
  const char *hk(const char *s);
  void SetHasHotkeys(void);
  virtual void Clear(void);
  bool SpecialItem(int idx);
  void SetCurrent(cOsdItem *Item);
  void RefreshCurrent(void);
  void DisplayCurrent(bool Current);
  void CursorUp(void);
  void CursorDown(void);
  void PageUp(void);
  void PageDown(void);
  void Mark(void);
  eOSState HotKey(eKeys Key);
  eOSState AddSubMenu(cOsdMenu *SubMenu);
  bool HasSubMenu(void) { return subMenu; }
  void SetStatus(const char *s);
  void SetTitle(const char *Title, bool ShowDate = true);
  void SetHelp(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  virtual void Del(int Index);
public:
  cOsdMenu(const char *Title, int c0 = 0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0);
  virtual ~cOsdMenu();
  int Current(void) { return current; }
  void Add(cOsdItem *Item, bool Current = false, cOsdItem *After = NULL);
  void Ins(cOsdItem *Item, bool Current = false, cOsdItem *Before = NULL);
  void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

#endif //__OSD_H
