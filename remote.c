/*
 * remote.c: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.c 1.28 2002/09/29 12:51:26 kls Exp $
 */

#include "remote.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#if defined REMOTE_KBD
#include <ncurses.h>
#endif

#include "tools.h"

// --- cRemote ---------------------------------------------------------------

eKeys cRemote::keys[MaxKeys];
int cRemote::in = 0;
int cRemote::out = 0;
bool cRemote::learning = false;
char *cRemote::unknownCode = NULL;
cMutex cRemote::mutex;
cCondVar cRemote::keyPressed;

cRemote::cRemote(const char *Name)
{
  if (Name)
     name = strdup(Name);
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
     if ((Key & k_Release) != 0)
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

bool cRemote::Put(uint64 Code, bool Repeat, bool Release)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%016LX", Code);
  return Put(buffer, Repeat, Release);
}

bool cRemote::Put(const char *Code, bool Repeat, bool Release)
{
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

#if defined REMOTE_KBD

cKbdRemote::cKbdRemote(void)
:cRemote("KBD")
{
  Start();
}

cKbdRemote::~cKbdRemote()
{
  Cancel();
}

void cKbdRemote::Action(void)
{
  dsyslog("KBD remote control thread started (pid=%d)", getpid());
  cPoller Poller(STDIN_FILENO);
  for (;;) {//XXX
      int Command = getch();
      if (Command != EOF)
         Put(Command);
      Poller.Poll(100);
      }
  dsyslog("KBD remote control thread ended (pid=%d)", getpid());
}

#endif
