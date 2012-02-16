/*
 * cutter.h: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.h 2.3 2012/02/16 12:05:33 kls Exp $
 */

#ifndef __CUTTER_H
#define __CUTTER_H

#include "thread.h"
#include "tools.h"

class cCuttingThread;

class cCutter {
private:
  static cMutex mutex;
  static cString originalVersionName;
  static cString editedVersionName;
  static cCuttingThread *cuttingThread;
  static bool error;
  static bool ended;
public:
  static bool Start(const char *FileName);
  static void Stop(void);
  static bool Active(const char *FileName = NULL);
         ///< Returns true if the cutter is currently active.
         ///< If a FileName is given, true is only returned if either the
         ///< original or the edited file name is equal to FileName.
  static bool Error(void);
  static bool Ended(void);
  };

bool CutRecording(const char *FileName);

#endif //__CUTTER_H
