/*
 * config.c: Configuration file handling
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "dvbapi.h"
#include "interface.h"

// -- cKeys ------------------------------------------------------------------

tKey keyTable[] = { // "Up" and "Down" must be the first two keys!
                    { kUp,    "Up",    0 },
                    { kDown,  "Down",  0 },
                    { kMenu,  "Menu",  0 },
                    { kOk,    "Ok",    0 },
                    { kBack,  "Back",  0 },
                    { kLeft,  "Left",  0 },
                    { kRight, "Right", 0 },
                    { k0,     "0",     0 },
                    { k1,     "1",     0 },
                    { k2,     "2",     0 },
                    { k3,     "3",     0 },
                    { k4,     "4",     0 },
                    { k5,     "5",     0 },
                    { k6,     "6",     0 },
                    { k7,     "7",     0 },
                    { k8,     "8",     0 },
                    { k9,     "9",     0 },
                    { kNone,  "",      0 },
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
                           fprintf(stderr, "unknown key in %s, line %d\n", fileName, line);
                           result = false;
                           break;
                           }
                        }
                    }
                 continue;
                 }
              fprintf(stderr, "error in %s, line %d\n", fileName, line);
              result = false;
              break;
              }
        fclose(f);
        }
     else
        fprintf(stderr, "can't open '%s'\n", fileName);
     }
  else
     fprintf(stderr, "no key configuration file name supplied!\n");
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

bool cChannel::Parse(char *s)
{
  char *buffer = NULL;
  if (7 == sscanf(s, "%a[^:]:%d:%c:%d:%d:%d:%d", &buffer, &frequency, &polarization, &diseqc, &srate, &vpid, &apid)) {
     strncpy(name, buffer, MaxChannelName - 1);
     name[strlen(buffer)] = 0;
     delete buffer;
     return true;
     }
  return false;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, "%s:%d:%c:%d:%d:%d:%d\n", name, frequency, polarization, diseqc, srate, vpid, apid) > 0;
}

bool cChannel::Switch(void)
{
  if (!ChannelLocked) {
     isyslog(LOG_INFO, "switching to channel %d", Index() + 1);
     CurrentChannel = Index();
     Interface.DisplayChannel(CurrentChannel + 1, name);
     for (int i = 3; --i;) {
         if (DvbSetChannel(frequency, polarization, diseqc, srate, vpid, apid))
            return true;
         esyslog(LOG_ERR, "retrying");
         }
     }
  Interface.Info("Channel locked (recording)!");
  return false;
}

bool cChannel::SwitchTo(int i)
{
  cChannel *channel = Channels.Get(i);
  return channel && channel->Switch();
}

// -- cTimer -----------------------------------------------------------------

cTimer::cTimer(void)
{
  *file = 0;
}

int cTimer::TimeToInt(int t)
{
  return (t / 100 * 60 + t % 100) * 60;
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
  if (9 == sscanf(s, "%d:%d:%a[^:]:%d:%d:%c:%d:%d:%as", &active, &channel, &buffer1, &start, &stop, &quality, &priority, &lifetime, &buffer2)) {
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
  return fprintf(f, "%d:%d:%s:%d:%d:%c:%d:%d:%s\n", active, channel, PrintDay(day), start, stop, quality, priority, lifetime, file) > 0;
}

bool cTimer::Matches(void)
{
  if (active) {
     time_t t = time(NULL);
     struct tm *now = localtime(&t);
     int weekday = now->tm_wday == 0 ? 6 : now->tm_wday - 1; // we start with monday==0!
     int current = (now->tm_hour * 60 + now->tm_min) * 60 + now->tm_sec;
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
     else if (day == now->tm_mday)
        todayMatches = true;
     else if (twoDays) {
        t -= 86400;
        now = localtime(&t);
        if (day == now->tm_mday)
           yesterdayMatches = true;
        }
     return (todayMatches && current >= begin && (current <= end || twoDays))
            || (twoDays && yesterdayMatches && current <= end);
     }
  return false;
}

cTimer *cTimer::GetMatch(void)
{
  cTimer *t = (cTimer *)Timers.First();
  while (t) {
        if (t->Matches())
           return t;
        t = (cTimer *)t->Next();
        }
  return NULL;
}

// -- cKeys ------------------------------------------------------------------

cKeys Keys;

// -- cChannels --------------------------------------------------------------

int CurrentChannel = 0;
bool ChannelLocked = false;

cChannels Channels;

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

