/*
 * cutter.h: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.h 1.1 2002/06/22 10:03:15 kls Exp $
 */

#ifndef __CUTTER_H
#define __CUTTER_H

class cCuttingThread;

class cCutter {
private:
  static char *editedVersionName;
  static cCuttingThread *cuttingThread;
  static bool error;
  static bool ended;
public:
  static bool Start(const char *FileName);
  static void Stop(void);
  static bool Active(void);
  static bool Error(void);
  static bool Ended(void);
  };

#endif //__CUTTER_H
