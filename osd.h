/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 1.7 2000/04/30 10:37:15 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include "config.h"
#include "interface.h"
#include "tools.h"

#define MAXOSDITEMS 9

enum eOSState { osUnknown,
                osMenu,
                osContinue,
                osChannels,
                osTimer,
                osRecordings,
                osReplay,
                osStopRecord,
                osBack,
                osEnd,
              };

class cOsdItem : public cListObject {
private:
  const char *text;
  int offset;
  eOSState state;
protected:
  bool fresh;
public:
  cOsdItem(eOSState State = osUnknown);
  cOsdItem(char *Text, eOSState State = osUnknown);
  virtual ~cOsdItem();
  void SetText(const char *Text, bool Copy = true);
  const char *Text(void) { return text; }
  void Display(int Offset = -1, bool Current = false);
  virtual void Set(void) {}
  virtual eOSState ProcessKey(eKeys Key);
  };

class cOsdBase {
protected:
  bool needsFastResponse;
public:
  cOsdBase(bool FastResponse = false) { needsFastResponse = FastResponse; }
  virtual ~cOsdBase() {}
  virtual eOSState ProcessKey(eKeys Key) = 0;
  bool NeedsFastResponse(void) { return needsFastResponse; }
  };

class cOsdMenu : public cOsdBase, public cList<cOsdItem> {
private:
  char *title;
  int cols[cInterface::MaxCols];
  int first, current, marked;
  cOsdMenu *subMenu;
  const char *helpRed, *helpGreen, *helpYellow, *helpBlue;
  const char *status;
protected:
  bool visible;
  void RefreshCurrent(void);
  void DisplayCurrent(bool Current);
  void CursorUp(void);
  void CursorDown(void);
  void Mark(void);
  eOSState AddSubMenu(cOsdMenu *SubMenu);
  bool HasSubMenu(void) { return subMenu; }
  void SetStatus(const char *s);
  void SetHelp(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  virtual void Del(int Index);
public:
  cOsdMenu(char *Title, int c0 = 0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0);
  virtual ~cOsdMenu();
  int Current(void) { return current; }
  void Add(cOsdItem *Item, bool Current = false);
  void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

#endif //__OSD_H
