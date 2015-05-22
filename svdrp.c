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
 * $Id: svdrp.c 4.2 2015/05/22 11:01:33 kls Exp $
 */

#include "svdrp.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
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
#include "eitscan.h"
#include "keys.h"
#include "menu.h"
#include "plugin.h"
#include "recording.h"
#include "remote.h"
#include "skins.h"
#include "thread.h"
#include "timers.h"
#include "videodir.h"

static bool DumpSVDRPDataTransfer = false;

#define dbgsvdrp(a...) if (DumpSVDRPDataTransfer) fprintf(stderr, a)

static const char *HostName(void)
{
  static char buffer[HOST_NAME_MAX] = "";
  if (!*buffer) {
     if (gethostname(buffer, sizeof(buffer)) < 0) {
        LOG_ERROR;
        strcpy(buffer, "vdr");
        }
     }
  return buffer;
}

// --- cIpAddress ------------------------------------------------------------

class cIpAddress {
private:
  cString address;
  int port;
  cString connection;
public:
  cIpAddress(void);
  cIpAddress(const char *Address, int Port);
  const char *Address(void) const { return address; }
  int Port(void) const { return port; }
  void Set(const char *Address, int Port);
  void Set(const sockaddr *SockAddr);
  const char *Connection(void) const { return connection; }
  };

cIpAddress::cIpAddress(void)
{
  Set(INADDR_ANY, 0);
}

cIpAddress::cIpAddress(const char *Address, int Port)
{
  Set(Address, Port);
}

void cIpAddress::Set(const char *Address, int Port)
{
  address = Address;
  port = Port;
  connection = cString::sprintf("%s:%d", *address, port);
}

void cIpAddress::Set(const sockaddr *SockAddr)
{
  const sockaddr_in *Addr = (sockaddr_in *)SockAddr;
  Set(inet_ntoa(Addr->sin_addr), ntohs(Addr->sin_port));
}

// --- cSocket ---------------------------------------------------------------

#define MAXUDPBUF 1024

class cSocket {
private:
  int port;
  bool tcp;
  int sock;
  cIpAddress lastIpAddress;
  bool IsOwnInterface(sockaddr_in *Addr);
public:
  cSocket(int Port, bool Tcp);
  ~cSocket();
  bool Listen(void);
  bool Connect(const char *Address);
  void Close(void);
  int Port(void) const { return port; }
  int Socket(void) const { return sock; }
  static bool SendDgram(const char *Dgram, int Port, const char *Address = NULL);
  int Accept(void);
  cString Discover(void);
  const cIpAddress *LastIpAddress(void) const { return &lastIpAddress; }
  };

cSocket::cSocket(int Port, bool Tcp)
{
  port = Port;
  tcp = Tcp;
  sock = -1;
}

cSocket::~cSocket()
{
  Close();
}

bool cSocket::IsOwnInterface(sockaddr_in *Addr)
{
  ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) >= 0) {
     bool Own = false;
     for (ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
         if (ifa->ifa_addr) {
            if (ifa->ifa_addr->sa_family == AF_INET) {
               sockaddr_in *addr = (sockaddr_in *)ifa->ifa_addr;
               if (addr->sin_addr.s_addr == Addr->sin_addr.s_addr) {
                  Own = true;
                  break;
                  }
               }
            }
         }
     freeifaddrs(ifaddr);
     return Own;
     }
  else
     LOG_ERROR;
  return false;
}

void cSocket::Close(void)
{
  if (sock >= 0) {
     close(sock);
     sock = -1;
     }
}

bool cSocket::Listen(void)
{
  if (sock < 0) {
     // create socket:
     sock = tcp ? socket(PF_INET, SOCK_STREAM, IPPROTO_IP) : socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
     if (sock < 0) {
        LOG_ERROR;
        return false;
        }
     // allow it to always reuse the same port:
     int ReUseAddr = 1;
     setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &ReUseAddr, sizeof(ReUseAddr));
     // configure port and ip:
     sockaddr_in Addr;
     memset(&Addr, 0, sizeof(Addr));
     Addr.sin_family = AF_INET;
     Addr.sin_port = htons(port);
     Addr.sin_addr.s_addr = SVDRPhosts.LocalhostOnly() ? htonl(INADDR_LOOPBACK) : htonl(INADDR_ANY);
     if (bind(sock, (sockaddr *)&Addr, sizeof(Addr)) < 0) {
        LOG_ERROR;
        Close();
        return false;
        }
     // make it non-blocking:
     int Flags = fcntl(sock, F_GETFL, 0);
     if (Flags < 0) {
        LOG_ERROR;
        return false;
        }
     Flags |= O_NONBLOCK;
     if (fcntl(sock, F_SETFL, Flags) < 0) {
        LOG_ERROR;
        return false;
        }
     if (tcp) {
        // listen to the socket:
        if (listen(sock, 1) < 0) {
           LOG_ERROR;
           return false;
           }
        }
     isyslog("SVDRP listening on port %d/%s", port, tcp ? "tcp" : "udp");
     }
  return true;
}

bool cSocket::Connect(const char *Address)
{
  if (sock < 0 && tcp) {
     // create socket:
     sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
     if (sock < 0) {
        LOG_ERROR;
        return false;
        }
     // configure port and ip:
     sockaddr_in Addr;
     memset(&Addr, 0, sizeof(Addr));
     Addr.sin_family = AF_INET;
     Addr.sin_port = htons(port);
     Addr.sin_addr.s_addr = inet_addr(Address);
     if (connect(sock, (sockaddr *)&Addr, sizeof(Addr)) < 0) {
        LOG_ERROR;
        Close();
        return false;
        }
     // make it non-blocking:
     int Flags = fcntl(sock, F_GETFL, 0);
     if (Flags < 0) {
        LOG_ERROR;
        return false;
        }
     Flags |= O_NONBLOCK;
     if (fcntl(sock, F_SETFL, Flags) < 0) {
        LOG_ERROR;
        return false;
        }
     isyslog("SVDRP > %s:%d connection established", Address, port);
     return true;
     }
  return false;
}

bool cSocket::SendDgram(const char *Dgram, int Port, const char *Address)
{
  // Create a socket:
  int Socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (Socket < 0) {
     LOG_ERROR;
     return false;
     }
  if (!Address) {
     // Enable broadcast:
     int One = 1;
     if (setsockopt(Socket, SOL_SOCKET, SO_BROADCAST, &One, sizeof(One)) < 0) {
        LOG_ERROR;
        close(Socket);
        return false;
        }
     }
  // Configure port and ip:
  sockaddr_in Addr;
  memset(&Addr, 0, sizeof(Addr));
  Addr.sin_family = AF_INET;
  Addr.sin_addr.s_addr = Address ? inet_addr(Address) : htonl(INADDR_BROADCAST);
  Addr.sin_port = htons(Port);
  // Send datagram:
  dsyslog("SVDRP > %s:%d send dgram '%s'", inet_ntoa(Addr.sin_addr), Port, Dgram);
  int Length = strlen(Dgram);
  int Sent = sendto(Socket, Dgram, Length, 0, (sockaddr *)&Addr, sizeof(Addr));
  if (Sent < 0)
     LOG_ERROR;
  close(Socket);
  return Sent == Length;
}

int cSocket::Accept(void)
{
  if (sock >= 0 && tcp) {
     sockaddr_in Addr;
     uint Size = sizeof(Addr);
     int NewSock = accept(sock, (sockaddr *)&Addr, &Size);
     if (NewSock >= 0) {
        bool Accepted = SVDRPhosts.Acceptable(Addr.sin_addr.s_addr);
        if (!Accepted) {
           const char *s = "Access denied!\n";
           if (write(NewSock, s, strlen(s)) < 0)
              LOG_ERROR;
           close(NewSock);
           NewSock = -1;
           }
        lastIpAddress.Set((sockaddr *)&Addr);
        isyslog("SVDRP < %s connection %s", lastIpAddress.Connection(), Accepted ? "accepted" : "DENIED");
        }
     else if (FATALERRNO)
        LOG_ERROR;
     return NewSock;
     }
  return -1;
}

cString cSocket::Discover(void)
{
  if (sock >= 0 && !tcp) {
     char buf[MAXUDPBUF];
     sockaddr_in Addr;
     uint Size = sizeof(Addr);
     int NumBytes = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr *)&Addr, &Size);
     if (NumBytes >= 0) {
        buf[NumBytes] = 0;
        if (!IsOwnInterface(&Addr)) {
           lastIpAddress.Set((sockaddr *)&Addr);
           if (!SVDRPhosts.Acceptable(Addr.sin_addr.s_addr)) {
              dsyslog("SVDRP < %s discovery ignored (%s)", lastIpAddress.Connection(), buf);
              return NULL;
              }
           if (!startswith(buf, "SVDRP:discover")) {
              dsyslog("SVDRP < %s discovery unrecognized (%s)", lastIpAddress.Connection(), buf);
              return NULL;
              }
           isyslog("SVDRP < %s discovery received (%s)", lastIpAddress.Connection(), buf);
           return buf;
           }
        }
     else if (FATALERRNO)
        LOG_ERROR;
     }
  return NULL;
}

// --- cPUTEhandler ----------------------------------------------------------

class cPUTEhandler {
private:
  FILE *f;
  int status;
  const char *message;
public:
  cPUTEhandler(void);
  ~cPUTEhandler();
  bool Process(const char *s);
  int Status(void) { return status; }
  const char *Message(void) { return message; }
  };

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

// --- cSVDRPServer ----------------------------------------------------------

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
  "LSTR [ <number> [ path ] ]\n"
  "    List recordings. Without option, all recordings are listed. Otherwise\n"
  "    the information for the given recording is listed. If a recording\n"
  "    number and the keyword 'path' is given, the actual file name of that\n"
  "    recording's directory is listed.",
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
  "MOVR <number> <new name>\n"
  "    Move the recording with the given number. Before a recording can be\n"
  "    moved, an LSTR command must have been executed in order to retrieve\n"
  "    the recording numbers. The numbers don't change during subsequent MOVR\n"
  "    commands.\n",
  "NEWC <settings>\n"
  "    Create a new channel. Settings must be in the same format as returned\n"
  "    by the LSTC command.",
  "NEWT <settings>\n"
  "    Create a new timer. Settings must be in the same format as returned\n"
  "    by the LSTT command.",
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
  "PING\n"
  "    Used by peer-to-peer connections between VDRs to keep the connection\n"
  "    from timing out. May be used at any time and simply returns a line of\n"
  "    the form '<hostname> is alive'.",
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

static cString grabImageDir;

class cSVDRPServer {
private:
  int socket;
  cString connection;
  cFile file;
  cRecordings recordings;
  cPUTEhandler *PUTEhandler;
  int numChars;
  int length;
  char *cmdLine;
  time_t lastActivity;
  void Close(bool SendReply = false, bool Timeout = false);
  bool Send(const char *s, int length = -1);
  void Reply(int Code, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
  void PrintHelpTopics(const char **hp);
  void CmdCHAN(const char *Option);
  void CmdCLRE(const char *Option);
  void CmdDELC(const char *Option);
  void CmdDELR(const char *Option);
  void CmdDELT(const char *Option);
  void CmdEDIT(const char *Option);
  void CmdGRAB(const char *Option);
  void CmdHELP(const char *Option);
  void CmdHITK(const char *Option);
  void CmdLSTC(const char *Option);
  void CmdLSTE(const char *Option);
  void CmdLSTR(const char *Option);
  void CmdLSTT(const char *Option);
  void CmdMESG(const char *Option);
  void CmdMODC(const char *Option);
  void CmdMODT(const char *Option);
  void CmdMOVC(const char *Option);
  void CmdMOVR(const char *Option);
  void CmdNEWC(const char *Option);
  void CmdNEWT(const char *Option);
  void CmdNEXT(const char *Option);
  void CmdPING(const char *Option);
  void CmdPLAY(const char *Option);
  void CmdPLUG(const char *Option);
  void CmdPUTE(const char *Option);
  void CmdREMO(const char *Option);
  void CmdSCAN(const char *Option);
  void CmdSTAT(const char *Option);
  void CmdUPDT(const char *Option);
  void CmdUPDR(const char *Option);
  void CmdVOLU(const char *Option);
  void Execute(char *Cmd);
public:
  cSVDRPServer(int Socket, const char *Connection);
  ~cSVDRPServer();
  bool HasConnection(void) { return file.IsOpen(); }
  bool Process(void);
  };

static cPoller SVDRPServerPoller;
static cPoller SVDRPClientPoller;

cSVDRPServer::cSVDRPServer(int Socket, const char *Connection)
{
  socket = Socket;
  connection = Connection;
  PUTEhandler = NULL;
  numChars = 0;
  length = BUFSIZ;
  cmdLine = MALLOC(char, length);
  lastActivity = time(NULL);
  if (file.Open(socket)) {
     time_t now = time(NULL);
     Reply(220, "%s SVDRP VideoDiskRecorder %s; %s; %s", HostName(), VDRVERSION, *TimeToString(now), cCharSetConv::SystemCharacterTable() ? cCharSetConv::SystemCharacterTable() : "UTF-8");
     SVDRPServerPoller.Add(file, false);
     }
  dsyslog("SVDRP < %s server created", *connection);
}

cSVDRPServer::~cSVDRPServer()
{
  Close(true);
  free(cmdLine);
  dsyslog("SVDRP < %s server destroyed", *connection);
}

void cSVDRPServer::Close(bool SendReply, bool Timeout)
{
  if (file.IsOpen()) {
     if (SendReply) {
        Reply(221, "%s closing connection%s", HostName(), Timeout ? " (timeout)" : "");
        }
     isyslog("SVDRP < %s connection closed", *connection);
     SVDRPServerPoller.Del(file, false);
     file.Close();
     DELETENULL(PUTEhandler);
     }
  close(socket);
}

bool cSVDRPServer::Send(const char *s, int length)
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

void cSVDRPServer::Reply(int Code, const char *fmt, ...)
{
  if (file.IsOpen()) {
     if (Code != 0) {
        va_list ap;
        va_start(ap, fmt);
        cString buffer = cString::vsprintf(fmt, ap);
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
        esyslog("SVDRP < %s zero return code!", *connection);
        }
     }
}

void cSVDRPServer::PrintHelpTopics(const char **hp)
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

void cSVDRPServer::CmdCHAN(const char *Option)
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

void cSVDRPServer::CmdCLRE(const char *Option)
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
              for (cTimer *Timer = Timers.First(); Timer; Timer = Timers.Next(Timer)) {
                  if (ChannelID == Timer->Channel()->GetChannelID().ClrRid())
                     Timer->SetEvent(NULL);
                  }
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
     cEitFilter::SetDisableUntil(time(NULL) + EITDISABLETIME);
     if (cSchedules::ClearAll()) {
        Reply(250, "EPG data cleared");
        cEitFilter::SetDisableUntil(time(NULL) + EITDISABLETIME);
        }
     else
        Reply(451, "Error while clearing EPG data");
     }
}

void cSVDRPServer::CmdDELC(const char *Option)
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
              isyslog("SVDRP < %s channel %s deleted", *connection, Option);
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

static cString RecordingInUseMessage(int Reason, const char *RecordingId, cRecording *Recording)
{
  cRecordControl *rc;
  if ((Reason & ruTimer) != 0 && (rc = cRecordControls::GetRecordControl(Recording->FileName())) != NULL)
     return cString::sprintf("Recording \"%s\" is in use by timer %d", RecordingId, rc->Timer()->Index() + 1);
  else if ((Reason & ruReplay) != 0)
     return cString::sprintf("Recording \"%s\" is being replayed", RecordingId);
  else if ((Reason & ruCut) != 0)
     return cString::sprintf("Recording \"%s\" is being edited", RecordingId);
  else if ((Reason & (ruMove | ruCopy)) != 0)
     return cString::sprintf("Recording \"%s\" is being copied/moved", RecordingId);
  else if (Reason)
     return cString::sprintf("Recording \"%s\" is in use", RecordingId);
  return NULL;
}

void cSVDRPServer::CmdDELR(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           if (int RecordingInUse = recording->IsInUse())
              Reply(550, "%s", *RecordingInUseMessage(RecordingInUse, Option, recording));
           else {
              if (recording->Delete()) {
                 Reply(250, "Recording \"%s\" deleted", Option);
                 Recordings.DelByName(recording->FileName());
                 }
              else
                 Reply(554, "Error while deleting recording!");
              }
           }
        else
           Reply(550, "Recording \"%s\" not found%s", Option, recordings.Count() ? "" : " (use LSTR before deleting)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", Option);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRPServer::CmdDELT(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        if (!Timers.BeingEdited()) {
           cTimer *timer = Timers.Get(strtol(Option, NULL, 10) - 1);
           if (timer) {
              if (!timer->Recording()) {
                 isyslog("SVDRP < %s deleting timer %s", *connection, *timer->ToDescr());
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

void cSVDRPServer::CmdEDIT(const char *Option)
{
  if (*Option) {
     if (isnumber(Option)) {
        cRecording *recording = recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           cMarks Marks;
           if (Marks.Load(recording->FileName(), recording->FramesPerSecond(), recording->IsPesRecording()) && Marks.Count()) {
              if (RecordingsHandler.Add(ruCut, recording->FileName()))
                 Reply(250, "Editing recording \"%s\" [%s]", Option, recording->Title());
              else
                 Reply(554, "Can't start editing process");
              }
           else
              Reply(554, "No editing marks defined");
           }
        else
           Reply(550, "Recording \"%s\" not found%s", Option, recordings.Count() ? "" : " (use LSTR before editing)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", Option);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRPServer::CmdGRAB(const char *Option)
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
        if (*grabImageDir) {
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
                 dsyslog("SVDRP < %s grabbed image to %s", *connection, FileName);
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

void cSVDRPServer::CmdHELP(const char *Option)
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

void cSVDRPServer::CmdHITK(const char *Option)
{
  if (*Option) {
     if (!cRemote::Enabled()) {
        Reply(550, "Remote control currently disabled (key \"%s\" discarded)", Option);
        return;
        }
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

void cSVDRPServer::CmdLSTC(const char *Option)
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

void cSVDRPServer::CmdLSTE(const char *Option)
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

void cSVDRPServer::CmdLSTR(const char *Option)
{
  int Number = 0;
  bool Path = false;
  recordings.Update(true);
  if (*Option) {
     char buf[strlen(Option) + 1];
     strcpy(buf, Option);
     const char *delim = " \t";
     char *strtok_next;
     char *p = strtok_r(buf, delim, &strtok_next);
     while (p) {
           if (!Number) {
              if (isnumber(p))
                 Number = strtol(p, NULL, 10);
              else {
                 Reply(501, "Error in recording number \"%s\"", Option);
                 return;
                 }
              }
           else if (strcasecmp(p, "PATH") == 0)
              Path = true;
           else {
              Reply(501, "Unknown option: \"%s\"", p);
              return;
              }
           p = strtok_r(NULL, delim, &strtok_next);
           }
     if (Number) {
        cRecording *recording = recordings.Get(strtol(Option, NULL, 10) - 1);
        if (recording) {
           FILE *f = fdopen(file, "w");
           if (f) {
              if (Path)
                 Reply(250, "%s", recording->FileName());
              else {
                 recording->Info()->Write(f, "215-");
                 fflush(f);
                 Reply(215, "End of recording information");
                 }
              // don't 'fclose(f)' here!
              }
           else
              Reply(451, "Can't open file connection");
           }
        else
           Reply(550, "Recording \"%s\" not found", Option);
        }
     }
  else if (recordings.Count()) {
     cRecording *recording = recordings.First();
     while (recording) {
           Reply(recording == recordings.Last() ? 250 : -250, "%d %s", recording->Index() + 1, recording->Title(' ', true));
           recording = recordings.Next(recording);
           }
     }
  else
     Reply(550, "No recordings available");
}

void cSVDRPServer::CmdLSTT(const char *Option)
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

void cSVDRPServer::CmdMESG(const char *Option)
{
  if (*Option) {
     isyslog("SVDRP < %s message '%s'", *connection, Option);
     Skins.QueueMessage(mtInfo, Option);
     Reply(250, "Message queued");
     }
  else
     Reply(501, "Missing message");
}

void cSVDRPServer::CmdMODC(const char *Option)
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
                    isyslog("SVDRP < %s modifed channel %d %s", *connection, channel->Number(), *channel->ToText());
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

void cSVDRPServer::CmdMODT(const char *Option)
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
              isyslog("SVDRP < %s timer %s modified (%s)", *connection, *timer->ToDescr(), timer->HasFlags(tfActive) ? "active" : "inactive");
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

void cSVDRPServer::CmdMOVC(const char *Option)
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
                       isyslog("SVDRP < %s channel %d moved to %d", *connection, FromNumber, ToNumber);
                       Reply(250,"Channel \"%d\" moved to \"%d\"", From, To);
                       }
                    else
                       Reply(501, "Can't move channel to same position");
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

void cSVDRPServer::CmdMOVR(const char *Option)
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
        cRecording *recording = recordings.Get(strtol(num, NULL, 10) - 1);
        if (recording) {
           if (int RecordingInUse = recording->IsInUse())
              Reply(550, "%s", *RecordingInUseMessage(RecordingInUse, Option, recording));
           else {
              if (c)
                 option = skipspace(++option);
              if (*option) {
                 cString oldName = recording->Name();
                 if ((recording = Recordings.GetByName(recording->FileName())) != NULL && recording->ChangeName(option))
                    Reply(250, "Recording \"%s\" moved to \"%s\"", *oldName, recording->Name());
                 else
                    Reply(554, "Error while moving recording \"%s\" to \"%s\"!", *oldName, option);
                 }
              else
                 Reply(501, "Missing new recording name");
              }
           }
        else
           Reply(550, "Recording \"%s\" not found%s", num, recordings.Count() ? "" : " (use LSTR before moving)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", num);
     free(opt);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRPServer::CmdNEWC(const char *Option)
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
           isyslog("SVDRP < %s new channel %d %s", *connection, channel->Number(), *channel->ToText());
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

void cSVDRPServer::CmdNEWT(const char *Option)
{
  if (*Option) {
     cTimer *timer = new cTimer;
     if (timer->Parse(Option)) {
        Timers.Add(timer);
        Timers.SetModified();
        isyslog("SVDRP < %s timer %s added", *connection, *timer->ToDescr());
        Reply(250, "%d %s", timer->Index() + 1, *timer->ToText());
        return;
        }
     else
        Reply(501, "Error in timer settings");
     delete timer;
     }
  else
     Reply(501, "Missing timer settings");
}

void cSVDRPServer::CmdNEXT(const char *Option)
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

void cSVDRPServer::CmdPING(const char *Option)
{
  Reply(250, "%s is alive", HostName());
}

void cSVDRPServer::CmdPLAY(const char *Option)
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
        cRecording *recording = recordings.Get(strtol(num, NULL, 10) - 1);
        if (recording) {
           if (c)
              option = skipspace(++option);
           cReplayControl::SetRecording(NULL);
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
           cReplayControl::SetRecording(recording->FileName());
           cControl::Launch(new cReplayControl);
           cControl::Attach();
           Reply(250, "Playing recording \"%s\" [%s]", num, recording->Title());
           }
        else
           Reply(550, "Recording \"%s\" not found%s", num, recordings.Count() ? "" : " (use LSTR before playing)");
        }
     else
        Reply(501, "Error in recording number \"%s\"", num);
     free(opt);
     }
  else
     Reply(501, "Missing recording number");
}

void cSVDRPServer::CmdPLUG(const char *Option)
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

void cSVDRPServer::CmdPUTE(const char *Option)
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

void cSVDRPServer::CmdREMO(const char *Option)
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

void cSVDRPServer::CmdSCAN(const char *Option)
{
  EITScanner.ForceScan();
  Reply(250, "EPG scan triggered");
}

void cSVDRPServer::CmdSTAT(const char *Option)
{
  if (*Option) {
     if (strcasecmp(Option, "DISK") == 0) {
        int FreeMB, UsedMB;
        int Percent = cVideoDirectory::VideoDiskSpace(&FreeMB, &UsedMB);
        Reply(250, "%dMB %dMB %d%%", FreeMB + UsedMB, FreeMB, Percent);
        }
     else
        Reply(501, "Invalid Option \"%s\"", Option);
     }
  else
     Reply(501, "No option given");
}

void cSVDRPServer::CmdUPDT(const char *Option)
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
              isyslog("SVDRP < %s timer %s updated", *connection, *timer->ToDescr());
              }
           else {
              Timers.Add(timer);
              isyslog("SVDRP < %s timer %s added", *connection, *timer->ToDescr());
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

void cSVDRPServer::CmdUPDR(const char *Option)
{
  Recordings.Update(false);
  Reply(250, "Re-read of recordings directory triggered");
}

void cSVDRPServer::CmdVOLU(const char *Option)
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

void cSVDRPServer::Execute(char *Cmd)
{
  // handle PUTE data:
  if (PUTEhandler) {
     if (!PUTEhandler->Process(Cmd)) {
        Reply(PUTEhandler->Status(), "%s", PUTEhandler->Message());
        DELETENULL(PUTEhandler);
        }
     cEitFilter::SetDisableUntil(time(NULL) + EITDISABLETIME); // re-trigger the timeout, in case there is very much EPG data
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
  else if (CMD("MOVR"))  CmdMOVR(s);
  else if (CMD("NEWC"))  CmdNEWC(s);
  else if (CMD("NEWT"))  CmdNEWT(s);
  else if (CMD("NEXT"))  CmdNEXT(s);
  else if (CMD("PING"))  CmdPING(s);
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

bool cSVDRPServer::Process(void)
{
  if (file.IsOpen()) {
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
                       esyslog("SVDRP < %s ERROR: out of memory", *connection);
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
              isyslog("SVDRP < %s lost connection to client", *connection);
              Close();
              }
           }
     if (Setup.SVDRPTimeout && time(NULL) - lastActivity > Setup.SVDRPTimeout) {
        isyslog("SVDRP < %s timeout on connection", *connection);
        Close(true, true);
        }
     }
  return file.IsOpen();
}

void SetSVDRPGrabImageDir(const char *GrabImageDir)
{
  grabImageDir = GrabImageDir;
}

// --- cSVDRPClient ----------------------------------------------------------

class cSVDRPClient {
private:
  cIpAddress ipAddress;
  cSocket socket;
  cString serverName;
  int timeout;
  cTimeMs pingTime;
  cFile file;
  void Close(void);
public:
  cSVDRPClient(const char *Address, int Port, const char *ServerName, int Timeout);
  ~cSVDRPClient();
  const char *ServerName(void) const { return serverName; }
  const char *Connection(void) const { return ipAddress.Connection(); }
  bool HasAddress(const char *Address, int Port) const;
  bool Send(const char *Command);
  bool Ping(void);
  bool Process(cStringList *Response = NULL);
  bool Execute(const char *Command, cStringList *Response);
  };

cSVDRPClient::cSVDRPClient(const char *Address, int Port, const char *ServerName, int Timeout)
:ipAddress(Address, Port)
,socket(Port, true)
{
  serverName = ServerName;
  timeout = Timeout * 1000 * 9 / 10; // ping after 90% of timeout
  pingTime.Set(timeout);
  if (socket.Connect(Address)) {
     if (file.Open(socket.Socket()))
        SVDRPClientPoller.Add(file, false);
     }
  dsyslog("SVDRP > %s client created for '%s'", ipAddress.Connection(), *serverName);
  SendSVDRPDiscover(Address);
}

cSVDRPClient::~cSVDRPClient()
{
  Close();
  dsyslog("SVDRP > %s client destroyed for '%s'", ipAddress.Connection(), *serverName);
}

void cSVDRPClient::Close(void)
{
  if (file.IsOpen()) {
     SVDRPClientPoller.Del(file, false);
     file.Close();
     socket.Close();
     }
}

bool cSVDRPClient::HasAddress(const char *Address, int Port) const
{
  return strcmp(ipAddress.Address(), Address) == 0 && ipAddress.Port() == Port;
}

bool cSVDRPClient::Send(const char *Command)
{
  pingTime.Set(timeout);
  dbgsvdrp("> %s: %s\n", *serverName, Command);
  if (safe_write(file, Command, strlen(Command) + 1) < 0) {
     LOG_ERROR;
     return false;
     }
  return true;
}

bool cSVDRPClient::Ping(void)
{
  cSVDRPCommand Cmd(serverName, "PING");
  if (Cmd.Execute())
     return Cmd.Response(0) && atoi(Cmd.Response(0)) == 250;
  return false;
}

bool cSVDRPClient::Process(cStringList *Response)
{
  if (file.IsOpen()) {
     char input[BUFSIZ];
     int numChars = 0;
#define SVDRPResonseTimeout 5000 // ms
     cTimeMs Timeout(SVDRPResonseTimeout);
     for (;;) {
         if (file.Ready(false)) {
            unsigned char c;
            int r = safe_read(file, &c, 1);
            if (r > 0) {
               if (c == '\n' || c == 0x00) {
                  // strip trailing whitespace:
                  while (numChars > 0 && strchr(" \t\r\n", input[numChars - 1]))
                        input[--numChars] = 0;
                  // make sure the string is terminated:
                  input[numChars] = 0;
                  dbgsvdrp("< %s: %s\n", *serverName, input);
                  if (Response) {
                     Response->Append(strdup(input));
                     if (input[3] != '-') // no more lines will follow
                        break;
                     }
                  else {
                     switch (atoi(input)) {
                       case 220: if (numChars > 4) {
                                    char *n = input + 4;
                                    if (char *t = strchr(n, ' ')) {
                                       *t = 0;
                                       if (strcmp(n, serverName) != 0) {
                                          serverName = n;
                                          dsyslog("SVDRP < %s remote server name is '%s'", ipAddress.Connection(), *serverName);
                                          }
                                       }
                                    }
                                 break;
                       case 221: dsyslog("SVDRP < %s remote server closed connection to '%s'", ipAddress.Connection(), *serverName);
                                 Close();
                                 break;
                       }
                     }
                  numChars = 0;
                  }
               else {
                  if (numChars >= int(sizeof(input))) {
                     esyslog("SVDRP < %s ERROR: out of memory", ipAddress.Connection());
                     Close();
                     break;
                     }
                  input[numChars++] = c;
                  input[numChars] = 0;
                  }
               Timeout.Set(SVDRPResonseTimeout);
               }
            else if (r <= 0) {
               isyslog("SVDRP < %s lost connection to remote server '%s'", ipAddress.Connection(), *serverName);
               Close();
               }
            }
         else if (!Response)
            break;
         else if (Timeout.TimedOut()) {
           esyslog("SVDRP < %s timeout while waiting for response from '%s'", ipAddress.Connection(), *serverName);
            return false;
            }
         }
     if (pingTime.TimedOut())
        Ping();
     }
  return file.IsOpen();
}

bool cSVDRPClient::Execute(const char *Command, cStringList *Response)
{
  Response->Clear();
  return Send(Command) && Process(Response);
}

// --- cSVDRPServerHandler ---------------------------------------------------

class cSVDRPServerHandler : public cThread {
private:
  cMutex mutex;
  bool ready;
  cSocket tcpSocket;
  cVector<cSVDRPServer *> serverConnections;
  void HandleServerConnection(void);
  void ProcessConnections(void);
protected:
  virtual void Action(void);
public:
  cSVDRPServerHandler(int TcpPort);
  virtual ~cSVDRPServerHandler();
  void WaitUntilReady(void);
  };

cSVDRPServerHandler::cSVDRPServerHandler(int TcpPort)
:cThread("SVDRP server handler", true)
,tcpSocket(TcpPort, true)
{
  ready = false;
}

cSVDRPServerHandler::~cSVDRPServerHandler()
{
  Cancel(3);
  for (int i = 0; i < serverConnections.Size(); i++)
      delete serverConnections[i];
}

void cSVDRPServerHandler::WaitUntilReady(void)
{
  cTimeMs Timeout(3000);
  while (!ready && !Timeout.TimedOut())
        cCondWait::SleepMs(10);
}

void cSVDRPServerHandler::ProcessConnections(void)
{
  cMutexLock MutexLock(&mutex);
  for (int i = 0; i < serverConnections.Size(); i++) {
      if (!serverConnections[i]->Process()) {
         delete serverConnections[i];
         serverConnections.Remove(i);
         i--;
         }
      }
}

void cSVDRPServerHandler::HandleServerConnection(void)
{
  int NewSocket = tcpSocket.Accept();
  if (NewSocket >= 0)
     serverConnections.Append(new cSVDRPServer(NewSocket, tcpSocket.LastIpAddress()->Connection()));
}

void cSVDRPServerHandler::Action(void)
{
  if (tcpSocket.Listen()) {
     SVDRPServerPoller.Add(tcpSocket.Socket(), false);
     ready = true;
     while (Running()) {
           SVDRPServerPoller.Poll(1000);
           cMutexLock MutexLock(&mutex);
           HandleServerConnection();
           ProcessConnections();
           }
     SVDRPServerPoller.Del(tcpSocket.Socket(), false);
     tcpSocket.Close();
     }
}

// --- cSVDRPClientHandler ---------------------------------------------------

class cSVDRPClientHandler : public cThread {
private:
  cMutex mutex;
  cSocket udpSocket;
  int tcpPort;
  cVector<cSVDRPClient *> clientConnections;
  void HandleClientConnection(void);
  void ProcessConnections(void);
  cSVDRPClient *GetClientForServer(const char *ServerName);
protected:
  virtual void Action(void);
public:
  cSVDRPClientHandler(int UdpPort, int TcpPort);
  virtual ~cSVDRPClientHandler();
  void SendDiscover(const char *Address = NULL);
  bool Execute(const char *ServerName, const char *Command, cStringList *Response);
  bool GetServerNames(cStringList *ServerNames);
  };

cSVDRPClientHandler::cSVDRPClientHandler(int UdpPort, int TcpPort)
:cThread("SVDRP client handler", true)
,udpSocket(UdpPort, false)
{
  tcpPort = TcpPort;
}

cSVDRPClientHandler::~cSVDRPClientHandler()
{
  Cancel(3);
  for (int i = 0; i < clientConnections.Size(); i++)
      delete clientConnections[i];
}

cSVDRPClient *cSVDRPClientHandler::GetClientForServer(const char *ServerName)
{
  for (int i = 0; i < clientConnections.Size(); i++) {
      if (strcmp(clientConnections[i]->ServerName(), ServerName) == 0)
         return clientConnections[i];
      }
  return NULL;
}

void cSVDRPClientHandler::SendDiscover(const char *Address)
{
  cMutexLock MutexLock(&mutex);
  cString Dgram = cString::sprintf("SVDRP:discover name:%s port:%d vdrversion:%d apiversion:%d timeout:%d", HostName(), tcpPort, VDRVERSNUM, APIVERSNUM, Setup.SVDRPTimeout);
  udpSocket.SendDgram(Dgram, udpSocket.Port(), Address);
}

void cSVDRPClientHandler::ProcessConnections(void)
{
  cMutexLock MutexLock(&mutex);
  for (int i = 0; i < clientConnections.Size(); i++) {
      if (!clientConnections[i]->Process()) {
         delete clientConnections[i];
         clientConnections.Remove(i);
         i--;
         }
      }
}

void cSVDRPClientHandler::HandleClientConnection(void)
{
  cString NewDiscover = udpSocket.Discover();
  if (*NewDiscover) {
     cString p = strgetval(NewDiscover, "port", ':');
     if (*p) {
        int Port = atoi(p);
        for (int i = 0; i < clientConnections.Size(); i++) {
            if (clientConnections[i]->HasAddress(udpSocket.LastIpAddress()->Address(), Port)) {
               dsyslog("SVDRP < %s connection to '%s' confirmed", clientConnections[i]->Connection(), clientConnections[i]->ServerName());
               return;
               }
            }
        cString ServerName = strgetval(NewDiscover, "name", ':');
        if (*ServerName) {
           cString t = strgetval(NewDiscover, "timeout", ':');
           if (*t) {
              int Timeout = atoi(t);
              if (Timeout > 10) // don't let it get too small
                 clientConnections.Append(new cSVDRPClient(udpSocket.LastIpAddress()->Address(), Port, ServerName, Timeout));
              else
                 esyslog("SVDRP < %s ERROR: invalid timeout (%d)", udpSocket.LastIpAddress()->Connection(), Timeout);
              }
           else
              esyslog("SVDRP < %s ERROR: missing timeout", udpSocket.LastIpAddress()->Connection());
           }
        else
           esyslog("SVDRP < %s ERROR: missing server name", udpSocket.LastIpAddress()->Connection());
        }
     else
        esyslog("SVDRP < %s ERROR: missing port number", udpSocket.LastIpAddress()->Connection());
     }
}

void cSVDRPClientHandler::Action(void)
{
  if (udpSocket.Listen()) {
     SVDRPClientPoller.Add(udpSocket.Socket(), false);
     SendDiscover();
     while (Running()) {
           SVDRPClientPoller.Poll(1000);
           cMutexLock MutexLock(&mutex);
           HandleClientConnection();
           ProcessConnections();
           }
     SVDRPClientPoller.Del(udpSocket.Socket(), false);
     udpSocket.Close();
     }
}

bool cSVDRPClientHandler::Execute(const char *ServerName, const char *Command, cStringList *Response)
{
  cMutexLock MutexLock(&mutex);
  if (cSVDRPClient *Client = GetClientForServer(ServerName))
     return Client->Execute(Command, Response);
  return false;
}

bool cSVDRPClientHandler::GetServerNames(cStringList *ServerNames)
{
  cMutexLock MutexLock(&mutex);
  ServerNames->Clear();
  for (int i = 0; i < clientConnections.Size(); i++)
      ServerNames->Append(strdup(clientConnections[i]->ServerName()));
  return ServerNames->Size() > 0;
}

// --- SVDRP Handler ---------------------------------------------------------

static cMutex SVDRPHandlerMutex;
static cSVDRPServerHandler *SVDRPServerHandler = NULL;
static cSVDRPClientHandler *SVDRPClientHandler = NULL;

void StartSVDRPHandler(int TcpPort, int UdpPort)
{
  cMutexLock MutexLock(&SVDRPHandlerMutex);
  if (TcpPort && !SVDRPServerHandler) {
     SVDRPServerHandler = new cSVDRPServerHandler(TcpPort);
     SVDRPServerHandler->Start();
     SVDRPServerHandler->WaitUntilReady();
     }
  if (UdpPort && !SVDRPClientHandler) {
     SVDRPClientHandler = new cSVDRPClientHandler(UdpPort, TcpPort);
     SVDRPClientHandler->Start();
     }
}

void StopSVDRPHandler(void)
{
  cMutexLock MutexLock(&SVDRPHandlerMutex);
  delete SVDRPServerHandler;
  SVDRPServerHandler = NULL;
  delete SVDRPClientHandler;
  SVDRPClientHandler = NULL;
}

void SendSVDRPDiscover(const char *Address)
{
  cMutexLock MutexLock(&SVDRPHandlerMutex);
  if (SVDRPClientHandler)
     SVDRPClientHandler->SendDiscover(Address);
}

bool GetSVDRPServerNames(cStringList *ServerNames)
{
  cMutexLock MutexLock(&SVDRPHandlerMutex);
  if (SVDRPClientHandler)
     return SVDRPClientHandler->GetServerNames(ServerNames);
  return false;
}

// --- cSVDRPCommand ---------------------------------------------------------

cSVDRPCommand::cSVDRPCommand(const char *ServerName, const char *Command)
{
  serverName = ServerName;
  command = Command;
}

cSVDRPCommand::~cSVDRPCommand()
{
}

bool cSVDRPCommand::Execute(void)
{
  cMutexLock MutexLock(&SVDRPHandlerMutex);
  if (SVDRPClientHandler)
     return SVDRPClientHandler->Execute(serverName, command, &response);
  return false;
}
