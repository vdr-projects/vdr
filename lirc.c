/*
 * lirc.c: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * LIRC support added by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: lirc.c 1.12 2005/08/15 12:28:10 kls Exp $
 */

#include "lirc.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#define REPEATLIMIT  20 // ms
#define REPEATDELAY 350 // ms
#define KEYPRESSDELAY 150 // ms

cLircRemote::cLircRemote(const char *DeviceName)
:cRemote("LIRC")
,cThread("LIRC remote control")
{
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

cLircRemote::~cLircRemote()
{
  int fh = f;
  f = -1;
  Cancel();
  if (fh >= 0)
     close(fh);
}

bool cLircRemote::Ready(void)
{
  return f >= 0;
}

void cLircRemote::Action(void)
{
  cTimeMs FirstTime;
  cTimeMs LastTime;
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF] = "";
  bool repeat = false;
  int timeout = -1;

  while (Running() && f >= 0) {

        bool ready = cFile::FileReady(f, timeout);
        int ret = ready ? safe_read(f, buf, sizeof(buf)) : -1;

        if (ready && ret <= 0 ) {
           esyslog("ERROR: lircd connection lost");
           close(f);
           f = -1;
           break;
           }

        if (ready && ret > 21) {
           int count;
           char KeyName[LIRC_KEY_BUF];
           sscanf(buf, "%*x %x %29s", &count, KeyName); // '29' in '%29s' is LIRC_KEY_BUF-1!
           if (count == 0) {
              if (strcmp(KeyName, LastKeyName) == 0 && FirstTime.Elapsed() < KEYPRESSDELAY)
                 continue; // skip keys coming in too fast
              if (repeat)
                 Put(LastKeyName, false, true);
              strcpy(LastKeyName, KeyName);
              repeat = false;
              FirstTime.Set();
              timeout = -1;
              }
           else {
              if (FirstTime.Elapsed() < REPEATDELAY)
                 continue; // repeat function kicks in after a short delay
              repeat = true;
              timeout = REPEATDELAY;
              }
           LastTime.Set();
           Put(KeyName, repeat);
           }
        else if (repeat) { // the last one was a repeat, so let's generate a release
           if (LastTime.Elapsed() >= REPEATDELAY) {
              Put(LastKeyName, false, true);
              repeat = false;
              *LastKeyName = 0;
              timeout = -1;
              }
           }
        }
}
