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
 * $Id: svdrp.c 1.51 2003/04/27 14:21:07 kls Exp $
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
#include "device.h"
#include "keys.h"
#include "remote.h"
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
     name.sin_addr.s_addr = htonl(INADDR_ANY);
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
           write(newsock, s, strlen(s));
           close(newsock);
           newsock = -1;
           }
        isyslog("connect from %s, port %hd - %s", inet_ntoa(clientname.sin_addr), ntohs(clientname.sin_port), accepted ? "accepted" : "DENIED");
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
           cSIProcessor::TriggerDump();
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

const char *HelpPages[] = {
  "CHAN [ + | - | <number> | <name> | <id> ]\n"
  "    Switch channel up, down or to the given channel number, name or id.\n"
  "    Without option (or after successfully switching to the channel)\n"
  "    it returns the current channel number and name.",
  "CLRE\n"
  "    Clear the entire EPG list.",
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
  "GRAB <filename> [ jpeg | pnm [ <quality> [ <sizex> <sizey> ] ] ]\n"
  "    Grab the current frame and save it to the given file. Images can\n"
  "    be stored as JPEG (default) or PNM, at the given quality (default\n"
  "    is 'maximum', only applies to JPEG) and size (default is full screen).",
  "HELP [ <topic> ]\n"
  "    The HELP command gives help info.",
  "HITK [ <key> ]\n"
  "    Hit the given remote control key. Without option a list of all\n"
  "    valid key names is given.",
  "LSTC [ <number> | <name> ]\n"
  "    List channels. Without option, all channels are listed. Otherwise\n"
  "    only the given channel is listed. If a name is given, all channels\n"
  "    containing the given string as part of their name are listed.",
  "LSTE\n"
  "    List EPG data.",
  "LSTR [ <number> ]\n"
  "    List recordings. Without option, all recordings are listed. Otherwise\n"
  "    the summary for the given recording is listed.",
  "LSTT [ <number> ]\n"
  "    List timers. Without option, all timers are listed. Otherwise\n"
  "    only the given timer is listed.",
  "MESG [ <message> ]\n"
  "    Displays the given message on the OSD. If message is omitted, the\n"
  "    currently pending message (if any) will be returned. The message\n"
  "    will be displayed for a few seconds as soon as the OSD has become\n"
  "    idle. If a new MESG command is entered while the previous message\n"
  "    has not yet been displayed, the old message will be overwritten.",
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
  "PUTE\n"
  "    Put data into the EPG list. The data entered has to strictly follow the\n"
  "    format defined in vdr(5) for the 'epg.data' file.  A '.' on a line\n"
  "    by itself terminates the input and starts processing of the data (all\n"
  "    entered data is buffered until the terminating '.' is seen).",
  "STAT disk\n"
  "    Return information about disk usage (total, free, percent).",
  "UPDT <settings>\n"
  "    Updates a timer. Settings must be in the same format as returned\n"
  "    by the LSTT command. If a timer with the same channel, day, start\n"
  "    and stop time does not yet exists, it will be created.",
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
 215 EPG data record
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

const char *GetHelpPage(const char *Cmd)
{
  const char **p = HelpPages;
  while (*p) {
        const char *t = GetHelpTopic(*p);
        if (strcasecmp(Cmd, t) == 0)
           return *p;
        p++;
        }
  return NULL;
}

cSVDRP::cSVDRP(int Port)
:socket(Port)
{
  PUTEhandler = NULL;
  numChars = 0;
  message = NULL;
  lastActivity = 0;
  isyslog("SVDRP listening on port %d", Port);
}

cSVDRP::~cSVDRP()
{
  Close();
  free(message);
}

void cSVDRP::Close(bool Timeout)
{
  if (file.IsOpen()) {
     //TODO how can we get the *full* hostname?
     char buffer[BUFSIZ];
     gethostname(buffer, sizeof(buffer));
     Reply(221, "%s closing connection%s", buffer, Timeout ? " (timeout)" : "");
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
     file.Close();
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
        char *buffer;
        vasprintf(&buffer, fmt, ap);
        char *nl = strchr(buffer, '\n');
        if (Code > 0 && nl && *(nl + 1)) // trailing newlines don't count!
           Code = -Code;
        char number[16];
        sprintf(number, "%03d%c", abs(Code), Code < 0 ? '-' : ' ');
        const char *s = buffer;
        while (s && *s) {
              const char *n = strchr(s, '\n');
              if (!(Send(number) && Send(s, n ? n - s : -1) && Send("\r\n"))) {
                 Close();
                 break;
                 }
              s = n ? n + 1 : NULL;
              }
        free(buffer);
        va_end(ap);
        }
     else {
        Reply(451, "Zero return code - looks like a programming error!");
        esyslog("SVDRP: zero return code!");
        }
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
           int i = 1;
           while ((channel = Channels.GetByNumber(i, 1)) != NULL) {
                 if (strcasecmp(channel->Name(), Option) == 0) {
                    n = channel->Number();
                    break;
                    }
                 i = channel->Number() + 1;
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
  cSIProcessor::Clear();
  Reply(250, "EPG data cleared");
}

void cSVDRP::CmdDELC(const char *Option)
{
  //TODO combine this with menu action (timers must be updated)
  Reply(502, "DELC not yet implemented");
}

void cSVDRP::CmdDELR(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = Recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           if (recording->Delete())
              Reply(250, "Recording \"%s\" deleted", Option);
           else
              Reply(554, "Error while deleting recording!");
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
        cTimer *timer = Timers.Get(strtol(Option, NULL, 10) - 1);
        if (timer) {
           if (!timer->Recording()) {
              Timers.Del(timer);
              Timers.Save();
              isyslog("timer %s deleted", Option);
              Reply(250, "Timer \"%s\" deleted", Option);
              }
           else
              Reply(550, "Timer \"%s\" is recording", Option);
           }
        else
           Reply(501, "Timer \"%s\" not defined", Option);
        }
     else
        Reply(501, "Error in timer number \"%s\"", Option);
     }
  else
     Reply(501, "Missing timer number");
}

void cSVDRP::CmdGRAB(const char *Option)
{
  char *FileName = NULL;
  bool Jpeg = true;
  int Quality = -1, SizeX = -1, SizeY = -1;
  if (*Option) {
     char buf[strlen(Option) + 1];
     char *p = strcpy(buf, Option);
     const char *delim = " \t";
     FileName = strtok(p, delim);
     if ((p = strtok(NULL, delim)) != NULL) {
        if (strcasecmp(p, "JPEG") == 0)
           Jpeg = true;
        else if (strcasecmp(p, "PNM") == 0)
           Jpeg = false;
        else {
           Reply(501, "Unknown image type \"%s\"", p);
           return;
           }
        }
     if ((p = strtok(NULL, delim)) != NULL) {
        if (isnumber(p))
           Quality = atoi(p);
        else {
           Reply(501, "Illegal quality \"%s\"", p);
           return;
           }
        }
     if ((p = strtok(NULL, delim)) != NULL) {
        if (isnumber(p))
           SizeX = atoi(p);
        else {
           Reply(501, "Illegal sizex \"%s\"", p);
           return;
           }
        if ((p = strtok(NULL, delim)) != NULL) {
           if (isnumber(p))
              SizeY = atoi(p);
           else {
              Reply(501, "Illegal sizey \"%s\"", p);
              return;
              }
           }
        else {
           Reply(501, "Missing sizey");
           return;
           }
        }
     if ((p = strtok(NULL, delim)) != NULL) {
        Reply(501, "Unexpected parameter \"%s\"", p);
        return;
        }
     if (cDevice::PrimaryDevice()->GrabImage(FileName, Jpeg, Quality, SizeX, SizeY))
        Reply(250, "Grabbed image %s", Option);
     else
        Reply(451, "Grab image failed");
     }
  else
     Reply(501, "Missing filename");
}

void cSVDRP::CmdHELP(const char *Option)
{
  if (*Option) {
     const char *hp = GetHelpPage(Option);
     if (hp)
        Reply(214, hp);
     else {
        Reply(504, "HELP topic \"%s\" unknown", Option);
        return;
        }
     }
  else {
     Reply(-214, "This is VDR version %s", VDRVERSION);
     Reply(-214, "Topics:");
     const char **hp = HelpPages;
     int NumPages = 0;
     while (*hp) {
           NumPages++;
           hp++;
           }
     const int TopicsPerLine = 5;
     int x = 0;
     for (int y = 0; (y * TopicsPerLine + x) < NumPages; y++) {
         char buffer[TopicsPerLine * (MAXHELPTOPIC + 5)];
         char *q = buffer;
         for (x = 0; x < TopicsPerLine && (y * TopicsPerLine + x) < NumPages; x++) {
             const char *topic = GetHelpTopic(HelpPages[(y * TopicsPerLine + x)]);
             if (topic)
                q += sprintf(q, "    %s", topic);
             }
         x = 0;
         Reply(-214, buffer);
         }
     Reply(-214, "To report bugs in the implementation send email to");
     Reply(-214, "    vdr-bugs@cadsoft.de");
     }
  Reply(214, "End of HELP info");
}

void cSVDRP::CmdHITK(const char *Option)
{
  if (*Option) {
     eKeys k = cKey::FromString(Option);
     if (k != kNone) {
        cRemote::Put(k);
        Reply(250, "Key \"%s\" accepted", Option);
        }
     else
        Reply(504, "Unknown key: \"%s\"", Option);
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
  if (*Option) {
     if (isnumber(Option)) {
        cChannel *channel = Channels.GetByNumber(strtol(Option, NULL, 10));
        if (channel)
           Reply(250, "%d %s", channel->Number(), channel->ToText());
        else
           Reply(501, "Channel \"%s\" not defined", Option);
        }
     else {
        int i = 1;
        cChannel *next = NULL;
        while (i <= Channels.MaxNumber()) {
              cChannel *channel = Channels.GetByNumber(i, 1);
              if (channel) {
                 if (strcasestr(channel->Name(), Option)) {
                    if (next)
                       Reply(-250, "%d %s", next->Number(), next->ToText());
                    next = channel;
                    }
                 }
              else {
                 Reply(501, "Channel \"%d\" not found", i);
                 return;
                 }
              i = channel->Number() + 1;
              }
        if (next)
           Reply(250, "%d %s", next->Number(), next->ToText());
        else
           Reply(501, "Channel \"%s\" not defined", Option);
        }
     }
  else if (Channels.MaxNumber() >= 1) {
     int i = 1;
     while (i <= Channels.MaxNumber()) {
           cChannel *channel = Channels.GetByNumber(i, 1);
           if (channel)
              Reply(channel->Number() < Channels.MaxNumber() ? -250 : 250, "%d %s", channel->Number(), channel->ToText());
           else
              Reply(501, "Channel \"%d\" not found", i);
           i = channel->Number() + 1;
           }
     }
  else
     Reply(550, "No channels defined");
}

void cSVDRP::CmdLSTE(const char *Option)
{
  cMutexLock MutexLock;
  const cSchedules *Schedules = cSIProcessor::Schedules(MutexLock);
  if (Schedules) {
     FILE *f = fdopen(file, "w");
     if (f) {
        Schedules->Dump(f, "215-");
        fflush(f);
        Reply(215, "End of EPG data");
        // don't 'fclose(f)' here!
        }
     else
        Reply(451, "Can't open file connection");
     }
  else
     Reply(451, "Can't get EPG data");
}

void cSVDRP::CmdLSTR(const char *Option)
{
  bool recordings = Recordings.Load();
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = Recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           if (recording->Summary()) {
              char *summary = strdup(recording->Summary());
              Reply(250, "%s", strreplace(summary,'\n','|'));
              free(summary);
              }
           else
              Reply(550, "No summary availabe");
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
  if (*Option) {
     if (isnumber(Option)) {
        cTimer *timer = Timers.Get(strtol(Option, NULL, 10) - 1);
        if (timer)
           Reply(250, "%d %s", timer->Index() + 1, timer->ToText());
        else
           Reply(501, "Timer \"%s\" not defined", Option);
        }
     else
        Reply(501, "Error in timer number \"%s\"", Option);
     }
  else if (Timers.Count()) {
     for (int i = 0; i < Timers.Count(); i++) {
         cTimer *timer = Timers.Get(i);
        if (timer)
           Reply(i < Timers.Count() - 1 ? -250 : 250, "%d %s", timer->Index() + 1, timer->ToText());
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
     free(message);
     message = strdup(Option);
     isyslog("SVDRP message: '%s'", message);
     Reply(250, "Message stored");
     }
  else if (message)
     Reply(250, "%s", message);
  else
     Reply(550, "No pending message");
}

void cSVDRP::CmdMODC(const char *Option)
{
  if (*Option) {
     char *tail;
     int n = strtol(Option, &tail, 10);
     if (tail && tail != Option) {
        tail = skipspace(tail);
        cChannel *channel = Channels.GetByNumber(n);
        if (channel) {
           cChannel ch;
           if (ch.Parse(tail, true)) {
              if (Channels.HasUniqueChannelID(&ch, channel)) {
                 *channel = ch;
                 Channels.ReNumber();
                 Channels.Save();
                 isyslog("modifed channel %d %s", channel->Number(), channel->ToText());
                 Reply(250, "%d %s", channel->Number(), channel->ToText());
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
        cTimer *timer = Timers.Get(n - 1);
        if (timer) {
           cTimer t = *timer;
           if (strcasecmp(tail, "ON") == 0)
              t.SetActive(taActive);
           else if (strcasecmp(tail, "OFF") == 0)
              t.SetActive(taInactive);
           else if (!t.Parse(tail)) {
              Reply(501, "Error in timer settings");
              return;
              }
           *timer = t;
           Timers.Save();
           isyslog("timer %d modified (%s)", timer->Index() + 1, timer->Active() ? "active" : "inactive");
           Reply(250, "%d %s", timer->Index() + 1, timer->ToText());
           }
        else
           Reply(501, "Timer \"%d\" not defined", n);
        }
     else
        Reply(501, "Error in timer number");
     }
  else
     Reply(501, "Missing timer settings");
}

void cSVDRP::CmdMOVC(const char *Option)
{
  //TODO combine this with menu action (timers must be updated)
  Reply(502, "MOVC not yet implemented");
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
     if (ch.Parse(Option, true)) {
        if (Channels.HasUniqueChannelID(&ch)) {
           cChannel *channel = new cChannel;
           *channel = ch;
           Channels.Add(channel);
           Channels.ReNumber();
           Channels.Save();
           isyslog("new channel %d %s", channel->Number(), channel->ToText());
           Reply(250, "%d %s", channel->Number(), channel->ToText());
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
           Timers.Save();
           isyslog("timer %d added", timer->Index() + 1);
           Reply(250, "%d %s", timer->Index() + 1, timer->ToText());
           return;
           }
        else
           Reply(550, "Timer already defined: %d %s", t->Index() + 1, t->ToText());
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
     if (!*Option) {
        char *s = ctime(&Start);
        s[strlen(s) - 1] = 0; // strip trailing newline
        Reply(250, "%d %s", Number, s);
        }
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

void cSVDRP::CmdPUTE(const char *Option)
{
  delete PUTEhandler;
  PUTEhandler = new cPUTEhandler;
  Reply(PUTEhandler->Status(), PUTEhandler->Message());
  if (PUTEhandler->Status() != 354)
     DELETENULL(PUTEhandler);
}

void cSVDRP::CmdSTAT(const char *Option)
{
  if (*Option) {
     if (strcasecmp(Option, "DISK") == 0) {
        int FreeMB;
        int Percent = VideoDiskSpace(&FreeMB);
        int Total = (FreeMB / (100 - Percent)) * 100;
        Reply(250, "%dMB %dMB %d%%", Total, FreeMB, Percent);
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
        cTimer *t = Timers.GetTimer(timer);
        if (t) {
           t->Parse(Option);
           delete timer;
           timer = t;
           isyslog("timer %d updated", timer->Index() + 1);
           }
        else {
           Timers.Add(timer);
           isyslog("timer %d added", timer->Index() + 1);
           }
        Timers.Save();
        Reply(250, "%d %s", timer->Index() + 1, timer->ToText());
        return;
        }
     else
        Reply(501, "Error in timer settings");
     delete timer;
     }
  else
     Reply(501, "Missing timer settings");
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
        Reply(PUTEhandler->Status(), PUTEhandler->Message());
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
  else if (CMD("PUTE"))  CmdPUTE(s);
  else if (CMD("STAT"))  CmdSTAT(s);
  else if (CMD("UPDT"))  CmdUPDT(s);
  else if (CMD("VOLU"))  CmdVOLU(s);
  else if (CMD("QUIT"))  Close();
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
        Reply(220, "%s SVDRP VideoDiskRecorder %s; %s", buffer, VDRVERSION, ctime(&now));
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
                 }
              else if (c == 0x04 && numChars == 0) {
                 // end of file (only at beginning of line)
                 Close();
                 }
              else if (c == 0x08 || c == 0x7F) {
                 // backspace or delete (last character)
                 if (numChars > 0)
                    numChars--;
                 }
              else if (c <= 0x03 || c == 0x0D) {
                 // ignore control characters
                 }
              else if (numChars < sizeof(cmdLine) - 1) {
                 cmdLine[numChars++] = c;
                 cmdLine[numChars] = 0;
                 }
              else {
                 Reply(501, "Command line too long");
                 esyslog("SVDRP: command line too long: '%s'", cmdLine);
                 numChars = 0;
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
        Close(true);
        }
     return true;
     }
  return false;
}

char *cSVDRP::GetMessage(void)
{
  char *s = message;
  message = NULL;
  return s;
}

//TODO more than one connection???
