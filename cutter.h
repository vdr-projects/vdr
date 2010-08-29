/*
 * cutter.h: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.h 2.2 2010/08/29 13:32:33 kls Exp $
 */

#ifndef __CUTTER_H
#define __CUTTER_H

#include "thread.h"

class cCuttingThread;

class cCutter {
private:
  static cMutex mutex;
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

bool CutRecording(const char *FileName);

#endif //__CUTTER_H
