/*
 * svdrp.c: Simple Video Disk Recorder Protocol
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * The "Simple Video Disk Recorder Protocol" (SVDRP) was inspired
 * by the "Simple Mail Transfer Protocol" (SMTP) and is fully ASCII
 * text based. Therefore you can simply 'telnet' to your VDR port
 * and interact with the Video Disk Recorder - or write a full featured
 * graphical interface that sits on top of an SVDRP connection.
 *
 * $Id: svdrp.c 2.12 2011/12/04 13:58:33 kls Exp $
 */

#include "svdrp.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "channels.h"
#include "config.h"
#include "cutter.h"
#include "device.h"
#include "eitscan.h"
#include "keys.h"
#include "menu.h"
#include "plugin.h"
#include "remote.h"
#include "skins.h"
#include "timers.h"
#include "tools.h"
#include "videodir.h"

// --- cSocket ---------------------------------------------------------------

cSocket::cSocket(int Port, int Queue)
{
  port = Port;
  sock = -1;
  queue = Queue;
}

cSocket::~cSocket()
{
  Close();
}

void cSocket::Close(void)
{
  if (sock >= 0) {
     close(sock);
     sock = -1;
     }
}

bool cSocket::Open(void)
{
  if (sock < 0) {
     // create socket:
     sock = socket(PF_INET, SOCK_STREAM, 0);
     if (sock < 0) {
        LOG_ERROR;
        port = 0;
        return false;
        }
     // allow it to always reuse the same port:
     int ReUseAddr = 1;
     setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ReUseAddr, sizeof(ReUseAddr));
     //
     struct sockaddr_in name;
     name.sin_family = AF_INET;
     name.sin_port = htons(port);
     name.sin_addr.s_addr = SVDRPhosts.LocalhostOnly() ? htonl(INADDR_LOOPBACK) : htonl(INADDR_ANY);
     if (bind(sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
        LOG_ERROR;
        Close();
        return false;
        }
     // make it non-blocking:
     int oldflags = fcntl(sock, F_GETFL, 0);
     if (oldflags < 0) {
        LOG_ERROR;
        return false;
        }
     oldflags |= O_NONBLOCK;
     if (fcntl(sock, F_SETFL, oldflags) < 0) {
        LOG_ERROR;
        return false;
        }
     // listen to the socket:
     if (listen(sock, queue) < 0) {
        LOG_ERROR;
        return false;
        }
     }
  return true;
}

int cSocket::Accept(void)
{
  if (Open()) {
     struct sockaddr_in clientname;
     uint size = sizeof(clientname);
     int newsock = accept(sock, (struct sockaddr *)&clientname, &size);
     if (newsock > 0) {
        bool accepted = SVDRPhosts.Acceptable(clientname.sin_addr.s_addr);
        if (!accepted) {
           const char *s = "Access denied!\n";
           if (write(newsock, s, strlen(s)) < 0)
              LOG_ERROR;
           close(newsock);
           newsock = -1;
           }
        isyslog("connect from %s, port %hu - %s", inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port), accepted ? "accepted" : "DENIED");
        }
     else if (errno != EINTR && errno != EAGAIN)
        LOG_ERROR;
     return newsock;
     }
  return -1;
}

// --- cPUTEhandler ----------------------------------------------------------

cPUTEhandler::cPUTEhandler(void)
{
  if ((f = tmpfile()) != NULL) {
     status = 354;
     message = "Enter EPG data, end with \".\" on a line by itself";
     }
  else {
     LOG_ERROR;
     status = 554;
     message = "Error while opening temporary file";
     }
}

cPUTEhandler::~cPUTEhandler()
{
  if (f)
     fclose(f);
}

bool cPUTEhandler::Process(const char *s)
{
  if (f) {
     if (strcmp(s, ".") != 0) {
        fputs(s, f);
        fputc('\n', f);
        return true;
        }
     else {
        rewind(f);
        if (cSchedules::Read(f)) {
           cSchedules::Cleanup(true);
           status = 250;
           message = "EPG data processed";
           }
        else {
           status = 451;
           message = "Error while processing EPG data";
           }
        fclose(f);
        f = NULL;
        }
     }
  return false;
}

// --- cSVDRP ----------------------------------------------------------------

#define MAXHELPTOPIC 10
#define EITDISABLETIME 10 // seconds until EIT processing is enabled again after a CLRE command
                          // adjust the help for CLRE accordingly if changing this!

const char *HelpPages[] = {
  "CHAN [ + | - | <number> | <name> | <id> ]\n"
  "    Switch channel up, down or to the given channel number, name or id.\n"
  "    Without option (or after successfully switching to the channel)\n"
  "    it returns the current channel number and name.",
  "CLRE [ <number> | <name> | <id> ]\n"
  "    Clear the EPG list of the given channel number, name or id.\n"
  "    Without option it clears the entire EPG list.\n"
  "    After a CLRE command, no further EPG processing is done for 10\n"
  "    seconds, so that data sent with subsequent PUTE commands doesn't\n"
  "    interfere with data from the broadcasters.",
  "DELC <number>\n"
  "    Delete channel.",
  "DELR <number>\n"
  "    Delete the recording with the given number. Before a recording can be\n"
  "    deleted, an LSTR command must have been executed in order to retrieve\n"
  "    the recording numbers. The numbers don't change during subsequent DELR\n"
  "    commands. CAUTION: THERE IS NO CONFIRMATION PROMPT WHEN DELETING A\n"
  "    RECORDING - BE SURE YOU KNOW WHAT YOU ARE DOING!",
  "DELT <number>\n"
  "    Delete timer.",
  "EDIT <number>\n"
  "    Edit the recording with the given number. Before a recording can be\n"
  "    edited, an LSTR command must have been executed in order to retrieve\n"
  "    the recording numbers.",
  "GRAB <filename> [ <quality> [ <sizex> <sizey> ] ]\n"
  "    Grab the current frame and save it to the given file. Images can\n"
  "    be stored as JPEG or PNM, depending on the given file name extension.\n"
  "    The quality of the grabbed image can be in the range 0..100, where 100\n"
  "    (the default) means \"best\" (only applies to JPEG). The size parameters\n"
  "    define the size of the resulting image (default is full screen).\n"
  "    If the file name is just an extension (.jpg, .jpeg or .pnm) the image\n"
  "    data will be sent to the SVDRP connection encoded in base64. The same\n"
  "    happens if '-' (a minus sign) is given as file name, in which case the\n"
  "    image format defaults to JPEG.",
  "HELP [ <topic> ]\n"
  "    The HELP command gives help info.",
  "HITK [ <key> ... ]\n"
  "    Hit the given remote control key. Without option a list of all\n"
  "    valid key names is given. If more than one key is given, they are\n"
  "    entered into the remote control queue in the given sequence. There\n"
  "    can be up to 31 keys.",
  "LSTC [ :groups | <number> | <name> | <id> ]\n"
  "    List channels. Without option, all channels are listed. Otherwise\n"
  "    only the given channel is listed. If a name is given, all channels\n"
  "    containing the given string as part of their name are listed.\n"
  "    If ':groups' is given, all channels are listed including group\n"
  "    separators. The channel number of a group separator is always 0.",
  "LSTE [ <channel> ] [ now | next | at <time> ]\n"
  "    List EPG data. Without any parameters all data of all channels is\n"
  "    listed. If a channel is given (either by number or by channel ID),\n"
  "    only data for that channel is listed. 'now', 'next', or 'at <time>'\n"
  "    restricts the returned data to present events, following events, or\n"
  "    events at the given time (which must be in time_t form).",
  "LSTR [ <number> ]\n"
  "    List recordings. Without option, all recordings are listed. Otherwise\n"
  "    the information for the given recording is listed.",
  "LSTT [ <number> ] [ id ]\n"
  "    List timers. Without option, all timers are listed. Otherwise\n"
  "    only the given timer is listed. If the keyword 'id' is given, the\n"
  "    channels will be listed with their unique channel ids instead of\n"
  "    their numbers.",
  "MESG <message>\n"
  "    Displays the given message on the OSD. The message will be queued\n"
  "    and displayed whenever this is suitable.\n",
  "MODC <number> <settings>\n"
  "    Modify a channel. Settings must be in the same format as returned\n"
  "    by the LSTC command.",
  "MODT <number> on | off | <settings>\n"
  "    Modify a timer. Settings must be in the same format as returned\n"
  "    by the LSTT command. The special keywords 'on' and 'off' can be\n"
  "    used to easily activate or deactivate a timer.",
  "MOVC <number> <to>\n"
  "    Move a channel to a new position.",
  "MOVT <number> <to>\n"
  "    Move a timer to a new position.",
  "NEWC <settings>\n"
  "    Create a new channel. Settings must be in the same format as returned\n"
  "    by the LSTC command.",
  "NEWT <settings>\n"
  "    Create a new timer. Settings must be in the same format as returned\n"
  "    by the LSTT command. It is an error if a timer with the same channel,\n"
  "    day, start and stop time already exists.",
  "NEXT [ abs | rel ]\n"
  "    Show the next timer event. If no option is given, the output will be\n"
  "    in human readable form. With option 'abs' the absolute time of the next\n"
  "    event will be given as the number of seconds since the epoch (time_t\n"
  "    format), while with option 'rel' the relative time will be given as the\n"
  "    number of seconds from now until the event. If the absolute time given\n"
  "    is smaller than the current time, or if the relative time is less than\n"
  "    zero, this means that the timer is currently recording and has started\n"
  "    at the given time. The first value in the resulting line is the number\n"
  "    of the timer.",
  "PLAY <number> [ begin | <position> ]\n"
  "    Play the recording with the given number. Before a recording can be\n"
  "    played, an LSTR command must have been executed in order to retrieve\n"
  "    the recording numbers.\n"
  "    The keyword 'begin' plays the recording from its very beginning, while\n"
  "    a <position> (given as hh:mm:ss[.ff] or framenumber) starts at that\n"
  "    position. If neither 'begin' nor a <position> are given, replay is resumed\n"
  "    at the position where any previous replay was stopped, or from the beginning\n"
  "    by default. To control or stop the replay session, use the usual remote\n"
  "    control keypresses via the HITK command.",
  "PLUG <name> [ help | main ] [ <command> [ <options> ]]\n"
  "    Send a command to a plugin.\n"
  "    The PLUG command without any parameters lists all plugins.\n"
  "    If only a name is given, all commands known to that plugin are listed.\n"
  "    If a command is given (optionally followed by parameters), that command\n"
  "    is sent to the plugin, and the result will be displayed.\n"
  "    The keyword 'help' lists all the SVDRP commands known to the named plugin.\n"
  "    If 'help' is followed by a command, the detailed help for that command is\n"
  "    given. The keyword 'main' initiates a call to the main menu function of the\n"
  "    given plugin.\n",
  "PUTE [ file ]\n"
  "    Put data into the EPG list. The data entered has to strictly follow the\n"
  "    format defined in vdr(5) for the 'epg.data' file.  A '.' on a line\n"
  "    by itself terminates the input and starts processing of the data (all\n"
  "    entered data is buffered until the terminating '.' is seen).\n"
  "    If a file name is given, epg data will be read from this file (which\n"
  "    must be accessible under the given name from the machine VDR is running\n"
  "    on). In case of file input, no terminating '.' shall be given.\n",
  "REMO [ on | off ]\n"
  "    Turns the remote control on or off. Without a parameter, the current\n"
  "    status of the remote control is reported.",
  "SCAN\n"
  "    Forces an EPG scan. If this is a single DVB device system, the scan\n"
  "    will be done on the primary device unless it is currently recording.",
  "STAT disk\n"
  "    Return information about disk usage (total, free, percent).",
  "UPDT <settings>\n"
  "    Updates a timer. Settings must be in the same format as returned\n"
  "    by the LSTT command. If a timer with the same channel, day, start\n"
  "    and stop time does not yet exists, it will be created.",
  "UPDR\n"
  "    Initiates a re-read of the recordings directory, which is the SVDRP\n"
  "    equivalent to 'touch .update'.",
  "VOLU [ <number> | + | - | mute ]\n"
  "    Set the audio volume to the given number (which is limited to the range\n"
  "    0...255). If the special options '+' or '-' are given, the volume will\n"
  "    be turned up or down, respectively. The option 'mute' will toggle the\n"
  "    audio muting. If no option is given, the current audio volume level will\n"
  "    be returned.",
  "QUIT\n"
  "    Exit vdr (SVDRP).\n"
  "    You can also hit Ctrl-D to exit.",
  NULL
  };

/* SVDRP Reply Codes:

 214 Help message
 215 EPG or recording data record
 216 Image grab data (base 64)
 220 VDR service ready
 221 VDR service closing transmission channel
 250 Requested VDR action okay, completed
 354 Start sending EPG data
 451 Requested action aborted: local error in processing
 500 Syntax error, command unrecognized
 501 Syntax error in parameters or arguments
 502 Command not implemented
 504 Command parameter not implemented
 550 Requested action not taken
 554 Transaction failed
 900 Default plugin reply code
 901..999 Plugin specific reply codes

*/

const char *GetHelpTopic(const char *HelpPage)
{
  static char topic[MAXHELPTOPIC];
  const char *q = HelpPage;
  while (*q) {
        if (isspace(*q)) {
           uint n = q - HelpPage;
           if (n >= sizeof(topic))
              n = sizeof(topic) - 1;
           strncpy(topic, HelpPage, n);
           topic[n] = 0;
           return topic;
           }
        q++;
        }
  return NULL;
}

const char *GetHelpPage(const char *Cmd, const char **p)
{
  if (p) {
     while (*p) {
           const char *t = GetHelpTopic(*p);
           if (strcasecmp(Cmd, t) == 0)
              return *p;
           p++;
           }
     }
  return NULL;
}

char *cSVDRP::grabImageDir = NULL;

cSVDRP::cSVDRP(int Port)
:socket(Port)
{
  PUTEhandler = NULL;
  numChars = 0;
  length = BUFSIZ;
  cmdLine = MALLOC(char, length);
  lastActivity = 0;
  isyslog("SVDRP listening on port %d", Port);
}

cSVDRP::~cSVDRP()
{
  Close(true);
  free(cmdLine);
}

void cSVDRP::Close(bool SendReply, bool Timeout)
{
  if (file.IsOpen()) {
     if (SendReply) {
        //TODO how can we get the *full* hostname?
        char buffer[BUFSIZ];
        gethostname(buffer, sizeof(buffer));
        Reply(221, "%s closing connection%s", buffer, Timeout ? " (timeout)" : "");
        }
     isyslog("closing SVDRP connection"); //TODO store IP#???
     file.Close();
     DELETENULL(PUTEhandler);
     }
}

bool cSVDRP::Send(const char *s, int length)
{
  if (length < 0)
     length = strlen(s);
  if (safe_write(file, s, length) < 0) {
     LOG_ERROR;
     Close();
     return false;
     }
  return true;
}

void cSVDRP::Reply(int Code, const char *fmt, ...)
{
  if (file.IsOpen()) {
     if (Code != 0) {
        va_list ap;
        va_start(ap, fmt);
        cString buffer = cString::sprintf(fmt, ap);
        va_end(ap);
        const char *s = buffer;
        while (s && *s) {
              const char *n = strchr(s, '\n');
              char cont = ' ';
              if (Code < 0 || n && *(n + 1)) // trailing newlines don't count!
                 cont = '-';
              char number[16];
              sprintf(number, "%03d%c", abs(Code), cont);
              if (!(Send(number) && Send(s, n ? n - s : -1) && Send("\r\n")))
                 break;
              s = n ? n + 1 : NULL;
              }
        }
     else {
        Reply(451, "Zero return code - looks like a programming error!");
        esyslog("SVDRP: zero return code!");
        }
     }
}

void cSVDRP::PrintHelpTopics(const char **hp)
{
  int NumPages = 0;
  if (hp) {
     while (*hp) {
           NumPages++;
           hp++;
           }
     hp -= NumPages;
     }
  const int TopicsPerLine = 5;
  int x = 0;
  for (int y = 0; (y * TopicsPerLine + x) < NumPages; y++) {
      char buffer[TopicsPerLine * MAXHELPTOPIC + 5];
      char *q = buffer;
      q += sprintf(q, "    ");
      for (x = 0; x < TopicsPerLine && (y * TopicsPerLine + x) < NumPages; x++) {
          const char *topic = GetHelpTopic(hp[(y * TopicsPerLine + x)]);
          if (topic)
             q += sprintf(q, "%*s", -MAXHELPTOPIC, topic);
          }
      x = 0;
      Reply(-214, "%s", buffer);
      }
}

void cSVDRP::CmdCHAN(const char *Option)
{
  if (*Option) {
     int n = -1;
     int d = 0;
     if (isnumber(Option)) {
        int o = strtol(Option, NULL, 10);
        if (o >= 1 && o <= Channels.MaxNumber())
           n = o;
        }
     else if (strcmp(Option, "-") == 0) {
        n = cDevice::CurrentChannel();
        if (n > 1) {
           n--;
           d = -1;
           }
        }
     else if (strcmp(Option, "+") == 0) {
        n = cDevice::CurrentChannel();
        if (n < Channels.MaxNumber()) {
           n++;
           d = 1;
           }
        }
     else {
        cChannel *channel = Channels.GetByChannelID(tChannelID::FromString(Option));
        if (channel)
           n = channel->Number();
        else {
           for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
               if (!channel->GroupSep()) {
                  if (strcasecmp(channel->Name(), Option) == 0) {
                     n = channel->Number();
                     break;
                     }
                  }
               }
           }
        }
     if (n < 0) {
        Reply(501, "Undefined channel \"%s\"", Option);
        return;
        }
     if (!d) {
        cChannel *channel = Channels.GetByNumber(n);
        if (channel) {
           if (!cDevice::PrimaryDevice()->SwitchChannel(channel, true)) {
              Reply(554, "Error switching to channel \"%d\"", channel->Number());
              return;
              }
           }
        else {
           Reply(550, "Unable to find channel \"%s\"", Option);
           return;
           }
        }
     else
        cDevice::SwitchChannel(d);
     }
  cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
  if (channel)
     Reply(250, "%d %s", channel->Number(), channel->Name());
  else
     Reply(550, "Unable to find channel \"%d\"", cDevice::CurrentChannel());
}

void cSVDRP::CmdCLRE(const char *Option)
{
  if (*Option) {
     tChannelID ChannelID = tChannelID::InvalidID;
     if (isnumber(Option)) {
        int o = strtol(Option, NULL, 10);
        if (o >= 1 && o <= Channels.MaxNumber())
           ChannelID = Channels.GetByNumber(o)->GetChannelID();
        }
     else {
        ChannelID = tChannelID::FromString(Option);
        if (ChannelID == tChannelID::InvalidID) {
           for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
               if (!Channel->GroupSep()) {
                  if (strcasecmp(Channel->Name(), Option) == 0) {
                     ChannelID = Channel->GetChannelID();
                     break;
                     }
                  }
               }
           }
        }
     if (!(ChannelID == tChannelID::InvalidID)) {
        cSchedulesLock SchedulesLock(true, 1000);
        cSchedules *s = (cSchedules *)cSchedules::Schedules(SchedulesLock);
        if (s) {
           cSchedule *Schedule = NULL;
           ChannelID.ClrRid();
           for (cSchedule *p = s->First(); p; p = s->Next(p)) {
               if (p->ChannelID() == ChannelID) {
                  Schedule = p;
                  break;
                  }
               }
           if (Schedule) {
              Schedule->Cleanup(INT_MAX);
              cEitFilter::SetDisableUntil(time(NULL) + EITDISABLETIME);
              Reply(250, "EPG data of channel \"%s\" cleared", Option);
              }
           else {
              Reply(550, "No EPG data found for channel \"%s\"", Option);
              return;
              }
           }
        else
           Reply(451, "Can't get EPG data");
        }
     else
        Reply(501, "Undefined channel \"%s\"", Option);
     }
  else {
     cSchedules::ClearAll();
     cEitFilter::SetDisableUntil(time(NULL) + EITDISABLETIME);
     Reply(250, "EPG data cleared");
     }
}

void cSVDRP::CmdDELC(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        if (!Channels.BeingEdited()) {
           cChannel *channel = Channels.GetByNumber(strtol(Option, NULL, 10));
           if (channel) {
              for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer)) {
                  if (timer->Channel() == channel) {
                     Reply(550, "Channel \"%s\" is in use by timer %d", Option, timer->Index() + 1);
                     return;
                     }
                  }
              int CurrentChannelNr = cDevice::CurrentChannel();
              cChannel *CurrentChannel = Channels.GetByNumber(CurrentChannelNr);
              if (CurrentChannel && channel == CurrentChannel) {
                 int n = Channels.GetNextNormal(CurrentChannel->Index());
                 if (n < 0)
                    n = Channels.GetPrevNormal(CurrentChannel->Index());
                 CurrentChannel = Channels.Get(n);
                 CurrentChannelNr = 0; // triggers channel switch below
                 }
              Channels.Del(channel);
              Channels.ReNumber();
              Channels.SetModified(true);
              isyslog("channel %s deleted", Option);
              if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
                 if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
                    Channels.SwitchTo(CurrentChannel->Number());
                 else
                    cDevice::SetCurrentChannel(CurrentChannel);
                 }
              Reply(250, "Channel \"%s\" deleted", Option);
              }
           else
              Reply(501, "Channel \"%s\" not defined", Option);
           }
        else
           Reply(550, "Channels are being edited - try again later");
        }
     else
        Reply(501, "Error in channel number \"%s\"", Option);
     }
  else
     Reply(501, "Missing channel number");
}

void cSVDRP::CmdDELR(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = Recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           cRecordControl *rc = cRecordControls::GetRecordControl(recording->FileName());
           if (!rc) {
              if (recording->Delete()) {
                 Reply(250, "Recording \"%s\" deleted", Option);
                 ::Recordings.DelByName(recording->FileName());
                 }
              else
                 Reply(554, "Error while deleting recording!");
              }
           else
              Reply(550, "Recording \"%s\" is in use by timer %d", Option, rc->Timer()->Index() + 1);
           }
        else
           Reply(550, "Recording \"%s\" not found%s", Option, Recordings.Count() ? "" : " (use LSTR before deleting)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", Option);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRP::CmdDELT(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        if (!Timers.BeingEdited()) {
           cTimer *timer = Timers.Get(strtol(Option, NULL, 10) - 1);
           if (timer) {
              if (!timer->Recording()) {
                 isyslog("deleting timer %s", *timer->ToDescr());
                 Timers.Del(timer);
                 Timers.SetModified();
                 Reply(250, "Timer \"%s\" deleted", Option);
                 }
              else
                 Reply(550, "Timer \"%s\" is recording", Option);
              }
           else
              Reply(501, "Timer \"%s\" not defined", Option);
           }
        else
           Reply(550, "Timers are being edited - try again later");
        }
     else
        Reply(501, "Error in timer number \"%s\"", Option);
     }
  else
     Reply(501, "Missing timer number");
}

void cSVDRP::CmdEDIT(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = Recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           cMarks Marks;
           if (Marks.Load(recording->FileName(), recording->FramesPerSecond(), recording->IsPesRecording()) && Marks.Count()) {
              if (!cCutter::Active()) {
                 if (cCutter::Start(recording->FileName()))
                    Reply(250, "Editing recording \"%s\" [%s]", Option, recording->Title());
                 else
                    Reply(554, "Can't start editing process");
                 }
              else
                 Reply(554, "Editing process already active");
              }
           else
              Reply(554, "No editing marks defined");
           }
        else
           Reply(550, "Recording \"%s\" not found%s", Option, Recordings.Count() ? "" : " (use LSTR before editing)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", Option);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRP::CmdGRAB(const char *Option)
{
  const char *FileName = NULL;
  bool Jpeg = true;
  int Quality = -1, SizeX = -1, SizeY = -1;
  if (*Option) {
     char buf[strlen(Option) + 1];
     char *p = strcpy(buf, Option);
     const char *delim = " \t";
     char *strtok_next;
     FileName = strtok_r(p, delim, &strtok_next);
     // image type:
     const char *Extension = strrchr(FileName, '.');
     if (Extension) {
        if (strcasecmp(Extension, ".jpg") == 0 || strcasecmp(Extension, ".jpeg") == 0)
           Jpeg = true;
        else if (strcasecmp(Extension, ".pnm") == 0)
           Jpeg = false;
        else {
           Reply(501, "Unknown image type \"%s\"", Extension + 1);
           return;
           }
        if (Extension == FileName)
           FileName = NULL;
        }
     else if (strcmp(FileName, "-") == 0)
        FileName = NULL;
     // image quality (and obsolete type):
     if ((p = strtok_r(NULL, delim, &strtok_next)) != NULL) {
        if (strcasecmp(p, "JPEG") == 0 || strcasecmp(p, "PNM") == 0) {
           // tolerate for backward compatibility
           p = strtok_r(NULL, delim, &strtok_next);
           }
        if (p) {
           if (isnumber(p))
              Quality = atoi(p);
           else {
              Reply(501, "Invalid quality \"%s\"", p);
              return;
              }
           }
        }
     // image size:
     if ((p = strtok_r(NULL, delim, &strtok_next)) != NULL) {
        if (isnumber(p))
           SizeX = atoi(p);
        else {
           Reply(501, "Invalid sizex \"%s\"", p);
           return;
           }
        if ((p = strtok_r(NULL, delim, &strtok_next)) != NULL) {
           if (isnumber(p))
              SizeY = atoi(p);
           else {
              Reply(501, "Invalid sizey \"%s\"", p);
              return;
              }
           }
        else {
           Reply(501, "Missing sizey");
           return;
           }
        }
     if ((p = strtok_r(NULL, delim, &strtok_next)) != NULL) {
        Reply(501, "Unexpected parameter \"%s\"", p);
        return;
        }
     // canonicalize the file name:
     char RealFileName[PATH_MAX];
     if (FileName) {
        if (grabImageDir) {
           cString s(FileName);
           FileName = s;
           const char *slash = strrchr(FileName, '/');
           if (!slash) {
              s = AddDirectory(grabImageDir, FileName);
              FileName = s;
              }
           slash = strrchr(FileName, '/'); // there definitely is one
           cString t(s);
           t.Truncate(slash - FileName);
           char *r = realpath(t, RealFileName);
           if (!r) {
              LOG_ERROR_STR(FileName);
              Reply(501, "Invalid file name \"%s\"", FileName);
              return;
              }
           strcat(RealFileName, slash);
           FileName = RealFileName;
           if (strncmp(FileName, grabImageDir, strlen(grabImageDir)) != 0) {
              Reply(501, "Invalid file name \"%s\"", FileName);
              return;
              }
           }
        else {
           Reply(550, "Grabbing to file not allowed (use \"GRAB -\" instead)");
           return;
           }
        }
     // actual grabbing:
     int ImageSize;
     uchar *Image = cDevice::PrimaryDevice()->GrabImage(ImageSize, Jpeg, Quality, SizeX, SizeY);
     if (Image) {
        if (FileName) {
           int fd = open(FileName, O_WRONLY | O_CREAT | O_NOFOLLOW | O_TRUNC, DEFFILEMODE);
           if (fd >= 0) {
              if (safe_write(fd, Image, ImageSize) == ImageSize) {
                 dsyslog("grabbed image to %s", FileName);
                 Reply(250, "Grabbed image %s", Option);
                 }
              else {
                 LOG_ERROR_STR(FileName);
                 Reply(451, "Can't write to '%s'", FileName);
                 }
              close(fd);
              }
           else {
              LOG_ERROR_STR(FileName);
              Reply(451, "Can't open '%s'", FileName);
              }
           }
        else {
           cBase64Encoder Base64(Image, ImageSize);
           const char *s;
           while ((s = Base64.NextLine()) != NULL)
                 Reply(-216, "%s", s);
           Reply(216, "Grabbed image %s", Option);
           }
        free(Image);
        }
     else
        Reply(451, "Grab image failed");
     }
  else
     Reply(501, "Missing filename");
}

void cSVDRP::CmdHELP(const char *Option)
{
  if (*Option) {
     const char *hp = GetHelpPage(Option, HelpPages);
     if (hp)
        Reply(-214, "%s", hp);
     else {
        Reply(504, "HELP topic \"%s\" unknown", Option);
        return;
        }
     }
  else {
     Reply(-214, "This is VDR version %s", VDRVERSION);
     Reply(-214, "Topics:");
     PrintHelpTopics(HelpPages);
     cPlugin *plugin;
     for (int i = 0; (plugin = cPluginManager::GetPlugin(i)) != NULL; i++) {
         const char **hp = plugin->SVDRPHelpPages();
         if (hp)
            Reply(-214, "Plugin %s v%s - %s", plugin->Name(), plugin->Version(), plugin->Description());
         PrintHelpTopics(hp);
         }
     Reply(-214, "To report bugs in the implementation send email to");
     Reply(-214, "    vdr-bugs@tvdr.de");
     }
  Reply(214, "End of HELP info");
}

void cSVDRP::CmdHITK(const char *Option)
{
  if (*Option) {
     char buf[strlen(Option) + 1];
     strcpy(buf, Option);
     const char *delim = " \t";
     char *strtok_next;
     char *p = strtok_r(buf, delim, &strtok_next);
     int NumKeys = 0;
     while (p) {
           eKeys k = cKey::FromString(p);
           if (k != kNone) {
              if (!cRemote::Put(k)) {
                 Reply(451, "Too many keys in \"%s\" (only %d accepted)", Option, NumKeys);
                 return;
                 }
              }
           else {
              Reply(504, "Unknown key: \"%s\"", p);
              return;
              }
           NumKeys++;
           p = strtok_r(NULL, delim, &strtok_next);
           }
     Reply(250, "Key%s \"%s\" accepted", NumKeys > 1 ? "s" : "", Option);
     }
  else {
     Reply(-214, "Valid <key> names for the HITK command:");
     for (int i = 0; i < kNone; i++) {
         Reply(-214, "    %s", cKey::ToString(eKeys(i)));
         }
     Reply(214, "End of key list");
     }
}

void cSVDRP::CmdLSTC(const char *Option)
{
  bool WithGroupSeps = strcasecmp(Option, ":groups") == 0;
  if (*Option && !WithGroupSeps) {
     if (isnumber(Option)) {
        cChannel *channel = Channels.GetByNumber(strtol(Option, NULL, 10));
        if (channel)
           Reply(250, "%d %s", channel->Number(), *channel->ToText());
        else
           Reply(501, "Channel \"%s\" not defined", Option);
        }
     else {
        cChannel *next = Channels.GetByChannelID(tChannelID::FromString(Option));
        if (!next) {
           for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
              if (!channel->GroupSep()) {
                 if (strcasestr(channel->Name(), Option)) {
                    if (next)
                       Reply(-250, "%d %s", next->Number(), *next->ToText());
                    next = channel;
                    }
                 }
              }
           }
        if (next)
           Reply(250, "%d %s", next->Number(), *next->ToText());
        else
           Reply(501, "Channel \"%s\" not defined", Option);
        }
     }
  else if (Channels.MaxNumber() >= 1) {
     for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
         if (WithGroupSeps)
            Reply(channel->Next() ? -250: 250, "%d %s", channel->GroupSep() ? 0 : channel->Number(), *channel->ToText());
         else if (!channel->GroupSep())
            Reply(channel->Number() < Channels.MaxNumber() ? -250 : 250, "%d %s", channel->Number(), *channel->ToText());
         }
     }
  else
     Reply(550, "No channels defined");
}

void cSVDRP::CmdLSTE(const char *Option)
{
  cSchedulesLock SchedulesLock;
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  if (Schedules) {
     const cSchedule* Schedule = NULL;
     eDumpMode DumpMode = dmAll;
     time_t AtTime = 0;
     if (*Option) {
        char buf[strlen(Option) + 1];
        strcpy(buf, Option);
        const char *delim = " \t";
        char *strtok_next;
        char *p = strtok_r(buf, delim, &strtok_next);
        while (p && DumpMode == dmAll) {
              if (strcasecmp(p, "NOW") == 0)
                 DumpMode = dmPresent;
              else if (strcasecmp(p, "NEXT") == 0)
                 DumpMode = dmFollowing;
              else if (strcasecmp(p, "AT") == 0) {
                 DumpMode = dmAtTime;
                 if ((p = strtok_r(NULL, delim, &strtok_next)) != NULL) {
                    if (isnumber(p))
                       AtTime = strtol(p, NULL, 10);
                    else {
                       Reply(501, "Invalid time");
                       return;
                       }
                    }
                 else {
                    Reply(501, "Missing time");
                    return;
                    }
                 }
              else if (!Schedule) {
                 cChannel* Channel = NULL;
                 if (isnumber(p))
                    Channel = Channels.GetByNumber(strtol(Option, NULL, 10));
                 else
                    Channel = Channels.GetByChannelID(tChannelID::FromString(Option));
                 if (Channel) {
                    Schedule = Schedules->GetSchedule(Channel);
                    if (!Schedule) {
                       Reply(550, "No schedule found");
                       return;
                       }
                    }
                 else {
                    Reply(550, "Channel \"%s\" not defined", p);
                    return;
                    }
                 }
              else {
                 Reply(501, "Unknown option: \"%s\"", p);
                 return;
                 }
              p = strtok_r(NULL, delim, &strtok_next);
              }
        }
     int fd = dup(file);
     if (fd) {
        FILE *f = fdopen(fd, "w");
        if (f) {
           if (Schedule)
              Schedule->Dump(f, "215-", DumpMode, AtTime);
           else
              Schedules->Dump(f, "215-", DumpMode, AtTime);
           fflush(f);
           Reply(215, "End of EPG data");
           fclose(f);
           }
        else {
           Reply(451, "Can't open file connection");
           close(fd);
           }
        }
     else
        Reply(451, "Can't dup stream descriptor");
     }
  else
     Reply(451, "Can't get EPG data");
}

void cSVDRP::CmdLSTR(const char *Option)
{
  bool recordings = Recordings.Update(true);
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = Recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           FILE *f = fdopen(file, "w");
           if (f) {
              recording->Info()->Write(f, "215-");
              fflush(f);
              Reply(215, "End of recording information");
              // don't 'fclose(f)' here!
              }
           else
              Reply(451, "Can't open file connection");
           }
        else
           Reply(550, "Recording \"%s\" not found", Option);
        }
     else
        Reply(501, "Error in recording number \"%s\"", Option);
     }
  else if (recordings) {
     cRecording *recording = Recordings.First();
     while (recording) {
           Reply(recording == Recordings.Last() ? 250 : -250, "%d %s", recording->Index() + 1, recording->Title(' ', true));
           recording = Recordings.Next(recording);
           }
     }
  else
     Reply(550, "No recordings available");
}

void cSVDRP::CmdLSTT(const char *Option)
{
  int Number = 0;
  bool Id = false;
  if (*Option) {
     char buf[strlen(Option) + 1];
     strcpy(buf, Option);
     const char *delim = " \t";
     char *strtok_next;
     char *p = strtok_r(buf, delim, &strtok_next);
     while (p) {
           if (isnumber(p))
              Number = strtol(p, NULL, 10);
           else if (strcasecmp(p, "ID") == 0)
              Id = true;
           else {
              Reply(501, "Unknown option: \"%s\"", p);
              return;
              }
           p = strtok_r(NULL, delim, &strtok_next);
           }
     }
  if (Number) {
     cTimer *timer = Timers.Get(Number - 1);
     if (timer)
        Reply(250, "%d %s", timer->Index() + 1, *timer->ToText(Id));
     else
        Reply(501, "Timer \"%s\" not defined", Option);
     }
  else if (Timers.Count()) {
     for (int i = 0; i < Timers.Count(); i++) {
         cTimer *timer = Timers.Get(i);
        if (timer)
           Reply(i < Timers.Count() - 1 ? -250 : 250, "%d %s", timer->Index() + 1, *timer->ToText(Id));
        else
           Reply(501, "Timer \"%d\" not found", i + 1);
         }
     }
  else
     Reply(550, "No timers defined");
}

void cSVDRP::CmdMESG(const char *Option)
{
  if (*Option) {
     isyslog("SVDRP message: '%s'", Option);
     Skins.QueueMessage(mtInfo, Option);
     Reply(250, "Message queued");
     }
  else
     Reply(501, "Missing message");
}

void cSVDRP::CmdMODC(const char *Option)
{
  if (*Option) {
     char *tail;
     int n = strtol(Option, &tail, 10);
     if (tail && tail != Option) {
        tail = skipspace(tail);
        if (!Channels.BeingEdited()) {
           cChannel *channel = Channels.GetByNumber(n);
           if (channel) {
              cChannel ch;
              if (ch.Parse(tail)) {
                 if (Channels.HasUniqueChannelID(&ch, channel)) {
                    *channel = ch;
                    Channels.ReNumber();
                    Channels.SetModified(true);
                    isyslog("modifed channel %d %s", channel->Number(), *channel->ToText());
                    Reply(250, "%d %s", channel->Number(), *channel->ToText());
                    }
                 else
                    Reply(501, "Channel settings are not unique");
                 }
              else
                 Reply(501, "Error in channel settings");
              }
           else
              Reply(501, "Channel \"%d\" not defined", n);
           }
        else
           Reply(550, "Channels are being edited - try again later");
        }
     else
        Reply(501, "Error in channel number");
     }
  else
     Reply(501, "Missing channel settings");
}

void cSVDRP::CmdMODT(const char *Option)
{
  if (*Option) {
     char *tail;
     int n = strtol(Option, &tail, 10);
     if (tail && tail != Option) {
        tail = skipspace(tail);
        if (!Timers.BeingEdited()) {
           cTimer *timer = Timers.Get(n - 1);
           if (timer) {
              cTimer t = *timer;
              if (strcasecmp(tail, "ON") == 0)
                 t.SetFlags(tfActive);
              else if (strcasecmp(tail, "OFF") == 0)
                 t.ClrFlags(tfActive);
              else if (!t.Parse(tail)) {
                 Reply(501, "Error in timer settings");
                 return;
                 }
              *timer = t;
              Timers.SetModified();
              isyslog("timer %s modified (%s)", *timer->ToDescr(), timer->HasFlags(tfActive) ? "active" : "inactive");
              Reply(250, "%d %s", timer->Index() + 1, *timer->ToText());
              }
           else
              Reply(501, "Timer \"%d\" not defined", n);
           }
        else
           Reply(550, "Timers are being edited - try again later");
        }
     else
        Reply(501, "Error in timer number");
     }
  else
     Reply(501, "Missing timer settings");
}

void cSVDRP::CmdMOVC(const char *Option)
{
  if (*Option) {
     if (!Channels.BeingEdited() && !Timers.BeingEdited()) {
        char *tail;
        int From = strtol(Option, &tail, 10);
        if (tail && tail != Option) {
           tail = skipspace(tail);
           if (tail && tail != Option) {
              int To = strtol(tail, NULL, 10);
              int CurrentChannelNr = cDevice::CurrentChannel();
              cChannel *CurrentChannel = Channels.GetByNumber(CurrentChannelNr);
              cChannel *FromChannel = Channels.GetByNumber(From);
              if (FromChannel) {
                 cChannel *ToChannel = Channels.GetByNumber(To);
                 if (ToChannel) {
                    int FromNumber = FromChannel->Number();
                    int ToNumber = ToChannel->Number();
                    if (FromNumber != ToNumber) {
                       Channels.Move(FromChannel, ToChannel);
                       Channels.ReNumber();
                       Channels.SetModified(true);
                       if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
                          if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
                             Channels.SwitchTo(CurrentChannel->Number());
                          else
                             cDevice::SetCurrentChannel(CurrentChannel);
                          }
                       isyslog("channel %d moved to %d", FromNumber, ToNumber);
                       Reply(250,"Channel \"%d\" moved to \"%d\"", From, To);
                       }
                    else
                       Reply(501, "Can't move channel to same postion");
                    }
                 else
                    Reply(501, "Channel \"%d\" not defined", To);
                 }
              else
                 Reply(501, "Channel \"%d\" not defined", From);
              }
           else
              Reply(501, "Error in channel number");
           }
        else
           Reply(501, "Error in channel number");
        }
     else
        Reply(550, "Channels or timers are being edited - try again later");
     }
  else
     Reply(501, "Missing channel number");
}

void cSVDRP::CmdMOVT(const char *Option)
{
  //TODO combine this with menu action
  Reply(502, "MOVT not yet implemented");
}

void cSVDRP::CmdNEWC(const char *Option)
{
  if (*Option) {
     cChannel ch;
     if (ch.Parse(Option)) {
        if (Channels.HasUniqueChannelID(&ch)) {
           cChannel *channel = new cChannel;
           *channel = ch;
           Channels.Add(channel);
           Channels.ReNumber();
           Channels.SetModified(true);
           isyslog("new channel %d %s", channel->Number(), *channel->ToText());
           Reply(250, "%d %s", channel->Number(), *channel->ToText());
           }
        else
           Reply(501, "Channel settings are not unique");
        }
     else
        Reply(501, "Error in channel settings");
     }
  else
     Reply(501, "Missing channel settings");
}

void cSVDRP::CmdNEWT(const char *Option)
{
  if (*Option) {
     cTimer *timer = new cTimer;
     if (timer->Parse(Option)) {
        cTimer *t = Timers.GetTimer(timer);
        if (!t) {
           Timers.Add(timer);
           Timers.SetModified();
           isyslog("timer %s added", *timer->ToDescr());
           Reply(250, "%d %s", timer->Index() + 1, *timer->ToText());
           return;
           }
        else
           Reply(550, "Timer already defined: %d %s", t->Index() + 1, *t->ToText());
        }
     else
        Reply(501, "Error in timer settings");
     delete timer;
     }
  else
     Reply(501, "Missing timer settings");
}

void cSVDRP::CmdNEXT(const char *Option)
{
  cTimer *t = Timers.GetNextActiveTimer();
  if (t) {
     time_t Start = t->StartTime();
     int Number = t->Index() + 1;
     if (!*Option)
        Reply(250, "%d %s", Number, *TimeToString(Start));
     else if (strcasecmp(Option, "ABS") == 0)
        Reply(250, "%d %ld", Number, Start);
     else if (strcasecmp(Option, "REL") == 0)
        Reply(250, "%d %ld", Number, Start - time(NULL));
     else
        Reply(501, "Unknown option: \"%s\"", Option);
     }
  else
     Reply(550, "No active timers");
}

void cSVDRP::CmdPLAY(const char *Option)
{
  if (*Option) {
     char *opt = strdup(Option);
     char *num = skipspace(opt);
     char *option = num;
     while (*option && !isspace(*option))
           option++;
     char c = *option;
     *option = 0;
     if (isnumber(num)) {
        cRecording *recording = Recordings.Get(strtol(num, NULL, 10) - 1);
        if (recording) {
           if (c)
              option = skipspace(++option);
           cReplayControl::SetRecording(NULL, NULL);
           cControl::Shutdown();
           if (*option) {
              int pos = 0;
              if (strcasecmp(option, "BEGIN") != 0)
                 pos = HMSFToIndex(option, recording->FramesPerSecond());
              cResumeFile resume(recording->FileName(), recording->IsPesRecording());
              if (pos <= 0)
                 resume.Delete();
              else
                 resume.Save(pos);
              }
           cReplayControl::SetRecording(recording->FileName(), recording->Title());
           cControl::Launch(new cReplayControl);
           cControl::Attach();
           Reply(250, "Playing recording \"%s\" [%s]", num, recording->Title());
           }
        else
           Reply(550, "Recording \"%s\" not found%s", num, Recordings.Count() ? "" : " (use LSTR before playing)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", num);
     free(opt);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRP::CmdPLUG(const char *Option)
{
  if (*Option) {
     char *opt = strdup(Option);
     char *name = skipspace(opt);
     char *option = name;
     while (*option && !isspace(*option))
        option++;
     char c = *option;
     *option = 0;
     cPlugin *plugin = cPluginManager::GetPlugin(name);
     if (plugin) {
        if (c)
           option = skipspace(++option);
        char *cmd = option;
        while (*option && !isspace(*option))
              option++;
        if (*option) {
           *option++ = 0;
           option = skipspace(option);
           }
        if (!*cmd || strcasecmp(cmd, "HELP") == 0) {
           if (*cmd && *option) {
              const char *hp = GetHelpPage(option, plugin->SVDRPHelpPages());
              if (hp) {
                 Reply(-214, "%s", hp);
                 Reply(214, "End of HELP info");
                 }
              else
                 Reply(504, "HELP topic \"%s\" for plugin \"%s\" unknown", option, plugin->Name());
              }
           else {
              Reply(-214, "Plugin %s v%s - %s", plugin->Name(), plugin->Version(), plugin->Description());
              const char **hp = plugin->SVDRPHelpPages();
              if (hp) {
                 Reply(-214, "SVDRP commands:");
                 PrintHelpTopics(hp);
                 Reply(214, "End of HELP info");
                 }
              else
                 Reply(214, "This plugin has no SVDRP commands");
              }
           }
        else if (strcasecmp(cmd, "MAIN") == 0) {
           if (cRemote::CallPlugin(plugin->Name()))
              Reply(250, "Initiated call to main menu function of plugin \"%s\"", plugin->Name());
           else
              Reply(550, "A plugin call is already pending - please try again later");
           }
        else {
           int ReplyCode = 900;
           cString s = plugin->SVDRPCommand(cmd, option, ReplyCode);
           if (*s)
              Reply(abs(ReplyCode), "%s", *s);
           else
              Reply(500, "Command unrecognized: \"%s\"", cmd);
           }
        }
     else
        Reply(550, "Plugin \"%s\" not found (use PLUG for a list of plugins)", name);
     free(opt);
     }
  else {
     Reply(-214, "Available plugins:");
     cPlugin *plugin;
     for (int i = 0; (plugin = cPluginManager::GetPlugin(i)) != NULL; i++)
         Reply(-214, "%s v%s - %s", plugin->Name(), plugin->Version(), plugin->Description());
     Reply(214, "End of plugin list");
     }
}

void cSVDRP::CmdPUTE(const char *Option)
{
  if (*Option) {
     FILE *f = fopen(Option, "r");
     if (f) {
        if (cSchedules::Read(f)) {
           cSchedules::Cleanup(true);
           Reply(250, "EPG data processed from \"%s\"", Option);
           }
        else
           Reply(451, "Error while processing EPG from \"%s\"", Option);
        fclose(f);
        }
     else
        Reply(501, "Cannot open file \"%s\"", Option);
     }
  else {     
     delete PUTEhandler;
     PUTEhandler = new cPUTEhandler;
     Reply(PUTEhandler->Status(), "%s", PUTEhandler->Message());
     if (PUTEhandler->Status() != 354)
        DELETENULL(PUTEhandler);
     }
}

void cSVDRP::CmdREMO(const char *Option)
{
  if (*Option) {
     if (!strcasecmp(Option, "ON")) {
        cRemote::SetEnabled(true);
        Reply(250, "Remote control enabled");
        }
     else if (!strcasecmp(Option, "OFF")) {
        cRemote::SetEnabled(false);
        Reply(250, "Remote control disabled");
        }
     else
        Reply(501, "Invalid Option \"%s\"", Option);
     }
  else
     Reply(250, "Remote control is %s", cRemote::Enabled() ? "enabled" : "disabled");
}

void cSVDRP::CmdSCAN(const char *Option)
{
  EITScanner.ForceScan();
  Reply(250, "EPG scan triggered");
}

void cSVDRP::CmdSTAT(const char *Option)
{
  if (*Option) {
     if (strcasecmp(Option, "DISK") == 0) {
        int FreeMB, UsedMB;
        int Percent = VideoDiskSpace(&FreeMB, &UsedMB);
        Reply(250, "%dMB %dMB %d%%", FreeMB + UsedMB, FreeMB, Percent);
        }
     else
        Reply(501, "Invalid Option \"%s\"", Option);
     }
  else
     Reply(501, "No option given");
}

void cSVDRP::CmdUPDT(const char *Option)
{
  if (*Option) {
     cTimer *timer = new cTimer;
     if (timer->Parse(Option)) {
        if (!Timers.BeingEdited()) {
           cTimer *t = Timers.GetTimer(timer);
           if (t) {
              t->Parse(Option);
              delete timer;
              timer = t;
              isyslog("timer %s updated", *timer->ToDescr());
              }
           else {
              Timers.Add(timer);
              isyslog("timer %s added", *timer->ToDescr());
              }
           Timers.SetModified();
           Reply(250, "%d %s", timer->Index() + 1, *timer->ToText());
           return;
           }
        else
           Reply(550, "Timers are being edited - try again later");
        }
     else
        Reply(501, "Error in timer settings");
     delete timer;
     }
  else
     Reply(501, "Missing timer settings");
}

void cSVDRP::CmdUPDR(const char *Option)
{
  Recordings.Update(false);
  Reply(250, "Re-read of recordings directory triggered");
}

void cSVDRP::CmdVOLU(const char *Option)
{
  if (*Option) {
     if (isnumber(Option))
        cDevice::PrimaryDevice()->SetVolume(strtol(Option, NULL, 10), true);
     else if (strcmp(Option, "+") == 0)
        cDevice::PrimaryDevice()->SetVolume(VOLUMEDELTA);
     else if (strcmp(Option, "-") == 0)
        cDevice::PrimaryDevice()->SetVolume(-VOLUMEDELTA);
     else if (strcasecmp(Option, "MUTE") == 0)
        cDevice::PrimaryDevice()->ToggleMute();
     else {
        Reply(501, "Unknown option: \"%s\"", Option);
        return;
        }
     }
  if (cDevice::PrimaryDevice()->IsMute())
     Reply(250, "Audio is mute");
  else
     Reply(250, "Audio volume is %d", cDevice::CurrentVolume());
}

#define CMD(c) (strcasecmp(Cmd, c) == 0)

void cSVDRP::Execute(char *Cmd)
{
  // handle PUTE data:
  if (PUTEhandler) {
     if (!PUTEhandler->Process(Cmd)) {
        Reply(PUTEhandler->Status(), "%s", PUTEhandler->Message());
        DELETENULL(PUTEhandler);
        }
     return;
     }
  // skip leading whitespace:
  Cmd = skipspace(Cmd);
  // find the end of the command word:
  char *s = Cmd;
  while (*s && !isspace(*s))
        s++;
  if (*s)
     *s++ = 0;
  s = skipspace(s);
  if      (CMD("CHAN"))  CmdCHAN(s);
  else if (CMD("CLRE"))  CmdCLRE(s);
  else if (CMD("DELC"))  CmdDELC(s);
  else if (CMD("DELR"))  CmdDELR(s);
  else if (CMD("DELT"))  CmdDELT(s);
  else if (CMD("EDIT"))  CmdEDIT(s);
  else if (CMD("GRAB"))  CmdGRAB(s);
  else if (CMD("HELP"))  CmdHELP(s);
  else if (CMD("HITK"))  CmdHITK(s);
  else if (CMD("LSTC"))  CmdLSTC(s);
  else if (CMD("LSTE"))  CmdLSTE(s);
  else if (CMD("LSTR"))  CmdLSTR(s);
  else if (CMD("LSTT"))  CmdLSTT(s);
  else if (CMD("MESG"))  CmdMESG(s);
  else if (CMD("MODC"))  CmdMODC(s);
  else if (CMD("MODT"))  CmdMODT(s);
  else if (CMD("MOVC"))  CmdMOVC(s);
  else if (CMD("MOVT"))  CmdMOVT(s);
  else if (CMD("NEWC"))  CmdNEWC(s);
  else if (CMD("NEWT"))  CmdNEWT(s);
  else if (CMD("NEXT"))  CmdNEXT(s);
  else if (CMD("PLAY"))  CmdPLAY(s);
  else if (CMD("PLUG"))  CmdPLUG(s);
  else if (CMD("PUTE"))  CmdPUTE(s);
  else if (CMD("REMO"))  CmdREMO(s);
  else if (CMD("SCAN"))  CmdSCAN(s);
  else if (CMD("STAT"))  CmdSTAT(s);
  else if (CMD("UPDR"))  CmdUPDR(s);
  else if (CMD("UPDT"))  CmdUPDT(s);
  else if (CMD("VOLU"))  CmdVOLU(s);
  else if (CMD("QUIT"))  Close(true);
  else                   Reply(500, "Command unrecognized: \"%s\"", Cmd);
}

bool cSVDRP::Process(void)
{
  bool NewConnection = !file.IsOpen();
  bool SendGreeting = NewConnection;

  if (file.IsOpen() || file.Open(socket.Accept())) {
     if (SendGreeting) {
        //TODO how can we get the *full* hostname?
        char buffer[BUFSIZ];
        gethostname(buffer, sizeof(buffer));
        time_t now = time(NULL);
        Reply(220, "%s SVDRP VideoDiskRecorder %s; %s; %s", buffer, VDRVERSION, *TimeToString(now), cCharSetConv::SystemCharacterTable() ? cCharSetConv::SystemCharacterTable() : "UTF-8");
        }
     if (NewConnection)
        lastActivity = time(NULL);
     while (file.Ready(false)) {
           unsigned char c;
           int r = safe_read(file, &c, 1);
           if (r > 0) {
              if (c == '\n' || c == 0x00) {
                 // strip trailing whitespace:
                 while (numChars > 0 && strchr(" \t\r\n", cmdLine[numChars - 1]))
                       cmdLine[--numChars] = 0;
                 // make sure the string is terminated:
                 cmdLine[numChars] = 0;
                 // showtime!
                 Execute(cmdLine);
                 numChars = 0;
                 if (length > BUFSIZ) {
                    free(cmdLine); // let's not tie up too much memory
                    length = BUFSIZ;
                    cmdLine = MALLOC(char, length);
                    }
                 }
              else if (c == 0x04 && numChars == 0) {
                 // end of file (only at beginning of line)
                 Close(true);
                 }
              else if (c == 0x08 || c == 0x7F) {
                 // backspace or delete (last character)
                 if (numChars > 0)
                    numChars--;
                 }
              else if (c <= 0x03 || c == 0x0D) {
                 // ignore control characters
                 }
              else {
                 if (numChars >= length - 1) {
                    int NewLength = length + BUFSIZ;
                    if (char *NewBuffer = (char *)realloc(cmdLine, NewLength)) {
                       length = NewLength;
                       cmdLine = NewBuffer;
                       }
                    else {
                       esyslog("ERROR: out of memory");
                       Close();
                       break;
                       }
                    }
                 cmdLine[numChars++] = c;
                 cmdLine[numChars] = 0;
                 }
              lastActivity = time(NULL);
              }
           else if (r <= 0) {
              isyslog("lost connection to SVDRP client");
              Close();
              }
           }
     if (Setup.SVDRPTimeout && time(NULL) - lastActivity > Setup.SVDRPTimeout) {
        isyslog("timeout on SVDRP connection");
        Close(true, true);
        }
     return true;
     }
  return false;
}

void cSVDRP::SetGrabImageDir(const char *GrabImageDir)
{
  free(grabImageDir);
  grabImageDir = GrabImageDir ? strdup(GrabImageDir) : NULL;
}

//TODO more than one connection???
