/*
 * dvbplayer.h: The DVB player
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbplayer.h 2.0 2002/06/23 10:13:51 kls Exp $
 */

#ifndef __DVBPLAYER_H
#define __DVBPLAYER_H

#include "player.h"
#include "thread.h"

class cDvbPlayer;

class cDvbPlayerControl : public cControl {
private:
  cDvbPlayer *player;
public:
  cDvbPlayerControl(const char *FileName);
       // Sets up a player for the given file.
  virtual ~cDvbPlayerControl();
  bool Active(void);
  void Stop(void);
       // Stops the current replay session (if any).
  void Pause(void);
       // Pauses the current replay session, or resumes a paused session.
  void Play(void);
       // Resumes normal replay mode.
  void Forward(void);
       // Runs the current replay session forward at a higher speed.
  void Backward(void);
       // Runs the current replay session backwards at a higher speed.
  int  SkipFrames(int Frames);
       // Returns the new index into the current replay session after skipping
       // the given number of frames (no actual repositioning is done!).
       // The sign of 'Frames' determines the direction in which to skip.
  void SkipSeconds(int Seconds);
       // Skips the given number of seconds in the current replay session.
       // The sign of 'Seconds' determines the direction in which to skip.
       // Use a very large negative value to go all the way back to the
       // beginning of the recording.
  bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
       // Returns the current and total frame index, optionally snapped to the
       // nearest I-frame.
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
       // Returns the current replay mode (if applicable).
       // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
       // we are going forward or backward and 'Speed' is -1 if this is normal
       // play/pause mode, 0 if it is single speed fast/slow forward/back mode
       // and >0 if this is multi speed mode.
  void Goto(int Index, bool Still = false);
       // Positions to the given index and displays that frame as a still picture
       // if Still is true.
  };

#endif //__DVBPLAYER_H
