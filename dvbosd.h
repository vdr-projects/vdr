/*
 * dvbosd.h: Implementation of the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbosd.h 1.12 2002/05/18 12:39:31 kls Exp $
 */

#ifndef __DVBOSD_H
#define __DVBOSD_H

#include <ost/osd.h>
#include "osdbase.h"

class cDvbOsd : public cOsdBase {
private:
  int videoDev;
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
  cDvbOsd(int VideoDev, int x, int y);
  virtual ~cDvbOsd();
  };

#endif //__DVBOSD_H
