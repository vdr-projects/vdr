/*
 * dvbplayer.c: The DVB player
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbplayer.c 1.1 2002/06/16 10:59:45 kls Exp $
 */

#include "dvbplayer.h"
#include <poll.h>
#include "recording.h"
#include "ringbuffer.h"
#include "thread.h"

// --- ReadFrame -------------------------------------------------------------

int ReadFrame(int f, uchar *b, int Length, int Max)
{
  if (Length == -1)
     Length = Max; // this means we read up to EOF (see cIndex)
  else if (Length > Max) {
     esyslog("ERROR: frame larger than buffer (%d > %d)", Length, Max);
     Length = Max;
     }
  int r = safe_read(f, b, Length);
  if (r < 0)
     LOG_ERROR;
  return r;
}

// --- cBackTrace ----------------------------------------------------------

#define AVG_FRAME_SIZE 15000         // an assumption about the average frame size
#define DVB_BUF_SIZE   (256 * 1024)  // an assumption about the dvb firmware buffer size
#define BACKTRACE_ENTRIES (DVB_BUF_SIZE / AVG_FRAME_SIZE + 20) // how many entries are needed to backtrace buffer contents

class cBackTrace {
private:
  int index[BACKTRACE_ENTRIES];
  int length[BACKTRACE_ENTRIES];
  int pos, num;
public:
  cBackTrace(void);
  void Clear(void);
  void Add(int Index, int Length);
  int Get(bool Forward);
  };

cBackTrace::cBackTrace(void)
{
  Clear();
}

void cBackTrace::Clear(void)
{
  pos = num = 0;
}

void cBackTrace::Add(int Index, int Length)
{
  index[pos] = Index;
  length[pos] = Length;
  if (++pos >= BACKTRACE_ENTRIES)
     pos = 0;
  if (num < BACKTRACE_ENTRIES)
     num++;
}

int cBackTrace::Get(bool Forward)
{
  int p = pos;
  int n = num;
  int l = DVB_BUF_SIZE + (Forward ? 0 : 256 * 1024); //XXX (256 * 1024) == DVB_BUF_SIZE ???
  int i = -1;

  while (n && l > 0) {
        if (--p < 0)
           p = BACKTRACE_ENTRIES - 1;
        i = index[p] - 1;
        l -= length[p];
        n--; 
        }
  return i;
}

// --- cDvbPlayer ------------------------------------------------------------

//XXX+ also used in recorder.c - find a better place???
// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(1)

// The maximum size of a single frame:
#define MAXFRAMESIZE  KILOBYTE(192)

// The number of frames to back up when resuming an interrupted replay session:
#define RESUMEBACKUP (10 * FRAMESPERSEC)

class cDvbPlayer : public cPlayer, cThread {
private:
  enum ePlayModes { pmPlay, pmPause, pmSlow, pmFast, pmStill };
  enum ePlayDirs { pdForward, pdBackward };
  static int Speeds[];
  cRingBufferFrame *ringBuffer;
  cBackTrace *backTrace;
  cFileName *fileName;
  cIndexFile *index;
  int replayFile;
  bool eof;
  bool active;
  ePlayModes playMode;
  ePlayDirs playDir;
  int trickSpeed;
  int readIndex, writeIndex;
  cFrame *readFrame;
  const cFrame *playFrame;
  void TrickSpeed(int Increment);
  void Empty(void);
  void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00);
  bool NextFile(uchar FileNumber = 0, int FileOffset = -1);
  int Resume(void);
  bool Save(void);
protected:
  virtual void Activate(bool On);
  virtual void Action(void);
public:
  cDvbPlayer(const char *FileName);
  virtual ~cDvbPlayer();
  bool Active(void) { return active; }
  void Pause(void);
  void Play(void);
  void Forward(void);
  void Backward(void);
  int SkipFrames(int Frames);
  void SkipSeconds(int Seconds);
  void Goto(int Position, bool Still = false);
  void GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
  };

#define MAX_VIDEO_SLOWMOTION 63 // max. arg to pass to VIDEO_SLOWMOTION // TODO is this value correct?
#define NORMAL_SPEED  4 // the index of the '1' entry in the following array
#define MAX_SPEEDS    3 // the offset of the maximum speed from normal speed in either direction
#define SPEED_MULT   12 // the speed multiplier
int cDvbPlayer::Speeds[] = { 0, -2, -4, -8, 1, 2, 4, 12, 0 };

cDvbPlayer::cDvbPlayer(const char *FileName)
{
  ringBuffer = NULL;
  backTrace = NULL;
  index = NULL;
  eof = false;
  active = false;
  playMode = pmPlay;
  playDir = pdForward;
  trickSpeed = NORMAL_SPEED;
  readIndex = writeIndex = -1;
  readFrame = NULL;
  playFrame = NULL;
  isyslog("replay %s", FileName);
  fileName = new cFileName(FileName, false);
  replayFile = fileName->Open();
  if (replayFile < 0)
     return;
  ringBuffer = new cRingBufferFrame(VIDEOBUFSIZE);
  // Create the index file:
  index = new cIndexFile(FileName, false);
  if (!index)
     esyslog("ERROR: can't allocate index");
  else if (!index->Ok()) {
     delete index;
     index = NULL;
     }
  backTrace = new cBackTrace;
}

cDvbPlayer::~cDvbPlayer()
{
  Detach();
  Save();
  delete index;
  delete fileName;
  delete backTrace;
  delete ringBuffer;
}

void cDvbPlayer::TrickSpeed(int Increment)
{
  int nts = trickSpeed + Increment;
  if (Speeds[nts] == 1) {
     trickSpeed = nts;
     if (playMode == pmFast)
        Play();
     else
        Pause();
     }
  else if (Speeds[nts]) {
     trickSpeed = nts;
     int Mult = (playMode == pmSlow && playDir == pdForward) ? 1 : SPEED_MULT;
     int sp = (Speeds[nts] > 0) ? Mult / Speeds[nts] : -Speeds[nts] * Mult;
     if (sp > MAX_VIDEO_SLOWMOTION)
        sp = MAX_VIDEO_SLOWMOTION;
     DeviceTrickSpeed(sp);
     }
}

void cDvbPlayer::Empty(void)
{
  Lock();
  if ((readIndex = backTrace->Get(playDir == pdForward)) < 0)
     readIndex = writeIndex;
  readFrame = NULL;
  playFrame = NULL;
  ringBuffer->Clear();
  backTrace->Clear();
  DeviceClear();
  Unlock();
}

void cDvbPlayer::StripAudioPackets(uchar *b, int Length, uchar Except)
{
  if (index) {
     for (int i = 0; i < Length - 6; i++) {
         if (b[i] == 0x00 && b[i + 1] == 0x00 && b[i + 2] == 0x01) {
            uchar c = b[i + 3];
            int l = b[i + 4] * 256 + b[i + 5] + 6;
            switch (c) {
              case 0xBD: // dolby
                   if (Except)
                      ;//XXX+ PlayExternalDolby(&b[i], Length - i);
                   // continue with deleting the data - otherwise it disturbs DVB replay
              case 0xC0 ... 0xC1: // audio
                   if (c == 0xC1)
                      ;//XXX+ canToggleAudioTrack = true;
                   if (!Except || c != Except) {
                      int n = l;
                      for (int j = i; j < Length && n--; j++)
                          b[j] = 0x00;
                      }
                   break;
              case 0xE0 ... 0xEF: // video
                   break;
              default:
                   //esyslog("ERROR: unexpected packet id %02X", c);
                   l = 0;
              }
            if (l)
               i += l - 1; // the loop increments, too!
            }
         /*XXX
         else
            esyslog("ERROR: broken packet header");
            XXX*/
         }
     }
}

bool cDvbPlayer::NextFile(uchar FileNumber, int FileOffset)
{
  if (FileNumber > 0)
     replayFile = fileName->SetOffset(FileNumber, FileOffset);
  else if (replayFile >= 0 && eof)
     replayFile = fileName->NextFile();
  eof = false;
  return replayFile >= 0;
}

int cDvbPlayer::Resume(void)
{
  if (index) {
     int Index = index->GetResume();
     if (Index >= 0) {
        uchar FileNumber;
        int FileOffset;
        if (index->Get(Index, &FileNumber, &FileOffset) && NextFile(FileNumber, FileOffset))
           return Index;
        }
     }
  return -1;
}

bool cDvbPlayer::Save(void)
{
  if (index) {
     int Index = writeIndex;
     if (Index >= 0) {
        Index -= RESUMEBACKUP;
        if (Index > 0)
           Index = index->GetNextIFrame(Index, false);
        else
           Index = 0;
        if (Index >= 0)
           return index->StoreResume(Index);
        }
     }
  return false;
}

void cDvbPlayer::Activate(bool On)
{
  if (On) {
     if (replayFile >= 0)
        Start();
     }
  else if (active) {
     active = false;
     Cancel(3);
     }
}

void cDvbPlayer::Action(void)
{
  active = true;
  dsyslog("dvbplayer thread started (pid=%d)", getpid());

  uchar b[MAXFRAMESIZE];
  const uchar *p = NULL;
  int pc = 0;

  pollfd pfd[2];
  pfd[0].fd = DeviceFileHandle();
  pfd[0].events = pfd[0].revents = POLLOUT;
  pfd[1].fd = replayFile;
  pfd[1].events = pfd[1].revents = POLLIN;

  readIndex = Resume();
  if (readIndex >= 0)
     isyslog("resuming replay at index %d (%s)", readIndex, IndexToHMSF(readIndex, true));

  while (active && NextFile()) {
        {
          LOCK_THREAD;

          // Read the next frame from the file:

          if (!readFrame && (pfd[1].revents & POLLIN)) {
             if (playMode != pmStill) {
                int r = 0;
                if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward)) {
                   uchar FileNumber;
                   int FileOffset, Length;
                   int Index = index->GetNextIFrame(readIndex, playDir == pdForward, &FileNumber, &FileOffset, &Length, true);
                   if (Index >= 0) {
                      if (!NextFile(FileNumber, FileOffset))
                         break;
                      }
                   else {
                      // can't call Play() here, because those functions may only be
                      // called from the foreground thread - and we also don't need
                      // to empty the buffer here
                      DevicePlay();
                      playMode = pmPlay;
                      playDir = pdForward;
                      continue;
                      }
                   readIndex = Index;
                   r = ReadFrame(replayFile, b, Length, sizeof(b));
                   // must call StripAudioPackets() here because the buffer is not emptied
                   // when falling back from "fast forward" to "play" (see above)
                   StripAudioPackets(b, r);
                   }
                else if (index) {
                   uchar FileNumber;
                   int FileOffset, Length;
                   readIndex++;
                   if (!(index->Get(readIndex, &FileNumber, &FileOffset, NULL, &Length) && NextFile(FileNumber, FileOffset)))
                      break;
                   r = ReadFrame(replayFile, b, Length, sizeof(b));
                   }
                else // allows replay even if the index file is missing
                   r = read(replayFile, b, sizeof(b));
                if (r > 0)
                   readFrame = new cFrame(b, r, ftUnknown, readIndex);
                else if (r == 0)
                   eof = true;
                else if (r < 0 && FATALERRNO) {
                   LOG_ERROR;
                   break;
                   }
                }
             else//XXX
                usleep(1); // this keeps the CPU load low
             }

          // Store the frame in the buffer:

          if (readFrame) {
             if (ringBuffer->Put(readFrame))
                readFrame = NULL;
             }

          // Get the next frame from the buffer:
        
          if (!playFrame) {
             playFrame = ringBuffer->Get();
             p = NULL;
             pc = 0;
             }

          // Play the frame:

          if (playFrame && (pfd[0].revents & POLLOUT)) {
             if (!p) {
                p = playFrame->Data();
                pc = playFrame->Count();
                }
             if (p) {
                int w = PlayVideo(p, pc);
                if (w > 0) {
                   p += w;
                   pc -= w;
                   }
                else if (w < 0 && FATALERRNO) {
                   LOG_ERROR;
                   break;
                   }
                }
             if (pc == 0) {
                writeIndex = playFrame->Index();
                backTrace->Add(playFrame->Index(), playFrame->Count());
                ringBuffer->Drop(playFrame);
                playFrame = NULL;
                p = 0;
                }
             }
        }

        // Wait for input or output to become ready:

        if (poll(pfd, readFrame ? 1 : 2, 10) < 0 && FATALERRNO) {
           LOG_ERROR;
           break;
           }
        }

  dsyslog("dvbplayer thread ended (pid=%d)", getpid());
  active = false;
}

void cDvbPlayer::Pause(void)
{
  if (playMode == pmPause || playMode == pmStill)
     Play();
  else {
     LOCK_THREAD;
     if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward))
        Empty();
     DeviceFreeze();
     playMode = pmPause;
     }
}

void cDvbPlayer::Play(void)
{
  if (playMode != pmPlay) {
     LOCK_THREAD;
     if (playMode == pmStill || playMode == pmFast || (playMode == pmSlow && playDir == pdBackward))
        Empty();
     DevicePlay();
     playMode = pmPlay;
     playDir = pdForward;
    }
}

void cDvbPlayer::Forward(void)
{
  if (index) {
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? 1 : -1);
               break;
               }
            else if (playDir == pdForward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay: {
            LOCK_THREAD;
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            }
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? -1 : 1);
               break;
               }
            else if (playDir == pdForward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause:
            DeviceMute();
            playMode = pmSlow;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
       }
     }
}

void cDvbPlayer::Backward(void)
{
  if (index) {
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? 1 : -1);
               break;
               }
            else if (playDir == pdBackward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay: {
            LOCK_THREAD;
            Empty();
            DeviceMute();
            playMode = pmFast;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            }
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? -1 : 1);
               break;
               }
            else if (playDir == pdBackward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause: {
            LOCK_THREAD;
            Empty();
            DeviceMute();
            playMode = pmSlow;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            }
            break;
       }
     }
}

int cDvbPlayer::SkipFrames(int Frames)
{
  if (index && Frames) {
     int Current, Total;
     GetIndex(Current, Total, true);
     int OldCurrent = Current;
     Current = index->GetNextIFrame(Current + Frames, Frames > 0);
     return Current >= 0 ? Current : OldCurrent;
     }
  return -1;
}

void cDvbPlayer::SkipSeconds(int Seconds)
{
  if (index && Seconds) {
     LOCK_THREAD;
     Empty();
     int Index = writeIndex;
     if (Index >= 0) {
        Index = max(Index + Seconds * FRAMESPERSEC, 0);
        if (Index > 0)
           Index = index->GetNextIFrame(Index, false, NULL, NULL, NULL, true);
        if (Index >= 0)
           readIndex = writeIndex = Index - 1; // Input() will first increment it!
        }
     Play();
     }
}

void cDvbPlayer::Goto(int Index, bool Still)
{
  if (index) {
     LOCK_THREAD;
     Empty();
     if (++Index <= 0)
        Index = 1; // not '0', to allow GetNextIFrame() below to work!
     uchar FileNumber;
     int FileOffset, Length;
     Index = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset, &Length);
     if (Index >= 0 && NextFile(FileNumber, FileOffset) && Still) {
        uchar b[MAXFRAMESIZE];
        int r = ReadFrame(replayFile, b, Length, sizeof(b));
        if (r > 0) {
           if (playMode == pmPause)
              DevicePlay();
           StripAudioPackets(b, r);
           DeviceStillPicture(b, r);
           }
        playMode = pmStill;
        }
     readIndex = writeIndex = Index;
     }
}

void cDvbPlayer::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (index) {
     if (playMode == pmStill)
        Current = max(readIndex, 0);
     else {
        Current = max(writeIndex, 0);
        if (SnapToIFrame) {
           int i1 = index->GetNextIFrame(Current + 1, false);
           int i2 = index->GetNextIFrame(Current, true);
           Current = (abs(Current - i1) <= abs(Current - i2)) ? i1 : i2;
           }
        }
     Total = index->Last();
     }
  else
     Current = Total = -1;
}

bool cDvbPlayer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  Play = (playMode == pmPlay || playMode == pmFast);
  Forward = (playDir == pdForward);
  if (playMode == pmFast || playMode == pmSlow)
     Speed = Setup.MultiSpeedMode ? abs(trickSpeed - NORMAL_SPEED) : 0;
  else
     Speed = -1;
  return true;
}

// --- cDvbPlayerControl -----------------------------------------------------

cDvbPlayerControl::cDvbPlayerControl(void)
{
  player = NULL;
}

cDvbPlayerControl::~cDvbPlayerControl()
{
  Stop();
}

bool cDvbPlayerControl::Active(void)
{
  return player && player->Active();
}

bool cDvbPlayerControl::Start(const char *FileName)
{
  delete player;
  player = new cDvbPlayer(FileName);
  if (cDevice::PrimaryDevice()->Attach(player))
     return true;
  Stop();
  return false;
}

void cDvbPlayerControl::Stop(void)
{
  delete player;
  player = NULL;
}

void cDvbPlayerControl::Pause(void)
{
  if (player)
     player->Pause();
}

void cDvbPlayerControl::Play(void)
{
  if (player)
     player->Play();
}

void cDvbPlayerControl::Forward(void)
{
  if (player)
     player->Forward();
}

void cDvbPlayerControl::Backward(void)
{
  if (player)
     player->Backward();
}

void cDvbPlayerControl::SkipSeconds(int Seconds)
{
  if (player)
     player->SkipSeconds(Seconds);
}

int cDvbPlayerControl::SkipFrames(int Frames)
{
  if (player)
     return player->SkipFrames(Frames);
  return -1;
}

bool cDvbPlayerControl::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (player) {
     player->GetIndex(Current, Total, SnapToIFrame);
     return true;
     }
  return false;
}

bool cDvbPlayerControl::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  return player && player->GetReplayMode(Play, Forward, Speed);
}

void cDvbPlayerControl::Goto(int Position, bool Still)
{
  if (player)
     player->Goto(Position, Still);
}
