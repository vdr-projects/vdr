/*
 * remote.c: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Ported to LIRC by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: remote.c 1.9 2000/07/15 12:19:50 kls Exp $
 */

#include "remote.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#if defined REMOTE_LIRC
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#endif

#include "config.h"
#include "tools.h"

#define REPEATLIMIT 100 // ms
#define REPEATDELAY 250 // ms

// --- cRcIoBase -------------------------------------------------------------

cRcIoBase::cRcIoBase(void)
{
  t = 0;
  firstTime = lastTime = 0;
  lastCommand = 0;
}

cRcIoBase::~cRcIoBase()
{
}

// --- cRcIoKBD --------------------------------------------------------------

#if defined REMOTE_KBD

cRcIoKBD::cRcIoKBD(void)
{
}

cRcIoKBD::~cRcIoKBD()
{
}

void cRcIoKBD::Flush(int WaitSeconds)
{
  time_t t0 = time(NULL);

  timeout(10);
  for (;;) {
      while (getch() > 0)
            t0 = time(NULL);
      if (time(NULL) - t0 >= WaitSeconds)
         break;
      }
}

bool cRcIoKBD::InputAvailable(bool Wait)
{
  timeout(Wait ? 1000 : 10);
  return true;//XXX
}

bool cRcIoKBD::GetCommand(unsigned int *Command, unsigned short *)
{
  if (Command) {
     *Command = getch();
     return *Command > 0;
     }
  return false;
}

// --- cRcIoRCU --------------------------------------------------------------

#elif defined REMOTE_RCU

cRcIoRCU::cRcIoRCU(char *DeviceName)
{
  dp = 0;
  mode = modeB;
  code = 0;
  address = 0xFFFF;
  lastNumber = 0;
  if ((f = open(DeviceName, O_RDWR | O_NONBLOCK)) >= 0) {
     struct termios t;
     if (tcgetattr(f, &t) == 0) {
        cfsetspeed(&t, B9600);
        cfmakeraw(&t);
        if (tcsetattr(f, TCSAFLUSH, &t) == 0)
           return;
        }
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cRcIoRCU::~cRcIoRCU()
{
  if (f >= 0)
     close(f);
}

int cRcIoRCU::ReceiveByte(bool Wait)
{
  // Returns the byte if one was received within a timeout, -1 otherwise
  if (InputAvailable(Wait)) {
     unsigned char b;
     if (read(f, &b, 1) == 1)
        return b;
     else
        LOG_ERROR;
     }
  return -1;
}

bool cRcIoRCU::SendByteHandshake(unsigned char c)
{
  if (f >= 0) {
     int w = write(f, &c, 1);
     if (w == 1) {
        for (int reply = ReceiveByte(); reply >= 0;) {
            if (reply == c)
               return true;
            else if (reply == 'X') {
               // skip any incoming RC code - it will come again
               for (int i = 6; i--;) {
                   if (ReceiveByte(false) < 0)
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

bool cRcIoRCU::SendByte(unsigned char c)
{
  for (int retry = 5; retry--;) {
      if (SendByteHandshake(c))
         return true;
      }
  return false;
}

bool cRcIoRCU::SetCode(unsigned char Code, unsigned short Address)
{
  code = Code;
  address = Address;
  return SendCommand(code);
}

bool cRcIoRCU::SetMode(unsigned char Mode)
{
  mode = Mode;
  return SendCommand(mode);
}

void cRcIoRCU::Flush(int WaitSeconds)
{
  time_t t0 = time(NULL);

  for (;;) {
      while (ReceiveByte(false) >= 0)
            t0 = time(NULL);
      if (time(NULL) - t0 >= WaitSeconds)
         break;
      }
}

bool cRcIoRCU::InputAvailable(bool Wait)
{
  return DataAvailable(f, Wait);
}

bool cRcIoRCU::GetCommand(unsigned int *Command, unsigned short *Address)
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
    
  Flush();
  if (Command && ReceiveByte() == 'X') {
     for (int i = 0; i < 6; i++) {
         int b = ReceiveByte(false);
         if (b >= 0)
            buffer.raw[i] = b;
         else
            return false;
         }
     if (Address)
        *Address = ntohs(buffer.data.address); // the PIC sends bytes in "network order"
     else if (address != ntohs(buffer.data.address))
        return false;
     *Command = ntohl(buffer.data.command);
     if (code == 'B' && address == 0x0000 && *Command == 0x00004000)
        // Well, well, if it isn't the "d-box"...
        // This remote control sends the above command before and after
        // each keypress - let's just drop this:
        return false;
     if (*Command == lastCommand) {
        // let's have a timeout to avoid getting overrun by commands
        int now = time_ms();
        int delta = now - lastTime;
        lastTime = now;
        if (delta < REPEATLIMIT) { // if commands come in rapidly...
           if (now - firstTime < REPEATDELAY)
              return false; // ...repeat function kicks in after a short delay
           return true;
           }
        }
     lastTime = firstTime = time_ms();
     lastCommand = *Command;
     return true;
     }
  if (time(NULL) - t > 60) {
     SendCommand(code); // in case the PIC listens to the wrong code
     t = time(NULL);
     }
  return false;
}

bool cRcIoRCU::SendCommand(unsigned char Cmd)
{ 
  return SendByte(Cmd | 0x80);
}

bool cRcIoRCU::Digit(int n, int v)
{ 
  return SendByte(((n & 0x03) << 5) | (v & 0x0F) | (((dp >> n) & 0x01) << 4));
}

bool cRcIoRCU::Number(int n, bool Hex)
{
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
  lastNumber = n;
  for (int i = 0; i < 4; i++) {
      if (!Digit(i, n))
         return false;
      n >>= 4;
      }
  return SendCommand(mode);
}

bool cRcIoRCU::String(char *s)
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
  return Number(n, true);
}

void cRcIoRCU::SetPoints(unsigned char Dp, bool On)
{ 
  if (On)
     dp |= Dp;
  else
     dp &= ~Dp;
  Number(lastNumber, true);
}

bool cRcIoRCU::DetectCode(unsigned char *Code, unsigned short *Address)
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
     String(buf);
     SetCode(*Code, 0);
     unsigned int Command;
     if (GetCommand(&Command, Address)) {
        SetMode(modeB);
        String("----");
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

// --- cRcIoLIRC -------------------------------------------------------------

#elif defined REMOTE_LIRC

cRcIoLIRC::cRcIoLIRC(char *DeviceName)
{
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, DeviceName);
  f = socket(AF_UNIX, SOCK_STREAM, 0);
  if (f >= 0) {
     if (connect(f, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
        return;
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cRcIoLIRC::~cRcIoLIRC()
{
  if (f >= 0)
     close(f);
}

const char *cRcIoLIRC::ReceiveString(void)
{
  while (InputAvailable(true)) {
        if (read(f, buf, sizeof(buf)) > 21) {
           const int repeat = 10 * (buf[17] - '0') + (buf[18] - '0');
           const int now = time_ms();
           if (repeat == 0) {
              firstTime = lastTime = now;
              return buf + 20;
              }
           else if ((now > firstTime + REPEATDELAY) && (now > lastTime + REPEATLIMIT)) {
              lastTime = now;
              return buf + 20;
              }
           }
        }
  return NULL;
}

void cRcIoLIRC::Flush(int WaitSeconds)
{
  time_t t0 = time(NULL);

  for (;;) {
      while (InputAvailable(false)) {
            read(f, buf, sizeof(buf));
            t0 = time(NULL);
            }
      if (time(NULL) - t0 >= WaitSeconds)
         break;
      }
}

bool cRcIoLIRC::InputAvailable(bool Wait)
{
  return DataAvailable(f, Wait);
}

bool cRcIoLIRC::GetCommand(unsigned int *Command, unsigned short *)
{
  Flush();
  if (Command) {
     const char *cmd = ReceiveString();
     if (cmd) {
        *Command = Keys.Encode(cmd);
        return true;
        }
     }
  return false;
}

#endif

