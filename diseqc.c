/*
 * diseqc.c: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.c 2.9 2011/09/17 14:13:31 kls Exp $
 */

#include "diseqc.h"
#include <ctype.h>
#include "sources.h"
#include "thread.h"

static bool ParseDeviceNumbers(const char *s, int &Devices)
{
  if (*s && s[strlen(s) - 1] == ':') {
     const char *p = s;
     while (*p && *p != ':') {
           char *t = NULL;
           int d = strtol(p, &t, 10);
           p = t;
           if (0 < d && d < 31)
              Devices |= (1 << d - 1);
           else {
              esyslog("ERROR: invalid device number %d in '%s'", d, s);
              return false;
              }
           }
     }
  return true;
}

// --- cScr ------------------------------------------------------------------

cScr::cScr(void)
{
  devices = 0;
  channel = -1;
  userBand = 0;
  pin = -1;
  used = false;
}

bool cScr::Parse(const char *s)
{
  if (!ParseDeviceNumbers(s, devices))
     return false;
  if (devices)
     return true;
  bool result = false;
  int fields = sscanf(s, "%d %u %d", &channel, &userBand, &pin);
  if (fields == 2 || fields == 3) {
     if (channel >= 0 && channel < 8) {
        result = true;
        if (fields == 3 && (pin < 0 || pin > 255)) {
           esyslog("Error: invalid SCR pin '%d'", pin);
           result = false;
           }
        }
     else
        esyslog("Error: invalid SCR channel '%d'", channel);
     }
  return result;
}

// --- cScrs -----------------------------------------------------------------

cScrs Scrs;

cScr *cScrs::GetUnused(int Device)
{
  cMutexLock MutexLock(&mutex);
  int Devices = 0;
  for (cScr *p = First(); p; p = Next(p)) {
      if (p->Devices()) {
         Devices = p->Devices();
         continue;
         }
      if (Devices && !(Devices & (1 << Device - 1)))
         continue;
      if (!p->Used()) {
        p->SetUsed(true);
        return p;
        }
      }
  return NULL;
}

// --- cDiseqc ---------------------------------------------------------------

cDiseqc::cDiseqc(void)
{
  devices = 0;
  source = 0;
  slof = 0;
  polarization = 0;
  lof = 0;
  scrBank = -1;
  commands = NULL;
  parsing = false;
}

cDiseqc::~cDiseqc()
{
  free(commands);
}

bool cDiseqc::Parse(const char *s)
{
  if (!ParseDeviceNumbers(s, devices))
     return false;
  if (devices)
     return true;
  bool result = false;
  char *sourcebuf = NULL;
  int fields = sscanf(s, "%a[^ ] %d %c %d %a[^\n]", &sourcebuf, &slof, &polarization, &lof, &commands);
  if (fields == 4)
     commands = NULL; //XXX Apparently sscanf() doesn't work correctly if the last %a argument results in an empty string
  if (4 <= fields && fields <= 5) {
     source = cSource::FromString(sourcebuf);
     if (Sources.Get(source)) {
        polarization = char(toupper(polarization));
        if (polarization == 'V' || polarization == 'H' || polarization == 'L' || polarization == 'R') {
           parsing = true;
           const char *CurrentAction = NULL;
           while (Execute(&CurrentAction, NULL, NULL, NULL, NULL) != daNone)
                 ;
           parsing = false;
           result = !commands || !*CurrentAction;
           }
        else
           esyslog("ERROR: unknown polarization '%c'", polarization);
        }
     else
        esyslog("ERROR: unknown source '%s'", sourcebuf);
     }
  free(sourcebuf);
  return result;
}

uint cDiseqc::SetScrFrequency(uint SatFrequency, const cScr *Scr, uint8_t *Codes) const
{
  uint t = SatFrequency == 0 ? 0 : (SatFrequency + Scr->UserBand() + 2) / 4 - 350; // '+ 2' together with '/ 4' results in rounding!
  if (t < 1024 && Scr->Channel() >= 0 && Scr->Channel() < 8) {
     Codes[3] = t >> 8 | (t == 0 ? 0 : scrBank << 2) | Scr->Channel() << 5;
     Codes[4] = t;
     if (t)
        return (t + 350) * 4 - SatFrequency;
     }
  return 0;
}

int cDiseqc::SetScrPin(const cScr *Scr, uint8_t *Codes) const
{
  if (Scr->Pin() >= 0 && Scr->Pin() <= 255) {
     Codes[2] = 0x5C;
     Codes[5] = Scr->Pin();
     return 6;
     }
  else {
     Codes[2] = 0x5A;
     return 5;
     }
}

const char *cDiseqc::Wait(const char *s) const
{
  char *p = NULL;
  errno = 0;
  int n = strtol(s, &p, 10);
  if (!errno && p != s && n >= 0) {
     if (!parsing)
        cCondWait::SleepMs(n);
     return p;
     }
  esyslog("ERROR: invalid value for wait time in '%s'", s - 1);
  return NULL;
}

const char *cDiseqc::GetScrBank(const char *s) const
{
  char *p = NULL;
  errno = 0;
  int n = strtol(s, &p, 10);
  if (!errno && p != s && n >= 0 && n < 8) {
     if (parsing) {
        if (scrBank < 0)
           scrBank = n;
        else
           esyslog("ERROR: more than one scr bank in '%s'", s - 1);
        }
     return p;
     }
  esyslog("ERROR: more than one scr bank in '%s'", s - 1);
  return NULL;
}

const char *cDiseqc::GetCodes(const char *s, uchar *Codes, uint8_t *MaxCodes) const
{
  const char *e = strchr(s, ']');
  if (e) {
     int NumCodes = 0;
     const char *t = s;
     while (t < e) {
           if (NumCodes < MaxDiseqcCodes) {
              errno = 0;
              char *p;
              int n = strtol(t, &p, 16);
              if (!errno && p != t && 0 <= n && n <= 255) {
                 if (Codes) {
                    if (NumCodes < *MaxCodes)
                       Codes[NumCodes++] = uchar(n);
                    else {
                       esyslog("ERROR: too many codes in code sequence '%s'", s - 1);
                       return NULL;
                       }
                    }
                 t = skipspace(p);
                 }
              else {
                 esyslog("ERROR: invalid code at '%s'", t);
                 return NULL;
                 }
              }
           else {
              esyslog("ERROR: too many codes in code sequence '%s'", s - 1);
              return NULL;
              }
           }
     if (MaxCodes)
        *MaxCodes = NumCodes;
     return e + 1;
     }
  else
     esyslog("ERROR: missing closing ']' in code sequence '%s'", s - 1);
  return NULL;
}

cDiseqc::eDiseqcActions cDiseqc::Execute(const char **CurrentAction, uchar *Codes, uint8_t *MaxCodes, const cScr *Scr, uint *Frequency) const
{
  if (!*CurrentAction)
     *CurrentAction = commands;
  while (*CurrentAction && **CurrentAction) {
        switch (*(*CurrentAction)++) {
          case ' ': break;
          case 't': return daToneOff;
          case 'T': return daToneOn;
          case 'v': return daVoltage13;
          case 'V': return daVoltage18;
          case 'A': return daMiniA;
          case 'B': return daMiniB;
          case 'W': *CurrentAction = Wait(*CurrentAction); break;
          case 'S': *CurrentAction = GetScrBank(*CurrentAction); break;
          case '[': *CurrentAction = GetCodes(*CurrentAction, Codes, MaxCodes);
                    if (*CurrentAction) {
                       if (Scr && Frequency) {
                          *Frequency = SetScrFrequency(*Frequency, Scr, Codes);
                          *MaxCodes = SetScrPin(Scr, Codes);
                          }
                       return daCodes;
                       }
                    break;
          default: return daNone;
          }
        }
  return daNone;
}

// --- cDiseqcs --------------------------------------------------------------

cDiseqcs Diseqcs;

const cDiseqc *cDiseqcs::Get(int Device, int Source, int Frequency, char Polarization, const cScr **Scr) const
{
  int Devices = 0;
  for (const cDiseqc *p = First(); p; p = Next(p)) {
      if (p->Devices()) {
         Devices = p->Devices();
         continue;
         }
      if (Devices && !(Devices & (1 << Device - 1)))
         continue;
      if (p->Source() == Source && p->Slof() > Frequency && p->Polarization() == toupper(Polarization)) {
         if (p->IsScr() && Scr && !*Scr) {
            *Scr = Scrs.GetUnused(Device);
            if (*Scr)
               dsyslog("SCR %d assigned to device %d", (*Scr)->Channel(), Device);
            else
               esyslog("ERROR: no free SCR entry available for device %d", Device);
            }
         return p;
         }
      }
  return NULL;
}
