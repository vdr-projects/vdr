/*
 * dvbosd.c: Interface to the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.c 1.3 2000/10/07 14:42:48 kls Exp $
 */

#include "dvbosd.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "tools.h"

// --- cBitmap ---------------------------------------------------------------

cBitmap::cBitmap(int Width, int Height)
{
  width = Width;
  height = Height;
  bitmap = NULL;
  font = NULL;
  if (width > 0 && height > 0) {
     bitmap = new char[width * height];
     if (bitmap) {
        Clean();
        memset(bitmap, clrTransparent, width * height);
        SetFont(fontOsd);
        }
     else
        esyslog(LOG_ERR, "ERROR: can't allocate bitmap!");
     }
  else
     esyslog(LOG_ERR, "ERROR: illegal bitmap parameters (%d, %d)!", width, height);
}

cBitmap::~cBitmap()
{
  delete font;
  delete bitmap;
}

void cBitmap::SetFont(eDvbFont Font)
{
  delete font;
  font = new cFont(Font);
}

bool cBitmap::Dirty(void)
{
  return dirtyX2 >= 0;
}

void cBitmap::Clean(void)
{
  dirtyX1 = width;
  dirtyY1 = height;
  dirtyX2 = -1;
  dirtyY2 = -1;
}

void cBitmap::SetPixel(int x, int y, eDvbColor Color)
{
  if (bitmap) {
     if (0 <= x && x < width && 0 <= y && y < height) {
        if (bitmap[width * y + x] != Color) {
           bitmap[width * y + x] = Color;
           if (dirtyX1 > x)  dirtyX1 = x;
           if (dirtyY1 > y)  dirtyY1 = y;
           if (dirtyX2 < x)  dirtyX2 = x;
           if (dirtyY2 < y)  dirtyY2 = y;
           }
        }
     }
}

void cBitmap::Text(int x, int y, const char *s, eDvbColor ColorFg, eDvbColor ColorBg)
{
  if (bitmap) {
     int h = font->Height(s);
     while (s && *s) {
           const cFont::tCharData *CharData = font->CharData(*s++);
           if (int(x + CharData->width) > width)
              break;
           for (int row = 0; row < h; row++) {
               cFont::tPixelData PixelData = CharData->lines[row];
               for (int col = CharData->width; col-- > 0; ) {
                   SetPixel(x + col, y + row, (PixelData & 1) ? ColorFg : ColorBg);
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
     for (int y = y1; y <= y2; y++)
         for (int x = x1; x <= x2; x++)
             SetPixel(x, y, Color);
     }
}

void cBitmap::Clear(void)
{
  Fill(0, 0, width - 1, height - 1, clrBackground);
}

// --- cDvbOsd ---------------------------------------------------------------

cDvbOsd::cDvbOsd(int VideoDev, int x1, int y1, int x2, int y2, int Bpp)
:cBitmap(x2 - x1 + 1, y2 - y1 + 1)
{
  videoDev = VideoDev;
  if (videoDev >= 0)
     Cmd(OSD_Open, Bpp, x1, y1, x2, y2);
  else
     esyslog(LOG_ERR, "ERROR: illegal video device handle (%d)!", videoDev);
}

cDvbOsd::~cDvbOsd()
{
  if (videoDev >= 0)
     Cmd(OSD_Close);
}

void cDvbOsd::Cmd(OSD_Command cmd, int color, int x0, int y0, int x1, int y1, const void *data)
{
  if (videoDev >= 0) {
     struct drawcmd dc;
     dc.cmd   = cmd;
     dc.color = color;
     dc.x0    = x0;
     dc.y0    = y0;
     dc.x1    = x1;
     dc.y1    = y1;
     dc.data  = (void *)data;
     // must block all signals, otherwise the command might not be fully executed
     sigset_t set, oldset;
     sigfillset(&set);
     sigprocmask(SIG_BLOCK, &set, &oldset);
     ioctl(videoDev, VIDIOCSOSDCOMMAND, &dc);
     usleep(10); // XXX Workaround for a driver bug (cInterface::DisplayChannel() displayed texts at wrong places
                 // XXX and sometimes the OSD was no longer displayed).
                 // XXX Increase the value if the problem still persists on your particular system.
                 // TODO Check if this is still necessary with driver versions after 0.7.
     sigprocmask(SIG_SETMASK, &oldset, NULL);
     }
}

void cDvbOsd::Flush(void)
{
  if (Dirty()) {
     //XXX Workaround: apparently the bitmap sent to the driver always has to be a multiple
     //XXX of 8 bits wide, and (dx * dy) also has to be a multiple of 8.
     //TODO Fix driver (should be able to handle any size bitmaps!)
     while ((dirtyX1 > 0 || dirtyX2 < width - 1) && ((dirtyX2 - dirtyX1) & 7) != 7) {
           if (dirtyX2 < width - 1)
              dirtyX2++;
           else if (dirtyX1 > 0)
              dirtyX1--;
           }
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
     Cmd(OSD_SetBlock, width, dirtyX1, dirtyY1, dirtyX2, dirtyY2, &bitmap[dirtyY1 * width + dirtyX1]);
     Clean();
     }
}

