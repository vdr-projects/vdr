/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 1.62 2005/06/19 10:43:04 kls Exp $
 */

#include "osd.h"
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "tools.h"

// --- cPalette --------------------------------------------------------------

cPalette::cPalette(int Bpp)
{
  SetBpp(Bpp);
}

void cPalette::Reset(void)
{
  numColors = 0;
  modified = false;
}

int cPalette::Index(tColor Color)
{
  for (int i = 0; i < numColors; i++) {
      if (color[i] == Color)
         return i;
      }
  if (numColors < maxColors) {
     color[numColors++] = Color;
     modified = true;
     return numColors - 1;
     }
  dsyslog("too many different colors used in palette");
  //TODO: return the index of the "closest" color?
  return 0;
}

void cPalette::SetBpp(int Bpp)
{
  bpp = Bpp;
  maxColors = 1 << bpp;
  Reset();
}

void cPalette::SetColor(int Index, tColor Color)
{
  if (Index < maxColors) {
     if (numColors <= Index) {
        numColors = Index + 1;
        modified = true;
        }
     else
        modified |= color[Index] != Color;
     color[Index] = Color;
     }
}

const tColor *cPalette::Colors(int &NumColors)
{
  NumColors = numColors;
  return numColors ? color : NULL;
}

void cPalette::Take(const cPalette &Palette, tIndexes *Indexes, tColor ColorFg, tColor ColorBg)
{
  for (int i = 0; i < Palette.numColors; i++) {
      tColor Color = Palette.color[i];
      if (ColorFg || ColorBg) {
         switch (i) {
           case 0: Color = ColorBg; break;
           case 1: Color = ColorFg; break;
           }
         }
      int n = Index(Color);
      if (Indexes)
         (*Indexes)[i] = n;
      }
}

void cPalette::Replace(const cPalette &Palette)
{
  for (int i = 0; i < Palette.numColors; i++)
      SetColor(i, Palette.color[i]);
  numColors = Palette.numColors;
}

// --- cBitmap ---------------------------------------------------------------

cBitmap::cBitmap(int Width, int Height, int Bpp, int X0, int Y0)
:cPalette(Bpp)
{
  bitmap = NULL;
  x0 = X0;
  y0 = Y0;
  SetSize(Width, Height);
}

cBitmap::cBitmap(const char *FileName)
{
  bitmap = NULL;
  x0 = 0;
  y0 = 0;
  LoadXpm(FileName);
}

cBitmap::cBitmap(char *Xpm[])
{
  bitmap = NULL;
  x0 = 0;
  y0 = 0;
  SetXpm(Xpm);
}

cBitmap::~cBitmap()
{
  free(bitmap);
}

void cBitmap::SetSize(int Width, int Height)
{
  if (bitmap && Width == width && Height == height)
     return;
  width = Width;
  height = Height;
  free(bitmap);
  bitmap = NULL;
  dirtyX1 = 0;
  dirtyY1 = 0;
  dirtyX2 = width - 1;
  dirtyY2 = height - 1;
  if (width > 0 && height > 0) {
     bitmap = MALLOC(tIndex, width * height);
     if (bitmap)
        memset(bitmap, 0x00, width * height);
     else
        esyslog("ERROR: can't allocate bitmap!");
     }
  else
     esyslog("ERROR: illegal bitmap parameters (%d, %d)!", width, height);
}

bool cBitmap::Contains(int x, int y) const
{
  x -= x0;
  y -= y0;
  return 0 <= x && x < width && 0 <= y && y < height;
}

bool cBitmap::Covers(int x1, int y1, int x2, int y2) const
{
  x1 -= x0;
  y1 -= y0;
  x2 -= x0;
  y2 -= y0;
  return x1 <= 0 && y1 <= 0 && x2 >= width - 1 && y2 >= height - 1;
}

bool cBitmap::Intersects(int x1, int y1, int x2, int y2) const
{
  x1 -= x0;
  y1 -= y0;
  x2 -= x0;
  y2 -= y0;
  return !(x2 < 0 || x1 >= width || y2 < 0 || y1 >= height);
}

bool cBitmap::Dirty(int &x1, int &y1, int &x2, int &y2)
{
  if (dirtyX2 >= 0) {
     x1 = dirtyX1;
     y1 = dirtyY1;
     x2 = dirtyX2;
     y2 = dirtyY2;
     return true;
     }
  return false;
}

void cBitmap::Clean(void)
{
  dirtyX1 = width;
  dirtyY1 = height;
  dirtyX2 = -1;
  dirtyY2 = -1;
}

bool cBitmap::LoadXpm(const char *FileName)
{
  bool Result = false;
  FILE *f = fopen(FileName, "r");
  if (f) {
     char **Xpm = NULL;
     bool isXpm = false;
     int lines = 0;
     int index = 0;
     char *s;
     cReadLine ReadLine;
     while ((s = ReadLine.Read(f)) != NULL) {
           s = skipspace(s);
           if (!isXpm) {
              if (strcmp(s, "/* XPM */") != 0) {
                 esyslog("ERROR: invalid header in XPM file '%s'", FileName);
                 break;
                 }
              isXpm = true;
              }
           else if (*s++ == '"') {
              if (!lines) {
                 int w, h, n, c;
                 if (4 != sscanf(s, "%d %d %d %d", &w, &h, &n, &c)) {
                    esyslog("ERROR: faulty 'values' line in XPM file '%s'", FileName);
                    break;
                    }
                 lines = h + n + 1;
                 Xpm = MALLOC(char *, lines);
                 }
              char *q = strchr(s, '"');
              if (!q) {
                 esyslog("ERROR: missing quotes in XPM file '%s'", FileName);
                 break;
                 }
              *q = 0;
              if (index < lines)
                 Xpm[index++] = strdup(s);
              else {
                 esyslog("ERROR: too many lines in XPM file '%s'", FileName);
                 break;
                 }
              }
           }
     if (index == lines)
        Result = SetXpm(Xpm);
     else
        esyslog("ERROR: too few lines in XPM file '%s'", FileName);
     for (int i = 0; i < index; i++)
         free(Xpm[i]);
     free(Xpm);
     fclose(f);
     }
  else
     esyslog("ERROR: can't open XPM file '%s'", FileName);
  return Result;
}

bool cBitmap::SetXpm(char *Xpm[], bool IgnoreNone)
{
  char **p = Xpm;
  int w, h, n, c;
  if (4 != sscanf(*p, "%d %d %d %d", &w, &h, &n, &c)) {
     esyslog("ERROR: faulty 'values' line in XPM: '%s'", *p);
     return false;
     }
  if (n > MAXNUMCOLORS) {
     esyslog("ERROR: too many colors in XPM: %d", n);
     return false;
     }
  int b = 0;
  while (1 << (1 << b) < (IgnoreNone ? n - 1 : n))
        b++;
  SetBpp(1 << b);
  SetSize(w, h);
  int NoneColorIndex = MAXNUMCOLORS;
  for (int i = 0; i < n; i++) {
      const char *s = *++p;
      if (int(strlen(s)) < c) {
         esyslog("ERROR: faulty 'colors' line in XPM: '%s'", s);
         return false;
         }
      s = skipspace(s + c);
      if (*s != 'c') {
         esyslog("ERROR: unknown color key in XPM: '%c'", *s);
         return false;
         }
      s = skipspace(s + 1);
      if (strcasecmp(s, "none") == 0) {
         s = "#00000000";
         NoneColorIndex = i;
         if (IgnoreNone)
            continue;
         }
      if (*s != '#') {
         esyslog("ERROR: unknown color code in XPM: '%c'", *s);
         return false;
         }
      tColor color = strtoul(++s, NULL, 16) | 0xFF000000;
      SetColor((IgnoreNone && i > NoneColorIndex) ? i - 1 : i, color);
      }
  for (int y = 0; y < h; y++) {
      const char *s = *++p;
      if (int(strlen(s)) != w * c) {
         esyslog("ERROR: faulty pixel line in XPM: %d '%s'", y, s);
         return false;
         }
      for (int x = 0; x < w; x++) {
          for (int i = 0; i <= n; i++) {
              if (i == n) {
                 esyslog("ERROR: undefined pixel color in XPM: %d %d '%s'", x, y, s);
                 return false;
                 }
              if (strncmp(Xpm[i + 1], s, c) == 0) {
                 if (i == NoneColorIndex)
                    NoneColorIndex = MAXNUMCOLORS;
                 SetIndex(x, y, (IgnoreNone && i > NoneColorIndex) ? i - 1 : i);
                 break;
                 }
              }
          s += c;
          }
      }
  if (NoneColorIndex < MAXNUMCOLORS && !IgnoreNone)
     return SetXpm(Xpm, true);
  return true;
}

void cBitmap::SetIndex(int x, int y, tIndex Index)
{
  if (bitmap) {
     if (0 <= x && x < width && 0 <= y && y < height) {
        if (bitmap[width * y + x] != Index) {
           bitmap[width * y + x] = Index;
           if (dirtyX1 > x)  dirtyX1 = x;
           if (dirtyY1 > y)  dirtyY1 = y;
           if (dirtyX2 < x)  dirtyX2 = x;
           if (dirtyY2 < y)  dirtyY2 = y;
           }
        }
     }
}

void cBitmap::DrawPixel(int x, int y, tColor Color)
{
  x -= x0;
  y -= y0;
  if (0 <= x && x < width && 0 <= y && y < height)
     SetIndex(x, y, Index(Color));
}

void cBitmap::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette)
{
  if (bitmap && Bitmap.bitmap && Intersects(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1)) {
     if (Covers(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1))
        Reset();
     x -= x0;
     y -= y0;
     if (ReplacePalette && Covers(x + x0, y + y0, x + x0 + Bitmap.Width() - 1, y + y0 + Bitmap.Height() - 1)) {
        Replace(Bitmap);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++)
                SetIndex(x + ix, y + iy, Bitmap.bitmap[Bitmap.width * iy + ix]);
            }
        }
     else {
        tIndexes Indexes;
        Take(Bitmap, &Indexes, ColorFg, ColorBg);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++)
                SetIndex(x + ix, y + iy, Indexes[int(Bitmap.bitmap[Bitmap.width * iy + ix])]);
            }
        }
     }
}

void cBitmap::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
  if (bitmap) {
     int w = Font->Width(s);
     int h = Font->Height();
     int limit = 0;
     if (Width || Height) {
        int cw = Width ? Width : w;
        int ch = Height ? Height : h;
        if (!Intersects(x, y, x + cw - 1, y + ch - 1))
           return;
        if (ColorBg != clrTransparent)
           DrawRectangle(x, y, x + cw - 1, y + ch - 1, ColorBg);
        limit = x + cw - x0;
        if (Width) {
           if ((Alignment & taLeft) != 0)
              ;
           else if ((Alignment & taRight) != 0) {
              if (w < Width)
                 x += Width - w;
              }
           else { // taCentered
              if (w < Width)
                 x += (Width - w) / 2;
              }
           }
        if (Height) {
           if ((Alignment & taTop) != 0)
              ;
           else if ((Alignment & taBottom) != 0) {
              if (h < Height)
                 y += Height - h;
              }
           else { // taCentered
              if (h < Height)
                 y += (Height - h) / 2;
              }
           }
        }
     else if (!Intersects(x, y, x + w - 1, y + h - 1))
        return;
     x -= x0;
     y -= y0;
     tIndex fg = Index(ColorFg);
     tIndex bg = (ColorBg != clrTransparent) ? Index(ColorBg) : 0;
     while (s && *s) {
           const cFont::tCharData *CharData = Font->CharData(*s++);
           if (limit && int(x + CharData->width) > limit)
              break; // we don't draw partial characters
           if (int(x + CharData->width) > 0) {
              for (int row = 0; row < h; row++) {
                  cFont::tPixelData PixelData = CharData->lines[row];
                  for (int col = CharData->width; col-- > 0; ) {
                      if (ColorBg != clrTransparent || (PixelData & 1))
                         SetIndex(x + col, y + row, (PixelData & 1) ? fg : bg);
                      PixelData >>= 1;
                      }
                  }
              }
           x += CharData->width;
           if (x > width - 1)
              break;
           }
     }
}

void cBitmap::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
  if (bitmap && Intersects(x1, y1, x2, y2)) {
     if (Covers(x1, y1, x2, y2))
        Reset();
     x1 -= x0;
     y1 -= y0;
     x2 -= x0;
     y2 -= y0;
     x1 = max(x1, 0);
     y1 = max(y1, 0);
     x2 = min(x2, width - 1);
     y2 = min(y2, height - 1);
     tIndex c = Index(Color);
     for (int y = y1; y <= y2; y++)
         for (int x = x1; x <= x2; x++)
             SetIndex(x, y, c);
     }
}

void cBitmap::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
  if (!Intersects(x1, y1, x2, y2))
     return;
  // Algorithm based on http://homepage.smc.edu/kennedy_john/BELIPSE.PDF
  int rx = x2 - x1;
  int ry = y2 - y1;
  int cx = (x1 + x2) / 2;
  int cy = (y1 + y2) / 2;
  switch (abs(Quadrants)) {
    case 0: rx /= 2; ry /= 2; break;
    case 1: cx = x1; cy = y2; break;
    case 2: cx = x2; cy = y2; break;
    case 3: cx = x2; cy = y1; break;
    case 4: cx = x1; cy = y1; break;
    case 5: cx = x1;          ry /= 2; break;
    case 6:          cy = y2; rx /= 2; break;
    case 7: cx = x2;          ry /= 2; break;
    case 8:          cy = y1; rx /= 2; break;
    }
  int TwoASquare = 2 * rx * rx;
  int TwoBSquare = 2 * ry * ry;
  int x = rx;
  int y = 0;
  int XChange = ry * ry * (1 - 2 * rx);
  int YChange = rx * rx;
  int EllipseError = 0;
  int StoppingX = TwoBSquare * rx;
  int StoppingY = 0;
  while (StoppingX >= StoppingY) {
        switch (Quadrants) {
          case  5: DrawRectangle(cx,     cy + y, cx + x, cy + y, Color); // no break
          case  1: DrawRectangle(cx,     cy - y, cx + x, cy - y, Color); break;
          case  7: DrawRectangle(cx - x, cy + y, cx,     cy + y, Color); // no break
          case  2: DrawRectangle(cx - x, cy - y, cx,     cy - y, Color); break;
          case  3: DrawRectangle(cx - x, cy + y, cx,     cy + y, Color); break;
          case  4: DrawRectangle(cx,     cy + y, cx + x, cy + y, Color); break;
          case  0:
          case  6: DrawRectangle(cx - x, cy - y, cx + x, cy - y, Color); if (Quadrants == 6) break;
          case  8: DrawRectangle(cx - x, cy + y, cx + x, cy + y, Color); break;
          case -1: DrawRectangle(cx + x, cy - y, x2,     cy - y, Color); break;
          case -2: DrawRectangle(x1,     cy - y, cx - x, cy - y, Color); break;
          case -3: DrawRectangle(x1,     cy + y, cx - x, cy + y, Color); break;
          case -4: DrawRectangle(cx + x, cy + y, x2,     cy + y, Color); break;
          }
        y++;
        StoppingY += TwoASquare;
        EllipseError += YChange;
        YChange += TwoASquare;
        if (2 * EllipseError + XChange > 0) {
           x--;
           StoppingX -= TwoBSquare;
           EllipseError += XChange;
           XChange += TwoBSquare;
           }
        }
  x = 0;
  y = ry;
  XChange = ry * ry;
  YChange = rx * rx * (1 - 2 * ry);
  EllipseError = 0;
  StoppingX = 0;
  StoppingY = TwoASquare * ry;
  while (StoppingX <= StoppingY) {
        switch (Quadrants) {
          case  5: DrawRectangle(cx,     cy + y, cx + x, cy + y, Color); // no break
          case  1: DrawRectangle(cx,     cy - y, cx + x, cy - y, Color); break;
          case  7: DrawRectangle(cx - x, cy + y, cx,     cy + y, Color); // no break
          case  2: DrawRectangle(cx - x, cy - y, cx,     cy - y, Color); break;
          case  3: DrawRectangle(cx - x, cy + y, cx,     cy + y, Color); break;
          case  4: DrawRectangle(cx,     cy + y, cx + x, cy + y, Color); break;
          case  0:
          case  6: DrawRectangle(cx - x, cy - y, cx + x, cy - y, Color); if (Quadrants == 6) break;
          case  8: DrawRectangle(cx - x, cy + y, cx + x, cy + y, Color); break;
          case -1: DrawRectangle(cx + x, cy - y, x2,     cy - y, Color); break;
          case -2: DrawRectangle(x1,     cy - y, cx - x, cy - y, Color); break;
          case -3: DrawRectangle(x1,     cy + y, cx - x, cy + y, Color); break;
          case -4: DrawRectangle(cx + x, cy + y, x2,     cy + y, Color); break;
          }
        x++;
        StoppingX += TwoBSquare;
        EllipseError += XChange;
        XChange += TwoBSquare;
        if (2 * EllipseError + YChange > 0) {
           y--;
           StoppingY -= TwoASquare;
           EllipseError += YChange;
           YChange += TwoASquare;
           }
        }
}

void cBitmap::DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type)
{
  // TODO This is just a quick and dirty implementation of a slope drawing
  // machanism. If somebody can come up with a better solution, let's have it!
  if (!Intersects(x1, y1, x2, y2))
     return;
  bool upper    = Type & 0x01;
  bool falling  = Type & 0x02;
  bool vertical = Type & 0x04;
  if (vertical) {
     for (int y = y1; y <= y2; y++) {
         double c = cos((y - y1) * M_PI / (y2 - y1 + 1));
         if (falling)
            c = -c;
         int x = int((x2 - x1 + 1) * c / 2);
         if (upper && !falling || !upper && falling)
            DrawRectangle(x1, y, (x1 + x2) / 2 + x, y, Color);
         else
            DrawRectangle((x1 + x2) / 2 + x, y, x2, y, Color);
         }
     }
  else {
     for (int x = x1; x <= x2; x++) {
         double c = cos((x - x1) * M_PI / (x2 - x1 + 1));
         if (falling)
            c = -c;
         int y = int((y2 - y1 + 1) * c / 2);
         if (upper)
            DrawRectangle(x, y1, x, (y1 + y2) / 2 + y, Color);
         else
            DrawRectangle(x, (y1 + y2) / 2 + y, x, y2, Color);
         }
     }
}

const tIndex *cBitmap::Data(int x, int y)
{
  return &bitmap[y * width + x];
}

// --- cOsd ------------------------------------------------------------------

int cOsd::isOpen = 0;

cOsd::cOsd(int Left, int Top)
{
  if (isOpen)
     esyslog("ERROR: OSD opened without closing previous OSD!");
  savedRegion = NULL;
  numBitmaps = 0;
  left = Left;
  top = Top;
  width = height = 0;
  isOpen++;
}

cOsd::~cOsd()
{
  for (int i = 0; i < numBitmaps; i++)
      delete bitmaps[i];
  delete savedRegion;
  isOpen--;
}

cBitmap *cOsd::GetBitmap(int Area)
{
  return Area < numBitmaps ? bitmaps[Area] : NULL;
}

eOsdError cOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
  eOsdError Result = oeOk;
  for (int i = 0; i < NumAreas; i++) {
      if (Areas[i].x1 > Areas[i].x2 || Areas[i].y1 > Areas[i].y2 || Areas[i].x1 < 0 || Areas[i].y1 < 0)
         return oeWrongAlignment;
      for (int j = i + 1; j < NumAreas; j++) {
          if (Areas[i].Intersects(Areas[j])) {
             Result = oeAreasOverlap;
             break;
             }
          }
      }
  return Result;
}

eOsdError cOsd::SetAreas(const tArea *Areas, int NumAreas)
{
  eOsdError Result = oeUnknown;
  if (numBitmaps == 0) {
     Result = CanHandleAreas(Areas, NumAreas);
     if (Result == oeOk) {
        width = height = 0;
        for (int i = 0; i < NumAreas; i++) {
            bitmaps[numBitmaps++] = new cBitmap(Areas[i].Width(), Areas[i].Height(), Areas[i].bpp, Areas[i].x1, Areas[i].y1);
            width = max(width, Areas[i].x2 + 1);
            height = max(height, Areas[i].y2 + 1);
            }
        }
     }
  if (Result != oeOk)
     esyslog("ERROR: cOsd::SetAreas returned %d\n", Result);
  return Result;
}

void cOsd::SaveRegion(int x1, int y1, int x2, int y2)
{
  delete savedRegion;
  savedRegion = new cBitmap(x2 - x1 + 1, y2 - y1 + 1, 8, x1, y1);
  for (int i = 0; i < numBitmaps; i++)
      savedRegion->DrawBitmap(bitmaps[i]->X0(), bitmaps[i]->Y0(), *bitmaps[i]);
}

void cOsd::RestoreRegion(void)
{
  if (savedRegion) {
     DrawBitmap(savedRegion->X0(), savedRegion->Y0(), *savedRegion);
     delete savedRegion;
     savedRegion = NULL;
     }
}

eOsdError cOsd::SetPalette(const cPalette &Palette, int Area)
{
  if (Area < numBitmaps)
     bitmaps[Area]->Take(Palette);
  return oeUnknown;
}

void cOsd::DrawPixel(int x, int y, tColor Color)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawPixel(x, y, Color);
}

void cOsd::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawBitmap(x, y, Bitmap, ColorFg, ColorBg, ReplacePalette);
}

void cOsd::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawText(x, y, s, ColorFg, ColorBg, Font, Width, Height, Alignment);
}

void cOsd::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawRectangle(x1, y1, x2, y2, Color);
}

void cOsd::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawEllipse(x1, y1, x2, y2, Color, Quadrants);
}

void cOsd::DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type)
{
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->DrawSlope(x1, y1, x2, y2, Color, Type);
}

void cOsd::Flush(void)
{
}

// --- cOsdProvider ----------------------------------------------------------

cOsdProvider *cOsdProvider::osdProvider = NULL;

cOsdProvider::cOsdProvider(void)
{
  delete osdProvider;
  osdProvider = this;
}

cOsdProvider::~cOsdProvider()
{
  osdProvider = NULL;
}

cOsd *cOsdProvider::NewOsd(int Left, int Top)
{
  if (cOsd::IsOpen())
     esyslog("ERROR: attempt to open OSD while it is already open - using dummy OSD!");
  else if (osdProvider)
     return osdProvider->CreateOsd(Left, Top);
  else
     esyslog("ERROR: no OSD provider available - using dummy OSD!");
  return new cOsd(Left, Top); // create a dummy cOsd, so that access won't result in a segfault
}

void cOsdProvider::Shutdown(void)
{
  delete osdProvider;
  osdProvider = NULL;
}

// --- cTextScroller ---------------------------------------------------------

cTextScroller::cTextScroller(void)
{
  osd = NULL;
  left = top = width = height = 0;
  font = NULL;
  colorFg = 0;
  colorBg = 0;
  offset = 0;
  shown = 0;
}

cTextScroller::cTextScroller(cOsd *Osd, int Left, int Top, int Width, int Height, const char *Text, const cFont *Font, tColor ColorFg, tColor ColorBg)
{
  Set(Osd, Left, Top, Width, Height, Text, Font, ColorFg, ColorBg);
}

void cTextScroller::Set(cOsd *Osd, int Left, int Top, int Width, int Height, const char *Text, const cFont *Font, tColor ColorFg, tColor ColorBg)
{
  osd = Osd;
  left = Left;
  top = Top;
  width = Width;
  height = Height;
  font = Font;
  colorFg = ColorFg;
  colorBg = ColorBg;
  offset = 0;
  textWrapper.Set(Text, Font, Width);
  shown = min(Total(), height / font->Height());
  height = shown * font->Height(); // sets height to the actually used height, which may be less than Height
  DrawText();
}

void cTextScroller::Reset(void)
{
  osd = NULL; // just makes sure it won't draw anything
}

void cTextScroller::DrawText(void)
{
  if (osd) {
     for (int i = 0; i < shown; i++)
         osd->DrawText(left, top + i * font->Height(), textWrapper.GetLine(offset + i), colorFg, colorBg, font, width);
     }
}

void cTextScroller::Scroll(bool Up, bool Page)
{
  if (Up) {
     if (CanScrollUp()) {
        offset -= Page ? shown : 1;
        if (offset < 0)
           offset = 0;
        DrawText();
        }
     }
  else {
     if (CanScrollDown()) {
        offset += Page ? shown : 1;
        if (offset + shown > Total())
           offset = Total() - shown;
        DrawText();
        }
     }
}
