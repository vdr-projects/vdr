/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.2 2000/03/11 10:39:09 kls Exp $
 */

#include "dvbapi.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "interface.h"
#include "tools.h"

#define VIDEODEVICE "/dev/video"

const char *DvbQuality = "LMH"; // Low, Medium, High

bool DvbSetChannel(int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid)
{
  int v = open(VIDEODEVICE, O_RDWR);
  
  if (v >= 0) {
     struct frontend front;
     ioctl(v, VIDIOCGFRONTEND, &front);
     unsigned int freq = FrequencyMHz;
     front.ttk = (freq < 11800UL) ? 0 : 1;
     if (freq < 11800UL)
        freq -=  9750UL;
     else
        freq -= 10600UL;
     front.freq      = freq * 1000000UL;
     front.diseqc    = Diseqc;
     front.srate     = Srate * 1000;
     front.volt      = (Polarization == 'v') ? 0 : 1;
     front.video_pid = Vpid;
     front.audio_pid = Apid;
     front.AFC       = 1;
     ioctl(v, VIDIOCSFRONTEND, &front);
     close(v);
     if (front.sync & 0x1F == 0x1F)
        return true;
     esyslog(LOG_ERR, "channel not sync'ed (front.sync=%X)!", front.sync);
     }
  else
     Interface.Error("can't open VIDEODEVICE");//XXX
  return false;
}

// -- cDvbRecorder -----------------------------------------------------------

cDvbRecorder::cDvbRecorder(void)
{
  recording = false;
}

cDvbRecorder::~cDvbRecorder()
{
  Stop();
}

bool cDvbRecorder::Recording(void)
{
  return recording;
}

bool cDvbRecorder::Record(const char *FileName, char Quality)
{
  isyslog(LOG_INFO, "record %s (%c)", FileName, Quality);
  if (MakeDirs(FileName)) {
     FILE *f = fopen(FileName, "a");
     if (f) {
        fprintf(f, "%s, %c\n", FileName, Quality);
        fclose(f);
        recording = true;
        // TODO
        Interface.Error("Recording not yet implemented!");
        return true;
        }
     else {
        Interface.Error("Can't write to file!");
        return false;
        }
     }
  // TODO
  return false;
}

bool cDvbRecorder::Play(const char *FileName, int Frame)
{
  if (!recording) {
     isyslog(LOG_INFO, "play %s (%d)", FileName, Frame);
     // TODO
     Interface.Error("Playback not yet implemented!");
     return true;
     }
  return false;
}

bool cDvbRecorder::FastForward(void)
{
  isyslog(LOG_INFO, "fast forward");
  // TODO
  return false;
}

bool cDvbRecorder::FastRewind(void)
{
  isyslog(LOG_INFO, "fast rewind");
  // TODO
  return false;
}

bool cDvbRecorder::Pause(void)
{
  isyslog(LOG_INFO, "pause");
  // TODO
  return false;
}

void cDvbRecorder::Stop(void)
{
  isyslog(LOG_INFO, "stop");
  recording = false;
  // TODO
}

int cDvbRecorder::Frame(void)
{
  isyslog(LOG_INFO, "frame");
  // TODO
  return 0;
}

// --- cDvbOsd ---------------------------------------------------------------

cDvbOsd::cDvbOsd(void)
{
  cols = rows = 0;
#ifdef DEBUG_OSD
  memset(&colorPairs, 0, sizeof(colorPairs));
  initscr();
  start_color();
  keypad(stdscr, TRUE);
  nonl();
  cbreak();
  noecho();
  timeout(1000);
  leaveok(stdscr, TRUE);
  window = stdscr;
#endif
#ifdef DEBUG_REMOTE
  initscr();
  keypad(stdscr, TRUE);
  nonl();
  cbreak();
  noecho();
  timeout(1000);
#endif
}

cDvbOsd::~cDvbOsd()
{
  Close();
}

#ifdef DEBUG_OSD
void cDvbOsd::SetColor(eDvbColor colorFg, eDvbColor colorBg)
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
#else
void cDvbOsd::Cmd(OSD_Command cmd, int color, int x0, int y0, int x1, int y1, const void *data)
{
  int v = open(VIDEODEVICE, O_RDWR);

  if (v >= 0) {
     struct drawcmd dc;
     dc.cmd   = cmd;
     dc.color = color;
     dc.x0    = x0;
     dc.y0    = y0;
     dc.x1    = x1;
     dc.y1    = y1;
     dc.data  = (void *)data;
     ioctl(v, VIDIOCSOSDCOMMAND, &dc);
     close(v);
     }
  else
     Interface.Error("can't open VIDEODEVICE");//XXX
}
#endif

void cDvbOsd::Open(int w, int h)
{
  cols = w;
  rows = h;
#ifdef DEBUG_OSD
  //XXX size...
  #define B2C(b) (((b) * 1000) / 255)
  #define SETCOLOR(n, r, g, b, o) init_color(n, B2C(r), B2C(g), B2C(b))
#else
  w *= charWidth;
  h *= lineHeight;
  int x = (720 - w) / 2; //TODO PAL vs. NTSC???
  int y = (576 - h) / 2;
  Cmd(OSD_Open, 4, x, y, x + w - 1, y + h - 1);
  #define SETCOLOR(n, r, g, b, o) Cmd(OSD_SetColor, n, r, g, b, o)
#endif
  SETCOLOR(clrBackground, 0x00, 0x00, 0x00, 127); // background 50% gray
  SETCOLOR(clrBlack,      0x00, 0x00, 0x00, 255);
  SETCOLOR(clrRed,        0xFC, 0x14, 0x14, 255);
  SETCOLOR(clrGreen,      0x24, 0xFC, 0x24, 255);
  SETCOLOR(clrYellow,     0xFC, 0xC0, 0x24, 255);
  SETCOLOR(clrBlue,       0x00, 0x00, 0xFC, 255);
  SETCOLOR(clrCyan,       0x00, 0xFC, 0xFC, 255);
  SETCOLOR(clrMagenta,    0xB0, 0x00, 0xFC, 255);
  SETCOLOR(clrWhite,      0xFC, 0xFC, 0xFC, 255);
}

void cDvbOsd::Close(void)
{
#ifndef DEBUG_OSD
  Cmd(OSD_Close);
#endif
}

void cDvbOsd::Clear(void)
{
#ifdef DEBUG_OSD
  SetColor(clrBackground, clrBackground);
  Fill(0, 0, cols, rows, clrBackground);
#else
  Cmd(OSD_Clear);
#endif
}

void cDvbOsd::Fill(int x, int y, int w, int h, eDvbColor color)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(color, color);
  for (int r = 0; r < h; r++) {
      wmove(window, y + r, x); // ncurses wants 'y' before 'x'!
      whline(window, ' ', w);
      }
#else
  Cmd(OSD_FillBlock, color, x * charWidth, y * lineHeight, (x + w) * charWidth - 1, (y + h) * lineHeight - 1);
#endif
}

void cDvbOsd::ClrEol(int x, int y, eDvbColor color)
{
  Fill(x, y, cols - x, 1, color);
}

void cDvbOsd::Text(int x, int y, const char *s, eDvbColor colorFg, eDvbColor colorBg)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(colorFg, colorBg);
  wmove(window, y, x); // ncurses wants 'y' before 'x'!
  waddstr(window, s);
#else
  Cmd(OSD_Text, (int(colorBg) << 16) | colorFg, x * charWidth, y * lineHeight, 1, 0, s);
#endif
}
