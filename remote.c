/*
 * remote.c: Interface to the Remote Control Unit
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "remote.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#include "tools.h"

cRcIo::cRcIo(char *DeviceName)
{
  dp = 0;
  mode = modeB;
  code = 0;
  address = 0xFFFF;
  t = 0;
  firstTime = lastTime = 0;
  minDelta = 0;
  lastCommand = 0;
  if ((f = open(DeviceName, O_RDWR | O_NONBLOCK)) >= 0) {
     struct termios t;
     if (tcgetattr(f, &t) == 0) {
        cfsetspeed(&t, B9600);
        cfmakeraw(&t);
        if (tcsetattr(f, TCSAFLUSH, &t) == 0)
           return;
        }
     close(f);
     }
  f = -1;
}

cRcIo::~cRcIo()
{
  if (f >= 0)
     close(f);
}

int cRcIo::ReceiveByte(bool Wait)
{
  // Returns the byte if one was received within 1 second, -1 otherwise
  if (f >= 0) {
     fd_set set;
     struct timeval timeout;
     timeout.tv_sec = Wait ? 1 : 0;
     timeout.tv_usec = Wait ? 0 : 10000;
     FD_ZERO(&set);
     FD_SET(f, &set);
     if (select(FD_SETSIZE, &set, NULL, NULL, &timeout)  > 0) {
        if (FD_ISSET(f, &set)) {
           unsigned char b;
           if (read(f, &b, 1) == 1)
              return b;
           }
        }
     }
  return -1;
}

bool cRcIo::SendByteHandshake(unsigned char c)
{
  if (f >= 0 && write(f, &c, 1) == 1) {
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
  return false;
}

bool cRcIo::SendByte(unsigned char c)
{
  for (int retry = 5; retry--;) {
      if (SendByteHandshake(c))
         return true;
      }
  return false;
}

void cRcIo::Flush(int WaitSeconds)
{
  time_t t0 = time(NULL);

  for (;;) {
      while (ReceiveByte(false) >= 0)
            t0 = time(NULL);
      if (time(NULL) - t0 >= WaitSeconds)
         break;
      }
}

bool cRcIo::SetCode(unsigned char Code, unsigned short Address)
{
  code = Code;
  address = Address;
  minDelta = 200;
  return SendCommand(code);
}

bool cRcIo::SetMode(unsigned char Mode)
{
  mode = Mode;
  return SendCommand(mode);
}

bool cRcIo::GetCommand(unsigned int *Command, unsigned short *Address)
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
        if (delta < minDelta)
           minDelta = delta; // dynamically adjust to the smallest delta
        lastTime = now;
        if (delta < minDelta * 1.3) { // if commands come in rapidly...
           if (now - firstTime < 250)
              return false; // ...repeat function kicks in after 250ms
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

bool cRcIo::SendCommand(unsigned char Cmd)
{ 
  return SendByte(Cmd | 0x80);
}

bool cRcIo::Digit(int n, int v)
{ 
  return SendByte(((n & 0x03) << 5) | (v & 0x0F) | (((dp >> n) & 0x01) << 4));
}

bool cRcIo::Number(int n, bool Hex)
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
  for (int i = 0; i < 4; i++) {
      if (!Digit(i, n))
         return false;
      n >>= 4;
      }
  return SendCommand(mode);
}

bool cRcIo::String(char *s)
{
  char *chars = mode == modeH ? "0123456789ABCDEF" : "0123456789-EHLP ";
  int n = 0;

  for (int i = 0; *s && i < 4; s++, i++) {
      n <<= 4;
      for (char *c = chars; *c; c++) {
          if (*c == *s) {
             n |= c - chars;
             break;
             }
          }
      }
  return Number(n, mode == modeH);
}

bool cRcIo::DetectCode(unsigned char *Code, unsigned short *Address)
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
     if (GetCommand(&Command, Address))
        return true;
     if (*Code < 'D') {
        (*Code)++;
        return false;
        }
     }
  *Code = 0;
  return false;
}

