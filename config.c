/*
 * config.c: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 1.12 2000/07/21 13:10:50 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include "dvbapi.h"
#include "interface.h"

// -- cKeys ------------------------------------------------------------------

tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,            "Up",            0 },
                    { kDown,          "Down",          0 },
                    { kMenu,          "Menu",          0 },
                    { kOk,            "Ok",            0 },
                    { kBack,          "Back",          0 },
                    { kLeft,          "Left",          0 },
                    { kRight,         "Right",         0 },
                    { kRed,           "Red",           0 },
                    { kGreen,         "Green",         0 },
                    { kYellow,        "Yellow",        0 },
                    { kBlue,          "Blue",          0 },
                    { k0,             "0",             0 },
                    { k1,             "1",             0 },
                    { k2,             "2",             0 },
                    { k3,             "3",             0 },
                    { k4,             "4",             0 },
                    { k5,             "5",             0 },
                    { k6,             "6",             0 },
                    { k7,             "7",             0 },
                    { k8,             "8",             0 },
                    { k9,             "9",             0 },
                    { kNone,          "",              0 },
                  };

cKeys::cKeys(void)
{
  fileName = NULL;
  code = 0;
  address = 0;
  keys = keyTable;
}

void cKeys::Clear(void)
{
  for (tKey *k = keys; k->type != kNone; k++)
      k->code = 0;
}

void cKeys::SetDummyValues(void)
{
  for (tKey *k = keys; k->type != kNone; k++)
      k->code = k->type + 1; // '+1' to avoid 0
}

bool cKeys::Load(char *FileName)
{
  isyslog(LOG_INFO, "loading %s", FileName);
  bool result = false;
  if (FileName)
     fileName = strdup(FileName);
  if (fileName) {
     FILE *f = fopen(fileName, "r");
     if (f) {
        int line = 0;
        char buffer[MaxBuffer];
        result = true;
        while (fgets(buffer, sizeof(buffer), f) > 0) {
              line++;
              char *Name = buffer;
              char *p = strpbrk(Name, " \t");
              if (p) {
                 *p = 0; // terminates 'Name'
                 while (*++p && isspace(*p))
                       ;
                 if (*p) {
                    if (strcasecmp(Name, "Code") == 0)
                       code = *p;
                    else if (strcasecmp(Name, "Address") == 0)
                       address = strtol(p, NULL, 16);
                    else {
                       for (tKey *k = keys; k->type != kNone; k++) {
                           if (strcasecmp(Name, k->name) == 0) {
                              k->code = strtol(p, NULL, 16);
                              Name = NULL; // to indicate that we found it
                              break;
                              }
                           }
                        if (Name) {
                           esyslog(LOG_ERR, "unknown key in %s, line %d\n", fileName, line);
                           result = false;
                           break;
                           }
                        }
                    }
                 continue;
                 }
              esyslog(LOG_ERR, "error in %s, line %d\n", fileName, line);
              result = false;
              break;
              }
        fclose(f);
        }
     else
        esyslog(LOG_ERR, "can't open '%s'\n", fileName);
     }
  else
     esyslog(LOG_ERR, "no key configuration file name supplied!\n");
  return result;
}

bool cKeys::Save(void)
{
  //TODO make backup copies???
  bool result = true;
  FILE *f = fopen(fileName, "w");
  if (f) {
     if (fprintf(f, "Code\t%c\nAddress\t%04X\n", code, address) > 0) {
        for (tKey *k = keys; k->type != kNone; k++) {
            if (fprintf(f, "%s\t%08X\n", k->name, k->code) <= 0) {
               result = false;
               break;
               }
            }
         }
     else
        result = false;
     fclose(f);
     }
  else
     result = false;
  return result;
}

eKeys cKeys::Get(unsigned int Code)
{
  if (Code != 0) {
     tKey *k;
     for (k = keys; k->type != kNone; k++) {
         if (k->code == Code)
            break;
         }
     return k->type;
     }
  return kNone;
}

unsigned int cKeys::Encode(const char *Command)
{  
  if (Command != NULL) {
     const tKey *k = keys;
     while ((k->type != kNone) && strcmp(k->name, Command) != 0)
           k++;
     return k->code;
     }
  return 0;
}

void cKeys::Set(eKeys Key, unsigned int Code)
{
  for (tKey *k = keys; k->type != kNone; k++) {
      if (k->type == Key) {
         k->code = Code;
         break;
         }
      }
}

// -- cChannel ---------------------------------------------------------------

cChannel::cChannel(void)
{
  *name = 0;
}

cChannel::cChannel(const cChannel *Channel)
{
  strcpy(name,   Channel ? Channel->name         : "Pro7");
  frequency    = Channel ? Channel->frequency    : 12480;
  polarization = Channel ? Channel->polarization : 'v';
  diseqc       = Channel ? Channel->diseqc       : 1;
  srate        = Channel ? Channel->srate        : 27500;
  vpid         = Channel ? Channel->vpid         : 255;
  apid         = Channel ? Channel->apid         : 256;
  ca           = Channel ? Channel->ca           : 0;
  pnr          = Channel ? Channel->pnr          : 0;
}

bool cChannel::Parse(char *s)
{
  char *buffer = NULL;
  if (9 == sscanf(s, "%a[^:]:%d:%c:%d:%d:%d:%d:%d:%d", &buffer, &frequency, &polarization, &diseqc, &srate, &vpid, &apid, &ca, &pnr)) {
     strncpy(name, buffer, MaxChannelName - 1);
     name[strlen(buffer)] = 0;
     delete buffer;
     return true;
     }
  return false;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, "%s:%d:%c:%d:%d:%d:%d:%d:%d\n", name, frequency, polarization, diseqc, srate, vpid, apid, ca, pnr) > 0;
}

bool cChannel::Switch(cDvbApi *DvbApi)
{
  if (!DvbApi)
     DvbApi = cDvbApi::PrimaryDvbApi;
  if (!DvbApi->Recording()) {
     isyslog(LOG_INFO, "switching to channel %d", Index() + 1);
     CurrentChannel = Index();
     for (int i = 3; i--;) {
         if (DvbApi->SetChannel(frequency, polarization, diseqc, srate, vpid, apid, ca, pnr))
            return true;
         esyslog(LOG_ERR, "retrying");
         }
     return false;
     }
  Interface.Info("Channel locked (recording)!");
  return false;
}

bool cChannel::SwitchTo(int i, cDvbApi *DvbApi)
{
  cChannel *channel = Channels.Get(i);
  return channel && channel->Switch(DvbApi);
}

const char *cChannel::GetChannelName(int i)
{
  cChannel *channel = Channels.Get(i);
  return channel ? channel->name : NULL;
}

// -- cTimer -----------------------------------------------------------------

cTimer::cTimer(bool Instant)
{
  startTime = stopTime = 0;
  recording = false;
  active = Instant;
  channel = CurrentChannel + 1;
  time_t t = time(NULL);
  struct tm *now = localtime(&t);
  day = now->tm_mday;
  start = now->tm_hour * 100 + now->tm_min;
  stop = start + 200; // "instant recording" records 2 hours by default
  if (stop >= 2400)
     stop -= 2400;
//TODO VPS???
  priority = 99;
  lifetime = 99;
  *file = 0;
  if (Instant)
     snprintf(file, sizeof(file), "@%s", cChannel::GetChannelName(CurrentChannel));
}

int cTimer::TimeToInt(int t)
{
  return (t / 100 * 60 + t % 100) * 60;
}

time_t cTimer::Day(time_t t)
{
  struct tm d = *localtime(&t);
  d.tm_hour = d.tm_min = d.tm_sec = 0;
  return mktime(&d);
}

int cTimer::ParseDay(char *s)
{
  char *tail;
  int d = strtol(s, &tail, 10);
  if (tail && *tail) {
     d = 0;
     if (tail == s) {
        if (strlen(s) == 7) {
           for (char *p = s + 6; p >= s; p--) {
                 d <<= 1;
                 d |= (*p != '-');
                 }
           d |= 0x80000000;
           }
        }
     }
  else if (d < 1 || d > 31)
     d = 0;
  return d;
}

char *cTimer::PrintDay(int d)
{
  static char buffer[8];
  if ((d & 0x80000000) != 0) {
     char *b = buffer;
     char *w = "MTWTFSS";
     *b = 0;
     while (*w) {
           *b++ = (d & 1) ? *w : '-';
           d >>= 1;
           w++;
           }
     }
  else
     sprintf(buffer, "%d", d);
  return buffer;
}

bool cTimer::Parse(char *s)
{
  char *buffer1 = NULL;
  char *buffer2 = NULL;
  if (8 == sscanf(s, "%d:%d:%a[^:]:%d:%d:%d:%d:%a[^:\n]", &active, &channel, &buffer1, &start, &stop, &priority, &lifetime, &buffer2)) {
     day = ParseDay(buffer1);
     strncpy(file, buffer2, MaxFileName - 1);
     file[strlen(buffer2)] = 0;
     delete buffer1;
     delete buffer2;
     return day != 0;
     }
  return false;
}

bool cTimer::Save(FILE *f)
{
  return fprintf(f, "%d:%d:%s:%d:%d:%d:%d:%s\n", active, channel, PrintDay(day), start, stop, priority, lifetime, file) > 0;
}

bool cTimer::IsSingleEvent(void)
{
  return (day & 0x80000000) == 0;
}

bool cTimer::Matches(time_t t)
{
  if (active) {
     if (t == 0)
        t = time(NULL);
     struct tm now = *localtime(&t);
     int weekday = now.tm_wday == 0 ? 6 : now.tm_wday - 1; // we start with monday==0!
     int begin = TimeToInt(start);
     int end   = TimeToInt(stop);
     bool twoDays = (end < begin);

     bool todayMatches = false, yesterdayMatches = false;
     if ((day & 0x80000000) != 0) {
        if ((day & (1 << weekday)) != 0)
           todayMatches = true;
        else if (twoDays) {
           int yesterday = weekday == 0 ? 6 : weekday - 1;
           if ((day & (1 << yesterday)) != 0)
              yesterdayMatches = true;
           }
        }
     else if (day == now.tm_mday)
        todayMatches = true;
     else if (twoDays) {
        time_t ty = t - SECSINDAY;
        if (day == localtime(&ty)->tm_mday)
           yesterdayMatches = true;
        }
     if (todayMatches || (twoDays && yesterdayMatches)) {
        startTime = Day(t - (yesterdayMatches ? SECSINDAY : 0)) + begin;
        stopTime  = startTime + (twoDays ? SECSINDAY - begin + end : end - begin);
        }
     else
        startTime = stopTime = 0;
     return startTime <= t && t <= stopTime;
     }
  return false;
}

time_t cTimer::StartTime(void)
{ 
  if (!startTime)
     Matches();
  return startTime;
}

time_t cTimer::StopTime(void)
{ 
  if (!stopTime)
     Matches();
  return stopTime;
}

void cTimer::SetRecording(bool Recording)
{
  recording = Recording;
  isyslog(LOG_INFO, "timer %d %s", Index() + 1, recording ? "start" : "stop");
}

cTimer *cTimer::GetMatch(void)
{
  time_t t = time(NULL); // all timers must be checked against the exact same time to correctly handle Priority!
  cTimer *t0 = NULL;
  cTimer *ti = (cTimer *)Timers.First();
  while (ti) {
        if (!ti->recording && ti->Matches(t)) {
           if (!t0 || ti->priority > t0->priority)
              t0 = ti;
           }
        ti = (cTimer *)ti->Next();
        }
  return t0;
}

// -- cKeys ------------------------------------------------------------------

cKeys Keys;

// -- cChannels --------------------------------------------------------------

int CurrentChannel = 0;

cChannels Channels;

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

