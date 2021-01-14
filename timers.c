/*
 * timers.c: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.c 5.3 2021/01/14 10:29:05 kls Exp $
 */

#include "timers.h"
#include <ctype.h>
#include "device.h"
#include "i18n.h"
#include "libsi/si.h"
#include "recording.h"
#include "remote.h"
#include "status.h"
#include "svdrp.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// --- cTimer ----------------------------------------------------------------

cTimer::cTimer(bool Instant, bool Pause, const cChannel *Channel)
{
  id = 0;
  startTime = stopTime = 0;
  scheduleState = -1;
  deferred = 0;
  pending = inVpsMargin = false;
  flags = tfNone;
  *pattern = 0;
  *file = 0;
  aux = NULL;
  remote = NULL;
  event = NULL;
  if (Instant)
     SetFlags(tfActive | tfInstant);
  LOCK_CHANNELS_READ;
  channel = Channel ? Channel : Channels->GetByNumber(cDevice::CurrentChannel());
  time_t t = time(NULL);
  struct tm tm_r;
  struct tm *now = localtime_r(&t, &tm_r);
  day = SetTime(t, 0);
  weekdays = 0;
  start = now->tm_hour * 100 + now->tm_min;
  stop = 0;
  if (!Setup.InstantRecordTime && channel && (Instant || Pause)) {
     LOCK_SCHEDULES_READ;
     if (const cSchedule *Schedule = Schedules->GetSchedule(channel)) {
        if (const cEvent *Event = Schedule->GetPresentEvent()) {
           time_t tstart = Event->StartTime();
           time_t tstop = Event->EndTime();
           if (Event->Vps() && Setup.UseVps) {
              SetFlags(tfVps);
              tstart = Event->Vps();
              }
           else {
              tstop  += Setup.MarginStop * 60;
              tstart -= Setup.MarginStart * 60;
              }
           day = SetTime(tstart, 0);
           struct tm *time = localtime_r(&tstart, &tm_r);
           start = time->tm_hour * 100 + time->tm_min;
           time = localtime_r(&tstop, &tm_r);
           stop = time->tm_hour * 100 + time->tm_min;
           SetEvent(Event);
           }
        }
     }
  if (!stop) {
     stop = now->tm_hour * 60 + now->tm_min + (Setup.InstantRecordTime ? Setup.InstantRecordTime : DEFINSTRECTIME);
     stop = (stop / 60) * 100 + (stop % 60);
     }
  if (stop >= 2400)
     stop -= 2400;
  priority = Pause ? Setup.PausePriority : Setup.DefaultPriority;
  lifetime = Pause ? Setup.PauseLifetime : Setup.DefaultLifetime;
  if (Instant && channel)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", *Setup.NameInstantRecord ? Setup.NameInstantRecord : channel->Name());
}

static bool MatchPattern(const char *Pattern, const char *Title, cString *Before = NULL, cString *Match = NULL, cString *After = NULL)
{
  if (Title) {
     bool AvoidDuplicates = startswith(Pattern, TIMERPATTERN_AVOID);
     if (AvoidDuplicates)
        Pattern++;
     if (strcmp(Pattern, "*") == 0) {
        if (Before)
           *Before = "";
        if (Match)
           *Match = Title;
        if (After)
           *After = "";
        return true;
        }
     bool AnchorBegin = startswith(Pattern, TIMERPATTERN_BEGIN);
     if (AnchorBegin)
        Pattern++;
     bool AnchorEnd = endswith(Pattern, TIMERPATTERN_END);
     cNullTerminate nt;
     if (AnchorEnd)
        nt.Set(const_cast<char *>(Pattern + strlen(Pattern) - 1));
     if (AnchorBegin && AnchorEnd) {
        if (strcmp(Title, Pattern) == 0) {
           if (Before)
              *Before = "";
           if (Match)
              *Match = Title;
           if (After)
              *After = "";
           return true;
           }
        }
     else if (AnchorBegin) {
        if (strstr(Title, Pattern) == Title) {
           if (Before)
              *Before = "";
           if (Match)
              *Match = Pattern;
           if (After)
              *After = cString(Title + strlen(Pattern));
           return true;
           }
        }
     else if (AnchorEnd) {
        if (endswith(Title, Pattern)) {
           if (Before)
              *Before = cString(Title, Title + strlen(Title) - strlen(Pattern));
           if (Match)
              *Match = Pattern;
           if (After)
              *After = "";
           return true;
           }
        }
     else if (const char *p = strstr(Title, Pattern)) {
        if (Before)
           *Before = cString(Title, p);
        if (Match)
           *Match = Pattern;
        if (After)
           *After = cString(p + strlen(Pattern));
        return true;
        }
     }
  return false;
}

static cString MakePatternFileName(const char *Pattern, const char *Title, const char *Episode, const char *File)
{
  if (!Pattern || !Title || !File)
     return NULL;
  cString Before = "";
  cString Match = "";
  cString After = "";
  if (MatchPattern(Pattern, Title, &Before, &Match, &After)) {
     char *Result = strdup(File);
     Result = strreplace(Result, TIMERMACRO_TITLE, Title);
     Result = strreplace(Result, TIMERMACRO_EPISODE, Episode);
     Result = strreplace(Result, TIMERMACRO_BEFORE, Before);
     Result = strreplace(Result, TIMERMACRO_MATCH, Match);
     Result = strreplace(Result, TIMERMACRO_AFTER, After);
     return cString(Result, true);;
     }
  return NULL;
}

cTimer::cTimer(const cEvent *Event, const char *FileName, const cTimer *PatternTimer)
{
  id = 0;
  startTime = stopTime = 0;
  scheduleState = -1;
  deferred = 0;
  pending = inVpsMargin = false;
  flags = tfActive;
  *pattern = 0;
  *file = 0;
  aux = NULL;
  remote = NULL;
  event = NULL;
  if (!PatternTimer || PatternTimer->HasFlags(tfVps)) {
     if (Event->Vps() && (PatternTimer || Setup.UseVps))
        SetFlags(tfVps);
     }
  LOCK_CHANNELS_READ;
  channel = Channels->GetByChannelID(Event->ChannelID(), true);
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
  priority = PatternTimer ? PatternTimer->Priority() : Setup.DefaultPriority;
  lifetime = PatternTimer ? PatternTimer->Lifetime() : Setup.DefaultLifetime;
  if (!FileName)
     FileName = Event->Title();
  if (!isempty(FileName))
     Utf8Strn0Cpy(file, FileName, sizeof(file));
  SetEvent(Event);
}

cTimer::cTimer(const cTimer &Timer)
{
  channel = NULL;
  aux = NULL;
  remote = NULL;
  event = NULL;
  flags = tfNone;
  *this = Timer;
}

cTimer::~cTimer()
{
  if (event)
     event->DecNumTimers();
  free(aux);
  free(remote);
}

cTimer& cTimer::operator= (const cTimer &Timer)
{
  if (&Timer != this) {
     id           = Timer.id;
     startTime    = Timer.startTime;
     stopTime     = Timer.stopTime;
     scheduleState = -1;
     deferred     = 0;
     pending      = Timer.pending;
     inVpsMargin  = Timer.inVpsMargin;
     flags        = Timer.flags;
     channel      = Timer.channel;
     day          = Timer.day;
     weekdays     = Timer.weekdays;
     start        = Timer.start;
     stop         = Timer.stop;
     priority     = Timer.priority;
     lifetime     = Timer.lifetime;
     strncpy(pattern, Timer.pattern, sizeof(pattern));
     strncpy(file, Timer.file, sizeof(file));
     free(aux);
     aux = Timer.aux ? strdup(Timer.aux) : NULL;
     free(remote);
     remote = Timer.remote ? strdup(Timer.remote) : NULL;
     if (event)
        event->DecNumTimers();
     event = Timer.event;
     if (event)
        event->IncNumTimers();
     }
  return *this;
}

int cTimer::Compare(const cListObject &ListObject) const
{
  const cTimer *ti = (const cTimer *)&ListObject;
  time_t t1 = StartTime();
  time_t t2 = ti->StartTime();
  int r = t1 - t2;
  if (r == 0)
     r = ti->priority - priority;
  if (IsPatternTimer() ^ ti->IsPatternTimer()) {
     if (IsPatternTimer())
        r = 1;
     else
        r = -1;
     }
  else if (IsPatternTimer() && ti->IsPatternTimer())
     r = strcoll(Pattern(), ti->Pattern());
  return r;
}

cString cTimer::PatternAndFile(void) const
{
  if (IsPatternTimer())
     return cString::sprintf("{%s}%s", pattern, file);
  return file;
}

cString cTimer::ToText(bool UseChannelID) const
{
  strreplace(pattern, ':', '|');
  strreplace(file, ':', '|');
  cString buffer = cString::sprintf("%u:%s:%s:%04d:%04d:%d:%d:%s:%s", flags, UseChannelID ? *Channel()->GetChannelID().ToString() : *itoa(Channel()->Number()), *PrintDay(day, weekdays, true), start, stop, priority, lifetime, *PatternAndFile(), aux ? aux : "");
  strreplace(pattern, '|', ':');
  strreplace(file, '|', ':');
  return buffer;
}

cString cTimer::ToDescr(void) const
{
  return cString::sprintf("%d%s%s (%d %04d-%04d %s'%s')", Id(), remote ? "@" : "", remote ? remote : "", Channel()->Number(), start, stop, HasFlags(tfVps) ? "VPS " : "", *PatternAndFile());
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
  //XXX Apparently sscanf() doesn't work correctly if the last %m argument
  //XXX results in an empty string (this first occurred when the EIT gathering
  //XXX was put into a separate thread - don't know why this happens...
  //XXX As a cure we copy the original string and add a blank.
  //XXX If anybody can shed some light on why sscanf() fails here, I'd love
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
  if (8 <= sscanf(s, "%u :%m[^:]:%m[^:]:%d :%d :%d :%d :%m[^:\n]:%m[^\n]", &flags, &channelbuffer, &daybuffer, &start, &stop, &priority, &lifetime, &filebuffer, &aux)) {
     if (aux && !*skipspace(aux)) {
        free(aux);
        aux = NULL;
        }
     //TODO add more plausibility checks
     result = ParseDay(daybuffer, day, weekdays);
     char *fb = filebuffer;
     if (*fb == '{') {
        if (char *p = strchr(fb, '}')) {
           *p = 0;
           Utf8Strn0Cpy(pattern, fb + 1, sizeof(pattern));
           strreplace(pattern, '|', ':');
           fb = p + 1;
           }
        }
     else
        *pattern = 0;
     Utf8Strn0Cpy(file, fb, sizeof(file));
     strreplace(file, '|', ':');
     LOCK_CHANNELS_READ;
     if (isnumber(channelbuffer))
        channel = Channels->GetByNumber(atoi(channelbuffer));
     else
        channel = Channels->GetByChannelID(tChannelID::FromString(channelbuffer), true, true);
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
  if (!Remote())
     return fprintf(f, "%s\n", *ToText(true)) > 0;
  return true;
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

void cTimer::SetPattern(const char *Pattern)
{
  Utf8Strn0Cpy(pattern, Pattern, sizeof(pattern));
}

void cTimer::SetFile(const char *File)
{
  if (!isempty(File))
     Utf8Strn0Cpy(file, File, sizeof(file));
}

#define EITPRESENTFOLLOWINGRATE 10 // max. seconds between two occurrences of the "EIT present/following table for the actual multiplex" (2s by the standard, using some more for safety)

bool cTimer::Matches(time_t t, bool Directly, int Margin) const
{
  startTime = stopTime = 0;
  if (t == 0)
     t = time(NULL);

  int begin  = TimeToInt(start); // seconds from midnight
  int end    = TimeToInt(stop);
  int length = end - begin;

  if (IsSingleEvent()) {
     time_t t0 = day;
     startTime = SetTime(t0, begin);
     if (length < 0)
        t0 = IncDay(day, 1);
     stopTime  = SetTime(t0, end);
     }
  else {
     time_t d = day ? max(day, t) : t;
     for (int i = -1; i <= 7; i++) {
         time_t t0 = IncDay(d, i);
         if (DayMatches(t0)) {
            time_t a = SetTime(t0, begin);
            if (length < 0)
               t0 = IncDay(d, i + 1);
            time_t b = SetTime(t0, end);
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

  if (IsPatternTimer())
     return false; // we only need to have start/stopTime initialized

  if (t < deferred)
     return false;
  deferred = 0;

  if (HasFlags(tfActive)) {
     if (HasFlags(tfVps) && event && event->Vps()) {
        if (Margin || !Directly) {
           startTime = event->StartTime();
           stopTime = event->EndTime();
           if (!Margin) { // this is an actual check
              if (event->Schedule()->PresentSeenWithin(EITPRESENTFOLLOWINGRATE)) // VPS control can only work with up-to-date events...
                 return event->IsRunning(true);
              return startTime <= t && t < stopTime; // ...otherwise we fall back to normal timer handling
              }
           }
        }
     return startTime <= t + Margin && t < stopTime; // must stop *before* stopTime to allow adjacent timers
     }
  return false;
}

#define FULLMATCH 1000

eTimerMatch cTimer::Matches(const cEvent *Event, int *Overlap) const
{
  // Overlap is the percentage of the Event's duration that is covered by
  // this timer (based on FULLMATCH for finer granularity than just 100).
  // To make sure a VPS timer can be distinguished from a plain 100% overlap,
  // it gets an additional 100 added, and a VPS event that is actually running
  // gets 200 added to the FULLMATCH.
  if (channel->GetChannelID() == Event->ChannelID()) {
     bool UseVps = HasFlags(tfVps) && Event->Vps();
     if (IsPatternTimer()) {
        if (startswith(Pattern(), TIMERPATTERN_AVOID)) {
           cString FileName = MakePatternFileName(Pattern(), Event->Title(), Event->ShortText(), File());
           if (*FileName) {
              const char *p = strgetlast(*FileName, FOLDERDELIMCHAR);
              if (DoneRecordingsPattern.Contains(p))
                 return tmNone;
              }
           else
              return tmNone;
           }
        else if (!MatchPattern(Pattern(), Event->Title()))
           return tmNone;
        UseVps = false;
        }
     Matches(UseVps ? Event->Vps() : Event->StartTime(), true);
     int overlap = 0;
     if (UseVps) {
        if (startTime == Event->Vps()) {
           overlap = FULLMATCH;
           if (Event->IsRunning())
              overlap += 200;
           else if (Event->RunningStatus() != SI::RunningStatusNotRunning)
              overlap += 100;
           }
        }
     else {
        if (startTime <= Event->StartTime() && Event->EndTime() <= stopTime)
           overlap = FULLMATCH;
        else if (stopTime <= Event->StartTime() || Event->EndTime() <= startTime)
           overlap = 0;
        else {
           overlap = (min(stopTime, Event->EndTime()) - max(startTime, Event->StartTime())) * FULLMATCH / max(Event->Duration(), 1);
           if (IsPatternTimer() && overlap > 0)
              overlap = FULLMATCH;
           }
        }
     startTime = stopTime = 0;
     if (Overlap)
        *Overlap = overlap;
     return overlap >= FULLMATCH ? tmFull : overlap > 0 ? tmPartial : tmNone;
     }
  return tmNone;
}

#define EXPIRELATENCY 60 // seconds (just in case there's a short glitch in the VPS signal)

bool cTimer::Expired(void) const
{
  return IsSingleEvent()
      && !Recording()
      && StopTime() + EXPIRELATENCY <= time(NULL)
      && (!HasFlags(tfVps) || !event || !event->Vps() || event->EndTime() + EXPIRELATENCY <= time(NULL));
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

void cTimer::SetId(int Id)
{
  id = Id;
}

void cTimer::SpawnPatternTimer(const cEvent *Event, cTimers *Timers)
{
  cString FileName = MakePatternFileName(Pattern(), Event->Title(), Event->ShortText(), File());
  isyslog("spawning timer %s for event %s", *ToDescr(), *Event->ToDescr());
  cTimer *t = new cTimer(Event, FileName, this);
  t->SetFlags(tfSpawned);
  if (startswith(Pattern(), TIMERPATTERN_AVOID))
     t->SetFlags(tfAvoid);
  Timers->Add(t);
  HandleRemoteTimerModifications(t);
}

bool cTimer::SpawnPatternTimers(const cSchedules *Schedules, cTimers *Timers)
{
  bool TimersSpawned = false;
  const cSchedule *Schedule = Schedules->GetSchedule(Channel());
  if (Schedule && Schedule->Events()->First()) {
     if (Schedule->Modified(scheduleState)) {
        time_t Now = time(NULL);
        for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
            if (Matches(e) != tmNone) {
               bool CheckThis = false;
               bool CheckNext = false;
               if (e->HasTimer()) // a matching event that already has a timer
                  CheckNext = true;
               else if (e->EndTime() > Now) { // only look at events that have not yet ended
                  CheckThis = true;
                  CheckNext = true;
                  }
               if (CheckThis) {
                  SpawnPatternTimer(e, Timers);
                  TimersSpawned = true;
                  }
               if (CheckNext) {
                  // We also check the event immediately following this one:
                  e = Schedule->Events()->Next(e);
                  if (e && !e->HasTimer() && Matches(e) != tmNone) {
                     SpawnPatternTimer(e, Timers);
                     TimersSpawned = true;
                     }
                  }
               if (CheckThis || CheckNext)
                  break;
               }
            }
        }
     }
  return TimersSpawned;
}

void cTimer::TriggerRespawn(void)
{
  if (HasFlags(tfSpawned) || IsPatternTimer()) {
     if (Channel()) {
        LOCK_CHANNELS_READ;
        if (const cSchedule *Schedule = Channel()->Schedule()) {
           dsyslog("triggering respawn for timer %s", *ToDescr());
           LOCK_SCHEDULES_WRITE;
           const_cast<cSchedule *>(Schedule)->SetModified();
           }
        }
     }
}

bool cTimer::SetEventFromSchedule(const cSchedules *Schedules)
{
  if (IsPatternTimer())
     return SetEvent(NULL);
  const cSchedule *Schedule = Schedules->GetSchedule(Channel());
  if (Schedule && Schedule->Events()->First()) {
     if (Schedule->Modified(scheduleState)) {
        const cEvent *Event = NULL;
        if (HasFlags(tfVps) && Schedule->Events()->First()->Vps()) {
           // VPS timers only match if their start time exactly matches the event's VPS time:
           for (const cEvent *e = Schedule->Events()->First(); e; e = Schedule->Events()->Next(e)) {
               if (e->StartTime()) {
                  int overlap = 0;
                  if (Matches(e, &overlap) == tmFull) {
                     Event = e;
                     if (overlap > FULLMATCH)
                        break; // take the first matching event
                     }
                  }
               }
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
        return SetEvent(Event);
        }
     }
  return false;
}

bool cTimer::SetEvent(const cEvent *Event)
{
  if (event != Event) {
     if (event)
        event->DecNumTimers();
     if (Event) {
        isyslog("timer %s set to event %s", *ToDescr(), *Event->ToDescr());
        Event->IncNumTimers();
        Event->Schedule()->Modified(scheduleState); // to get the current state
        }
     else {
        isyslog("timer %s set to no event", *ToDescr());
        scheduleState = -1;
        }
     event = Event;
     return true;
     }
  return false;
}

void cTimer::SetRecording(bool Recording)
{
  if (Recording)
     SetFlags(tfRecording);
  else
     ClrFlags(tfRecording);
  isyslog("timer %s %s", *ToDescr(), Recording ? "start" : "stop");
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

void cTimer::SetDay(time_t Day)
{
  day = Day;
}

void cTimer::SetWeekDays(int WeekDays)
{
  weekdays = WeekDays;
}

void cTimer::SetStart(int Start)
{
  start = Start;
}

void cTimer::SetStop(int Stop)
{
  stop = Stop;
}

void cTimer::SetPriority(int Priority)
{
  priority = Priority;
}

void cTimer::SetLifetime(int Lifetime)
{
  lifetime = Lifetime;
}

void cTimer::SetAux(const char *Aux)
{
  free(aux);
  aux = Aux ? strdup(Aux) : NULL;
}

void cTimer::SetRemote(const char *Remote)
{
  free(remote);
  remote = Remote ? strdup(Remote) : NULL;
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
  if (IsSingleEvent() || IsPatternTimer())
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
  if (HasFlags(tfActive))
     TriggerRespawn(); // have pattern timers spawn if necessary
  Matches(); // refresh start and end time
}

// --- cTimers ---------------------------------------------------------------

cTimers cTimers::timers;
int cTimers::lastTimerId = 0;

cTimers::cTimers(void)
:cConfig<cTimer>("1 Timers")
{
  lastDeleteExpired = 0;
}

bool cTimers::Load(const char *FileName)
{
  LOCK_TIMERS_WRITE;
  Timers->SetExplicitModify();
  if (timers.cConfig<cTimer>::Load(FileName)) {
     for (cTimer *ti = timers.First(); ti; ti = timers.Next(ti)) {
         ti->SetId(NewTimerId());
         ti->ClrFlags(tfRecording);
         Timers->SetModified();
         }
     return true;
     }
  return false;
}

int cTimers::NewTimerId(void)
{
  return ++lastTimerId; // no need for locking, the caller must have a lock on the global Timers list
}

const cTimer *cTimers::GetById(int Id, const char *Remote) const
{
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->Id() == Id) {
         if (!Remote && !ti->Remote() || Remote && ti->Remote() && strcmp(Remote, ti->Remote()) == 0)
            return ti;
         }
      }
  return NULL;
}

const cTimer *cTimers::GetTimer(const cTimer *Timer) const
{
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Remote() &&
          ti->Channel() == Timer->Channel() &&
          (ti->WeekDays() && ti->WeekDays() == Timer->WeekDays() || !ti->WeekDays() && ti->Day() == Timer->Day()) &&
          ti->Start() == Timer->Start() &&
          ti->Stop() == Timer->Stop())
         return ti;
      }
  return NULL;
}

const cTimer *cTimers::GetMatch(time_t t) const
{
  static int LastPending = -1;
  const cTimer *t0 = NULL;
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Remote() && !ti->Recording() && ti->Matches(t)) {
         if (ti->Pending()) {
            if (ti->Index() > LastPending) {
               LastPending = ti->Index();
               return ti;
               }
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

const cTimer *cTimers::GetMatch(const cEvent *Event, eTimerMatch *Match) const
{
  const cTimer *t = NULL;
  eTimerMatch m = tmNone;
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      eTimerMatch tm = ti->Matches(Event);
      if (tm > m || tm == tmFull && ti->Local()) {
         t = ti;
         m = tm;
         if (m == tmFull && ti->Local())
            break;
         }
      }
  if (Match)
     *Match = m;
  return t;
}

int cTimers::GetMaxPriority(void) const
{
  int n = -1;
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Remote() && ti->Recording())
         n = max(n, ti->Priority());
      }
  return n;
}

const cTimer *cTimers::GetNextActiveTimer(void) const
{
  const cTimer *t0 = NULL;
  for (const cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->Remote() && !ti->IsPatternTimer()) {
         ti->Matches();
         if ((ti->HasFlags(tfActive)) && (!t0 || ti->StopTime() > time(NULL) && ti->Compare(*t0) < 0))
            t0 = ti;
         }
      }
  return t0;
}

const cTimers *cTimers::GetTimersRead(cStateKey &StateKey, int TimeoutMs)
{
  return timers.Lock(StateKey, false, TimeoutMs) ? &timers : NULL;
}

cTimers *cTimers::GetTimersWrite(cStateKey &StateKey, int TimeoutMs)
{
  return timers.Lock(StateKey, true, TimeoutMs) ? &timers : NULL;
}

void cTimers::Add(cTimer *Timer, cTimer *After)
{
  if (!Timer->Remote())
     Timer->SetId(NewTimerId());
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

const cTimer *cTimers::UsesChannel(const cChannel *Channel) const
{
  for (const cTimer *Timer = First(); Timer; Timer = Next(Timer)) {
      if (Timer->Channel() == Channel)
         return Timer;
      }
  return NULL;
}

bool cTimers::SetEvents(const cSchedules *Schedules)
{
  bool TimersModified = false;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (!ti->IsPatternTimer())
         TimersModified |= ti->SetEventFromSchedule(Schedules);
      }
  return TimersModified;
}

bool cTimers::SpawnPatternTimers(const cSchedules *Schedules)
{
  bool TimersModified = false;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->IsPatternTimer() && ti->Local()) {
         if (ti->HasFlags(tfActive))
            TimersModified |= ti->SpawnPatternTimers(Schedules, this);
         }
      }
  return TimersModified;
}

bool cTimers::DeleteExpired(void)
{
  if (time(NULL) - lastDeleteExpired < 30)
     return false;
  bool TimersModified = false;
  cTimer *ti = First();
  while (ti) {
        cTimer *next = Next(ti);
        if (!ti->Remote() && ti->Expired()) {
           ti->SetEvent(NULL); // Del() doesn't call ~cTimer() right away, so this is necessary here
           ti->TriggerRespawn(); // in case this is a spawned timer
           isyslog("deleting timer %s", *ti->ToDescr());
           Del(ti);
           TimersModified = true;
           }
        ti = next;
        }
  lastDeleteExpired = time(NULL);
  return TimersModified;
}

bool cTimers::StoreRemoteTimers(const char *ServerName, const cStringList *RemoteTimers)
{
  bool Result = false;
  if (!ServerName || !RemoteTimers || RemoteTimers->Size() == 0) {
     // Remove remote timers from this list:
     cTimer *Timer = First();
     while (Timer) {
           cTimer *t = Next(Timer);
           if (Timer->Remote() && (!ServerName || strcmp(Timer->Remote(), ServerName) == 0)) {
              Del(Timer);
              Result = true;
              }
           Timer = t;
           }
     return Result;
     }
  // Collect all locally stored remote timers from ServerName:
  cStringList tl;
  for (cTimer *ti = First(); ti; ti = Next(ti)) {
      if (ti->Remote() && strcmp(ti->Remote(), ServerName) == 0)
         tl.Append(strdup(cString::sprintf("%d %s", ti->Id(), *ti->ToText(true))));
      }
  tl.SortNumerically(); // RemoteTimers is also sorted numerically!
  // Compare the two lists and react accordingly:
  int il = 0; // index into the local ("left") list of remote timers
  int ir = 0; // index into the remote ("right") list of timers
  int sl = tl.Size();
  int sr = RemoteTimers->Size();
  for (;;) {
      int AddTimer = 0;
      int DelTimer = 0;
      if (il < sl) { // still have left entries
         int nl = atoi(tl[il]);
         if (ir < sr) { // still have right entries
            // Compare timers:
            int nr = atoi((*RemoteTimers)[ir]);
            if (nl == nr) // same timer id
               AddTimer = DelTimer = nl;
            else if (nl < nr) // left entry not in right list
               DelTimer = nl;
            else // right entry not in left list
               AddTimer = nr;
            }
         else // processed all right entries
            DelTimer = nl;
         }
      else if (ir < sr) { // still have right entries
         AddTimer = atoi((*RemoteTimers)[ir]);
         if (!AddTimer) {
            esyslog("ERROR: %s: error in timer settings: %s", ServerName, (*RemoteTimers)[ir]);
            ir++;
            continue; // let's see if we can process the rest
            }
         }
      else // processed all left and right entries
         break;
      if (AddTimer && DelTimer) {
         if (strcmp(tl[il], (*RemoteTimers)[ir]) != 0) {
            // Overwrite timer:
            char *v = (*RemoteTimers)[ir];
            while (*v && *v != ' ')
                  v++; // skip id
            if (cTimer *l = GetById(DelTimer, ServerName)) {
               cTimer r;
               if (r.Parse(v)) {
                  r.SetRemote(ServerName);
                  r.SetId(AddTimer);
                  *l = r;
                  Result = true;
                  }
               else
                  esyslog("ERROR: %d@%s: error in timer settings: %s", DelTimer, ServerName, v);
               }
            }
         else // identical timer, nothing to do
            ;
         il++;
         ir++;
         }
      else if (AddTimer) {
         char *v = (*RemoteTimers)[ir];
         while (*v && *v != ' ')
               v++; // skip id
         cTimer *Timer = new cTimer;
         if (Timer->Parse(v)) {
            Timer->SetRemote(ServerName);
            Timer->SetId(AddTimer);
            Add(Timer);
            Result = true;
            }
         else {
            esyslog("ERROR: %s: error in timer settings: %s", ServerName, v);
            delete Timer;
            }
         ir++;
         }
      else if (DelTimer) {
         if (cTimer *t = GetById(DelTimer, ServerName)) {
            Del(t);
            Result = true;
            }
         il++;
         }
      else {
         esyslog("ERROR: oops while storing remote timers!");
         break; // let's not get stuck here!
         }
      }
  return Result;
}

static bool RemoteTimerError(const cTimer *Timer, cString *Msg)
{
  if (Msg)
     *Msg = cString::sprintf("%s %d@%s!", tr("Error while accessing remote timer"), Timer->Id(), Timer->Remote());
  return false; // convenience return code
}

bool HandleRemoteTimerModifications(cTimer *NewTimer, cTimer *OldTimer, cString *Msg)
{
  cStringList Response;
  if (!NewTimer) {
     if (OldTimer) { // timer shall be deleted from remote machine
        if (OldTimer->Remote() && OldTimer->Id()) {
           if (!ExecSVDRPCommand(OldTimer->Remote(), cString::sprintf("DELT %d", OldTimer->Id()), &Response) || SVDRPCode(Response[0]) != 250)
              return RemoteTimerError(OldTimer, Msg);
           }
        isyslog("deleted timer %s", *OldTimer->ToDescr());
        }
     }
  else if (!OldTimer || OldTimer->Local() || !OldTimer->Id()) {
     if (NewTimer->Local()) { // timer stays local, nothing to do
        if (OldTimer && OldTimer->Id())
           isyslog("modified timer %s", *NewTimer->ToDescr());
        else
           isyslog("added timer %s", *NewTimer->ToDescr());
        }
     else { // timer is new, or moved from local to remote
        if (!ExecSVDRPCommand(NewTimer->Remote(), cString::sprintf("NEWT %s", *NewTimer->ToText(true)), &Response) || SVDRPCode(Response[0]) != 250)
           return RemoteTimerError(NewTimer, Msg);
        int RemoteId = atoi(SVDRPValue(Response[0]));
        if (RemoteId <= 0)
           return RemoteTimerError(NewTimer, Msg);
        NewTimer->SetId(RemoteId);
        if (OldTimer && OldTimer->Id()) {
           isyslog("moved timer %d to %s", OldTimer->Id(), *NewTimer->ToDescr());
           }
        else
           isyslog("added timer %s", *NewTimer->ToDescr());
        }
     }
  else if (NewTimer->Local()) { // timer is moved from remote to local
     if (!ExecSVDRPCommand(OldTimer->Remote(), cString::sprintf("DELT %d", OldTimer->Id()), &Response) || SVDRPCode(Response[0]) != 250)
        return RemoteTimerError(OldTimer, Msg);
     NewTimer->SetId(cTimers::NewTimerId());
     NewTimer->ClrFlags(tfRecording); // in case it was recording on the remote machine
     isyslog("moved timer %d@%s to %s", OldTimer->Id(), OldTimer->Remote(), *NewTimer->ToDescr());
     }
  else if (strcmp(OldTimer->Remote(), NewTimer->Remote()) == 0) { // timer stays remote on same machine
     if (!ExecSVDRPCommand(OldTimer->Remote(), cString::sprintf("MODT %d %s", OldTimer->Id(), *NewTimer->ToText(true)), &Response) || SVDRPCode(Response[0]) != 250)
        return RemoteTimerError(NewTimer, Msg);
     isyslog("modified timer %s", *NewTimer->ToDescr());
     }
  else { // timer is moved from one remote machine to an other
     if (!ExecSVDRPCommand(NewTimer->Remote(), cString::sprintf("NEWT %s", *NewTimer->ToText(true)), &Response) || SVDRPCode(Response[0]) != 250)
        return RemoteTimerError(NewTimer, Msg);
     int RemoteId = atoi(SVDRPValue(Response[0]));
     if (RemoteId <= 0)
        return RemoteTimerError(NewTimer, Msg);
     NewTimer->SetId(RemoteId);
     if (!ExecSVDRPCommand(OldTimer->Remote(), cString::sprintf("DELT %d", OldTimer->Id()), &Response) || SVDRPCode(Response[0]) != 250)
        return RemoteTimerError(OldTimer, Msg);
     isyslog("moved timer %d@%s to %s", OldTimer->Id(), OldTimer->Remote(), *NewTimer->ToDescr());
     }
  return true;
}

// --- cSortedTimers ---------------------------------------------------------

static int CompareTimers(const void *a, const void *b)
{
  return (*(const cTimer **)a)->Compare(**(const cTimer **)b);
}

cSortedTimers::cSortedTimers(const cTimers *Timers)
:cVector<const cTimer *>(Timers->Count())
{
  for (const cTimer *Timer = Timers->First(); Timer; Timer = Timers->Next(Timer))
      Append(Timer);
  Sort(CompareTimers);
}
