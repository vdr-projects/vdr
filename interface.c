/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 1.12 2000/09/09 14:17:48 kls Exp $
 */

#include "interface.h"
#include <unistd.h>
#include "eit.h"
#include "remote.h"

cEIT EIT;

#if defined(REMOTE_RCU)
cRcIoRCU RcIo("/dev/ttyS1");
#elif defined(REMOTE_LIRC)
cRcIoLIRC RcIo("/dev/lircd");
#else
cRcIoKBD RcIo;
#endif

cInterface Interface;

cInterface::cInterface(void)
{
  open = 0;
  cols[0] = 0;
  keyFromWait = kNone;
}

void cInterface::Init(void)
{
  RcIo.SetCode(Keys.code, Keys.address);
}

void cInterface::Open(int NumCols, int NumLines)
{
  if (!open++)
     cDvbApi::PrimaryDvbApi->Open(NumCols, NumLines);
}

void cInterface::Close(void)
{
  if (open == 1)
     Clear();
  if (!--open)
     cDvbApi::PrimaryDvbApi->Close();
}

unsigned int cInterface::GetCh(bool Wait)
{
#ifdef DEBUG_OSD
  timeout(0);
  getch(); // just to make 'ncurses' display the window:
#endif
  if (RcIo.InputAvailable(Wait)) {
     unsigned int Command;
     return RcIo.GetCommand(&Command, NULL) ? Command : 0;
     }
  return 0;
}

eKeys cInterface::GetKey(bool Wait)
{
  eKeys Key = keyFromWait != kNone ? keyFromWait : Keys.Get(GetCh(Wait));
  keyFromWait = kNone;
  return Key;
}

eKeys cInterface::Wait(int Seconds, bool KeepChar)
{
  int t0 = time_ms() + Seconds * 1000;
  eKeys Key = kNone;

  while (time_ms() < t0) {
        Key = GetKey();
        if (Key != kNone)
           break;
        }
  if (KeepChar)
     keyFromWait = Key;
  return Key;
}

void cInterface::Clear(void)
{
  if (open)
     cDvbApi::PrimaryDvbApi->Clear();
}

void cInterface::ClearEol(int x, int y, eDvbColor Color)
{
  if (open)
     cDvbApi::PrimaryDvbApi->ClrEol(x, y, Color);
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
     cDvbApi::PrimaryDvbApi->Text(x, y, s, FgColor, BgColor);
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
     cDvbApi::PrimaryDvbApi->Fill(Index * w, -1, w, 1, BgColor);
     cDvbApi::PrimaryDvbApi->Text(Index * w + l, -1, Text, FgColor, BgColor);
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
#ifndef REMOTE_KBD
  unsigned char Code = 0;
  unsigned short Address;
#endif
  for (;;) {
#ifdef REMOTE_KBD
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

eKeys cInterface::DisplayChannel(int Number, const char *Name)
{
  // Number = 0 is used for channel group display and no EIT
  if (Number)
     RcIo.Number(Number);
  if (Name && !Recording()) {
     //XXX Maybe show only those lines that have actual information???
     Open(MenuColumns, Number && EIT.IsValid() ? 5 : 1);
     int BufSize = MenuColumns + 1;
     char buffer[BufSize];
     if (Number)
        snprintf(buffer, BufSize, "%d  %s", Number, Name ? Name : "");
     else
        snprintf(buffer, BufSize, "%s", Name ? Name : "");
     Write(0, 0, buffer);
     time_t t = time(NULL);
     struct tm *now = localtime(&t);
     snprintf(buffer, BufSize, "%02d:%02d", now->tm_hour, now->tm_min);
     Write(-5, 0, buffer);
     if (Number && EIT.IsValid()) { 
        const int t = 6;
        int w = MenuColumns - t;
        Write(0, 1, EIT.GetRunningTime(), clrYellow, clrBackground); 
        snprintf(buffer, BufSize, "%.*s", w, EIT.GetRunningTitle());    Write(t, 1, buffer, clrCyan, clrBackground); 
        snprintf(buffer, BufSize, "%.*s", w, EIT.GetRunningSubtitle()); Write(t, 2, buffer, clrCyan, clrBackground); 
        Write(0, 3, EIT.GetNextTime(), clrYellow, clrBackground); 
        snprintf(buffer, BufSize, "%.*s", w, EIT.GetNextTitle());       Write(t, 3, buffer, clrCyan, clrBackground); 
        snprintf(buffer, BufSize, "%.*s", w, EIT.GetNextSubtitle());    Write(t, 4, buffer, clrCyan, clrBackground); 
        }
     eKeys Key = Wait(5, true);
     if (Key == kOk)
        GetKey();
     Close();
     return Key;
     }
  return kNone;
}

void cInterface::DisplayRecording(int Index, bool On)
{
  RcIo.SetPoints(1 << Index, On);
}

bool cInterface::Recording(void)
{
  // This is located here because the Interface has to do with the "PrimaryDvbApi" anyway
  return cDvbApi::PrimaryDvbApi->Recording();
}
