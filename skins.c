/*
 * skins.c: The optical appearance of the OSD
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skins.c 1.4 2005/01/14 13:07:19 kls Exp $
 */

#include "skins.h"
#include "interface.h"
#include "status.h"
#include "tools.h"

// --- cSkinDisplay ----------------------------------------------------------

cSkinDisplay *cSkinDisplay::current = NULL;

cSkinDisplay::cSkinDisplay(void)
{
  current = this;
  editableWidth = 100; //XXX
}

cSkinDisplay::~cSkinDisplay()
{
  current = NULL;
}

// --- cSkinDisplayMenu ------------------------------------------------------

cSkinDisplayMenu::cSkinDisplayMenu(void)
{
  SetTabs(0);
}

void cSkinDisplayMenu::SetTabs(int Tab1, int Tab2, int Tab3, int Tab4, int Tab5)
{
  tabs[0] = 0;
  tabs[1] = Tab1 ? tabs[0] + Tab1 : 0;
  tabs[2] = Tab2 ? tabs[1] + Tab2 : 0;
  tabs[3] = Tab3 ? tabs[2] + Tab3 : 0;
  tabs[4] = Tab4 ? tabs[3] + Tab4 : 0;
  tabs[5] = Tab5 ? tabs[4] + Tab5 : 0;
  for (int i = 1; i < MaxTabs; i++)
      tabs[i] *= 12;//XXX average character width of font used for items!!!
}

void cSkinDisplayMenu::Scroll(bool Up, bool Page)
{
  textScroller.Scroll(Up, Page);
}

const char *cSkinDisplayMenu::GetTabbedText(const char *s, int Tab)
{
  if (!s)
     return NULL;
  static char buffer[1000];
  const char *a = s;
  const char *b = strchrnul(a, '\t');
  while (*b && Tab-- > 0) {
        a = b + 1;
        b = strchrnul(a, '\t');
        }
  if (!*b)
     return (Tab <= 0) ? a : NULL;
  unsigned int n = b - a;
  if (n >= sizeof(buffer))
     n = sizeof(buffer) - 1;
  strncpy(buffer, a, n);
  buffer[n] = 0;
  return buffer;
}

// --- cSkinDisplayReplay::cProgressBar --------------------------------------

cSkinDisplayReplay::cProgressBar::cProgressBar(int Width, int Height, int Current, int Total, const cMarks *Marks, tColor ColorSeen, tColor ColorRest, tColor ColorSelected, tColor ColorMark, tColor ColorCurrent)
:cBitmap(Width, Height, 2)
{
  total = Total;
  if (total > 0) {
     int p = Pos(Current);
     DrawRectangle(0, 0, p, Height - 1, ColorSeen);
     DrawRectangle(p + 1, 0, Width - 1, Height - 1, ColorRest);
     if (Marks) {
        bool Start = true;
        for (const cMark *m = Marks->First(); m; m = Marks->Next(m)) {
            int p1 = Pos(m->position);
            if (Start) {
               const cMark *m2 = Marks->Next(m);
               int p2 = Pos(m2 ? m2->position : total);
               int h = Height / 3;
               DrawRectangle(p1, h, p2, Height - h, ColorSelected);
               }
            Mark(p1, Start, m->position == Current, ColorMark, ColorCurrent);
            Start = !Start;
            }
        }
     }
}

void cSkinDisplayReplay::cProgressBar::Mark(int x, bool Start, bool Current, tColor ColorMark, tColor ColorCurrent)
{
  DrawRectangle(x, 0, x, Height() - 1, ColorMark);
  const int d = Height() / (Current ? 3 : 9);
  for (int i = 0; i < d; i++) {
      int h = Start ? i : Height() - 1 - i;
      DrawRectangle(x - d + i, h, x + d - i, h, Current ? ColorCurrent : ColorMark);
      }
}

// --- cSkinDisplayReplay ----------------------------------------------------

cSkinDisplayReplay::cSkinDisplayReplay(void)
{
  marks = NULL;
}

void cSkinDisplayReplay::SetMarks(const cMarks *Marks)
{
  marks = Marks;
}

// --- cSkin -----------------------------------------------------------------

cSkin::cSkin(const char *Name, cTheme *Theme)
{
  name = strdup(Name);
  theme = Theme;
  if (theme)
     cThemes::Save(name, theme);
  Skins.Add(this);
  Skins.SetCurrent(Name);
}

cSkin::~cSkin()
{
  free(name);
}

// --- cSkins ----------------------------------------------------------------

cSkins Skins;

cSkins::cSkins(void)
{
  displayMessage = NULL;
}

cSkins::~cSkins()
{
  delete displayMessage;
}

bool cSkins::SetCurrent(const char *Name)
{
  if (Name) {
     for (cSkin *Skin = First(); Skin; Skin = Next(Skin)) {
         if (strcmp(Skin->Name(), Name) == 0) {
            current = Skin;
            return true;
            }
         }
     }
  current = First();
  return current != NULL;
}

eKeys cSkins::Message(eMessageType Type, const char *s, int Seconds)
{
  switch (Type) {
    case mtInfo:  isyslog("info: %s", s); break;
    case mtError: esyslog("ERROR: %s", s); break;
    default: ;
    }
  if (!Current())
     return kNone;
  if (!cSkinDisplay::Current() && !displayMessage)
     displayMessage = Current()->DisplayMessage();
  cSkinDisplay::Current()->SetMessage(Type, s);
  cSkinDisplay::Current()->Flush();
  cStatus::MsgOsdStatusMessage(s);
  eKeys k = kNone;
  if (Type != mtStatus) {
     k = Interface->Wait(Seconds);
     if (displayMessage) {
        delete displayMessage;
        displayMessage = NULL;
        cStatus::MsgOsdClear();
        }
     else {
        cSkinDisplay::Current()->SetMessage(Type, NULL);
        cStatus::MsgOsdStatusMessage(NULL);
        }
     }
  else if (!s && displayMessage) {
     delete displayMessage;
     displayMessage = NULL;
     cStatus::MsgOsdClear();
     }
  return k;
}

void cSkins::Flush(void)
{
  if (cSkinDisplay::Current())
     cSkinDisplay::Current()->Flush();
}
