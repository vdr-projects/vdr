/*
 * cutter.h: The video cutting facilities
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: cutter.h 5.1 2024/09/19 20:21:58 kls Exp $
 */

#ifndef __CUTTER_H
#define __CUTTER_H

#include "recording.h"
#include "thread.h"
#include "tools.h"

class cCuttingThread;

class cCutter {
private:
  cString originalVersionName;
  cString editedVersionName;
  cRecordingInfo recordingInfo;
  cCuttingThread *cuttingThread;
  bool error;
public:
  cCutter(const char *FileName);
      ///< Sets up a new cutter for the given FileName, which must be the full path
      ///< name of an existing recording directory.
  ~cCutter();
  static cString EditedFileName(const char *FileName);
      ///< Returns the full path name of the edited version of the recording with
      ///< the given FileName. This static function can be used independent of any
      ///< cCutter object, to determine the file name beforehand.
      ///< Returns NULL in case of error.
  bool Start(void);
      ///< Starts the actual cutting process.
      ///< Returns true if successful.
      ///< If Start() is called while the cutting process is already active, nothing
      ///< happens and false will be returned.
  void Stop(void);
      ///< Stops an ongoing cutting process.
  bool Active(void);
      ///< Returns true if the cutter is currently active.
  bool Error(void);
      ///< Returns true if an error occurred while cutting the recording.
  };

bool CutRecording(const char *FileName);

#endif //__CUTTER_H
