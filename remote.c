/*
 * remote.c: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Ported to LIRC by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: remote.c 1.25 2001/09/30 11:39:49 kls Exp $
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

// --- cRcIoBase -------------------------------------------------------------

cRcIoBase::cRcIoBase(void)
{
  t = 0;
}

cRcIoBase::~cRcIoBase()
{
}

// --- cRcIoKBD --------------------------------------------------------------

#if defined REMOTE_KBD

cRcIoKBD::cRcIoKBD(void)
{
  f.Open(0); // stdin
}

cRcIoKBD::~cRcIoKBD()
{
}

void cRcIoKBD::Flush(int WaitMs)
{
  int t0 = time_ms();

  timeout(10);
  for (;;) {
      while (getch() > 0)
            t0 = time_ms();
      if (time_ms() - t0 >= WaitMs)
         break;
      }
}

bool cRcIoKBD::InputAvailable(void)
{
  return f.Ready(false);
}

bool cRcIoKBD::GetCommand(unsigned int *Command, bool *Repeat, bool *Release)
{
  if (Command) {
     *Command = getch();
     return int(*Command) > 0;
     }
  return false;
}

// --- cRcIoRCU --------------------------------------------------------------

#elif defined REMOTE_RCU

#define REPEATLIMIT  20 // ms
#define REPEATDELAY 350 // ms

cRcIoRCU::cRcIoRCU(char *DeviceName)
{
  dp = 0;
  mode = modeB;
  code = 0;
  address = 0xFFFF;
  receivedAddress = 0;
  receivedCommand = 0;
  receivedData = receivedRepeat = receivedRelease = false;
  lastNumber = 0;
  if ((f = open(DeviceName, O_RDWR | O_NONBLOCK)) >= 0) {
     struct termios t;
     if (tcgetattr(f, &t) == 0) {
        cfsetspeed(&t, B9600);
        cfmakeraw(&t);
        if (tcsetattr(f, TCSAFLUSH, &t) == 0) {
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

cRcIoRCU::~cRcIoRCU()
{
  Cancel();
}

void cRcIoRCU::Action(void)
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

  dsyslog(LOG_INFO, "RCU remote control thread started (pid=%d)", getpid());

  int FirstTime = 0;
  unsigned int LastCommand = 0;

  for (; f >= 0;) {

      LOCK_THREAD;

      if (ReceiveByte(REPEATLIMIT) == 'X') {
         for (int i = 0; i < 6; i++) {
             int b = ReceiveByte();
             if (b >= 0) {
                buffer.raw[i] = b;
                if (i == 5) {
                   unsigned short Address = ntohs(buffer.data.address); // the PIC sends bytes in "network order"
                   unsigned int   Command = ntohl(buffer.data.command);
                   if (code == 'B' && address == 0x0000 && Command == 0x00004000)
                      // Well, well, if it isn't the "d-box"...
                      // This remote control sends the above command before and after
                      // each keypress - let's just drop this:
                      break;
                   if (!receivedData) { // only accept new data the previous data has been fetched
                      int Now = time_ms();
                      if (Command != LastCommand) {
                         receivedAddress = Address;
                         receivedCommand = Command;
                         receivedData = true;
                         receivedRepeat = receivedRelease = false;
                         FirstTime = Now;
                         }
                      else {
                         if (Now - FirstTime < REPEATDELAY)
                            break; // repeat function kicks in after a short delay
                         receivedData = receivedRepeat = true;
                         }
                      LastCommand = Command;
                      WakeUp();
                      }
                   }
                }
             else
                break;
             }
         }
      else if (receivedData) { // the last data before releasing the key hasn't been fetched yet
         if (receivedRepeat) { // it was a repeat, so let's make it a release
            receivedRepeat = false;
            receivedRelease = true;
            LastCommand = 0;
            WakeUp();
            }
         }
      else if (receivedRepeat) { // all data has already been fetched, but the last one was a repeat, so let's generate a release
         receivedData = receivedRelease = true;
         receivedRepeat = false;
         LastCommand = 0;
         WakeUp();
         }
      else
         LastCommand = 0;
      }
}

int cRcIoRCU::ReceiveByte(int TimeoutMs)
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

bool cRcIoRCU::SendByteHandshake(unsigned char c)
{
  if (f >= 0) {
     int w = write(f, &c, 1);
     if (w == 1) {
        for (int reply = ReceiveByte(REPEATLIMIT); reply >= 0;) {
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

bool cRcIoRCU::SendByte(unsigned char c)
{
  LOCK_THREAD;

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

void cRcIoRCU::Flush(int WaitMs)
{
  LOCK_THREAD;

  int t0 = time_ms();
  for (;;) {
      while (ReceiveByte() >= 0)
            t0 = time_ms();
      if (time_ms() - t0 >= WaitMs)
         break;
      }
  receivedData = receivedRepeat = false;
}

bool cRcIoRCU::GetCommand(unsigned int *Command, bool *Repeat, bool *Release)
{
  if (receivedData) { // first we check the boolean flag without a lock, to avoid delays

     LOCK_THREAD;

     if (receivedData) { // need to check again, since the status might have changed while waiting for the lock
        if (Command)
           *Command = receivedCommand;
        if (Repeat)
           *Repeat = receivedRepeat;
        if (Release)
           *Release = receivedRelease;
        receivedData = false;
        return true;
        }
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
  LOCK_THREAD;

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
  LOCK_THREAD;

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
     delay_ms(REPEATDELAY);
     receivedData = receivedRepeat = 0;
     delay_ms(REPEATDELAY);
     if (GetCommand()) {
        *Address = receivedAddress;
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

#define REPEATLIMIT  20 // ms
#define REPEATDELAY 350 // ms

cRcIoLIRC::cRcIoLIRC(char *DeviceName)
{
  *keyName = 0;
  receivedData = receivedRepeat = false;
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, DeviceName);
  if ((f = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
     if (connect(f, (struct sockaddr *)&addr, sizeof(addr)) >= 0) {
        Start();
        return;
        }
     LOG_ERROR_STR(DeviceName);
     close(f);
     }
  else
     LOG_ERROR_STR(DeviceName);
  f = -1;
}

cRcIoLIRC::~cRcIoLIRC()
{
  Cancel();
}

void cRcIoLIRC::Action(void)
{
  dsyslog(LOG_INFO, "LIRC remote control thread started (pid=%d)", getpid());

  int FirstTime = 0;
  int LastTime = 0;
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF];

  for (; f >= 0;) {

      LOCK_THREAD;

      if (cFile::FileReady(f, REPEATLIMIT) && safe_read(f, buf, sizeof(buf)) > 21) {
         if (!receivedData) { // only accept new data the previous data has been fetched
            int count;
            sscanf(buf, "%*x %x %29s", &count, LastKeyName); // '29' in '%29s' is LIRC_KEY_BUF-1!
            int Now = time_ms();
            if (count == 0) {
               strcpy(keyName, LastKeyName);
               receivedData = true;
               receivedRepeat = receivedRelease = false;
               FirstTime = Now;
               }
            else {
               if (Now - FirstTime < REPEATDELAY)
                  continue; // repeat function kicks in after a short delay
               receivedData = receivedRepeat = true;
               receivedRelease = false;
               }
            LastTime = Now;
            WakeUp();
            }
         }
      else if (receivedData) { // the last data before releasing the key hasn't been fetched yet
         if (receivedRepeat) { // it was a repeat, so let's make it a release
            if (time_ms() - LastTime > REPEATDELAY) {
               receivedRepeat = false;
               receivedRelease = true;
               WakeUp();
               }
            }
         }
      else if (receivedRepeat) { // all data has already been fetched, but the last one was a repeat, so let's generate a release
         if (time_ms() - LastTime > REPEATDELAY) {
            receivedData = receivedRelease = true;
            receivedRepeat = false;
            WakeUp();
            }
         }
      }
}

bool cRcIoLIRC::GetCommand(unsigned int *Command, bool *Repeat, bool *Release)
{
  if (receivedData) { // first we check the boolean flag without a lock, to avoid delays

     LOCK_THREAD;

     if (receivedData) { // need to check again, since the status might have changed while waiting for the lock
        if (Command)
           *Command = Keys.Encode(keyName);
        if (Repeat)
           *Repeat = receivedRepeat;
        if (Release)
           *Release = receivedRelease;
        receivedData = false;
        return true;
        }
     }
  return false;
}

#endif

