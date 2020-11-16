/*
 * osd.c: Abstract On Screen Display layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osd.c 4.10 2020/11/16 15:53:53 kls Exp $
 */

#include "osd.h"
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "device.h"
#include "tools.h"

tColor HsvToColor(double H, double S, double V)
{
  if (S > 0) {
     H /= 60;
     int i = floor(H);
     double f = H - i;
     double p = V * (1 - S);
     double q = V * (1 - S * f);
     double t = V * (1 - S * (1 - f));
     switch (i) {
       case 0:  return RgbToColor(V, t, p);
       case 1:  return RgbToColor(q, V, p);
       case 2:  return RgbToColor(p, V, t);
       case 3:  return RgbToColor(p, q, V);
       case 4:  return RgbToColor(t, p, V);
       default: return RgbToColor(V, p, q);
       }
     }
  else { // greyscale
     uint8_t n = V * 0xFF;
     return RgbToColor(n, n, n);
     }
}

tColor RgbShade(tColor Color, double Factor)
{
  double f = fabs(constrain(Factor, -1.0, 1.0));
  double w = Factor > 0 ? f * 0xFF : 0;
  return (Color & 0xFF000000) |
         (min(0xFF, int((1 - f) * ((Color >> 16) & 0xFF) + w + 0.5)) << 16) |
         (min(0xFF, int((1 - f) * ((Color >>  8) & 0xFF) + w + 0.5)) <<  8) |
         (min(0xFF, int((1 - f) * ( Color        & 0xFF) + w + 0.5))      );
}

#define USE_ALPHA_LUT
#ifdef USE_ALPHA_LUT
// Alpha blending with lookup table (by Reinhard Nissl <rnissl@gmx.de>)
// A little slower (138 %) on fast machines than the implementation below and faster
// on slow machines (79 %), but requires some 318KB of RAM for the lookup table.
static uint16_t AlphaLutFactors[255][256][2];
static uint8_t AlphaLutAlpha[255][256];

class cInitAlphaLut {
public:
  cInitAlphaLut(void)
  {
    for (int alphaA = 0; alphaA < 255; alphaA++) {
        int range = (alphaA == 255 ? 255 : 254);
        for (int alphaB = 0; alphaB < 256; alphaB++) {
            int alphaO_x_range = 255 * alphaA + alphaB * (range - alphaA);
            if (!alphaO_x_range)
               alphaO_x_range++;
            int factorA = (256 * 255 * alphaA + alphaO_x_range / 2) / alphaO_x_range;
            int factorB = (256 * alphaB * (range - alphaA) + alphaO_x_range / 2) / alphaO_x_range;
            AlphaLutFactors[alphaA][alphaB][0] = factorA;
            AlphaLutFactors[alphaA][alphaB][1] = factorB;
            AlphaLutAlpha[alphaA][alphaB] = alphaO_x_range / range;
            }
        }
  }
  } InitAlphaLut;

tColor AlphaBlend(tColor ColorFg, tColor ColorBg, uint8_t AlphaLayer)
{
  tColor Alpha = (ColorFg & 0xFF000000) >> 24;
  Alpha *= AlphaLayer;
  Alpha >>= 8;
  uint16_t *lut = &AlphaLutFactors[Alpha][(ColorBg & 0xFF000000) >> 24][0];
  return (tColor)((AlphaLutAlpha[Alpha][(ColorBg & 0xFF000000) >> 24] << 24)
    | (((((ColorFg & 0x00FF00FF) * lut[0] + (ColorBg & 0x00FF00FF) * lut[1])) & 0xFF00FF00)
    |  ((((ColorFg & 0x0000FF00) * lut[0] + (ColorBg & 0x0000FF00) * lut[1])) & 0x00FF0000)) >> 8);
}
#else
// Alpha blending without lookup table.
// Also works fast, but doesn't return the theoretically correct result.
// It's "good enough", though.
static tColor Multiply(tColor Color, uint8_t Alpha)
{
  tColor RB = (Color & 0x00FF00FF) * Alpha;
  RB = ((RB + ((RB >> 8) & 0x00FF00FF) + 0x00800080) >> 8) & 0x00FF00FF;
  tColor AG = ((Color >> 8) & 0x00FF00FF) * Alpha;
  AG = ((AG + ((AG >> 8) & 0x00FF00FF) + 0x00800080)) & 0xFF00FF00;
  return AG | RB;
}

tColor AlphaBlend(tColor ColorFg, tColor ColorBg, uint8_t AlphaLayer)
{
  tColor Alpha = (ColorFg & 0xFF000000) >> 24;
  if (AlphaLayer < ALPHA_OPAQUE) {
     Alpha *= AlphaLayer;
     Alpha = ((Alpha + ((Alpha >> 8) & 0x000000FF) + 0x00000080) >> 8) & 0x000000FF;
     }
  return Multiply(ColorFg, Alpha) + Multiply(ColorBg, 255 - Alpha);
}
#endif

// --- cPalette --------------------------------------------------------------

cPalette::cPalette(int Bpp)
{
  SetBpp(Bpp);
  SetAntiAliasGranularity(10, 10);
}

cPalette::~cPalette()
{
}

void cPalette::SetAntiAliasGranularity(uint FixedColors, uint BlendColors)
{
  if (FixedColors >= MAXNUMCOLORS || BlendColors == 0)
     antiAliasGranularity = MAXNUMCOLORS - 1;
  else {
     int ColorsForBlending = MAXNUMCOLORS - FixedColors;
     int ColorsPerBlend = ColorsForBlending / BlendColors + 2; // +2 = the full foreground and background colors, which are among the fixed colors
     antiAliasGranularity = double(MAXNUMCOLORS - 1) / (ColorsPerBlend - 1);
     }
}

void cPalette::Reset(void)
{
  numColors = 0;
  modified = false;
}

int cPalette::Index(tColor Color)
{
  // Check if color is already defined:
  for (int i = 0; i < numColors; i++) {
      if (color[i] == Color)
         return i;
      }
  // No exact color, try a close one:
  int i = ClosestColor(Color, 4);
  if (i >= 0)
     return i;
  // No close one, try to define a new one:
  if (numColors < maxColors) {
     color[numColors++] = Color;
     modified = true;
     return numColors - 1;
     }
  // Out of colors, so any close color must do:
  return ClosestColor(Color);
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

const tColor *cPalette::Colors(int &NumColors) const
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
           default: ;
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
  antiAliasGranularity = Palette.antiAliasGranularity;
}

tColor cPalette::Blend(tColor ColorFg, tColor ColorBg, uint8_t Level) const
{
  if (antiAliasGranularity > 0)
     Level = uint8_t(int(Level / antiAliasGranularity + 0.5) * antiAliasGranularity);
  int Af = (ColorFg & 0xFF000000) >> 24;
  int Rf = (ColorFg & 0x00FF0000) >> 16;
  int Gf = (ColorFg & 0x0000FF00) >>  8;
  int Bf = (ColorFg & 0x000000FF);
  int Ab = (ColorBg & 0xFF000000) >> 24;
  int Rb = (ColorBg & 0x00FF0000) >> 16;
  int Gb = (ColorBg & 0x0000FF00) >>  8;
  int Bb = (ColorBg & 0x000000FF);
  int A = (Ab + (Af - Ab) * Level / 0xFF) & 0xFF;
  int R = (Rb + (Rf - Rb) * Level / 0xFF) & 0xFF;
  int G = (Gb + (Gf - Gb) * Level / 0xFF) & 0xFF;
  int B = (Bb + (Bf - Bb) * Level / 0xFF) & 0xFF;
  return (A << 24) | (R << 16) | (G << 8) | B;
}

int cPalette::ClosestColor(tColor Color, int MaxDiff) const
{
  int n = 0;
  int d = INT_MAX;
  int A1 = (Color & 0xFF000000) >> 24;
  int R1 = (Color & 0x00FF0000) >> 16;
  int G1 = (Color & 0x0000FF00) >>  8;
  int B1 = (Color & 0x000000FF);
  for (int i = 0; i < numColors && d > 0; i++) {
      int A2 = (color[i] & 0xFF000000) >> 24;
      int R2 = (color[i] & 0x00FF0000) >> 16;
      int G2 = (color[i] & 0x0000FF00) >>  8;
      int B2 = (color[i] & 0x000000FF);
      int diff = 0;
      if (A1 || A2) // fully transparent colors are considered equal
         diff = (abs(A1 - A2) << 1) + (abs(R1 - R2) << 1) + (abs(G1 - G2) << 1) + (abs(B1 - B2) << 1);
      if (diff < d) {
         d = diff;
         n = i;
         }
      }
  return d <= MaxDiff ? n : -1;
}

// --- cBitmap ---------------------------------------------------------------

cBitmap::cBitmap(int Width, int Height, int Bpp, int X0, int Y0)
:cPalette(Bpp)
{
  bitmap = NULL;
  x0 = X0;
  y0 = Y0;
  width = height = 0;
  SetSize(Width, Height);
}

cBitmap::cBitmap(const char *FileName)
{
  bitmap = NULL;
  x0 = 0;
  y0 = 0;
  width = height = 0;
  LoadXpm(FileName);
}

cBitmap::cBitmap(const char *const Xpm[])
{
  bitmap = NULL;
  x0 = 0;
  y0 = 0;
  width = height = 0;
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
     esyslog("ERROR: invalid bitmap parameters (%d, %d)!", width, height);
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
                    isXpm = false;
                    break;
                    }
                 lines = h + n + 1;
                 Xpm = MALLOC(char *, lines);
                 memset(Xpm, 0, lines * sizeof(char*));
                 }
              char *q = strchr(s, '"');
              if (!q) {
                 esyslog("ERROR: missing quotes in XPM file '%s'", FileName);
                 isXpm = false;
                 break;
                 }
              *q = 0;
              if (index < lines)
                 Xpm[index++] = strdup(s);
              else {
                 esyslog("ERROR: too many lines in XPM file '%s'", FileName);
                 isXpm = false;
                 break;
                 }
              }
           }
     if (isXpm) {
        if (index == lines)
           Result = SetXpm(Xpm);
        else
           esyslog("ERROR: too few lines in XPM file '%s'", FileName);
        }
     if (Xpm) {
        for (int i = 0; i < index; i++)
            free(Xpm[i]);
        }
     free(Xpm);
     fclose(f);
     }
  else
     esyslog("ERROR: can't open XPM file '%s'", FileName);
  return Result;
}

bool cBitmap::SetXpm(const char *const Xpm[], bool IgnoreNone)
{
  if (!Xpm)
     return false;
  const char *const *p = Xpm;
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
         NoneColorIndex = i;
         if (!IgnoreNone)
            SetColor(i, clrTransparent);
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

void cBitmap::Fill(tIndex Index)
{
  if (bitmap) {
     memset(bitmap, Index, width * height);
     dirtyX1 = 0;
     dirtyY1 = 0;
     dirtyX2 = width - 1;
     dirtyY2 = height - 1;
     }
}

void cBitmap::DrawPixel(int x, int y, tColor Color)
{
  x -= x0;
  y -= y0;
  SetIndex(x, y, Index(Color));
}

void cBitmap::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette, bool Overlay)
{
  if (bitmap && Bitmap.bitmap && Intersects(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1)) {
     if (Covers(x, y, x + Bitmap.Width() - 1, y + Bitmap.Height() - 1))
        Reset();
     x -= x0;
     y -= y0;
     if (ReplacePalette && Covers(x + x0, y + y0, x + x0 + Bitmap.Width() - 1, y + y0 + Bitmap.Height() - 1)) {
        Replace(Bitmap);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++) {
                if (!Overlay || Bitmap.bitmap[Bitmap.width * iy + ix] != 0)
                   SetIndex(x + ix, y + iy, Bitmap.bitmap[Bitmap.width * iy + ix]);
                }
            }
        }
     else {
        tIndexes Indexes;
        Take(Bitmap, &Indexes, ColorFg, ColorBg);
        for (int ix = 0; ix < Bitmap.width; ix++) {
            for (int iy = 0; iy < Bitmap.height; iy++) {
                if (!Overlay || Bitmap.bitmap[Bitmap.width * iy + ix] != 0)
                   SetIndex(x + ix, y + iy, Indexes[int(Bitmap.bitmap[Bitmap.width * iy + ix])]);
                }
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
     int cw = Width ? Width : w;
     int ch = Height ? Height : h;
     if (!Intersects(x, y, x + cw - 1, y + ch - 1))
        return;
     if (ColorBg != clrTransparent)
        DrawRectangle(x, y, x + cw - 1, y + ch - 1, ColorBg);
     if (Width || Height) {
        limit = x + cw - x0;
        if (Width) {
           if ((Alignment & taLeft) != 0) {
              if ((Alignment & taBorder) != 0)
                 x += max(h / TEXT_ALIGN_BORDER, 1);
              }
           else if ((Alignment & taRight) != 0) {
              if (w < Width)
                 x += Width - w;
              if ((Alignment & taBorder) != 0)
                 x -= max(h / TEXT_ALIGN_BORDER, 1);
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
     x -= x0;
     y -= y0;
     Font->DrawText(this, x, y, s, ColorFg, ColorBg, limit);
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
     for (int y = y1; y <= y2; y++) {
         for (int x = x1; x <= x2; x++)
             SetIndex(x, y, c);
         }
     }
}

void cBitmap::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
  if (!Intersects(x1, y1, x2, y2))
     return;
  // Algorithm based on https://dai.fmph.uniba.sk/upload/0/01/Ellipse.pdf
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
    default: ;
    }
  int TwoASquare = max(1, 2 * rx * rx);
  int TwoBSquare = max(1, 2 * ry * ry);
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
          default: ;
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
          default: ;
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

const tIndex *cBitmap::Data(int x, int y) const
{
  return &bitmap[y * width + x];
}

void cBitmap::ReduceBpp(const cPalette &Palette)
{
  int NewBpp = Palette.Bpp();
  if (Bpp() == 4 && NewBpp == 2) {
     for (int i = width * height; i--; ) {
         tIndex p = bitmap[i];
         bitmap[i] = (p >> 2) | ((p & 0x03) != 0);
         }
     }
  else if (Bpp() == 8) {
     if (NewBpp == 2) {
        for (int i = width * height; i--; ) {
            tIndex p = bitmap[i];
            bitmap[i] = (p >> 6) | ((p & 0x30) != 0);
            }
        }
     else if (NewBpp == 4) {
        for (int i = width * height; i--; ) {
            tIndex p = bitmap[i];
            bitmap[i] = p >> 4;
            }
        }
     else
        return;
     }
  else
     return;
  SetBpp(NewBpp);
  Replace(Palette);
}

void cBitmap::ShrinkBpp(int NewBpp)
{
  int NumOldColors;
  const tColor *Colors = this->Colors(NumOldColors);
  if (Colors) {
     // Find the most frequently used colors and create a map table:
     int Used[MAXNUMCOLORS] = { 0 };
     int Map[MAXNUMCOLORS] = { 0 };
     for (int i = width * height; i--; )
         Used[bitmap[i]]++;
     int MaxNewColors = (NewBpp == 4) ? 16 : 4;
     cPalette NewPalette(NewBpp);
     for (int i = 0; i < MaxNewColors; i++) {
         int Max = 0;
         int Index = -1;
         for (int n = 0; n < NumOldColors; n++) {
             if (Used[n] > Max) {
                Max = Used[n];
                Index = n;
                }
             }
         if (Index >= 0) {
            Used[Index] = 0;
            Map[Index] = i;
            NewPalette.SetColor(i, Colors[Index]);
            }
         else
            break;
         }
     // Complete the map table for all other colors (will be set to closest match):
     for (int n = 0; n < NumOldColors; n++) {
         if (Used[n])
            Map[n] = NewPalette.Index(Colors[n]);
         }
     // Do the actual index mapping:
     for (int i = width * height; i--; )
         bitmap[i] = Map[bitmap[i]];
     SetBpp(NewBpp);
     Replace(NewPalette);
     }
}

cBitmap *cBitmap::Scaled(double FactorX, double FactorY, bool AntiAlias) const
{
  // Fixed point scaling code based on www.inversereality.org/files/bitmapscaling.pdf
  // by deltener@mindtremors.com
  int w = max(1, int(round(Width() * FactorX)));
  int h = max(1, int(round(Height() * FactorY)));
  cBitmap *b = new cBitmap(w, h, Bpp(), X0(), Y0());
  int RatioX = (Width() << 16) / b->Width();
  int RatioY = (Height() << 16) / b->Height();
  if (!AntiAlias || FactorX <= 1.0 && FactorY <= 1.0) {
     // Downscaling - no anti-aliasing:
     b->Replace(*this); // copy palette
     tIndex *DestRow = b->bitmap;
     int SourceY = 0;
     for (int y = 0; y < b->Height(); y++) {
         int SourceX = 0;
         tIndex *SourceRow = bitmap + (SourceY >> 16) * Width();
         tIndex *Dest = DestRow;
         for (int x = 0; x < b->Width(); x++) {
             *Dest++ = SourceRow[SourceX >> 16];
             SourceX += RatioX;
             }
         SourceY += RatioY;
         DestRow += b->Width();
         }
     }
  else {
     // Upscaling - anti-aliasing:
     b->SetBpp(8);
     b->Replace(*this); // copy palette (must be done *after* SetBpp()!)
     int SourceY = 0;
     for (int y = 0; y < b->Height(); y++) {
         int SourceX = 0;
         int sy = min(SourceY >> 16, Height() - 2);
         uint8_t BlendY = 0xFF - ((SourceY >> 8) & 0xFF);
         for (int x = 0; x < b->Width(); x++) {
             int sx = min(SourceX >> 16, Width() - 2);
             uint8_t BlendX = 0xFF - ((SourceX >> 8) & 0xFF);
             tColor c1 = b->Blend(GetColor(sx, sy),     GetColor(sx + 1, sy),     BlendX);
             tColor c2 = b->Blend(GetColor(sx, sy + 1), GetColor(sx + 1, sy + 1), BlendX);
             tColor c3 = b->Blend(c1, c2, BlendY);
             b->DrawPixel(x + X0(), y + Y0(), c3);
             SourceX += RatioX;
             }
         SourceY += RatioY;
         }
     }
  return b;
}

// --- cRect -----------------------------------------------------------------

const cRect cRect::Null;

void cRect::Grow(int Dx, int Dy)
{
  point.Shift(-Dx, -Dy);
  size.Grow(Dx, Dy);
}

bool cRect::Contains(const cPoint &Point) const
{
  return Left() <= Point.X() &&
         Top() <= Point.Y() &&
         Right() >= Point.X() &&
         Bottom() >= Point.Y();
}

bool cRect::Contains(const cRect &Rect) const
{
  return Left() <= Rect.Left() &&
         Top() <= Rect.Top() &&
         Right() >= Rect.Right() &&
         Bottom() >= Rect.Bottom();
}

bool cRect::Intersects(const cRect &Rect) const
{
  return !(Left() > Rect.Right() ||
           Top() > Rect.Bottom() ||
           Right() < Rect.Left() ||
           Bottom() < Rect.Top());
}

cRect cRect::Intersected(const cRect &Rect) const
{
  cRect r;
  if (!IsEmpty() && !Rect.IsEmpty()) {
     r.SetLeft(max(Left(), Rect.Left()));
     r.SetTop(max(Top(), Rect.Top()));
     r.SetRight(min(Right(), Rect.Right()));
     r.SetBottom(min(Bottom(), Rect.Bottom()));
     }
  return r;
}

void cRect::Combine(const cRect &Rect)
{
  if (IsEmpty())
     *this = Rect;
  if (Rect.IsEmpty())
     return;
  // must set right/bottom *before* top/left!
  SetRight(max(Right(), Rect.Right()));
  SetBottom(max(Bottom(), Rect.Bottom()));
  SetLeft(min(Left(), Rect.Left()));
  SetTop(min(Top(), Rect.Top()));
}

void cRect::Combine(const cPoint &Point)
{
  if (IsEmpty())
     Set(Point.X(), Point.Y(), 1, 1);
  // must set right/bottom *before* top/left!
  SetRight(max(Right(), Point.X()));
  SetBottom(max(Bottom(), Point.Y()));
  SetLeft(min(Left(), Point.X()));
  SetTop(min(Top(), Point.Y()));
}

// --- cPixmap ---------------------------------------------------------------

cMutex cPixmap::mutex;

cPixmap::cPixmap(void)
{
  layer = -1;
  alpha = ALPHA_OPAQUE;
  tile = false;
}

cPixmap::cPixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort)
{
  layer = Layer;
  if (layer >= MAXPIXMAPLAYERS) {
     layer = MAXPIXMAPLAYERS - 1;
     esyslog("ERROR: pixmap layer %d limited to %d", Layer, layer);
     }
  viewPort = ViewPort;
  if (!DrawPort.IsEmpty())
     drawPort = DrawPort;
  else {
     drawPort = viewPort;
     drawPort.SetPoint(0, 0);
     }
  alpha = ALPHA_OPAQUE;
  tile = false;
}

void cPixmap::MarkViewPortDirty(const cRect &Rect)
{
  if (layer >= 0)
     dirtyViewPort.Combine(Rect.Intersected(viewPort));
}

void cPixmap::MarkViewPortDirty(const cPoint &Point)
{
  if (layer >= 0 && viewPort.Contains(Point))
     dirtyViewPort.Combine(Point);
}

void cPixmap::MarkDrawPortDirty(const cRect &Rect)
{
  dirtyDrawPort.Combine(Rect.Intersected(drawPort));
  if (tile)
     MarkViewPortDirty(viewPort);
  else
     MarkViewPortDirty(Rect.Shifted(viewPort.Point()));
}

void cPixmap::MarkDrawPortDirty(const cPoint &Point)
{
  if (drawPort.Contains(Point)) {
     dirtyDrawPort.Combine(Point);
     if (tile)
        MarkViewPortDirty(viewPort);
     else
        MarkViewPortDirty(Point.Shifted(viewPort.Point()));
     }
}

void cPixmap::SetClean(void)
{
  dirtyViewPort = dirtyDrawPort = cRect();
}

void cPixmap::SetLayer(int Layer)
{
  Lock();
  if (Layer >= MAXPIXMAPLAYERS) {
     esyslog("ERROR: pixmap layer %d limited to %d", Layer, MAXPIXMAPLAYERS - 1);
     Layer = MAXPIXMAPLAYERS - 1;
     }
  // The sequence here is important, because the view port is only marked as dirty
  // if the layer is >= 0:
  if (layer >= 0) {
     MarkViewPortDirty(viewPort); // the pixmap is visible and may or may not become invisible
     layer = Layer;
     }
  else if (Layer >= 0) {
     layer = Layer;
     MarkViewPortDirty(viewPort); // the pixmap was invisible and has become visible
     }
  else
     layer = Layer; // the pixmap was invisible and remains so
  Unlock();
}

void cPixmap::SetAlpha(int Alpha)
{
  Lock();
  Alpha = constrain(Alpha, ALPHA_TRANSPARENT, ALPHA_OPAQUE);
  if (Alpha != alpha) {
     MarkViewPortDirty(viewPort);
     alpha = Alpha;
     }
  Unlock();
}

void cPixmap::SetTile(bool Tile)
{
  Lock();
  if (Tile != tile) {
     if (drawPort.Point() != cPoint(0, 0) || drawPort.Width() < viewPort.Width() || drawPort.Height() < viewPort.Height())
        MarkViewPortDirty(viewPort);
     tile = Tile;
     }
  Unlock();
}

void cPixmap::SetViewPort(const cRect &Rect)
{
  Lock();
  if (Rect != viewPort) {
     if (tile)
        MarkViewPortDirty(viewPort);
     else
        MarkViewPortDirty(drawPort.Shifted(viewPort.Point()));
     viewPort = Rect;
     if (tile)
        MarkViewPortDirty(viewPort);
     else
        MarkViewPortDirty(drawPort.Shifted(viewPort.Point()));
     }
  Unlock();
}

void cPixmap::SetDrawPortPoint(const cPoint &Point, bool Dirty)
{
  Lock();
  if (Point != drawPort.Point()) {
     if (Dirty) {
        if (tile)
           MarkViewPortDirty(viewPort);
        else
           MarkViewPortDirty(drawPort.Shifted(viewPort.Point()));
        }
     drawPort.SetPoint(Point);
     if (Dirty && !tile)
        MarkViewPortDirty(drawPort.Shifted(viewPort.Point()));
     }
  Unlock();
}

// --- cImage ----------------------------------------------------------------

cImage::cImage(void)
{
  data = NULL;
}

cImage::cImage(const cImage &Image)
{
  size = Image.Size();
  int l = size.Width() * size.Height();
  data = MALLOC(tColor, l);
  memcpy(data, Image.Data(), l * sizeof(tColor));
}

cImage::cImage(const cSize &Size, const tColor *Data)
{
  size = Size;
  int l = size.Width() * size.Height();
  data = MALLOC(tColor, l);
  if (Data)
     memcpy(data, Data, l * sizeof(tColor));
}

cImage::~cImage()
{
  free(data);
}

void cImage::Clear(void)
{
  memset(data, 0x00, Width() * Height() * sizeof(tColor));
}

void cImage::Fill(tColor Color)
{
  for (int i = Width() * Height() - 1; i >= 0; i--)
      data[i] = Color;
}

// --- cPixmapMemory ---------------------------------------------------------

cPixmapMemory::cPixmapMemory(void)
{
  data = NULL;
  panning = false;
}

cPixmapMemory::cPixmapMemory(int Layer, const cRect &ViewPort, const cRect &DrawPort)
:cPixmap(Layer, ViewPort, DrawPort)
{
  data = MALLOC(tColor, this->DrawPort().Width() * this->DrawPort().Height());
  panning = false;
}

cPixmapMemory::~cPixmapMemory()
{
  free(data);
}

void cPixmapMemory::Clear(void)
{
  Lock();
  memset(data, 0x00, DrawPort().Width() * DrawPort().Height() * sizeof(tColor));
  MarkDrawPortDirty(DrawPort());
  Unlock();
}

void cPixmapMemory::Fill(tColor Color)
{
  Lock();
  for (int i = DrawPort().Width() * DrawPort().Height() - 1; i >= 0; i--)
      data[i] = Color;
  MarkDrawPortDirty(DrawPort());
  Unlock();
}

void cPixmap::DrawPixmap(const cPixmap *Pixmap, const cRect &Dirty)
{
  if (Pixmap->Tile() && (Pixmap->DrawPort().Point() != cPoint(0, 0) || Pixmap->DrawPort().Size() < Pixmap->ViewPort().Size())) {
     cPoint t0 = Pixmap->DrawPort().Point().Shifted(Pixmap->ViewPort().Point()); // the origin of the draw port in absolute OSD coordinates
     // Find the top/leftmost location where the draw port touches the view port:
     while (t0.X() > Pixmap->ViewPort().Left())
           t0.Shift(-Pixmap->DrawPort().Width(), 0);
     while (t0.Y() > Pixmap->ViewPort().Top())
           t0.Shift(0, -Pixmap->DrawPort().Height());
     cPoint t = t0;
     while (t.Y() <= Pixmap->ViewPort().Bottom()) {
           while (t.X() <= Pixmap->ViewPort().Right()) {
                 cRect Source = Pixmap->DrawPort(); // assume the entire pixmap needs to be rendered
                 Source.Shift(Pixmap->ViewPort().Point()); // Source is now in absolute OSD coordinates
                 cPoint Delta = Source.Point() - t;
                 Source.SetPoint(t); // Source is now where the pixmap's data shall be drawn
                 Source = Source.Intersected(Pixmap->ViewPort()); // Source is now limited to the pixmap's view port
                 Source = Source.Intersected(Dirty); // Source is now limited to the actual dirty rectangle
                 if (!Source.IsEmpty()) {
                    cPoint Dest = Source.Point().Shifted(-ViewPort().Point()); // remember the destination point
                    Source.Shift(Delta); // Source is now back at the pixmap's draw port location, still in absolute OSD coordinates
                    Source.Shift(-Pixmap->ViewPort().Point()); // Source is now relative to the pixmap's view port again
                    Source.Shift(-Pixmap->DrawPort().Point()); // Source is now relative to the pixmap's data
                    if (Pixmap->Layer() == 0)
                       Copy(Pixmap, Source, Dest); // this is the "background" pixmap
                    else
                       Render(Pixmap, Source, Dest); // all others are alpha blended over the background
                    }
                 t.Shift(Pixmap->DrawPort().Width(), 0); // increase one draw port width to the right
                 }
           t.SetX(t0.X()); // go back to the leftmost position
           t.Shift(0, Pixmap->DrawPort().Height()); // increase one draw port height down
           }
     }
  else {
     cRect Source = Pixmap->DrawPort(); // assume the entire pixmap needs to be rendered
     Source.Shift(Pixmap->ViewPort().Point()); // Source is now in absolute OSD coordinates
     Source = Source.Intersected(Pixmap->ViewPort()); // Source is now limited to the pixmap's view port
     Source = Source.Intersected(Dirty); // Source is now limited to the actual dirty rectangle
     if (!Source.IsEmpty()) {
        cPoint Dest = Source.Point().Shifted(-ViewPort().Point()); // remember the destination point
        Source.Shift(-Pixmap->ViewPort().Point()); // Source is now relative to the pixmap's draw port again
        Source.Shift(-Pixmap->DrawPort().Point()); // Source is now relative to the pixmap's data
        if (Pixmap->Layer() == 0)
           Copy(Pixmap, Source, Dest); // this is the "background" pixmap
        else
           Render(Pixmap, Source, Dest); // all others are alpha blended over the background
        }
     }
}

void cPixmapMemory::DrawImage(const cPoint &Point, const cImage &Image)
{
  Lock();
  cRect r = cRect(Point, Image.Size()).Intersected(DrawPort().Size());
  if (!r.IsEmpty()) {
     int ws = Image.Size().Width();
     int wd = DrawPort().Width();
     int w = r.Width() * sizeof(tColor);
     const tColor *ps = Image.Data();
     if (Point.Y() < 0)
        ps -= Point.Y() * ws;
     if (Point.X() < 0)
        ps -= Point.X();
     tColor *pd = data + wd * r.Top() + r.Left();
     for (int y = r.Height(); y-- > 0; ) {
         memcpy(pd, ps, w);
         ps += ws;
         pd += wd;
         }
     MarkDrawPortDirty(r);
     }
  Unlock();
}

void cPixmapMemory::DrawImage(const cPoint &Point, int ImageHandle)
{
  Lock();
  if (const cImage *Image = cOsdProvider::GetImageData(ImageHandle))
     DrawImage(Point, *Image);
  Unlock();
}

void cPixmapMemory::DrawPixel(const cPoint &Point, tColor Color)
{
  Lock();
  if (DrawPort().Size().Contains(Point)) {
     int p = Point.Y() * DrawPort().Width() + Point.X();
     if (Layer() == 0 && !IS_OPAQUE(Color))
        data[p] = AlphaBlend(Color, data[p]);
     else
        data[p] = Color;
     MarkDrawPortDirty(Point);
     }
  Unlock();
}

void cPixmapMemory::DrawBlendedPixel(const cPoint &Point, tColor Color, uint8_t Alpha)
{
  Lock();
  if (DrawPort().Size().Contains(Point)) {
     int p = Point.Y() * DrawPort().Width() + Point.X();
     if (Alpha != ALPHA_OPAQUE) {
        if (Color == clrTransparent)
           data[p] = (data[p] & 0x00FFFFFF) | ((((data[p] >> 24) * (255 - Alpha)) << 16) & 0xFF000000);
        else
           data[p] = AlphaBlend(Color, data[p], Alpha);
        }
     else
        data[p] = Color;
     MarkDrawPortDirty(Point);
     }
  Unlock();
}

void cPixmapMemory::DrawBitmap(const cPoint &Point, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool Overlay)
{
  Lock();
  cRect r = cRect(Point, cSize(Bitmap.Width(), Bitmap.Height())).Intersected(DrawPort().Size());
  if (!r.IsEmpty()) {
     bool UseColors = ColorFg || ColorBg;
     int wd = DrawPort().Width();
     tColor *pd = data + wd * r.Top() + r.Left();
     for (int y = r.Top(); y <= r.Bottom(); y++) {
         tColor *cd = pd;
         for (int x = r.Left(); x <= r.Right(); x++) {
             tIndex Index = *Bitmap.Data(x - Point.X(), y - Point.Y());
             if (Index || !Overlay) {
                if (UseColors)
                   *cd = Index ? ColorFg : ColorBg;
                else
                   *cd = Bitmap.Color(Index);
                }
             cd++;
             }
         pd += wd;
         }
     MarkDrawPortDirty(r);
     }
  Unlock();
}

void cPixmapMemory::DrawText(const cPoint &Point, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
  Lock();
  int x = Point.X();
  int y = Point.Y();
  int w = Font->Width(s);
  int h = Font->Height();
  int limit = 0;
  int cw = Width ? Width : w;
  int ch = Height ? Height : h;
  cRect r(x, y, cw, ch);
  if (ColorBg != clrTransparent)
     DrawRectangle(r, ColorBg);
  if (Width || Height) {
     limit = x + cw;
     if (Width) {
        if ((Alignment & taLeft) != 0) {
           if ((Alignment & taBorder) != 0)
              x += max(h / TEXT_ALIGN_BORDER, 1);
           }
        else if ((Alignment & taRight) != 0) {
           if (w < Width)
              x += Width - w;
           if ((Alignment & taBorder) != 0)
              x -= max(h / TEXT_ALIGN_BORDER, 1);
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
  Font->DrawText(this, x, y, s, ColorFg, ColorBg, limit);
  MarkDrawPortDirty(r);
  Unlock();
}

void cPixmapMemory::DrawRectangle(const cRect &Rect, tColor Color)
{
  Lock();
  cRect r = Rect.Intersected(DrawPort().Size());
  if (!r.IsEmpty()) {
     int wd = DrawPort().Width();
     int w = r.Width() * sizeof(tColor);
     tColor *ps = NULL;
     tColor *pd = data + wd * r.Top() + r.Left();
     for (int y = r.Height(); y-- > 0; ) {
         if (ps)
            memcpy(pd, ps, w); // all other lines are copied fast from the first one
         else {
            // explicitly fill the first line:
            tColor *cd = ps = pd;
            for (int x = r.Width(); x-- > 0; ) {
                *cd = Color;
                cd++;
                }
            }
         pd += wd;
         }
     MarkDrawPortDirty(r);
     }
  Unlock();
}

void cPixmapMemory::DrawEllipse(const cRect &Rect, tColor Color, int Quadrants)
{
  Lock();
  // Algorithm based on https://dai.fmph.uniba.sk/upload/0/01/Ellipse.pdf
  int x1 = Rect.Left();
  int y1 = Rect.Top();
  int x2 = Rect.Right();
  int y2 = Rect.Bottom();
  int rx = x2 - x1;
  int ry = y2 - y1;
  int ax = rx & 0x01; // alignment to make semi-circles match rectangles of same size
  int ay = ry & 0x01;
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
    default: ;
    }
  int TwoASquare = max(1, 2 * rx * rx);
  int TwoBSquare = max(1, 2 * ry * ry);
  int x = rx;
  int y = 0;
  int XChange = ry * ry * (1 - 2 * rx);
  int YChange = rx * rx;
  int EllipseError = 0;
  int StoppingX = TwoBSquare * rx;
  int StoppingY = 0;
  int Delta = 0;
  bool AntiAliased = Setup.AntiAlias;
  while (StoppingX >= StoppingY) {
        if (!AntiAliased) {
           switch (Quadrants) {
             case  5: DrawRectangle(cRect(cx,     cy + y + ay, x + 1, 1), Color); // no break
             case  1: DrawRectangle(cRect(cx,     cy - y,      x + 1, 1), Color); break;
             case  7: DrawRectangle(cRect(cx - x, cy + y + ay, x + 1, 1), Color); // no break
             case  2: DrawRectangle(cRect(cx - x, cy - y,      x + 1, 1), Color); break;
             case  3: DrawRectangle(cRect(cx - x, cy + y,      x + 1, 1), Color); break;
             case  4: DrawRectangle(cRect(cx,     cy + y,      x + 1, 1), Color); break;
             case  0:
             case  6: DrawRectangle(cRect(cx - x, cy - y, 2 * x + ax + 1,  1), Color); if (Quadrants == 6) break;
             case  8: DrawRectangle(cRect(cx - x, cy + y, 2 * x + ax + 1,  1), Color); break;
             case -1: DrawRectangle(cRect(cx + x, cy - y, rx - x + 1,      1), Color); break;
             case -2: DrawRectangle(cRect(x1,     cy - y, cx - x - x1 + 1, 1), Color); break;
             case -3: DrawRectangle(cRect(x1,     cy + y, cx - x - x1 + 1, 1), Color); break;
             case -4: DrawRectangle(cRect(cx + x, cy + y, rx - x + 1,      1), Color); break;
             default: ;
             }
           }
        else {
           uint8_t intensity = abs(255 * (long int)EllipseError / XChange);
           if (EllipseError >= 0) {
              intensity = 255 - intensity;
              Delta = 0;
              }
           else
              Delta = 1;        
           switch (Quadrants) {
             case  5: DrawRectangle(   cRect( cx,             cy + y + ay, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx + x + Delta, cy + y + ay),               Color, intensity);
                      // no break
             case  1: DrawRectangle(   cRect( cx,             cy - y, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx + x + Delta, cy - y),               Color, intensity);
                      break;
             case  7: DrawRectangle(   cRect( cx - x + 1 - Delta, cy + ay + y, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta,     cy + ay + y),               Color, intensity);
                      // no break
             case  2: DrawRectangle(   cRect( cx - x + 1 - Delta, cy - y, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta,     cy - y),               Color, intensity);
                      break;
             case  3: DrawRectangle(   cRect( cx - x + 1 - Delta, cy + y, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta,     cy + y),               Color, intensity);
                      break;
             case  4: DrawRectangle(   cRect( cx,             cy + y, x + Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx + x + Delta, cy + y),               Color, intensity);
                      break;
             case  0:
             case  6: DrawRectangle(   cRect( cx - x - Delta + 1,  cy - y, 2 * (x + Delta) + ax - 1, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta,      cy - y),                              Color, intensity);
                      DrawBlendedPixel(cPoint(cx + x + Delta + ax, cy - y),                              Color, intensity);
                      if (Quadrants == 6)
                         break;
             case  8: DrawRectangle(   cRect( cx - x - Delta + 1,  cy + y, 2 * (x + Delta) + ax - 1 , 1), Color);   
                      DrawBlendedPixel(cPoint(cx - x - Delta,      cy + y),                               Color, intensity);
                      DrawBlendedPixel(cPoint(cx + x + Delta + ax, cy + y),                               Color, intensity);
                      break;
             case -1: DrawRectangle(   cRect( cx + x + 1 + Delta, cy - y, rx - (x + Delta), 1), Color);
                      DrawBlendedPixel(cPoint(cx + x + Delta,     cy - y),                      Color, 255-intensity);
                      break;
             case -2: DrawRectangle(   cRect( x1,             cy - y, rx - x - Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta, cy - y),                    Color, 255-intensity);
                      break;
             case -3: DrawRectangle(   cRect( x1,             cy + y, rx - x - Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx - x - Delta, cy + y),                    Color, 255-intensity);
                      break;
             case -4: DrawRectangle(   cRect( cx + x + 1 + Delta, cy + y, rx - x - Delta, 1), Color);
                      DrawBlendedPixel(cPoint(cx + x + Delta,     cy + y),                    Color, 255-intensity);
                      break;
             default: ;
             }
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
  int ymax = y - 1;
  x = 0;
  y = ry;
  XChange = ry * ry;
  YChange = rx * rx * (1 - 2 * ry);
  EllipseError = 0;
  StoppingX = 0;
  StoppingY = TwoASquare * ry;
  while (StoppingX <= StoppingY) {
        if (!AntiAliased) {
           switch (Quadrants) {
             case  5: DrawRectangle(cRect(cx,     cy + y + ay, x + 1, 1), Color); // no break
             case  1: DrawRectangle(cRect(cx,     cy - y,      x + 1, 1), Color); break;
             case  7: DrawRectangle(cRect(cx - x, cy + y + ay, x + 1, 1), Color); // no break
             case  2: DrawRectangle(cRect(cx - x, cy - y,      x + 1, 1), Color); break;
             case  3: DrawRectangle(cRect(cx - x, cy + y,      x + 1, 1), Color); break;
             case  4: DrawRectangle(cRect(cx,     cy + y,      x + 1, 1), Color); break;
             case  0:
             case  6: DrawRectangle(cRect(cx - x, cy - y, 2 * x + ax + 1,  1), Color); if (Quadrants == 6) break;
             case  8: DrawRectangle(cRect(cx - x, cy + y, 2 * x + ax + 1,  1), Color); break;
             case -1: DrawRectangle(cRect(cx + x, cy - y, rx - x + 1,      1), Color); break;
             case -2: DrawRectangle(cRect(x1,     cy - y, cx - x - x1 + 1, 1), Color); break;
             case -3: DrawRectangle(cRect(x1,     cy + y, cx - x - x1 + 1, 1), Color); break;
             case -4: DrawRectangle(cRect(cx + x, cy + y, rx - x + 1,      1), Color); break;
             default: ;
             }
           }
        else {
           uint8_t intensity = abs(255 * (long int)EllipseError / YChange);
           if (EllipseError >= 0) {
              intensity = 255 - intensity;
              Delta = 1;
              }
           else
              Delta = 0;
           switch (Quadrants) {
             case  5: DrawRectangle(   cRect( cx + x, cy + ay + 1 + ymax,  1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx + x, cy + ay + y + 1 - Delta),                  Color, intensity);
                      // no break
             case  1: DrawRectangle(   cRect( cx + x, cy - y + Delta, 1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx + x, cy - y - 1 + Delta),                  Color, intensity);
                      break;
             case  7: DrawRectangle(   cRect( cx - x, cy + ay + 1 + ymax,  1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx - x, cy + ay + y + 1 - Delta),                  Color, intensity);
                      // no break
             case  2: DrawRectangle(   cRect( cx - x, cy - y + Delta, 1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx - x, cy - y - 1 + Delta),                  Color, intensity);
                      break;
             case  3: DrawRectangle(   cRect( cx - x, cy + 1 + ymax, 1, y - ymax - Delta),  Color);
                      DrawBlendedPixel(cPoint(cx - x, cy + y + 1 - Delta),                  Color, intensity);
                      break;
             case  4: DrawRectangle(   cRect( cx + x, cy + 1 + ymax, 1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx + x, cy + y + 1 - Delta),                  Color, intensity);
                      break;
             case  0:
             case  6: DrawRectangle(   cRect( cx + x + ax, cy - y + Delta, 1, y - ymax - Delta), Color);
                      DrawRectangle(   cRect( cx - x,      cy - y + Delta, 1, y - ymax - Delta), Color);
                      DrawBlendedPixel(cPoint(cx - x,      cy - y + Delta - 1),                  Color, intensity);
                      DrawBlendedPixel(cPoint(cx + x + ax, cy - y + Delta - 1),                  Color, intensity);
                      if (Quadrants == 6)
                         break;
             case  8: DrawRectangle(   cRect( cx - x,      cy + 1 + ymax, 1, y - ymax - Delta), Color);
                      DrawRectangle(   cRect( cx + x + ax, cy + 1 + ymax, 1, y - ymax - Delta), Color);   
                      DrawBlendedPixel(cPoint(cx - x,      cy + y + 1 - Delta),                 Color, intensity);
                      DrawBlendedPixel(cPoint(cx + x + ax, cy + y + 1 - Delta),                 Color, intensity);
                      break;
             case -1: DrawRectangle(   cRect( cx + x, cy - ry, 1, ry - y - 1 + Delta), Color);
                      DrawBlendedPixel(cPoint(cx + x, cy - y - 1 + Delta),             Color, 255-intensity);
                      break;
             case -2: DrawRectangle(   cRect( cx - x, cy - ry, 1, ry - y - 1 + Delta), Color);
                      DrawBlendedPixel(cPoint(cx - x, cy - y - 1 + Delta),             Color, 255-intensity);
                      break;
             case -3: DrawRectangle(   cRect( cx - x, cy + y + 2 - Delta, 1, ry - y - 1 + Delta), Color);
                      DrawBlendedPixel(cPoint(cx - x, cy + y + 1 - Delta),                        Color, 255-intensity);
                      break;
             case -4: DrawRectangle(   cRect( cx + x, cy + y + 2 - Delta, 1, ry - y - 1 + Delta), Color);
                      DrawBlendedPixel(cPoint(cx + x, cy + y + 1 - Delta),                        Color, 255-intensity);
                      break;
             default: ;
             }
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
  if (AntiAliased && Quadrants < 0 ) {
     switch (Quadrants) {
        case -1: DrawRectangle(cRect(cx + x, cy - ry,    rx - x + 1, ry - y), Color); break;
        case -2: DrawRectangle(cRect(x1,     cy - ry,    rx - x + 1, ry - y), Color); break;
        case -3: DrawRectangle(cRect(x1,     cy + y + 1, rx - x + 1, ry - y), Color); break;
        case -4: DrawRectangle(cRect(cx + x, cy + y + 1, rx - x + 1, ry - y), Color); break;
        default: ;
        }
     }
  MarkDrawPortDirty(Rect);
  Unlock();
}

void cPixmapMemory::DrawSlope(const cRect &Rect, tColor Color, int Type)
{
  //TODO also simplify cBitmap::DrawSlope()
  Lock();
  bool upper    = Type & 0x01;
  bool falling  = Type & 0x02;
  bool vertical = Type & 0x04;
  int x1 = Rect.Left();
  int y1 = Rect.Top();
  int x2 = Rect.Right();
  int y2 = Rect.Bottom();
  int w  = Rect.Width();
  int h  = Rect.Height();
  bool AntiAliased = Setup.AntiAlias;
  uint8_t intensity = 0;

  if (vertical) {
     if (upper)
        DrawRectangle(cRect(x1, y1, w, 1), Color);
     else
        DrawRectangle(cRect(x1, y2, w, 1), Color);
     for (int y = 1; y <= (y2 - y1) / 2; y++) {
         double c = cos(y * M_PI / (y2 - y1));
         if (AntiAliased) {
            double wc = (w * c + (w & 1)) / 2;
            intensity = 255 * fabs(wc - floor(wc));
            }
         if (falling)
            c = -c;
         int x = (x1 + x2 + w * c + 1) / 2;
         if (upper && !falling || !upper && falling) {
            if (AntiAliased) {
               DrawRectangle(cRect(x1, y1 + y, x - x1, 1), Color);
               DrawBlendedPixel(cPoint(x, y1 + y), Color, upper ? intensity : 255 - intensity);
               DrawRectangle(cRect(x1, y2 - y, x2 - x, 1), Color);
               DrawBlendedPixel(cPoint(x1 + x2 - x, y2 - y), Color, upper ? 255 - intensity : intensity);
               }
            else {
               DrawRectangle(cRect(x1, y1 + y, x - x1 + 1, 1), Color);
               DrawRectangle(cRect(x1, y2 - y, x2 - x + 1, 1), Color);
               }
            }
         else {
            if (AntiAliased) {
               DrawRectangle(cRect(x + 1, y1 + y, x2 - x, 1), Color);
               DrawBlendedPixel(cPoint(x, y1 + y), Color, falling ? intensity : 255 - intensity);
               DrawRectangle(cRect(x1 + x2 - x + 1, y2 - y, x - x1, 1), Color);
               DrawBlendedPixel(cPoint(x1 + x2 - x, y2 - y), Color, falling ? 255 - intensity : intensity);
               }
            else {
               DrawRectangle(cRect(x, y1 + y, x2 - x + 1, 1), Color);
               DrawRectangle(cRect(x1 + x2 - x, y2 - y, x - x1 + 1, 1), Color);
               }
            }
         }
     }
  else {
     if ((upper && !falling) || (!upper && falling))
        DrawRectangle(cRect(x1, y1, 1, h), Color);
     else
        DrawRectangle(cRect(x2, y1, 1, h), Color);
     for (int x = 1; x <= (x2 - x1) / 2; x++) {
         double c = cos(x * M_PI / (x2 - x1));
         if (AntiAliased) {
            double hc = (h * c + (h & 1)) / 2;
            intensity = 255 * fabs(hc - floor(hc));
            }
         if (falling)
            c = -c;
         int y = (y1 + y2 + h * c + 1) / 2;
         if (upper) {
            if (AntiAliased) {
               DrawRectangle(cRect(x1 + x, y1, 1, y - y1), Color);
               DrawBlendedPixel(cPoint(x1 + x, y), Color, falling ? 255 - intensity : intensity);
               DrawRectangle(cRect(x2 - x, y1, 1, y2 - y), Color); 
               DrawBlendedPixel(cPoint(x2 - x, y1 + y2 - y), Color, falling ? intensity : 255 - intensity);
               }
            else {
               DrawRectangle(cRect(x1 + x, y1, 1, y - y1 + 1), Color);
               DrawRectangle(cRect(x2 - x, y1, 1, y2 - y + 1), Color);
               }
            }
         else {
            if (AntiAliased) {
               DrawRectangle(cRect(x1 + x, y + 1, 1, y2 - y), Color);
               DrawBlendedPixel(cPoint(x1 + x, y), Color, falling ? intensity : 255 - intensity);
               DrawRectangle(cRect(x2 - x, y1 + y2 - y + 1, 1, y - y1), Color);
               DrawBlendedPixel(cPoint(x2 - x, y1 + y2 - y), Color, falling ? 255 - intensity : intensity);
               }
            else {
               DrawRectangle(cRect(x1 + x, y, 1, y2 - y + 1), Color);
               DrawRectangle(cRect(x2 - x, y1 + y2 - y, 1, y - y1 + 1), Color);
               }
            }
         }
     }
  MarkDrawPortDirty(Rect);
  Unlock();
}

void cPixmapMemory::Render(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest)
{
  Lock();
  if (Pixmap->Alpha() != ALPHA_TRANSPARENT) {
     if (const cPixmapMemory *pm = dynamic_cast<const cPixmapMemory *>(Pixmap)) {
        cRect s = Source.Intersected(Pixmap->DrawPort().Size());
        if (!s.IsEmpty()) {
           cPoint v = Dest - Source.Point();
           cRect d = s.Shifted(v).Intersected(DrawPort().Size());
           if (!d.IsEmpty()) {
              s = d.Shifted(-v);
              int a = pm->Alpha();
              int ws = pm->DrawPort().Width();
              int wd = DrawPort().Width();
              const tColor *ps = pm->data + ws * s.Top() + s.Left();
              tColor *pd = data + wd * d.Top() + d.Left();
              for (int y = d.Height(); y-- > 0; ) {
                  const tColor *cs = ps;
                  tColor *cd = pd;
                  for (int x = d.Width(); x-- > 0; ) {
                      *cd = AlphaBlend(*cs, *cd, a);
                      cs++;
                      cd++;
                      }
                  ps += ws;
                  pd += wd;
                  }
              MarkDrawPortDirty(d);
              }
           }
        }
     }
  Unlock();
}

void cPixmapMemory::Copy(const cPixmap *Pixmap, const cRect &Source, const cPoint &Dest)
{
  Lock();
  if (const cPixmapMemory *pm = dynamic_cast<const cPixmapMemory *>(Pixmap)) {
     cRect s = Source.Intersected(pm->DrawPort().Size());
     if (!s.IsEmpty()) {
        cPoint v = Dest - Source.Point();
        cRect d = s.Shifted(v).Intersected(DrawPort().Size());
        if (!d.IsEmpty()) {
           s = d.Shifted(-v);
           int ws = pm->DrawPort().Width();
           int wd = DrawPort().Width();
           int w = d.Width() * sizeof(tColor);
           const tColor *ps = pm->data + ws * s.Top() + s.Left();
           tColor *pd = data + wd * d.Top() + d.Left();
           for (int y = d.Height(); y-- > 0; ) {
               memcpy(pd, ps, w);
               ps += ws;
               pd += wd;
               }
           MarkDrawPortDirty(d);
           }
        }
     }
  Unlock();
}

void cPixmapMemory::Scroll(const cPoint &Dest, const cRect &Source)
{
  Lock();
  cRect s;
  if (&Source == &cRect::Null)
     s = DrawPort().Shifted(-DrawPort().Point());
  else
     s = Source.Intersected(DrawPort().Size());
  if (!s.IsEmpty()) {
     cPoint v = Dest - Source.Point();
     cRect d = s.Shifted(v).Intersected(DrawPort().Size());
     if (!d.IsEmpty()) {
        s = d.Shifted(-v);
        if (d.Point() != s.Point()) {
           int ws = DrawPort().Width();
           int wd = ws;
           int w = d.Width() * sizeof(tColor);
           const tColor *ps = data + ws * s.Top() + s.Left();
           tColor *pd = data + wd * d.Top() + d.Left();
           for (int y = d.Height(); y-- > 0; ) {
               memmove(pd, ps, w); // source and destination might overlap!
               ps += ws;
               pd += wd;
               }
           if (panning)
              SetDrawPortPoint(DrawPort().Point().Shifted(s.Point() - d.Point()), false);
           else
              MarkDrawPortDirty(d);
           }
        }
     }
  Unlock();
}

void cPixmapMemory::Pan(const cPoint &Dest, const cRect &Source)
{
  Lock();
  panning = true;
  Scroll(Dest, Source);
  panning = false;
  Unlock();
}

// --- cOsd ------------------------------------------------------------------

static const char *OsdErrorTexts[] = {
  "ok",
  "too many areas",
  "too many colors",
  "bpp not supported",
  "areas overlap",
  "wrong alignment",
  "out of memory",
  "wrong area size",
  "unknown",
  };

int cOsd::osdLeft = 0;
int cOsd::osdTop = 0;
int cOsd::osdWidth = 0;
int cOsd::osdHeight = 0;
cSize cOsd::maxPixmapSize(INT_MAX, INT_MAX);
cVector<cOsd *> cOsd::Osds;
cMutex cOsd::mutex;

cOsd::cOsd(int Left, int Top, uint Level)
{
  cMutexLock MutexLock(&mutex);
  isTrueColor = false;
  savedBitmap = NULL;
  numBitmaps = 0;
  savedPixmap = NULL;
  left = Left;
  top = Top;
  width = height = 0;
  level = Level;
  active = false;
  for (int i = 0; i < Osds.Size(); i++) {
      if (Osds[i]->level > level) {
         Osds.Insert(this, i);
         return;
         }
      }
  Osds.Append(this);
}

cOsd::~cOsd()
{
  cMutexLock MutexLock(&mutex);
  for (int i = 0; i < numBitmaps; i++)
      delete bitmaps[i];
  delete savedBitmap;
  delete savedPixmap;
  for (int i = 0; i < pixmaps.Size(); i++)
      delete pixmaps[i];
  for (int i = 0; i < Osds.Size(); i++) {
      if (Osds[i] == this) {
         Osds.Remove(i);
         if (Osds.Size())
            Osds[0]->SetActive(true);
         break;
         }
      }
}

void cOsd::SetOsdPosition(int Left, int Top, int Width, int Height)
{
  osdLeft = Left;
  osdTop = Top;
  osdWidth = constrain(Width, MINOSDWIDTH, MAXOSDWIDTH);
  osdHeight = constrain(Height, MINOSDHEIGHT, MAXOSDHEIGHT);
}

void cOsd::SetAntiAliasGranularity(uint FixedColors, uint BlendColors)
{
  if (isTrueColor)
     return;
  for (int i = 0; i < numBitmaps; i++)
      bitmaps[i]->SetAntiAliasGranularity(FixedColors, BlendColors);
}

cBitmap *cOsd::GetBitmap(int Area)
{
  return Area < numBitmaps ? (isTrueColor ? bitmaps[0] : bitmaps[Area]) : NULL;
}

const cSize &cOsd::MaxPixmapSize(void) const
{
  return maxPixmapSize;
}

cPixmap *cOsd::CreatePixmap(int Layer, const cRect &ViewPort, const cRect &DrawPort)
{
  if (isTrueColor) {
     LOCK_PIXMAPS;
     cPixmap *Pixmap = new cPixmapMemory(Layer, ViewPort, DrawPort);
     if (AddPixmap(Pixmap))
        return Pixmap;
     delete Pixmap;
     }
  return NULL;
}

void cOsd::DestroyPixmap(cPixmap *Pixmap)
{
  if (Pixmap) {
     LOCK_PIXMAPS;
     for (int i = 1; i < pixmaps.Size(); i++) { // begin at 1 - don't let the background pixmap be destroyed!
         if (pixmaps[i] == Pixmap) {
            if (Pixmap->Layer() >= 0)
               pixmaps[0]->MarkViewPortDirty(Pixmap->ViewPort());
            delete Pixmap;
            pixmaps[i] = NULL;
            return;
            }
         }
     esyslog("ERROR: attempt to destroy an unregistered pixmap");
     }
}

cPixmap *cOsd::AddPixmap(cPixmap *Pixmap)
{
  if (Pixmap) {
     LOCK_PIXMAPS;
     for (int i = 0; i < pixmaps.Size(); i++) {
         if (!pixmaps[i])
            return pixmaps[i] = Pixmap;
         }
     pixmaps.Append(Pixmap);
     }
  return Pixmap;
}

cPixmap *cOsd::RenderPixmaps(void)
{
  cPixmap *Pixmap = NULL;
  if (isTrueColor) {
     LOCK_PIXMAPS;
     // Collect overlapping dirty rectangles:
     cRect d;
     for (int i = 0; i < pixmaps.Size(); i++) {
         if (cPixmap *pm = pixmaps[i]) {
            if (!pm->DirtyViewPort().IsEmpty()) {
               if (d.IsEmpty() || d.Intersects(pm->DirtyViewPort())) {
                  d.Combine(pm->DirtyViewPort());
                  pm->SetClean();
                  }
               }
            }
         }
     if (!d.IsEmpty()) {
//#define DebugDirty
#ifdef DebugDirty
        static cRect OldDirty;
        cRect NewDirty = d;
        d.Combine(OldDirty);
        OldDirty = NewDirty;
#endif
        Pixmap = CreatePixmap(-1, d);
        if (Pixmap) {
           Pixmap->Clear();
           // Render the individual pixmaps into the resulting pixmap:
           for (int Layer = 0; Layer < MAXPIXMAPLAYERS; Layer++) {
               for (int i = 0; i < pixmaps.Size(); i++) {
                   if (cPixmap *pm = pixmaps[i]) {
                      if (pm->Layer() == Layer)
                         Pixmap->DrawPixmap(pm, d);
                      }
                   }
               }
#ifdef DebugDirty
           cPixmapMemory DirtyIndicator(7, NewDirty);
           static tColor DirtyIndicatorColors[] = { 0x7FFFFF00, 0x7F00FFFF };
           static int DirtyIndicatorIndex = 0;
           DirtyIndicator.Fill(DirtyIndicatorColors[DirtyIndicatorIndex]);
           DirtyIndicatorIndex = 1 - DirtyIndicatorIndex;
           Pixmap->Render(&DirtyIndicator, DirtyIndicator.DrawPort(), DirtyIndicator.ViewPort().Point().Shifted(-Pixmap->ViewPort().Point()));
#endif
           }
        }
     }
  return Pixmap;
}

eOsdError cOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
  if (NumAreas > MAXOSDAREAS)
     return oeTooManyAreas;
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
      if (Areas[i].bpp == 32) {
         if (NumAreas > 1)
            return oeTooManyAreas;
         }
      }
  return Result;
}

eOsdError cOsd::SetAreas(const tArea *Areas, int NumAreas)
{
  eOsdError Result = CanHandleAreas(Areas, NumAreas);
  if (Result == oeOk) {
     while (numBitmaps)
           delete bitmaps[--numBitmaps];
     for (int i = 0; i < pixmaps.Size(); i++) {
         delete pixmaps[i];
         pixmaps[i] = NULL;
         }
     width = height = 0;
     isTrueColor = NumAreas == 1 && Areas[0].bpp == 32;
     if (isTrueColor) {
        width = Areas[0].x2 - Areas[0].x1 + 1;
        height = Areas[0].y2 - Areas[0].y1 + 1;
        cPixmap *Pixmap = CreatePixmap(0, cRect(Areas[0].x1, Areas[0].y1, width, height));
        if (Pixmap)
           Pixmap->Clear();
        else
           Result = oeOutOfMemory;
        bitmaps[numBitmaps++] = new cBitmap(10, 10, 8); // dummy bitmap for GetBitmap()
        }
     else {
        for (int i = 0; i < NumAreas; i++) {
            bitmaps[numBitmaps++] = new cBitmap(Areas[i].Width(), Areas[i].Height(), Areas[i].bpp, Areas[i].x1, Areas[i].y1);
            width = max(width, Areas[i].x2 + 1);
            height = max(height, Areas[i].y2 + 1);
            }
        }
     }
  else
     esyslog("ERROR: cOsd::SetAreas returned %d (%s)", Result, Result < oeUnknown ? OsdErrorTexts[Result] : OsdErrorTexts[oeUnknown]);
  return Result;
}

void cOsd::SaveRegion(int x1, int y1, int x2, int y2)
{
  if (isTrueColor) {
     delete savedPixmap;
     cRect r(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
     savedPixmap = new cPixmapMemory(0, r);
     savedPixmap->Copy(pixmaps[0], r, cPoint(0, 0));
     }
  else {
     delete savedBitmap;
     savedBitmap = new cBitmap(x2 - x1 + 1, y2 - y1 + 1, 8, x1, y1);
     for (int i = 0; i < numBitmaps; i++)
         savedBitmap->DrawBitmap(bitmaps[i]->X0(), bitmaps[i]->Y0(), *bitmaps[i]);
     }
}

void cOsd::RestoreRegion(void)
{
  if (isTrueColor) {
     if (savedPixmap) {
        pixmaps[0]->Copy(savedPixmap, savedPixmap->DrawPort(), savedPixmap->ViewPort().Point());
        delete savedPixmap;
        savedPixmap = NULL;
        }
     }
  else {
     if (savedBitmap) {
        DrawBitmap(savedBitmap->X0(), savedBitmap->Y0(), *savedBitmap);
        delete savedBitmap;
        savedBitmap = NULL;
        }
     }
}

eOsdError cOsd::SetPalette(const cPalette &Palette, int Area)
{
  if (isTrueColor)
     return oeOk;
  if (Area < numBitmaps) {
     bitmaps[Area]->Take(Palette);
     return oeOk;
     }
  return oeUnknown;
}

void cOsd::DrawImage(const cPoint &Point, const cImage &Image)
{
  if (isTrueColor)
     pixmaps[0]->DrawImage(Point, Image);
}

void cOsd::DrawImage(const cPoint &Point, int ImageHandle)
{
  if (isTrueColor)
     pixmaps[0]->DrawImage(Point, ImageHandle);
}

void cOsd::DrawPixel(int x, int y, tColor Color)
{
  if (isTrueColor)
     pixmaps[0]->DrawPixel(cPoint(x, y), Color);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawPixel(x, y, Color);
     }
}

void cOsd::DrawBitmap(int x, int y, const cBitmap &Bitmap, tColor ColorFg, tColor ColorBg, bool ReplacePalette, bool Overlay)
{
  if (isTrueColor)
     pixmaps[0]->DrawBitmap(cPoint(x, y), Bitmap, ColorFg, ColorBg, Overlay);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawBitmap(x, y, Bitmap, ColorFg, ColorBg, ReplacePalette, Overlay);
     }
}

void cOsd::DrawScaledBitmap(int x, int y, const cBitmap &Bitmap, double FactorX, double FactorY, bool AntiAlias)
{
  const cBitmap *b = &Bitmap;
  if (!DoubleEqual(FactorX, 1.0) || !DoubleEqual(FactorY, 1.0))
     b = b->Scaled(FactorX, FactorY, AntiAlias);
  DrawBitmap(x, y, *b);
  if (b != &Bitmap)
     delete b;
}

void cOsd::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
  if (isTrueColor)
     pixmaps[0]->DrawText(cPoint(x, y), s, ColorFg, ColorBg, Font, Width, Height, Alignment);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawText(x, y, s, ColorFg, ColorBg, Font, Width, Height, Alignment);
     }
}

void cOsd::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
  if (isTrueColor)
     pixmaps[0]->DrawRectangle(cRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1), Color);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawRectangle(x1, y1, x2, y2, Color);
     }
}

void cOsd::DrawEllipse(int x1, int y1, int x2, int y2, tColor Color, int Quadrants)
{
  if (isTrueColor)
     pixmaps[0]->DrawEllipse(cRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1), Color, Quadrants);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawEllipse(x1, y1, x2, y2, Color, Quadrants);
     }
}

void cOsd::DrawSlope(int x1, int y1, int x2, int y2, tColor Color, int Type)
{
  if (isTrueColor)
     pixmaps[0]->DrawSlope(cRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1), Color, Type);
  else {
     for (int i = 0; i < numBitmaps; i++)
         bitmaps[i]->DrawSlope(x1, y1, x2, y2, Color, Type);
     }
}

void cOsd::Flush(void)
{
}

// --- cOsdProvider ----------------------------------------------------------

cOsdProvider *cOsdProvider::osdProvider = NULL;
int cOsdProvider::oldWidth = 0;
int cOsdProvider::oldHeight = 0;
double cOsdProvider::oldAspect = 1.0;
cImage *cOsdProvider::images[MAXOSDIMAGES] = { NULL };
int cOsdProvider::osdState = 0;

cOsdProvider::cOsdProvider(void)
{
  delete osdProvider;
  osdProvider = this;
}

cOsdProvider::~cOsdProvider()
{
  osdProvider = NULL;
}

cOsd *cOsdProvider::NewOsd(int Left, int Top, uint Level)
{
  cMutexLock MutexLock(&cOsd::mutex);
  if (Level == OSD_LEVEL_DEFAULT && cOsd::IsOpen())
     esyslog("ERROR: attempt to open OSD while it is already open - using dummy OSD!");
  else if (osdProvider) {
     cOsd *ActiveOsd = cOsd::Osds.Size() ? cOsd::Osds[0] : NULL;
     cOsd *Osd = osdProvider->CreateOsd(Left, Top, Level);
     if (Osd == cOsd::Osds[0]) {
        if (ActiveOsd)
           ActiveOsd->SetActive(false);
        Osd->SetActive(true);
        }
     return Osd;
     }
  else
     esyslog("ERROR: no OSD provider available - using dummy OSD!");
  return new cOsd(Left, Top, 999); // create a dummy cOsd, so that access won't result in a segfault
}

void cOsdProvider::UpdateOsdSize(bool Force)
{
  int Width;
  int Height;
  double Aspect;
  cMutexLock MutexLock(&cOsd::mutex);
  cDevice::PrimaryDevice()->GetOsdSize(Width, Height, Aspect);
  if (Width != oldWidth || Height != oldHeight || !DoubleEqual(Aspect, oldAspect) || Force) {
     Setup.OSDLeft = int(round(Width * Setup.OSDLeftP));
     Setup.OSDTop = int(round(Height * Setup.OSDTopP));
     Setup.OSDWidth = min(Width - Setup.OSDLeft, int(round(Width * Setup.OSDWidthP))) & ~0x07; // OSD width must be a multiple of 8
     Setup.OSDHeight = min(Height - Setup.OSDTop, int(round(Height * Setup.OSDHeightP)));
     Setup.OSDAspect = Aspect;
     Setup.FontOsdSize = int(round(Height * Setup.FontOsdSizeP));
     Setup.FontFixSize = int(round(Height * Setup.FontFixSizeP));
     Setup.FontSmlSize = int(round(Height * Setup.FontSmlSizeP));
     cFont::SetFont(fontOsd, Setup.FontOsd, Setup.FontOsdSize);
     cFont::SetFont(fontFix, Setup.FontFix, Setup.FontFixSize);
     cFont::SetFont(fontSml, Setup.FontSml, min(Setup.FontSmlSize, Setup.FontOsdSize));
     oldWidth = Width;
     oldHeight = Height;
     oldAspect = Aspect;
     dsyslog("OSD size changed to %dx%d @ %g", Width, Height, Aspect);
     osdState++;
     }
}

bool cOsdProvider::OsdSizeChanged(int &State)
{
  cMutexLock MutexLock(&cOsd::mutex);
  bool Result = osdState != State;
  State = osdState;
  return Result;
}

bool cOsdProvider::SupportsTrueColor(void)
{
  if (osdProvider) {
     return osdProvider->ProvidesTrueColor();
     }
  else
     esyslog("ERROR: no OSD provider available in call to SupportsTrueColor()");
  return false;
}

int cOsdProvider::StoreImageData(const cImage &Image)
{
  LOCK_PIXMAPS;
  for (int i = 1; i < MAXOSDIMAGES; i++) {
      if (!images[i]) {
         images[i] = new cImage(Image);
         return i;
         }
      }
  return 0;
}

void cOsdProvider::DropImageData(int ImageHandle)
{
  LOCK_PIXMAPS;
  if (0 < ImageHandle && ImageHandle < MAXOSDIMAGES) {
     delete images[ImageHandle];
     images[ImageHandle] = NULL;
     }
}

const cImage *cOsdProvider::GetImageData(int ImageHandle)
{
  LOCK_PIXMAPS;
  if (0 < ImageHandle && ImageHandle < MAXOSDIMAGES)
     return images[ImageHandle];
  return NULL;
}

int cOsdProvider::StoreImage(const cImage &Image)
{
  if (osdProvider)
     return osdProvider->StoreImageData(Image);
  return 0;
}

void cOsdProvider::DropImage(int ImageHandle)
{
  if (osdProvider)
     osdProvider->DropImageData(ImageHandle);
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
