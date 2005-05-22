/*
 * dvbosd.c: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.c 1.27 2005/05/22 10:57:45 kls Exp $
 */

#include "dvbosd.h"
#include <linux/dvb/osd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include "dvbdevice.h"
#include "tools.h"

// --- cDvbOsd ---------------------------------------------------------------

#define MAXNUMWINDOWS 7 // OSD windows are counted 1...7
#define MAXOSDMEMORY  92000 // number of bytes available to the OSD (for unmodified DVB cards)

class cDvbOsd : public cOsd {
private:
  int osdDev;
  int osdMem;
  bool shown;
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
public:
  cDvbOsd(int Left, int Top, int OsdDev);
  virtual ~cDvbOsd();
  virtual eOsdError CanHandleAreas(const tArea *Areas, int NumAreas);
  virtual void Flush(void);
  };

cDvbOsd::cDvbOsd(int Left, int Top, int OsdDev)
:cOsd(Left, Top)
{
  osdDev = OsdDev;
  shown = false;
  if (osdDev < 0)
     esyslog("ERROR: illegal OSD device handle (%d)!", osdDev);
  else {
     osdMem = MAXOSDMEMORY;
#ifdef OSD_CAP_MEMSIZE
     // modified DVB cards may have more OSD memory:
     osd_cap_t cap;
     cap.cmd = OSD_CAP_MEMSIZE;
     if (ioctl(osdDev, OSD_GET_CAPABILITY, &cap) == 0)
        osdMem = cap.val;
     #define FW_NEEDS_BUFFER_RESERVE_FOR_AC3
     #ifdef FW_NEEDS_BUFFER_RESERVE_FOR_AC3
     //XXX This is a very ugly hack to allow cDvbOsd to reduce the buffer
     //XXX requirements in cTransfer if it detects a 4MB full featured DVB card.
     extern bool DvbCardWith4MBofSDRAM;
     DvbCardWith4MBofSDRAM = osdMem >= 1000000;
     #endif
#endif
     // must clear all windows here to avoid flashing effects - doesn't work if done
     // in Flush() only for the windows that are actually used...
     for (int i = 0; i < MAXNUMWINDOWS; i++) {
         Cmd(OSD_SetWindow, 0, i + 1);
         Cmd(OSD_Clear);
         }
     }
}

cDvbOsd::~cDvbOsd()
{
  if (shown) {
     cBitmap *Bitmap;
     for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
         Cmd(OSD_SetWindow, 0, i + 1);
         Cmd(OSD_Close);
         }
     }
}

eOsdError cDvbOsd::CanHandleAreas(const tArea *Areas, int NumAreas)
{
  eOsdError Result = cOsd::CanHandleAreas(Areas, NumAreas);
  if (Result == oeOk) {
     if (NumAreas > MAXNUMWINDOWS)
        return oeTooManyAreas;
     int TotalMemory = 0;
     for (int i = 0; i < NumAreas; i++) {
         if (Areas[i].bpp != 1 && Areas[i].bpp != 2 && Areas[i].bpp != 4 && Areas[i].bpp != 8)
            return oeBppNotSupported;
         if ((Areas[i].Width() & (8 / Areas[i].bpp - 1)) != 0)
            return oeWrongAlignment;
         TotalMemory += Areas[i].Width() * Areas[i].Height() / (8 / Areas[i].bpp);
         }
     if (TotalMemory > osdMem)
        return oeOutOfMemory;
     }
  return Result;
}

void cDvbOsd::Cmd(OSD_Command cmd, int color, int x0, int y0, int x1, int y1, const void *data)
{
  if (osdDev >= 0) {
     osd_cmd_t dc;
     dc.cmd   = cmd;
     dc.color = color;
     dc.x0    = x0;
     dc.y0    = y0;
     dc.x1    = x1;
     dc.y1    = y1;
     dc.data  = (void *)data;
     ioctl(osdDev, OSD_SEND_CMD, &dc);
     }
}

void cDvbOsd::Flush(void)
{
  cBitmap *Bitmap;
  for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
      Cmd(OSD_SetWindow, 0, i + 1);
      if (!shown)
         Cmd(OSD_Open, Bitmap->Bpp(), Left() + Bitmap->X0(), Top() + Bitmap->Y0(), Left() + Bitmap->X0() + Bitmap->Width() - 1, Top() + Bitmap->Y0() + Bitmap->Height() - 1, (void *)1); // initially hidden!
      int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
      if (Bitmap->Dirty(x1, y1, x2, y2)) {
         //TODO Workaround: apparently the bitmap sent to the driver always has to be a multiple
         //TODO of 8 bits wide, and (dx * dy) also has to be a multiple of 8.
         //TODO Fix driver (should be able to handle any size bitmaps!)
         while ((x1 > 0 || x2 < Bitmap->Width() - 1) && ((x2 - x1) & 7) != 7) {
               if (x2 < Bitmap->Width() - 1)
                  x2++;
               else if (x1 > 0)
                  x1--;
               }
         //TODO "... / 2" <==> Bpp???
         while ((y1 > 0 || y2 < Bitmap->Height() - 1) && (((x2 - x1 + 1) * (y2 - y1 + 1) / 2) & 7) != 0) {
               if (y2 < Bitmap->Height() - 1)
                  y2++;
               else if (y1 > 0)
                  y1--;
               }
         while ((x1 > 0 || x2 < Bitmap->Width() - 1) && (((x2 - x1 + 1) * (y2 - y1 + 1) / 2) & 7) != 0) {
               if (x2 < Bitmap->Width() - 1)
                  x2++;
               else if (x1 > 0)
                  x1--;
               }
         // commit colors:
         int NumColors;
         const tColor *Colors = Bitmap->Colors(NumColors);
         if (Colors) {
            //TODO this should be fixed in the driver!
            tColor colors[NumColors];
            for (int i = 0; i < NumColors; i++) {
                // convert AARRGGBB to AABBGGRR (the driver expects the colors the wrong way):
                colors[i] = (Colors[i] & 0xFF000000) | ((Colors[i] & 0x0000FF) << 16) | (Colors[i] & 0x00FF00) | ((Colors[i] & 0xFF0000) >> 16);
                }
            Colors = colors;
            //TODO end of stuff that should be fixed in the driver
            Cmd(OSD_SetPalette, 0, NumColors - 1, 0, 0, 0, Colors);
            }
         // commit modified data:
         Cmd(OSD_SetBlock, Bitmap->Width(), x1, y1, x2, y2, Bitmap->Data(x1, y1));
         }
      Bitmap->Clean();
      }
  if (!shown) {
     // Showing the windows in a separate loop to avoid seeing them come up one after another
     for (int i = 0; (Bitmap = GetBitmap(i)) != NULL; i++) {
         Cmd(OSD_SetWindow, 0, i + 1);
         Cmd(OSD_MoveWindow, 0, Left() + Bitmap->X0(), Top() + Bitmap->Y0());
         }
     shown = true;
     }
}

// --- cDvbOsdProvider -------------------------------------------------------

cDvbOsdProvider::cDvbOsdProvider(int OsdDev)
{
  osdDev = OsdDev;
}

cOsd *cDvbOsdProvider::CreateOsd(int Left, int Top)
{
  return new cDvbOsd(Left, Top, osdDev);
}
