/*
 * osdbase.c: Basic interface to the On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: osdbase.c 1.10 2003/08/24 11:38:27 kls Exp $
 */

#include "osdbase.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "tools.h"

// --- cPalette --------------------------------------------------------------

cPalette::cPalette(int Bpp)
{
  maxColors = 1 << Bpp;
  numColors = 0;
  full = false;
}

void cPalette::SetColor(int Index, eDvbColor Color)
{
  if (Index < maxColors) {
     if (numColors < Index)
        numColors = Index + 1;
     used[Index] = true;
     color[Index] = Color;
     fetched[Index] = false;
     }
}

int cPalette::Index(eDvbColor Color)
{
#if __BYTE_ORDER == __BIG_ENDIAN
  Color = eDvbColor(((Color & 0xFF) << 24) | ((Color & 0xFF00) << 8) | ((Color & 0xFF0000) >> 8) | ((Color & 0xFF000000) >> 24));
#endif
  for (int i = 0; i < numColors; i++) {
      if (color[i] == Color) {
         used[i] = true;
         return i;
         }
      }
  if (!full) {
     if (numColors < maxColors) {
        color[numColors++] = Color;
        used[numColors - 1] = true;
        fetched[numColors - 1] = false;
        return numColors - 1;
        }
     for (int i = maxColors; --i >= 0; ) {
         if (!used[i]) {
            color[i] = Color;
            used[i] = true;
            fetched[i] = false;
            return i;
            }
         }
     esyslog("ERROR: too many different colors used in palette");
     full = true;
     }
  return 0;
}

void cPalette::Reset(void)
{
  for (int i = 0; i < numColors; i++)
      used[i] = fetched[i] = false;
  full = false;
}

const eDvbColor *cPalette::NewColors(int &FirstColor, int &LastColor)
{
  for (FirstColor = 0; FirstColor < numColors; FirstColor++) {
      if (!fetched[FirstColor]) {
         for (LastColor = FirstColor; LastColor < numColors && !fetched[LastColor]; LastColor++)
             fetched[LastColor] = true;
         LastColor--; // the loop ended one past the last one!
         return &color[FirstColor];
         }
      }
  return NULL;
}

const eDvbColor *cPalette::AllColors(int &NumColors)
{
  NumColors = numColors;
  return numColors ? color : NULL;
}

void cPalette::Take(const cPalette &Palette, tIndexes *Indexes)
{
  for (int i = 0; i < Palette.numColors; i++) {
      if (Palette.used[i]) {
         int n = Index(Palette.color[i]);
         if (Indexes)
            (*Indexes)[i] = n;
         }
      }
}

// --- cBitmap ---------------------------------------------------------------

cBitmap::cBitmap(int Width, int Height, int Bpp, bool ClearWithBackground)
:cPalette(Bpp)
{
  width = Width;
  height = Height;
  clearWithBackground = ClearWithBackground;
  bitmap = NULL;
  fontType = fontOsd;
  font = NULL;
  if (width > 0 && height > 0) {
     bitmap = MALLOC(char, width * height);
     if (bitmap) {
        Clean();
        memset(bitmap, 0x00, width * height);
        SetFont(fontOsd);
        }
     else
        esyslog("ERROR: can't allocate bitmap!");
     }
  else
     esyslog("ERROR: illegal bitmap parameters (%d, %d)!", width, height);
}

cBitmap::~cBitmap()
{
  delete font;
  free(bitmap);
}

eDvbFont cBitmap::SetFont(eDvbFont Font)
{
  eDvbFont oldFont = fontType;
  if (fontType != Font || !font) {
     delete font;
     font = new cFont(Font);
     fontType = Font;
     }
  return oldFont;
}

bool cBitmap::Dirty(int &x1, int &y1, int &x2, int &y2)
{
  if (dirtyX2 >= 0) {
     //XXX Workaround: apparently the bitmap sent to the driver always has to be a multiple
     //XXX of 8 bits wide, and (dx * dy) also has to be a multiple of 8.
     //TODO Fix driver (should be able to handle any size bitmaps!)
     while ((dirtyX1 > 0 || dirtyX2 < width - 1) && ((dirtyX2 - dirtyX1) & 7) != 7) {
           if (dirtyX2 < width - 1)
              dirtyX2++;
           else if (dirtyX1 > 0)
              dirtyX1--;
           }
     //XXX "... / 2" <==> Bpp???
     while ((dirtyY1 > 0 || dirtyY2 < height - 1) && (((dirtyX2 - dirtyX1 + 1) * (dirtyY2 - dirtyY1 + 1) / 2) & 7) != 0) {
           if (dirtyY2 < height - 1)
              dirtyY2++;
           else if (dirtyY1 > 0)
              dirtyY1--;
           }
     while ((dirtyX1 > 0 || dirtyX2 < width - 1) && (((dirtyX2 - dirtyX1 + 1) * (dirtyY2 - dirtyY1 + 1) / 2) & 7) != 0) {
           if (dirtyX2 < width - 1)
              dirtyX2++;
           else if (dirtyX1 > 0)
              dirtyX1--;
           }
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

void cBitmap::SetIndex(int x, int y, char Index)
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

void cBitmap::SetPixel(int x, int y, eDvbColor Color)
{
  SetIndex(x, y, Index(Color));
}

void cBitmap::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
  if (bitmap && Bitmap.bitmap) {
     tIndexes Indexes;
     Take(Bitmap, &Indexes);
     for (int ix = 0; ix < Bitmap.width; ix++) {
         for (int iy = 0; iy < Bitmap.height; iy++)
             SetIndex(x + ix, y + iy, Indexes[int(Bitmap.bitmap[Bitmap.width * iy + ix])]);
         }
     }
}

int cBitmap::Width(unsigned char c)
{
  return font ? font->Width(c) : -1;
}

int cBitmap::Width(const char *s)
{
  return font ? font->Width(s) : -1;
}

void cBitmap::Text(int x, int y, const char *s, eDvbColor ColorFg, eDvbColor ColorBg)
{
  if (bitmap) {
     char fg = Index(ColorFg);
     char bg = Index(ColorBg);
     int h = font->Height(s);
     while (s && *s) {
           const cFont::tCharData *CharData = font->CharData(*s++);
           if (int(x + CharData->width) > width)
              break;
           for (int row = 0; row < h; row++) {
               cFont::tPixelData PixelData = CharData->lines[row];
               for (int col = CharData->width; col-- > 0; ) {
                   SetIndex(x + col, y + row, (PixelData & 1) ? fg : bg);
                   PixelData >>= 1;
                   }
               }
           x += CharData->width;
           }
     }
}

void cBitmap::Fill(int x1, int y1, int x2, int y2, eDvbColor Color)
{
  if (bitmap) {
     char c = Index(Color);
     for (int y = y1; y <= y2; y++)
         for (int x = x1; x <= x2; x++)
             SetIndex(x, y, c);
     }
}

void cBitmap::Clear(void)
{
  Reset();
  if (clearWithBackground)
     Fill(0, 0, width - 1, height - 1, clrBackground);
}

const char *cBitmap::Data(int x, int y)
{
  return &bitmap[y * width + x];
}

// --- cWindow ---------------------------------------------------------------

cWindow::cWindow(int Handle, int x, int y, int w, int h, int Bpp, bool ClearWithBackground, bool Tiled)
:cBitmap(w, h, Bpp, ClearWithBackground)
{
  handle = Handle;
  x0 = x;
  y0 = y;
  bpp = Bpp;
  tiled = Tiled;
  shown = false;
}

bool cWindow::Contains(int x, int y)
{
  x -= x0;
  y -= y0;
  return x >= 0 && y >= 0 && x < width && y < height;
}

void cWindow::Relocate(int x, int y)
{
  x0 = x;
  y0 = y;
}

void cWindow::Fill(int x1, int y1, int x2, int y2, eDvbColor Color)
{
  if (tiled) {
     x1 -= x0;
     y1 -= y0;
     x2 -= x0;
     y2 -= y0;
     }
  cBitmap::Fill(x1, y1, x2, y2, Color);
}

void cWindow::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
  if (tiled) {
     x -= x0;
     y -= y0;
     }
  cBitmap::SetBitmap(x, y, Bitmap);
}

void cWindow::Text(int x, int y, const char *s, eDvbColor ColorFg, eDvbColor ColorBg)
{
  if (tiled) {
     x -= x0;
     y -= y0;
     }
  cBitmap::Text(x, y, s, ColorFg, ColorBg);
}

const char *cWindow::Data(int x, int y)
{
  return cBitmap::Data(x, y);
}

// --- cOsdBase --------------------------------------------------------------

cOsdBase::cOsdBase(int x, int y)
{
  numWindows = 0;
  x0 = x;
  y0 = y;
}

cOsdBase::~cOsdBase()
{
  for (int i = 0; i < numWindows; i++)
      delete window[i];
}

tWindowHandle cOsdBase::Create(int x, int y, int w, int h, int Bpp, bool ClearWithBackground, bool Tiled)
{
  if (numWindows < MAXNUMWINDOWS) {
     if (x >= 0 && y >= 0 && w > 0 && h > 0 && (Bpp == 1 || Bpp == 2 || Bpp == 4 || Bpp == 8)) {
        if ((w & 0x03) != 0) {
           w += 4 - (w & 0x03);
           dsyslog("OSD window width must be a multiple of 4 - increasing to %d", w);
           }
        cWindow *win = new cWindow(numWindows, x, y, w, h, Bpp, ClearWithBackground, Tiled);
        if (OpenWindow(win)) {
           window[win->Handle()] = win;
           numWindows++;
           return win->Handle();
           }
        else
           delete win;
        }
     else
        esyslog("ERROR: illegal OSD parameters");
     }
  else
     esyslog("ERROR: too many OSD windows");
  return -1;
}

void cOsdBase::AddColor(eDvbColor Color, tWindowHandle Window)
{
  cWindow *w = GetWindow(Window);
  if (w) {
     w->Index(Color);
     w->Reset();
     }
}

cWindow *cOsdBase::GetWindow(int x, int y)
{
  for (int i = 0; i < numWindows; i++) {
      if (window[i]->Tiled() && window[i]->Contains(x, y))
         return window[i];
      }
  return NULL;
}

cWindow *cOsdBase::GetWindow(tWindowHandle Window)
{
  if (0 <= Window && Window < numWindows)
     return window[Window];
  if (Window == LAST_CREATED_WINDOW && numWindows > 0)
     return window[numWindows - 1];
  return NULL;
}

void cOsdBase::Flush(void)
{
  for (int i = 0; i < numWindows; i++) {
      CommitWindow(window[i]);
      window[i]->Clean();
      }
  // Showing the windows in a separate loop to avoid seeing them come up one after another
  for (int i = 0; i < numWindows; i++) {
      if (!window[i]->Shown())
         ShowWindow(window[i]);
      }
}

void cOsdBase::Clear(tWindowHandle Window)
{
  if (Window == ALL_TILED_WINDOWS || Window == ALL_WINDOWS) {
     for (int i = 0; i < numWindows; i++)
         if (Window == ALL_WINDOWS || window[i]->Tiled())
            window[i]->Clear();
     }
  else {
     cWindow *w = GetWindow(Window);
     if (w)
        w->Clear();
     }
}

void cOsdBase::Fill(int x1, int y1, int x2, int y2, eDvbColor Color, tWindowHandle Window)
{
  cWindow *w = (Window == ALL_TILED_WINDOWS) ? GetWindow(x1, y1) : GetWindow(Window);
  if (w)
     w->Fill(x1, y1, x2, y2, Color);
}

void cOsdBase::SetBitmap(int x, int y, const cBitmap &Bitmap, tWindowHandle Window)
{
  cWindow *w = (Window == ALL_TILED_WINDOWS) ? GetWindow(x, y) : GetWindow(Window);
  if (w)
     w->SetBitmap(x, y, Bitmap);
}

int cOsdBase::Width(unsigned char c)
{
  return numWindows ? window[0]->Width(c) : 0;
}

int cOsdBase::Width(const char *s)
{
  return numWindows ? window[0]->Width(s) : 0;
}

eDvbFont cOsdBase::SetFont(eDvbFont Font)
{
  eDvbFont oldFont = Font;
  for (int i = 0; i < numWindows; i++)
      oldFont = window[i]->SetFont(Font);
  return oldFont;
}

void cOsdBase::Text(int x, int y, const char *s, eDvbColor ColorFg, eDvbColor ColorBg, tWindowHandle Window)
{
  cWindow *w = (Window == ALL_TILED_WINDOWS) ? GetWindow(x, y) : GetWindow(Window);
  if (w)
     w->Text(x, y, s, ColorFg, ColorBg);
}

void cOsdBase::Relocate(tWindowHandle Window, int x, int y, int NewWidth, int NewHeight)
{
  cWindow *w = GetWindow(Window);
  if (w) {
     if (NewWidth > 0 && NewHeight > 0) {
        if ((NewWidth & 0x03) != 0) {
           NewWidth += 4 - (NewWidth & 0x03);
           dsyslog("OSD window width must be a multiple of 4 - increasing to %d", NewWidth);
           }
        CloseWindow(w);
        cWindow *NewWindow = new cWindow(w->Handle(), x, y, NewWidth, NewHeight, w->Bpp(), w->ClearWithBackground(), w->Tiled());
        window[w->Handle()] = NewWindow;
        delete w;
        OpenWindow(NewWindow);
        }
     else {
        MoveWindow(w, x, y);
        w->Relocate(x, y);
        }
     }
}

void cOsdBase::Hide(tWindowHandle Window)
{
  HideWindow(GetWindow(Window), true);
}

void cOsdBase::Show(tWindowHandle Window)
{
  HideWindow(GetWindow(Window), false);
}
