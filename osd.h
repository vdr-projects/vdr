/*
 * osd.h: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.h 5.3 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __OSD_H
#define __OSD_H

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include "config.h"
#include "font.h"
#include "thread.h"
#include "tools.h"

#define OSD_LEVEL_DEFAULT     0
#define OSD_LEVEL_SUBTITLES  10

#define MAXNUMCOLORS 256
#define ALPHA_TRANSPARENT  0x00
#define ALPHA_OPAQUE       0xFF
#define IS_OPAQUE(c)       ((c >> 24) == ALPHA_OPAQUE)
#define TEXT_ALIGN_BORDER  10 // fraction of the font height used for sizing border

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

enum eOsdError { oeOk, // see also OsdErrorTexts in osd.c
                 oeTooManyAreas,
                 oeTooManyColors,
                 oeBppNotSupported,
                 oeAreasOverlap,
                 oeWrongAlignment,
                 oeOutOfMemory,
                 oeWrongAreaSize,
                 oeUnknown,
               };

typedef uint32_t tColor; // see also font.h
typedef uint8_t tIndex;

inline tColor ArgbToColor(uint8_t A, uint8_t R, uint8_t G, uint8_t B)
{
  return (tColor(A) << 24) | (tColor(R) << 16) | (tColor(G) << 8) | B;
}

inline tColor RgbToColor(uint8_t R, uint8_t G, uint8_t B)
{
  return (tColor(R) << 16) | (tColor(G) << 8) | B;
}

inline tColor RgbToColor(double R, double G, double B)
{
  return RgbToColor(uint8_t(0xFF * R), uint8_t(0xFF * G), uint8_t(0xFF * B));
}

tColor RgbShade(tColor Color, double Factor);
   ///< Returns a brighter (Factor > 0) or darker (Factor < 0) version
   ///< of the given Color.
   ///< If Factor is 0.0, the return value is the unchanged Color,
   ///< If Factor is 1.0, white is returned.
   ///< If Factor is -1.0, black is returned.
   ///< The alpha value of Color is returned unchanged.

tColor HsvToColor(double H, double S, double V);
   ///< Converts the given Hue (0..360), Saturation (0..1) and Value (0..1)
   ///< to an RGB tColor value. The alpha value of the result is 0x00, so
   ///< the caller may need to set it accordingly.

tColor AlphaBlend(tColor ColorFg, tColor ColorBg, uint8_t AlphaLayer = ALPHA_OPAQUE);

class cPalette {
private:
  tColor color[MAXNUMCOLORS];
  int bpp;
  int maxColors, numColors;
  bool modified;
  double antiAliasGranularity;
protected:
  typedef tIndex tIndexes[MAXNUMCOLORS];
public:
  cPalette(int Bpp = 8);
        ///< Initializes the palette with the given color depth.
  virtual ~cPalette();
  void SetAntiAliasGranularity(uint FixedColors, uint BlendColors);
        ///< Allows the system to optimize utilization of the limited color
        ///< palette entries when generating blended colors for anti-aliasing.
        ///< FixedColors is the maximum number of colors used, and BlendColors
        ///< is the maximum number of foreground/background color combinations
        ///< used with anti-aliasing. If this function is not called with
        ///< useful values, the palette may be filled up with many shades of
        ///< a single color combination, and may not be able to serve all
        ///< requested colors. By default the palette assumes there will be
        ///< 10 fixed colors and 10 color combinations.
  int Bpp(void) const { return bpp; }
  void Reset(void);
        ///< Resets the palette, making it contain no colors.
  int Index(tColor Color);
        ///< Returns the index of the given Color (the first color has index 0).
        ///< If Color is not yet contained in this palette, it will be added if
        ///< there is a free slot. If the color can't be added to this palette,
        ///< the closest existing color will be returned.
  tColor Color(int Index) const { return Index < maxColors ? color[Index] : 0; }
        ///< Returns the color at the given Index. If Index is outside the valid
        ///< range, 0 will be returned.
  void SetBpp(int Bpp);
        ///< Sets the color depth of this palette to the given value.
        ///< The palette contents will be reset, so that it contains no colors.
  void SetColor(int Index, tColor Color);
        ///< Sets the palette entry at Index to Color. If Index is larger than
        ///< the number of currently used entries in this palette, the entries
        ///< in between will have undefined values.
  const tColor *Colors(int &NumColors) const;
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
  tColor Blend(tColor ColorFg, tColor ColorBg, uint8_t Level) const;
        ///< Determines a color that consists of a linear blend between ColorFg
        ///< and ColorBg. If Level is 0, the result is ColorBg, if it is 255,
        ///< the result is ColorFg. If SetAntiAliasGranularity() has been called previously,
        ///< Level will be mapped to a limited range of levels that allow to make best
        ///< use of the palette entries.
  int ClosestColor(tColor Color, int MaxDiff = INT_MAX) const;
        ///< Returns the index of a color in this palette that is closest to the given
        ///< Color. MaxDiff can be used to control the maximum allowed color difference.
        ///< If no color with a maximum difference of MaxDiff can be found, -1 will
        ///< be returned. With the default value of INT_MAX, there will always be
        ///< a valid color index returned, but the color may be completely different.
  };

enum eTextAlignment { taCenter  = 0x00,
                      taLeft    = 0x01,
                      taRight   = 0x02,
                      taTop     = 0x04,
                      taBottom  = 0x08,
                      taBorder  = 0x10, // keeps some distance from the left or right alignment edge
                      taDefault = taTop | taLeft
                    };

class cFont;

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
  cBitmap(const char *const Xpm[]);
       ///< Creates a bitmap from the given XPM data.
  virtual ~cBitmap() override;
  int X0(void) const { return x0; }
  int Y0(void) const { return y0; }
  int Width(void) const { return width; }
  int Height(void) const { return height; }
  void SetSize(int Width, int Height);
       ///< Sets the size of this bitmap to the given values. Any previous
       ///< contents of the bitmap will be lost. If Width and Height are the same
       ///< as the current values, nothing will happen and the bitmap remains
       ///< unchanged.
  void SetOffset(int X0, int Y0) { x0 = X0; y0 = Y0; }
       ///< Sets the offset of this bitmap to the given values.
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
  bool SetXpm(const char *const Xpm[], bool IgnoreNone = false);
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
  void Fill(tIndex Index);
       ///< Fills the bitmap data with the given Index.
  void DrawPixel(int x, int y, tColor Color);
       ///< Sets the pixel at the given coordinates to the given Color, which is
       ///< a full 32 bit ARGB value.
       ///< If the coordinates are outside the bitmap area, no pixel will be set.
  void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false);
       ///< Sets the pixels in this bitmap with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at (x, y).
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color). ReplacePalette controls whether the target
       ///< area shall have its palette replaced with the one from Bitmap.
       ///< If Overlay is true, any pixel in Bitmap that has color index 0 will
       ///< not overwrite the corresponding pixel in the target area.
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
       ///< -1..-4  draws the inverted part of the given quadrant
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
  const tIndex *Data(int x, int y) const;
       ///< Returns the address of the index byte at the given coordinates.
  tColor GetColor(int x, int y) const { return Color(*Data(x, y)); }
       ///< Returns the color at the given coordinates.
  void ReduceBpp(const cPalette &Palette);
       ///< Reduces the color depth of the bitmap to that of the given Palette.
       ///< If Palette's color depth is not smaller than the bitmap's current
       ///< color depth, or if it is not one of 4bpp or 2bpp, nothing happens. After
       ///< reducing the color depth the current palette is replaced with
       ///< the given one.
  void ShrinkBpp(int NewBpp);
       ///< Shrinks the color depth of the bitmap to NewBpp by keeping only
       ///< the 2^NewBpp most frequently used colors as defined in the current palette.
       ///< If NewBpp is not smaller than the bitmap's current color depth,
       ///< or if it is not one of 4bpp or 2bpp, nothing happens.
  cBitmap *Scaled(double FactorX, double FactorY, bool AntiAlias = false) const;
       ///< Creates a copy of this bitmap, scaled by the given factors.
       ///< If AntiAlias is true and either of the factors is greater than 1.0,
       ///< anti-aliasing is applied. This will also set the color depth of the
       ///< returned bitmap to 8bpp.
       ///< The caller must delete the returned bitmap once it is no longer used.
  };

struct tArea {
  int x1, y1, x2, y2;
  int bpp;
  int Width(void) const { return x2 - x1 + 1; }
  int Height(void) const { return y2 - y1 + 1; }
  bool Intersects(const tArea &Area) const { return !(x2 < Area.x1 || x1 > Area.x2 || y2 < Area.y1 || y1 > Area.y2); }
  };

class cPoint {
private:
  int x;
  int y;
public:
  cPoint(void) { x = y = 0; }
  cPoint(int X, int Y) { x = X; y = Y; }
  cPoint(const cPoint &Point) { x = Point.X(); y = Point.Y(); }
  bool operator==(const cPoint &Point) const { return x == Point.X() && y == Point.Y(); }
  bool operator!=(const cPoint &Point) const { return !(*this == Point); }
  cPoint operator-(void) const { return cPoint(-x, -y); }
  cPoint operator-(const cPoint &Point) const { return cPoint(x - Point.X(), y - Point.Y()); }
  int X(void) const { return x; }
  int Y(void) const { return y; }
  void SetX(int X) { x = X; }
  void SetY(int Y) { y = Y; }
  void Set(int X, int Y) { x = X; y = Y; }
  void Set(const cPoint &Point) { x = Point.X(); y = Point.Y(); }
  void Shift(int Dx, int Dy) { x += Dx; y += Dy; }
  void Shift(const cPoint &Dp) { x += Dp.X(); y += Dp.Y(); }
  cPoint Shifted(int Dx, int Dy) const { cPoint p(*this); p.Shift(Dx, Dy); return p; }
  cPoint Shifted(const cPoint &Dp) const { cPoint p(*this); p.Shift(Dp); return p; }
  };

class cSize {
private:
  int width;
  int height;
public:
  cSize(void) { width = height = 0; }
  cSize(int Width, int Height) { width = Width; height = Height; }
  cSize(const cSize &Size) { width = Size.Width(); height = Size.Height(); }
  bool operator==(const cSize &Size) const { return width == Size.Width() && height == Size.Height(); }
  bool operator!=(const cSize &Size) const { return !(*this == Size); }
  bool operator<(const cSize &Size) const { return width < Size.Width() && height < Size.Height(); }
  int Width(void) const { return width; }
  int Height(void) const { return height; }
  void SetWidth(int Width) { width = Width; }
  void SetHeight(int Height) { height = Height; }
  void Set(int Width, int Height) { width = Width; height = Height; }
  void Set(const cSize &Size) { width = Size.Width(); height = Size.Height(); }
  bool Contains(const cPoint &Point) const { return 0 <= Point.X() && 0 <= Point.Y() && Point.X() < width && Point.Y() < height; }
  void Grow(int Dw, int Dh) { width += 2 * Dw; height += 2 * Dh; }
  cSize Grown(int Dw, int Dh) const { cSize s(*this); s.Grow(Dw, Dh); return s; }
  };

class cRect {
private:
  cPoint point;
  cSize size;
public:
  static const cRect Null;
  cRect(void): point(0, 0), size(0, 0) {}
  cRect(int X, int Y, int Width, int Height): point(X, Y), size(Width, Height) {}
  cRect(const cPoint &Point, const cSize &Size): point(Point), size(Size) {}
  cRect(const cSize &Size): point(0, 0), size(Size) {}
  cRect(const cRect &Rect): point(Rect.Point()), size(Rect.Size()) {}
  bool operator==(const cRect &Rect) const { return point == Rect.Point() && size == Rect.Size(); }
  bool operator!=(const cRect &Rect) const { return !(*this == Rect); }
  int X(void) const { return point.X(); }
  int Y(void) const { return point.Y(); }
  int Width(void) const { return size.Width(); }
  int Height(void) const { return size.Height(); }
  int Left(void) const { return X(); }
  int Top(void) const { return Y(); }
  int Right(void) const { return X() + Width() - 1; }
  int Bottom(void) const { return Y() + Height() - 1; }
  const cPoint &Point(void) const { return point; }
  const cSize &Size(void) const { return size; }
  void Set(int X, int Y, int Width, int Height) { point.Set(X, Y); size.Set(Width, Height); }
  void Set(cPoint Point, cSize Size) { point.Set(Point); size.Set(Size); }
  void SetPoint(int X, int Y) { point.Set(X, Y); }
  void SetPoint(const cPoint &Point) { point.Set(Point); }
  void SetSize(int Width, int Height) { size.Set(Width, Height); }
  void SetSize(const cSize &Size) { size.Set(Size); }
  void SetX(int X) { point.SetX(X); }
  void SetY(int Y) { point.SetY(Y); }
  void SetWidth(int Width) { size.SetWidth(Width); }
  void SetHeight(int Height) { size.SetHeight(Height); }
  void SetLeft(int Left) { SetWidth(Width() + X() - Left); SetX(Left); }
  void SetTop(int Top) { SetHeight(Height() + Y() - Top); SetY(Top); }
  void SetRight(int Right) { SetWidth(Right - X() + 1); }
  void SetBottom(int Bottom) { SetHeight(Bottom - Y() + 1); }
  void Shift(int Dx, int Dy) { point.Shift(Dx, Dy); }
  void Shift(const cPoint &Dp) { point.Shift(Dp); }
  cRect Shifted(int Dx, int Dy) const { cRect r(*this); r.Shift(Dx, Dy); return r; }
  cRect Shifted(const cPoint &Dp) const { cRect r(*this); r.Shift(Dp); return r; }
  void Grow(int Dx, int Dy);
       ///< Grows the rectangle by the given number of pixels in either direction.
       ///< A negative value will shrink the rectangle.
  cRect Grown(int Dw, int Dh) const { cRect r(*this); r.Grow(Dw, Dh); return r; }
  bool Contains(const cPoint &Point) const;
       ///< Returns true if this rectangle contains Point.
  bool Contains(const cRect &Rect) const;
       ///< Returns true if this rectangle completely contains Rect.
  bool Intersects(const cRect &Rect) const;
       ///< Returns true if this rectangle intersects with Rect.
  cRect Intersected(const cRect &Rect) const;
       ///< Returns the intersection of this rectangle and the given Rect.
  void Combine(const cRect &Rect);
       ///< Combines this rectangle with the given Rect.
  cRect Combined(const cRect &Rect) const { cRect r(*this); r.Combine(Rect); return r; }
       ///< Returns the surrounding rectangle that contains this rectangle and the
       ///< given Rect.
  void Combine(const cPoint &Point);
       ///< Combines this rectangle with the given Point.
  cRect Combined(const cPoint &Point) const { cRect r(*this); r.Combine(Point); return r; }
       ///< Returns the surrounding rectangle that contains this rectangle and the
       ///< given Point.
  bool IsEmpty(void) const { return Width() <= 0 || Height() <= 0; }
       ///< Returns true if this rectangle is empty.
  };

class cImage {
private:
  cSize size;
  tColor *data;
public:
  cImage(void);
  cImage(const cImage &Image);
  cImage(const cSize &Size, const tColor *Data = NULL);
       ///< Creates an image with the given Size and allocates the necessary memory
       ///< to copy the pixels pointed to by Data, which is a sequence of
       ///< (Size.Width() * Size.Height()) tColor values.
       ///< If Data is NULL, the allocated memory is not initialized.
       ///< The alpha value of the Image's pixels is taken into account, so it has to be
       ///< greater than 0 for the image to be visible.
  virtual ~cImage();
  const cSize &Size(void) const { return size; }
  int Width(void) const { return size.Width(); }
  int Height(void) const { return size.Height(); }
  const tColor *Data(void) const { return data; }
  tColor GetPixel(const cPoint &Point) const { return data[size.Width() * Point.Y() + Point.X()]; }
       ///< Returns the pixel value at the given Point.
       ///< For performance reasons there is no range check here, so the caller
       ///< must make sure that the Point is within the images size.
  void SetPixel(const cPoint &Point, tColor Color) { data[size.Width() * Point.Y() + Point.X()] = Color; }
       ///< Sets the pixel at the given Point to Color.
       ///< For performance reasons there is no range check here, so the caller
       ///< must make sure that the Point is within the images size.
  void Clear(void);
       ///< Clears the image data by setting all pixels to be fully transparent.
  void Fill(tColor Color);
       ///< Fills the image data with the given Color.
  cImage *Scaled(double FactorX, double FactorY, bool AntiAlias = false) const;
       ///< Creates a copy of this image, scaled by the given factors.
       ///< If AntiAlias is true and either of the factors is greater than 1.0,
       ///< anti-aliasing is applied.
       ///< The caller must delete the returned image once it is no longer used.
  };

#define MAXPIXMAPLAYERS    8

class cPixmap {
  friend class cOsd;
  friend class cPixmapMutexLock;
private:
  static cMutex mutex;
  int layer;
  int alpha;
  bool tile;
  cRect viewPort;
  cRect drawPort;
  cRect dirtyViewPort;
  cRect dirtyDrawPort;
protected:
  virtual ~cPixmap() {}
  void MarkViewPortDirty(const cRect &Rect);
       ///< Marks the given rectangle of the view port of this pixmap as dirty.
       ///< Rect is combined with the existing dirtyViewPort rectangle.
       ///< The coordinates of Rect are given in absolute OSD values.
  void MarkViewPortDirty(const cPoint &Point);
       ///< Marks the given point of the view port of this pixmap as dirty.
       ///< Point is combined with the existing dirtyViewPort rectangle.
       ///< The coordinates of Point are given in absolute OSD values.
  void MarkDrawPortDirty(const cRect &Rect);
       ///< Marks the given rectangle of the draw port of this pixmap as dirty.
       ///< Rect is combined with the existing dirtyDrawPort rectangle.
       ///< The coordinates of Rect are relative to the pixmap's draw port.
       ///< If Rect extends into the currently visible view port of this pixmap,
       ///< MarkViewPortDirty() is called with the appropriate value.
  void MarkDrawPortDirty(const cPoint &Point);
       ///< Marks the given point of the draw port of this pixmap as dirty.
       ///< Point is combined with the existing dirtyDrawPort rectangle.
       ///< The coordinates of Point are relative to the pixmap's draw port.
       ///< If Point is within the currently visible view port of this pixmap,
       ///< MarkViewPortDirty() is called with the appropriate value.
  void SetClean(void);
       ///< Resets the "dirty" rectangles of this pixmap.
  virtual void DrawPixmap(const cPixmap *Pixmap, const cRect &Dirty);
       ///< Draws the Dirty part of the given Pixmap into this pixmap. If the
       ///< Pixmap's layer is 0, it is copied, otherwise it is rendered into this
       ///< pixmap. This function is used only to implement the tile handling
       ///< in the final rendering to the OSD.
public:
  cPixmap(void);
  cPixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort = cRect::Null);
       ///< Creates a pixmap in the given Layer. When rendering the final OSD, pixmaps
       ///< are handled in ascending order of their individual layer. This is
       ///< important if pixmaps overlap each other. The one with the highest layer is
       ///< rendered last. The actual value of Layer doesn't matter, it is only used
       ///< for defining the rendering sequence. If Layer is less than zero, this
       ///< pixmap will not be rendered into the final OSD (it can be activated by a
       ///< later call to SetLayer()). The value 0 is reserved for the background
       ///< pixmap and shall not be used otherwise (with the sole exception of
       ///< temporarily using layer 0 to have a text with transparent background
       ///< rendered with alpha blending into that pixmap; see also DrawPixel()).
       ///< If there are several pixmaps with the same value of Layer, their rendering
       ///< sequence within that layer is undefined.
       ///< In order to allow devices that can handle only a limited number of layers,
       ///< the Layer parameter must be less than 8 (MAXPIXMAPLAYERS).
       ///< ViewPort defines the rectangle in which this pixmap will be rendered on
       ///< the OSD. The coordinate (0, 0) corresponds to the upper left corner of the
       ///< OSD. If no DrawPort is given, it defaults to the same size as the
       ///< ViewPort, with its upper left corner set to (0, 0). The DrawPort's origin
       ///< is relative to the ViewPort's origin.
       ///< All drawing operations will be executed relative to the origin of the
       ///< DrawPort rectangle, and will be clipped to the size of this rectangle.
       ///< The DrawPort may have a different size than the ViewPort. If it is smaller
       ///< than the ViewPort, the rest of the ViewPort is treated as fully transparent
       ///< (unless this is a tiled pixmap, in which case the DrawPort is repeated
       ///< horizontally and vertically to fill the entire ViewPort). If the DrawPort
       ///< is larger than the ViewPort, only that portion of the DrawPort that
       ///< intersects with the ViewPort will be visible on the OSD.
       ///< The drawing area of a newly created cPixmap is not initialized and may
       ///< contain random data.
       ///< See cOsd::MaxPixmapSize() for information on the maximum size of pixmaps
       ///< supported by the system.
  static void Lock(void) { mutex.Lock(); }
       ///< All public member functions of cPixmap set locks as necessary to make sure
       ///< they are thread-safe (unless noted otherwise). If several cPixmap member
       ///< functions need to be called in a row, the caller must surround these calls
       ///< with proper Lock()/Unlock() calls. See the LOCK_PIXMAPS macro for a
       ///< convenient way of doing this.
  static void Unlock(void) { mutex.Unlock(); }
  int Layer(void) const { return layer; }
  int Alpha(void) const { return alpha; }
  bool Tile(void) const { return tile; }
  const cRect &ViewPort(void) const { return viewPort; }
       ///< Returns the pixmap's view port, which is relative to the OSD's origin.
       ///< Since this function returns a reference to a data member, the caller must
       ///< use Lock()/Unlock() to make sure the data doesn't change while it is used.
  const cRect &DrawPort(void) const { return drawPort; }
       ///< Returns the pixmap's draw port, which is relative to the view port.
       ///< Since this function returns a reference to a data member, the caller must
       ///< use Lock()/Unlock() to make sure the data doesn't change while it is used.
  const cRect &DirtyViewPort(void) const { return dirtyViewPort; }
       ///< Returns the "dirty" rectangle this pixmap causes on the OSD. This is the
       ///< surrounding rectangle around all pixels that have been modified since the
       ///< last time this pixmap has been rendered to the OSD. The rectangle is
       ///< relative to the OSD's origin.
       ///< Since this function returns a reference to a data member, the caller must
       ///< use Lock()/Unlock() to make sure the data doesn't change while it is used.
  const cRect &DirtyDrawPort(void) const { return dirtyDrawPort; }
       ///< Returns the "dirty" rectangle in the draw port of this this pixmap. This is
       ///< the surrounding rectangle around all pixels that have been modified since the
       ///< last time this pixmap has been rendered to the OSD. The rectangle is
       ///< relative to the draw port's origin.
       ///< Since this function returns a reference to a data member, the caller must
       ///< use Lock()/Unlock() to make sure the data doesn't change while it is used.
  virtual void SetLayer(int Layer);
       ///< Sets the layer of this pixmap to the given value.
       ///< If the new layer is greater than zero, the pixmap will be visible.
       ///< If it is less than zero, it will be invisible.
       ///< A value of 0 will be silently ignored.
       ///< If a derived class reimplements this function, it needs to call the base
       ///< class function.
  virtual void SetAlpha(int Alpha);
       ///< Sets the alpha value of this pixmap to the given value.
       ///< Alpha is limited to the range 0 (fully transparent) to 255 (fully opaque).
       ///< If a derived class reimplements this function, it needs to call the base
       ///< class function.
  virtual void SetTile(bool Tile);
       ///< Sets the tile property of this pixmap to the given value. If Tile is true,
       ///< the pixmaps data will be repeated horizontally and vertically if necessary
       ///< to fill the entire view port.
       ///< If a derived class reimplements this function, it needs to call the base
       ///< class function.
  virtual void SetViewPort(const cRect &Rect);
       ///< Sets the pixmap's view port to the given Rect.
       ///< If a derived class reimplements this function, it needs to call the base
       ///< class function.
  virtual void SetDrawPortPoint(const cPoint &Point, bool Dirty = true);
       ///< Sets the pixmap's draw port to the given Point.
       ///< Only the origin point of the draw port can be modified, its size is fixed.
       ///< By default, setting a new draw port point results in marking the relevant
       ///< part of the view port as "dirty". If Dirty is set to false, the view port
       ///< will not be marked as dirty. This is mainly used to implement the Pan()
       ///< function.
       ///< If a derived class reimplements this function, it needs to call the base
       ///< class function.
  virtual void Clear(void) = 0;
       ///< Clears the pixmap's draw port by setting all pixels to be fully transparent.
       ///< A derived class must call Lock()/Unlock().
  virtual void Fill(tColor Color) = 0;
       ///< Fills the pixmap's draw port with the given Color.
       ///< A derived class must call Lock()/Unlock().
  virtual void DrawImage(const cPoint &Point, const cImage &Image) = 0;
       ///< Draws the given Image into this pixmap at the given Point.
  virtual void DrawImage(const cPoint &Point, int ImageHandle) = 0;
       ///< Draws the image referenced by the given ImageHandle into this pixmap at
       ///< the given Point. ImageHandle must be a value that has previously been
       ///< returned by a call to cOsdProvider::StoreImage(). If ImageHandle
       ///< has an invalid value, nothing happens.
  virtual void DrawScaledImage(const cPoint &Point, const cImage &Image, double FactorX, double FactorY, bool AntiAlias = false) {};
       ///< Draws the given Image into this pixmap at the given Point and scales it.
       ///< If AntiAlias is true and either of the factors is greater than
       ///< 1.0, anti-aliasing is applied.
       ///< This function should be 'pure', just like all the others. However, since it was
       ///< introduced when this class was already in widespread use, the default was made
       ///< empty, so that existing code will still compile.
  virtual void DrawScaledImage(const cPoint &Point, int ImageHandle, double FactorX, double FactorY, bool AntiAlias = false) {};
       ///< Draws the image referenced by the given ImageHandle into this pixmap at
       ///< the given Point and scales it. ImageHandle must be a value that has
       ///< previously been returned by a call to cOsdProvider::StoreImage().
       ///< If ImageHandle has an invalid value, nothing happens.
       ///< If AntiAlias is true and either of the factors is greater than
       ///< 1.0, anti-aliasing is applied.
       ///< This function should be 'pure', just like all the others. However, since it was
       ///< introduced when this class was already in widespread use, the default was made
       ///< empty, so that existing code will still compile.
  virtual void DrawPixel(const cPoint &Point, tColor Color) = 0;
       ///< Sets the pixel at the given Point to the given Color, which is
       ///< a full 32 bit ARGB value. If the alpha value of Color is not 0xFF
       ///< (fully opaque), and this is the background pixmap (layer 0), the pixel is
       ///< alpha blended with the existing color at the given position in this pixmap.
  virtual void DrawBlendedPixel(const cPoint &Point, tColor Color, uint8_t AlphaLayer = ALPHA_OPAQUE) { DrawPixel(Point, Color); }
       ///< Like DrawPixel(), but with an additional AlphaLayer, and works on any
       ///< pixmap, not only the background. The default implementation just calls
       ///< DrawPixel(), ignoring AlphaLayer.
  virtual void DrawBitmap(const cPoint &Point, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool Overlay = false) = 0;
       ///< Sets the pixels in the OSD with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at Point.
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color).
       ///< If Overlay is true, any pixel in Bitmap that has color index 0 will
       ///< not overwrite the corresponding pixel in the target area.
       ///< This function is mainly for compatibility with skins or plugins that
       ///< draw bitmaps onto the OSD.
  virtual void DrawText(const cPoint &Point, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault) = 0;
       ///< Draws the given string at Point with the given foreground
       ///< and background color and font. If Width and Height are given, the text
       ///< will be drawn into a rectangle with the given size and the given
       ///< Alignment (default is top-left). If ColorBg is clrTransparent, no
       ///< background pixels will be drawn, which allows drawing "transparent" text.
  virtual void DrawRectangle(const cRect &Rect, tColor Color) = 0;
       ///< Draws a filled rectangle with the given Color.
  virtual void DrawEllipse(const cRect &Rect, tColor Color, int Quadrants = 0) = 0;
       ///< Draws a filled ellipse with the given Color that fits into the given
       ///< rectangle. Quadrants controls which parts of the ellipse are actually drawn:
       ///< 0       draws the entire ellipse
       ///< 1..4    draws only the first, second, third or fourth quadrant, respectively
       ///< 5..8    draws the right, top, left or bottom half, respectively
       ///< -1..-4  draws the inverted part of the given quadrant
       ///< If Quadrants is not 0, the coordinates are those of the actual area, not
       ///< the full circle!
  virtual void DrawSlope(const cRect &Rect, tColor Color, int Type) = 0;
       ///< Draws a "slope" with the given Color into the given rectangle.
       ///< Type controls the direction of the slope and which side of it will be drawn:
       ///< 0: horizontal, rising,  lower
       ///< 1: horizontal, rising,  upper
       ///< 2: horizontal, falling, lower
       ///< 3: horizontal, falling, upper
       ///< 4: vertical,   rising,  lower
       ///< 5: vertical,   rising,  upper
       ///< 6: vertical,   falling, lower
       ///< 7: vertical,   falling, upper
  virtual void Render(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest) = 0;
       ///< Renders the part of the given Pixmap covered by Source into this pixmap at
       ///< location Dest. The Source rectangle is relative to the given Pixmap's draw port.
       ///< The Pixmap's alpha value is to be used when rendering.
  virtual void Copy(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest) = 0;
       ///< Copies the part of the given Pixmap covered by Source into this pixmap at
       ///< location Dest. The Source rectangle is relative to the given Pixmap's draw port.
       ///< The data from Pixmap is copied as is, no alpha handling of any kind takes
       ///< place.
  virtual void Scroll(const cPoint &Dest, const cRect &Source = cRect::Null) = 0;
       ///< Scrolls the data in the pixmap's draw port to the given Dest point.
       ///< If Source is given, only the data within that rectangle is scrolled.
       ///< Source and Dest are relative to this pixmap's draw port.
  virtual void Pan(const cPoint &Dest, const cRect &Source = cRect::Null) = 0;
       ///< Does the same as Scroll(), but also shifts the draw port accordingly,
       ///< so that the view port doesn't get dirty if the scrolled rectangle
       ///< covers the entire view port. This may be of advantage if, e.g.,
       ///< there is a draw port that holds, say, 11 lines of text, while the
       ///< view port displays only 10 lines. By Pan()'ing the draw port up one
       ///< line, a new bottom line can be written into the draw port (without
       ///< being seen through the view port), and later the draw port can be
       ///< shifted smoothly, resulting in a smooth scrolling.
       ///< It is the caller's responsibility to make sure that Source and Dest
       ///< are given in such a way that the view port will not get dirty. No
       ///< check is done whether this condition actually holds true.
  };

class cPixmapMutexLock : public cMutexLock {
public:
  cPixmapMutexLock(void): cMutexLock(&cPixmap::mutex) {}
  };

#define LOCK_PIXMAPS cPixmapMutexLock PixmapMutexLock

// cPixmapMemory is an implementation of cPixmap that uses an array of tColor
// values to store the pixmap.

class cPixmapMemory : public cPixmap {
private:
  tColor *data;
  bool panning;
public:
  cPixmapMemory(void);
  cPixmapMemory(int Layer, const cRect &ViewPort, const cRect &DrawPort = cRect::Null);
  virtual ~cPixmapMemory() override;
  const uint8_t *Data(void) { return (uint8_t *)data; }
  virtual void Clear(void) override;
  virtual void Fill(tColor Color) override;
  virtual void DrawImage(const cPoint &Point, const cImage &Image) override;
  virtual void DrawImage(const cPoint &Point, int ImageHandle) override;
  virtual void DrawScaledImage(const cPoint &Point, const cImage &Image, double FactorX, double FactorY, bool AntiAlias = false) override;
  virtual void DrawScaledImage(const cPoint &Point, int ImageHandle, double FactorX, double FactorY, bool AntiAlias = false) override;
  virtual void DrawPixel(const cPoint &Point, tColor Color) override;
  virtual void DrawBlendedPixel(const cPoint &Point, tColor Color, uint8_t AlphaLayer = ALPHA_OPAQUE) override;
  virtual void DrawBitmap(const cPoint &Point, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool Overlay = false) override;
  virtual void DrawText(const cPoint &Point, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault) override;
  virtual void DrawRectangle(const cRect &Rect, tColor Color) override;
  virtual void DrawEllipse(const cRect &Rect, tColor Color, int Quadrants = 0) override;
  virtual void DrawSlope(const cRect &Rect, tColor Color, int Type) override;
  virtual void Render(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest) override;
  virtual void Copy(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest) override;
  virtual void Scroll(const cPoint &Dest, const cRect &Source = cRect::Null) override;
  virtual void Pan(const cPoint &Dest, const cRect &Source = cRect::Null) override;
  };

#define MAXOSDAREAS 16

/// The cOsd class is the interface to the "On Screen Display".
/// An actual output device needs to derive from this class and implement
/// the functionality necessary to display the OSD on the TV screen.
/// If the actual OSD supports "True Color", it can either let VDR do
/// all the rendering by calling RenderPixmaps() ("raw mode"), or it can
/// reimplement all necessary cPixmap functions and do the rendering
/// itself ("high level mode").
/// If an OSD provides a "high level mode", it shall also provide a "raw mode"
/// in order to verify proper operation. The plugin that implements the OSD
/// shall offer a configuration switch in its setup.

class cOsd {
  friend class cOsdProvider;
private:
  static int osdLeft, osdTop, osdWidth, osdHeight;
  static cVector<cOsd *> Osds;
  static cSize maxPixmapSize;
  static cMutex mutex;
  bool isTrueColor;
  cBitmap *savedBitmap;
  cBitmap *bitmaps[MAXOSDAREAS];
  int numBitmaps;
  cPixmapMemory *savedPixmap;
  cVector<cPixmap *> pixmaps;
  int left, top, width, height;
  uint level;
  bool active;
protected:
  cOsd(int Left, int Top, uint Level);
       ///< Initializes the OSD with the given coordinates.
       ///< By default it is assumed that the full area will be able to display
       ///< full 32 bit graphics (ARGB with eight bit for each color and the alpha
       ///< value, respectively). However, the actual hardware in use may not be
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
       ///< There can be any number of cOsd objects at the same time, but only
       ///< one of them will be active at any given time. The active OSD is the
       ///< one with the lowest value of Level. If there are several cOsd objects
       ///< with the same Level, the one that was created first will be active.
  bool Active(void) { return active; }
  virtual void SetActive(bool On) { active = On; }
       ///< Sets this OSD to be the active one.
       ///< A derived class must call cOsd::SetActive(On).
  cPixmap *AddPixmap(cPixmap *Pixmap);
       ///< Adds the given Pixmap to the list of currently active pixmaps in this OSD.
       ///< Returns Pixmap if the operation was successful, or NULL if for some reason
       ///< the pixmap could not be added to the list.
       ///< A derived class that implements its own cPixmap class must call AddPixmap()
       ///< in order to add a newly created pixmap to the OSD's list of pixmaps.
  cPixmap *RenderPixmaps(void);
       ///< Renders the dirty part of all pixmaps into a resulting pixmap that
       ///< shall be displayed on the OSD. The returned pixmap's view port is
       ///< set to the location of the rectangle on the OSD that needs to be
       ///< refreshed; its draw port's origin is at (0, 0), and it has the same
       ///< size as the view port.
       ///< Only pixmaps with a non-negative layer value are rendered.
       ///< If there are several non-overlapping dirty rectangles from different pixmaps,
       ///< they are returned separately in order to avoid re-rendering large parts
       ///< of the OSD that haven't changed at all. The caller must therefore call
       ///< RenderPixmaps() repeatedly until it returns NULL, and display the returned
       ///< parts of the OSD at their appropriate locations. During this entire
       ///< operation the caller must hold a lock on the cPixmap mutex (for instance
       ///< by putting a LOCK_PIXMAPS into the scope of the operation).
       ///< If there are no dirty pixmaps, or if this is not a true color OSD,
       ///< this function returns NULL.
       ///< The caller must call DestroyPixmap() for the returned pixmap after use.
  cBitmap *GetBitmap(int Area);
       ///< Returns a pointer to the bitmap for the given Area, or NULL if no
       ///< such bitmap exists.
       ///< If this is a true color OSD, a pointer to a dummy bitmap with 8bpp
       ///< is returned. This is done so that skins that call this function
       ///< in order to preset the bitmap's palette won't crash.
       ///< Use of this function outside of derived classes is deprecated and it
       ///< may be made 'protected' in a future version.
public:
  virtual ~cOsd();
       ///< Shuts down the OSD.
  static int OsdLeft(void) { return osdLeft ? osdLeft : Setup.OSDLeft; }
  static int OsdTop(void) { return osdTop ? osdTop : Setup.OSDTop; }
  static int OsdWidth(void) { return osdWidth ? osdWidth : Setup.OSDWidth; }
  static int OsdHeight(void) { return osdHeight ? osdHeight : Setup.OSDHeight; }
  static void SetOsdPosition(int Left, int Top, int Width, int Height);
       ///< Sets the position and size of the OSD to the given values.
       ///< This may be useful for plugins that determine the scaling of the
       ///< video image and need to scale the OSD accordingly to fit on the
       ///< screen.
  static int IsOpen(void) { return Osds.Size() && Osds[0]->level == OSD_LEVEL_DEFAULT; }
       ///< Returns true if there is currently a level 0 OSD open.
  bool IsTrueColor(void) const { return isTrueColor; }
       ///< Returns 'true' if this is a true color OSD (providing full 32 bit color
       ///< depth).
  int Left(void) { return left; }
  int Top(void) { return top; }
  int Width(void) { return width; }
  int Height(void) { return height; }
  void SetAntiAliasGranularity(uint FixedColors, uint BlendColors);
       ///< Allows the system to optimize utilization of the limited color
       ///< palette entries when generating blended colors for anti-aliasing.
       ///< FixedColors is the maximum number of colors used, and BlendColors
       ///< is the maximum number of foreground/background color combinations
       ///< used with anti-aliasing. If this function is not called with
       ///< useful values, the palette may be filled up with many shades of
       ///< a single color combination, and may not be able to serve all
       ///< requested colors. By default the palette assumes there will be
       ///< 10 fixed colors and 10 color combinations.
       ///< If this is a true color OSD, this function does nothing.
  virtual const cSize &MaxPixmapSize(void) const;
       ///< Returns the maximum possible size of a pixmap this OSD can create.
       ///< Derived classes can reimplement this function if their implementation
       ///< of cPixmap can only provide pixmaps up to a certain size.
       ///< The default implementation returns a cSize object of maximal size
       ///< (INT_MAX). However, memory restrictions may still apply.
  virtual cPixmap *CreatePixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort = cRect::Null);
       ///< Creates a new true color pixmap on this OSD (see cPixmap for details).
       ///< The caller must not delete the returned object, it will be deleted when
       ///< the OSD is deleted. DestroyPixmap() can be called if a pixmap shall be
       ///< destroyed before the OSD is deleted.
       ///< If this is not a true color OSD, or if the pixmap could not be created
       ///< due to limited resources, this function returns NULL.
  virtual void DestroyPixmap(cPixmap *Pixmap);
       ///< Destroys the given Pixmap, which has previously been created by a call to
       ///< CreatePixmap(). When the OSD is deleted, all pixmaps are destroyed
       ///< automatically. So this function only needs to be used if a pixmap shall
       ///< be destroyed while the OSD is still being used.
  virtual void DrawImage(const cPoint &Point, const cImage &Image);
       ///< Draws the given Image on this OSD at the given Point.
       ///< If this is not a true color OSD, this function does nothing.
  virtual void DrawImage(const cPoint &Point, int ImageHandle);
       ///< Draws the image referenced by the given ImageHandle on this OSD at
       ///< the given Point. ImageHandle must be a value that has previously been
       ///< returned by a call to cOsdProvider::StoreImage(). If ImageHandle
       ///< has an invalid value, nothing happens.
       ///< If this is not a true color OSD, this function does nothing.
  virtual void DrawScaledImage(const cPoint &Point, const cImage &Image, double FactorX, double FactorY, bool AntiAlias = false);
       ///< Draws the given Image on this OSD at the given Point and scales it.
       ///< If AntiAlias is true and either of the factors is greater than
       ///< 1.0, anti-aliasing is applied.
       ///< If this is not a true color OSD, this function does nothing.
  virtual void DrawScaledImage(const cPoint &Point, int ImageHandle, double FactorX, double FactorY, bool AntiAlias = false);
       ///< Draws the image referenced by the given ImageHandle on this OSD at
       ///< the given Point and scales it. ImageHandle must be a value that has
       ///< previously been returned by a call to cOsdProvider::StoreImage().
       ///< If ImageHandle has an invalid value, nothing happens.
       ///< If AntiAlias is true and either of the factors is greater than
       ///< 1.0, anti-aliasing is applied.
       ///< If this is not a true color OSD, this function does nothing.
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
       ///< A new call overwrites any previous settings
       ///< To set up a true color OSD, exactly one area must be requested, with
       ///< its coordinates set to the full area the OSD shall cover, and the
       ///< bpp value set to 32.
  virtual void SaveRegion(int x1, int y1, int x2, int y2);
       ///< Saves the region defined by the given coordinates for later restoration
       ///< through RestoreRegion(). Only one saved region can be active at any
       ///< given time.
  virtual void RestoreRegion(void);
       ///< Restores the region previously saved by a call to SaveRegion().
       ///< If SaveRegion() has not been called before, nothing will happen.
  virtual eOsdError SetPalette(const cPalette &Palette, int Area);
       ///< Sets the Palette for the given Area (the first area is numbered 0).
       ///< If this is a true color OSD, nothing happens and oeOk is returned.
  virtual void DrawPixel(int x, int y, tColor Color);
       ///< Sets the pixel at the given coordinates to the given Color, which is
       ///< a full 32 bit ARGB value.
       ///< If the OSD area has been divided into separate sub-areas, and the
       ///< given coordinates don't fall into any of these sub-areas, no pixel will
       ///< be set.
  virtual void DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg = 0, tColor ColorBg = 0, bool ReplacePalette = false, bool Overlay = false);
       ///< Sets the pixels in the OSD with the data from the given
       ///< Bitmap, putting the upper left corner of the Bitmap at (x, y).
       ///< If ColorFg or ColorBg is given, the first palette entry of the Bitmap
       ///< will be mapped to ColorBg and the second palette entry will be mapped to
       ///< ColorFg (palette indexes are defined so that 0 is the background and
       ///< 1 is the foreground color). ReplacePalette controls whether the target
       ///< area shall have its palette replaced with the one from Bitmap.
       ///< If Overlay is true, any pixel in Bitmap that has color index 0 will
       ///< not overwrite the corresponding pixel in the target area.
       ///< If this is a true color OSD, ReplacePalette has no meaning.
  virtual void DrawScaledBitmap(int x, int y, const cBitmap &Bitmap, double FactorX, double FactorY, bool AntiAlias = false);
       ///< Sets the pixels in the OSD with the data from the given Bitmap, putting
       ///< the upper left corner of the Bitmap at (x, y) and scaled by the given
       ///< factors. If AntiAlias is true and either of the factors is greater than
       ///< 1.0, anti-aliasing is applied.
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
       ///< -1..-4  draws the inverted part of the given quadrant
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
       ///< Flush() should return as soon as possible.
       ///< For a true color OSD using the default implementation with in memory
       ///< pixmaps, the Flush() function should basically do something like this:
       ///<
       ///<  LOCK_PIXMAPS;
       ///<  while (cPixmapMemory *pm = dynamic_cast<cPixmapMemory *>(RenderPixmaps())) {
       ///<        int w = pm->ViewPort().Width();
       ///<        int h = pm->ViewPort().Height();
       ///<        int d = w * sizeof(tColor);
       ///<        MyOsdDrawPixmap(Left() + pm->ViewPort().X(), Top() + pm->ViewPort().Y(), pm->Data(), w, h, h * d);
       ///<        DestroyPixmap(pm);
       ///<        }
       ///<
       ///< If a plugin uses a derived cPixmap implementation, it needs to use that
       ///< type instead of cPixmapMemory.
  };

#define MAXOSDIMAGES 64

class cOsdProvider {
  friend class cPixmapMemory;
private:
  static cOsdProvider *osdProvider;
  static int oldWidth;
  static int oldHeight;
  static double oldAspect;
  static cImage *images[MAXOSDIMAGES];
  static int osdState;
protected:
  virtual cOsd *CreateOsd(int Left, int Top, uint Level) = 0;
      ///< Returns a pointer to a newly created cOsd object, which will be located
      ///< at the given coordinates.
  virtual bool ProvidesTrueColor(void) { return false; }
      ///< Returns true if this OSD provider is able to handle a true color OSD.
  virtual int StoreImageData(const cImage &Image);
      ///< Copies the given Image and returns a handle for later reference.
      ///< A derived class can implement its own image storing mechanism by
      ///< reimplementing this function as well as DropImageData().
      ///< The base class implementation simply copies the image data to allow
      ///< plugins to always use this interface, no matter if the actual device
      ///< provides support for storing image data or not. The handles returned
      ///< by the default implementation are positive integers. A derived class
      ///< might want to use negative integers as handles, so that it can fall
      ///< back to using the base class image storing mechanism if, e.g.,  it runs
      ///< out of memory.
  virtual void DropImageData(int ImageHandle);
      ///< Drops the image data referenced by ImageHandle.
  static const cImage *GetImageData(int ImageHandle);
      ///< Gets the image data referenced by ImageHandle.
public:
  cOsdProvider(void);
      //XXX maybe parameter to make this one "sticky"??? (frame-buffer etc.)
  virtual ~cOsdProvider();
  static cOsd *NewOsd(int Left, int Top, uint Level = OSD_LEVEL_DEFAULT);
      ///< Returns a pointer to a newly created cOsd object, which will be located
      ///< at the given coordinates. When the cOsd object is no longer needed, the
      ///< caller must delete it. If the OSD is already in use, or there is no OSD
      ///< provider, a dummy OSD is returned so that the caller may always use the
      ///< returned pointer without having to check it every time it is accessed.
  static void UpdateOsdSize(bool Force = false);
      ///< Inquires the actual size of the video display and adjusts the OSD and
      ///< font sizes accordingly. If Force is true, all settings are recalculated,
      ///< even if the video resolution hasn't changed since the last call to
      ///< this function.
  static bool OsdSizeChanged(int &State);
      ///< Checks if the OSD size has changed and a currently displayed OSD needs to
      ///< be redrawn. An internal reference value is incremented on every size change
      ///< and is compared against State when calling the method.
      ///< OsdSizeChanged() can be called with an uninitialized State to just get
      ///< the current value of State.
  static bool SupportsTrueColor(void);
      ///< Returns true if the current OSD provider is able to handle a true color OSD.
  static int StoreImage(const cImage &Image);
      ///< Stores the given Image for later use with DrawImage() on an OSD or
      ///< pixmap. The returned number is a handle that must be used when
      ///< referencing this image in a call to DrawImage() or DropImage().
      ///< The image data is copied, so any later changes to Image will have
      ///< no effect on the stored image.
      ///< A derived class may be able to copy frequently used images to some
      ///< space where they can be retrieved faster than using a cImage in each call.
      ///< If this is not a true color OSD, or if the image data can't be stored for
      ///< any reason, this function returns 0 and nothing is stored.
  static void DropImage(int ImageHandle);
      ///< Drops the image referenced by the given ImageHandle. If ImageHandle
      ///< has an invalid value, nothing happens.
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
