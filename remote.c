/*
 * remote.c: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.c 1.44 2005/08/14 10:53:55 kls Exp $
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

#define INITTIMEOUT 10000 // ms

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

bool cRemote::Initialize(void)
{
  if (Ready()) {
     char *NewCode = NULL;
     eKeys Key = Get(INITTIMEOUT, &NewCode);
     if (Key != kNone || NewCode)
        return true;
     }
  return false;
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

bool cRemote::Put(eKeys Key, bool AtFront)
{
  if (Key != kNone) {
     cMutexLock MutexLock(&mutex);
     if (in != out && (keys[out] & k_Repeat) && (Key & k_Release))
        Clear();
     int d = out - in;
     if (d <= 0)
        d = MaxKeys + d;
     if (d - 1 > 0) {
        if (AtFront) {
           if (--out < 0)
              out = MaxKeys - 1;
           keys[out] = Key;
           }
        else {
           keys[in] = Key;
           if (++in >= MaxKeys)
              in = 0;
           }
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

bool cRemote::HasKeys(void)
{
  cMutexLock MutexLock(&mutex);
  return in != out && !(keys[out] & k_Repeat);
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

struct tKbdMap {
  eKbdFunc func;
  uint64 code;
  };

static tKbdMap KbdMap[] = {
  { kfF1,     0x0000001B5B31317EULL },
  { kfF2,     0x0000001B5B31327EULL },
  { kfF3,     0x0000001B5B31337EULL },
  { kfF4,     0x0000001B5B31347EULL },
  { kfF5,     0x0000001B5B31357EULL },
  { kfF6,     0x0000001B5B31377EULL },
  { kfF7,     0x0000001B5B31387EULL },
  { kfF8,     0x0000001B5B31397EULL },
  { kfF9,     0x0000001B5B32307EULL },
  { kfF10,    0x0000001B5B32317EULL },
  { kfF11,    0x0000001B5B32327EULL },
  { kfF12,    0x0000001B5B32337EULL },
  { kfUp,     0x00000000001B5B41ULL },
  { kfDown,   0x00000000001B5B42ULL },
  { kfLeft,   0x00000000001B5B44ULL },
  { kfRight,  0x00000000001B5B43ULL },
  { kfHome,   0x00000000001B5B48ULL },
  { kfEnd,    0x00000000001B5B46ULL },
  { kfPgUp,   0x000000001B5B357EULL },
  { kfPgDown, 0x000000001B5B367EULL },
  { kfIns,    0x000000001B5B327EULL },
  { kfDel,    0x000000001B5B337EULL },
  { kfNone,   0x0000000000000000ULL }
  };

bool cKbdRemote::kbdAvailable = false;
bool cKbdRemote::rawMode = false;

cKbdRemote::cKbdRemote(void)
:cRemote("KBD")
,cThread("KBD remote control")
{
  tcgetattr(STDIN_FILENO, &savedTm);
  struct termios tm;
  if (tcgetattr(STDIN_FILENO, &tm) == 0) {
     tm.c_iflag = 0;
     tm.c_lflag &= ~(ICANON | ECHO);
     tm.c_cc[VMIN] = 0;
     tm.c_cc[VTIME] = 0;
     tcsetattr(STDIN_FILENO, TCSANOW, &tm);
     }
  kbdAvailable = true;
  Start();
}

cKbdRemote::~cKbdRemote()
{
  kbdAvailable = false;
  Cancel(3);
  tcsetattr(STDIN_FILENO, TCSANOW, &savedTm);
}

void cKbdRemote::SetRawMode(bool RawMode)
{
  rawMode = RawMode;
}

uint64 cKbdRemote::MapFuncToCode(int Func)
{
  for (tKbdMap *p = KbdMap; p->func != kfNone; p++) {
      if (p->func == Func)
         return p->code;
      }
  return (Func <= 0xFF) ? Func : 0;
}

int cKbdRemote::MapCodeToFunc(uint64 Code)
{
  for (tKbdMap *p = KbdMap; p->func != kfNone; p++) {
      if (p->code == Code)
         return p->func;
      }
  return (Code <= 0xFF) ? Code : kfNone;
}

void cKbdRemote::Action(void)
{
  cPoller Poller(STDIN_FILENO);
  while (Running()) {
        if (Poller.Poll(100)) {
           uint64 Command = 0;
           uint i = 0;
           while (Running() && i < sizeof(Command)) {
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
                    if (Command == 0x1B && Poller.Poll(100))
                       continue;
                    if (Command) {
                       if (rawMode || !Put(Command)) {
                          int func = MapCodeToFunc(Command);
                          if (func)
                             Put(KBDKEY(func));
                          }
                       }
                    break;
                    }
                 else {
                    LOG_ERROR;
                    break;
                    }
                 }
           }
        }
}
