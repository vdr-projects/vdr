/*
 * remote.c: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.c 1.34 2002/12/08 13:37:13 kls Exp $
 */

#include "remote.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "tools.h"

// --- cRemote ---------------------------------------------------------------

eKeys cRemote::keys[MaxKeys];
int cRemote::in = 0;
int cRemote::out = 0;
cRemote *cRemote::learning = NULL;
char *cRemote::unknownCode = NULL;
cMutex cRemote::mutex;
cCondVar cRemote::keyPressed;
const char *cRemote::plugin = NULL;

cRemote::cRemote(const char *Name)
{
  name = Name ? strdup(Name) : NULL;
  Remotes.Add(this);
}

cRemote::~cRemote()
{
  free(name);
}

const char *cRemote::GetSetup(void)
{
  return Keys.GetSetup(Name());
}

void cRemote::PutSetup(const char *Setup)
{
  Keys.PutSetup(Name(), Setup);
}

void cRemote::Clear(void)
{
  cMutexLock MutexLock(&mutex);
  in = out = 0;
  if (learning) {
     free(unknownCode);
     unknownCode = NULL;
     }
}

bool cRemote::Put(eKeys Key)
{
  if (Key != kNone) {
     cMutexLock MutexLock(&mutex);
     if (in != out && (keys[out] & k_Repeat) && (Key & k_Release))
        Clear();
     int d = out - in;
     if (d <= 0)
        d = MaxKeys + d;
     if (d - 1 > 0) {
        keys[in] = Key;
        if (++in >= MaxKeys)
           in = 0;
        keyPressed.Broadcast();
        return true;
        }
     return false;
     }
  return true; // only a real key shall report an overflow!
}

bool cRemote::PutMacro(eKeys Key)
{
  const cKeyMacro *km = KeyMacros.Get(Key);
  if (km) {
     plugin = km->Plugin();
     for (int i = 1; i < MAXKEYSINMACRO; i++) {
         if (km->Macro()[i] != kNone) {
            if (!Put(km->Macro()[i]))
               return false;
            }
         else
            break;
         }
     }
  return true;
}

bool cRemote::Put(uint64 Code, bool Repeat, bool Release)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%016LX", Code);
  return Put(buffer, Repeat, Release);
}

bool cRemote::Put(const char *Code, bool Repeat, bool Release)
{
  if (learning && this != learning)
     return false;
  eKeys Key = Keys.Get(Name(), Code);
  if (Key != kNone) {
     if (Repeat)
        Key = eKeys(Key | k_Repeat);
     if (Release)
        Key = eKeys(Key | k_Release);
     return Put(Key);
     }
  if (learning) {
     free(unknownCode);
     unknownCode = strdup(Code);
     keyPressed.Broadcast();
     }
  return false;
}

eKeys cRemote::Get(int WaitMs, char **UnknownCode)
{
  for (;;) {
      cMutexLock MutexLock(&mutex);
      if (in != out) {
         eKeys k = keys[out];
         if (++out >= MaxKeys)
            out = 0;
         return k;
         }
      else if (!WaitMs || !keyPressed.TimedWait(mutex, WaitMs)) {
         if (learning && UnknownCode) {
            *UnknownCode = unknownCode;
            unknownCode = NULL;
            }
         return kNone;
         }
      }
}

// --- cRemotes --------------------------------------------------------------

cRemotes Remotes;

// --- cKbdRemote ------------------------------------------------------------

cKbdRemote::cKbdRemote(void)
:cRemote("KBD")
{
  active = false;
  tcgetattr(STDIN_FILENO, &savedTm);
  struct termios tm;
  if (tcgetattr(STDIN_FILENO, &tm) == 0) {
     tm.c_iflag = 0;
     tm.c_lflag &= ~(ICANON | ECHO);
     tm.c_cc[VMIN] = 0;
     tm.c_cc[VTIME] = 0;
     tcsetattr(STDIN_FILENO, TCSANOW, &tm);
     }
  Start();
}

cKbdRemote::~cKbdRemote()
{
  active = false;
  Cancel(3);
  tcsetattr(STDIN_FILENO, TCSANOW, &savedTm);
}

void cKbdRemote::Action(void)
{
  dsyslog("KBD remote control thread started (pid=%d)", getpid());
  cPoller Poller(STDIN_FILENO);
  active = true;
  while (active) {
        if (Poller.Poll(100)) {
           uint64 Command = 0;
           uint i = 0;
           int t0 = time_ms();
           while (active && i < sizeof(Command)) {
                 uchar ch;
                 int r = read(STDIN_FILENO, &ch, 1);
                 if (r == 1) {
                    Command <<= 8;
                    Command |= ch;
                    i++;
                    }
                 else if (r == 0) {
                    // don't know why, but sometimes special keys that start with
                    // 0x1B ('ESC') cause a short gap between the 0x1B and the rest
                    // of their codes, so we'll need to wait some 100ms to see if
                    // there is more coming up - or whether this really is the 'ESC'
                    // key (if somebody knows how to clean this up, please let me know):
                    if (Command == 0x1B && time_ms() - t0 < 100)
                       continue;
                    if (Command)
                       Put(Command);
                    break;
                    }
                 else {
                    LOG_ERROR;
                    break;
                    }
                 }
           }
        }
  dsyslog("KBD remote control thread ended (pid=%d)", getpid());
}
