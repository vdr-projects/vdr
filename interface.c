/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 1.66 2003/10/24 14:38:08 kls Exp $
 */

#include "interface.h"
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "i18n.h"
#include "osd.h"
#include "status.h"

cInterface *Interface = NULL;

cInterface::cInterface(int SVDRPport)
{
  open = 0;
  cols[0] = 0;
  width = height = 0;
  interrupted = false;
  SVDRP = NULL;
  if (SVDRPport)
     SVDRP = new cSVDRP(SVDRPport);
}

cInterface::~cInterface()
{
  delete SVDRP;
}

void cInterface::Open(int NumCols, int NumLines)
{
  if (!open++) {
     if (NumCols == 0)
        NumCols = Setup.OSDwidth;
     if (NumLines == 0)
        NumLines = Setup.OSDheight;
     cOsd::Open(width = NumCols, height = NumLines);
     }
}

void cInterface::Close(void)
{
  if (open == 1)
     Clear();
  if (!--open) {
     cOsd::Close();
     width = height = 0;
     }
}

eKeys cInterface::GetKey(bool Wait)
{
  if (!cRemote::HasKeys())
     Flush();
  if (SVDRP) {
     if (SVDRP->Process())
        Wait = false;
     if (!open) {
        char *message = SVDRP->GetMessage();
        if (message) {
           Info(message);
           free(message);
           }
        }
     }
  return cRemote::Get(Wait ? 1000 : 10);
}

eKeys cInterface::Wait(int Seconds, bool KeepChar)
{
  if (Seconds == 0)
     Seconds = Setup.OSDMessageTime;
  Flush();
  eKeys Key = kNone;
  time_t timeout = time(NULL) + Seconds;
  for (;;) {
      Key = GetKey();
      if ((Key != kNone && (RAWKEY(Key) != kOk || RAWKEY(Key) == Key)) || time(NULL) > timeout || interrupted)
         break;
      }
  if (KeepChar && ISRAWKEY(Key))
     cRemote::Put(Key);
  interrupted = false;
  return Key;
}

void cInterface::Clear(void)
{
  if (open)
     cOsd::Clear();
  cStatus::MsgOsdClear();
}

void cInterface::ClearEol(int x, int y, eDvbColor Color)
{
  if (open)
     cOsd::ClrEol(x, y, Color);
}

void cInterface::Fill(int x, int y, int w, int h, eDvbColor Color)
{
  if (open)
     cOsd::Fill(x, y, w, h, Color);
}

void cInterface::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
  if (open)
     cOsd::SetBitmap(x, y, Bitmap);
}

void cInterface::Flush(void)
{
  if (open)
     cOsd::Flush();
}

void cInterface::SetCols(int *c)
{
  for (int i = 0; i < MaxCols; i++) {
      cols[i] = *c++;
      if (cols[i] == 0)
         break;
      }
}

eDvbFont cInterface::SetFont(eDvbFont Font)
{
  return cOsd::SetFont(Font);
}

char *cInterface::WrapText(const char *Text, int Width, int *Height)
{
  // Wraps the Text to make it fit into the area defined by the given Width
  // (which is given in character cells).
  // The actual number of lines resulting from this operation is returned in
  // Height.
  // The returned string is newly created on the heap and the caller
  // is responsible for deleting it once it is no longer used.
  // Wrapping is done by inserting the necessary number of newline
  // characters into the string.

  int Lines = 1;
  char *t = strdup(Text);
  char *Blank = NULL;
  char *Delim = NULL;
  int w = 0;

  Width *= cOsd::CellWidth();

  while (*t && t[strlen(t) - 1] == '\n')
        t[strlen(t) - 1] = 0; // skips trailing newlines

  for (char *p = t; *p; ) {
      if (*p == '|')
         *p = '\n';
      if (*p == '\n') {
         Lines++;
         w = 0;
         Blank = Delim = NULL;
         p++;
         continue;
         }
      else if (isspace(*p))
         Blank = p;
      int cw = cOsd::Width(*p);
      if (w + cw > Width) {
         if (Blank) {
            *Blank = '\n';
            p = Blank;
            continue;
            }
         else {
            // Here's the ugly part, where we don't have any whitespace to
            // punch in a newline, so we need to make room for it:
            if (Delim)
               p = Delim + 1; // let's fall back to the most recent delimiter
            char *s = MALLOC(char, strlen(t) + 2); // The additional '\n' plus the terminating '\0'
            int l = p - t;
            strncpy(s, t, l);
            s[l] = '\n';
            strcpy(s + l + 1, p);
            free(t);
            t = s;
            p = t + l;
            continue;
            }
         }
      else
         w += cw;
      if (strchr("-.,:;!?_", *p)) {
         Delim = p;
         Blank = NULL;
         }
      p++;
      }

  *Height = Lines;
  return t;
}

void cInterface::Write(int x, int y, const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open)
     cOsd::Text(x, y, s, FgColor, BgColor);
}

void cInterface::WriteText(int x, int y, const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open) {
     ClearEol(x, y, BgColor);
     int col = 0;
     for (;;) {
         const char *t = strchr(s, '\t');
         const char *p = s;
         char buf[1000];
         if (t && col < MaxCols && cols[col] > 0) {
            unsigned int n = t - s;
            if (n >= sizeof(buf))
               n = sizeof(buf) - 1;
            strncpy(buf, s, n);
            buf[n] = 0;
            p = buf;
            s = t + 1;
            }
         Write(x, y, p, FgColor, BgColor);
         if (p == s)
            break;
         x += cols[col++];
         }
     }
}

void cInterface::Title(const char *s)
{
  ClearEol(0, 0, clrCyan);
  const char *t = strchr(s, '\t');
  if (t) {
     char buffer[Width() + 1];
     unsigned int n = t - s;
     if (n >= sizeof(buffer))
        n = sizeof(buffer) - 1;
     strn0cpy(buffer, s, n + 1);
     Write(1, 0, buffer, clrBlack, clrCyan);
     t++;
     Write(-(cOsd::WidthInCells(t) + 1), 0, t, clrBlack, clrCyan);
     }
  else {
     int x = (Width() - strlen(s)) / 2;
     if (x < 0)
        x = 0;
     Write(x, 0, s, clrBlack, clrCyan);
     }
  cStatus::MsgOsdTitle(s);
}

void cInterface::Status(const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  int Line = (abs(height) == 1) ? 0 : -2;
  ClearEol(0, Line, s ? BgColor : clrBackground);
  if (s) {
     int x = (Width() - int(strlen(s))) / 2;
     if (x < 0)
        x = 0;
     Write(x, Line, s, FgColor, BgColor);
     }
  cStatus::MsgOsdStatusMessage(s);
}

void cInterface::Info(const char *s)
{
  Open(Setup.OSDwidth, -1);
  isyslog("info: %s", s);
  Status(s, clrBlack, clrGreen);
  Wait();
  Status(NULL);
  Close();
}

void cInterface::Error(const char *s)
{
  Open(Setup.OSDwidth, -1);
  esyslog("ERROR: %s", s);
  Status(s, clrWhite, clrRed);
  Wait();
  Status(NULL);
  Close();
}

bool cInterface::Confirm(const char *s, int Seconds, bool WaitForTimeout)
{
  Open(Setup.OSDwidth, -1);
  isyslog("confirm: %s", s);
  Status(s, clrBlack, clrYellow);
  eKeys k = Wait(Seconds);
  bool result = WaitForTimeout ? k == kNone : k == kOk;
  Status(NULL);
  Close();
  isyslog("%sconfirmed", result ? "" : "not ");
  return result;
}

void cInterface::HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open) {
     const int w = Width() / 4;
     cOsd::Fill(Index * w, -1, w, 1, Text ? BgColor : clrBackground);
     if (Text) {
        int l = (w - int(strlen(Text))) / 2;
        if (l < 0)
           l = 0;
        cOsd::Text(Index * w + l, -1, Text, FgColor, BgColor);
        }
     }
}

void cInterface::Help(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  HelpButton(0, Red,    clrBlack, clrRed);
  HelpButton(1, Green,  clrBlack, clrGreen);
  HelpButton(2, Yellow, clrBlack, clrYellow);
  HelpButton(3, Blue,   clrWhite, clrBlue);
  cStatus::MsgOsdHelpKeys(Red, Green, Yellow, Blue);
}

bool cInterface::QueryKeys(cRemote *Remote)
{
  WriteText(1, 3, tr("Phase 1: Detecting RC code type"));
  WriteText(1, 5, tr("Press any key on the RC unit"));
  Flush();
  if (Remote->Initialize()) {
     WriteText(1, 5, tr("RC code detected!"));
     WriteText(1, 6, tr("Do not press any key..."));
     Flush();
     sleep(3);
     ClearEol(0, 5);
     ClearEol(0, 6);

     WriteText(1, 3, tr("Phase 2: Learning specific key codes"));
     eKeys NewKey = kUp;
     while (NewKey != kNone) {
           char *Prompt;
           asprintf(&Prompt, tr("Press key for '%s'"), tr(cKey::ToString(NewKey)));
           WriteText(1, 5, Prompt);
           free(Prompt);
           cRemote::Clear();
           Flush();
           for (eKeys k = NewKey; k == NewKey; ) {
               char *NewCode = NULL;
               eKeys Key = cRemote::Get(100, &NewCode);
               switch (Key) {
                 case kUp:   if (NewKey > kUp) {
                                NewKey = eKeys(NewKey - 1);
                                cKey *last = Keys.Last();
                                if (last && last->Key() == NewKey)
                                   Keys.Del(last);
                                }
                             break;
                 case kDown: WriteText(1, 5, tr("Press 'Up' to confirm"));
                             WriteText(1, 6, tr("Press 'Down' to continue"));
                             ClearEol(0, 7);
                             ClearEol(0, 8);
                             ClearEol(0, 9);
                             Flush();
                             for (;;) {
                                 Key = cRemote::Get(100);
                                 if (Key == kUp) {
                                    Clear();
                                    return true;
                                    }
                                 else if (Key == kDown) {
                                    ClearEol(0, 6);
                                    k = kNone; // breaks the outer for() loop
                                    break;
                                    }
                                 }
                             break;
                 case kMenu: NewKey = eKeys(NewKey + 1);
                             break;
                 case kNone: if (NewCode) {
                                dsyslog("new %s code: %s = %s", Remote->Name(), NewCode, cKey::ToString(NewKey));
                                Keys.Add(new cKey(Remote->Name(), NewCode, NewKey));
                                NewKey = eKeys(NewKey + 1);
                                free(NewCode);
                                }
                             break;
                 default:    break;
                 }
               }
           if (NewKey > kUp)
              WriteText(1, 7, tr("(press 'Up' to go back)"));
           else
              ClearEol(0, 7);
           if (NewKey > kDown)
              WriteText(1, 8, tr("(press 'Down' to end key definition)"));
           else
              ClearEol(0, 8);
           if (NewKey > kMenu)
              WriteText(1, 9, tr("(press 'Menu' to skip this key)"));
           else
              ClearEol(0, 9);
           }
     return true;
     }
  return false;
}

void cInterface::LearnKeys(void)
{
  for (cRemote *Remote = Remotes.First(); Remote; Remote = Remotes.Next(Remote)) {
      if (!Remote->Ready()) {
         esyslog("ERROR: remote control %s not ready!", Remote->Name());
         continue;
         }
      bool known = Keys.KnowsRemote(Remote->Name());
      dsyslog("remote control %s - %s", Remote->Name(), known ? "keys known" : "learning keys");
      if (!known) {
         Open();
         char Headline[Width()];
         snprintf(Headline, sizeof(Headline), tr("Learning Remote Control Keys (%s)"), Remote->Name());
         Clear();
         cRemote::Clear();
         WriteText(1, 1, Headline);
         cRemote::SetLearning(Remote);
         bool rc = QueryKeys(Remote);
         cRemote::SetLearning(NULL);
         Clear();
         if (!rc) {
            Close();
            continue;
            }
         WriteText(1, 1, Headline);
         WriteText(1, 3, tr("Phase 3: Saving key codes"));
         WriteText(1, 5, tr("Press 'Up' to save, 'Down' to cancel"));
         for (;;) {
             eKeys key = GetKey();
             if (key == kUp) {
                Keys.Save();
                Close();
                break;
                }
             else if (key == kDown) {
                Keys.Load();
                Close();
                break;
                }
             }
         }
      }
}
