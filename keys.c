/*
 * keys.c: Remote control Key handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: keys.c 1.1 2002/09/29 09:56:51 kls Exp $
 */

#include "keys.h"

static tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,            "Up"      },
                    { kDown,          "Down"    },
                    { kMenu,          "Menu"    },
                    { kOk,            "Ok"      },
                    { kBack,          "Back"    },
                    { kLeft,          "Left"    },
                    { kRight,         "Right"   },
                    { kRed,           "Red"     },
                    { kGreen,         "Green"   },
                    { kYellow,        "Yellow"  },
                    { kBlue,          "Blue"    },
                    { k0,             "0"       },
                    { k1,             "1"       },
                    { k2,             "2"       },
                    { k3,             "3"       },
                    { k4,             "4"       },
                    { k5,             "5"       },
                    { k6,             "6"       },
                    { k7,             "7"       },
                    { k8,             "8"       },
                    { k9,             "9"       },
                    { kPower,         "Power"   },
                    { kVolUp,         "Volume+" },
                    { kVolDn,         "Volume-" },
                    { kMute,          "Mute"    },
                    { kNone,          ""        },
                    { k_Setup,        "_Setup"  },
                    { kNone,          NULL      },
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
