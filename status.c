/*
 * status.c: Status monitoring
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: status.c 1.1 2002/05/19 14:54:30 kls Exp $
 */

#include "status.h"

// --- cStatusMonitor --------------------------------------------------------

cList<cStatusMonitor> cStatusMonitor::statusMonitors;

cStatusMonitor::cStatusMonitor(void)
{
  statusMonitors.Add(this);
}

cStatusMonitor::~cStatusMonitor()
{
  statusMonitors.Del(this, false);
}

void cStatusMonitor::MsgChannelSwitch(const cDvbApi *DvbApi, int ChannelNumber)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->ChannelSwitch(DvbApi, ChannelNumber);
}

void cStatusMonitor::MsgRecording(const cDvbApi *DvbApi, const char *Name)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->Recording(DvbApi, Name);
}

void cStatusMonitor::MsgReplaying(const cDvbApi *DvbApi, const char *Name)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->Replaying(DvbApi, Name);
}

void cStatusMonitor::MsgSetVolume(int Volume, bool Absolute)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->SetVolume(Volume, Absolute);
}

void cStatusMonitor::MsgOsdClear(void)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdClear();
}

void cStatusMonitor::MsgOsdTitle(const char *Title)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdTitle(Title);
}

void cStatusMonitor::MsgOsdStatusMessage(const char *Message)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdStatusMessage(Message);
}

void cStatusMonitor::MsgOsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdHelpKeys(Red, Green, Yellow, Blue);
}

void cStatusMonitor::MsgOsdCurrentItem(const char *Text)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdCurrentItem(Text);
}

void cStatusMonitor::MsgOsdTextItem(const char *Text, bool Scroll)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdTextItem(Text, Scroll);
}

void cStatusMonitor::MsgOsdChannel(const char *Text)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdChannel(Text);
}

void cStatusMonitor::MsgOsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
  for (cStatusMonitor *sm = statusMonitors.First(); sm; sm = statusMonitors.Next(sm))
      sm->OsdProgramme(PresentTime, PresentTitle, PresentSubtitle, FollowingTime, FollowingTitle, FollowingSubtitle);
}
