/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "osd.h"
#include <assert.h>
#include <string.h>

// --- cOsdItem --------------------------------------------------------------

cOsdItem::cOsdItem(eOSStatus Status)
{
  text = NULL;
  offset = -1;
  status = Status;
  fresh = false;
}

cOsdItem::cOsdItem(char *Text, eOSStatus Status)
{
  text = NULL;
  offset = -1;
  status = Status;
  fresh = false;
  SetText(Text);
}

cOsdItem::~cOsdItem()
{
  delete text;
}

void cOsdItem::SetText(char *Text, bool Copy)
{
  delete text;
  text = Copy ? strdup(Text) : Text;
}

void cOsdItem::Display(int Offset, bool Current)
{
  fresh |= Offset >= 0;
  Current |= Offset < 0;
  if (Offset >= 0)
     offset = Offset;
  //TODO current if Offset == -1 ???
  if (offset >= 0)
     Interface.WriteText(0, offset + 2, text, Current);
}

eOSStatus cOsdItem::ProcessKey(eKeys Key)
{
  return Key == kOk ? status : osUnknown;
}

// --- cOsdMenu --------------------------------------------------------------

cOsdMenu::cOsdMenu(char *Title, int c0, int c1, int c2, int c3, int c4)
{
  title = strdup(Title);
  cols[0] = c0;
  cols[1] = c1;
  cols[2] = c2;
  cols[3] = c3;
  cols[4] = c4;
  first = count = 0;
  current = -1;
  subMenu = NULL;
  Interface.Open();
}

cOsdMenu::~cOsdMenu()
{
  delete title;
  delete subMenu;
  Interface.Clear();
  Interface.Close();
}

void cOsdMenu::Add(cOsdItem *Item, bool Current)
{
  cList<cOsdItem>::Add(Item);
  count++;
  if (Current && current < 0)
     current = Item->Index();
}

void cOsdMenu::Display(void)
{
  Interface.Clear();
  Interface.SetCols(cols);
  Interface.WriteText(0, 0, title);
  if (current < 0 && count)
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
         item->Display(i - first, i == current);
      if (++n == MAXOSDITEMS) //TODO get this from Interface!!!
         break;
      }
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
     item->Display(current - first, Current);
}

void cOsdMenu::CursorUp(void)
{
  if (current > 0) {
     DisplayCurrent(false);
     if (--current < first) {
        first -= MAXOSDITEMS;
        if (first < 0)
           first = 0;
        Display();
        }
     else
        DisplayCurrent(true);
     }
}

void cOsdMenu::CursorDown(void)
{
  if (current < count - 1) {
     DisplayCurrent(false);
     if (++current >= first + MAXOSDITEMS) {
        first += MAXOSDITEMS;
        if (first > count - MAXOSDITEMS)
           first = count - MAXOSDITEMS;
        Display();
        }
     else
        DisplayCurrent(true);
     }
}

eOSStatus cOsdMenu::AddSubMenu(cOsdMenu *SubMenu)
{
  delete subMenu;
  subMenu = SubMenu;
  subMenu->Display();
  return osContinue; // convenience return value (see cMenuMain)
}

eOSStatus cOsdMenu::ProcessKey(eKeys Key)
{
  if (subMenu) {
     eOSStatus status = subMenu->ProcessKey(Key);
     if (status == osBack) {
        delete subMenu;
        subMenu = NULL;
        RefreshCurrent();
        Display();
        status = osContinue;
        }
     return status;
     }

  cOsdItem *item = Get(current);
  if (item) {
     eOSStatus status = item->ProcessKey(Key);
     if (status != osUnknown)
        return status;
     }
  switch (Key) {
    case kUp:   CursorUp();   break;
    case kDown: CursorDown(); break;
    case kBack: return osBack;
    default: return osUnknown;
    }
  return osContinue;
}

