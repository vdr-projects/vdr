/*
 * rcu.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: rcu.c 1.2 2012/03/07 14:22:44 kls Exp $
 */

#include <getopt.h>
#include <netinet/in.h>
#include <termios.h>
#include <unistd.h>
#include <vdr/plugin.h>
#include <vdr/remote.h>
#include <vdr/status.h>
#include <vdr/thread.h>
#include <vdr/tools.h>

static const char *VERSION        = "0.0.2";
static const char *DESCRIPTION    = "Remote Control Unit";

#define REPEATLIMIT      150 // ms
#define REPEATDELAY      350 // ms
#define HANDSHAKETIMEOUT  20 // ms
#define DEFAULTDEVICE    "/dev/ttyS1"

class cRcuRemote : public cRemote, private cThread, private cStatus {
private:
  enum { modeH = 'h', modeB = 'b', modeS = 's' };
  int f;
  unsigned char dp, code, mode;
  int number;
  unsigned int data;
  bool receivedCommand;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(int TimeoutMs = 0);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
  bool SendData(unsigned int n);
  void SetCode(unsigned char Code);
  void SetMode(unsigned char Mode);
  void SetNumber(int n, bool Hex = false);
  void SetPoints(unsigned char Dp, bool On);
  void SetString(const char *s);
  bool DetectCode(unsigned char *Code);
  virtual void Action(void);
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView);
  virtual void Recording(const cDevice *Device, const char *Name, const char *FileName, bool On);
public:
  cRcuRemote(const char *DeviceName);
  virtual ~cRcuRemote();
  virtual bool Ready(void);
  virtual bool Initialize(void);
  };

cRcuRemote::cRcuRemote(const char *DeviceName)
:cRemote("RCU")
,cThread("RCU remote control")
{
  dp = 0;
  mode = modeB;
  code = 0;
  number = 0;
  data = 0;
  receivedCommand = false;
  if ((f = open(DeviceName, O_RDWR | O_NONBLOCK)) >= 0) {
     struct termios t;
     if (tcgetattr(f, &t) == 0) {
        cfsetspeed(&t, B9600);
        cfmakeraw(&t);
        if (tcsetattr(f, TCSAFLUSH, &t) == 0) {
           SetNumber(8888);
           const char *Setup = GetSetup();
           if (Setup) {
              code = *Setup;
              SetCode(code);
              isyslog("connecting to %s remote control using code %c", Name(), code);
              }
           Start();
           return;
           }
        }
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cRcuRemote::~cRcuRemote()
{
  Cancel();
}

bool cRcuRemote::Ready(void)
{
  return f >= 0;
}

bool cRcuRemote::Initialize(void)
{
  if (f >= 0) {
     unsigned char Code = '0';
     isyslog("trying codes for %s remote control...", Name());
     for (;;) {
         if (DetectCode(&Code)) {
            code = Code;
            break;
            }
         }
     isyslog("established connection to %s remote control using code %c", Name(), code);
     char buffer[16];
     snprintf(buffer, sizeof(buffer), "%c", code);
     PutSetup(buffer);
     return true;
     }
  return false;
}

void cRcuRemote::Action(void)
{
#pragma pack(1)
  union {
    struct {
      unsigned short address;
      unsigned int command;
      } data;
    unsigned char raw[6];
    } buffer;
#pragma pack()

  time_t LastCodeRefresh = 0;
  cTimeMs FirstTime;
  unsigned char LastCode = 0, LastMode = 0;
  uint64_t LastCommand = ~0; // 0x00 might be a valid command
  unsigned int LastData = 0;
  bool repeat = false;

  while (Running() && f >= 0) {
        if (ReceiveByte(REPEATLIMIT) == 'X') {
           for (int i = 0; i < 6; i++) {
               int b = ReceiveByte();
               if (b >= 0) {
                  buffer.raw[i] = b;
                  if (i == 5) {
                     unsigned short Address = ntohs(buffer.data.address); // the PIC sends bytes in "network order"
                     uint64_t       Command = ntohl(buffer.data.command);
                     if (code == 'B' && Address == 0x0000 && Command == 0x00004000)
                        // Well, well, if it isn't the "d-box"...
                        // This remote control sends the above command before and after
                        // each keypress - let's just drop this:
                        break;
                     Command |= uint64_t(Address) << 32;
                     if (Command != LastCommand) {
                        LastCommand = Command;
                        repeat = false;
                        FirstTime.Set();
                        }
                     else {
                        if (FirstTime.Elapsed() < REPEATDELAY)
                           break; // repeat function kicks in after a short delay
                        repeat = true;
                        }
                     Put(Command, repeat);
                     receivedCommand = true;
                     }
                  }
               else
                  break;
               }
           }
        else if (repeat) { // the last one was a repeat, so let's generate a release
           Put(LastCommand, false, true);
           repeat = false;
           LastCommand = ~0;
           }
        else {
           unsigned int d = data;
           if (d != LastData) {
              SendData(d);
              LastData = d;
              }
           unsigned char c = code;
           if (c != LastCode) {
              SendCommand(c);
              LastCode = c;
              }
           unsigned char m = mode;
           if (m != LastMode) {
              SendCommand(m);
              LastMode = m;
              }
           LastCommand = ~0;
           }
        if (!repeat && code && time(NULL) - LastCodeRefresh > 60) {
           SendCommand(code); // in case the PIC listens to the wrong code
           LastCodeRefresh = time(NULL);
           }
        }
}

int cRcuRemote::ReceiveByte(int TimeoutMs)
{
  // Returns the byte if one was received within a timeout, -1 otherwise
  if (cFile::FileReady(f, TimeoutMs)) {
     unsigned char b;
     if (safe_read(f, &b, 1) == 1)
        return b;
     else
        LOG_ERROR;
     }
  return -1;
}

bool cRcuRemote::SendByteHandshake(unsigned char c)
{
  if (f >= 0) {
     int w = write(f, &c, 1);
     if (w == 1) {
        for (int reply = ReceiveByte(HANDSHAKETIMEOUT); reply >= 0;) {
            if (reply == c)
               return true;
            else if (reply == 'X') {
               // skip any incoming RC code - it will come again
               for (int i = 6; i--;) {
                   if (ReceiveByte() < 0)
                      return false;
                   }
               }
            else
               return false;
            }
        }
     LOG_ERROR;
     }
  return false;
}

bool cRcuRemote::SendByte(unsigned char c)
{
  for (int retry = 5; retry--;) {
      if (SendByteHandshake(c))
         return true;
      }
  return false;
}

bool cRcuRemote::SendData(unsigned int n)
{
  for (int i = 0; i < 4; i++) {
      if (!SendByte(n & 0x7F))
         return false;
      n >>= 8;
      }
  return SendCommand(mode);
}

void cRcuRemote::SetCode(unsigned char Code)
{
  code = Code;
}

void cRcuRemote::SetMode(unsigned char Mode)
{
  mode = Mode;
}

bool cRcuRemote::SendCommand(unsigned char Cmd)
{
  return SendByte(Cmd | 0x80);
}

void cRcuRemote::SetNumber(int n, bool Hex)
{
  number = n;
  if (!Hex) {
     char buf[8];
     sprintf(buf, "%4d", n & 0xFFFF);
     n = 0;
     for (char *d = buf; *d; d++) {
         if (*d == ' ')
            *d = 0xF;
         n = (n << 4) | ((*d - '0') & 0x0F);
         }
     }
  unsigned int m = 0;
  for (int i = 0; i < 4; i++) {
      m <<= 8;
      m |= ((i & 0x03) << 5) | (n & 0x0F) | (((dp >> i) & 0x01) << 4);
      n >>= 4;
      }
  data = m;
}

void cRcuRemote::SetString(const char *s)
{
  const char *chars = mode == modeH ? "0123456789ABCDEF" : "0123456789-EHLP ";
  int n = 0;

  for (int i = 0; *s && i < 4; s++, i++) {
      n <<= 4;
      for (const char *c = chars; *c; c++) {
          if (*c == *s) {
             n |= c - chars;
             break;
             }
          }
      }
  SetNumber(n, true);
}

void cRcuRemote::SetPoints(unsigned char Dp, bool On)
{
  if (On)
     dp |= Dp;
  else
     dp &= ~Dp;
  SetNumber(number);
}

bool cRcuRemote::DetectCode(unsigned char *Code)
{
  // Caller should initialize 'Code' to 0 and call DetectCode()
  // until it returns true. Whenever DetectCode() returns false
  // and 'Code' is not 0, the caller can use 'Code' to display
  // a message like "Trying code '%c'". If false is returned and
  // 'Code' is 0, all possible codes have been tried and the caller
  // can either stop calling DetectCode() (and give some error
  // message), or start all over again.
  if (*Code < 'A' || *Code > 'D') {
     *Code = 'A';
     return false;
     }
  if (*Code <= 'D') {
     SetMode(modeH);
     char buf[5];
     sprintf(buf, "C0D%c", *Code);
     SetString(buf);
     SetCode(*Code);
     cCondWait::SleepMs(2 * REPEATDELAY);
     if (receivedCommand) {
        SetMode(modeB);
        SetString("----");
        return true;
        }
     if (*Code < 'D') {
        (*Code)++;
        return false;
        }
     }
  *Code = 0;
  return false;
}

void cRcuRemote::ChannelSwitch(const cDevice *Device, int ChannelNumber, bool LiveView)
{
  if (ChannelNumber && LiveView)
     SetNumber(cDevice::CurrentChannel());
}

void cRcuRemote::Recording(const cDevice *Device, const char *Name, const char *FileName, bool On)
{
  SetPoints(1 << Device->DeviceNumber(), Device->Receiving());
}

class cPluginRcu : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  const char *device;
public:
  cPluginRcu(void);
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  };

cPluginRcu::cPluginRcu(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  device = DEFAULTDEVICE;
}

const char *cPluginRcu::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -d DEV,   --device=DEV   set the device to use (default is " DEFAULTDEVICE ")\n";
}

bool cPluginRcu::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  static struct option long_options[] = {
       { "dev",      required_argument, NULL, 'd' },
       { NULL,       no_argument,       NULL,  0  }
     };

  int c;
  while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (c) {
          case 'd': device = optarg;
                    break;
          default:  return false;
          }
        }
  return true;
}

bool cPluginRcu::Start(void)
{
  // Start any background activities the plugin shall perform.
  new cRcuRemote(device);
  return true;
}

VDRPLUGINCREATOR(cPluginRcu); // Don't touch this!
