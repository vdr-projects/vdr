/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 1.3 2000/04/15 17:38:11 kls Exp $
 */

#include "interface.h"
#include <unistd.h>
#include "remote.h"

#define MenuLines   15
#define MenuColumns 40

#ifndef DEBUG_REMOTE
cRcIo RcIo("/dev/ttyS1");
#endif

cDvbApi DvbApi; //XXX member of cInterface???

cInterface Interface;

cInterface::cInterface(void)
{
  open = 0;
  cols[0] = 0;
}

void cInterface::Init(void)
{
#ifndef DEBUG_REMOTE
  RcIo.SetCode(Keys.code, Keys.address);
#endif
}

void cInterface::Open(void)
{
  if (!open++)
     DvbApi.Open(MenuColumns, MenuLines);
}

void cInterface::Close(void)
{
  if (open == 1)
     Clear();
  if (!--open)
     DvbApi.Close();
}

unsigned int cInterface::GetCh(void)
{
#ifdef DEBUG_REMOTE
  int c = getch();
  return (c > 0) ? c : 0;
#else
//XXX #ifdef DEBUG_OSD
//XXX   wrefresh(window);//XXX
//XXX #endif
  unsigned int Command;
  return RcIo.GetCommand(&Command) ? Command : 0;
#endif
}

eKeys cInterface::GetKey(void)
{
  return Keys.Get(GetCh());
}

eKeys cInterface::Wait(int Seconds)
{
  int t0 = time_ms();

  while (time_ms() - t0 < Seconds * 1000) {
        eKeys Key = GetKey();
        if (Key != kNone)
           return Key;
        }
  return kNone;
}

void cInterface::Clear(void)
{
  if (open)
     DvbApi.Clear();
}

void cInterface::ClearEol(int x, int y, eDvbColor Color)
{
  if (open)
     DvbApi.ClrEol(x, y, Color);
}

void cInterface::SetCols(int *c)
{
  for (int i = 0; i < MaxCols; i++) {
      cols[i] = *c++;
      if (cols[i] == 0)
         break;
      }
}

void cInterface::Write(int x, int y, const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open)
     DvbApi.Text(x, y, s, FgColor, BgColor);
}

void cInterface::WriteText(int x, int y, const char *s, bool Current)
{
  if (open) {
     eDvbColor FgColor = Current ? clrBlack : clrWhite;
     eDvbColor BgColor = Current ? clrCyan : clrBackground;
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
  int x = (MenuColumns - strlen(s)) / 2;
  if (x < 0)
     x = 0;
  ClearEol(0, 0, clrCyan);
  Write(x, 0, s, clrBlack, clrCyan);
}

void cInterface::Status(const char *s, eDvbColor FgColor, eDvbColor BgColor)
{
  ClearEol(0, -3, s ? BgColor : clrBackground);
  if (s)
     Write(0, -3, s, FgColor, BgColor);
}

void cInterface::Info(const char *s)
{
  Open();
  isyslog(LOG_INFO, s);
  Status(s, clrWhite, clrGreen);
  Wait();
  Status(NULL);
  Close();
}

void cInterface::Error(const char *s)
{
  Open();
  esyslog(LOG_ERR, s);
  Status(s, clrWhite, clrRed);
  Wait();
  Status(NULL);
  Close();
}

bool cInterface::Confirm(const char *s)
{
  Open();
  isyslog(LOG_INFO, "confirm: %s", s);
  Status(s, clrBlack, clrGreen);
  bool result = Wait(10) == kOk;
  Status(NULL);
  Close();
  isyslog(LOG_INFO, "%sconfirmed", result ? "" : "not ");
  return result;
}

void cInterface::HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor)
{
  if (open && Text) {
     const int w = MenuColumns / 4;
     int l = (w - strlen(Text)) / 2;
     if (l < 0)
        l = 0;
     DvbApi.Fill(Index * w, -1, w, 1, BgColor);
     DvbApi.Text(Index * w + l, -1, Text, FgColor, BgColor);
     }
}

void cInterface::Help(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  HelpButton(0, Red,    clrBlack, clrRed);
  HelpButton(1, Green,  clrBlack, clrGreen);
  HelpButton(2, Yellow, clrBlack, clrYellow);
  HelpButton(3, Blue,   clrWhite, clrBlue);
}

void cInterface::QueryKeys(void)
{
  Keys.Clear();
  WriteText(1, 1, "Learning Remote Control Keys");
  WriteText(1, 3, "Phase 1: Detecting RC code type");
  WriteText(1, 5, "Press any key on the RC unit");
#ifndef DEBUG_REMOTE
  unsigned char Code = 0;
  unsigned short Address;
#endif
  for (;;) {
#ifdef DEBUG_REMOTE
      if (GetCh())
         break;
#else
      //TODO on screen display...
      if (RcIo.DetectCode(&Code, &Address)) {
         Keys.code = Code;
         Keys.address = Address;
         WriteText(1, 5, "RC code detected!");
         WriteText(1, 6, "Do not press any key...");
         RcIo.Flush(3);
         ClearEol(0, 5);
         ClearEol(0, 6);
         break;
         }
#endif
      }
  WriteText(1, 3, "Phase 2: Learning specific key codes");
  tKey *k = Keys.keys;
  while (k->type != kNone) {
        char *Prompt;
        asprintf(&Prompt, "Press key for '%s'", k->name);
        WriteText(1, 5, Prompt);
        delete Prompt;
        for (;;) {
            unsigned int ch = GetCh();
            if (ch != 0) {
               switch (Keys.Get(ch)) {
                 case kUp:   if (k > Keys.keys) {
                                k--;
                                break;
                                }
                 case kDown: if (k > Keys.keys + 1) {
                                WriteText(1, 5, "Press 'Up' to confirm");
                                WriteText(1, 6, "Press 'Down' to continue");
                                ClearEol(0, 7);
                                ClearEol(0, 8);
                                for (;;) {
                                    eKeys key = GetKey();
                                    if (key == kUp) {
                                       Clear();
                                       return;
                                       }
                                    else if (key == kDown) {
                                       ClearEol(0, 6);
                                       break;
                                       }
                                    }
                                break;
                                }
                 case kNone: k->code = ch;
                             k++;
                             break;
                 default:    break;
                 }
               break;
               }
            }
        if (k > Keys.keys)
           WriteText(1, 7, "(press 'Up' to go back)");
        else
           ClearEol(0, 7);
        if (k > Keys.keys + 1)
           WriteText(1, 8, "(press 'Down' to end key definition)");
        else
           ClearEol(0, 8);
        }
}

void cInterface::LearnKeys(void)
{
  isyslog(LOG_INFO, "learning keys");
  Open();
  for (;;) {
      Clear();
      QueryKeys();
      Clear();
      WriteText(1, 1, "Learning Remote Control Keys");
      WriteText(1, 3, "Phase 3: Saving key codes");
      WriteText(1, 5, "Press 'Up' to save, 'Down' to cancel");
      for (;;) {
          eKeys key = GetKey();
          if (key == kUp) {
             Keys.Save();
             Close();
             return;
             }
          else if (key == kDown) {
             Keys.Load();
             Close();
             return;
             }
          }
      }
}

void cInterface::DisplayChannel(int Number, const char *Name)
{
//TODO
#ifndef DEBUG_REMOTE
  RcIo.Number(Number);
#endif
}
