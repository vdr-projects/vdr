/*
 * osdbase.h: Basic interface to the On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osdbase.h 1.6 2002/09/08 14:12:41 kls Exp $
 */

#ifndef __OSDBASE_H
#define __OSDBASE_H

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
  void SetColor(int Index, eDvbColor Color);
  eDvbColor GetColor(int Index) { return Index < maxColors ? color[Index] : clrBlack; }
  const eDvbColor *NewColors(int &FirstColor, int &LastColor);
        // With every call this function returns a consecutive range of
        // color entries that have been added since the last call. The
        // return value is the address of the first new color, and the
        // index of the first and last new color are returned in the given
        // int parameters. If there are no new color entries, NULL will
        // be returned.
  const eDvbColor *AllColors(int &NumColors);
        // Returns a pointer to the complete color table and stores the
        // number of valid entries in NumColors. If no colors have been
        // stored yet, NumColors will be set to 0 and the function will
        // return NULL.
  void Take(const cPalette &Palette, tIndexes *Indexes = NULL);
  };

class cBitmap : public cPalette {
private:
  cFont *font;
  eDvbFont fontType;
  char *bitmap;
  bool clearWithBackground;
protected:
  int width, height;
  int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
public:
  cBitmap(int Width, int Height, int Bpp, bool ClearWithBackground = true);
  virtual ~cBitmap();
  bool ClearWithBackground(void) { return clearWithBackground; }
  eDvbFont SetFont(eDvbFont Font);
  bool Dirty(int &x1, int &y1, int &x2, int &y2);
  void SetIndex(int x, int y, char Index);
  void SetPixel(int x, int y, eDvbColor Color);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  int Width(void) { return width; }
  int Width(unsigned char c);
  int Width(const char *s);
  int Height(void) { return height; }
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground);
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color);
  void Clean(void);
  void Clear(void);
  const char *Data(int x, int y);
  };

#define MAXNUMWINDOWS 7 // OSD windows are counted 1...7

class cWindow : public cBitmap {
private:
  int handle;  // the index within the OSD's window array (0...MAXNUMWINDOWS - 1)
  int x0, y0;
  int bpp;
  bool tiled;
  bool shown;
public:
  cWindow(int Handle, int x, int y, int w, int h, int Bpp, bool ClearWithBackground, bool Tiled);
  int X0(void) { return x0; }
  int Y0(void) { return y0; }
  int Bpp(void) { return bpp; }
  bool Tiled(void) { return tiled; }
  bool Shown(void) { bool s = shown; shown = true; return s; }
  int Handle(void) { return handle; }
  bool Contains(int x, int y);
  void Relocate(int x, int y);
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground);
  const char *Data(int x, int y);
  };

typedef int tWindowHandle;

// '-1' is used as an error return value!
#define ALL_WINDOWS         (-2)
#define ALL_TILED_WINDOWS   (-3)
#define LAST_CREATED_WINDOW (-4)

class cOsdBase {
private:
  int numWindows;
  int x0, y0;
  cWindow *window[MAXNUMWINDOWS];
  cWindow *GetWindow(int x, int y);
  cWindow *GetWindow(tWindowHandle Window);
protected:
  cWindow *GetWindowNr(int i) { return i < MAXNUMWINDOWS ? window[i] : NULL; }
  int NumWindows(void) { return numWindows; }
  int X0(void) { return x0; }
  int Y0(void) { return y0; }
  virtual bool OpenWindow(cWindow *Window) = 0;
       // Opens the window on the OSD hardware, without actually showing it (the
       // initial state shall be "hidden").
  virtual void CommitWindow(cWindow *Window) = 0;
       // Write any modified data and color definitions to the OSD hardware.
       // Use the window's Dirty() function to find out which area of data
       // actually needs to be transferred. If the window has not yet been explicitly
       // shown through a call to ShowWindow(), no visible activity shall take place.
  virtual void ShowWindow(cWindow *Window) = 0;
       // Make the window actually visible on the OSD hardware.
  virtual void HideWindow(cWindow *Window, bool Hide) = 0;
       // Temporarily hide the window (if 'Hide' is 'true') or make a previously
       // hidden window visible again (if 'Hide' is 'false').
  virtual void MoveWindow(cWindow *Window, int x, int y) = 0;
       // Move the window to a new location.
  virtual void CloseWindow(cWindow *Window) = 0;
       // Close the window and release any OSD hardware resources allocated for it.
public:
  cOsdBase(int x, int y);
       // Initializes the OSD, starting at screen coordinates (x, y).
  virtual ~cOsdBase();
       // Destroys all windows and shuts down the OSD.
  tWindowHandle Create(int x, int y, int w, int h, int Bpp, bool ClearWithBackground = true, bool Tiled = true);
       // Creates a window at coordinates (x, y), which are relative to the OSD's
       // origin given in the constructor, with the given width, height and color
       // depth. ClearWithBackground controls whether the window will be filled with
       // clrBackground when it is cleared. Setting this to 'false' may be useful
       // for windows that don't need clrBackground but want to save this color
       // palette entry for a different color. Tiled controls whether this will
       // be part of a multi section OSD (with several windows that all have
       // different color depths and palettes and form one large OSD area), or
       // whether this is a "standalone" window that will be drawn "in front"
       // of any windows defined *after* this one (this can be used for highlighting
       // certain parts of the OSD, as would be done in a 'cursor').
       // Returns a handle that can be used to identify this window.
  void AddColor(eDvbColor Color, tWindowHandle Window = LAST_CREATED_WINDOW);
       // Adds the Color to the color palette of the given window if it is not
       // already contained in the palette (and if the palette still has free
       // slots for new colors). The default value LAST_CREATED_WINDOW will
       // access the most recently created window, without the need of explicitly
       // using a window handle.
  void Flush(void);
       // Actually commits all data of all windows to the OSD.
  void Clear(tWindowHandle Window = ALL_TILED_WINDOWS);
       // Clears the given window. If ALL_TILED_WINDOWS is given, only the tiled
       // windows will be cleared, leaving the standalone windows untouched. If
       // ALL_WINDOWS is given, the standalone windows will also be cleared.
  void Fill(int x1, int y1, int x2, int y2, eDvbColor Color, tWindowHandle Window = ALL_TILED_WINDOWS);
       // Fills the rectangle defined by the upper left (x1, y2) and lower right
       // (x2, y2) corners with the given Color. If a specific window is given,
       // the coordinates are relative to that window's upper left corner.
       // Otherwise they are relative to the upper left corner of the entire OSD.
       // If all tiled windows are selected, only that window which contains the
       // point (x1, y1) will actually be filled.
  void SetBitmap(int x, int y, const cBitmap &Bitmap, tWindowHandle Window = ALL_TILED_WINDOWS);
       // Sets the pixels within the given window with the data from the given
       // Bitmap. See Fill() for details about the coordinates.
  int Width(unsigned char c);
       // Returns the width (in pixels) of the given character in the current font.
  int Width(const char *s);
       // Returns the width (in pixels) of the given string in the current font.
  eDvbFont SetFont(eDvbFont Font);
       // Sets the current font for subsequent Width() and Text() operations.
  void Text(int x, int y, const char *s, eDvbColor ColorFg = clrWhite, eDvbColor ColorBg = clrBackground, tWindowHandle Window = ALL_TILED_WINDOWS);
       // Writes the given string at coordinates (x, y) with the given foreground
       // and background color into the given window (see Fill() for details
       // about the coordinates).
  void Relocate(tWindowHandle Window, int x, int y, int NewWidth = -1, int NewHeight = -1);
       // Moves the given window to the new location at (x, y). If NewWidth and
       // NewHeight are also given, the window will also be resized to the new
       // width and height.
  void Hide(tWindowHandle Window);
       // Hides the given window.
  void Show(tWindowHandle Window);
       // Shows the given window.
  };

#endif //__OSDBASE_H
