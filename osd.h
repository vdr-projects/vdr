/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 1.1 2000/02/19 13:36:48 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include "config.h"
#include "interface.h"
#include "tools.h"

#define MAXOSDITEMS 9

enum eOSStatus { osUnknown,
                 osContinue,
                 osProcessed,
                 osChannels,
                 osTimer,
                 osRecordings,
                 osBack,
                 osEnd,
               };

class cOsdItem : public cListObject {
private:
  char *text;
  int offset;
  eOSStatus status;
protected:
  bool fresh;
public:
  cOsdItem(eOSStatus Status = osUnknown);
  cOsdItem(char *Text, eOSStatus Status = osUnknown);
  virtual ~cOsdItem();
  void SetText(char *Text, bool Copy = true);
  char *Text(void) { return text; }
  void Display(int Offset = -1, bool Current = false);
  virtual void Set(void) {}
  virtual eOSStatus ProcessKey(eKeys Key);
  };

class cOsdMenu : public cList<cOsdItem> {
private:
  char *title;
  int cols[cInterface::MaxCols];
  int first, current, count;
  cOsdMenu *subMenu;
protected:
  void RefreshCurrent(void);
  void DisplayCurrent(bool Current);
  void CursorUp(void);
  void CursorDown(void);
  eOSStatus AddSubMenu(cOsdMenu *SubMenu);
public:
  cOsdMenu(char *Title, int c0 = 0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0);
  virtual ~cOsdMenu();
  int Current(void) { return current; }
  void Add(cOsdItem *Item, bool Current = false);
  void Display(void);
  virtual eOSStatus ProcessKey(eKeys Key);
  };

#endif //__OSD_H
