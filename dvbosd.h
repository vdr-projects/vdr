/*
 * dvbosd.h: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.16 2002/11/01 10:05:47 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#include <linux/dvb/osd.h>
#include "dvbdevice.h"
#include "osdbase.h"

class cDvbOsd : public cOsdBase {
private:
  static const cDvbDevice *dvbDevice;
  int osdDev;
  bool SetWindow(cWindow *Window);
  void Cmd(OSD_Command cmd, int color = 0, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0, const void *data = NULL);
protected:
  virtual bool OpenWindow(cWindow *Window);
  virtual void CommitWindow(cWindow *Window);
  virtual void ShowWindow(cWindow *Window);
  virtual void HideWindow(cWindow *Window, bool Hide);
  virtual void MoveWindow(cWindow *Window, int x, int y);
  virtual void CloseWindow(cWindow *Window);
public:
  cDvbOsd(int x, int y);
  virtual ~cDvbOsd();
  static void SetDvbDevice(const cDvbDevice *DvbDevice);
  };

#endif //__DVBOSD_H
