/*
 * lirc.c: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * LIRC support added by Carsten Koch <Carsten.Koch@icem.de>  2000-06-16.
 *
 * $Id: lirc.c 5.6 2026/10/25 20:49:18 kls Exp $
 */

#include "lirc.h"
#include <linux/version.h>
#define HAVE_KERNEL_LIRC (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
// cLircUsrRemote
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
// cLircDevRemote
#if HAVE_KERNEL_LIRC
#include <linux/lirc.h>
#include <sys/ioctl.h>
#endif

#define RECONNECTDELAY 3000 // ms
#define LIRC_REPEAT_TIMEOUT 250 // ms (sufficient to compensate 1 lost transmission)

class cLircUsrRemote : public cLircRemote {
private:
  enum { LIRC_KEY_BUF = 30, LIRC_BUFFER_SIZE = 128 };
  struct sockaddr_un addr;
  bool Connect(void);
  virtual void Action(void) override;
public:
  cLircUsrRemote(const char *DeviceName);
  virtual ~cLircUsrRemote() override;
  };

#if HAVE_KERNEL_LIRC
class cLircDevRemote : public cLircRemote {
private:
  virtual void Action(void) override;
public:
  cLircDevRemote(void);
  virtual ~cLircDevRemote() override;
  bool Connect(const char *DeviceName);
  };
#endif

// --- cLircRemote -----------------------------------------------------------

cLircRemote::cLircRemote(const char *Name)
:cRemote(Name)
,cThread("LIRC remote control")
{
}

cLircRemote::~cLircRemote()
{
  int fh = f;
  f = -1;
  Cancel();
  if (fh >= 0)
     close(fh);
}

void cLircRemote::NewLircRemote(const char *Name)
{
#if HAVE_KERNEL_LIRC
  cLircDevRemote *r = new cLircDevRemote();
  if (r->Connect(Name))
     return;
  delete r;
#endif
  new cLircUsrRemote(Name);
}
// --- cLircUsrRemote --------------------------------------------------------

cLircUsrRemote::cLircUsrRemote(const char *DeviceName)
: cLircRemote("LIRC")
{
  addr.sun_family = AF_UNIX;
  strn0cpy(addr.sun_path, DeviceName, sizeof(addr.sun_path));
  Connect();
  Start();
}

bool cLircUsrRemote::Connect(void)
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

cLircUsrRemote::~cLircUsrRemote()
{
  Cancel(3); // Action() uses this object, so stop it before the object goes away
}

void cLircUsrRemote::Action(void)
{
  cTimeMs FirstTime;
  cTimeMs LastTime;
  char buf[LIRC_BUFFER_SIZE];
  char LastKeyName[LIRC_KEY_BUF] = "";
  bool repeat = false;
  int timeout = -1;

  while (Running()) {

        bool ready = f >= 0 && cFile::FileReady(f, timeout >= 0 ? timeout : 100); // check Running() at least every 100 ms
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

        else if (ready) {
           buf[ret - 1] = 0;
           int count;
           char KeyName[LIRC_KEY_BUF];
           if (sscanf(buf, "%*x %x %29s", &count, KeyName) != 2) { // '29' in '%29s' is LIRC_KEY_BUF-1!
              esyslog("ERROR: unparsable lirc command: %s", buf);
              continue;
              }
           if (count == 0) { // new key pressed
              if (repeat)
                 Put(LastKeyName, false, true); // generated release for previous repeated key
              strn0cpy(LastKeyName, KeyName, sizeof(LastKeyName));
              FirstTime.Set();
              repeat = false;
              timeout = -1;
              }
           else { // repeat of a continuously pressed key
              if (FirstTime.Elapsed() < (uint)Setup.RcRepeatDelay)
                 continue; // repeat function kicks in after a short delay
              if (LastTime.Elapsed() < (uint)Setup.RcRepeatDelta)
                 continue; // skip identical keys coming in too fast
              repeat = true;
              timeout = LIRC_REPEAT_TIMEOUT;
              }
           LastTime.Set();
           Put(KeyName, repeat);
           }
        else { // no key pressed within timeout
           if (repeat) // the last one was a repeat, so let's generate a release
              Put(LastKeyName, false, true);
           repeat = false;
           timeout = -1;
           }
        }
}

// --- cLircDevRemote --------------------------------------------------------

#if HAVE_KERNEL_LIRC
bool cLircDevRemote::Connect(const char *DeviceName)
{
  unsigned mode = LIRC_MODE_SCANCODE;
  f = open(DeviceName, O_RDONLY, 0);
  if (f < 0) {
     switch (errno) {
       case ENXIO:
       case ENODEV:
            // Do not complain about an attempt to open a lircd socket file.
            break;
       default:
            LOG_ERROR_STR(DeviceName);
       }
     }
  else if (ioctl(f, LIRC_SET_REC_MODE, &mode)) {
     LOG_ERROR_STR(DeviceName);
     close(f);
     f = -1;
     }
  if (f >= 0)
     Start();
  return f >= 0;
}

cLircDevRemote::cLircDevRemote(void)
:cLircRemote("DEV_LIRC")
{
}

cLircDevRemote::~cLircDevRemote()
{
  Cancel(3); // see ~cLircUsrRemote()
}

void cLircDevRemote::Action(void)
{
  if (f < 0)
     return;
  uint64_t FirstTime = 0, LastTime = 0;
  uint32_t LastKeyCode = 0;
  uint16_t LastFlags = false;
  bool SeenRepeat = false;
  bool repeat = false;

  while (Running()) {
        if (!cFile::FileReady(f, 100))
           continue; // check Running() at least every 100 ms
        lirc_scancode sc;
        ssize_t ret = read(f, &sc, sizeof sc);

        if (ret == sizeof sc) {
           const bool SameKey = sc.keycode == LastKeyCode && !((sc.flags ^ LastFlags) & LIRC_SCANCODE_FLAG_TOGGLE);

           if ((sc.flags & LIRC_SCANCODE_FLAG_REPEAT) != 0)
              // Before Linux 6.0, this flag is never set for some devices.
              SeenRepeat = true;

           if (SameKey && uint((sc.timestamp - FirstTime) / 1000000) < uint(Setup.RcRepeatDelay))
              continue; // skip keys coming in too fast

           if (!SameKey || (SeenRepeat && !(sc.flags & LIRC_SCANCODE_FLAG_REPEAT))) {
              // This is a key-press event, not key-repeat.
              if (repeat)
                 Put(LastKeyCode, false, true); // generated release for previous key
              repeat = false;
              FirstTime = sc.timestamp;
              LastKeyCode = sc.keycode;
              LastFlags = sc.flags;
              }
           else if (uint((sc.timestamp - LastTime) / 1000000) < uint(Setup.RcRepeatDelta))
              continue; // filter out too frequent key-repeat events
           else
              repeat = true;

           LastTime = sc.timestamp;
           Put(sc.keycode, repeat);
           }
        else {
           if (repeat) // the last one was a repeat, so let's generate a release
              Put(LastKeyCode, false, true);
           repeat = false;
           LastKeyCode = 0;
           }
        }
}
#endif
