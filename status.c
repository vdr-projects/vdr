/*
 * status.c: Status monitoring
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: status.c 1.5 2003/05/03 14:47:44 kls Exp $
 */

#include "status.h"

// --- cStatus ---------------------------------------------------------------

cList<cStatus> cStatus::statusMonitors;

cStatus::cStatus(void)
{
  statusMonitors.Add(this);
}

cStatus::~cStatus()
{
  statusMonitors.Del(this, false);
}

void cStatus::MsgChannelSwitch(const cDevice *Device, int ChannelNumber)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->ChannelSwitch(Device, ChannelNumber);
}

void cStatus::MsgRecording(const cDevice *Device, const char *Name)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->Recording(Device, Name);
}

void cStatus::MsgReplaying(const cControl *Control, const char *Name)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->Replaying(Control, Name);
}

void cStatus::MsgSetVolume(int Volume, bool Absolute)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->SetVolume(Volume, Absolute);
}

void cStatus::MsgOsdClear(void)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdClear();
}

void cStatus::MsgOsdTitle(const char *Title)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdTitle(Title);
}

void cStatus::MsgOsdStatusMessage(const char *Message)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdStatusMessage(Message);
}

void cStatus::MsgOsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdHelpKeys(Red, Green, Yellow, Blue);
}

void cStatus::MsgOsdItem(const char *Text, int Index)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdItem(Text, Index);
}

void cStatus::MsgOsdCurrentItem(const char *Text)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdCurrentItem(Text);
}

void cStatus::MsgOsdTextItem(const char *Text, bool Scroll)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdTextItem(Text, Scroll);
}

void cStatus::MsgOsdChannel(const char *Text)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdChannel(Text);
}

void cStatus::MsgOsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
  for (cStatus *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdProgramme(PresentTime, PresentTitle, PresentSubtitle, FollowingTime, FollowingTitle, FollowingSubtitle);
}
