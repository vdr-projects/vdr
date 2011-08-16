/*
 * timers.c: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.c 2.5 2011/08/06 13:13:54 kls Exp $
 */

#include "timers.h"
#include <ctype.h>
#include "channels.h"
#include "device.h"
#include "i18n.h"
#include "libsi/si.h"
#include "recording.h"
#include "remote.h"
#include "status.h"

#define VFAT_MAX_FILENAME 40 // same as MAX_SUBTITLE_LENGTH in recording.c

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// --- cTimer ----------------------------------------------------------------

cTimer::cTimer(bool Instant, bool Pause, cChannel *Channel)
{
  startTime = stopTime = 0;
  lastSetEvent = 0;
  deferred = 0;
  recording = pending = inVpsMargin = false;
  flags = tfNone;
  if (Instant)
     SetFlags(tfActive | tfInstant);
  channel = Channel ? Channel : Channels.GetByNumber(cDevice::CurrentChannel());
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
  aux = NULL;
  event = NULL;
  if (Instant && channel)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", *Setup.NameInstantRecord ? Setup.NameInstantRecord : channel->Name());
  if (VfatFileSystem && (Utf8StrLen(file) > VFAT_MAX_FILENAME)) {
     dsyslog("timer file name too long for VFAT file system: '%s'", file);
     file[Utf8SymChars(file, VFAT_MAX_FILENAME)] = 0;
     dsyslog("timer file name truncated to '%s'", file);
     }
}

cTimer::cTimer(const cEvent *Event)
{
  startTime = stopTime = 0;
  lastSetEvent = 0;
  deferred = 0;
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
     Utf8Strn0Cpy(file, Event->Title(), sizeof(file));
  if (VfatFileSystem && (Utf8StrLen(file) > VFAT_MAX_FILENAME)) {
     dsyslog("timer file name too long for VFAT file system: '%s'", file);
     file[Utf8SymChars(file, VFAT_MAX_FILENAME)] = 0;
     dsyslog("timer file name truncated to '%s'", file);
     }
  aux = NULL;
  event = NULL; // let SetEvent() be called to get a log message
}

cTimer::cTimer(const cTimer &Timer)
{
  channel = NULL;
  aux = NULL;
  event = NULL;
  flags = tfNone;
  *this = Timer;
}

cTimer::~cTimer()
{
  free(aux);
}

cTimer& cTimer::operator= (const cTimer &Timer)
{
  if (&Timer != this) {
     uint OldFlags = flags & tfRecording;
     startTime    = Timer.startTime;
     stopTime     = Timer.stopTime;
     lastSetEvent = 0;
     deferred = 0;
     recording    = Timer.recording;
     pending      = Timer.pending;
     inVpsMargin  = Timer.inVpsMargin;
     flags        = Timer.flags | OldFlags;
     channel      = Timer.channel;
     day          = Timer.day;
     weekdays     = Timer.weekdays;
     start        = Timer.start;
     stop         = Timer.stop;
     priority     = Timer.priority;
     lifetime     = Timer.lifetime;
     strncpy(file, Timer.file, sizeof(file));
     free(aux);
     aux = Timer.aux ? strdup(Timer.aux) : NULL;
     event = NULL;
     }
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

cString cTimer::ToText(bool UseChannelID) const
{
  strreplace(file, ':', '|');
  cString buffer = cString::sprintf("%u:%s:%s:%04d:%04d:%d:%d:%s:%s\n", flags, UseChannelID ? *Channel()->GetChannelID().ToString() : *itoa(Channel()->Number()), *PrintDay(day, weekdays, true), start, stop, priority, lifetime, file, aux ? aux : "");
  strreplace(file, '|', ':');
  return buffer;
}

cString cTimer::ToDescr(void) const
{
  return cString::sprintf("%d (%d %04d-%04d %s'%s')", Index() + 1, Channel()->Number(), start, stop, HasFlags(tfVps) ? "VPS " : "", file);
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

cString cTimer::PrintDay(time_t Day, int WeekDays, bool SingleByteChars)
{
#define DAYBUFFERSIZE 64
  char buffer[DAYBUFFERSIZE];
  char *b = buffer;
  if (WeekDays) {
     // TRANSLATORS: the first character of each weekday, beginning with monday
     const char *w = trNOOP("MTWTFSS");
     if (!SingleByteChars)
        w = tr(w);
     while (*w) {
           int sl = Utf8CharLen(w);
           if (WeekDays & 1) {
              for (int i = 0; i < sl; i++)
                  b[i] = w[i];
              b += sl;
              }
           else
              *b++ = '-';
           WeekDays >>= 1;
           w += sl;
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
     cString s = PrintDay(day, weekdays, true);
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
  free(aux);
  aux = NULL;
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
  if (8 <= sscanf(s, "%u :%a[^:]:%a[^:]:%d :%d :%d :%d :%a[^:\n]:%a[^\n]", &flags, &channelbuffer, &daybuffer, &start, &stop, &priority, &lifetime, &filebuffer, &aux)) {
     ClrFlags(tfRecording);
     if (aux && !*skipspace(aux)) {
        free(aux);
        aux = NULL;
        }
     //TODO add more plausibility checks
     result = ParseDay(daybuffer, day, weekdays);
     if (VfatFileSystem) {
        char *p = strrchr(filebuffer, FOLDERDELIMCHAR);
        if (p)
           p++;
        else
           p = filebuffer;
        if (Utf8StrLen(p) > VFAT_MAX_FILENAME) {
           dsyslog("timer file name too long for VFAT file system: '%s'", p);
           p[Utf8SymChars(p, VFAT_MAX_FILENAME)] = 0;
           dsyslog("timer file name truncated to '%s'", p);
           }
        }
     Utf8Strn0Cpy(file, filebuffer, sizeof(file));
     strreplace(file, '|', ':');
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
  return weekday == 0 ? 6 : weekday - 1; // we start with Monday==0!
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
     Utf8Strn0Cpy(file, File, sizeof(file));
  return file;
}

bool cTimer::Matches(time_t t, bool Directly, int Margin) const
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
         time_t t0 = IncDay(day ? max(day, t) : t, i);
         if (DayMatches(t0)) {
            time_t a = SetTime(t0, begin);
            time_t b = a + length;
            if ((!day || a >= day) && t < b) {
               startTime = a;
               stopTime = b;
               break;
               }
            }
         }
     if (!startTime)
        startTime = IncDay(t, 7); // just to have something that's more than a week in the future
     else if (!Directly && (t > startTime || t > day + SECSINDAY + 3600)) // +3600 in case of DST change
        day = 0;
     }

  if (t < deferred)
     return false;
  deferred = 0;

  if (HasFlags(tfActive)) {
     if (HasFlags(tfVps) && event && event->Vps()) {
        if (Margin || !Directly) {
           startTime = event->StartTime();
           stopTime = event->EndTime();
           if (!Margin)
              return event->IsRunning(true);
           }
        }
     return startTime <= t + Margin && t < stopTime; // must stop *before* stopTime to allow adjacent timers
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
     if (UseVps)
        return overlap > FULLMATCH ? tmFull : tmNone;
     return overlap >= FULLMATCH ? tmFull : overlap > 0 ? tmPartial : tmNone;
     }
  return tmNone;
}

#define EXPIRELATENCY 60 // seconds (just in case there's a short glitch in the VPS signal)

bool cTimer::Expired(void) const
{
  return IsSingleEvent() && !Recording() && StopTime() + EXPIRELATENCY <= time(NULL) && (!HasFlags(tfVps) || !event || !event->Vps());
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

#define EPGLIMITBEFORE   (1 * 3600) // Time in seconds before a timer's start time and
#define EPGLIMITAFTER    (1 * 3600) // after its stop time within which EPG events will be taken into consideration.

void cTimer::SetEventFromSchedule(const cSchedules *Schedules)
{
  cSchedulesLock SchedulesLock;
  if (!Schedules) {
     lastSetEvent = 0; // forces setting the event, even if the schedule hasn't been modified
     if (!(Schedules = cSchedules::Schedules(SchedulesLock)))
        return;
     }
  const cSchedule *Schedule = Schedules->GetSchedule(Channel());
  if (Schedule && Schedule->Events()->First()) {
     time_t now = time(NULL);
     if (!lastSetEvent || Schedule->Modified() >= lastSetEvent) {
        lastSetEvent = now;
        const cEvent *Event = NULL;
        if (HasFlags(tfVps) && Schedule->Events()->First()->Vps()) {
           if (event && Recording())
              return; // let the recording end first
           // VPS timers only match if their start time exactly matches the event's VPS time:
           for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
               if (e->StartTime() && e->RunningStatus() != SI::RunningStatusNotRunning) { // skip outdated events
                  int overlap = 0;
                  Matches(e, &overlap);
                  if (overlap > FULLMATCH) {
                     Event = e;
                     break; // take the first matching event
                     }
                  }
               }
           if (!Event && event && (now <= event->EndTime() || Matches(0, true)))
              return; // stay with the old event until the timer has completely expired
           }
        else {
           // Normal timers match the event they have the most overlap with:
           int Overlap = 0;
           // Set up the time frame within which to check events:
           Matches(0, true);
           time_t TimeFrameBegin = StartTime() - EPGLIMITBEFORE;
           time_t TimeFrameEnd   = StopTime()  + EPGLIMITAFTER;
           for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
               if (e->EndTime() < TimeFrameBegin)
                  continue; // skip events way before the timer starts
               if (e->StartTime() > TimeFrameEnd)
                  break; // the rest is way after the timer ends
               int overlap = 0;
               Matches(e, &overlap);
               if (overlap && overlap >= Overlap) {
                  if (Event && overlap == Overlap && e->Duration() <= Event->Duration())
                     continue; // if overlap is the same, we take the longer event
                  Overlap = overlap;
                  Event = e;
                  }
               }
           }
        SetEvent(Event);
        }
     }
}

void cTimer::SetEvent(const cEvent *Event)
{
  if (event != Event) { //XXX TODO check event data, too???
     if (Event)
        isyslog("timer %s set to event %s", *ToDescr(), *Event->ToDescr());
     else
        isyslog("timer %s set to no event", *ToDescr());
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

void cTimer::SetPriority(int Priority)
{
  priority = Priority;
}

void cTimer::SetDeferred(int Seconds)
{
  deferred = time(NULL) + Seconds;
  isyslog("timer %s deferred for %d seconds", *ToDescr(), Seconds);
}

void cTimer::SetFlags(uint Flags)
{
  flags |= Flags;
}

void cTimer::ClrFlags(uint Flags)
{
  flags &= ~Flags;
}

void cTimer::InvFlags(uint Flags)
{
  flags ^= Flags;
}

bool cTimer::HasFlags(uint Flags) const
{
  return (flags & Flags) == Flags;
}

void cTimer::Skip(void)
{
  day = IncDay(SetTime(StartTime(), 0), 1);
  startTime = 0;
  SetEvent(NULL);
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
  SetEvent(NULL);
  Matches(); // refresh start and end time
}

// --- cTimers ---------------------------------------------------------------

cTimers Timers;

cTimers::cTimers(void)
{
  state = 0;
  beingEdited = 0;;
  lastSetEvents = 0;
  lastDeleteExpired = 0;
}

cTimer *cTimers::GetTimer(cTimer *Timer)
{
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->Channel() == Timer->Channel() &&
          (ti->WeekDays() && ti->WeekDays() == Timer->WeekDays() || !ti->WeekDays() && ti->Day() == Timer->Day()) &&
          ti->Start() == Timer->Start() &&
          ti->Stop() == Timer->Stop())
         return ti;
      }
  return NULL;
}

cTimer *cTimers::GetMatch(time_t t)
{
  static int LastPending = -1;
  cTimer *t0 = NULL;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Recording() && ti->Matches(t)) {
         if (ti->Pending()) {
            if (ti->Index() > LastPending)
               LastPending = ti->Index();
            else
               continue;
            }
         if (!t0 || ti->Priority() > t0->Priority())
            t0 = ti;
         }
      }
  if (!t0)
     LastPending = -1;
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
      ti->Matches();
      if ((ti->HasFlags(tfActive)) && (!t0 || ti->StopTime() > time(NULL) && ti->Compare(*t0) < 0))
         t0 = ti;
      }
  return t0;
}

void cTimers::SetModified(void)
{
  cStatus::MsgTimerChange(NULL, tcMod);
  state++;
}

void cTimers::Add(cTimer *Timer, cTimer *After)
{
  cConfig<cTimer>::Add(Timer, After);
  cStatus::MsgTimerChange(Timer, tcAdd);
}

void cTimers::Ins(cTimer *Timer, cTimer *Before)
{
  cConfig<cTimer>::Ins(Timer, Before);
  cStatus::MsgTimerChange(Timer, tcAdd);
}

void cTimers::Del(cTimer *Timer, bool DeleteObject)
{
  cStatus::MsgTimerChange(Timer, tcDel);
  cConfig<cTimer>::Del(Timer, DeleteObject);
}

bool cTimers::Modified(int &State)
{
  bool Result = state != State;
  State = state;
  return Result;
}

void cTimers::SetEvents(void)
{
  if (time(NULL) - lastSetEvents < 5)
     return;
  cSchedulesLock SchedulesLock(false, 100);
  const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
  if (Schedules) {
     if (!lastSetEvents || Schedules->Modified() >= lastSetEvents) {
        for (cTimer *ti = First(); ti; ti = Next(ti)) {
            if (cRemote::HasKeys())
               return; // react immediately on user input
            ti->SetEventFromSchedule(Schedules);
            }
        }
     }
  lastSetEvents = time(NULL);
}

void cTimers::DeleteExpired(void)
{
  if (time(NULL) - lastDeleteExpired < 30)
     return;
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
  lastDeleteExpired = time(NULL);
}
