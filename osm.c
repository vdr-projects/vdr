/*
 * osm.c: On Screen Menu for the Video Disk Recorder
 *
 * Copyright (C) 2000 Klaus Schmidinger
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 
 * The author can be reached at kls@cadsoft.de
 *
 * The project's page is at http://www.cadsoft.de/people/kls/vdr
 *
 * $Id: osm.c 1.7 2000/04/22 13:35:37 kls Exp $
 */

#include <signal.h>
#include "config.h"
#include "interface.h"
#include "menu.h"
#include "recording.h"
#include "tools.h"

#ifdef DEBUG_REMOTE
#define KEYS_CONF "keys-pc.conf"
#else
#define KEYS_CONF "keys.conf"
#endif

#define DIRECTCHANNELTIMEOUT 500 //ms

static int Interrupted = 0;

void SignalHandler(int signum)
{
  Interrupted = signum;
}

int main(int argc, char *argv[])
{
  openlog("vdr", LOG_PID | LOG_CONS, LOG_USER);
  isyslog(LOG_INFO, "started");

  Channels.Load("channels.conf");
  Timers.Load("timers.conf");
  if (!Keys.Load(KEYS_CONF))
     Interface.LearnKeys();
  Interface.Init();

  cChannel::SwitchTo(CurrentChannel);

  if (signal(SIGHUP,  SignalHandler) == SIG_IGN) signal(SIGHUP,  SIG_IGN);
  if (signal(SIGINT,  SignalHandler) == SIG_IGN) signal(SIGINT,  SIG_IGN);
  if (signal(SIGTERM, SignalHandler) == SIG_IGN) signal(SIGTERM, SIG_IGN);

  cMenuMain *Menu = NULL;
  cTimer *Timer = NULL;
  int dcTime = 0, dcNumber = 0;
  int LastChannel = -1;

  while (!Interrupted) {
        // Channel display:
        if (CurrentChannel != LastChannel) {
           cChannel *channel = Channels.Get(CurrentChannel);
           if (channel)
              Interface.DisplayChannel(CurrentChannel + 1, channel->name);
           LastChannel = CurrentChannel;
           }
        // Direct Channel Select (action):
        if (dcNumber) {
           Interface.DisplayChannel(dcNumber);
           if (time_ms() - dcTime > DIRECTCHANNELTIMEOUT) {
              cChannel::SwitchTo(dcNumber - 1);
              dcNumber = 0;
              }
           }
        // Timer Processing:
        else {
           AssertFreeDiskSpace();
           if (!Timer && (Timer = cTimer::GetMatch()) != NULL) {
              DELETENULL(Menu);
              // make sure the timer won't be deleted:
              Timer->SetRecording(true);
              // switch to channel:
              cChannel::SwitchTo(Timer->channel - 1);
              // start recording:
              cRecording Recording(Timer);
              DvbApi.StartRecord(Recording.FileName());
              }
           if (Timer && !Timer->Matches()) {
              // stop recording:
              DvbApi.StopRecord();
              // release timer:
              Timer->SetRecording(false);
              // clear single event timer:
              if (Timer->IsSingleEvent()) {
                 DELETENULL(Menu); // must make sure no menu uses it
                 isyslog(LOG_INFO, "deleting timer %d", Timer->Index() + 1);
                 Timers.Del(Timer);
                 Timers.Save();
                 }
              Timer = NULL;
              }
           }
        // User Input:
        eKeys key = Interface.GetKey();
        if (Menu) {
           switch (Menu->ProcessKey(key)) {
             default: if (key != kMenu)
                         break;
             case osBack:
             case osEnd: DELETENULL(Menu);
                         break;
             }
           }
        else {
           switch (key) {
             // Direct Channel Select (input):
             case k0: case k1: case k2: case k3: case k4: case k5: case k6: case k7: case k8: case k9:
             {
               if (!DvbApi.Recording()) {
                  dcNumber = dcNumber * 10 + key - k0;
                  dcTime = time_ms();
                  }
             }
             // Record/Replay Control:
             case kBegin:         DvbApi.Skip(-INT_MAX); break;
             case kRecord:        if (!DvbApi.Recording()) {
                                     cTimer *timer = new cTimer(true);
                                     Timers.Add(timer);
                                     Timers.Save();
                                     }
                                  else
                                     Interface.Error("Already recording!");
                                  break;
             case kPause:         DvbApi.PauseReplay(); break;
             case kStop:          DvbApi.StopReplay(); break;
             case kSearchBack:    DvbApi.FastRewind(); break;
             case kSearchForward: DvbApi.FastForward(); break;
             case kSkipBack:      DvbApi.Skip(-60); break;
             case kSkipForward:   DvbApi.Skip(60); break;
             // Menu Control:
             case kMenu: Menu = new cMenuMain;
                         Menu->Display();
                         break;
             // Up/Down Channel Select:
             case kUp:
             case kDown: {
                           int n = CurrentChannel + (key == kUp ? 1 : -1);
                           cChannel *channel = Channels.Get(n);
                           if (channel)
                              channel->Switch();
                         }
                         break;
             // Viewing Control:
             case kOk:   LastChannel = -1; break; // forces channel display
                         //TODO if replaying switch to progress display instead
             default:    break;
             }
           }
        }
  isyslog(LOG_INFO, "caught signal %d", Interrupted);
  DvbApi.StopRecord();
  DvbApi.StopReplay();
  //TODO kill any remaining sub-processes!
  isyslog(LOG_INFO, "exiting");
  closelog();
  return 0;
}
