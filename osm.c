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
 * $Id: osm.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "config.h"
#include "dvbapi.h"
#include "interface.h"
#include "menu.h"
#include "tools.h"

#ifdef DEBUG_REMOTE
#define KEYS_CONF "keys-pc.conf"
#else
#define KEYS_CONF "keys.conf"
#endif

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

  cMenuMain *Menu = NULL;
  cTimer *Timer = NULL;
  cDvbRecorder *Recorder = NULL;

  for (;;) {
      //TODO check for free disk space and delete files if necessary/possible
      //     in case there is an ongoing recording
      if (!Timer && (Timer = cTimer::GetMatch()) != NULL) {
         // switch to channel:
         isyslog(LOG_INFO, "timer %d start", Timer->Index() + 1);
         delete Menu;
         Menu = NULL;
         cChannel::SwitchTo(Timer->channel - 1);
         ChannelLocked = true;
         // start recording:
         delete Recorder;
         Recorder = new cDvbRecorder;
         //TODO special filename handling!!!
         if (!Recorder->Record(Timer->file, Timer->quality)) {
            delete Recorder;
            Recorder = NULL;
            }
         }
      if (Timer) {
         if (!Timer->Matches()) {
            // stop recording:
            if (Recorder)
               Recorder->Stop();
            // end timer:
            ChannelLocked = false;
            isyslog(LOG_INFO, "timer %d stop", Timer->Index() + 1);
            Timer = NULL;
            //TODO switch back to the previous channel???
            //TODO clear single event timer???
            }
         }
      eKeys key = Interface.GetKey();
      if (Menu) {
         switch (Menu->ProcessKey(key)) {
           default: if (key != kMenu)
                       break;
           case osBack:
           case osEnd: delete Menu;
                       Menu = NULL;
                       break;
           }
         }
      else {
         switch (key) {
           case kMenu: Menu = new cMenuMain;
                       Menu->Display();
                       break;
           case kUp:
           case kDown: {
                         int n = CurrentChannel + (key == kUp ? 1 : -1);
                         cChannel *channel = Channels.Get(n);
                         if (channel)
                            channel->Switch();
                       }
                       break;
           default:    break;
           }
         }
      }
  closelog();
  return 1;
}
