/*
 * dvbosd.h: Interface to the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.8 2001/07/28 16:22:32 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
#include <ncurses.h>
#endif
#include <ost/osd.h>
#include <stdio.h>
#include "font.h"

#define MAXNUMCOLORS 16

enum eDvbColor {
#ifdef DEBUG_OSD
  clrBackground,
  clrTransparent = clrBackground,
  clrBlack = clrBackground,
  clrRed,
  clrGreen,
  clrYellow,
  clrBlue,
  clrMagenta,
  clrCyan,
  clrWhite,
#else
  clrTransparent = 0x00000000,
  clrBackground  = 0x7F000000, // 50% gray
  clrBlack       = 0xFF000000,
  clrRed         = 0xFF1414FC,
  clrGreen       = 0xFF24FC24,
  clrYellow      = 0xFF24C0FC,
  clrMagenta     = 0xFFFC00B0,
  clrBlue        = 0xFFFC0000,
  clrCyan        = 0xFFFCFC00,
  clrWhite       = 0xFFFCFCFC,
#endif
  };

class cPalette {
private:
  eDvbColor color[MAXNUMCOLORS];
  int maxColors, numColors;
  bool used[MAXNUMCOLORS];
  bool fetched[MAXNUMCOLORS];
  bool full;
protected:
  typedef unsigned char tIndexes[MAXNUMCOLORS];
public:
  cPalette(int Bpp);
  int Index(eDvbColor Color);
  void Reset(void);
  const eDvbColor *Colors(int &FirstColor, int &LastColor);
  void Take(const cPalette &Palette, tIndexes *Indexes = NULL);
  };

class cBitmap : public cPalette {
private:
  cFont *font;
  eDvbFont fontType;
  void SetIndex(int x, int y, char Index);
  char *bitmap;
  bool clearWithBackground;
protected:
  int width, height;
  int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
public:
  cBitmap(int Width, int Height, int Bpp, bool ClearWithBackground = true);
  virtual ~cBitmap();
  eDvbFont SetFont(eDvbFont Font);
  bool Dirty(int &x1, int &y1, int &x2, int &y2);
  void SetPixel(int x, int y, eDvbColor Color);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  int Width(void) { return width; }
  int Width(unsigned char c);
  int Width(const char *s);
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground);
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color);
  void Clean(void);
  void Clear(void);
  const char *Data(int x, int y);
  };

#define MAXNUMWINDOWS 7 // OSD windows are counted 1...7

class cWindow;

class cDvbOsd {
private:
  int videoDev;
  int numWindows;
  int x0, y0;
  cWindow *window[MAXNUMWINDOWS];
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
  cWindow *GetWindow(int x, int y);
public:
  cDvbOsd(int VideoDev, int x, int y, int w = -1, int h = -1, int Bpp = -1);
  ~cDvbOsd();
  bool Create(int x, int y, int w, int h, int Bpp, bool ClearWithBackground = true, eDvbColor Color0 = clrTransparent, eDvbColor Color1 = clrTransparent, eDvbColor Color2 = clrTransparent, eDvbColor Color3 = clrTransparent);
  void Flush(void);
  void Clear(void);
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  int Width(unsigned char c);
  int Width(const char *s);
  eDvbFont SetFont(eDvbFont Font);
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground);
  };

#endif //__DVBOSD_H
