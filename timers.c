/*
 * timers.c: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.c 1.5 2003/05/11 13:48:49 kls Exp $
 */

#include "timers.h"
#include <ctype.h>
#include "channels.h"
#include "i18n.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// -- cTimer -----------------------------------------------------------------

char *cTimer::buffer = NULL;

cTimer::cTimer(bool Instant, bool Pause)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = Instant ? taActInst : taInactive;
  channel = Channels.GetByNumber(cDevice::CurrentChannel());
  time_t t = time(NULL);
  struct tm tm_r;
  struct tm *now = localtime_r(&t, &tm_r);
  day = now->tm_mday;
  start = now->tm_hour * 100 + now->tm_min;
  stop = now->tm_hour * 60 + now->tm_min + Setup.InstantRecordTime;
  stop = (stop / 60) * 100 + (stop % 60);
  if (stop >= 2400)
     stop -= 2400;
  priority = Pause ? Setup.PausePriority : Setup.DefaultPriority;
  lifetime = Pause ? Setup.PauseLifetime : Setup.DefaultLifetime;
  *file = 0;
  firstday = 0;
  summary = NULL;
  if (Instant && channel)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", *Setup.NameInstantRecord ? Setup.NameInstantRecord : channel->Name());
}

cTimer::cTimer(const cEventInfo *EventInfo)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = true;
  channel = Channels.GetByChannelID(EventInfo->GetChannelID(), true);
  time_t tstart = EventInfo->GetTime();
  time_t tstop = tstart + EventInfo->GetDuration() + Setup.MarginStop * 60;
  tstart -= Setup.MarginStart * 60;
  struct tm tm_r;
  struct tm *time = localtime_r(&tstart, &tm_r);
  day = time->tm_mday;
  start = time->tm_hour * 100 + time->tm_min;
  time = localtime_r(&tstop, &tm_r);
  stop = time->tm_hour * 100 + time->tm_min;
  if (stop >= 2400)
     stop -= 2400;
  priority = Setup.DefaultPriority;
  lifetime = Setup.DefaultLifetime;
  *file = 0;
  const char *Title = EventInfo->GetTitle();
  if (!isempty(Title))
     strn0cpy(file, EventInfo->GetTitle(), sizeof(file));
  firstday = 0;
  summary = NULL;
}

cTimer::~cTimer()
{
  free(summary);
}

cTimer& cTimer::operator= (const cTimer &Timer)
{
  memcpy(this, &Timer, sizeof(*this));
  if (summary)
     summary = strdup(summary);
  return *this;
}

bool cTimer::operator< (const cListObject &ListObject)
{
  cTimer *ti = (cTimer *)&ListObject;
  time_t t1 = StartTime();
  time_t t2 = ti->StartTime();
  return t1 < t2 || (t1 == t2 && priority > ti->priority);
}

const char *cTimer::ToText(bool UseChannelID)
{
  free(buffer);
  strreplace(file, ':', '|');
  strreplace(summary, '\n', '|');
  asprintf(&buffer, "%d:%s:%s:%04d:%04d:%d:%d:%s:%s\n", active, UseChannelID ? Channel()->GetChannelID().ToString() : itoa(Channel()->Number()), PrintDay(day, firstday), start, stop, priority, lifetime, file, summary ? summary : "");
  strreplace(summary, '|', '\n');
  strreplace(file, '|', ':');
  return buffer;
}

int cTimer::TimeToInt(int t)
{
  return (t / 100 * 60 + t % 100) * 60;
}

int cTimer::ParseDay(const char *s, time_t *FirstDay)
{
  char *tail;
  int d = strtol(s, &tail, 10);
  if (FirstDay)
     *FirstDay = 0;
  if (tail && *tail) {
     d = 0;
     if (tail == s) {
        const char *first = strchr(s, '@');
        int l = first ? first - s : strlen(s);
        if (l == 7) {
           for (const char *p = s + 6; p >= s; p--) {
               d <<= 1;
               d |= (*p != '-');
               }
           d |= 0x80000000;
           }
        if (FirstDay && first) {
           ++first;
           if (strlen(first) == 10) {
              struct tm tm_r;
              if (3 == sscanf(first, "%d-%d-%d", &tm_r.tm_year, &tm_r.tm_mon, &tm_r.tm_mday)) {
                 tm_r.tm_year -= 1900;
                 tm_r.tm_mon--;
                 tm_r.tm_hour = tm_r.tm_min = tm_r.tm_sec = 0;
                 tm_r.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
                 *FirstDay = mktime(&tm_r);
                 }
              }
           else
              d = 0;
           }
        }
     }
  else if (d < 1 || d > 31)
     d = 0;
  return d;
}

const char *cTimer::PrintDay(int d, time_t FirstDay)
{
#define DAYBUFFERSIZE 32
  static char buffer[DAYBUFFERSIZE];
  if ((d & 0x80000000) != 0) {
     char *b = buffer;
     const char *w = tr("MTWTFSS");
     while (*w) {
           *b++ = (d & 1) ? *w : '-';
           d >>= 1;
           w++;
           }
     if (FirstDay) {
        struct tm tm_r;
        localtime_r(&FirstDay, &tm_r);
        b += strftime(b, DAYBUFFERSIZE - (b - buffer), "@%Y-%m-%d", &tm_r);
        }
     *b = 0;
     }
  else
     sprintf(buffer, "%d", d);
  return buffer;
}

const char *cTimer::PrintFirstDay(void)
{
  if (firstday) {
     const char *s = PrintDay(day, firstday);
     if (strlen(s) == 18)
        return s + 8;
     }
  return ""; // not NULL, so the caller can always use the result
}

bool cTimer::Parse(const char *s)
{
  char *channelbuffer = NULL;
  char *daybuffer = NULL;
  char *filebuffer = NULL;
  free(summary);
  summary = NULL;
  //XXX Apparently sscanf() doesn't work correctly if the last %a argument
  //XXX results in an empty string (this first occured when the EIT gathering
  //XXX was put into a separate thread - don't know why this happens...
  //XXX As a cure we copy the original string and add a blank.
  //XXX If anybody can shed some light on why sscanf() failes here, I'd love
  //XXX to hear about that!
  char *s2 = NULL;
  int l2 = strlen(s);
  while (l2 > 0 && isspace(s[l2 - 1]))
        l2--;
  if (s[l2 - 1] == ':') {
     s2 = MALLOC(char, l2 + 3);
     strcat(strn0cpy(s2, s, l2 + 1), " \n");
     s = s2;
     }
  bool result = false;
  if (8 <= sscanf(s, "%d :%a[^:]:%a[^:]:%d :%d :%d :%d :%a[^:\n]:%a[^\n]", &active, &channelbuffer, &daybuffer, &start, &stop, &priority, &lifetime, &filebuffer, &summary)) {
     if (summary && !*skipspace(summary)) {
        free(summary);
        summary = NULL;
        }
     //TODO add more plausibility checks
     day = ParseDay(daybuffer, &firstday);
     result = day != 0;
     strn0cpy(file, filebuffer, MaxFileName);
     strreplace(file, '|', ':');
     strreplace(summary, '|', '\n');
     tChannelID cid = tChannelID::FromString(channelbuffer);
     channel = cid.Valid() ? Channels.GetByChannelID(cid) : Channels.GetByNumber(atoi(channelbuffer));
     if (!channel) {
        esyslog("ERROR: channel %s not defined", channelbuffer);
        result = false;
        }
     }
  free(channelbuffer);
  free(daybuffer);
  free(filebuffer);
  free(s2);
  return result;
}

bool cTimer::Save(FILE *f)
{
  return fprintf(f, ToText(true)) > 0;
}

bool cTimer::IsSingleEvent(void)
{
  return (day & 0x80000000) == 0;
}

int cTimer::GetMDay(time_t t)
{
  struct tm tm_r;
  return localtime_r(&t, &tm_r)->tm_mday;
}

int cTimer::GetWDay(time_t t)
{
  struct tm tm_r;
  int weekday = localtime_r(&t, &tm_r)->tm_wday;
  return weekday == 0 ? 6 : weekday - 1; // we start with monday==0!
}

bool cTimer::DayMatches(time_t t)
{
  return IsSingleEvent() ? GetMDay(t) == day : (day & (1 << GetWDay(t))) != 0;
}

time_t cTimer::IncDay(time_t t, int Days)
{
  struct tm tm_r;
  tm tm = *localtime_r(&t, &tm_r);
  tm.tm_mday += Days; // now tm_mday may be out of its valid range
  int h = tm.tm_hour; // save original hour to compensate for DST change
  tm.tm_isdst = -1;   // makes sure mktime() will determine the correct DST setting
  t = mktime(&tm);    // normalize all values
  tm.tm_hour = h;     // compensate for DST change
  return mktime(&tm); // calculate final result
}

time_t cTimer::SetTime(time_t t, int SecondsFromMidnight)
{
  struct tm tm_r;
  tm tm = *localtime_r(&t, &tm_r);
  tm.tm_hour = SecondsFromMidnight / 3600;
  tm.tm_min = (SecondsFromMidnight % 3600) / 60;
  tm.tm_sec =  SecondsFromMidnight % 60;
  tm.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
  return mktime(&tm);
}

char *cTimer::SetFile(const char *File)
{
  if (!isempty(File))
     strn0cpy(file, File, sizeof(file));
  return file;
}

bool cTimer::Matches(time_t t)
{
  startTime = stopTime = 0;
  if (t == 0)
     t = time(NULL);

  int begin  = TimeToInt(start); // seconds from midnight
  int length = TimeToInt(stop) - begin;
  if (length < 0)
     length += SECSINDAY;

  int DaysToCheck = IsSingleEvent() ? 61 : 7; // 61 to handle months with 31/30/31
  for (int i = -1; i <= DaysToCheck; i++) {
      time_t t0 = IncDay(t, i);
      if (DayMatches(t0)) {
         time_t a = SetTime(t0, begin);
         time_t b = a + length;
         if ((!firstday || a >= firstday) && t <= b) {
            startTime = a;
            stopTime = b;
            break;
            }
         }
      }
  if (!startTime)
     startTime = firstday; // just to have something that's more than a week in the future
  else if (t > startTime || t > firstday + SECSINDAY + 3600) // +3600 in case of DST change
     firstday = 0;
  return active && startTime <= t && t < stopTime; // must stop *before* stopTime to allow adjacent timers
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
  isyslog("timer %d (%d %04d-%04d '%s') %s", Index() + 1, Channel()->Number(), start, stop, file, recording ? "start" : "stop");
}

void cTimer::SetPending(bool Pending)
{
  pending = Pending;
}

void cTimer::SetActive(int Active)
{
  active = Active;
}

void cTimer::Skip(void)
{
  firstday = IncDay(SetTime(StartTime(), 0), 1);
}

void cTimer::OnOff(void)
{
  if (IsSingleEvent())
     active = !active;
  else if (firstday) {
     firstday = 0;
     active = false;
     }
  else if (active)
     Skip();
  else
     active = true;
  Matches(); // refresh start and end time
}

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

cTimer *cTimers::GetTimer(cTimer *Timer)
{
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->Channel() == Timer->Channel() && ti->Day() == Timer->Day() && ti->Start() == Timer->Start() && ti->Stop() == Timer->Stop())
         return ti;
      }
  return NULL;
}

cTimer *cTimers::GetMatch(time_t t)
{
  cTimer *t0 = NULL;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Recording() && ti->Matches(t)) {
         if (!t0 || ti->Priority() > t0->Priority())
            t0 = ti;
         }
      }
  return t0;
}

cTimer *cTimers::GetNextActiveTimer(void)
{
  cTimer *t0 = NULL;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->Active() && (!t0 || *ti < *t0))
         t0 = ti;
      }
  return t0;
}
