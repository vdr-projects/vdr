/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 1.23 2002/03/29 16:34:03 kls Exp $
 */

#include "osd.h"
#include <string.h>
#include "i18n.h"

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
  delete text;
}

void cOsdItem::SetText(const char *Text, bool Copy)
{
  delete text;
  text = Copy ? strdup(Text) : Text;
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
  delete title;
  delete subMenu;
  delete status;
  Interface->Clear();
  Interface->Close();
}

const char *cOsdMenu::hk(const char *s)
{
  static char buffer[32];
  if (s && hasHotkeys) {
     if (digit == 0 && '1' <= *s && *s <= '9' && *(s + 1) == ' ')
        digit = 10; // prevents automatic hotkeys - input already has them
     if (digit < 9) {
        snprintf(buffer, sizeof(buffer), " %d %s", ++digit, s);
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
  delete status;
  status = s ? strdup(s) : NULL;
  if (visible)
     Interface->Status(status);
}

void cOsdMenu::SetTitle(const char *Title, bool ShowDate)
{
  delete title;
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

void cOsdMenu::Add(cOsdItem *Item, bool Current)
{
  cList<cOsdItem>::Add(Item);
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
         if (item)
            item->Display(i - first, i == current ? clrBlack : clrWhite, i == current ? clrCyan : clrBackground);
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
  if (item)
     item->Display(current - first, Current ? clrBlack : clrWhite, Current ? clrCyan : clrBackground);
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
  if (Count() <= MAXOSDITEMS)
     return;
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
  if (Count() <= MAXOSDITEMS)
     return;
  current += MAXOSDITEMS;
  first += MAXOSDITEMS;
  if (current > Count() - 1) {
     current = Count() - 1;
     first = Count() - MAXOSDITEMS;
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
            return ProcessKey(kOk);
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
  return osContinue; // convenience return value (see cMenuMain)
}

eOSState cOsdMenu::ProcessKey(eKeys Key)
{
  if (subMenu) {
     eOSState state = subMenu->ProcessKey(Key);
     if (state == osBack) {
        delete subMenu;
        subMenu = NULL;
        RefreshCurrent();
        Display();
        state = osContinue;
        }
     return state;
     }

  cOsdItem *item = Get(current);
  if (marked < 0 && item) {
     eOSState state = item->ProcessKey(Key);
     if (state != osUnknown)
        return state;
     }
  switch (Key) {
    case k1...k9: if (hasHotkeys)
                     return HotKey(Key);
                  break;
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

