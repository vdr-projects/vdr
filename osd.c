/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 1.43 2003/06/04 16:13:00 kls Exp $
 */

#include "osd.h"
#include <string.h>
#include "device.h"
#include "i18n.h"
#include "status.h"

// --- cOsd ------------------------------------------------------------------

#ifdef DEBUG_OSD
  WINDOW *cOsd::window = NULL;
  int cOsd::colorPairs[MaxColorPairs] = { 0 };
#else
  cOsdBase *cOsd::osd = NULL;
#endif
  int cOsd::cols = 0;
  int cOsd::rows = 0;

void cOsd::Initialize(void)
{
#if defined(DEBUG_OSD)
  initscr();
  start_color();
  leaveok(stdscr, true);
#endif
}

void cOsd::Shutdown(void)
{
  Close();
#if defined(DEBUG_OSD)
  endwin();
#endif
}

#ifdef DEBUG_OSD
void cOsd::SetColor(eDvbColor colorFg, eDvbColor colorBg)
{
  int color = (colorBg << 16) | colorFg | 0x80000000;
  for (int i = 0; i < MaxColorPairs; i++) {
      if (!colorPairs[i]) {
         colorPairs[i] = color;
         init_pair(i + 1, colorFg, colorBg);
         wattrset(window, COLOR_PAIR(i + 1));
         break;
         }
      else if (color == colorPairs[i]) {
         wattrset(window, COLOR_PAIR(i + 1));
         break;
         }
      }
}
#endif

cOsdBase *cOsd::OpenRaw(int x, int y)
{
#ifdef DEBUG_OSD
  return NULL;
#else
  return osd ? NULL : cDevice::PrimaryDevice()->NewOsd(x, y);
#endif
}

void cOsd::Open(int w, int h)
{
  int d = (h < 0) ? Setup.OSDheight + h : 0;
  h = abs(h);
  cols = w;
  rows = h;
#ifdef DEBUG_OSD
  window = subwin(stdscr, h, w, d, (Setup.OSDwidth - w) / 2);
  syncok(window, true);
  #define B2C(b) (((b) * 1000) / 255)
  #define SETCOLOR(n, r, g, b, o) init_color(n, B2C(r), B2C(g), B2C(b))
  //XXX
  SETCOLOR(clrBackground,  0x00, 0x00, 0x00, 127); // background 50% gray
  SETCOLOR(clrBlack,       0x00, 0x00, 0x00, 255);
  SETCOLOR(clrRed,         0xFC, 0x14, 0x14, 255);
  SETCOLOR(clrGreen,       0x24, 0xFC, 0x24, 255);
  SETCOLOR(clrYellow,      0xFC, 0xC0, 0x24, 255);
  SETCOLOR(clrBlue,        0x00, 0x00, 0xFC, 255);
  SETCOLOR(clrCyan,        0x00, 0xFC, 0xFC, 255);
  SETCOLOR(clrMagenta,     0xB0, 0x00, 0xFC, 255);
  SETCOLOR(clrWhite,       0xFC, 0xFC, 0xFC, 255);
#else
  w *= charWidth;
  h *= lineHeight;
  d *= lineHeight;
  int x = (720 - w + charWidth) / 2; //TODO PAL vs. NTSC???
  int y = (576 - Setup.OSDheight * lineHeight) / 2 + d;
  //XXX
  osd = OpenRaw(x, y);
  //XXX TODO this should be transferred to the places where the individual windows are requested (there's too much detailed knowledge here!)
  if (!osd)
     return;
  if (h / lineHeight == 5) { //XXX channel display
     osd->Create(0,              0, w, h, 4);
     }
  else if (h / lineHeight == 1) { //XXX info display
     osd->Create(0,              0, w, h, 4);
     }
  else if (d == 0) { //XXX full menu
     osd->Create(0,                            0, w,                         lineHeight, 2);
     osd->Create(0,                   lineHeight, w, (Setup.OSDheight - 3) * lineHeight, 2);
     osd->AddColor(clrBackground);
     osd->AddColor(clrCyan);
     osd->AddColor(clrWhite);
     osd->AddColor(clrBlack);
     osd->Create(0, (Setup.OSDheight - 2) * lineHeight, w,               2 * lineHeight, 4);
     }
  else { //XXX progress display
     /*XXX
     osd->Create(0,              0, w, lineHeight, 1);
     osd->Create(0,     lineHeight, w, lineHeight, 2, false);
     osd->Create(0, 2 * lineHeight, w, lineHeight, 1);
     XXX*///XXX some pixels are not drawn correctly with lower bpp values
     osd->Create(0,              0, w, h, 4);
     }
#endif
}

void cOsd::Close(void)
{
#ifdef DEBUG_OSD
  if (window) {
     delwin(window);
     window = 0;
     }
#else
  delete osd;
  osd = NULL;
#endif
}

void cOsd::Clear(void)
{
#ifdef DEBUG_OSD
  SetColor(clrBackground, clrBackground);
  Fill(0, 0, cols, rows, clrBackground);
  refresh();
#else
  if (osd)
     osd->Clear();
#endif
}

void cOsd::Fill(int x, int y, int w, int h, eDvbColor color)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(color, color);
  for (int r = 0; r < h; r++) {
      wmove(window, y + r, x); // ncurses wants 'y' before 'x'!
      whline(window, ' ', w);
      }
  wsyncup(window); // shouldn't be necessary because of 'syncok()', but w/o it doesn't work
#else
  if (osd)
     osd->Fill(x * charWidth, y * lineHeight, (x + w) * charWidth - 1, (y + h) * lineHeight - 1, color);
#endif
}

void cOsd::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
#ifndef DEBUG_OSD
  if (osd)
     osd->SetBitmap(x, y, Bitmap);
#endif
}

void cOsd::ClrEol(int x, int y, eDvbColor color)
{
  Fill(x, y, cols - x, 1, color);
}

int cOsd::CellWidth(void)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return charWidth;
#endif
}

int cOsd::LineHeight(void)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return lineHeight;
#endif
}

int cOsd::Width(unsigned char c)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return osd ? osd->Width(c) : 1;
#endif
}

int cOsd::WidthInCells(const char *s)
{
#ifdef DEBUG_OSD
  return strlen(s);
#else
  return osd ? (osd->Width(s) + charWidth - 1) / charWidth : strlen(s);
#endif
}

eDvbFont cOsd::SetFont(eDvbFont Font)
{
#ifdef DEBUG_OSD
  return Font;
#else
  return osd ? osd->SetFont(Font) : Font;
#endif
}

void cOsd::Text(int x, int y, const char *s, eDvbColor colorFg, eDvbColor colorBg)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(colorFg, colorBg);
  wmove(window, y, x); // ncurses wants 'y' before 'x'!
  waddnstr(window, s, cols - x);
#else
  if (osd)
     osd->Text(x * charWidth, y * lineHeight, s, colorFg, colorBg);
#endif
}

void cOsd::Flush(void)
{
#ifdef DEBUG_OSD
  refresh();
#else
  if (osd)
     osd->Flush();
#endif
}

// --- cOsdItem --------------------------------------------------------------

cOsdItem::cOsdItem(eOSState State)
{
  text = NULL;
  offset = -1;
  state = State;
  fresh = false;
  userColor = false;
  fgColor = clrWhite;
  bgColor = clrBackground;
}

cOsdItem::cOsdItem(const char *Text, eOSState State)
{
  text = NULL;
  offset = -1;
  state = State;
  fresh = false;
  userColor = false;
  fgColor = clrWhite;
  bgColor = clrBackground;
  SetText(Text);
}

cOsdItem::~cOsdItem()
{
  free(text);
}

void cOsdItem::SetText(const char *Text, bool Copy)
{
  free(text);
  text = Copy ? strdup(Text) : (char *)Text; // text assumes ownership!
}

void cOsdItem::SetColor(eDvbColor FgColor, eDvbColor BgColor)
{
  userColor = true;
  fgColor = FgColor; 
  bgColor = BgColor; 
}

void cOsdItem::Display(int Offset, eDvbColor FgColor, eDvbColor BgColor)
{
  if (Offset < 0) {
     FgColor = clrBlack;
     BgColor = clrCyan;
     }
  fresh |= Offset >= 0;
  if (Offset >= 0)
     offset = Offset;
  if (offset >= 0)
     Interface->WriteText(0, offset + 2, text, userColor ? fgColor : FgColor, userColor ? bgColor : BgColor);
}

eOSState cOsdItem::ProcessKey(eKeys Key)
{
  return Key == kOk ? state : osUnknown;
}

// --- cOsdMenu --------------------------------------------------------------

cOsdMenu::cOsdMenu(const char *Title, int c0, int c1, int c2, int c3, int c4)
{
  isMenu = true;
  digit = 0;
  hasHotkeys = false;
  visible = false;
  title = NULL;
  SetTitle(Title);
  cols[0] = c0;
  cols[1] = c1;
  cols[2] = c2;
  cols[3] = c3;
  cols[4] = c4;
  first = 0;
  current = marked = -1;
  subMenu = NULL;
  helpRed = helpGreen = helpYellow = helpBlue = NULL;
  status = NULL;
  Interface->Open();
}

cOsdMenu::~cOsdMenu()
{
  free(title);
  delete subMenu;
  free(status);
  Interface->Clear();
  Interface->Close();
}

const char *cOsdMenu::hk(const char *s)
{
  static char buffer[64];
  if (s && hasHotkeys) {
     if (digit == 0 && '1' <= *s && *s <= '9' && *(s + 1) == ' ')
        digit = -1; // prevents automatic hotkeys - input already has them
     if (digit >= 0) {
        digit++;
        snprintf(buffer, sizeof(buffer), " %c %s", (digit < 10) ? '0' + digit : ' ' , s);
        s = buffer;
        }
     }
  return s;
}

void cOsdMenu::SetHasHotkeys(void)
{
  hasHotkeys = true;
  digit = 0;
}

void cOsdMenu::SetStatus(const char *s)
{
  free(status);
  status = s ? strdup(s) : NULL;
  if (visible)
     Interface->Status(status);
}

void cOsdMenu::SetTitle(const char *Title, bool ShowDate)
{
  free(title);
  if (ShowDate)
     asprintf(&title, "%s\t%s", Title, DayDateTime(time(NULL)));
  else
     title = strdup(Title);
}

void cOsdMenu::SetHelp(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  // strings are NOT copied - must be constants!!!
  helpRed    = Red;
  helpGreen  = Green;
  helpYellow = Yellow;
  helpBlue   = Blue;
  if (visible)
     Interface->Help(helpRed, helpGreen, helpYellow, helpBlue);
}

void cOsdMenu::Del(int Index)
{
  cList<cOsdItem>::Del(Get(Index));
  if (current == Count())
     current--;
  if (Index == first && first > 0)
     first--;
}

void cOsdMenu::Add(cOsdItem *Item, bool Current, cOsdItem *After)
{
  cList<cOsdItem>::Add(Item, After);
  if (Current)
     current = Item->Index();
}

void cOsdMenu::Ins(cOsdItem *Item, bool Current, cOsdItem *Before)
{
  cList<cOsdItem>::Ins(Item, Before);
  if (Current)
     current = Item->Index();
}

void cOsdMenu::Display(void)
{
  if (subMenu) {
     subMenu->Display();
     return;
     }
  visible = true;
  Interface->Clear();
  Interface->SetCols(cols);
  Interface->Title(title);
  Interface->Help(helpRed, helpGreen, helpYellow, helpBlue);
  int count = Count();
  if (count > 0) {
     for (int i = 0; i < count; i++) {
        cOsdItem *item = Get(i);
        if (item)
           cStatus::MsgOsdItem(item->Text(), i);
        }
     if (current < 0)
        current = 0; // just for safety - there HAS to be a current item!
     int n = 0;
     if (current - first >= MAXOSDITEMS) {
        first = current - MAXOSDITEMS / 2;
        if (first + MAXOSDITEMS > count)
           first = count - MAXOSDITEMS;
        if (first < 0)
           first = 0;
        }
     for (int i = first; i < count; i++) {
         cOsdItem *item = Get(i);
         if (item) {
            item->Display(i - first, i == current ? clrBlack : clrWhite, i == current ? clrCyan : clrBackground);
            if (i == current)
               cStatus::MsgOsdCurrentItem(item->Text());
            }
         if (++n == MAXOSDITEMS) //TODO get this from Interface!!!
            break;
         }
     }
  if (!isempty(status))
     Interface->Status(status);
}

void cOsdMenu::SetCurrent(cOsdItem *Item)
{
  current = Item ? Item->Index() : -1;
}

void cOsdMenu::RefreshCurrent(void)
{
  cOsdItem *item = Get(current);
  if (item)
     item->Set();
}

void cOsdMenu::DisplayCurrent(bool Current)
{
  cOsdItem *item = Get(current);
  if (item) {
     item->Display(current - first, Current ? clrBlack : clrWhite, Current ? clrCyan : clrBackground);
     if (Current)
        cStatus::MsgOsdCurrentItem(item->Text());
     }
}

void cOsdMenu::Clear(void)
{
  first = 0;
  current = marked = -1;
  cList<cOsdItem>::Clear();
}

bool cOsdMenu::SpecialItem(int idx)
{
  cOsdItem *item = Get(idx);
  return item && item->HasUserColor();
}

void cOsdMenu::CursorUp(void)
{
  if (current > 0) {
     int tmpCurrent = current;
     while (--tmpCurrent >= 0 && SpecialItem(tmpCurrent));
     if (tmpCurrent < 0)
        return;
     if (tmpCurrent >= first)
        DisplayCurrent(false);
     current = tmpCurrent;
     if (current < first) {
        first = first > MAXOSDITEMS - 1 ? first - (MAXOSDITEMS - 1) : 0;
        if (Setup.MenuScrollPage)
           current = SpecialItem(first) ? first + 1 : first;
        Display();
        }
     else
        DisplayCurrent(true);
     }
}

void cOsdMenu::CursorDown(void)
{
  int last = Count() - 1;
  int lastOnScreen = first + MAXOSDITEMS - 1;

  if (current < last) {
     int tmpCurrent = current;
     while (++tmpCurrent <= last && SpecialItem(tmpCurrent));
     if (tmpCurrent > last)
        return;
     if (tmpCurrent <= lastOnScreen)
        DisplayCurrent(false);
     current = tmpCurrent;
     if (current > lastOnScreen) {
        first += MAXOSDITEMS - 1;
        lastOnScreen = first + MAXOSDITEMS - 1;
        if (lastOnScreen > last) {
           first = last - (MAXOSDITEMS - 1);
           lastOnScreen = last;
           }
        if (Setup.MenuScrollPage)
           current = SpecialItem(lastOnScreen) ? lastOnScreen - 1 : lastOnScreen;
        Display();
        }
     else
        DisplayCurrent(true);
     }
}

void cOsdMenu::PageUp(void)
{
  current -= MAXOSDITEMS;
  first -= MAXOSDITEMS;
  if (first < 0)
     first = current = 0;
  if (SpecialItem(current)) {
     current -= (current > 0) ? 1 : -1;
     first = min(first, current - 1);
     }
  Display();
  DisplayCurrent(true);
}

void cOsdMenu::PageDown(void) 
{
  current += MAXOSDITEMS;
  first += MAXOSDITEMS;
  if (current > Count() - 1) {
     current = Count() - 1;
     first = max(0, Count() - MAXOSDITEMS);
     }
  if (SpecialItem(current)) {
     current += (current < Count() - 1) ? 1 : -1;
     first = max(first, current - MAXOSDITEMS);
     }
  Display();
  DisplayCurrent(true);
}

void cOsdMenu::Mark(void)
{
  if (Count() && marked < 0) {
     marked = current;
     SetStatus(tr("Up/Dn for new location - OK to move"));
     }
}

eOSState cOsdMenu::HotKey(eKeys Key)
{
  for (cOsdItem *item = First(); item; item = Next(item)) {
      const char *s = item->Text();
      if (s && (s = skipspace(s)) != NULL) {
         if (*s == Key - k1 + '1') {
            current = item->Index();
            cRemote::Put(kOk, true);
            break;
            }
         }
      }
  return osContinue;
}

eOSState cOsdMenu::AddSubMenu(cOsdMenu *SubMenu)
{
  delete subMenu;
  subMenu = SubMenu;
  subMenu->Display();
  return osContinue; // convenience return value
}

eOSState cOsdMenu::CloseSubMenu()
{
  delete subMenu;
  subMenu = NULL;
  RefreshCurrent();
  Display();
  return osContinue; // convenience return value
}

eOSState cOsdMenu::ProcessKey(eKeys Key)
{
  if (subMenu) {
     eOSState state = subMenu->ProcessKey(Key);
     if (state == osBack)
        return CloseSubMenu();
     return state;
     }

  cOsdItem *item = Get(current);
  if (marked < 0 && item) {
     eOSState state = item->ProcessKey(Key);
     if (state != osUnknown)
        return state;
     }
  switch (Key) {
    case k1...k9: return hasHotkeys ? HotKey(Key) : osUnknown;
    case kUp|k_Repeat:
    case kUp:   CursorUp();   break;
    case kDown|k_Repeat:
    case kDown: CursorDown(); break;
    case kLeft|k_Repeat:
    case kLeft: PageUp(); break;
    case kRight|k_Repeat:
    case kRight: PageDown(); break;
    case kBack: return osBack;
    case kOk:   if (marked >= 0) {
                   SetStatus(NULL);
                   if (marked != current)
                      Move(marked, current);
                   marked = -1;
                   break;
                   }
                // else run into default
    default: if (marked < 0)
                return osUnknown;
    }
  return osContinue;
}

