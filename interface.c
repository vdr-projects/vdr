/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "interface.h"
#include <ncurses.h>
#include <unistd.h>
#include "dvbapi.h"
#include "remote.h"

#ifndef DEBUG_REMOTE
cRcIo RcIo("/dev/ttyS1");//XXX
#endif

WINDOW *window;

cInterface Interface;

cInterface::cInterface(void)
{
  open = 0;
  cols[0] = 0;
#ifdef DEBUG_OSD
  initscr();
  keypad(stdscr, TRUE);
  nonl();
  cbreak();
  noecho();
  leaveok(stdscr, TRUE);
  window = stdscr;
#else
#endif
}

void cInterface::Init(void)
{
#ifndef DEBUG_REMOTE
  RcIo.SetCode(Keys.code, Keys.address);
#endif
}

void cInterface::Open(void)
{
  if (!open++) {
#ifdef DEBUG_OSD
#else
//TODO
     DvbOsdOpen(100, 100, 500, 400);
#endif
     }
}

void cInterface::Close(void)
{
  if (!--open) {
#ifdef DEBUG_OSD
#else
//TODO
     DvbOsdClose();
#endif
     }
}

unsigned int cInterface::GetCh(void)
{
#ifdef DEBUG_REMOTE
  return getch();
#else
#ifdef DEBUG_OSD
  wrefresh(window);//XXX
#endif
  unsigned int Command;
  return RcIo.GetCommand(&Command) ? Command : 0;
#endif
}

eKeys cInterface::GetKey(void)
{
  return Keys.Get(GetCh());
}

void cInterface::Clear(void)
{
  if (open) {
#ifdef DEBUG_OSD
     wclear(window);
#else
//TODO
     DvbOsdClear();
#endif
     }
}

void cInterface::SetCols(int *c)
{
  for (int i = 0; i < MaxCols; i++) {
      cols[i] = *c++;
      if (cols[i] == 0)
         break;
      }
}

void cInterface::Write(int x, int y, char *s)
{
  if (open) {
#ifdef DEBUG_OSD
     wmove(window, y, x); // ncurses wants 'y' before 'x'!
     waddstr(window, s);
#else
     DvbOsdText(x * DvbOsdCharWidth, y * DvbOsdLineHeight, s);
#endif
     }
}

void cInterface::WriteText(int x, int y, char *s, bool Current)
{
  if (open) {
#ifdef DEBUG_OSD
     wmove(window, y, x); // ncurses wants 'y' before 'x'!
     wclrtoeol(window);//XXX
#else
//TODO
     DvbOsdClrEol(x * DvbOsdCharWidth, y);//XXX
#endif
     Write(x, y, Current ? "*" : " ");
     x++;
     int col = 0;
     for (;;) {
         char *t = strchr(s, '\t');
         char *p = s;
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
         Write(x, y, p);
         if (p == s)
            break;
         x += cols[col++];
         }
     }
}

void cInterface::Info(char *s)
{
  Open();
  isyslog(LOG_ERR, s);
  WriteText(0, 11, s);//TODO
#ifdef DEBUG_OSD
  wrefresh(window);//XXX
#endif
  sleep(1);
  WriteText(0, 11, "");//TODO
#ifdef DEBUG_OSD
  wrefresh(window);//XXX
#endif
  Close();
}

void cInterface::Error(char *s)
{
  Open();
  esyslog(LOG_ERR, s);
  WriteText(0, 12, s);//TODO
#ifdef DEBUG_OSD
  wrefresh(window);//XXX
#endif
  sleep(1);
  WriteText(0, 12, "");//TODO
#ifdef DEBUG_OSD
  wrefresh(window);//XXX
#endif
  Close();
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
         WriteText(1, 5, "");
         WriteText(1, 6, "");
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
                                WriteText(1, 7, "");
                                WriteText(1, 8, "");
                                for (;;) {
                                    eKeys key = GetKey();
                                    if (key == kUp) {
                                       Clear();
                                       return;
                                       }
                                    else if (key == kDown) {
                                       WriteText(1, 6, "");
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
           WriteText(1, 7, "");
        if (k > Keys.keys + 1)
           WriteText(1, 8, "(press 'Down' to end key definition)");
        else
           WriteText(1, 8, "");
        }
}

void cInterface::LearnKeys(void)
{
  isyslog(LOG_INFO, "learning keys");
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
             Clear();
             return;
             }
          else if (key == kDown) {
             Keys.Load();
             Clear();
             return;
             }
          }
      }
}

void cInterface::DisplayChannel(int Number, char *Name)
{
//TODO
#ifndef DEBUG_REMOTE
  RcIo.Number(Number);
#endif
}
