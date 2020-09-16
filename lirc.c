/*
 * lirc.c: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * LIRC support added by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: lirc.c 4.2 2020/09/16 13:48:33 kls Exp $
 */

#include "lirc.h"
#include <netinet/in.h>
#include <sys/socket.h>

#define RECONNECTDELAY 3000 // ms

cLircRemote::cLircRemote(const char *DeviceName)
:cRemote("LIRC")
,cThread("LIRC remote control")
{
  addr.sun_family = AF_UNIX;
  strn0cpy(addr.sun_path, DeviceName, sizeof(addr.sun_path));
  if (!Connect())
     f = -1;
  Start();
}

cLircRemote::~cLircRemote()
{
  int fh = f;
  f = -1;
  Cancel();
  if (fh >= 0)
     close(fh);
}

bool cLircRemote::Connect(void)
{
  if ((f = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
     if (connect(f, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
        return true;
     LOG_ERROR_STR(addr.sun_path);
     close(f);
     f = -1;
     }
  else
     LOG_ERROR_STR(addr.sun_path);
  return false;
}

bool cLircRemote::Ready(void)
{
  return f >= 0;
}

void cLircRemote::Action(void)
{
  cTimeMs FirstTime;
  cTimeMs LastTime;
  cTimeMs ThisTime;
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF] = "";
  bool pressed = false;
  bool repeat = false;
  int timeout = -1;

  while (Running()) {

        bool ready = f >= 0 && cFile::FileReady(f, timeout);
        int ret = ready ? safe_read(f, buf, sizeof(buf)) : -1;

        if (f < 0 || ready && ret <= 0) {
           esyslog("ERROR: lircd connection broken, trying to reconnect every %.1f seconds", float(RECONNECTDELAY) / 1000);
           if (f >= 0)
              close(f);
           f = -1;
           while (Running() && f < 0) {
                 cCondWait::SleepMs(RECONNECTDELAY);
                 if (Connect()) {
                    isyslog("reconnected to lircd");
                    break;
                    }
                 }
           }

        if (ready && ret > 0) {
           buf[ret - 1] = 0;
           int count;
           char KeyName[LIRC_KEY_BUF];
           if (sscanf(buf, "%*x %x %29s", &count, KeyName) != 2) { // '29' in '%29s' is LIRC_KEY_BUF-1!
              esyslog("ERROR: unparsable lirc command: %s", buf);
              continue;
              }
           int Delta = ThisTime.Elapsed(); // the time between two subsequent LIRC events
           ThisTime.Set();
           if (count == 0) { // new key pressed
              if (strcmp(KeyName, LastKeyName) == 0 && FirstTime.Elapsed() < (uint)Setup.RcRepeatDelay)
                 continue; // skip keys coming in too fast
              if (repeat)
                 Put(LastKeyName, false, true); // generated release for previous repeated key
              strn0cpy(LastKeyName, KeyName, sizeof(LastKeyName));
              pressed = true;
              repeat = false;
              FirstTime.Set();
              timeout = -1;
              }
           else if (FirstTime.Elapsed() < (uint)Setup.RcRepeatDelay)
              continue; // repeat function kicks in after a short delay
           else if (LastTime.Elapsed() < (uint)Setup.RcRepeatDelta)
              continue; // skip same keys coming in too fast
           else {
              pressed = true;
              repeat = true;
              timeout = Delta * 3 / 2;
              }
           if (pressed) {
              LastTime.Set();
              Put(KeyName, repeat);
              }
           }
        else {
           if (pressed && repeat) // the last one was a repeat, so let's generate a release
              Put(LastKeyName, false, true);
           pressed = false;
           repeat = false;
           *LastKeyName = 0;
           timeout = -1;
           }
        }
}
