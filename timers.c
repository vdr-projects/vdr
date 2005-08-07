/*
 * timers.c: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.c 1.34 2005/07/30 13:03:51 kls Exp $
 */

#include "timers.h"
#include <ctype.h>
#include "channels.h"
#include "i18n.h"
#include "remote.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// -- cTimer -----------------------------------------------------------------

cTimer::cTimer(bool Instant, bool Pause)
{
  startTime = stopTime = 0;
  recording = pending = inVpsMargin = false;
  flags = tfNone;
  if (Instant)
     SetFlags(tfActive | tfInstant);
  channel = Channels.GetByNumber(cDevice::CurrentChannel());
  time_t t = time(NULL);
  struct tm tm_r;
  struct tm *now = localtime_r(&t, &tm_r);
  day = SetTime(t, 0);
  weekdays = 0;
  start = now->tm_hour * 100 + now->tm_min;
  stop = now->tm_hour * 60 + now->tm_min + Setup.InstantRecordTime;
  stop = (stop / 60) * 100 + (stop % 60);
  if (stop >= 2400)
     stop -= 2400;
  priority = Pause ? Setup.PausePriority : Setup.DefaultPriority;
  lifetime = Pause ? Setup.PauseLifetime : Setup.DefaultLifetime;
  *file = 0;
  summary = NULL;
  event = NULL;
  if (Instant && channel)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", *Setup.NameInstantRecord ? Setup.NameInstantRecord : channel->Name());
}

cTimer::cTimer(const cEvent *Event)
{
  startTime = stopTime = 0;
  recording = pending = inVpsMargin = false;
  flags = tfActive;
  if (Event->Vps() && Setup.UseVps)
     SetFlags(tfVps);
  channel = Channels.GetByChannelID(Event->ChannelID(), true);
  time_t tstart = (flags & tfVps) ? Event->Vps() : Event->StartTime();
  time_t tstop = tstart + Event->Duration();
  if (!(HasFlags(tfVps))) {
     tstop  += Setup.MarginStop * 60;
     tstart -= Setup.MarginStart * 60;
     }
  struct tm tm_r;
  struct tm *time = localtime_r(&tstart, &tm_r);
  day = SetTime(tstart, 0);
  weekdays = 0;
  start = time->tm_hour * 100 + time->tm_min;
  time = localtime_r(&tstop, &tm_r);
  stop = time->tm_hour * 100 + time->tm_min;
  if (stop >= 2400)
     stop -= 2400;
  priority = Setup.DefaultPriority;
  lifetime = Setup.DefaultLifetime;
  *file = 0;
  const char *Title = Event->Title();
  if (!isempty(Title))
     strn0cpy(file, Event->Title(), sizeof(file));
  summary = NULL;
  event = Event;
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
  event = NULL;
  return *this;
}

int cTimer::Compare(const cListObject &ListObject) const
{
  cTimer *ti = (cTimer *)&ListObject;
  time_t t1 = StartTime();
  time_t t2 = ti->StartTime();
  int r = t1 - t2;
  if (r == 0)
     r = ti->priority - priority;
  return r;
}

cString cTimer::ToText(bool UseChannelID)
{
  char *buffer;
  strreplace(file, ':', '|');
  strreplace(summary, '\n', '|');
  asprintf(&buffer, "%d:%s:%s:%04d:%04d:%d:%d:%s:%s\n", flags, UseChannelID ? *Channel()->GetChannelID().ToString() : *itoa(Channel()->Number()), *PrintDay(day, weekdays), start, stop, priority, lifetime, file, summary ? summary : "");
  strreplace(summary, '|', '\n');
  strreplace(file, '|', ':');
  return cString(buffer, true);
}

cString cTimer::ToDescr(void) const
{
  char *buffer;
  asprintf(&buffer, "%d (%d %04d-%04d '%s')", Index() + 1, Channel()->Number(), start, stop, file);
  return cString(buffer, true);
}

int cTimer::TimeToInt(int t)
{
  return (t / 100 * 60 + t % 100) * 60;
}

bool cTimer::ParseDay(const char *s, time_t &Day, int &WeekDays)
{
  // possible formats are:
  // 19
  // 2005-03-19
  // MTWTFSS
  // MTWTFSS@19
  // MTWTFSS@2005-03-19

  Day = 0;
  WeekDays = 0;
  s = skipspace(s);
  if (!*s)
     return false;
  const char *a = strchr(s, '@');
  const char *d = a ? a + 1 : isdigit(*s) ? s : NULL;
  if (d) {
     if (strlen(d) == 10) {
        struct tm tm_r;
        if (3 == sscanf(d, "%d-%d-%d", &tm_r.tm_year, &tm_r.tm_mon, &tm_r.tm_mday)) {
           tm_r.tm_year -= 1900;
           tm_r.tm_mon--;
           tm_r.tm_hour = tm_r.tm_min = tm_r.tm_sec = 0;
           tm_r.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
           Day = mktime(&tm_r);
           }
        else
           return false;
        }
     else {
        // handle "day of month" for compatibility with older versions:
        char *tail = NULL;
        int day = strtol(d, &tail, 10);
        if (tail && *tail || day < 1 || day > 31)
           return false;
        time_t t = time(NULL);
        int DaysToCheck = 61; // 61 to handle months with 31/30/31
        for (int i = -1; i <= DaysToCheck; i++) {
            time_t t0 = IncDay(t, i);
            if (GetMDay(t0) == day) {
               Day = SetTime(t0, 0);
               break;
               }
            }
        }
     }
  if (a || !isdigit(*s)) {
     if ((a && a - s == 7) || strlen(s) == 7) {
        for (const char *p = s + 6; p >= s; p--) {
            WeekDays <<= 1;
            WeekDays |= (*p != '-');
            }
        }
     else
        return false;
     }
  return true;
}

cString cTimer::PrintDay(time_t Day, int WeekDays)
{
#define DAYBUFFERSIZE 32
  char buffer[DAYBUFFERSIZE];
  char *b = buffer;
  if (WeekDays) {
     const char *w = tr("MTWTFSS");
     while (*w) {
           *b++ = (WeekDays & 1) ? *w : '-';
           WeekDays >>= 1;
           w++;
           }
     if (Day)
        *b++ = '@';
     }
  if (Day) {
     struct tm tm_r;
     localtime_r(&Day, &tm_r);
     b += strftime(b, DAYBUFFERSIZE - (b - buffer), "%Y-%m-%d", &tm_r);
     }
  *b = 0;
  return buffer;
}

cString cTimer::PrintFirstDay(void) const
{
  if (weekdays) {
     cString s = PrintDay(day, weekdays);
     if (strlen(s) == 18)
        return *s + 8;
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
  if (8 <= sscanf(s, "%d :%a[^:]:%a[^:]:%d :%d :%d :%d :%a[^:\n]:%a[^\n]", &flags, &channelbuffer, &daybuffer, &start, &stop, &priority, &lifetime, &filebuffer, &summary)) {
     if (summary && !*skipspace(summary)) {
        free(summary);
        summary = NULL;
        }
     //TODO add more plausibility checks
     result = ParseDay(daybuffer, day, weekdays);
     strn0cpy(file, filebuffer, MaxFileName);
     strreplace(file, '|', ':');
     strreplace(summary, '|', '\n');
     if (isnumber(channelbuffer))
        channel = Channels.GetByNumber(atoi(channelbuffer));
     else
        channel = Channels.GetByChannelID(tChannelID::FromString(channelbuffer), true, true);
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
  return fprintf(f, "%s", *ToText(true)) > 0;
}

bool cTimer::IsSingleEvent(void) const
{
  return !weekdays;
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

bool cTimer::DayMatches(time_t t) const
{
  return IsSingleEvent() ? SetTime(t, 0) == day : (weekdays & (1 << GetWDay(t))) != 0;
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

bool cTimer::Matches(time_t t, bool Directly) const
{
  startTime = stopTime = 0;
  if (t == 0)
     t = time(NULL);

  int begin  = TimeToInt(start); // seconds from midnight
  int length = TimeToInt(stop) - begin;
  if (length < 0)
     length += SECSINDAY;

  if (IsSingleEvent()) {
     startTime = SetTime(day, begin);
     stopTime = startTime + length;
     }
  else {
     for (int i = -1; i <= 7; i++) {
         time_t t0 = IncDay(t, i);
         if (DayMatches(t0)) {
            time_t a = SetTime(t0, begin);
            time_t b = a + length;
            if ((!day || a >= day) && t <= b) {
               startTime = a;
               stopTime = b;
               break;
               }
            }
         }
     if (!startTime)
        startTime = day; // just to have something that's more than a week in the future
     else if (!Directly && (t > startTime || t > day + SECSINDAY + 3600)) // +3600 in case of DST change
        day = 0;
     }

  if (HasFlags(tfActive)) {
     if (HasFlags(tfVps) && !Directly && event && event->Vps() && schedule && schedule->PresentSeenWithin(30)) {
        startTime = event->StartTime();
        stopTime = event->EndTime();
        return event->IsRunning(true);
        }
     return startTime <= t && t < stopTime; // must stop *before* stopTime to allow adjacent timers
     }
  return false;
}

#define FULLMATCH 1000

int cTimer::Matches(const cEvent *Event, int *Overlap) const
{
  // Overlap is the percentage of the Event's duration that is covered by
  // this timer (based on FULLMATCH for finer granularity than just 100).
  // To make sure a VPS timer can be distinguished from a plain 100% overlap,
  // it gets an additional 100 added, and a VPS event that is actually running
  // gets 200 added to the FULLMATCH.
  if (HasFlags(tfActive) && channel->GetChannelID() == Event->ChannelID()) {
     bool UseVps = HasFlags(tfVps) && Event->Vps();
     Matches(UseVps ? Event->Vps() : Event->StartTime(), true);
     int overlap = 0;
     if (UseVps)
        overlap = (startTime == Event->Vps()) ? FULLMATCH + (Event->IsRunning() ? 200 : 100) : 0;
     if (!overlap) {
        if (startTime <= Event->StartTime() && Event->EndTime() <= stopTime)
           overlap = FULLMATCH;
        else if (stopTime <= Event->StartTime() || Event->EndTime() <= startTime)
           overlap = 0;
        else
           overlap = (min(stopTime, Event->EndTime()) - max(startTime, Event->StartTime())) * FULLMATCH / max(Event->Duration(), 1);
        }
     startTime = stopTime = 0;
     if (Overlap)
        *Overlap = overlap;
     return overlap >= 1000 ? tmFull : overlap > 0 ? tmPartial : tmNone;
     }
  return tmNone;
}

#define EXPIRELATENCY 60 // seconds (just in case there's a short glitch in the VPS signal)

bool cTimer::Expired(void) const
{
  return IsSingleEvent() && !Recording() && StopTime() + EXPIRELATENCY <= time(NULL);
}

time_t cTimer::StartTime(void) const
{
  if (!startTime)
     Matches();
  return startTime;
}

time_t cTimer::StopTime(void) const
{
  if (!stopTime)
     Matches();
  return stopTime;
}

void cTimer::SetEvent(const cSchedule *Schedule, const cEvent *Event)
{
  if (event != Event) { //XXX TODO check event data, too???
     if (Event) {
        char vpsbuf[64] = "";
        if (Event->Vps())
           sprintf(vpsbuf, "(VPS: %s) ", *Event->GetVpsString());
        isyslog("timer %s set to event %s %s-%s %s'%s'", *ToDescr(), *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString(), vpsbuf, Event->Title());
        }
     else
        isyslog("timer %s set to no event", *ToDescr());
     schedule = Event ? Schedule : NULL;
     event = Event;
     }
}

void cTimer::SetRecording(bool Recording)
{
  recording = Recording;
  if (recording)
     SetFlags(tfRecording);
  else
     ClrFlags(tfRecording);
  isyslog("timer %s %s", *ToDescr(), recording ? "start" : "stop");
}

void cTimer::SetPending(bool Pending)
{
  pending = Pending;
}

void cTimer::SetInVpsMargin(bool InVpsMargin)
{
  if (InVpsMargin && !inVpsMargin)
     isyslog("timer %s entered VPS margin", *ToDescr());
  inVpsMargin = InVpsMargin;
}

void cTimer::SetFlags(int Flags)
{
  flags |= Flags;
}

void cTimer::ClrFlags(int Flags)
{
  flags &= ~Flags;
}

void cTimer::InvFlags(int Flags)
{
  flags ^= Flags;
}

bool cTimer::HasFlags(int Flags) const
{
  return (flags & Flags) == Flags;
}

void cTimer::Skip(void)
{
  day = IncDay(SetTime(StartTime(), 0), 1);
  event = NULL;
}

void cTimer::OnOff(void)
{
  if (IsSingleEvent())
     InvFlags(tfActive);
  else if (day) {
     day = 0;
     ClrFlags(tfActive);
     }
  else if (HasFlags(tfActive))
     Skip();
  else
     SetFlags(tfActive);
  event = NULL;
  Matches(); // refresh start and end time
}

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

cTimers::cTimers(void)
{
  modified = false;
  beingEdited = 0;;
  lastSetEvents = 0;
}

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

cTimer *cTimers::GetMatch(const cEvent *Event, int *Match)
{
  cTimer *t = NULL;
  int m = tmNone;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      int tm = ti->Matches(Event);
      if (tm > m) {
         t = ti;
         m = tm;
         if (m == tmFull)
            break;
         }
      }
  if (Match)
     *Match = m;
  return t;
}

cTimer *cTimers::GetNextActiveTimer(void)
{
  cTimer *t0 = NULL;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if ((ti->HasFlags(tfActive)) && (!t0 || ti->Compare(*t0) < 0))
         t0 = ti;
      }
  return t0;
}

void cTimers::SetModified(void)
{
  modified = true;
}

bool cTimers::Modified(void)
{
  bool Result = modified;
  modified = false;
  return Result;
}

#define EPGLIMITPAST   (2 * 3600) // time in seconds around now, within which EPG events will be taken into consideration
#define EPGLIMITFUTURE (4 * 3600)

void cTimers::SetEvents(void)
{
  if (time(NULL) - lastSetEvents < 5)
     return;
  cSchedulesLock SchedulesLock(false, 100);
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  if (Schedules) {
     if (!lastSetEvents || Schedules->Modified() >= lastSetEvents) {
        for (cTimer *ti = First(); ti; ti = Next(ti)) {
            const cSchedule *Schedule = Schedules->GetSchedule(ti->Channel()->GetChannelID());
            if (Schedule) {
               if (!lastSetEvents || Schedule->Modified() >= lastSetEvents) {
                  const cEvent *Event = NULL;
                  int Overlap = 0;
                  int Distance = INT_MIN;
                  time_t now = time(NULL);
                  for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
                      if (cRemote::HasKeys())
                         return; // react immediately on user input
                      if (e->EndTime() < now - EPGLIMITPAST)
                         continue; // skip old events
                      if (e->StartTime() > now + EPGLIMITFUTURE)
                         break; // no need to process events too far in the future
                      int overlap = 0;
                      ti->Matches(e, &overlap);
                      if (overlap && overlap >= Overlap) {
                         int distance = 0;
                         if (now < e->StartTime())
                            distance = e->StartTime() - now;
                         else if (now > e->EndTime())
                            distance = e->EndTime() - now;
                         if (Event && overlap == Overlap) {
                            if (Overlap > FULLMATCH) { // this means VPS
                               if (abs(Distance) < abs(distance)) 
                                  break; // we've already found the closest VPS event
                               }
                            else if (e->Duration() <= Event->Duration())
                               continue; // if overlap is the same, we take the longer event
                            }
                         Overlap = overlap;
                         Distance = distance;
                         Event = e;
                         }
                      }
                  if (Event && Event->EndTime() < now - EXPIRELATENCY && !Event->IsRunning())
                     Event = NULL;
                  ti->SetEvent(Schedule, Event);
                  }
               }
            }
        }
     }
  lastSetEvents = time(NULL);
}

void cTimers::DeleteExpired(void)
{
  cTimer *ti = First();
  while (ti) {
        cTimer *next = Next(ti);
        if (ti->Expired()) {
           isyslog("deleting timer %s", *ti->ToDescr());
           Del(ti);
           SetModified();
           }
        ti = next;
        }
}
