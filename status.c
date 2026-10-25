/*
 * status.c: Status monitoring
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: status.c 5.6 2026/10/25 14:49:46 kls Exp $
 */

#include "status.h"
#include "thread.h"

// --- cStatus ---------------------------------------------------------------

cList<cStatus> cStatus::statusMonitors;

static cMutex Mutex;
static cVector<cStatus *> InUse; // status monitors a thread is currently calling, so that ~cStatus() can wait for them

// The mutex is not held during the callbacks: they may take any lock (an output
// plugin attaches the player in ChannelSwitch()), and the main thread sends
// messages while it holds the cControl mutex.
static cStatus *NextMonitor(cList<cStatus> &Monitors, cStatus *This)
{
  cMutexLock MutexLock(&Mutex);
  if (This)
     InUse.Remove(InUse.IndexOf(This));
  cStatus *sm = This ? Monitors.Next(This) : Monitors.First();
  if (sm)
     InUse.Append(sm);
  return sm;
}

cStatus::cStatus(void)
{
  Mutex.Lock();
  statusMonitors.Add(this);
  Mutex.Unlock();
}

cStatus::~cStatus()
{
  Mutex.Lock();
  while (InUse.IndexOf(this) >= 0) { // another thread is inside one of this monitor's callbacks
        Mutex.Unlock();
        cCondWait::SleepMs(1);
        Mutex.Lock();
        }
  statusMonitors.Del(this, false);
  Mutex.Unlock();
}

void cStatus::MsgChannelChange(const cChannel *Channel)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->ChannelChange(Channel);
}

void cStatus::MsgTimerChange(const cTimer *Timer, eTimerChange Change)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->TimerChange(Timer, Change);
}

void cStatus::MsgChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->ChannelSwitch(Device, ChannelNumber, LiveView);
}

void cStatus::MsgRecording(const cDevice *Device, const char *Name, const char *FileName, bool On)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->Recording(Device, Name, FileName, On);
}

void cStatus::MsgReplaying(const cControl *Control, const char *Name, const char *FileName, bool On)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->Replaying(Control, Name, FileName, On);
}

void cStatus::MsgMarksModified(const cMarks* Marks)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->MarksModified(Marks);
}

void cStatus::MsgSetVolume(int Volume, bool Absolute)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->SetVolume(Volume, Absolute);
}

void cStatus::MsgSetAudioTrack(int Index, const char * const *Tracks)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->SetAudioTrack(Index, Tracks);
}

void cStatus::MsgSetAudioChannel(int AudioChannel)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->SetAudioChannel(AudioChannel);
}

void cStatus::MsgSetSubtitleTrack(int Index, const char * const *Tracks)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->SetSubtitleTrack(Index, Tracks);
}

void cStatus::MsgOsdClear(void)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdClear();
}

void cStatus::MsgOsdTitle(const char *Title)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdTitle(Title);
}

void cStatus::MsgOsdStatusMessage(eMessageType Type, const char *Message)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdStatusMessage(Type, Message);
}

void cStatus::MsgOsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdHelpKeys(Red, Green, Yellow, Blue);
}

void cStatus::MsgOsdItem(const char *Text, int Index, bool Selectable)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdItem(Text, Index, Selectable);
}

void cStatus::MsgOsdCurrentItem(const char *Text, int Index)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdCurrentItem(Text, Index);
}

void cStatus::MsgOsdTextItem(const char *Text, bool Scroll)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdTextItem(Text, Scroll);
}

void cStatus::MsgOsdChannel(const char *Text)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdChannel(Text);
}

void cStatus::MsgOsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
  for (cStatus *sm = NextMonitor(statusMonitors, NULL); sm; sm = NextMonitor(statusMonitors, sm))
      sm->OsdProgramme(PresentTime, PresentTitle, PresentSubtitle, FollowingTime, FollowingTitle, FollowingSubtitle);
}
