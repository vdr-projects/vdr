/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

// FIXME: these should be defined in ../DVB/driver/dvb.h!!!
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#include "dvbapi.h"
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "../DVB/driver/dvb.h"
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
}

cDvbRecorder::~cDvbRecorder()
{
  Stop();
}

bool cDvbRecorder::Record(const char *FileName, char Quality)
{
  isyslog(LOG_INFO, "record %s (%c)", FileName, Quality);
  return true;
  // TODO
  return false;
}

bool cDvbRecorder::Play(const char *FileName, int Frame)
{
  isyslog(LOG_INFO, "play %s (%d)", FileName, Frame);
  // TODO
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
  // TODO
}

int cDvbRecorder::Frame(void)
{
  isyslog(LOG_INFO, "frame");
  // TODO
  return 0;
}

// ---------------------------------------------------------------------------

static void DvbOsdCmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, void *data = NULL)
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
     dc.data  = data;
     ioctl(v, VIDIOCSOSDCOMMAND, &dc);
     close(v);
     }
  else
     Interface.Error("can't open VIDEODEVICE");//XXX
}

void DvbOsdOpen(int x, int y, int w, int h)
{
  DvbOsdCmd(OSD_Open, 1, x, y, x + w - 1, y + h - 1);
  DvbOsdCmd(OSD_SetColor, 0,   0,   0,   0, 127); // background 50% gray
  DvbOsdCmd(OSD_SetColor, 1, 255, 255, 255, 255); // text white
}

void DvbOsdClose(void)
{
  DvbOsdCmd(OSD_Close);
}

void DvbOsdClear(void)
{
  DvbOsdCmd(OSD_Clear);
}

void DvbOsdClrEol(int x, int y)
{
  DvbOsdCmd(OSD_FillBlock, 0, x, y * DvbOsdLineHeight, x + 490, (y + 1) * DvbOsdLineHeight);//XXX
}

void DvbOsdText(int x, int y, char *s)
{
  DvbOsdCmd(OSD_Text, 1, x, y, 1, 0, s);
}
