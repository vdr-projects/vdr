/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 1.11 2000/11/01 11:21:51 kls Exp $
 */

#include "osd.h"
#include <assert.h>
#include <string.h>

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
  visible = false;
  title = strdup(Title);
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

void cOsdMenu::SetStatus(const char *s)
{
  delete status;
  status = s ? strdup(s) : NULL;
  if (visible)
     Interface->Status(status);
}

void cOsdMenu::SetTitle(const char *Title, bool Copy)
{
  delete title;
  title = Copy ? strdup(Title) : Title;
}

void cOsdMenu::SetHelp(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  // strings are NOT copied - must be constants!!!
  helpRed    = Red;
  helpGreen  = Green;
  helpYellow = Yellow;
  helpBlue   = Blue;
  if (visible)
     Display();
     //XXX Interface->Help(helpRed, helpGreen, helpYellow, helpBlue);
     //XXX must clear unused button areas!
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

void cOsdMenu::Mark(void)
{
  if (Count() && marked < 0) {
     marked = current;
     SetStatus("Up/Dn for new location - OK to move");
     }
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
    case kUp|k_Repeat:
    case kUp:   CursorUp();   break;
    case kDown|k_Repeat:
    case kDown: CursorDown(); break;
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

