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
 * $Id: vdr.c 1.127 2002/10/13 12:13:19 kls Exp $
 */

#include <getopt.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "channels.h"
#include "config.h"
#include "cutter.h"
#include "device.h"
#include "diseqc.h"
#include "dvbdevice.h"
#include "eitscan.h"
#include "i18n.h"
#include "interface.h"
#include "keys.h"
#include "lirc.h"
#include "menu.h"
#include "osd.h"
#include "plugin.h"
#include "rcu.h"
#include "recording.h"
#include "sources.h"
#include "tools.h"
#include "videodir.h"

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
  esyslog("PANIC: watchdog timer expired - exiting!");
  exit(1);
}

int main(int argc, char *argv[])
{
  // Initiate locale:

  setlocale(LC_ALL, "");

  // Command line options:

#define DEFAULTSVDRPPORT 2001
#define DEFAULTWATCHDOG     0 // seconds
#define DEFAULTPLUGINDIR "./PLUGINS/lib"

  int SVDRPport = DEFAULTSVDRPPORT;
  const char *ConfigDirectory = NULL;
  bool DisplayHelp = false;
  bool DisplayVersion = false;
  bool DaemonMode = false;
  bool MuteAudio = false;
  int WatchdogTimeout = DEFAULTWATCHDOG;
  const char *Terminal = NULL;
  const char *Shutdown = NULL;
  cPluginManager PluginManager(DEFAULTPLUGINDIR);

  static struct option long_options[] = {
      { "audio",    required_argument, NULL, 'a' },
      { "config",   required_argument, NULL, 'c' },
      { "daemon",   no_argument,       NULL, 'd' },
      { "device",   required_argument, NULL, 'D' },
      { "epgfile",  required_argument, NULL, 'E' },
      { "help",     no_argument,       NULL, 'h' },
      { "lib",      required_argument, NULL, 'L' },
      { "log",      required_argument, NULL, 'l' },
      { "mute",     no_argument,       NULL, 'm' },
      { "plugin",   required_argument, NULL, 'P' },
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
  while ((c = getopt_long(argc, argv, "a:c:dD:E:hl:L:mp:P:r:s:t:v:Vw:", long_options, NULL)) != -1) {
        switch (c) {
          /*XXX+
          case 'a': cDevice::SetAudioCommand(optarg);
                    break;
                    XXX*/
          case 'c': ConfigDirectory = optarg;
                    break;
          case 'd': DaemonMode = true; break;
          case 'D': if (isnumber(optarg)) {
                       int n = atoi(optarg);
                       if (0 <= n && n < MAXDEVICES) {
                          cDevice::SetUseDevice(n);
                          break;
                          }
                       }
                    fprintf(stderr, "vdr: invalid DVB device number: %s\n", optarg);
                    return 2;
                    break;
          case 'E': cSIProcessor::SetEpgDataFileName(*optarg != '-' ? optarg : NULL);
                    break;
          case 'h': DisplayHelp = true;
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
          case 'L': if (access(optarg, R_OK | X_OK) == 0)
                       PluginManager.SetDirectory(optarg);
                    else {
                       fprintf(stderr, "vdr: can't access plugin directory: %s\n", optarg);
                       return 2;
                       }
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
          case 'P': PluginManager.AddPlugin(optarg);
                    break;
          case 'r': cRecordingUserCommand::SetCommand(optarg);
                    break;
          case 's': Shutdown = optarg;
                    break;
          case 't': Terminal = optarg;
                    break;
          case 'V': DisplayVersion = true;
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

  // Help and version info:

  if (DisplayHelp || DisplayVersion) {
     if (!PluginManager.HasPlugins())
        PluginManager.AddPlugin("*"); // adds all available plugins
     PluginManager.LoadPlugins();
     if (DisplayHelp) {
        printf("Usage: vdr [OPTIONS]\n\n"          // for easier orientation, this is column 80|
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
               "  -L DIR,   --lib=DIR      search for plugins in DIR (default is %s)\n"
               "  -m,       --mute         mute audio of the primary DVB device at startup\n"
               "  -p PORT,  --port=PORT    use PORT for SVDRP (default: %d)\n"
               "                           0 turns off SVDRP\n"
               "  -P OPT,   --plugin=OPT   load a plugin defined by the given options\n"
               "  -r CMD,   --record=CMD   call CMD before and after a recording\n"
               "  -s CMD,   --shutdown=CMD call CMD to shutdown the computer\n"
               "  -t TTY,   --terminal=TTY controlling tty\n"
               "  -v DIR,   --video=DIR    use DIR as video directory (default: %s)\n"
               "  -V,       --version      print version information and exit\n"
               "  -w SEC,   --watchdog=SEC activate the watchdog timer with a timeout of SEC\n"
               "                           seconds (default: %d); '0' disables the watchdog\n"
               "\n",
               cSIProcessor::GetEpgDataFileName() ? cSIProcessor::GetEpgDataFileName() : "'-'",
               DEFAULTPLUGINDIR,
               DEFAULTSVDRPPORT,
               VideoDirectory,
               DEFAULTWATCHDOG
               );
        }
     if (DisplayVersion)
        printf("vdr (%s) - The Video Disk Recorder\n", VDRVERSION);
     if (PluginManager.HasPlugins()) {
        if (DisplayHelp)
           printf("Plugins: vdr -P\"name [OPTIONS]\"\n\n");
        for (int i = 0; ; i++) {
            cPlugin *p = PluginManager.GetPlugin(i);
            if (p) {
               const char *help = p->CommandLineHelp();
               printf("%s (%s) - %s\n", p->Name(), p->Version(), p->Description());
               if (DisplayHelp && help) {
                  printf("\n");
                  puts(help);
                  }
               }
            else
               break;
            }
        }
     return 0;
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
        esyslog("ERROR: %m");
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

  isyslog("VDR version %s started", VDRVERSION);

  // Load plugins:

  if (!PluginManager.LoadPlugins(true))
     return 2;

  // Configuration data:

  if (!ConfigDirectory)
     ConfigDirectory = VideoDirectory;

  cPlugin::SetConfigDirectory(ConfigDirectory);

  Setup.Load(AddDirectory(ConfigDirectory, "setup.conf"));
  Sources.Load(AddDirectory(ConfigDirectory, "sources.conf"), true);
  Diseqcs.Load(AddDirectory(ConfigDirectory, "diseqc.conf"), true);
  Channels.Load(AddDirectory(ConfigDirectory, "channels.conf"));
  Timers.Load(AddDirectory(ConfigDirectory, "timers.conf"));
  Commands.Load(AddDirectory(ConfigDirectory, "commands.conf"), true);
  RecordingCommands.Load(AddDirectory(ConfigDirectory, "reccmds.conf"), true);
  SVDRPhosts.Load(AddDirectory(ConfigDirectory, "svdrphosts.conf"), true);
  CaDefinitions.Load(AddDirectory(ConfigDirectory, "ca.conf"), true);
  Keys.Load(AddDirectory(ConfigDirectory, "remote.conf"));

  // DVB interfaces:

  cDvbDevice::Initialize();

  cSIProcessor::Read();

  // Start plugins:

  if (!PluginManager.StartPlugins())
     return 2;

  // Primary device:

  cDevice::SetPrimaryDevice(Setup.PrimaryDVB);
  if (!cDevice::PrimaryDevice()) {
     const char *msg = "no primary device found - giving up!";
     fprintf(stderr, "vdr: %s\n", msg);
     esyslog("ERROR: %s", msg);
     return 2;
     }

  // OSD:

  cOsd::Initialize();

  // User interface:

  Interface = new cInterface(SVDRPport);

  // Remote Controls:
#if defined(REMOTE_RCU)
  new cRcuRemote("/dev/ttyS1");
#elif defined(REMOTE_LIRC)
  new cLircRemote("/dev/lircd");
#elif defined(REMOTE_KBD)
  new cKbdRemote;
#endif
  Interface->LearnKeys();

  // Channel:

  Channels.SwitchTo(Setup.CurrentChannel);
  if (MuteAudio)
     cDevice::PrimaryDevice()->ToggleMute();
  else
     cDevice::PrimaryDevice()->SetVolume(Setup.CurrentVolume, true);

  cEITScanner EITScanner;

  // Signal handlers:

  if (signal(SIGHUP,  SignalHandler) == SIG_IGN) signal(SIGHUP,  SIG_IGN);
  if (signal(SIGINT,  SignalHandler) == SIG_IGN) signal(SIGINT,  SIG_IGN);
  if (signal(SIGTERM, SignalHandler) == SIG_IGN) signal(SIGTERM, SIG_IGN);
  if (signal(SIGPIPE, SignalHandler) == SIG_IGN) signal(SIGPIPE, SIG_IGN);
  if (WatchdogTimeout > 0)
     if (signal(SIGALRM, Watchdog)   == SIG_IGN) signal(SIGALRM, SIG_IGN);

  // Main program loop:

  cOsdObject *Menu = NULL;
  cOsdObject *Temp = NULL;
  int LastChannel = -1;
  int PreviousChannel = cDevice::CurrentChannel();
  time_t LastActivity = 0;
  int MaxLatencyTime = 0;
  bool ForceShutdown = false;

  if (WatchdogTimeout > 0) {
     dsyslog("setting watchdog timer to %d seconds", WatchdogTimeout);
     alarm(WatchdogTimeout); // Initial watchdog timer start
     }

  while (!Interrupted) {
        // Handle emergency exits:
        if (cThread::EmergencyExit()) {
           esyslog("emergency exit requested - shutting down");
           break;
           }
        // Attach launched player control:
        cControl::Attach();
        // Make sure Transfer-Mode is re-started after detaching a player:
        if (cDevice::PrimaryDevice()->PlayerDetached() && !cDevice::PrimaryDevice()->Replaying())
           Channels.SwitchTo(cDevice::CurrentChannel());
        // Restart the Watchdog timer:
        if (WatchdogTimeout > 0) {
           int LatencyTime = WatchdogTimeout - alarm(WatchdogTimeout);
           if (LatencyTime > MaxLatencyTime) {
              MaxLatencyTime = LatencyTime;
              dsyslog("max. latency time %d seconds", MaxLatencyTime);
              }
           }
        // Channel display:
        if (!EITScanner.Active() && cDevice::CurrentChannel() != LastChannel) {
           if (!Menu)
              Menu = Temp = new cDisplayChannel(cDevice::CurrentChannel(), LastChannel > 0);
           if (LastChannel > 0)
              PreviousChannel = LastChannel;
           LastChannel = cDevice::CurrentChannel();
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
        cOsdObject *Interact = Menu ? Menu : cControl::Control();
        eKeys key = Interface->GetKey(!Interact || !Interact->NeedsFastResponse());
        if (NORMALKEY(key) != kNone) {
           EITScanner.Activity();
           LastActivity = time(NULL);
           }
        // Keys that must work independent of any interactive mode:
        switch (key) {
          // Menu control:
          case kMenu:
               if (Menu) {
                  DELETENULL(Menu);
                  if (!Temp)
                     break;
                  }
               if (cControl::Control())
                  cControl::Control()->Hide();
               Menu = new cMenuMain(cControl::Control());
               Temp = NULL;
               break;
          // Volume Control:
          case kVolUp|k_Repeat:
          case kVolUp:
          case kVolDn|k_Repeat:
          case kVolDn:
          case kMute:
               if (key == kMute) {
                  if (!cDevice::PrimaryDevice()->ToggleMute() && !Menu)
                     break; // no need to display "mute off"
                  }
               else
                  cDevice::PrimaryDevice()->SetVolume(NORMALKEY(key) == kVolDn ? -VOLUMEDELTA : VOLUMEDELTA);
               if (!Interface->IsOpen())
                  Menu = Temp = cDisplayVolume::Create();
               cDisplayVolume::Process(key);
               break;
          // Power off:
          case kPower: isyslog("Power button pressed");
                       DELETENULL(Menu);
                       cControl::Shutdown();
                       Temp = NULL;
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
            if (Interact) {
               switch (Interact->ProcessKey(key)) {
                 case osRecord: DELETENULL(Menu);
                                Temp = NULL;
                                if (!cRecordControls::Start())
                                   Interface->Error(tr("No free DVB device to record!"));
                                break;
                 case osRecordings:
                                DELETENULL(Menu);
                                cControl::Shutdown();
                                Temp = NULL;
                                Menu = new cMenuMain(false, osRecordings);
                                break;
                 case osReplay: DELETENULL(Menu);
                                cControl::Shutdown();
                                Temp = NULL;
                                cControl::Launch(new cReplayControl);
                                break;
                 case osStopReplay:
                                DELETENULL(Menu);
                                cControl::Shutdown();
                                Temp = NULL;
                                break;
                 case osSwitchDvb:
                                DELETENULL(Menu);
                                cControl::Shutdown();
                                Temp = NULL;
                                Interface->Info(tr("Switching primary DVB..."));
                                cDevice::SetPrimaryDevice(Setup.PrimaryDVB);
                                break;
                 case osBack:
                 case osEnd:    if (Interact == Menu)
                                   DELETENULL(Menu);
                                else
                                   cControl::Shutdown();
                                Temp = NULL;
                                break;
                 default:       ;
                 }
               }
            else {
               // Key functions in "normal" viewing mode:
               switch (key) {
                 // Toggle channels:
                 case k0: {
                      int CurrentChannel = cDevice::CurrentChannel();
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
                 case kDown:
                      cDevice::SwitchChannel(NORMALKEY(key) == kUp ? 1 : -1);
                      break;
                 // Viewing Control:
                 case kOk:   LastChannel = -1; break; // forces channel display
                 default:    break;
                 }
               }
          }
        if (!Menu) {
           EITScanner.Process();
           if (!cCutter::Active() && cCutter::Ended()) {
              if (cCutter::Error())
                 Interface->Error(tr("Editing process failed!"));
              else
                 Interface->Info(tr("Editing process finished"));
              }
           }
        if (!Interact && ((!cRecordControls::Active() && !cCutter::Active()) || ForceShutdown)) {
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
                       dsyslog("assuming manual start of VDR");
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
                    free(buf);
                    }
                 if (!Next || Delta > Setup.MinEventTimeout * 60 || ForceShutdown) {
                    ForceShutdown = false;
                    if (timer)
                       dsyslog("next timer event at %s", ctime(&Next));
                    if (WatchdogTimeout > 0)
                       signal(SIGALRM, SIG_IGN);
                    if (Interface->Confirm(tr("Press any key to cancel shutdown"), UserShutdown ? 5 : SHUTDOWNWAIT, true)) {
                       int Channel = timer ? timer->channel : 0;
                       const char *File = timer ? timer->file : "";
                       char *cmd;
                       asprintf(&cmd, "%s %ld %ld %d \"%s\" %d", Shutdown, Next, Delta, Channel, strescape(File, "\"$"), UserShutdown);
                       isyslog("executing '%s'", cmd);
                       SystemExec(cmd);
                       free(cmd);
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
              // Plugins housekeeping:
              PluginManager.Housekeeping();
              }
           }
        }
  if (Interrupted)
     isyslog("caught signal %d", Interrupted);
  cRecordControls::Shutdown();
  cCutter::Stop();
  delete Menu;
  cControl::Shutdown();
  delete Interface;
  cOsd::Shutdown();
  Remotes.Clear();
  Setup.CurrentChannel = cDevice::CurrentChannel();
  Setup.CurrentVolume  = cDevice::CurrentVolume();
  Setup.Save();
  cDevice::Shutdown();
  PluginManager.Shutdown(true);
  if (WatchdogTimeout > 0)
     dsyslog("max. latency time %d seconds", MaxLatencyTime);
  isyslog("exiting");
  if (SysLogLevel > 0)
     closelog();
  if (cThread::EmergencyExit()) {
     esyslog("emergency exit!");
     return 1;
     }
  return 0;
}
