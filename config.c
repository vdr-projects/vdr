/*
 * config.c: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 1.106 2002/09/28 09:43:41 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include "i18n.h"
#include "interface.h"
#include "plugin.h"
#include "recording.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// -- cChannel ---------------------------------------------------------------

char *cChannel::buffer = NULL;

cChannel::cChannel(void)
{
  *name = 0;
}

cChannel::cChannel(const cChannel *Channel)
{
  strcpy(name,   Channel ? Channel->name         : "Pro7");
  frequency    = Channel ? Channel->frequency    : 12480;
  polarization = Channel ? Channel->polarization : 'v';
  diseqc       = Channel ? Channel->diseqc       : 0;
  srate        = Channel ? Channel->srate        : 27500;
  vpid         = Channel ? Channel->vpid         : 255;
  apid1        = Channel ? Channel->apid1        : 256;
  apid2        = Channel ? Channel->apid2        : 0;
  dpid1        = Channel ? Channel->dpid1        : 257;
  dpid2        = Channel ? Channel->dpid2        : 0;
  tpid         = Channel ? Channel->tpid         : 32;
  ca           = Channel ? Channel->ca           : 0;
  pnr          = Channel ? Channel->pnr          : 0;
  groupSep     = Channel ? Channel->groupSep     : false;
}

const char *cChannel::ToText(cChannel *Channel)
{
  char buf[MaxChannelName * 2];
  char *s = Channel->name;
  if (strchr(s, ':')) {
     s = strcpy(buf, s);
     strreplace(s, ':', '|');
     }
  free(buffer);
  if (Channel->groupSep)
     asprintf(&buffer, ":%s\n", s);
  else {
     char apidbuf[32];
     char *q = apidbuf;
     q += snprintf(q, sizeof(apidbuf), "%d", Channel->apid1);
     if (Channel->apid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->apid2);
     if (Channel->dpid1 || Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ";%d", Channel->dpid1);
     if (Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->dpid2);
     *q = 0;
     asprintf(&buffer, "%s:%d:%c:%d:%d:%d:%s:%d:%d:%d\n", s, Channel->frequency, Channel->polarization, Channel->diseqc, Channel->srate, Channel->vpid, apidbuf, Channel->tpid, Channel->ca, Channel->pnr);
     }
  return buffer;
}

const char *cChannel::ToText(void)
{
  return ToText(this);
}

bool cChannel::Parse(const char *s)
{
  char *buffer = NULL;
  if (*s == ':') {
     if (*++s) {
        strn0cpy(name, s, MaxChannelName);
        groupSep = true;
        number = 0;
        }
     else
        return false;
     }
  else {
     groupSep = false;
     char *apidbuf = NULL;
     int fields = sscanf(s, "%a[^:]:%d :%c:%d :%d :%d :%a[^:]:%d :%d :%d ", &buffer, &frequency, &polarization, &diseqc, &srate, &vpid, &apidbuf, &tpid, &ca, &pnr);
     apid1 = apid2 = 0;
     dpid1 = dpid2 = 0;
     if (apidbuf) {
        char *p = strchr(apidbuf, ';');
        if (p)
           *p++ = 0;
        sscanf(apidbuf, "%d ,%d ", &apid1, &apid2);
        if (p)
           sscanf(p, "%d ,%d ", &dpid1, &dpid2);
        free(apidbuf);
        }
     else
        return false;
     if (fields >= 9) {
        if (fields == 9) {
           // allow reading of old format
           pnr = ca;
           ca = tpid;
           tpid = 0;
           }
        strn0cpy(name, buffer, MaxChannelName);
        free(buffer);
        }
     else
        return false;
     }
  strreplace(name, '|', ':');
  return true;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

// -- cTimer -----------------------------------------------------------------

char *cTimer::buffer = NULL;

cTimer::cTimer(bool Instant)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = Instant ? taActInst : taInactive;
  cChannel *ch = Channels.GetByNumber(cDevice::CurrentChannel());
  channel = ch ? ch->number : 0;
  time_t t = time(NULL);
  struct tm tm_r;
  struct tm *now = localtime_r(&t, &tm_r);
  day = now->tm_mday;
  start = now->tm_hour * 100 + now->tm_min;
  stop = now->tm_hour * 60 + now->tm_min + Setup.InstantRecordTime;
  stop = (stop / 60) * 100 + (stop % 60);
  if (stop >= 2400)
     stop -= 2400;
//TODO VPS???
  priority = Setup.DefaultPriority;
  lifetime = Setup.DefaultLifetime;
  *file = 0;
  firstday = 0;
  summary = NULL;
  if (Instant && ch)
     snprintf(file, sizeof(file), "%s%s", Setup.MarkInstantRecord ? "@" : "", *Setup.NameInstantRecord ? Setup.NameInstantRecord : ch->name);
}

cTimer::cTimer(const cEventInfo *EventInfo)
{
  startTime = stopTime = 0;
  recording = pending = false;
  active = true;
  cChannel *ch = Channels.GetByServiceID(EventInfo->GetServiceID());
  channel = ch ? ch->number : 0;
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

const char *cTimer::ToText(cTimer *Timer)
{
  free(buffer);
  strreplace(Timer->file, ':', '|');
  strreplace(Timer->summary, '\n', '|');
  asprintf(&buffer, "%d:%d:%s:%04d:%04d:%d:%d:%s:%s\n", Timer->active, Timer->channel, PrintDay(Timer->day, Timer->firstday), Timer->start, Timer->stop, Timer->priority, Timer->lifetime, Timer->file, Timer->summary ? Timer->summary : "");
  strreplace(Timer->summary, '|', '\n');
  strreplace(Timer->file, '|', ':');
  return buffer;
}

const char *cTimer::ToText(void)
{
  return ToText(this);
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
  char *buffer1 = NULL;
  char *buffer2 = NULL;
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
  if (8 <= sscanf(s, "%d :%d :%a[^:]:%d :%d :%d :%d :%a[^:\n]:%a[^\n]", &active, &channel, &buffer1, &start, &stop, &priority, &lifetime, &buffer2, &summary)) {
     if (summary && !*skipspace(summary)) {
        free(summary);
        summary = NULL;
        }
     //TODO add more plausibility checks
     day = ParseDay(buffer1, &firstday);
     strn0cpy(file, buffer2, MaxFileName);
     strreplace(file, '|', ':');
     strreplace(summary, '|', '\n');
     free(buffer1);
     free(buffer2);
     free(s2);
     return day != 0;
     }
  free(s2);
  return false;
}

bool cTimer::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
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
  isyslog("timer %d %s", Index() + 1, recording ? "start" : "stop");
}

void cTimer::SetPending(bool Pending)
{
  pending = Pending;
}

void cTimer::Skip(void)
{
  firstday = IncDay(SetTime(StartTime(), 0), 1);
}

// --- cCommand -------------------------------------------------------------

char *cCommand::result = NULL;

cCommand::cCommand(void)
{
  title = command = NULL;
}

cCommand::~cCommand()
{
  free(title);
  free(command);
}

bool cCommand::Parse(const char *s)
{
  const char *p = strchr(s, ':');
  if (p) {
     int l = p - s;
     if (l > 0) {
        title = new char[l + 1];
        strn0cpy(title, s, l + 1);
        if (!isempty(title)) {
           command = stripspace(strdup(skipspace(p + 1)));
           return !isempty(command);
           }
        }
     }
  return false;
}

const char *cCommand::Execute(void)
{
  dsyslog("executing command '%s'", command);
  free(result);
  result = NULL;
  FILE *p = popen(command, "r");
  if (p) {
     int l = 0;
     int c;
     while ((c = fgetc(p)) != EOF) {
           if (l % 20 == 0)
              result = (char *)realloc(result, l + 21);
           result[l++] = c;
           }
     if (result)
        result[l] = 0;
     pclose(p);
     }
  else
     esyslog("ERROR: can't open pipe for command '%s'", command);
  return result;
}

// -- cSVDRPhost -------------------------------------------------------------

cSVDRPhost::cSVDRPhost(void)
{
  addr.s_addr = 0;
  mask = 0;
}

bool cSVDRPhost::Parse(const char *s)
{
  mask = 0xFFFFFFFF;
  const char *p = strchr(s, '/');
  if (p) {
     char *error = NULL;
     int m = strtoul(p + 1, &error, 10);
     if (error && *error && !isspace(*error) || m > 32)
        return false;
     *(char *)p = 0; // yes, we know it's 'const' - will be restored!
     if (m == 0)
        mask = 0;
     else
        mask >>= (32 - m);
     }
  int result = inet_aton(s, &addr);
  if (p)
     *(char *)p = '/'; // there it is again
  return result != 0 && (mask != 0 || addr.s_addr == 0);
}

bool cSVDRPhost::Accepts(in_addr_t Address)
{
  return (Address & mask) == addr.s_addr;
}

// -- cCaDefinition ----------------------------------------------------------

cCaDefinition::cCaDefinition(void)
{
  number = 0;
  description = NULL;
}

cCaDefinition::~cCaDefinition()
{
  free(description);
}

bool cCaDefinition::Parse(const char *s)
{
  return 2 == sscanf(s, "%d %a[^\n]", &number, &description) && description && *description;
}

// -- cCommands --------------------------------------------------------------

cCommands Commands;

// -- cChannels --------------------------------------------------------------

cChannels Channels;

bool cChannels::Load(const char *FileName, bool AllowComments)
{
  if (cConfig<cChannel>::Load(FileName, AllowComments)) {
     ReNumber();
     return true;
     }
  return false;
}

int cChannels::GetNextGroup(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && !channel->groupSep)
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

int cChannels::GetPrevGroup(int Idx)
{
  cChannel *channel = Get(--Idx);
  while (channel && !channel->groupSep)
        channel = Get(--Idx);
  return channel ? Idx : -1;
}

int cChannels::GetNextNormal(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && channel->groupSep)
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

void cChannels::ReNumber( void )
{
  int Number = 0;
  cChannel *ch = (cChannel *)First();
  while (ch) {
        if (!ch->groupSep)
           ch->number = ++Number;
        ch = (cChannel *)ch->Next();
        }
  maxNumber = Number;
}

cChannel *cChannels::GetByNumber(int Number)
{
  cChannel *channel = (cChannel *)First();
  while (channel) {
        if (!channel->groupSep && channel->number == Number)
           return channel;
        channel = (cChannel *)channel->Next();
        }
  return NULL;
}

cChannel *cChannels::GetByServiceID(unsigned short ServiceId)
{
  cChannel *channel = (cChannel *)First();
  while (channel) {
        if (!channel->groupSep && channel->pnr == ServiceId)
           return channel;
        channel = (cChannel *)channel->Next();
        }
  return NULL;
}

bool cChannels::SwitchTo(int Number)
{
  cChannel *channel = GetByNumber(Number);
  return channel && cDevice::PrimaryDevice()->SwitchChannel(channel, true);
}

const char *cChannels::GetChannelNameByNumber(int Number)
{
  cChannel *channel = GetByNumber(Number);
  return channel ? channel->name : NULL;
}

// -- cTimers ----------------------------------------------------------------

cTimers Timers;

cTimer *cTimers::GetTimer(cTimer *Timer)
{
  cTimer *ti = (cTimer *)First();
  while (ti) {
        if (ti->channel == Timer->channel && ti->day == Timer->day && ti->start == Timer->start && ti->stop == Timer->stop)
           return ti;
        ti = (cTimer *)ti->Next();
        }
  return NULL;
}

cTimer *cTimers::GetMatch(time_t t)
{
  cTimer *t0 = NULL;
  cTimer *ti = First();
  while (ti) {
        if (!ti->recording && ti->Matches(t)) {
           if (!t0 || ti->priority > t0->priority)
              t0 = ti;
           }
        ti = (cTimer *)ti->Next();
        }
  return t0;
}

cTimer *cTimers::GetNextActiveTimer(void)
{
  cTimer *t0 = NULL;
  cTimer *ti = First();
  while (ti) {
        if (ti->active && (!t0 || *ti < *t0))
           t0 = ti;
        ti = (cTimer *)ti->Next();
        }
  return t0;
}

// -- cSVDRPhosts ------------------------------------------------------------

cSVDRPhosts SVDRPhosts;

bool cSVDRPhosts::Acceptable(in_addr_t Address)
{
  cSVDRPhost *h = First();
  while (h) {
        if (h->Accepts(Address))
           return true;
        h = (cSVDRPhost *)h->Next();
        }
  return false;
}

// -- cCaDefinitions ---------------------------------------------------------

cCaDefinitions CaDefinitions;

const cCaDefinition *cCaDefinitions::Get(int Number)
{
  cCaDefinition *p = First();
  while (p) {
        if (p->Number() == Number)
           return p;
        p = (cCaDefinition *)p->Next();
        }
  return NULL;
}

// -- cSetupLine -------------------------------------------------------------

cSetupLine::cSetupLine(void)
{
  plugin = name = value = NULL;
}

cSetupLine::cSetupLine(const char *Name, const char *Value, const char *Plugin)
{
  name = strdup(Name);
  value = strdup(Value);
  plugin = Plugin ? strdup(Plugin) : NULL;
}

cSetupLine::~cSetupLine()
{
  free(plugin);
  free(name);
  free(value);
}

bool cSetupLine::operator< (const cListObject &ListObject)
{
  const cSetupLine *sl = (cSetupLine *)&ListObject;
  if (!plugin && !sl->plugin)
     return strcasecmp(name, sl->name) < 0;
  if (!plugin)
     return true;
  if (!sl->plugin)
     return false;
  int result = strcasecmp(plugin, sl->plugin);
  if (result == 0)
     result = strcasecmp(name, sl->name);
  return result < 0;
}

bool cSetupLine::Parse(char *s)
{
  //dsyslog("cSetupLine::Parse '%s'", s);//XXX-
  char *p = strchr(s, '=');
  if (p) {
     *p = 0;
     char *Name  = compactspace(s);
     char *Value = compactspace(p + 1);
     if (*Name && *Value) {
        p = strchr(Name, '.');
        if (p) {
           *p = 0;
           char *Plugin = compactspace(Name);
           Name = compactspace(p + 1);
           if (!(*Plugin && *Name))
              return false;
           plugin = strdup(Plugin);
           }
        name = strdup(Name);
        value = strdup(Value);
        //dsyslog("cSetupLine::Parse '%s' = '%s'", name, value);//XXX-
        return true;
        }
     }
  return false;
}

bool cSetupLine::Save(FILE *f)
{
  //dsyslog("cSetupLine::Save '%s' = '%s'", name, value);//XXX-
  return fprintf(f, "%s%s%s = %s\n", plugin ? plugin : "", plugin ? "." : "", name, value) > 0;
}

// -- cSetup -----------------------------------------------------------------

cSetup Setup;

cSetup::cSetup(void)
{
  OSDLanguage = 0;
  PrimaryDVB = 1;
  ShowInfoOnChSwitch = 1;
  MenuScrollPage = 1;
  MarkInstantRecord = 1;
  strcpy(NameInstantRecord, "TITLE EPISODE");
  InstantRecordTime = 180;
  LnbSLOF    = 11700;
  LnbFrequLo =  9750;
  LnbFrequHi = 10600;
  DiSEqC = 0;
  SetSystemTime = 0;
  TimeTransponder = 0;
  MarginStart = 2;
  MarginStop = 10;
  EPGScanTimeout = 5;
  EPGBugfixLevel = 2;
  SVDRPTimeout = 300;
  SortTimers = 1;
  PrimaryLimit = 0;
  DefaultPriority = 50;
  DefaultLifetime = 50;
  UseSubtitle = 1;
  RecordingDirs = 1;
  VideoFormat = 0;
  RecordDolbyDigital = 1;
  ChannelInfoPos = 0;
  OSDwidth = 52;
  OSDheight = 18;
  OSDMessageTime = 1;
  MaxVideoFileSize = MAXVIDEOFILESIZE;
  SplitEditedFiles = 0;
  MinEventTimeout = 30;
  MinUserInactivity = 120;
  MultiSpeedMode = 0;
  ShowReplayMode = 0;
  memset(CaCaps, sizeof(CaCaps), 0);
  CurrentChannel = -1;
  CurrentVolume = MAXVOLUME;
}

cSetup& cSetup::operator= (const cSetup &s)
{
  memcpy(&__BeginData__, &s.__BeginData__, (char *)&s.__EndData__ - (char *)&s.__BeginData__);
  return *this;
}

cSetupLine *cSetup::Get(const char *Name, const char *Plugin)
{
  for (cSetupLine *l = First(); l; l = Next(l)) {
      if ((l->Plugin() == NULL) == (Plugin == NULL)) {
         if ((!Plugin || strcasecmp(l->Plugin(), Plugin) == 0) && strcasecmp(l->Name(), Name) == 0)
            return l;
         }
      }
  return NULL;
}

void cSetup::Store(const char *Name, const char *Value, const char *Plugin)
{
  if (Name && *Name) {
     cSetupLine *l = Get(Name, Plugin);
     if (l)
        Del(l);
     if (Value)
        Add(new cSetupLine(Name, Value, Plugin));
     }
}

void cSetup::Store(const char *Name, int Value, const char *Plugin)
{
  char *buffer = NULL;
  asprintf(&buffer, "%d", Value);
  Store(Name, buffer, Plugin);
  free(buffer);
}

bool cSetup::Load(const char *FileName)
{
  if (cConfig<cSetupLine>::Load(FileName, true)) {
     bool result = true;
     for (cSetupLine *l = First(); l; l = Next(l)) {
         bool error = false;
         if (l->Plugin()) {
            cPlugin *p = cPluginManager::GetPlugin(l->Plugin());
            if (p && !p->SetupParse(l->Name(), l->Value()))
               error = true;
            }
         else {
            if (!Parse(l->Name(), l->Value()))
               error = true;
            }
         if (error) {
            esyslog("ERROR: unknown config parameter: %s%s%s = %s", l->Plugin() ? l->Plugin() : "", l->Plugin() ? "." : "", l->Name(), l->Value());
            result = false;
            }
         }
     return result;
     }
  return false;
}

void cSetup::StoreCaCaps(const char *Name)
{
  for (int d = 0; d < MAXDEVICES; d++) {
      char buffer[MAXPARSEBUFFER];
      char *q = buffer;
      *buffer = 0;
      for (int i = 0; i < MAXCACAPS; i++) {
          if (CaCaps[d][i]) {
             if (!*buffer)
                q += snprintf(buffer, sizeof(buffer), "%d", d + 1);
             q += snprintf(q, sizeof(buffer) - (q - buffer), " %d", CaCaps[d][i]);
             }
          }
      if (*buffer)
         Store(Name, buffer);
      }
}

bool cSetup::ParseCaCaps(const char *Value)
{
  char *p;
  int d = strtol(Value, &p, 10);
  if (d > 0 && d <= MAXDEVICES) {
     d--;
     int i = 0;
     while (p != Value && p && *p) {
           if (i < MAXCACAPS) {
              int c = strtol(p, &p, 10);
              if (c > 0)
                 CaCaps[d][i++] = c;
              else
                 return false;
              }
           else
              return false;
           }
     return true;
     }
  return false;
}

bool cSetup::Parse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "OSDLanguage"))         OSDLanguage        = atoi(Value);
  else if (!strcasecmp(Name, "PrimaryDVB"))          PrimaryDVB         = atoi(Value);
  else if (!strcasecmp(Name, "ShowInfoOnChSwitch"))  ShowInfoOnChSwitch = atoi(Value);
  else if (!strcasecmp(Name, "MenuScrollPage"))      MenuScrollPage     = atoi(Value);
  else if (!strcasecmp(Name, "MarkInstantRecord"))   MarkInstantRecord  = atoi(Value);
  else if (!strcasecmp(Name, "NameInstantRecord"))   strn0cpy(NameInstantRecord, Value, MaxFileName);
  else if (!strcasecmp(Name, "InstantRecordTime"))   InstantRecordTime  = atoi(Value);
  else if (!strcasecmp(Name, "LnbSLOF"))             LnbSLOF            = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequLo"))          LnbFrequLo         = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequHi"))          LnbFrequHi         = atoi(Value);
  else if (!strcasecmp(Name, "DiSEqC"))              DiSEqC             = atoi(Value);
  else if (!strcasecmp(Name, "SetSystemTime"))       SetSystemTime      = atoi(Value);
  else if (!strcasecmp(Name, "TimeTransponder"))     TimeTransponder    = atoi(Value);
  else if (!strcasecmp(Name, "MarginStart"))         MarginStart        = atoi(Value);
  else if (!strcasecmp(Name, "MarginStop"))          MarginStop         = atoi(Value);
  else if (!strcasecmp(Name, "EPGScanTimeout"))      EPGScanTimeout     = atoi(Value);
  else if (!strcasecmp(Name, "EPGBugfixLevel"))      EPGBugfixLevel     = atoi(Value);
  else if (!strcasecmp(Name, "SVDRPTimeout"))        SVDRPTimeout       = atoi(Value);
  else if (!strcasecmp(Name, "SortTimers"))          SortTimers         = atoi(Value);
  else if (!strcasecmp(Name, "PrimaryLimit"))        PrimaryLimit       = atoi(Value);
  else if (!strcasecmp(Name, "DefaultPriority"))     DefaultPriority    = atoi(Value);
  else if (!strcasecmp(Name, "DefaultLifetime"))     DefaultLifetime    = atoi(Value);
  else if (!strcasecmp(Name, "UseSubtitle"))         UseSubtitle        = atoi(Value);
  else if (!strcasecmp(Name, "RecordingDirs"))       RecordingDirs      = atoi(Value);
  else if (!strcasecmp(Name, "VideoFormat"))         VideoFormat        = atoi(Value);
  else if (!strcasecmp(Name, "RecordDolbyDigital"))  RecordDolbyDigital = atoi(Value);
  else if (!strcasecmp(Name, "ChannelInfoPos"))      ChannelInfoPos     = atoi(Value);
  else if (!strcasecmp(Name, "OSDwidth"))            OSDwidth           = atoi(Value);
  else if (!strcasecmp(Name, "OSDheight"))           OSDheight          = atoi(Value);
  else if (!strcasecmp(Name, "OSDMessageTime"))      OSDMessageTime     = atoi(Value);
  else if (!strcasecmp(Name, "MaxVideoFileSize"))    MaxVideoFileSize   = atoi(Value);
  else if (!strcasecmp(Name, "SplitEditedFiles"))    SplitEditedFiles   = atoi(Value);
  else if (!strcasecmp(Name, "MinEventTimeout"))     MinEventTimeout    = atoi(Value);
  else if (!strcasecmp(Name, "MinUserInactivity"))   MinUserInactivity  = atoi(Value);
  else if (!strcasecmp(Name, "MultiSpeedMode"))      MultiSpeedMode     = atoi(Value);
  else if (!strcasecmp(Name, "ShowReplayMode"))      ShowReplayMode     = atoi(Value);
  else if (!strcasecmp(Name, "CaCaps"))              return ParseCaCaps(Value);
  else if (!strcasecmp(Name, "CurrentChannel"))      CurrentChannel     = atoi(Value);
  else if (!strcasecmp(Name, "CurrentVolume"))       CurrentVolume      = atoi(Value);
  else
     return false;
  return true;
}

bool cSetup::Save(void)
{
  Store("OSDLanguage",        OSDLanguage);
  Store("PrimaryDVB",         PrimaryDVB);
  Store("ShowInfoOnChSwitch", ShowInfoOnChSwitch);
  Store("MenuScrollPage",     MenuScrollPage);
  Store("MarkInstantRecord",  MarkInstantRecord);
  Store("NameInstantRecord",  NameInstantRecord);
  Store("InstantRecordTime",  InstantRecordTime);
  Store("LnbSLOF",            LnbSLOF);
  Store("LnbFrequLo",         LnbFrequLo);
  Store("LnbFrequHi",         LnbFrequHi);
  Store("DiSEqC",             DiSEqC);
  Store("SetSystemTime",      SetSystemTime);
  Store("TimeTransponder",    TimeTransponder);
  Store("MarginStart",        MarginStart);
  Store("MarginStop",         MarginStop);
  Store("EPGScanTimeout",     EPGScanTimeout);
  Store("EPGBugfixLevel",     EPGBugfixLevel);
  Store("SVDRPTimeout",       SVDRPTimeout);
  Store("SortTimers",         SortTimers);
  Store("PrimaryLimit",       PrimaryLimit);
  Store("DefaultPriority",    DefaultPriority);
  Store("DefaultLifetime",    DefaultLifetime);
  Store("UseSubtitle",        UseSubtitle);
  Store("RecordingDirs",      RecordingDirs);
  Store("VideoFormat",        VideoFormat);
  Store("RecordDolbyDigital", RecordDolbyDigital);
  Store("ChannelInfoPos",     ChannelInfoPos);
  Store("OSDwidth",           OSDwidth);
  Store("OSDheight",          OSDheight);
  Store("OSDMessageTime",     OSDMessageTime);
  Store("MaxVideoFileSize",   MaxVideoFileSize);
  Store("SplitEditedFiles",   SplitEditedFiles);
  Store("MinEventTimeout",    MinEventTimeout);
  Store("MinUserInactivity",  MinUserInactivity);
  Store("MultiSpeedMode",     MultiSpeedMode);
  Store("ShowReplayMode",     ShowReplayMode);
  StoreCaCaps("CaCaps");
  Store("CurrentChannel",     CurrentChannel);
  Store("CurrentVolume",      CurrentVolume);

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     isyslog("saved setup to %s", FileName());
     return true;
     }
  return false;
}
