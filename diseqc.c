/*
 * diseqc.c: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.c 2.2 2010/02/06 15:43:31 kls Exp $
 */

#include "diseqc.h"
#include <ctype.h>
#include "sources.h"
#include "thread.h"

// --- cDiseqc ---------------------------------------------------------------

cDiseqc::cDiseqc(void)
{
  devices = 0;
  source = 0;
  slof = 0;
  polarization = 0;
  lof = 0;
  commands = NULL;
  parsing = false;
  numCodes = 0;
}

cDiseqc::~cDiseqc()
{
  free(commands);
}

bool cDiseqc::Parse(const char *s)
{
  bool result = false;
  char *sourcebuf = NULL;
  if (*s && s[strlen(s) - 1] == ':') {
     const char *p = s;
     while (*p && *p != ':') {
           char *t = NULL;
           int d = strtol(p, &t, 10);
           p = t;
           if (0 < d && d < 32)
              devices |= (1 << d - 1);
           else {
              esyslog("ERROR: invalid device number %d in '%s'", d, s);
              return false;
              }
           }
     return true;
     }
  int fields = sscanf(s, "%a[^ ] %d %c %d %a[^\n]", &sourcebuf, &slof, &polarization, &lof, &commands);
  if (fields == 4)
     commands = NULL; //XXX Apparently sscanf() doesn't work correctly if the last %a argument results in an empty string
  if (4 <= fields && fields <= 5) {
     source = cSource::FromString(sourcebuf);
     if (Sources.Get(source)) {
        polarization = char(toupper(polarization));
        if (polarization == 'V' || polarization == 'H' || polarization == 'L' || polarization == 'R') {
           parsing = true;
           char *CurrentAction = NULL;
           while (Execute(&CurrentAction) != daNone)
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

char *cDiseqc::Wait(char *s)
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

char *cDiseqc::Codes(char *s)
{
  char *e = strchr(s, ']');
  if (e) {
     numCodes = 0;
     char *t = s;
     char *p = s;
     while (t < e) {
           if (numCodes < MaxDiseqcCodes) {
              errno = 0;
              int n = strtol(t, &p, 16);
              if (!errno && p != t && 0 <= n && n <= 255) {
                 codes[numCodes++] = uchar(n);
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
     return e + 1;
     }
  else
     esyslog("ERROR: missing closing ']' in code sequence '%s'", s - 1);
  return NULL;
}

cDiseqc::eDiseqcActions cDiseqc::Execute(char **CurrentAction)
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
          case '[': *CurrentAction = Codes(*CurrentAction); return *CurrentAction ? daCodes : daNone;
          default: return daNone;
          }
        }
  return daNone;
}

// --- cDiseqcs --------------------------------------------------------------

cDiseqcs Diseqcs;

cDiseqc *cDiseqcs::Get(int Device, int Source, int Frequency, char Polarization)
{
  int Devices = 0;
  for (cDiseqc *p = First(); p; p = Next(p)) {
      if (p->Devices()) {
         Devices = p->Devices();
         continue;
         }
      if (Devices && !(Devices & (1 << Device - 1)))
         continue;
      if (p->Source() == Source && p->Slof() > Frequency && p->Polarization() == toupper(Polarization))
        return p;
      }
  return NULL;
}
