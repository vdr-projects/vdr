/*
 * osdbase.c: Basic interface to the On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osdbase.c 5.4 2025/01/29 10:35:25 kls Exp $
 */

#include "osdbase.h"
#include <string.h>
#include "device.h"
#include "i18n.h"
#include "menuitems.h"
#include "remote.h"
#include "status.h"

// --- cOsdItem --------------------------------------------------------------

cOsdItem::cOsdItem(eOSState State)
{
  text = NULL;
  state = State;
  selectable = true;
  fresh = true;
}

cOsdItem::cOsdItem(const char *Text, eOSState State, bool Selectable)
{
  text = NULL;
  state = State;
  selectable = Selectable;
  fresh = true;
  SetText(Text);
}

cOsdItem::~cOsdItem()
{
  free(text);
}

void cOsdItem::SetText(const char *Text, bool Copy)
{
  free(text);
  text = Copy ? strdup(Text ? Text : "") : (char *)Text; // text assumes ownership!
}

void cOsdItem::SetSelectable(bool Selectable)
{
  selectable = Selectable;
}

void cOsdItem::SetFresh(bool Fresh)
{
  fresh = Fresh;
}

void cOsdItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

eOSState cOsdItem::ProcessKey(eKeys Key)
{
  return Key == kOk ? state : osUnknown;
}

// --- cOsdObject ------------------------------------------------------------

void cOsdObject::Show(void)
{
  if (isMenu)
     ((cOsdMenu *)this)->Display();
}

// --- cOsdMenu --------------------------------------------------------------

cSkinDisplayMenu *cOsdMenu::displayMenu = NULL;
int cOsdMenu::displayMenuCount = 0;
int cOsdMenu::osdState = 0;

cOsdMenu::cOsdMenu(const char *Title, int c0, int c1, int c2, int c3, int c4)
{
  isMenu = true;
  digit = 0;
  hasHotkeys = false;
  displayMenuItems = 0;
  title = NULL;
  menuCategory = mcUnknown;
  menuSortMode = msmUnknown;
  menuOrientation = moVertical;
  SetTitle(Title);
  SetCols(c0, c1, c2, c3, c4);
  first = 0;
  lastOffset = -1;
  current = marked = -1;
  subMenu = NULL;
  helpRed = helpGreen = helpYellow = helpBlue = NULL;
  helpDisplayed = false;
  status = NULL;
  if (!displayMenuCount++) {
     cOsdProvider::OsdSizeChanged(osdState); // to get the current state
     SetDisplayMenu();
     }
}

cOsdMenu::~cOsdMenu()
{
  free(title);
  delete subMenu;
  free(status);
  displayMenu->Clear();
  cStatus::MsgOsdClear();
  if (!--displayMenuCount)
     DELETENULL(displayMenu);
}

void cOsdMenu::SetMenuCategory(eMenuCategory MenuCategory)
{
  menuCategory = MenuCategory;
}

void cOsdMenu::SetMenuSortMode(eMenuSortMode MenuSortMode)
{
  menuSortMode = MenuSortMode;
}

void cOsdMenu::SetDisplayMenu(void)
{
  if (displayMenu) {
     displayMenu->Clear();
     delete displayMenu;
     }
  displayMenu = Skins.Current()->DisplayMenu();
}

const char *cOsdMenu::hk(const char *s)
{
  static cString buffer;
  if (s && hasHotkeys) {
     if (digit == 0 && '1' <= *s && *s <= '9' && *(s + 1) == ' ')
        digit = -1; // prevents automatic hotkeys - input already has them
     if (digit >= 0) {
        digit++;
        buffer = cString::sprintf(" %c %s", (digit < 10) ? '0' + digit : ' ' , s);
        s = buffer;
        }
     }
  return s;
}

void cOsdMenu::SetCols(int c0, int c1, int c2, int c3, int c4)
{
  cols[0] = c0;
  cols[1] = c1;
  cols[2] = c2;
  cols[3] = c3;
  cols[4] = c4;
}

void cOsdMenu::SetHasHotkeys(bool HasHotkeys)
{
  hasHotkeys = HasHotkeys;
  digit = 0;
}

void cOsdMenu::SetStatus(const char *s)
{
  free(status);
  status = s ? strdup(s) : NULL;
  displayMenu->SetMessage(mtStatus, s);
}

void cOsdMenu::SetTitle(const char *Title)
{
  free(title);
  title = strdup(Title);
}

void cOsdMenu::DisplayHelp(bool Force)
{
  if (!helpDisplayed || Force) {
     displayMenu->SetButtons(helpRed, helpGreen, helpYellow, helpBlue);
     cStatus::MsgOsdHelpKeys(helpRed, helpGreen, helpYellow, helpBlue);
     helpDisplayed = true;
     }
}

void cOsdMenu::SetHelp(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  // strings are NOT copied - must be constants!!!
  helpRed    = Red;
  helpGreen  = Green;
  helpYellow = Yellow;
  helpBlue   = Blue;
  DisplayHelp(true);
}

void cOsdMenu::Del(int Index)
{
  cList<cOsdItem>::Del(Get(Index));
  int count = Count();
  while (current < count && !SelectableItem(current))
        current++;
  if (current == count) {
     while (current > 0 && !SelectableItem(current))
           current--;
     }
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
  if (cOsdProvider::OsdSizeChanged(osdState))
     SetDisplayMenu();
  displayMenu->SetMessage(mtStatus, NULL);
  displayMenu->Clear();
  cStatus::MsgOsdClear();
  if (menuCategory != displayMenu->MenuCategory())
     displayMenu->SetMenuCategory(menuCategory);
  displayMenu->SetMenuSortMode(menuSortMode);
  menuOrientation = displayMenu->MenuOrientation();
  displayMenuItems = displayMenu->MaxItems();
  displayMenu->SetTabs(cols[0], cols[1], cols[2], cols[3], cols[4]);//XXX
  displayMenu->SetTitle(title);
  cStatus::MsgOsdTitle(title);
  DisplayHelp(true);
  int count = Count();
  if (count > 0) {
     int ni = 0;
     for (cOsdItem *item = First(); item; item = Next(item)) {
         cStatus::MsgOsdItem(item->Text(), ni++, item->Selectable());
         if (current < 0 && item->Selectable())
            current = item->Index();
         }
     if (current < 0)
        current = 0; // just for safety - there HAS to be a current item!
     first = max(0, min(first, max(0, count - displayMenuItems))); // in case the menu size has changed
     if (current - first >= displayMenuItems || current < first) {
        first = current - displayMenuItems / 2;
        if (first + displayMenuItems > count)
           first = count - displayMenuItems;
        if (first < 0)
           first = 0;
        }
     int i = first;
     int n = 0;
     for (cOsdItem *item = Get(first); item; item = Next(item)) {
         bool CurrentSelectable = (i == current) && item->Selectable();
         item->SetMenuItem(displayMenu, i - first, CurrentSelectable, item->Selectable());
         if (CurrentSelectable)
            cStatus::MsgOsdCurrentItem(item->Text());
         if (++n == displayMenuItems)
            break;
         i++;
         }
     }
  displayMenu->SetScrollbar(count, first);
  if (!isempty(status))
     displayMenu->SetMessage(mtStatus, status);
}

void cOsdMenu::SetCurrent(cOsdItem *Item)
{
  current = Item ? Item->Index() : -1;
  if (current >= 0 && lastOffset >= 0)
     first = max(0, current - lastOffset);
  lastOffset = -1;
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
     item->SetMenuItem(displayMenu, current - first, Current && item->Selectable(), item->Selectable());
     if (Current && item->Selectable())
        cStatus::MsgOsdCurrentItem(item->Text());
     if (!Current)
        item->SetFresh(true); // leaving the current item resets 'fresh'
     if (cMenuEditItem *MenuEditItem = dynamic_cast<cMenuEditItem *>(item)) {
        if (!MenuEditItem->DisplayHelp(Current))
           DisplayHelp();
        else
           helpDisplayed = false;
        }
     }
}

void cOsdMenu::DisplayItem(cOsdItem *Item)
{
  if (Item) {
     int Index = Item->Index();
     int Offset = Index - first;
     if (Offset >= 0 && Offset < first + displayMenuItems) {
        bool Current = Index == current;
        Item->SetMenuItem(displayMenu, Offset, Current && Item->Selectable(), Item->Selectable());
        if (Current && Item->Selectable())
           cStatus::MsgOsdCurrentItem(Item->Text());
        }
     }
}

void cOsdMenu::Clear(void)
{
  if (marked >= 0)
     SetStatus(NULL);
  if (current >= 0)
     lastOffset = (current > first) ? current - first : 0;
  first = 0;
  current = marked = -1;
  cList<cOsdItem>::Clear();
}

bool cOsdMenu::SelectableItem(int idx)
{
  cOsdItem *item = Get(idx);
  return item && item->Selectable();
}

void cOsdMenu::CursorUp(void)
{
  int tmpCurrent = current;
  int lastOnScreen = first + displayMenuItems - 1;
  int last = Count() - 1;
  if (last < 0)
     return;
  while (--tmpCurrent != current) {
        if (tmpCurrent < 0) {
           if (first > 0) {
              // make non-selectable items at the beginning visible:
              first = 0;
              Display();
              return;
              }
           if (Setup.MenuScrollWrap)
              tmpCurrent = last + 1;
           else
              return;
           }
        else if (SelectableItem(tmpCurrent))
           break;
        }
  if (first <= tmpCurrent && tmpCurrent <= lastOnScreen)
     DisplayCurrent(false);
  current = tmpCurrent;
  if (current < first) {
     first = Setup.MenuScrollPage ? max(0, current - displayMenuItems + 1) : current;
     Display();
     }
  else if (current > lastOnScreen) {
     first = max(0, current - displayMenuItems + 1);
     Display();
     }
  else
     DisplayCurrent(true);
}

void cOsdMenu::CursorDown(void)
{
  int tmpCurrent = current;
  int lastOnScreen = first + displayMenuItems - 1;
  int last = Count() - 1;
  if (last < 0)
     return;
  while (++tmpCurrent != current) {
        if (tmpCurrent > last) {
           if (first < last - displayMenuItems) {
              // make non-selectable items at the end visible:
              first = last - displayMenuItems + 1;
              Display();
              return;
              }
           if (Setup.MenuScrollWrap)
              tmpCurrent = -1;
           else
              return;
           }
        else if (SelectableItem(tmpCurrent))
           break;
        }
  if (first <= tmpCurrent && tmpCurrent <= lastOnScreen)
     DisplayCurrent(false);
  current = tmpCurrent;
  if (current > lastOnScreen) {
     first = Setup.MenuScrollPage ? current : max(0, current - displayMenuItems + 1);
     if (first + displayMenuItems > last)
        first = max(0, last - displayMenuItems + 1);
     Display();
     }
  else if (current < first) {
     first = current;
     Display();
     }
  else
     DisplayCurrent(true);
}

void cOsdMenu::PageUp(void)
{
  int oldCurrent = current;
  int oldFirst = first;
  current -= displayMenuItems;
  first -= displayMenuItems;
  int last = Count() - 1;
  if (current < 0)
     current = 0;
  if (first < 0)
     first = 0;
  int tmpCurrent = current;
  while (!SelectableItem(tmpCurrent) && --tmpCurrent >= 0)
        ;
  if (tmpCurrent < 0) {
     tmpCurrent = current;
     while (++tmpCurrent <= last && !SelectableItem(tmpCurrent))
           ;
     }
  current = tmpCurrent <= last ? tmpCurrent : -1;
  if (current >= 0) {
     if (current < first)
        first = current;
     else if (current - first >= displayMenuItems)
        first = current - displayMenuItems + 1;
     }
  if (current != oldCurrent || first != oldFirst)
     Display();
  else if (Setup.MenuScrollWrap)
     CursorUp();
}

void cOsdMenu::PageDown(void)
{
  int oldCurrent = current;
  int oldFirst = first;
  current += displayMenuItems;
  first += displayMenuItems;
  int last = Count() - 1;
  if (current > last)
     current = last;
  if (first + displayMenuItems > last)
     first = max(0, last - displayMenuItems + 1);
  int tmpCurrent = current;
  while (!SelectableItem(tmpCurrent) && ++tmpCurrent <= last)
        ;
  if (tmpCurrent > last) {
     tmpCurrent = current;
     while (--tmpCurrent >= 0 && !SelectableItem(tmpCurrent))
           ;
     }
  current = tmpCurrent > 0 ? tmpCurrent : -1;
  if (current >= 0) {
     if (current < first)
        first = current;
     else if (current - first >= displayMenuItems)
        first = current - displayMenuItems + 1;
     }
  if (current != oldCurrent || first != oldFirst)
     Display();
  else if (Setup.MenuScrollWrap)
     CursorDown();
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
            RefreshCurrent();
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

eOSState cOsdMenu::CloseSubMenu(bool ReDisplay)
{
  delete subMenu;
  subMenu = NULL;
  if (ReDisplay) {
     RefreshCurrent();
     Display();
     }
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
     if (state != osUnknown) {
        if (Key != kNone)
           DisplayCurrent(true);
        return state;
        }
     }
  switch (int(Key)) {
    case k0:      return osUnknown;
    case k1...k9: return hasHotkeys ? HotKey(Key) : osUnknown;
    case kUp|k_Repeat:
    case kUp:     if (menuOrientation == moHorizontal) PageUp();     else CursorUp(); break;
    case kDown|k_Repeat:
    case kDown:   if (menuOrientation == moHorizontal) PageDown();   else CursorDown(); break;
    case kLeft|k_Repeat:
    case kLeft:   if (menuOrientation == moHorizontal) CursorUp();   else PageUp(); break;
    case kRight|k_Repeat:
    case kRight:  if (menuOrientation == moHorizontal) CursorDown(); else PageDown(); break;
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
