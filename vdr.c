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
 * $Id: vdr.c 1.102 2002/03/29 10:09:20 kls Exp $
 */

#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "config.h"
#include "dvbapi.h"
#include "eit.h"
#include "i18n.h"
#include "interface.h"
#include "menu.h"
#include "recording.h"
#include "tools.h"
#include "videodir.h"

#ifdef REMOTE_KBD
#define KEYS_CONF "keys-pc.conf"
#else
#define KEYS_CONF "keys.conf"
#endif

#define ACTIVITYTIMEOUT 60 // seconds before starting housekeeping
#define SHUTDOWNWAIT   300 // seconds to wait in user prompt before automatic shutdown
#define MANUALSTART    600 // seconds the next timer must be in the future to assume manual start

static int Interrupted = 0;

static void SignalHandler(int signum)
{
  if (signum != SIGPIPE) {
     Interrupted = signum;
     Interface->Interrupt();
     }
  signal(signum, SignalHandler);
}

static void Watchdog(int signum)
{
  // Something terrible must have happened that prevented the 'alarm()' from
  // being called in time, so let's get out of here:
  esyslog(LOG_ERR, "PANIC: watchdog timer expired - exiting!");
  exit(1);
}

int main(int argc, char *argv[])
{
  // Initiate locale:

  setlocale(LC_ALL, "");

  // Command line options:

#define DEFAULTSVDRPPORT 2001
#define DEFAULTWATCHDOG     0 // seconds

  int SVDRPport = DEFAULTSVDRPPORT;
  const char *ConfigDirectory = NULL;
  bool DaemonMode = false;
  bool MuteAudio = false;
  int WatchdogTimeout = DEFAULTWATCHDOG;
  const char *Terminal = NULL;
  const char *Shutdown = NULL;

  static struct option long_options[] = {
      { "audio",    required_argument, NULL, 'a' },
      { "config",   required_argument, NULL, 'c' },
      { "daemon",   no_argument,       NULL, 'd' },
      { "device",   required_argument, NULL, 'D' },
      { "epgfile",  required_argument, NULL, 'E' },
      { "help",     no_argument,       NULL, 'h' },
      { "log",      required_argument, NULL, 'l' },
      { "mute",     no_argument,       NULL, 'm' },
      { "port",     required_argument, NULL, 'p' },
      { "record",   required_argument, NULL, 'r' },
      { "shutdown", required_argument, NULL, 's' },
      { "terminal", required_argument, NULL, 't' },
      { "version",  no_argument,       NULL, 'V' },
      { "video",    required_argument, NULL, 'v' },
      { "watchdog", required_argument, NULL, 'w' },
      { NULL }
    };

  int c;
  int option_index = 0;
  while ((c = getopt_long(argc, argv, "a:c:dD:E:hl:mp:r:s:t:v:Vw:", long_options, &option_index)) != -1) {
        switch (c) {
          case 'a': cDvbApi::SetAudioCommand(optarg);
                    break;
          case 'c': ConfigDirectory = optarg;
                    break;
          case 'd': DaemonMode = true; break;
          case 'D': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       if (0 <= n && n < MAXDVBAPI) {
                          cDvbApi::SetUseDvbApi(n);
                          break;
                          }
                       }
                    fprintf(stderr, "vdr: invalid DVB device number: %s\n", optarg);
                    return 2;
                    break;
          case 'E': cSIProcessor::SetEpgDataFileName(*optarg != '-' ? optarg : NULL);
                    break;
          case 'h': printf("Usage: vdr [OPTION]\n\n"           // for easier orientation, this is column 80|
                           "  -a CMD,   --audio=CMD    send Dolby Digital audio to stdin of command CMD\n"
                           "  -c DIR,   --config=DIR   read config files from DIR (default is to read them\n"
                           "                           from the video directory)\n"
                           "  -d,       --daemon       run in daemon mode\n"
                           "  -D NUM,   --device=NUM   use only the given DVB device (NUM = 0, 1, 2...)\n"
                           "                           there may be several -D options (default: all DVB\n"
                           "                           devices will be used)\n"
                           "  -E FILE   --epgfile=FILE write the EPG data into the given FILE (default is\n"
                           "                           %s); use '-E-' to disable this\n"
                           "                           if FILE is a directory, the default EPG file will be\n"
                           "                           created in that directory\n"
                           "  -h,       --help         print this help and exit\n"
                           "  -l LEVEL, --log=LEVEL    set log level (default: 3)\n"
                           "                           0 = no logging, 1 = errors only,\n"
                           "                           2 = errors and info, 3 = errors, info and debug\n"
                           "  -m,       --mute         mute audio of the primary DVB device at startup\n"
                           "  -p PORT,  --port=PORT    use PORT for SVDRP (default: %d)\n"
                           "                           0 turns off SVDRP\n"
                           "  -r CMD,   --record=CMD   call CMD before and after a recording\n"
                           "  -s CMD,   --shutdown=CMD call CMD to shutdown the computer\n"
                           "  -t TTY,   --terminal=TTY controlling tty\n"
                           "  -V,       --version      print version information and exit\n"
                           "  -v DIR,   --video=DIR    use DIR as video directory (default: %s)\n"
                           "  -w SEC,   --watchdog=SEC activate the watchdog timer with a timeout of SEC\n"
                           "                           seconds (default: %d); '0' disables the watchdog\n"
                           "\n"
                           "Report bugs to <vdr-bugs@cadsoft.de>\n",
                           cSIProcessor::GetEpgDataFileName() ? cSIProcessor::GetEpgDataFileName() : "'-'",
                           DEFAULTSVDRPPORT,
                           VideoDirectory,
                           DEFAULTWATCHDOG
                           );
                    return 0;
                    break;
          case 'l': if (isnumber(optarg)) {
                       int l = atoi(optarg);
                       if (0 <= l && l <= 3) {
                          SysLogLevel = l;
                          break;
                          }
                       }
                    fprintf(stderr, "vdr: invalid log level: %s\n", optarg);
                    return 2;
                    break;
          case 'm': MuteAudio = true;
                    break;
          case 'p': if (isnumber(optarg))
                       SVDRPport = atoi(optarg);
                    else {
                       fprintf(stderr, "vdr: invalid port number: %s\n", optarg);
                       return 2;
                       }
                    break;
          case 'r': cRecordingUserCommand::SetCommand(optarg);
                    break;
          case 's': Shutdown = optarg;
                    break;
          case 't': Terminal = optarg;
                    break;
          case 'V': printf("vdr, version %s\n", VDRVERSION);
                    return 0;
                    break;
          case 'v': VideoDirectory = optarg;
                    while (optarg && *optarg && optarg[strlen(optarg) - 1] == '/')
                          optarg[strlen(optarg) - 1] = 0;
                    break;
          case 'w': if (isnumber(optarg)) {
                       int t = atoi(optarg);
                       if (t >= 0) {
                          WatchdogTimeout = t;
                          break;
                          }
                       }
                    fprintf(stderr, "vdr: invalid watchdog timeout: %s\n", optarg);
                    return 2;
                    break;
          default:  return 2;
          }
        }

  // Log file:

  if (SysLogLevel > 0)
     openlog("vdr", LOG_PID | LOG_CONS, LOG_USER);

  // Check the video directory:

  if (!DirectoryOk(VideoDirectory, true)) {
     fprintf(stderr, "vdr: can't access video directory %s\n", VideoDirectory);
     return 2;
     }

  // Daemon mode:

  if (DaemonMode) {
#if !defined(DEBUG_OSD) && !defined(REMOTE_KBD)
     pid_t pid = fork();
     if (pid < 0) {
        fprintf(stderr, "%m\n");
        esyslog(LOG_ERR, "ERROR: %m");
        return 2;
        }
     if (pid != 0)
        return 0; // initial program immediately returns
     fclose(stdin);
     fclose(stdout);
     fclose(stderr);
#else
     fprintf(stderr, "vdr: can't run in daemon mode with DEBUG_OSD or REMOTE_KBD on!\n");
     return 2;
#endif
     }
  else if (Terminal) {
     // Claim new controlling terminal
     stdin  = freopen(Terminal, "r", stdin);
     stdout = freopen(Terminal, "w", stdout);
     stderr = freopen(Terminal, "w", stderr);
     }

  isyslog(LOG_INFO, "VDR version %s started", VDRVERSION);

  // Configuration data:

  if (!ConfigDirectory)
     ConfigDirectory = VideoDirectory;

  Setup.Load(AddDirectory(ConfigDirectory, "setup.conf"));
  Channels.Load(AddDirectory(ConfigDirectory, "channels.conf"));
  Timers.Load(AddDirectory(ConfigDirectory, "timers.conf"));
  Commands.Load(AddDirectory(ConfigDirectory, "commands.conf"));
  SVDRPhosts.Load(AddDirectory(ConfigDirectory, "svdrphosts.conf"), true);
  CaDefinitions.Load(AddDirectory(ConfigDirectory, "ca.conf"), true);
#if defined(REMOTE_LIRC)
  Keys.SetDummyValues();
#elif !defined(REMOTE_NONE)
  bool KeysLoaded = Keys.Load(AddDirectory(ConfigDirectory, KEYS_CONF));
#endif

  // DVB interfaces:

  if (!cDvbApi::Init())
     return 2;

  cDvbApi::SetPrimaryDvbApi(Setup.PrimaryDVB);

  cSIProcessor::Read();

  Channels.SwitchTo(Setup.CurrentChannel);
  if (MuteAudio)
     cDvbApi::PrimaryDvbApi->ToggleMute();
  else
     cDvbApi::PrimaryDvbApi->SetVolume(Setup.CurrentVolume, true);

  cEITScanner EITScanner;

  // User interface:

  Interface = new cInterface(SVDRPport);
#if !defined(REMOTE_LIRC) && !defined(REMOTE_NONE)
  if (!KeysLoaded)
     Interface->LearnKeys();
#endif

  // Signal handlers:

  if (signal(SIGHUP,  SignalHandler) == SIG_IGN) signal(SIGHUP,  SIG_IGN);
  if (signal(SIGINT,  SignalHandler) == SIG_IGN) signal(SIGINT,  SIG_IGN);
  if (signal(SIGTERM, SignalHandler) == SIG_IGN) signal(SIGTERM, SIG_IGN);
  if (signal(SIGPIPE, SignalHandler) == SIG_IGN) signal(SIGPIPE, SIG_IGN);
  if (WatchdogTimeout > 0)
     if (signal(SIGALRM, Watchdog)   == SIG_IGN) signal(SIGALRM, SIG_IGN);

  // Main program loop:

  cOsdBase *Menu = NULL;
  cReplayControl *ReplayControl = NULL;
  int LastChannel = -1;
  int PreviousChannel = cDvbApi::CurrentChannel();
  time_t LastActivity = 0;
  int MaxLatencyTime = 0;
  bool ForceShutdown = false;

  if (WatchdogTimeout > 0) {
     dsyslog(LOG_INFO, "setting watchdog timer to %d seconds", WatchdogTimeout);
     alarm(WatchdogTimeout); // Initial watchdog timer start
     }

  while (!Interrupted) {
        // Handle emergency exits:
        if (cThread::EmergencyExit()) {
           esyslog(LOG_ERR, "emergency exit requested - shutting down");
           break;
           }
        // Restart the Watchdog timer:
        if (WatchdogTimeout > 0) {
           int LatencyTime = WatchdogTimeout - alarm(WatchdogTimeout);
           if (LatencyTime > MaxLatencyTime) {
              MaxLatencyTime = LatencyTime;
              dsyslog(LOG_INFO, "max. latency time %d seconds", MaxLatencyTime);
              }
           }
        // Channel display:
        if (!EITScanner.Active() && cDvbApi::CurrentChannel() != LastChannel) {
           if (!Menu)
              Menu = new cDisplayChannel(cDvbApi::CurrentChannel(), LastChannel > 0);
           if (LastChannel > 0)
              PreviousChannel = LastChannel;
           LastChannel = cDvbApi::CurrentChannel();
           }
        // Timers and Recordings:
        if (!Menu) {
           time_t Now = time(NULL); // must do both following calls with the exact same time!
           cRecordControls::Process(Now);
           cTimer *Timer = Timers.GetMatch(Now);
           if (Timer) {
              if (!cRecordControls::Start(Timer))
                 Timer->SetPending(true);
              }
           }
        // User Input:
        cOsdBase **Interact = Menu ? &Menu : (cOsdBase **)&ReplayControl;
        eKeys key = Interface->GetKey(!*Interact || !(*Interact)->NeedsFastResponse());
        if (NORMALKEY(key) != kNone) {
           EITScanner.Activity();
           LastActivity = time(NULL);
           }
        // Keys that must work independent of any interactive mode:
        switch (key) {
          // Volume Control:
          case kVolUp|k_Repeat:
          case kVolUp:
          case kVolDn|k_Repeat:
          case kVolDn:
          case kMute:
               if (key == kMute) {
                  if (!cDvbApi::PrimaryDvbApi->ToggleMute() && !Menu)
                     break; // no need to display "mute off"
                  }
               else
                  cDvbApi::PrimaryDvbApi->SetVolume(NORMALKEY(key) == kVolDn ? -VOLUMEDELTA : VOLUMEDELTA);
               if (!Menu && (!ReplayControl || !ReplayControl->Visible()))
                  Menu = cDisplayVolume::Create();
               cDisplayVolume::Process(key);
               break;
          // Power off:
          case kPower: isyslog(LOG_INFO, "Power button pressed");
                       DELETENULL(*Interact);
                       if (!Shutdown) {
                          Interface->Error(tr("Can't shutdown - option '-s' not given!"));
                          break;
                          }
                       if (cRecordControls::Active()) {
                          if (Interface->Confirm(tr("Recording - shut down anyway?")))
                             ForceShutdown = true;
                          }
                       LastActivity = 1; // not 0, see below!
                       break;
          default:
            if (*Interact) {
               switch ((*Interact)->ProcessKey(key)) {
                 case osMenu:   DELETENULL(Menu);
                                Menu = new cMenuMain(ReplayControl);
                                break;
                 case osRecord: DELETENULL(Menu);
                                if (!cRecordControls::Start())
                                   Interface->Error(tr("No free DVB device to record!"));
                                break;
                 case osRecordings:
                                DELETENULL(Menu);
                                DELETENULL(ReplayControl);
                                Menu = new cMenuMain(ReplayControl, osRecordings);
                                break;
                 case osReplay: DELETENULL(Menu);
                                DELETENULL(ReplayControl);
                                ReplayControl = new cReplayControl;
                                break;
                 case osStopReplay:
                                DELETENULL(*Interact);
                                DELETENULL(ReplayControl);
                                break;
                 case osSwitchDvb:
                                DELETENULL(*Interact);
                                Interface->Info(tr("Switching primary DVB..."));
                                cDvbApi::SetPrimaryDvbApi(Setup.PrimaryDVB);
                                break;
                 case osBack:
                 case osEnd:    DELETENULL(*Interact);
                                break;
                 default:       ;
                 }
               }
            else {
               // Key functions in "normal" viewing mode:
               switch (key) {
                 // Toggle channels:
                 case k0: {
                      int CurrentChannel = cDvbApi::CurrentChannel();
                      Channels.SwitchTo(PreviousChannel);
                      PreviousChannel = CurrentChannel;
                      break;
                      }
                 // Direct Channel Select:
                 case k1 ... k9:
                      Menu = new cDisplayChannel(key);
                      break;
                 // Left/Right rotates trough channel groups:
                 case kLeft|k_Repeat:
                 case kLeft:
                 case kRight|k_Repeat:
                 case kRight:
                      Menu = new cDisplayChannel(NORMALKEY(key));
                      break;
                 // Up/Down Channel Select:
                 case kUp|k_Repeat:
                 case kUp:
                 case kDown|k_Repeat:
                 case kDown: {
                      int n = cDvbApi::CurrentChannel() + (NORMALKEY(key) == kUp ? 1 : -1);
                      cChannel *channel = Channels.GetByNumber(n);
                      if (channel)
                         channel->Switch();
                      break;
                      }
                 // Menu Control:
                 case kMenu: Menu = new cMenuMain(ReplayControl); break;
                 // Viewing Control:
                 case kOk:   LastChannel = -1; break; // forces channel display
                 default:    break;
                 }
               }
          }
        if (!Menu) {
           EITScanner.Process();
           if (!cVideoCutter::Active() && cVideoCutter::Ended()) {
              if (cVideoCutter::Error())
                 Interface->Error(tr("Editing process failed!"));
              else
                 Interface->Info(tr("Editing process finished"));
              }
           }
        if (!*Interact && (!cRecordControls::Active() || ForceShutdown)) {
           time_t Now = time(NULL);
           if (Now - LastActivity > ACTIVITYTIMEOUT) {
              // Shutdown:
              if (Shutdown && (Setup.MinUserInactivity || LastActivity == 1) && Now - LastActivity > Setup.MinUserInactivity * 60) {
                 cTimer *timer = Timers.GetNextActiveTimer();
                 time_t Next  = timer ? timer->StartTime() : 0;
                 time_t Delta = timer ? Next - Now : 0;
                 if (!LastActivity) {
                    if (!timer || Delta > MANUALSTART) {
                       // Apparently the user started VDR manually
                       dsyslog(LOG_INFO, "assuming manual start of VDR");
                       LastActivity = Now;
                       continue; // don't run into the actual shutdown procedure below
                       }
                    else
                       LastActivity = 1;
                    }
                 bool UserShutdown = key == kPower;
                 if (UserShutdown && Next && Delta <= Setup.MinEventTimeout * 60 && !ForceShutdown) {
                    char *buf;
                    asprintf(&buf, tr("Recording in %d minutes, shut down anyway?"), Delta / 60);
                    if (Interface->Confirm(buf))
                       ForceShutdown = true;
                    delete buf;
                    }
                 if (!Next || Delta > Setup.MinEventTimeout * 60 || ForceShutdown) {
                    ForceShutdown = false;
                    if (timer)
                       dsyslog(LOG_INFO, "next timer event at %s", ctime(&Next));
                    if (WatchdogTimeout > 0)
                       signal(SIGALRM, SIG_IGN);
                    if (Interface->Confirm(tr("Press any key to cancel shutdown"), UserShutdown ? 5 : SHUTDOWNWAIT, true)) {
                       int Channel = timer ? timer->channel : 0;
                       const char *File = timer ? timer->file : "";
                       char *cmd;
                       asprintf(&cmd, "%s %ld %ld %d \"%s\" %d", Shutdown, Next, Delta, Channel, strescape(File, "\"$"), UserShutdown);
                       isyslog(LOG_INFO, "executing '%s'", cmd);
                       SystemExec(cmd);
                       delete cmd;
                       }
                    else if (WatchdogTimeout > 0) {
                       alarm(WatchdogTimeout);
                       if (signal(SIGALRM, Watchdog) == SIG_IGN)
                          signal(SIGALRM, SIG_IGN);
                       }
                    LastActivity = time(NULL); // don't try again too soon
                    continue; // skip the rest of the housekeeping for now
                    }
                 }
              // Disk housekeeping:
              RemoveDeletedRecordings();
              }
           }
        }
  if (Interrupted)
     isyslog(LOG_INFO, "caught signal %d", Interrupted);
  Setup.CurrentChannel = cDvbApi::CurrentChannel();
  Setup.CurrentVolume  = cDvbApi::CurrentVolume();
  Setup.Save();
  cVideoCutter::Stop();
  delete Menu;
  delete ReplayControl;
  delete Interface;
  cDvbApi::Cleanup();
  if (WatchdogTimeout > 0)
     dsyslog(LOG_INFO, "max. latency time %d seconds", MaxLatencyTime);
  isyslog(LOG_INFO, "exiting");
  if (SysLogLevel > 0)
     closelog();
  if (cThread::EmergencyExit()) {
     esyslog(LOG_ERR, "emergency exit!");
     return 1;
     }
  return 0;
}
