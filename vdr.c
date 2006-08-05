/*
 * vdr.c: Video Disk Recorder main program
 *
 * Copyright (C) 2000, 2003, 2006 Klaus Schmidinger
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 * The author can be reached at kls@cadsoft.de
 *
 * The project's page is at http://www.cadsoft.de/vdr
 *
 * $Id: vdr.c 1.278 2006/08/05 10:46:38 kls Exp $
 */

#include <getopt.h>
#include <grp.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <termios.h>
#include <unistd.h>
#include "audio.h"
#include "channels.h"
#include "config.h"
#include "cutter.h"
#include "device.h"
#include "diseqc.h"
#include "dvbdevice.h"
#include "eitscan.h"
#include "epg.h"
#include "i18n.h"
#include "interface.h"
#include "keys.h"
#include "libsi/si.h"
#include "lirc.h"
#include "menu.h"
#include "osdbase.h"
#include "plugin.h"
#include "rcu.h"
#include "recording.h"
#include "skinclassic.h"
#include "skinsttng.h"
#include "sources.h"
#include "themes.h"
#include "timers.h"
#include "tools.h"
#include "transfer.h"
#include "videodir.h"

#define MINCHANNELWAIT     10 // seconds to wait between failed channel switchings
#define ACTIVITYTIMEOUT    60 // seconds before starting housekeeping
#define SHUTDOWNWAIT      300 // seconds to wait in user prompt before automatic shutdown
#define MANUALSTART       600 // seconds the next timer must be in the future to assume manual start
#define CHANNELSAVEDELTA  600 // seconds before saving channels.conf after automatic modifications
#define LASTCAMMENUTIMEOUT  3 // seconds to run the main loop 'fast' after a CAM menu has been closed
                              // in order to react on a possible new CAM menu as soon as possible
#define DEVICEREADYTIMEOUT 30 // seconds to wait until all devices are ready
#define MENUTIMEOUT       120 // seconds of user inactivity after which an OSD display is closed
#define SHUTDOWNRETRY     300 // seconds before trying again to shut down
#define TIMERCHECKDELTA    10 // seconds between checks for timers that need to see their channel
#define TIMERDEVICETIMEOUT  8 // seconds before a device used for timer check may be reused
#define TIMERLOOKAHEADTIME 60 // seconds before a non-VPS timer starts and the channel is switched if possible
#define VPSLOOKAHEADTIME   24 // hours within which VPS timers will make sure their events are up to date
#define VPSUPTODATETIME  3600 // seconds before the event or schedule of a VPS timer needs to be refreshed

#define EXIT(v) { ExitCode = (v); goto Exit; }

static int Interrupted = 0;

static bool SetUser(const char *UserName)
{
  if (UserName) {
     struct passwd *user = getpwnam(UserName);
     if (!user) {
        fprintf(stderr, "vdr: unknown user: '%s'\n", UserName);
        return false;
        }
     if (setgid(user->pw_gid) < 0) {
        fprintf(stderr, "vdr: cannot set group id %u: %s\n", (unsigned int)user->pw_gid, strerror(errno));
        return false;
        }
     if (initgroups(user->pw_name, user->pw_gid) < 0) {
        fprintf(stderr, "vdr: cannot set supplemental group ids for user %s: %s\n", user->pw_name, strerror(errno));
        return false;
        }
     if (setuid(user->pw_uid) < 0) {
        fprintf(stderr, "vdr: cannot set user id %u: %s\n", (unsigned int)user->pw_uid, strerror(errno));
        return false;
        }
     if (prctl(PR_SET_DUMPABLE, 2, 0, 0, 0) < 0) {
        fprintf(stderr, "vdr: warning - cannot set dumpable: %s\n", strerror(errno));
        // always non-fatal, and will not work with kernel < 2.6.13
        }
     }
  return true;
}

static bool SetCapSysTime(void)
{
  // drop all capabilities except cap_sys_time
  cap_t caps = cap_from_text("= cap_sys_time=ep");
  if (!caps) {
     fprintf(stderr, "vdr: cap_from_text failed: %s\n", strerror(errno));
     return false;
     }
  if (cap_set_proc(caps) == -1) {
     fprintf(stderr, "vdr: cap_set_proc failed: %s\n", strerror(errno));
     cap_free(caps);
     return false;
     }
  cap_free(caps);
  return true;
}

static bool SetKeepCaps(bool On)
{
  // set keeping capabilities during setuid() on/off
  if (prctl(PR_SET_KEEPCAPS, On ? 1 : 0, 0, 0, 0) != 0) {
     fprintf(stderr, "vdr: prctl failed\n");
     return false;
     }
  return true;
}

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
  // Save terminal settings:

  struct termios savedTm;
  bool HasStdin = (tcgetpgrp(STDIN_FILENO) == getpid() || getppid() != (pid_t)1) && tcgetattr(STDIN_FILENO, &savedTm) == 0;

  // Initiate locale:

  setlocale(LC_ALL, "");

  // Command line options:

#define DEFAULTSVDRPPORT 2001
#define DEFAULTWATCHDOG     0 // seconds
#define DEFAULTPLUGINDIR PLUGINDIR
#define DEFAULTEPGDATAFILENAME "epg.data"

  bool StartedAsRoot = false;
  const char *VdrUser = NULL;
  int SVDRPport = DEFAULTSVDRPPORT;
  const char *AudioCommand = NULL;
  const char *ConfigDirectory = NULL;
  const char *EpgDataFileName = DEFAULTEPGDATAFILENAME;
  bool DisplayHelp = false;
  bool DisplayVersion = false;
  bool DaemonMode = false;
  int SysLogTarget = LOG_USER;
  bool MuteAudio = false;
  int WatchdogTimeout = DEFAULTWATCHDOG;
  const char *Terminal = NULL;
  const char *Shutdown = NULL;

  bool UseKbd = true;
  const char *LircDevice = NULL;
  const char *RcuDevice = NULL;
#if !defined(REMOTE_KBD)
  UseKbd = false;
#endif
#if defined(REMOTE_LIRC)
  LircDevice = LIRC_DEVICE;
#elif defined(REMOTE_RCU)
  RcuDevice = RCU_DEVICE;
#endif
#if defined(VFAT)
  VfatFileSystem = true;
#endif
#if defined(VDR_USER)
  VdrUser = VDR_USER;
#endif

  cPluginManager PluginManager(DEFAULTPLUGINDIR);
  int ExitCode = 0;

  static struct option long_options[] = {
      { "audio",    required_argument, NULL, 'a' },
      { "config",   required_argument, NULL, 'c' },
      { "daemon",   no_argument,       NULL, 'd' },
      { "device",   required_argument, NULL, 'D' },
      { "epgfile",  required_argument, NULL, 'E' },
      { "grab",     required_argument, NULL, 'g' },
      { "help",     no_argument,       NULL, 'h' },
      { "lib",      required_argument, NULL, 'L' },
      { "lirc",     optional_argument, NULL, 'l' | 0x100 },
      { "log",      required_argument, NULL, 'l' },
      { "mute",     no_argument,       NULL, 'm' },
      { "no-kbd",   no_argument,       NULL, 'n' | 0x100 },
      { "plugin",   required_argument, NULL, 'P' },
      { "port",     required_argument, NULL, 'p' },
      { "rcu",      optional_argument, NULL, 'r' | 0x100 },
      { "record",   required_argument, NULL, 'r' },
      { "shutdown", required_argument, NULL, 's' },
      { "terminal", required_argument, NULL, 't' },
      { "user",     required_argument, NULL, 'u' },
      { "version",  no_argument,       NULL, 'V' },
      { "vfat",     no_argument,       NULL, 'v' | 0x100 },
      { "video",    required_argument, NULL, 'v' },
      { "watchdog", required_argument, NULL, 'w' },
      { NULL }
    };

  int c;
  while ((c = getopt_long(argc, argv, "a:c:dD:E:g:hl:L:mp:P:r:s:t:u:v:Vw:", long_options, NULL)) != -1) {
        switch (c) {
          case 'a': AudioCommand = optarg;
                    break;
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
          case 'E': EpgDataFileName = (*optarg != '-' ? optarg : NULL);
                    break;
          case 'g': cSVDRP::SetGrabImageDir(*optarg != '-' ? optarg : NULL);
                    break;
          case 'h': DisplayHelp = true;
                    break;
          case 'l': {
                      char *p = strchr(optarg, '.');
                      if (p)
                         *p = 0;
                      if (isnumber(optarg)) {
                         int l = atoi(optarg);
                         if (0 <= l && l <= 3) {
                            SysLogLevel = l;
                            if (!p)
                               break;
                            if (isnumber(p + 1)) {
                               int l = atoi(p + 1);
                               if (0 <= l && l <= 7) {
                                  int targets[] = { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7 };
                                  SysLogTarget = targets[l];
                                  break;
                                  }
                               }
                            }
                         }
                    if (p)
                       *p = '.';
                    fprintf(stderr, "vdr: invalid log level: %s\n", optarg);
                    return 2;
                    }
                    break;
          case 'L': if (access(optarg, R_OK | X_OK) == 0)
                       PluginManager.SetDirectory(optarg);
                    else {
                       fprintf(stderr, "vdr: can't access plugin directory: %s\n", optarg);
                       return 2;
                       }
                    break;
          case 'l' | 0x100:
                    LircDevice = optarg ? : LIRC_DEVICE;
                    break;
          case 'm': MuteAudio = true;
                    break;
          case 'n' | 0x100:
                    UseKbd = false;
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
          case 'r' | 0x100:
                    RcuDevice = optarg ? : RCU_DEVICE;
                    break;
          case 'r': cRecordingUserCommand::SetCommand(optarg);
                    break;
          case 's': Shutdown = optarg;
                    break;
          case 't': Terminal = optarg;
                    if (access(Terminal, R_OK | W_OK) < 0) {
                       fprintf(stderr, "vdr: can't access terminal: %s\n", Terminal);
                       return 2;
                       }
                    break;
          case 'u': if (*optarg)
                       VdrUser = optarg;
                    break;
          case 'V': DisplayVersion = true;
                    break;
          case 'v' | 0x100:
                    VfatFileSystem = true;
                    break;
          case 'v': VideoDirectory = optarg;
                    while (optarg && *optarg && optarg[strlen(optarg) - 1] == '/')
                          optarg[strlen(optarg) - 1] = 0;
                    break;
          case 'w': if (isnumber(optarg)) { int t = atoi(optarg);
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

  // Set user id in case we were started as root:

  if (VdrUser && geteuid() == 0) {
     StartedAsRoot = true;
     if (strcmp(VdrUser, "root")) {
        if (!SetKeepCaps(true))
           return 2;
        if (!SetUser(VdrUser))
           return 2;
        if (!SetKeepCaps(false))
           return 2;
        if (!SetCapSysTime())
           return 2;
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
               "  -E FILE,  --epgfile=FILE write the EPG data into the given FILE (default is\n"
               "                           '%s' in the video directory)\n"
               "                           '-E-' disables this\n"
               "                           if FILE is a directory, the default EPG file will be\n"
               "                           created in that directory\n"
               "  -g DIR,   --grab=DIR     write images from the SVDRP command GRAB into the\n"
               "                           given DIR; DIR must be the full path name of an\n"
               "                           existing directory, without any \"..\", double '/'\n"
               "                           or symlinks (default: none, same as -g-)\n"
               "  -h,       --help         print this help and exit\n"
               "  -l LEVEL, --log=LEVEL    set log level (default: 3)\n"
               "                           0 = no logging, 1 = errors only,\n"
               "                           2 = errors and info, 3 = errors, info and debug\n"
               "                           if logging should be done to LOG_LOCALn instead of\n"
               "                           LOG_USER, add '.n' to LEVEL, as in 3.7 (n=0..7)\n"
               "  -L DIR,   --lib=DIR      search for plugins in DIR (default is %s)\n"
               "            --lirc[=PATH]  use a LIRC remote control device, attached to PATH\n"
               "                           (default: %s)\n"
               "  -m,       --mute         mute audio of the primary DVB device at startup\n"
               "            --no-kbd       don't use the keyboard as an input device\n"
               "  -p PORT,  --port=PORT    use PORT for SVDRP (default: %d)\n"
               "                           0 turns off SVDRP\n"
               "  -P OPT,   --plugin=OPT   load a plugin defined by the given options\n"
               "            --rcu[=PATH]   use a remote control device, attached to PATH\n"
               "                           (default: %s)\n"
               "  -r CMD,   --record=CMD   call CMD before and after a recording\n"
               "  -s CMD,   --shutdown=CMD call CMD to shutdown the computer\n"
               "  -t TTY,   --terminal=TTY controlling tty\n"
               "  -u USER,  --user=USER    run as user USER; only applicable if started as\n"
               "                           root\n"
               "  -v DIR,   --video=DIR    use DIR as video directory (default: %s)\n"
               "  -V,       --version      print version information and exit\n"
               "            --vfat         encode special characters in recording names to\n"
               "                           avoid problems with VFAT file systems\n"
               "  -w SEC,   --watchdog=SEC activate the watchdog timer with a timeout of SEC\n"
               "                           seconds (default: %d); '0' disables the watchdog\n"
               "\n",
               DEFAULTEPGDATAFILENAME,
               DEFAULTPLUGINDIR,
               LIRC_DEVICE,
               DEFAULTSVDRPPORT,
               RCU_DEVICE,
               VideoDirectory,
               DEFAULTWATCHDOG
               );
        }
     if (DisplayVersion)
        printf("vdr (%s/%s) - The Video Disk Recorder\n", VDRVERSION, APIVERSION);
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

  // Check for UTF-8 and exit if present - asprintf() will fail if it encounters 8 bit ASCII codes
  char *LangEnv;
  if ((LangEnv = getenv("LANG"))     != NULL && strcasestr(LangEnv, "utf") ||
      (LangEnv = getenv("LC_CTYPE")) != NULL && strcasestr(LangEnv, "utf")) {
     fprintf(stderr, "vdr: please turn off UTF-8 before starting VDR\n");
     return 2;
     }

  // Log file:

  if (SysLogLevel > 0)
     openlog("vdr", LOG_CONS, SysLogTarget); // LOG_PID doesn't work as expected under NPTL

  // Check the video directory:

  if (!DirectoryOk(VideoDirectory, true)) {
     fprintf(stderr, "vdr: can't access video directory %s\n", VideoDirectory);
     return 2;
     }

  // Daemon mode:

  if (DaemonMode) {
     if (daemon(1, 0) == -1) {
        fprintf(stderr, "vdr: %m\n");
        esyslog("ERROR: %m");
        return 2;
        }
     }
  else if (Terminal) {
     // Claim new controlling terminal
     stdin  = freopen(Terminal, "r", stdin);
     stdout = freopen(Terminal, "w", stdout);
     stderr = freopen(Terminal, "w", stderr);
     HasStdin = true;
     }

  isyslog("VDR version %s started", VDRVERSION);
  if (StartedAsRoot && VdrUser)
     isyslog("switched to user '%s'", VdrUser);
  if (DaemonMode)
     dsyslog("running as daemon (tid=%d)", cThread::ThreadId());
  cThread::SetMainThreadId();

  // Main program loop variables - need to be here to have them initialized before any EXIT():

  cOsdObject *Menu = NULL;
  int LastChannel = 0;
  int LastTimerChannel = -1;
  int PreviousChannel[2] = { 1, 1 };
  int PreviousChannelIndex = 0;
  time_t LastChannelChanged = time(NULL);
  time_t LastActivity = 0;
  time_t LastCamMenu = 0;
  int MaxLatencyTime = 0;
  bool ForceShutdown = false;
  bool UserShutdown = false;
  bool InhibitEpgScan = false;
  bool IsInfoMenu = false;
  cSkin *CurrentSkin = NULL;

  // Load plugins:

  if (!PluginManager.LoadPlugins(true))
     EXIT(2);

  // Configuration data:

  if (!ConfigDirectory)
     ConfigDirectory = VideoDirectory;

  cPlugin::SetConfigDirectory(ConfigDirectory);
  cThemes::SetThemesDirectory(AddDirectory(ConfigDirectory, "themes"));

  Setup.Load(AddDirectory(ConfigDirectory, "setup.conf"));
  if (!(Sources.Load(AddDirectory(ConfigDirectory, "sources.conf"), true, true) &&
        Diseqcs.Load(AddDirectory(ConfigDirectory, "diseqc.conf"), true, Setup.DiSEqC) &&
        Channels.Load(AddDirectory(ConfigDirectory, "channels.conf"), false, true) &&
        Timers.Load(AddDirectory(ConfigDirectory, "timers.conf")) &&
        Commands.Load(AddDirectory(ConfigDirectory, "commands.conf"), true) &&
        RecordingCommands.Load(AddDirectory(ConfigDirectory, "reccmds.conf"), true) &&
        SVDRPhosts.Load(AddDirectory(ConfigDirectory, "svdrphosts.conf"), true) &&
        Keys.Load(AddDirectory(ConfigDirectory, "remote.conf")) &&
        KeyMacros.Load(AddDirectory(ConfigDirectory, "keymacros.conf"), true)
        ))
     EXIT(2);

  cFont::SetCode(I18nCharSets()[Setup.OSDLanguage]);

  // Recordings:

  Recordings.Update();
  DeletedRecordings.Update();

  // EPG data:

  if (EpgDataFileName) {
     const char *EpgDirectory = NULL;
     if (DirectoryOk(EpgDataFileName)) {
        EpgDirectory = EpgDataFileName;
        EpgDataFileName = DEFAULTEPGDATAFILENAME;
        }
     else if (*EpgDataFileName != '/' && *EpgDataFileName != '.')
        EpgDirectory = VideoDirectory;
     if (EpgDirectory)
        cSchedules::SetEpgDataFileName(AddDirectory(EpgDirectory, EpgDataFileName));
     else
        cSchedules::SetEpgDataFileName(EpgDataFileName);
     cSchedules::Read();
     }

  // DVB interfaces:

  cDvbDevice::Initialize();

  // Initialize plugins:

  if (!PluginManager.InitializePlugins())
     EXIT(2);

  // Primary device:

  cDevice::SetPrimaryDevice(Setup.PrimaryDVB);
  if (!cDevice::PrimaryDevice() || !cDevice::PrimaryDevice()->HasDecoder()) {
     if (cDevice::PrimaryDevice() && !cDevice::PrimaryDevice()->HasDecoder())
        isyslog("device %d has no MPEG decoder", cDevice::PrimaryDevice()->DeviceNumber() + 1);
     for (int i = 0; i < cDevice::NumDevices(); i++) {
         cDevice *d = cDevice::GetDevice(i);
         if (d && d->HasDecoder()) {
            isyslog("trying device number %d instead", i + 1);
            if (cDevice::SetPrimaryDevice(i + 1)) {
               Setup.PrimaryDVB = i + 1;
               break;
               }
            }
         }
     if (!cDevice::PrimaryDevice()) {
        const char *msg = "no primary device found - using first device!";
        fprintf(stderr, "vdr: %s\n", msg);
        esyslog("ERROR: %s", msg);
        if (!cDevice::SetPrimaryDevice(1))
           EXIT(2);
        if (!cDevice::PrimaryDevice()) {
           const char *msg = "no primary device found - giving up!";
           fprintf(stderr, "vdr: %s\n", msg);
           esyslog("ERROR: %s", msg);
           EXIT(2);
           }
        }
     }

  // User interface:

  Interface = new cInterface(SVDRPport);

  // Default skins:

  new cSkinClassic;
  new cSkinSTTNG;
  Skins.SetCurrent(Setup.OSDSkin);
  cThemes::Load(Skins.Current()->Name(), Setup.OSDTheme, Skins.Current()->Theme());
  CurrentSkin = Skins.Current();

  // Start plugins:

  if (!PluginManager.StartPlugins())
     EXIT(2);

  // Set skin and theme in case they're implemented by a plugin:

  if (!CurrentSkin || CurrentSkin == Skins.Current() && strcmp(Skins.Current()->Name(), Setup.OSDSkin) != 0) {
     Skins.SetCurrent(Setup.OSDSkin);
     cThemes::Load(Skins.Current()->Name(), Setup.OSDTheme, Skins.Current()->Theme());
     }

  // Remote Controls:
  if (RcuDevice)
     new cRcuRemote(RcuDevice);
  if (LircDevice)
     new cLircRemote(LircDevice);
  if (!DaemonMode && HasStdin && UseKbd)
     new cKbdRemote;
  Interface->LearnKeys();

  // External audio:

  if (AudioCommand)
     new cExternalAudio(AudioCommand);

  // Channel:

  if (!cDevice::WaitForAllDevicesReady(DEVICEREADYTIMEOUT))
     dsyslog("not all devices ready after %d seconds", DEVICEREADYTIMEOUT);
  if (Setup.InitialChannel > 0)
     Setup.CurrentChannel = Setup.InitialChannel;
  if (Setup.InitialVolume >= 0)
     Setup.CurrentVolume = Setup.InitialVolume;
  Channels.SwitchTo(Setup.CurrentChannel);
  if (MuteAudio)
     cDevice::PrimaryDevice()->ToggleMute();
  else
     cDevice::PrimaryDevice()->SetVolume(Setup.CurrentVolume, true);

  // Signal handlers:

  if (signal(SIGHUP,  SignalHandler) == SIG_IGN) signal(SIGHUP,  SIG_IGN);
  if (signal(SIGINT,  SignalHandler) == SIG_IGN) signal(SIGINT,  SIG_IGN);
  if (signal(SIGTERM, SignalHandler) == SIG_IGN) signal(SIGTERM, SIG_IGN);
  if (signal(SIGPIPE, SignalHandler) == SIG_IGN) signal(SIGPIPE, SIG_IGN);
  if (WatchdogTimeout > 0)
     if (signal(SIGALRM, Watchdog)   == SIG_IGN) signal(SIGALRM, SIG_IGN);

  // Watchdog:

  if (WatchdogTimeout > 0) {
     dsyslog("setting watchdog timer to %d seconds", WatchdogTimeout);
     alarm(WatchdogTimeout); // Initial watchdog timer start
     }

  // Main program loop:

#define DELETE_MENU ((IsInfoMenu &= (Menu == NULL)), delete Menu, Menu = NULL)

  while (!Interrupted) {
        // Handle emergency exits:
        if (cThread::EmergencyExit()) {
           esyslog("emergency exit requested - shutting down");
           break;
           }
#ifdef DEBUGRINGBUFFERS
        cRingBufferLinear::PrintDebugRBL();
#endif
        // Attach launched player control:
        cControl::Attach();
        // Make sure we have a visible programme in case device usage has changed:
        if (!EITScanner.Active() && cDevice::PrimaryDevice()->HasDecoder() && !cDevice::PrimaryDevice()->HasProgramme()) {
           static time_t lastTime = 0;
           if (time(NULL) - lastTime > MINCHANNELWAIT) {
              cChannel *Channel = Channels.GetByNumber(cDevice::CurrentChannel());
              if (Channel && (Channel->Vpid() || Channel->Apid(0))) {
                 if (!Channels.SwitchTo(cDevice::CurrentChannel()) // try to switch to the original channel...
                     && !(LastTimerChannel > 0 && Channels.SwitchTo(LastTimerChannel)) // ...or the one used by the last timer...
                     && !cDevice::SwitchChannel(1) // ...or the next higher available one...
                     && !cDevice::SwitchChannel(-1)) // ...or the next lower available one
                    ;
                 }
              lastTime = time(NULL); // don't do this too often
              LastTimerChannel = -1;
              }
           }
        // Restart the Watchdog timer:
        if (WatchdogTimeout > 0) {
           int LatencyTime = WatchdogTimeout - alarm(WatchdogTimeout);
           if (LatencyTime > MaxLatencyTime) {
              MaxLatencyTime = LatencyTime;
              dsyslog("max. latency time %d seconds", MaxLatencyTime);
              }
           }
        // Handle channel and timer modifications:
        if (!Channels.BeingEdited() && !Timers.BeingEdited()) {
           int modified = Channels.Modified();
           static time_t ChannelSaveTimeout = 0;
           static int TimerState = 0;
           // Channels and timers need to be stored in a consistent manner,
           // therefore if one of them is changed, we save both.
           if (modified == CHANNELSMOD_USER || Timers.Modified(TimerState))
              ChannelSaveTimeout = 1; // triggers an immediate save
           else if (modified && !ChannelSaveTimeout)
              ChannelSaveTimeout = time(NULL) + CHANNELSAVEDELTA;
           bool timeout = ChannelSaveTimeout == 1 || ChannelSaveTimeout && time(NULL) > ChannelSaveTimeout && !cRecordControls::Active();
           if ((modified || timeout) && Channels.Lock(false, 100)) {
              if (timeout) {
                 Channels.Save();
                 Timers.Save();
                 ChannelSaveTimeout = 0;
                 }
              for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                  if (Channel->Modification(CHANNELMOD_RETUNE)) {
                     cRecordControls::ChannelDataModified(Channel);
                     if (Channel->Number() == cDevice::CurrentChannel()) {
                        if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring()) {
                           if (cDevice::ActualDevice()->ProvidesTransponder(Channel)) { // avoids retune on devices that don't really access the transponder
                              isyslog("retuning due to modification of channel %d", Channel->Number());
                              Channels.SwitchTo(Channel->Number());
                              }
                           }
                        }
                     }
                  }
              Channels.Unlock();
              }
           }
        // Channel display:
        if (!EITScanner.Active() && cDevice::CurrentChannel() != LastChannel) {
           if (!Menu)
              Menu = new cDisplayChannel(cDevice::CurrentChannel(), LastChannel >= 0);
           LastChannel = cDevice::CurrentChannel();
           LastChannelChanged = time(NULL);
           }
        if (time(NULL) - LastChannelChanged >= Setup.ZapTimeout && LastChannel != PreviousChannel[PreviousChannelIndex])
           PreviousChannel[PreviousChannelIndex ^= 1] = LastChannel;
        // Timers and Recordings:
        if (!Timers.BeingEdited()) {
           // Assign events to timers:
           Timers.SetEvents();
           // Must do all following calls with the exact same time!
           time_t Now = time(NULL);
           // Process ongoing recordings:
           cRecordControls::Process(Now);
           // Start new recordings:
           cTimer *Timer = Timers.GetMatch(Now);
           if (Timer) {
              if (!cRecordControls::Start(Timer))
                 Timer->SetPending(true);
              else
                 LastTimerChannel = Timer->Channel()->Number();
              }
           // Make sure timers "see" their channel early enough:
           static time_t LastTimerCheck = 0;
           if (Now - LastTimerCheck > TIMERCHECKDELTA) { // don't do this too often
              InhibitEpgScan = false;
              static time_t DeviceUsed[MAXDEVICES] = { 0 };
              for (cTimer *Timer = Timers.First(); Timer; Timer = Timers.Next(Timer)) {
                  bool InVpsMargin = false;
                  bool NeedsTransponder = false;
                  if (Timer->HasFlags(tfActive) && !Timer->Recording()) {
                     if (Timer->HasFlags(tfVps)) {
                        if (Timer->Matches(Now, true, Setup.VpsMargin)) {
                           InVpsMargin = true;
                           Timer->SetInVpsMargin(InVpsMargin);
                           }
                        else if (Timer->Event()) {
                           InVpsMargin = Timer->Event()->StartTime() <= Now && Timer->Event()->RunningStatus() == SI::RunningStatusUndefined;
                           NeedsTransponder = Timer->Event()->StartTime() - Now < VPSLOOKAHEADTIME * 3600 && !Timer->Event()->SeenWithin(VPSUPTODATETIME);
                           }
                        else {
                           cSchedulesLock SchedulesLock;
                           const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
                           if (Schedules) {
                              const cSchedule *Schedule = Schedules->GetSchedule(Timer->Channel());
                              InVpsMargin = !Schedule; // we must make sure we have the schedule
                              NeedsTransponder = Schedule && !Schedule->PresentSeenWithin(VPSUPTODATETIME);
                              }
                           }
                        InhibitEpgScan |= InVpsMargin | NeedsTransponder;
                        }
                     else
                        NeedsTransponder = Timer->Matches(Now, true, TIMERLOOKAHEADTIME);
                     }
                  if (NeedsTransponder || InVpsMargin) {
                     // Find a device that provides the required transponder:
                     cDevice *Device = NULL;
                     bool DeviceAvailable = false;
                     for (int i = 0; i < cDevice::NumDevices(); i++) {
                         cDevice *d = cDevice::GetDevice(i);
                         if (d && d->ProvidesTransponder(Timer->Channel())) {
                            if (d->IsTunedToTransponder(Timer->Channel())) {
                               // if any device is tuned to the transponder, we're done
                               Device = d;
                               break;
                               }
                            bool timeout = Now - DeviceUsed[d->DeviceNumber()] > TIMERDEVICETIMEOUT; // only check other devices if they have been left alone for a while
                            if (d->MaySwitchTransponder()) {
                               DeviceAvailable = true; // avoids using the actual device below
                               if (timeout)
                                  Device = d; // only check other devices if they have been left alone for a while
                               }
                            else if (timeout && !Device && InVpsMargin && !d->Receiving() && d->ProvidesTransponderExclusively(Timer->Channel()))
                               Device = d; // use this one only if no other with less impact can be found
                            }
                         }
                     if (!Device && InVpsMargin && !DeviceAvailable) {
                        cDevice *d = cDevice::ActualDevice();
                        if (!d->Receiving() && d->ProvidesTransponder(Timer->Channel()) && Now - DeviceUsed[d->DeviceNumber()] > TIMERDEVICETIMEOUT)
                           Device = d; // use the actual device as a last resort
                        }
                     // Switch the device to the transponder:
                     if (Device) {
                        if (!Device->IsTunedToTransponder(Timer->Channel())) {
                           if (Device == cDevice::ActualDevice() && !Device->IsPrimaryDevice())
                              cDevice::PrimaryDevice()->StopReplay(); // stop transfer mode
                           dsyslog("switching device %d to channel %d", Device->DeviceNumber() + 1, Timer->Channel()->Number());
                           Device->SwitchChannel(Timer->Channel(), false);
                           DeviceUsed[Device->DeviceNumber()] = Now;
                           }
                        if (cDevice::PrimaryDevice()->HasDecoder() && !cDevice::PrimaryDevice()->HasProgramme()) {
                           // the previous SwitchChannel() has switched away the current live channel
                           Channels.SwitchTo(Timer->Channel()->Number()); // avoids toggling between old channel and black screen
                           Skins.Message(mtInfo, tr("Upcoming VPS recording!"));
                           }
                        }
                     }
                  }
              LastTimerCheck = time(NULL);
              }
           // Delete expired timers:
           Timers.DeleteExpired();
           }
        if (!Menu && Recordings.NeedsUpdate()) {
           Recordings.Update();
           DeletedRecordings.Update();
           }
        // CAM control:
        if (!Menu && !cOsd::IsOpen()) {
           Menu = CamControl();
           if (Menu)
              LastCamMenu = 0;
           else if (!LastCamMenu)
              LastCamMenu = time(NULL);
           }
        // Queued messages:
        if (!Skins.IsOpen())
           Skins.ProcessQueuedMessages();
        // User Input:
        cOsdObject *Interact = Menu ? Menu : cControl::Control();
        eKeys key = Interface->GetKey((!Interact || !Interact->NeedsFastResponse()) && time(NULL) - LastCamMenu > LASTCAMMENUTIMEOUT);
        if (NORMALKEY(key) != kNone) {
           EITScanner.Activity();
           LastActivity = time(NULL);
           }
        // Keys that must work independent of any interactive mode:
        switch (key) {
          // Menu control:
          case kMenu: {
               key = kNone; // nobody else needs to see this key
               bool WasOpen = Interact != NULL;
               bool WasMenu = Interact && Interact->IsMenu();
               if (Menu)
                  DELETE_MENU;
               else if (cControl::Control()) {
                  if (cOsd::IsOpen())
                     cControl::Control()->Hide();
                  else
                     WasOpen = false;
                  }
               if (!WasOpen || !WasMenu && !Setup.MenuButtonCloses)
                  Menu = new cMenuMain;
               }
               break;
          // Info:
          case kInfo: {
               bool WasInfoMenu = IsInfoMenu;
               DELETE_MENU;
               if (!WasInfoMenu) {
                  IsInfoMenu = true;
                  if (cControl::Control()) {
                     cControl::Control()->Hide();
                     Menu = cControl::Control()->GetInfo();
                     if (Menu)
                        Menu->Show();
                     else
                        IsInfoMenu = false;
                     }
                  else {
                     cRemote::Put(kOk, true);
                     cRemote::Put(kSchedule, true);
                     }
                  }
               }
               break;
          // Direct main menu functions:
          #define DirectMainFunction(function)\
            DELETE_MENU;\
            if (cControl::Control())\
               cControl::Control()->Hide();\
            Menu = new cMenuMain(function);\
            key = kNone; // nobody else needs to see this key
          case kSchedule:   DirectMainFunction(osSchedule); break;
          case kChannels:   DirectMainFunction(osChannels); break;
          case kTimers:     DirectMainFunction(osTimers); break;
          case kRecordings: DirectMainFunction(osRecordings); break;
          case kSetup:      DirectMainFunction(osSetup); break;
          case kCommands:   DirectMainFunction(osCommands); break;
          case kUser1 ... kUser9: cRemote::PutMacro(key); key = kNone; break;
          case k_Plugin: {
               DELETE_MENU;
               if (cControl::Control())
                  cControl::Control()->Hide();
               cPlugin *plugin = cPluginManager::GetPlugin(cRemote::GetPlugin());
               if (plugin) {
                  Menu = plugin->MainMenuAction();
                  if (Menu)
                     Menu->Show();
                  }
               else
                  esyslog("ERROR: unknown plugin '%s'", cRemote::GetPlugin());
               key = kNone; // nobody else needs to see these keys
               }
               break;
          // Channel up/down:
          case kChanUp|k_Repeat:
          case kChanUp:
          case kChanDn|k_Repeat:
          case kChanDn:
               if (!Interact)
                  Menu = new cDisplayChannel(NORMALKEY(key));
               else if (cDisplayChannel::IsOpen()) {
                  Interact->ProcessKey(key);
                  continue;
                  }
               else
                  cDevice::SwitchChannel(NORMALKEY(key) == kChanUp ? 1 : -1);
               key = kNone; // nobody else needs to see these keys
               break;
          // Volume control:
          case kVolUp|k_Repeat:
          case kVolUp:
          case kVolDn|k_Repeat:
          case kVolDn:
          case kMute:
               if (key == kMute) {
                  if (!cDevice::PrimaryDevice()->ToggleMute() && !Menu) {
                     key = kNone; // nobody else needs to see these keys
                     break; // no need to display "mute off"
                     }
                  }
               else
                  cDevice::PrimaryDevice()->SetVolume(NORMALKEY(key) == kVolDn ? -VOLUMEDELTA : VOLUMEDELTA);
               if (!Menu && !cOsd::IsOpen())
                  Menu = cDisplayVolume::Create();
               cDisplayVolume::Process(key);
               key = kNone; // nobody else needs to see these keys
               break;
          // Audio track control:
          case kAudio:
               if (cControl::Control())
                  cControl::Control()->Hide();
               if (!cDisplayTracks::IsOpen()) {
                  DELETE_MENU;
                  Menu = cDisplayTracks::Create();
                  }
               else
                  cDisplayTracks::Process(key);
               key = kNone;
               break;
          // Pausing live video:
          case kPause:
               if (!cControl::Control()) {
                  DELETE_MENU;
                  if (!cRecordControls::PauseLiveVideo())
                     Skins.Message(mtError, tr("No free DVB device to record!"));
                  key = kNone; // nobody else needs to see this key
                  }
               break;
          // Instant recording:
          case kRecord:
               if (!cControl::Control()) {
                  if (cRecordControls::Start())
                     Skins.Message(mtInfo, tr("Recording started"));
                  key = kNone; // nobody else needs to see this key
                  }
               break;
          // Power off:
          case kPower: {
               isyslog("Power button pressed");
               DELETE_MENU;
               if (!Shutdown) {
                  Skins.Message(mtError, tr("Can't shutdown - option '-s' not given!"));
                  break;
                  }
               LastActivity = 1; // not 0, see below!
               UserShutdown = true;
               if (cRecordControls::Active()) {
                  if (!Interface->Confirm(tr("Recording - shut down anyway?")))
                     break;
                  }
               if (cPluginManager::Active(tr("shut down anyway?")))
                  break;
               if (!cRecordControls::Active()) {
                  cTimer *timer = Timers.GetNextActiveTimer();
                  time_t Next  = timer ? timer->StartTime() : 0;
                  time_t Delta = timer ? Next - time(NULL) : 0;
                  if (Next && Delta <= Setup.MinEventTimeout * 60) {
                     char *buf;
                     asprintf(&buf, tr("Recording in %ld minutes, shut down anyway?"), Delta / 60);
                     bool confirm = Interface->Confirm(buf);
                     free(buf);
                     if (!confirm)
                        break;
                     }
                  }
               ForceShutdown = true;
               break;
               }
          default: break;
          }
        Interact = Menu ? Menu : cControl::Control(); // might have been closed in the mean time
        if (Interact) {
           eOSState state = Interact->ProcessKey(key);
           if (state == osUnknown && Interact != cControl::Control()) {
              if (ISMODELESSKEY(key) && cControl::Control()) {
                 state = cControl::Control()->ProcessKey(key);
                 if (state == osEnd) {
                    // let's not close a menu when replay ends:
                    cControl::Shutdown();
                    continue;
                    }
                 }
              else if (time(NULL) - LastActivity > MENUTIMEOUT)
                 state = osEnd;
              }
           // TODO make the CAM menu stay open in case of automatic updates and have it return osContinue; then the following two lines can be removed again
           else if (state == osEnd && LastActivity > 1)
              LastActivity = time(NULL);
           switch (state) {
             case osPause:  DELETE_MENU;
                            cControl::Shutdown(); // just in case
                            if (!cRecordControls::PauseLiveVideo())
                               Skins.Message(mtError, tr("No free DVB device to record!"));
                            break;
             case osRecord: DELETE_MENU;
                            if (cRecordControls::Start())
                               Skins.Message(mtInfo, tr("Recording started"));
                            break;
             case osRecordings:
                            DELETE_MENU;
                            cControl::Shutdown();
                            Menu = new cMenuMain(osRecordings);
                            break;
             case osReplay: DELETE_MENU;
                            cControl::Shutdown();
                            cControl::Launch(new cReplayControl);
                            break;
             case osStopReplay:
                            DELETE_MENU;
                            cControl::Shutdown();
                            break;
             case osSwitchDvb:
                            DELETE_MENU;
                            cControl::Shutdown();
                            Skins.Message(mtInfo, tr("Switching primary DVB..."));
                            cDevice::SetPrimaryDevice(Setup.PrimaryDVB);
                            break;
             case osPlugin: DELETE_MENU;
                            Menu = cMenuMain::PluginOsdObject();
                            if (Menu)
                               Menu->Show();
                            break;
             case osBack:
             case osEnd:    if (Interact == Menu)
                               DELETE_MENU;
                            else
                               cControl::Shutdown();
                            break;
             default:       ;
             }
           }
        else {
           // Key functions in "normal" viewing mode:
           if (key != kNone && KeyMacros.Get(key)) {
              cRemote::PutMacro(key);
              key = kNone;
              }
           switch (key) {
             // Toggle channels:
             case kChanPrev:
             case k0: {
                  if (PreviousChannel[PreviousChannelIndex ^ 1] == LastChannel || LastChannel != PreviousChannel[0] && LastChannel != PreviousChannel[1])
                     PreviousChannelIndex ^= 1;
                  Channels.SwitchTo(PreviousChannel[PreviousChannelIndex ^= 1]);
                  break;
                  }
             // Direct Channel Select:
             case k1 ... k9:
             // Left/Right rotates through channel groups:
             case kLeft|k_Repeat:
             case kLeft:
             case kRight|k_Repeat:
             case kRight:
             // Previous/Next rotates through channel groups:
             case kPrev|k_Repeat:
             case kPrev:
             case kNext|k_Repeat:
             case kNext:
             // Up/Down Channel Select:
             case kUp|k_Repeat:
             case kUp:
             case kDown|k_Repeat:
             case kDown:
                  Menu = new cDisplayChannel(NORMALKEY(key));
                  break;
             // Viewing Control:
             case kOk:   LastChannel = -1; break; // forces channel display
             // Instant resume of the last viewed recording:
             case kPlay:
                  if (cReplayControl::LastReplayed()) {
                     cControl::Shutdown();
                     cControl::Launch(new cReplayControl);
                     }
                  break;
             default:    break;
             }
           }
        if (!Menu) {
           if (!InhibitEpgScan)
              EITScanner.Process();
           if (!cCutter::Active() && cCutter::Ended()) {
              if (cCutter::Error())
                 Skins.Message(mtError, tr("Editing process failed!"));
              else
                 Skins.Message(mtInfo, tr("Editing process finished"));
              }
           }
        if (!Interact && ((!cRecordControls::Active() && !cCutter::Active() && !cPluginManager::Active() && (!Interface->HasSVDRPConnection() || UserShutdown)) || ForceShutdown)) {
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
                 if (timer && Delta < Setup.MinEventTimeout * 60 && ForceShutdown) {
                    Delta = Setup.MinEventTimeout * 60;
                    Next = Now + Delta;
                    timer = NULL;
                    dsyslog("reboot at %s", *TimeToString(Next));
                    }
                 if (!Next || Delta > Setup.MinEventTimeout * 60 || ForceShutdown) {
                    ForceShutdown = false;
                    if (timer)
                       dsyslog("next timer event at %s", *TimeToString(Next));
                    if (WatchdogTimeout > 0)
                       signal(SIGALRM, SIG_IGN);
                    if (Interface->Confirm(tr("Press any key to cancel shutdown"), UserShutdown ? 5 : SHUTDOWNWAIT, true)) {
                       cControl::Shutdown();
                       int Channel = timer ? timer->Channel()->Number() : 0;
                       const char *File = timer ? timer->File() : "";
                       if (timer)
                          Delta = Next - time(NULL); // compensates for Confirm() timeout
                       char *cmd;
                       asprintf(&cmd, "%s %ld %ld %d \"%s\" %d", Shutdown, Next, Delta, Channel, *strescape(File, "\"$"), UserShutdown);
                       isyslog("executing '%s'", cmd);
                       SystemExec(cmd);
                       free(cmd);
                       LastActivity = time(NULL) - Setup.MinUserInactivity * 60 + SHUTDOWNRETRY; // try again later
                       }
                    else {
                      LastActivity = Now;
                      if (WatchdogTimeout > 0) {
                         alarm(WatchdogTimeout);
                         if (signal(SIGALRM, Watchdog) == SIG_IGN)
                            signal(SIGALRM, SIG_IGN);
                         }
                      }
                    UserShutdown = false;
                    continue; // skip the rest of the housekeeping for now
                    }
                 }
              // Disk housekeeping:
              RemoveDeletedRecordings();
              cSchedules::Cleanup();
              // Plugins housekeeping:
              PluginManager.Housekeeping();
              }
           }
        // Main thread hooks of plugins:
        PluginManager.MainThreadHook();
        }
  if (Interrupted)
     isyslog("caught signal %d", Interrupted);

Exit:

  PluginManager.StopPlugins();
  cRecordControls::Shutdown();
  cCutter::Stop();
  delete Menu;
  cControl::Shutdown();
  delete Interface;
  cOsdProvider::Shutdown();
  Remotes.Clear();
  Audios.Clear();
  Skins.Clear();
  if (ExitCode != 2) {
     Setup.CurrentChannel = cDevice::CurrentChannel();
     Setup.CurrentVolume  = cDevice::CurrentVolume();
     Setup.Save();
     }
  cDevice::Shutdown();
  PluginManager.Shutdown(true);
  cSchedules::Cleanup(true);
  ReportEpgBugFixStats();
  if (WatchdogTimeout > 0)
     dsyslog("max. latency time %d seconds", MaxLatencyTime);
  isyslog("exiting");
  if (SysLogLevel > 0)
     closelog();
  if (HasStdin)
     tcsetattr(STDIN_FILENO, TCSANOW, &savedTm);
  if (cThread::EmergencyExit()) {
     esyslog("emergency exit!");
     return 1;
     }
  return ExitCode;
}
