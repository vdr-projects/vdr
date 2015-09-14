/*
 * diseqc.c: DiSEqC handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: diseqc.c 4.0 2015/01/26 12:02:14 kls Exp $
 */

#include "diseqc.h"
#include <ctype.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>
#include "sources.h"
#include "thread.h"

#define ALL_DEVICES (~0) // all bits set to '1'
#define MAX_DEVICES 32   // each bit in a 32-bit integer represents one device

static int CurrentDevices = 0;

static bool IsDeviceNumbers(const char *s)
{
  return *s && s[strlen(s) - 1] == ':';
}

static bool ParseDeviceNumbers(const char *s)
{
  if (IsDeviceNumbers(s)) {
     CurrentDevices = 0;
     const char *p = s;
     while (*p && *p != ':') {
           char *t = NULL;
           int d = strtol(p, &t, 10);
           p = t;
           if (0 < d && d <= MAX_DEVICES)
              CurrentDevices |= (1 << d - 1);
           else {
              esyslog("ERROR: invalid device number %d in '%s'", d, s);
              return false;
              }
           }
     }
  return true;
}

// --- cDiseqcPositioner -----------------------------------------------------

// See http://www.eutelsat.com/files/live/sites/eutelsatv2/files/contributed/satellites/pdf/Diseqc/associated%20docs/positioner_appli_notice.pdf

cDiseqcPositioner::cDiseqcPositioner(void)
{
  SetCapabilities(pcCanDrive |
                  pcCanStep |
                  pcCanHalt |
                  pcCanSetLimits |
                  pcCanDisableLimits |
                  pcCanEnableLimits |
                  pcCanStorePosition |
                  pcCanRecalcPositions |
                  pcCanGotoPosition |
                  pcCanGotoAngle
                  );
}

void cDiseqcPositioner::SendDiseqc(uint8_t *Codes, int NumCodes)
{
  struct dvb_diseqc_master_cmd cmd;
  NumCodes = min(NumCodes, int(sizeof(cmd.msg) - 2));
  cmd.msg_len = 0;
  cmd.msg[cmd.msg_len++] = 0xE0;
  cmd.msg[cmd.msg_len++] = 0x31;
  for (int i = 0; i < NumCodes; i++)
      cmd.msg[cmd.msg_len++] = Codes[i];
  CHECK(ioctl(Frontend(), FE_DISEQC_SEND_MASTER_CMD, &cmd));
}

void cDiseqcPositioner::Drive(ePositionerDirection Direction)
{
  uint8_t Code[] = { uint8_t(Direction == pdLeft ? 0x68 : 0x69), 0x00 };
  SendDiseqc(Code, 2);
}

void cDiseqcPositioner::Step(ePositionerDirection Direction, uint Steps)
{
  if (Steps == 0)
     return;
  uint8_t Code[] = { uint8_t(Direction == pdLeft ? 0x68 : 0x69), 0xFF };
  Code[1] -= min(Steps, uint(0x7F)) - 1;
  SendDiseqc(Code, 2);
}

void cDiseqcPositioner::Halt(void)
{
  uint8_t Code[] = { 0x60 };
  SendDiseqc(Code, 1);
}

void cDiseqcPositioner::SetLimit(ePositionerDirection Direction)
{
  uint8_t Code[] = { uint8_t(Direction == pdLeft ? 0x66 : 0x67) };
  SendDiseqc(Code, 1);
}

void cDiseqcPositioner::DisableLimits(void)
{
  uint8_t Code[] = { 0x63 };
  SendDiseqc(Code, 1);
}

void cDiseqcPositioner::EnableLimits(void)
{
  uint8_t Code[] = { 0x6A, 0x00 };
  SendDiseqc(Code, 2);
}

void cDiseqcPositioner::StorePosition(uint Number)
{
  uint8_t Code[] = { 0x6A, uint8_t(Number) };
  SendDiseqc(Code, 2);
}

void cDiseqcPositioner::RecalcPositions(uint Number)
{
  uint8_t Code[] = { 0x6F, uint8_t(Number), 0x00, 0x00 };
  SendDiseqc(Code, 4);
}

void cDiseqcPositioner::GotoPosition(uint Number, int Longitude)
{
  uint8_t Code[] = { 0x6B, uint8_t(Number) };
  SendDiseqc(Code, 2);
  cPositioner::GotoPosition(Number, Longitude);
}

void cDiseqcPositioner::GotoAngle(int Longitude)
{
  uint8_t Code[] = { 0x6E, 0x00, 0x00 };
  int Angle = CalcHourAngle(Longitude);
  int a = abs(Angle);
  Code[1] = a / 10 / 16;
  Code[2] = a / 10 % 16 * 16 + a % 10 * 16 / 10;
  Code[1] |= (Angle < 0) ? 0xE0 : 0xD0;
  SendDiseqc(Code, 3);
  cPositioner::GotoAngle(Longitude);
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
  if (IsDeviceNumbers(s))
     return ParseDeviceNumbers(s);
  devices = CurrentDevices;
  bool result = false;
  int fields = sscanf(s, "%d %u %d", &channel, &userBand, &pin);
  if (fields == 2 || fields == 3) {
     if (channel >= 0 && channel < 32) {
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

bool cScrs::Load(const char *FileName, bool AllowComments, bool MustExist)
{
  CurrentDevices = ALL_DEVICES;
  return cConfig<cScr>::Load(FileName, AllowComments, MustExist);
}

cScr *cScrs::GetUnused(int Device)
{
  cMutexLock MutexLock(&mutex);
  for (cScr *p = First(); p; p = Next(p)) {
      if (!IsBitSet(p->Devices(), Device - 1))
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
  position = -1;
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
  if (IsDeviceNumbers(s))
     return ParseDeviceNumbers(s);
  devices = CurrentDevices;
  bool result = false;
  char *sourcebuf = NULL;
  int fields = sscanf(s, "%m[^ ] %d %c %d %m[^\n]", &sourcebuf, &slof, &polarization, &lof, &commands);
  if (fields == 4)
     commands = NULL; //XXX Apparently sscanf() doesn't work correctly if the last %m argument results in an empty string
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
  if ((Codes[0] & 0xF0) == 0x70 ) { // EN50607 aka JESS
     uint t = SatFrequency == 0 ? 0 : (SatFrequency - 100);
     if (t < 2048 && Scr->Channel() >= 0 && Scr->Channel() < 32) {
        Codes[1] = t >> 8 | Scr->Channel() << 3;
        Codes[2] = t;
        Codes[3] = (t == 0 ? 0 : scrBank);
        if (t)
           return Scr->UserBand();
        }
     }
  else { // EN50494 aka Unicable
     uint t = SatFrequency == 0 ? 0 : (SatFrequency + Scr->UserBand() + 2) / 4 - 350; // '+ 2' together with '/ 4' results in rounding!
     if (t < 1024 && Scr->Channel() >= 0 && Scr->Channel() < 8) {
        Codes[3] = t >> 8 | (t == 0 ? 0 : scrBank << 2) | Scr->Channel() << 5;
        Codes[4] = t;
        if (t)
           return (t + 350) * 4 - SatFrequency;
        }
     }
  esyslog("ERROR: invalid SCR channel number %d or frequency %d", Scr->Channel(),SatFrequency);
  return 0;
}

int cDiseqc::SetScrPin(const cScr *Scr, uint8_t *Codes) const
{
  if ((Codes[0] & 0xF0) == 0x70 ) { // EN50607 aka JESS
     if (Scr->Pin() >= 0 && Scr->Pin() <= 255) {
        Codes[0] = 0x71;
        Codes[4] = Scr->Pin();
        return 5;
        }
     else {
        Codes[0] = 0x70;
        return 4;
        }
     }
  else { // EN50494 aka Unicable
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

const char *cDiseqc::GetPosition(const char *s) const
{
  if (!*s || !isdigit(*s)) {
     position = 0;
     return s;
     }
  char *p = NULL;
  errno = 0;
  int n = strtol(s, &p, 10);
  if (!errno && p != s && n >= 0 && n < 0xFF) {
     if (parsing) {
        if (position < 0)
           position = n;
        else
           esyslog("ERROR: more than one position in '%s'", s - 1);
        }
     return p;
     }
  esyslog("ERROR: invalid satellite position in '%s'", s - 1);
  return NULL;
}

const char *cDiseqc::GetScrBank(const char *s) const
{
  char *p = NULL;
  errno = 0;
  int n = strtol(s, &p, 10);
  if (!errno && p != s && n >= 0 && n < 256) {
     if (parsing) {
        if (scrBank < 0)
           scrBank = n;
        else
           esyslog("ERROR: more than one scr bank in '%s'", s - 1);
        }
     return p;
     }
  esyslog("ERROR: invalid value for scr bank in '%s'", s - 1);
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
          case 'W': *CurrentAction = Wait(*CurrentAction); return daWait;
          case 'P': *CurrentAction = GetPosition(*CurrentAction);
                    if (Setup.UsePositioner)
                       return position ? daPositionN : daPositionA;
                    break;
          case 'S': *CurrentAction = GetScrBank(*CurrentAction); return daScr;
          case '[': *CurrentAction = GetCodes(*CurrentAction, Codes, MaxCodes);
                    if (*CurrentAction) {
                       if (Scr && Frequency) {
                          *Frequency = SetScrFrequency(*Frequency, Scr, Codes);
                          *MaxCodes = SetScrPin(Scr, Codes);
                          }
                       return daCodes;
                       }
                    break;
          default:  esyslog("ERROR: unknown diseqc code '%c'", *(*CurrentAction - 1));
                    return daNone;
          }
        }
  return daNone;
}

// --- cDiseqcs --------------------------------------------------------------

cDiseqcs Diseqcs;

bool cDiseqcs::Load(const char *FileName, bool AllowComments, bool MustExist)
{
  CurrentDevices = ALL_DEVICES;
  return cConfig<cDiseqc>::Load(FileName, AllowComments, MustExist);
}

const cDiseqc *cDiseqcs::Get(int Device, int Source, int Frequency, char Polarization, const cScr **Scr) const
{
  for (const cDiseqc *p = First(); p; p = Next(p)) {
      if (!IsBitSet(p->Devices(), Device - 1))
         continue;
      if (cSource::Matches(p->Source(), Source) && p->Slof() > Frequency && p->Polarization() == toupper(Polarization)) {
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
