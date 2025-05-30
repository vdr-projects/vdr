/*
 * osdbase.h: Basic interface to the On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osdbase.h 5.3 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __OSDBASE_H
#define __OSDBASE_H

#include "config.h"
#include "osd.h"
#include "skins.h"
#include "tools.h"

enum eOSState { osUnknown,
                osContinue,
                osSchedule,
                osChannels,
                osTimers,
                osRecordings,
                osPlugin,
                osSetup,
                osCommands,
                osPause,
                osRecord,
                osReplay,
                osStopRecord,
                osStopReplay,
                osCancelEdit,
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
  char *text;
  eOSState state;
  bool selectable;
protected:
  bool fresh;
public:
  cOsdItem(eOSState State = osUnknown);
  cOsdItem(const char *Text, eOSState State = osUnknown, bool Selectable = true);
  virtual ~cOsdItem() override;
  bool Selectable(void) const { return selectable; }
  void SetText(const char *Text, bool Copy = true);
  void SetSelectable(bool Selectable);
  void SetFresh(bool Fresh);
  const char *Text(void) const { return text; }
  virtual void Set(void) {}
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cOsdObject {
  friend class cOsdMenu;
private:
  bool isMenu;
  bool needsFastResponse;
protected:
  void SetNeedsFastResponse(bool NeedsFastResponse) { needsFastResponse = NeedsFastResponse; }
public:
  cOsdObject(bool FastResponse = false) { isMenu = false; needsFastResponse = FastResponse; }
  virtual ~cOsdObject() {}
  virtual bool NeedsFastResponse(void) { return needsFastResponse; }
  bool IsMenu(void) const { return isMenu; }
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key) { return osUnknown; }
  };

class cOsdMenu : public cOsdObject, public cList<cOsdItem> {
private:
  static cSkinDisplayMenu *displayMenu;
  static int displayMenuCount;
  static int osdState;
  static cOsdMenu *topMenu;
  int displayMenuItems;
  char *title;
  int cols[cSkinDisplayMenu::MaxTabs];
  int first, current, marked;
  int lastOffset;
  bool conveyStatus;
  eMenuCategory menuCategory;
  eMenuSortMode menuSortMode;
  eMenuOrientation menuOrientation;
  cOsdMenu *subMenu;
  const char *helpRed, *helpGreen, *helpYellow, *helpBlue;
  bool helpDisplayed;
  char *status;
  int digit;
  bool hasHotkeys;
  bool active;
  void SetActive(bool Active);
  void DisplayHelp(bool Force = false);
  void DisplayNoStatus(void);
protected:
  void SetDisplayMenu(void);
  cSkinDisplayMenu *DisplayMenu(void) { return displayMenu; }
  const char *hk(const char *s);
  void SetCols(int c0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0);
  void SetHasHotkeys(bool HasHotkeys = true);
  virtual void Clear(void);
  const char *Title(void) { return title; }
  bool SelectableItem(int idx);
  void SetCurrent(cOsdItem *Item);
  void RefreshCurrent(void);
  void DisplayCurrent(bool Current);
  void DisplayItem(cOsdItem *Item);
  void CursorUp(void);
  void CursorDown(void);
  void PageUp(void);
  void PageDown(void);
  void Mark(void);
  eOSState HotKey(eKeys Key);
  eOSState AddSubMenu(cOsdMenu *SubMenu);
  eOSState CloseSubMenu(bool ReDisplay = true);
  bool HasSubMenu(void) { return subMenu; }
  cOsdMenu *SubMenu(void) { return subMenu; }
  void SetStatus(const char *s);
  void SetTitle(const char *Title);
  void SetHelp(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  virtual void Del(int Index);
public:
  cOsdMenu(const char *Title, int c0 = 0, int c1 = 0, int c2 = 0, int c3 = 0, int c4 = 0);
  virtual ~cOsdMenu() override;
  virtual bool NeedsFastResponse(void) override { return subMenu ? subMenu->NeedsFastResponse() : cOsdObject::NeedsFastResponse(); }
  void SetMenuCategory(eMenuCategory MenuCategory);
  void SetMenuSortMode(eMenuSortMode MenuSortMode);
  int Current(void) const { return current; }
  void Add(cOsdItem *Item, bool Current = false, cOsdItem *After = NULL);
  void Ins(cOsdItem *Item, bool Current = false, cOsdItem *Before = NULL);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

#endif //__OSDBASE_H
