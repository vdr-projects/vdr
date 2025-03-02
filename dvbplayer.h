/*
 * dvbplayer.h: The DVB player
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbplayer.h 5.2 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __DVBPLAYER_H
#define __DVBPLAYER_H

#include "player.h"
#include "recording.h"
#include "thread.h"

class cDvbPlayer;

class cDvbPlayerControl : public cControl {
private:
  cDvbPlayer *player;
public:
  cDvbPlayerControl(const char *FileName, bool PauseLive = false);
       // Sets up a player for the given file.
       // If PauseLive is true, special care is taken to make sure the index
       // file of the recording is long enough to allow the player to display
       // the first frame in still picture mode.
  virtual ~cDvbPlayerControl() override;
  void SetMarks(const cMarks *Marks);
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
  const cErrors *GetErrors(void);
       // Returns the frame indexes of errors in the recording (if any).
  bool GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
       // Returns the current and total frame index, optionally snapped to the
       // nearest I-frame.
  bool GetFrameNumber(int &Current, int &Total);
       // Returns the current and total frame number. In contrast to GetIndex(),
       // this function respects the chronological order of frames, which is
       // different from its index for streams containing B frames (e.g. H264)
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
       // Returns the current replay mode (if applicable).
       // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
       // we are going forward or backward and 'Speed' is -1 if this is normal
       // play/pause mode, 0 if it is single speed fast/slow forward/back mode
       // and >0 if this is multi speed mode.
  void Goto(int Index, bool Still = false);
       // Positions to the given index and displays that frame as a still picture
       // if Still is true. If Still is false, Play() will be called.
  };

#endif //__DVBPLAYER_H
