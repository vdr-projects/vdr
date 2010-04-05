/*
 * keys.c: Remote control Key handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: keys.c 2.1 2010/04/05 10:05:58 kls Exp $
 */

#include "keys.h"
#include "plugin.h"

static tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,            trNOOP("Key$Up")          },
                    { kDown,          trNOOP("Key$Down")        },
                    { kMenu,          trNOOP("Key$Menu")        },
                    { kOk,            trNOOP("Key$Ok")          },
                    { kBack,          trNOOP("Key$Back")        },
                    { kLeft,          trNOOP("Key$Left")        },
                    { kRight,         trNOOP("Key$Right")       },
                    { kRed,           trNOOP("Key$Red")         },
                    { kGreen,         trNOOP("Key$Green")       },
                    { kYellow,        trNOOP("Key$Yellow")      },
                    { kBlue,          trNOOP("Key$Blue")        },
                    { k0,                    "0"                },
                    { k1,                    "1"                },
                    { k2,                    "2"                },
                    { k3,                    "3"                },
                    { k4,                    "4"                },
                    { k5,                    "5"                },
                    { k6,                    "6"                },
                    { k7,                    "7"                },
                    { k8,                    "8"                },
                    { k9,                    "9"                },
                    { kInfo,          trNOOP("Key$Info")        },
                    { kPlay,          trNOOP("Key$Play")        },
                    { kPause,         trNOOP("Key$Pause")       },
                    { kStop,          trNOOP("Key$Stop")        },
                    { kRecord,        trNOOP("Key$Record")      },
                    { kFastFwd,       trNOOP("Key$FastFwd")     },
                    { kFastRew,       trNOOP("Key$FastRew")     },
                    { kNext,          trNOOP("Key$Next")        },
                    { kPrev,          trNOOP("Key$Prev")        },
                    { kPower,         trNOOP("Key$Power")       },
                    { kChanUp,        trNOOP("Key$Channel+")    },
                    { kChanDn,        trNOOP("Key$Channel-")    },
                    { kChanPrev,      trNOOP("Key$PrevChannel") },
                    { kVolUp,         trNOOP("Key$Volume+")     },
                    { kVolDn,         trNOOP("Key$Volume-")     },
                    { kMute,          trNOOP("Key$Mute")        },
                    { kAudio,         trNOOP("Key$Audio")       },
                    { kSubtitles,     trNOOP("Key$Subtitles")   },
                    { kSchedule,      trNOOP("Key$Schedule")    },
                    { kChannels,      trNOOP("Key$Channels")    },
                    { kTimers,        trNOOP("Key$Timers")      },
                    { kRecordings,    trNOOP("Key$Recordings")  },
                    { kSetup,         trNOOP("Key$Setup")       },
                    { kCommands,      trNOOP("Key$Commands")    },
                    { kUser0,         trNOOP("Key$User0")       },
                    { kUser1,         trNOOP("Key$User1")       },
                    { kUser2,         trNOOP("Key$User2")       },
                    { kUser3,         trNOOP("Key$User3")       },
                    { kUser4,         trNOOP("Key$User4")       },
                    { kUser5,         trNOOP("Key$User5")       },
                    { kUser6,         trNOOP("Key$User6")       },
                    { kUser7,         trNOOP("Key$User7")       },
                    { kUser8,         trNOOP("Key$User8")       },
                    { kUser9,         trNOOP("Key$User9")       },
                    { kNone,                 ""                 },
                    { k_Setup,               "_Setup"           },
                    { kNone,                 NULL               },
                  };

// --- cKey ------------------------------------------------------------------

cKey::cKey(void)
{
  remote = code = NULL;
  key = kNone;
}

cKey::cKey(const char *Remote, const char *Code, eKeys Key)
{
  remote = strdup(Remote);
  code = strdup(Code);
  key = Key;
}

cKey::~cKey()
{
  free(remote);
  free(code);
}

bool cKey::Parse(char *s)
{
  char *p = strchr(s, '.');
  if (p) {
     *p++ = 0;
     remote = strdup(s);
     char *q = strpbrk(p, " \t");
     if (q) {
        *q++ = 0;
        key = FromString(p);
        if (key != kNone) {
           q = skipspace(q);
           if (*q) {
              code = strdup(q);
              return true;
              }
           }
        }
     }
  return false;
}

bool cKey::Save(FILE *f)
{
  return fprintf(f, "%s.%-10s %s\n", remote, ToString(key), code) > 0;
}

eKeys cKey::FromString(const char *Name)
{
  if (Name) {
     for (tKey *k = keyTable; k->name; k++) {
         const char *n = k->name;
         const char *p = strchr(n, '$');
         if (p)
            n = p + 1;
         if (strcasecmp(n, Name) == 0)
            return k->type;
         }
     }
  return kNone;
}

const char *cKey::ToString(eKeys Key, bool Translate)
{
  for (tKey *k = keyTable; k->name; k++) {
      if (k->type == Key) {
         const char *n = k->name;
         if (Translate)
            n = tr(n);
         const char *p = strchr(n, '$');
         if (p)
            n = p + 1;
         return n;
         }
      }
  return NULL;
}

// --- cKeys -----------------------------------------------------------------

cKeys Keys;

bool cKeys::KnowsRemote(const char *Remote)
{
  if (Remote) {
     for (cKey *k = First(); k; k = Next(k)) {
         if (strcmp(Remote, k->Remote()) == 0)
            return true;
         }
     }
  return false;
}

eKeys cKeys::Get(const char *Remote, const char *Code)
{
  if (Remote && Code) {
     for (cKey *k = First(); k; k = Next(k)) {
         if (strcmp(Remote, k->Remote()) == 0 && strcmp(Code, k->Code()) == 0)
            return k->Key();
         }
     }
  return kNone;
}

const char *cKeys::GetSetup(const char *Remote)
{
  if (Remote) {
     for (cKey *k = First(); k; k = Next(k)) {
         if (strcmp(Remote, k->Remote()) == 0 && k->Key() == k_Setup)
            return k->Code();
         }
     }
  return NULL;
}

void cKeys::PutSetup(const char *Remote, const char *Setup)
{
  if (!GetSetup(Remote))
     Add(new cKey(Remote, Setup, k_Setup));
  else
     esyslog("ERROR: called PutSetup() for %s, but setup has already been defined!", Remote);
}

// --- cKeyMacro -------------------------------------------------------------

cKeyMacro::cKeyMacro(void)
{
  numKeys = 0;
  for (int i = 0; i < MAXKEYSINMACRO; i++)
      macro[i] = kNone; // for compatibility with old code that doesn't know about NumKeys()
  plugin = NULL;
}

cKeyMacro::~cKeyMacro()
{
  free(plugin);
}

bool cKeyMacro::Parse(char *s)
{
  int n = 0;
  char *p;
  char *strtok_next;
  while ((p = strtok_r(s, " \t", &strtok_next)) != NULL) {
        if (n < MAXKEYSINMACRO) {
           if (*p == '@') {
              if (plugin) {
                 esyslog("ERROR: only one @plugin allowed per macro");
                 return false;
                 }
              if (!n) {
                 esyslog("ERROR: @plugin can't be first in macro");
                 return false;
                 }
              macro[n] = k_Plugin;
              if (n < MAXKEYSINMACRO) {
                 plugin = strdup(p + 1);
                 if (!cPluginManager::GetPlugin(plugin)) {
                    esyslog("ERROR: unknown plugin '%s'", plugin);
                    // this is not a fatal error - plugins may or may not be loaded
                    macro[--n] = kNone; // makes sure the key doesn't cause any side effects
                    }
                 }
              else {
                 esyslog("ERROR: key macro too long");
                 return false;
                 }
              }
           else {
              macro[n] = cKey::FromString(p);
              if (macro[n] == kNone) {
                 esyslog("ERROR: unknown key '%s'", p);
                 return false;
                 }
              }
           n++;
           s = NULL;
           }
        else {
           esyslog("ERROR: key macro too long");
           return false;
           }
        }
  if (n < 2)
     esyslog("ERROR: empty key macro"); // non fatal
  numKeys = n;
  return true;
}

// --- cKeyMacros ------------------------------------------------------------

cKeyMacros KeyMacros;

const cKeyMacro *cKeyMacros::Get(eKeys Key)
{
  if (Key != kNone) {
     for (cKeyMacro *k = First(); k; k = Next(k)) {
         if (*k->Macro() == Key)
            return k;
         }
     }
  return NULL;
}
