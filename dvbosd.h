/*
 * dvbosd.h: Interface to the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.2 2000/11/01 09:13:44 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

// FIXME: these should be defined in ../DVB/driver/dvb.h!!!
typedef unsigned int __u32;
typedef unsigned short __u16;
typedef unsigned char __u8;

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
#include <ncurses.h>
#endif
#include <stdio.h>
#include <dvb.h>
#include "font.h"

enum eDvbColor { 
#ifndef DEBUG_OSD
  clrTransparent,
#endif
  clrBackground,
#ifdef DEBUG_OSD
  clrTransparent = clrBackground,
  clrBlack = clrBackground,
#else
  clrBlack,
#endif
  clrRed,
  clrGreen,
  clrYellow,
  clrBlue,
  clrMagenta,
  clrCyan,
  clrWhite,
  };

class cBitmap {
private:
  cFont *font;
protected:
  int width, height;
  char *bitmap;
  int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
  void Clean(void);
public:
  cBitmap(int Width, int Height);
  virtual ~cBitmap();
  void SetFont(eDvbFont Font);
  bool Dirty(void);
  void SetPixel(int x, int y, eDvbColor Color);
  int Width(unsigned char c);
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground);
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color);
  void Clear(void);
  };

class cDvbOsd : public cBitmap {
private:
  int videoDev;
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
public:
  cDvbOsd(int VideoDev, int x1, int y1, int x2, int y2, int Bpp);
  ~cDvbOsd();
  void Flush(void);
  };

#endif //__DVBOSD_H
