/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 1.40 2003/09/14 10:59:22 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#if defined(DEBUG_OSD)
#include <ncurses.h>
#endif
#include "config.h"
#include "osdbase.h"
#include "interface.h"
#include "osdbase.h"
#include "tools.h"

#define MAXOSDITEMS (Setup.OSDheight - 4)

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

class cOsd {
private:
  enum { charWidth  = 12, // average character width
         lineHeight = 27  // smallest text height
       };
#ifdef DEBUG_OSD
  static WINDOW *window;
  enum { MaxColorPairs = 16 };
  static int colorPairs[MaxColorPairs];
  static void SetColor(eDvbColor colorFg, eDvbColor colorBg = clrBackground);
#else
  static cOsdBase *osd;
#endif
  static int cols, rows;
public:
  static void Initialize(void);
  static void Shutdown(void);
  static cOsdBase *OpenRaw(int x, int y);
       // Returns a raw OSD without any predefined windows or colors.
       // If the "normal" OSD is currently in use, NULL will be returned.
       // The caller must delete this object before the "normal" OSD is used again!
  static void Open(int w, int h);
  static void Close(void);
  static void Clear(void);
  static void Fill(int x, int y, int w, int h, eDvbColor color = clrBackground);
  static void SetBitmap(int x, int y, const cBitmap &Bitmap);
  static void ClrEol(int x, int y, eDvbColor color = clrBackground);
  static int CellWidth(void);
  static int LineHeight(void);
  static int Width(unsigned char c);
  static int WidthInCells(const char *s);
  static eDvbFont SetFont(eDvbFont Font);
  static void Text(int x, int y, const char *s, eDvbColor colorFg = clrWhite, eDvbColor colorBg = clrBackground);
  static void Flush(void);
  };

class cOsdItem : public cListObject {
private:
  char *text;
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
  friend class cOsdMenu;
private:
  bool isMenu;
protected:
  bool needsFastResponse;
public:
  cOsdObject(bool FastResponse = false) { isMenu = false; needsFastResponse = FastResponse; }
  virtual ~cOsdObject() {}
  int Width(void) { return Interface->Width(); }
  int Height(void) { return Interface->Height(); }
  bool NeedsFastResponse(void) { return needsFastResponse; }
  bool IsMenu(void) { return isMenu; }
  virtual void Show(void) {}
  virtual eOSState ProcessKey(eKeys Key) { return osUnknown; }
  };

class cOsdMenu : public cOsdObject, public cList<cOsdItem> {
private:
  char *title;
  int cols[cInterface::MaxCols];
  int first, current, marked;
  cOsdMenu *subMenu;
  const char *helpRed, *helpGreen, *helpYellow, *helpBlue;
  char *status;
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
  eOSState CloseSubMenu();
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
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

#endif //__OSD_H
