/*
 * vdr.c: Video Disk Recorder main program
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
 * $Id: vdr.c 1.19 2000/05/27 15:38:35 kls Exp $
 */

#include <signal.h>
#include "config.h"
#include "dvbapi.h"
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

  if (!cDvbApi::Init())
     return 1;

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
  cReplayControl *ReplayControl = NULL;
  int dcTime = 0, dcNumber = 0;
  int LastChannel = -1;

  while (!Interrupted) {
        // Channel display:
        if (CurrentChannel != LastChannel) {
           if (!Menu) {
              cChannel *channel = Channels.Get(CurrentChannel);
              if (channel)
                 Interface.DisplayChannel(CurrentChannel + 1, channel->name);
              }
           LastChannel = CurrentChannel;
           }
        // Direct Channel Select (action):
        if (dcNumber && time_ms() - dcTime > DIRECTCHANNELTIMEOUT) {
           cChannel::SwitchTo(dcNumber - 1);
           dcNumber = 0;
           LastChannel = -1; // in case an invalid channel number was entered!
           }
        // Timers and Recordings:
        if (!Menu) {
           cTimer *Timer = cTimer::GetMatch();
           if (Timer) {
              if (!cRecordControls::Start(Timer)) {
                 //TODO need to do something to prevent the timer from hitting over and over again...
                 }
              }
           cRecordControls::Process();
           }
        // User Input:
        cOsdBase **Interact = Menu ? (cOsdBase **)&Menu : (cOsdBase **)&ReplayControl;
        eKeys key = Interface.GetKey(!*Interact || !(*Interact)->NeedsFastResponse());
        if (*Interact) {
           switch ((*Interact)->ProcessKey(key)) {
             case osMenu:   DELETENULL(Menu);
                            Menu = new cMenuMain(ReplayControl);
                            break;
             case osRecord: DELETENULL(Menu);
                            if (!cRecordControls::Start())
                               Interface.Error("No free DVB device to record!");
                            break;
             case osReplay: DELETENULL(Menu);
                            DELETENULL(ReplayControl);
                            ReplayControl = new cReplayControl;
                            break;
             case osStopReplay:
                            DELETENULL(*Interact);
                            DELETENULL(ReplayControl);
                            break;
             case osBack:
             case osEnd:    DELETENULL(*Interact);
                            break;
             default:       ;
             }
           }
        else {
           switch (key) {
             // Direct Channel Select (input):
             case k0: case k1: case k2: case k3: case k4: case k5: case k6: case k7: case k8: case k9:
                  {
                    if (!Interface.Recording()) {
                       dcNumber = dcNumber * 10 + key - k0;
                       dcTime = time_ms();
                       Interface.DisplayChannel(dcNumber);
                       }
                  }
                  break;
             // Up/Down Channel Select:
             case kUp:
             case kDown: if (!Interface.Recording()) {
                            int n = CurrentChannel + (key == kUp ? 1 : -1);
                            cChannel *channel = Channels.Get(n);
                            if (channel)
                               channel->Switch();
                            }
                         break;
             // Menu Control:
             case kMenu: Menu = new cMenuMain(ReplayControl); break;
             // Viewing Control:
             case kOk:   LastChannel = -1; break; // forces channel display
             default:    break;
             }
           }
        }
  isyslog(LOG_INFO, "caught signal %d", Interrupted);
  delete Menu;
  delete ReplayControl;
  cDvbApi::Cleanup();
  isyslog(LOG_INFO, "exiting");
  closelog();
  return 0;
}
