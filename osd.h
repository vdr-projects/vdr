/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 1.49 2005/06/19 10:35:25 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include <stdio.h>
#include <stdint.h>
#include "font.h"

#define MAXNUMCOLORS 256

enum {
                   //AARRGGBB
  clrTransparent = 0x00000000,
  clrGray50      = 0x7F000000, // 50% gray
  clrBlack       = 0xFF000000,
  clrRed         = 0xFFFC1414,
  clrGreen       = 0xFF24FC24,
  clrYellow      = 0xFFFCC024,
  clrMagenta     = 0xFFB000FC,
  clrBlue        = 0xFF0000FC,
  clrCyan        = 0xFF00FCFC,
  clrWhite       = 0xFFFCFCFC,
  };

enum eOsdError { oeOk,
                 oeTooManyAreas,
                 oeTooManyColors,
                 oeBppNotSupported,
                 oeAreasOverlap,
                 oeWrongAlignment,
                 oeOutOfMemory,
                 oeUnknown,
               };

typedef uint32_t tColor;
typedef uint8_t tIndex;

class cPalette {
private:
  tColor color[MAXNUMCOLORS];
  int bpp;
  int maxColors, numColors;
  bool modified;
protected:
  typedef tIndex tIndexes[MAXNUMCOLORS];
public:
  cPalette(int Bpp = 8);
        ///< Initializes the palette with the given color depth.
  int Bpp(void) { return bpp; }
  void Reset(void);
        ///< Resets the palette, making it contain no colors.
  int Index(tColor Color);
        ///< Returns the index of the given Color (the first color has index 0).
        ///< If Color is not yet contained in this palette, it will be added if
        ///< there is a free slot. If the color can't be added to this palette,
        ///< 0 will be returned.
  tColor Color(int Index) { return Index < maxColors ? color[Index] : 0; }
        ///< Returns the color at the given Index. If Index is outside the valid
        ///< range, 0 will be returned.
  void SetBpp(int Bpp);
        ///< Sets the color depth of this palette to the given value.
        ///< The palette contents will be reset, so that it contains no colors.
  void SetColor(int Index, tColor Color);
        ///< Sets the palette entry at Index to Color. If Index is larger than
        ///< the number of currently used entries in this palette, the entries
        ///< in between will have undefined values.
  const tColor *Colors(int &NumColors);
        ///< Returns a pointer to the complete color table and stores the
        ///< number of valid entries in NumColors. If no colors have been
        ///< stored yet, NumColors will be set to 0 and the function will
        ///< return NULL.
  void Take(const cPalette &Palette, tIndexes *Indexes = NULL, tColor ColorFg = 0, tColor ColorBg = 0);
        ///< Takes the colors from the given Palette and adds them to this palette,
        ///< using existing entries if possible. If Indexes is given, it will be
        ///< filled with the index values that each color of Palette has in this
        ///< palette. If either of ColorFg or ColorBg is not zero, the first color
        ///< in Palette will be taken as ColorBg, and the second color will become
        ///< ColorFg.
  void Replace(const cPalette &Palette);
        ///< Replaces the colors of this palette with the colors from the given
        ///< palette.
  };

enum eTextAlignment { taCenter  = 0x00,
                      taLeft    = 0x01,
                      taRight   = 0x02,
                      taTop     = 0x04,
                      taBottom  = 0x08,
                      taDefault = taTop | taLeft
                    };

class cBitmap : public cPalette {
private:
  tIndex *bitmap;
  int x0, y0;
  int width, height;
  int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
public:
  cBitmap(int Width, int Height, int Bpp, int X0 = 0, int Y0 = 0);
       ///< Creates a bitmap with the given Width, Height and color depth (Bpp).
       ///< X0 and Y0 define the offset at which this bitmap will be located on the OSD.
       ///< All coordinates given in the other functions will be relative to
       ///< this offset (unless specified otherwise).
  cBitmap(const char *FileName);
       ///< Creates a bitmap and loads an XPM image from the given file.
  cBitmap(char *Xpm[]);
       ///< Creates a bitmap from the given XPM data.
  virtual ~cBitmap();
  int X0(void) const { return x0; }
  int Y0(void) const { return y0; }
  int Width(void) const { return width; }
  int Height(void) const { return height; }
  void SetSize(int Width, int Height);
       ///< Sets the size of this bitmap to the given values. Any previous
       ///< contents of the bitmap will be lost. If Width and Height are the same
       ///< as the current values, nothing will happen and the bitmap remains
       ///< unchanged.
  bool Contains(int x, int y) const;
       ///< Returns true if this bitmap contains the point (x, y).
  bool Covers(int x1, int y1, int x2, int y2) const;
       ///< Returns true if the rectangle defined by the given coordinates
       ///< completely covers this bitmap.
  bool Intersects(int x1, int y1, int x2, int y2) const;
       ///< Returns true if the rectangle defined by the given coordinates
       ///< intersects with this bitmap.
  bool Dirty(int &x1, int &y1, int &x2, int &y2);
       ///< Tells whether there is a dirty area and returns the bounding
       ///< rectangle of that area (relative to the bitmaps origin).
  void Clean(void);
       ///< Marks the dirty area as clean.
  bool LoadXpm(const char *FileName);
       ///< Calls SetXpm() with the data from the file FileName.
       ///< Returns true if the operation was successful.
  bool SetXpm(char *Xpm[], bool IgnoreNone = false);
       ///< Sets this bitmap to the given XPM data. Any previous bitmap or
       ///< palette data will be overwritten with the new data.
       ///< If IgnoreNone is true, a "none" color entry will be ignored.
       ///< Only set IgnoreNone to true if you know that there is a "none"
       ///< color entry in the XPM data and that this entry is not used!
       ///< If SetXpm() is called with IgnoreNone set to false and the XPM
       ///< data contains an unused "none" entry, it will be automatically
       ///< called again with IgnoreNone set to true.
       ///< Returns true if the operation was successful.
  void SetIndex(int x, int y, tIndex Index);
       ///< Sets the index at the given coordinates to Index.
       ///< Coordinates are relative to the bitmap's origin.
  void DrawPixel(int x, int y, tColor Color);
       ///< Sets the pixel at the given coordinates to the given Color, which is
       ///< a full 32 bit ARGB value.
       ///< If the coordinates are outside the bitmap area, no pixel will be set.
  void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false);
       ///< Sets the pixels in this bitmap with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at (x, y).
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color).
  void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault);
       ///< Draws the given string at coordinates (x, y) with the given foreground
       ///< and background color and font. If Width and Height are given, the text
       ///< will be drawn into a rectangle with the given size and the given
       ///< Alignment (default is top-left). If ColorBg is clrTransparent, no
       ///< background pixels will be drawn, which allows drawing "transparent" text.
  void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color);
       ///< Draws a filled rectangle defined by the upper left (x1, y1) and lower right
       ///< (x2, y2) corners with the given Color. If the rectangle covers the entire
       ///< bitmap area, the color palette will be reset, so that new colors can be
       ///< used for drawing.
  void DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants = 0);
       ///< Draws a filled ellipse defined by the upper left (x1, y1) and lower right
       ///< (x2, y2) corners with the given Color. Quadrants controls which parts of
       ///< the ellipse are actually drawn:
       ///< 0       draws the entire ellipse
       ///< 1..4    draws only the first, second, third or fourth quadrant, respectively
       ///< 5..8    draws the right, top, left or bottom half, respectively
       ///< -1..-8  draws the inverted part of the given quadrant(s)
       ///< If Quadrants is not 0, the coordinates are those of the actual area, not
       ///< the full circle!
  void DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type);
       ///< Draws a "slope" into the rectangle defined by the upper left (x1, y1) and
       ///< lower right (x2, y2) corners with the given Color. Type controls the
       ///< direction of the slope and which side of it will be drawn:
       ///< 0: horizontal, rising,  lower
       ///< 1: horizontal, rising,  upper
       ///< 2: horizontal, falling, lower
       ///< 3: horizontal, falling, upper
       ///< 4: vertical,   rising,  lower
       ///< 5: vertical,   rising,  upper
       ///< 6: vertical,   falling, lower
       ///< 7: vertical,   falling, upper
  const tIndex *Data(int x, int y);
       ///< Returns the address of the index byte at the given coordinates.
  };

struct tArea {
  int x1, y1, x2, y2;
  int bpp;
  int Width(void) const { return x2 - x1 + 1; }
  int Height(void) const { return y2 - y1 + 1; }
  bool Intersects(const tArea &Area) const { return !(x2 < Area.x1 || x1 > Area.x2 || y2 < Area.y1 || y1 > Area.y2); }
  };

#define MAXOSDAREAS 16

class cOsd {
  friend class cOsdProvider;
private:
  static int isOpen;
  cBitmap *savedRegion;
  cBitmap *bitmaps[MAXOSDAREAS];
  int numBitmaps;
  int left, top, width, height;
protected:
  cOsd(int Left, int Top);
       ///< Initializes the OSD with the given coordinates.
       ///< By default it is assumed that the full area will be able to display
       ///< full 32 bit graphics (ARGB with eight bit for each color and the alpha
       ///< value, repectively). However, the actual hardware in use may not be
       ///< able to display such a high resolution OSD, so there is an option to
       ///< divide the full OSD area into several sub-areas with lower color depths
       ///< and individual palettes. The sub-areas need not necessarily cover the
       ///< entire OSD area, but only the OSD area actually covered by sub-areas
       ///< will be available for drawing.
       ///< At least one area must be defined in order to set the actual width and
       ///< height of the OSD. Also, the caller must first try to use an area that
       ///< consists of only one sub-area that covers the entire drawing space,
       ///< and should require only the minimum necessary color depth. This is
       ///< because a derived cOsd class may or may not be able to handle more
       ///< than one area.
public:
  virtual ~cOsd();
       ///< Shuts down the OSD.
  static int IsOpen(void) { return isOpen; }
  int Left(void) { return left; }
  int Top(void) { return top; }
  int Width(void) { return width; }
  int Height(void) { return height; }
  cBitmap *GetBitmap(int Area);
       ///< Returns a pointer to the bitmap for the given Area, or NULL if no
       ///< such bitmap exists.
  virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
       ///< Checks whether the OSD can display the given set of sub-areas.
       ///< The return value indicates whether a call to SetAreas() with this
       ///< set of areas will succeed. CanHandleAreas() may be called with an
       ///< OSD that is already in use with other areas and will not interfere
       ///< with the current operation of the OSD.
       ///< A derived class must first call the base class CanHandleAreas()
       ///< to check the basic conditions, like not overlapping etc.
  virtual eOsdError SetAreas(const tArea *Areas, int NumAreas);
       ///< Sets the sub-areas to the given areas.
       ///< The return value indicates whether the operation was successful.
       ///< If an error is reported, nothing will have changed and the previous
       ///< OSD (if any) will still be displayed as before.
       ///< If the OSD has been divided into several sub-areas, all areas that
       ///< are part of the rectangle that surrounds a given drawing operation
       ///< will be drawn into, with the proper offsets.
  virtual void SaveRegion(int x1, int y1, int x2, int y2);
       ///< Saves the region defined by the given coordinates for later restoration
       ///< through RestoreRegion(). Only one saved region can be active at any
       ///< given time.
  virtual void RestoreRegion(void);
       ///< Restores the region previously saved by a call to SaveRegion().
       ///< If SaveRegion() has not been called before, nothing will happen.
  virtual eOsdError SetPalette(const cPalette &Palette, int Area);
       ///< Sets the Palette for the given Area (the first area is numbered 0).
  virtual void DrawPixel(int x, int y, tColor Color);
       ///< Sets the pixel at the given coordinates to the given Color, which is
       ///< a full 32 bit ARGB value.
       ///< If the OSD area has been divided into separate sub-areas, and the
       ///< given coordinates don't fall into any of these sub-areas, no pixel will
       ///< be set.
  virtual void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false);
       ///< Sets the pixels in the OSD with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at (x, y).
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color).
  virtual void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault);
       ///< Draws the given string at coordinates (x, y) with the given foreground
       ///< and background color and font. If Width and Height are given, the text
       ///< will be drawn into a rectangle with the given size and the given
       ///< Alignment (default is top-left). If ColorBg is clrTransparent, no
       ///< background pixels will be drawn, which allows drawing "transparent" text.
  virtual void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color);
       ///< Draws a filled rectangle defined by the upper left (x1, y1) and lower right
       ///< (x2, y2) corners with the given Color.
  virtual void DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants = 0);
       ///< Draws a filled ellipse defined by the upper left (x1, y1) and lower right
       ///< (x2, y2) corners with the given Color. Quadrants controls which parts of
       ///< the ellipse are actually drawn:
       ///< 0       draws the entire ellipse
       ///< 1..4    draws only the first, second, third or fourth quadrant, respectively
       ///< 5..8    draws the right, top, left or bottom half, respectively
       ///< -1..-8  draws the inverted part of the given quadrant(s)
       ///< If Quadrants is not 0, the coordinates are those of the actual area, not
       ///< the full circle!
  virtual void DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type);
       ///< Draws a "slope" into the rectangle defined by the upper left (x1, y1) and
       ///< lower right (x2, y2) corners with the given Color. Type controls the
       ///< direction of the slope and which side of it will be drawn:
       ///< 0: horizontal, rising,  lower
       ///< 1: horizontal, rising,  upper
       ///< 2: horizontal, falling, lower
       ///< 3: horizontal, falling, upper
       ///< 4: vertical,   rising,  lower
       ///< 5: vertical,   rising,  upper
       ///< 6: vertical,   falling, lower
       ///< 7: vertical,   falling, upper
  virtual void Flush(void);
       ///< Actually commits all data to the OSD hardware.
  };

class cOsdProvider {
private:
  static cOsdProvider *osdProvider;
protected:
  virtual cOsd *CreateOsd(int Left, int Top) = 0;
      ///< Returns a pointer to a newly created cOsd object, which will be located
      ///< at the given coordinates.
public:
  cOsdProvider(void);
      //XXX maybe parameter to make this one "sticky"??? (frame-buffer etc.)
  virtual ~cOsdProvider();
  static cOsd *NewOsd(int Left, int Top);
      ///< Returns a pointer to a newly created cOsd object, which will be located
      ///< at the given coordinates. When the cOsd object is no longer needed, the
      ///< caller must delete it. If the OSD is already in use, or there is no OSD
      ///< provider, a dummy OSD is returned so that the caller may always use the
      ///< returned pointer without having to check it every time it is accessed.
  static void Shutdown(void);
      ///< Shuts down the OSD provider facility by deleting the current OSD provider.
  };

class cTextScroller {
private:
  cOsd *osd;
  int left, top, width, height;
  const cFont *font;
  tColor colorFg, colorBg;
  int offset, shown;
  cTextWrapper textWrapper;
  void DrawText(void);
public:
  cTextScroller(void);
  cTextScroller(cOsd *Osd, int Left, int Top, int Width, int Height, const char *Text, const cFont *Font, tColor ColorFg, tColor ColorBg);
  void Set(cOsd *Osd, int Left, int Top, int Width, int Height, const char *Text, const cFont *Font, tColor ColorFg, tColor ColorBg);
  void Reset(void);
  int Left(void) { return left; }
  int Top(void) { return top; }
  int Width(void) { return width; }
  int Height(void) { return height; }
  int Total(void) { return textWrapper.Lines(); }
  int Offset(void) { return offset; }
  int Shown(void) { return shown; }
  bool CanScroll(void) { return CanScrollUp() || CanScrollDown(); }
  bool CanScrollUp(void) { return offset > 0; }
  bool CanScrollDown(void) { return offset + shown < Total(); }
  void Scroll(bool Up, bool Page);
  };

#endif //__OSD_H
