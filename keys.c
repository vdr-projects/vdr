/*
 * keys.c: Remote control Key handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: keys.c 1.7 2004/12/27 11:08:34 kls Exp $
 */

#include "keys.h"
#include "plugin.h"

static tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,            "Up"         },
                    { kDown,          "Down"       },
                    { kMenu,          "Menu"       },
                    { kOk,            "Ok"         },
                    { kBack,          "Back"       },
                    { kLeft,          "Left"       },
                    { kRight,         "Right"      },
                    { kRed,           "Red"        },
                    { kGreen,         "Green"      },
                    { kYellow,        "Yellow"     },
                    { kBlue,          "Blue"       },
                    { k0,             "0"          },
                    { k1,             "1"          },
                    { k2,             "2"          },
                    { k3,             "3"          },
                    { k4,             "4"          },
                    { k5,             "5"          },
                    { k6,             "6"          },
                    { k7,             "7"          },
                    { k8,             "8"          },
                    { k9,             "9"          },
                    { kPlay,          "Play"       },
                    { kPause,         "Pause"      },
                    { kStop,          "Stop"       },
                    { kRecord,        "Record"     },
                    { kFastFwd,       "FastFwd"    },
                    { kFastRew,       "FastRew"    },
                    { kPower,         "Power"      },
                    { kChanUp,        "Channel+"   },
                    { kChanDn,        "Channel-"   },
                    { kVolUp,         "Volume+"    },
                    { kVolDn,         "Volume-"    },
                    { kMute,          "Mute"       },
                    { kAudio,         "Audio"      },
                    { kSchedule,      "Schedule"   },
                    { kChannels,      "Channels"   },
                    { kTimers,        "Timers"     },
                    { kRecordings,    "Recordings" },
                    { kSetup,         "Setup"      },
                    { kCommands,      "Commands"   },
                    { kUser1,         "User1"      },
                    { kUser2,         "User2"      },
                    { kUser3,         "User3"      },
                    { kUser4,         "User4"      },
                    { kUser5,         "User5"      },
                    { kUser6,         "User6"      },
                    { kUser7,         "User7"      },
                    { kUser8,         "User8"      },
                    { kUser9,         "User9"      },
                    { kNone,          ""           },
                    { k_Setup,        "_Setup"     },
                    { kNone,          NULL         },
                  };

// -- cKey -------------------------------------------------------------------

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
         if (strcasecmp(k->name, Name) == 0)
            return k->type;
         }
     }
  return kNone;
}

const char *cKey::ToString(eKeys Key)
{
  for (tKey *k = keyTable; k->name; k++) {
      if (k->type == Key)
         return k->name;
      }
  return NULL;
}

// -- cKeys ------------------------------------------------------------------

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

// -- cKeyMacro --------------------------------------------------------------

cKeyMacro::cKeyMacro(void)
{
  for (int i = 0; i < MAXKEYSINMACRO; i++)
      macro[i] = kNone;
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
              macro[n++] = k_Plugin;
              if (n < MAXKEYSINMACRO) {
                 macro[n] = kOk;
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
  if (n < 2) {
     esyslog("ERROR: empty key macro");
     }
  return true;
}

// -- cKeyMacros -------------------------------------------------------------

cKeyMacros KeyMacros;

const cKeyMacro *cKeyMacros::Get(eKeys Key)
{
  for (cKeyMacro *k = First(); k; k = Next(k)) {
      if (*k->Macro() == Key)
         return k;
      }
  return NULL;
}
