/*
 * dvbosd.c: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.c 1.21 2003/04/12 12:10:12 kls Exp $
 */

#include "dvbosd.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include "tools.h"

const cDvbDevice *cDvbOsd::dvbDevice = NULL;

cDvbOsd::cDvbOsd(int x, int y)
:cOsdBase(x, y)
{
  osdDev = dvbDevice ? dvbDevice->OsdDeviceHandle() : -1;
  if (dvbDevice && osdDev < 0)
     esyslog("ERROR: illegal OSD device handle (%d)!", osdDev);
}

cDvbOsd::~cDvbOsd()
{
  for (int i = 0; i < NumWindows(); i++)
      CloseWindow(GetWindowNr(i));
}

void cDvbOsd::SetDvbDevice(const cDvbDevice *DvbDevice)
{
  dvbDevice = DvbDevice;
}

bool cDvbOsd::SetWindow(cWindow *Window)
{
  if (Window) {
     // Window handles are counted 0...(MAXNUMWINDOWS - 1), but the actual window
     // numbers in the driver are used from 1...MAXNUMWINDOWS.
     int Handle = Window->Handle();
     if (0 <= Handle && Handle < MAXNUMWINDOWS) {
        Cmd(OSD_SetWindow, 0, Handle + 1);
        return true;
        }
     esyslog("ERROR: illegal window handle: %d", Handle);
     }
  return false;
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

bool cDvbOsd::OpenWindow(cWindow *Window)
{
  if (SetWindow(Window)) {
     Cmd(OSD_Open, Window->Bpp(), X0() + Window->X0(), Y0() + Window->Y0(), X0() + Window->X0() + Window->Width() - 1, Y0() + Window->Y0() + Window->Height() - 1, (void *)1); // initially hidden!
     return true;
     }
  return false;
}

void cDvbOsd::CommitWindow(cWindow *Window)
{
  if (SetWindow(Window)) {
     int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
     if (Window->Dirty(x1, y1, x2, y2)) {
        // commit colors:
        int FirstColor = 0, LastColor = 0;
        const eDvbColor *pal;
        while ((pal = Window->NewColors(FirstColor, LastColor)) != NULL)
              Cmd(OSD_SetPalette, FirstColor, LastColor, 0, 0, 0, pal);
        // commit modified data:
        Cmd(OSD_SetBlock, Window->Width(), x1, y1, x2, y2, Window->Data(x1, y1));
        }
     }
}

void cDvbOsd::ShowWindow(cWindow *Window)
{
  if (SetWindow(Window))
     Cmd(OSD_MoveWindow, 0, X0() + Window->X0(), Y0() + Window->Y0());
}

void cDvbOsd::HideWindow(cWindow *Window, bool Hide)
{
  if (SetWindow(Window))
     Cmd(Hide ? OSD_Hide : OSD_Show, 0);
}

void cDvbOsd::MoveWindow(cWindow *Window, int x, int y)
{
  if (SetWindow(Window))
     Cmd(OSD_MoveWindow, 0, X0() + x, Y0() + y);
}

void cDvbOsd::CloseWindow(cWindow *Window)
{
  if (SetWindow(Window))
     Cmd(OSD_Close);
}
