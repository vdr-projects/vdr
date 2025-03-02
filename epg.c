/*
 * epg.c: Electronic Program Guide
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version (as used in VDR before 1.3.0) written by
 * Robert Schneider <Robert.Schneider@web.de> and Rolf Hakenes <hakenes@hippomi.de>.
 *
 * $Id: epg.c 5.14 2025/03/02 11:03:35 kls Exp $
 */

#include "epg.h"
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "libsi/si.h"

#define RUNNINGSTATUSTIMEOUT 30 // seconds before the running status is considered unknown
#define EPGDATAWRITEDELTA   600 // seconds between writing the epg.data file

// --- tComponent ------------------------------------------------------------

cString tComponent::ToString(void)
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "%X %02X %s %s", stream, type, language, description ? description : "");
  return buffer;
}

bool tComponent::FromString(const char *s)
{
  unsigned int Stream, Type;
  int n = sscanf(s, "%X %02X %7s %m[^\n]", &Stream, &Type, language, &description); // 7 = MAXLANGCODE2 - 1
  if (n != 4 || isempty(description)) {
     free(description);
     description = NULL;
     }
  stream = Stream;
  type = Type;
  return n >= 3;
}

// --- cComponents -----------------------------------------------------------

cComponents::cComponents(void)
{
  numComponents = 0;
  components = NULL;
}

cComponents::~cComponents(void)
{
  for (int i = 0; i < numComponents; i++)
      free(components[i].description);
  free(components);
}

bool cComponents::Realloc(int Index)
{
  if (Index >= numComponents) {
     Index++;
     if (tComponent *NewBuffer = (tComponent *)realloc(components, Index * sizeof(tComponent))) {
        int n = numComponents;
        numComponents = Index;
        components = NewBuffer;
        memset(&components[n], 0, sizeof(tComponent) * (numComponents - n));
        }
     else {
        esyslog("ERROR: out of memory");
        return false;
        }
     }
  return true;
}

void cComponents::SetComponent(int Index, const char *s)
{
  if (Realloc(Index))
     components[Index].FromString(s);
}

void cComponents::SetComponent(int Index, uchar Stream, uchar Type, const char *Language, const char *Description)
{
  if (!Realloc(Index))
     return;
  tComponent *p = &components[Index];
  p->stream = Stream;
  p->type = Type;
  strn0cpy(p->language, Language, sizeof(p->language));
  char *q = strchr(p->language, ',');
  if (q)
     *q = 0; // strips rest of "normalized" language codes
  p->description = strcpyrealloc(p->description, !isempty(Description) ? Description : NULL);
}

tComponent *cComponents::GetComponent(int Index, uchar Stream, uchar Type)
{
  for (int i = 0; i < numComponents; i++) {
      if (components[i].stream == Stream && (
          Type == 0 || // don't care about the actual Type
          Stream == 2 && (components[i].type < 5) == (Type < 5) // fallback "Dolby" component according to the "Premiere pseudo standard"
         )) {
         if (!Index--)
            return &components[i];
         }
      }
  return NULL;
}

// --- cEvent ----------------------------------------------------------------

cMutex cEvent::numTimersMutex;

cEvent::cEvent(tEventID EventID)
{
  schedule = NULL;
  numTimers = 0;
  eventID = EventID;
  tableID = 0xFF; // actual table ids are 0x4E..0x60
  version = 0xFF; // actual version numbers are 0..31
  runningStatus = SI::RunningStatusUndefined;
  title = NULL;
  shortText = NULL;
  description = NULL;
  components = NULL;
  memset(contents, 0, sizeof(contents));
  parentalRating = 0;
  startTime = 0;
  duration = 0;
  vps = 0;
  aux = NULL;
  SetSeen();
}

cEvent::~cEvent()
{
  free(title);
  free(shortText);
  free(description);
  free(aux);
  delete components;
}

int cEvent::Compare(const cListObject &ListObject) const
{
  cEvent *e = (cEvent *)&ListObject;
  int d = startTime - e->startTime;
  if (d == 0)
     d = int(tableID) - int(e->tableID);
  return d;
}

tChannelID cEvent::ChannelID(void) const
{
  return schedule ? schedule->ChannelID() : tChannelID();
}

void cEvent::SetEventID(tEventID EventID)
{
  if (eventID != EventID) {
     if (schedule)
        schedule->UnhashEvent(this);
     eventID = EventID;
     if (schedule)
        schedule->HashEvent(this);
     }
}

void cEvent::SetTableID(uchar TableID)
{
  tableID = TableID;
}

void cEvent::SetVersion(uchar Version)
{
  version = Version;
}

void cEvent::SetRunningStatus(int RunningStatus, const cChannel *Channel)
{
  if (Channel && runningStatus != RunningStatus && (RunningStatus > SI::RunningStatusNotRunning || runningStatus > SI::RunningStatusUndefined) && schedule && schedule->HasTimer())
     isyslog("channel %d (%s) event %s status %d->%d", Channel->Number(), Channel->Name(), *ToDescr(), runningStatus, RunningStatus);
  runningStatus = RunningStatus;
}

void cEvent::SetTitle(const char *Title)
{
  title = strcpyrealloc(title, Title);
}

void cEvent::SetShortText(const char *ShortText)
{
  shortText = strcpyrealloc(shortText, ShortText);
}

void cEvent::SetDescription(const char *Description)
{
  description = strcpyrealloc(description, Description);
}

void cEvent::SetComponents(cComponents *Components)
{
  delete components;
  components = Components;
}

void cEvent::SetContents(uchar *Contents)
{
  for (int i = 0; i < MaxEventContents; i++)
      contents[i] = Contents[i];
}

void cEvent::SetParentalRating(int ParentalRating)
{
  parentalRating = ParentalRating;
}

void cEvent::SetStartTime(time_t StartTime)
{
  if (startTime != StartTime) {
     if (schedule)
        schedule->UnhashEvent(this);
     startTime = StartTime;
     if (schedule)
        schedule->HashEvent(this);
     }
}

void cEvent::SetDuration(int Duration)
{
  duration = Duration;
}

void cEvent::SetVps(time_t Vps)
{
  vps = Vps;
}

void cEvent::SetSeen(void)
{
  seen = time(NULL);
}

void cEvent::SetAux(const char *Aux)
{
  free(aux);
  aux = Aux ? strdup(Aux) : NULL;
}

cString cEvent::ToDescr(void) const
{
  char vpsbuf[64] = "";
  if (Vps())
     sprintf(vpsbuf, "(VPS: %s) ", *GetVpsString());
  return cString::sprintf("%s %s-%s %s'%s'", *GetDateString(), *GetTimeString(), *GetEndTimeString(), vpsbuf, Title());
}

void cEvent::IncNumTimers(void) const
{
  numTimersMutex.Lock();
  numTimers++;
  if (schedule)
     schedule->IncNumTimers();
  numTimersMutex.Unlock();
}

void cEvent::DecNumTimers(void) const
{
  numTimersMutex.Lock();
  numTimers--;
  if (schedule)
     schedule->DecNumTimers();
  numTimersMutex.Unlock();
}

bool cEvent::IsRunning(bool OrAboutToStart) const
{
  return runningStatus >= (OrAboutToStart ? SI::RunningStatusStartsInAFewSeconds : SI::RunningStatusPausing);
}

const char *cEvent::ContentToString(uchar Content)
{
  switch (Content & 0xF0) {
    case ecgMovieDrama:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Movie/Drama");
           case 0x01: return tr("Content$Detective/Thriller");
           case 0x02: return tr("Content$Adventure/Western/War");
           case 0x03: return tr("Content$Science Fiction/Fantasy/Horror");
           case 0x04: return tr("Content$Comedy");
           case 0x05: return tr("Content$Soap/Melodrama/Folkloric");
           case 0x06: return tr("Content$Romance");
           case 0x07: return tr("Content$Serious/Classical/Religious/Historical Movie/Drama");
           case 0x08: return tr("Content$Adult Movie/Drama");
           }
         break;
    case ecgNewsCurrentAffairs:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$News/Current Affairs");
           case 0x01: return tr("Content$News/Weather Report");
           case 0x02: return tr("Content$News Magazine");
           case 0x03: return tr("Content$Documentary");
           case 0x04: return tr("Content$Discussion/Interview/Debate");
           }
         break;
    case ecgShow:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Show/Game Show");
           case 0x01: return tr("Content$Game Show/Quiz/Contest");
           case 0x02: return tr("Content$Variety Show");
           case 0x03: return tr("Content$Talk Show");
           }
         break;
    case ecgSports:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Sports");
           case 0x01: return tr("Content$Special Event");
           case 0x02: return tr("Content$Sport Magazine");
           case 0x03: return tr("Content$Football/Soccer");
           case 0x04: return tr("Content$Tennis/Squash");
           case 0x05: return tr("Content$Team Sports");
           case 0x06: return tr("Content$Athletics");
           case 0x07: return tr("Content$Motor Sport");
           case 0x08: return tr("Content$Water Sport");
           case 0x09: return tr("Content$Winter Sports");
           case 0x0A: return tr("Content$Equestrian");
           case 0x0B: return tr("Content$Martial Sports");
           }
         break;
    case ecgChildrenYouth:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Children's/Youth Programme");
           case 0x01: return tr("Content$Pre-school Children's Programme");
           case 0x02: return tr("Content$Entertainment Programme for 6 to 14");
           case 0x03: return tr("Content$Entertainment Programme for 10 to 16");
           case 0x04: return tr("Content$Informational/Educational/School Programme");
           case 0x05: return tr("Content$Cartoons/Puppets");
           }
         break;
    case ecgMusicBalletDance:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Music/Ballet/Dance");
           case 0x01: return tr("Content$Rock/Pop");
           case 0x02: return tr("Content$Serious/Classical Music");
           case 0x03: return tr("Content$Folk/Traditional Music");
           case 0x04: return tr("Content$Jazz");
           case 0x05: return tr("Content$Musical/Opera");
           case 0x06: return tr("Content$Ballet");
           }
         break;
    case ecgArtsCulture:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Arts/Culture");
           case 0x01: return tr("Content$Performing Arts");
           case 0x02: return tr("Content$Fine Arts");
           case 0x03: return tr("Content$Religion");
           case 0x04: return tr("Content$Popular Culture/Traditional Arts");
           case 0x05: return tr("Content$Literature");
           case 0x06: return tr("Content$Film/Cinema");
           case 0x07: return tr("Content$Experimental Film/Video");
           case 0x08: return tr("Content$Broadcasting/Press");
           case 0x09: return tr("Content$New Media");
           case 0x0A: return tr("Content$Arts/Culture Magazine");
           case 0x0B: return tr("Content$Fashion");
           }
         break;
    case ecgSocialPoliticalEconomics:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Social/Political/Economics");
           case 0x01: return tr("Content$Magazine/Report/Documentary");
           case 0x02: return tr("Content$Economics/Social Advisory");
           case 0x03: return tr("Content$Remarkable People");
           }
         break;
    case ecgEducationalScience:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Education/Science/Factual");
           case 0x01: return tr("Content$Nature/Animals/Environment");
           case 0x02: return tr("Content$Technology/Natural Sciences");
           case 0x03: return tr("Content$Medicine/Physiology/Psychology");
           case 0x04: return tr("Content$Foreign Countries/Expeditions");
           case 0x05: return tr("Content$Social/Spiritual Sciences");
           case 0x06: return tr("Content$Further Education");
           case 0x07: return tr("Content$Languages");
           }
         break;
    case ecgLeisureHobbies:
         switch (Content & 0x0F) {
           default:
           case 0x00: return tr("Content$Leisure/Hobbies");
           case 0x01: return tr("Content$Tourism/Travel");
           case 0x02: return tr("Content$Handicraft");
           case 0x03: return tr("Content$Motoring");
           case 0x04: return tr("Content$Fitness & Health");
           case 0x05: return tr("Content$Cooking");
           case 0x06: return tr("Content$Advertisement/Shopping");
           case 0x07: return tr("Content$Gardening");
           }
         break;
    case ecgSpecial:
         switch (Content & 0x0F) {
           case 0x00: return tr("Content$Original Language");
           case 0x01: return tr("Content$Black & White");
           case 0x02: return tr("Content$Unpublished");
           case 0x03: return tr("Content$Live Broadcast");
           default: ;
           }
         break;
    default: ;
    }
  return "";
}

cString cEvent::GetParentalRatingString(void) const
{
  if (parentalRating)
     return cString::sprintf(tr("ParentalRating$from %d"), parentalRating);
  return NULL;
}

cString cEvent::GetDateString(void) const
{
  return DateString(startTime);
}

cString cEvent::GetTimeString(void) const
{
  return TimeString(startTime);
}

cString cEvent::GetEndTimeString(void) const
{
  return TimeString(startTime + duration);
}

cString cEvent::GetVpsString(void) const
{
  char buf[25];
  struct tm tm_r;
  strftime(buf, sizeof(buf), "%d.%m. %R", localtime_r(&vps, &tm_r));
  return buf;
}

void cEvent::Dump(FILE *f, const char *Prefix, bool InfoOnly) const
{
  if (InfoOnly || startTime + duration + EPG_LINGER_TIME >= time(NULL)) {
     fprintf(f, "%sE %u %jd %d %X %X\n", Prefix, eventID, intmax_t(startTime), duration, tableID, version);
     if (!isempty(title))
        fprintf(f, "%sT %s\n", Prefix, title);
     if (!isempty(shortText))
        fprintf(f, "%sS %s\n", Prefix, shortText);
     if (!isempty(description)) {
        strreplace(description, '\n', '|');
        fprintf(f, "%sD %s\n", Prefix, description);
        strreplace(description, '|', '\n');
        }
     if (contents[0]) {
        fprintf(f, "%sG", Prefix);
        for (int i = 0; Contents(i); i++)
            fprintf(f, " %02X", Contents(i));
        fprintf(f, "\n");
        }
     if (parentalRating)
        fprintf(f, "%sR %d\n", Prefix, parentalRating);
     if (components) {
        for (int i = 0; i < components->NumComponents(); i++) {
            tComponent *p = components->Component(i);
            fprintf(f, "%sX %s\n", Prefix, *p->ToString());
            }
        }
     if (vps)
        fprintf(f, "%sV %jd\n", Prefix, intmax_t(vps));
     if (!InfoOnly && !isempty(aux)) {
        strreplace(aux, '\n', '|');
        fprintf(f, "%s@ %s\n", Prefix, aux);
        strreplace(aux, '|', '\n');
        }
     if (!InfoOnly)
        fprintf(f, "%se\n", Prefix);
     }
}

bool cEvent::Parse(char *s)
{
  char *t = skipspace(s + 1);
  switch (*s) {
    case 'T': SetTitle(t);
              break;
    case 'S': SetShortText(t);
              break;
    case 'D': strreplace(t, '|', '\n');
              SetDescription(t);
              break;
    case 'G': {
                memset(contents, 0, sizeof(contents));
                for (int i = 0; i < MaxEventContents; i++) {
                    char *tail = NULL;
                    int c = strtol(t, &tail, 16);
                    if (0x00 < c && c <= 0xFF) {
                       contents[i] = c;
                       t = tail;
                       }
                    else
                       break;
                    }
              }
              break;
    case 'R': SetParentalRating(atoi(t));
              break;
    case 'X': if (!components)
                 components = new cComponents;
              components->SetComponent(components->NumComponents(), t);
              break;
    case 'V': SetVps(atol(t));
              break;
    case '@': strreplace(t, '|', '\n');
              SetAux(t);
              break;
    default:  esyslog("ERROR: unexpected tag while reading EPG data: %s", s);
              return false;
    }
  return true;
}

bool cEvent::Read(FILE *f, cSchedule *Schedule, int &Line)
{
  if (Schedule) {
     cEvent *Event = NULL;
     char *s;
     cReadLine ReadLine;
     while ((s = ReadLine.Read(f)) != NULL) {
           Line++;
           char *t = skipspace(s + 1);
           switch (*s) {
             case 'E': if (!Event) {
                          unsigned int EventID;
                          intmax_t StartTime; // actually time_t, but intmax_t for scanning with "%jd"
                          int Duration;
                          unsigned int TableID = 0;
                          unsigned int Version = 0xFF; // actual value is ignored
                          int n = sscanf(t, "%u %jd %d %X %X", &EventID, &StartTime, &Duration, &TableID, &Version);
                          if (n >= 3 && n <= 5) {
                             Event = (cEvent *)Schedule->GetEventByTime(StartTime);
                             cEvent *newEvent = NULL;
                             if (Event)
                                DELETENULL(Event->components);
                             if (!Event) {
                                Event = newEvent = new cEvent(EventID);
                                Event->seen = 0;
                                }
                             if (Event) {
                                Event->SetTableID(TableID);
                                Event->SetStartTime(StartTime);
                                Event->SetDuration(Duration);
                                if (newEvent)
                                   Schedule->AddEvent(newEvent);
                                }
                             }
                          }
                       break;
             case 'e': if (Event && !Event->Title())
                          Event->SetTitle(tr("No title"));
                       Event = NULL;
                       break;
             case 'c': // to keep things simple we react on 'c' here
                       return true;
             default:  if (Event && !Event->Parse(s)) {
                          esyslog("ERROR: EPG data problem in line %d", Line);
                          return false;
                          }
             }
           }
     esyslog("ERROR: unexpected end of file while reading EPG data");
     }
  return false;
}

#define MAXEPGBUGFIXSTATS 13
#define MAXEPGBUGFIXCHANS 100
struct tEpgBugFixStats {
  int hits;
  int n;
  tChannelID channelIDs[MAXEPGBUGFIXCHANS];
  tEpgBugFixStats(void) { hits = n = 0; }
  };

tEpgBugFixStats EpgBugFixStats[MAXEPGBUGFIXSTATS];

static void EpgBugFixStat(int Number, tChannelID ChannelID)
{
  if (0 <= Number && Number < MAXEPGBUGFIXSTATS) {
     tEpgBugFixStats *p = &EpgBugFixStats[Number];
     p->hits++;
     int i = 0;
     for (; i < p->n; i++) {
         if (p->channelIDs[i] == ChannelID)
            break;
         }
     if (i == p->n && p->n < MAXEPGBUGFIXCHANS)
        p->channelIDs[p->n++] = ChannelID;
     }
}

void ReportEpgBugFixStats(bool Force)
{
  if (Setup.EPGBugfixLevel > 0) {
     static time_t LastReport = 0;
     time_t now = time(NULL);
     if (now - LastReport > 3600 || Force) {
        LastReport = now;
        struct tm tm_r;
        struct tm *ptm = localtime_r(&now, &tm_r);
        if (ptm->tm_hour != 5)
           return;
        }
     else
        return;
     bool GotHits = false;
     char buffer[1024];
     for (int i = 0; i < MAXEPGBUGFIXSTATS; i++) {
         const char *delim = " ";
         tEpgBugFixStats *p = &EpgBugFixStats[i];
         if (p->hits) {
            bool PrintedStats = false;
            char *q = buffer;
            *buffer = 0;
            LOCK_CHANNELS_READ;
            for (int c = 0; c < p->n; c++) {
                if (const cChannel *Channel = Channels->GetByChannelID(p->channelIDs[c], true)) {
                   if (!GotHits) {
                      dsyslog("=====================");
                      dsyslog("EPG bugfix statistics");
                      dsyslog("=====================");
                      dsyslog("IF SOMEBODY WHO IS IN CHARGE OF THE EPG DATA FOR ONE OF THE LISTED");
                      dsyslog("CHANNELS READS THIS: PLEASE TAKE A LOOK AT THE FUNCTION cEvent::FixEpgBugs()");
                      dsyslog("IN VDR/epg.c TO LEARN WHAT'S WRONG WITH YOUR DATA, AND FIX IT!");
                      dsyslog("=====================");
                      dsyslog("Fix Hits Channels");
                      GotHits = true;
                      }
                   if (!PrintedStats) {
                      q += snprintf(q, sizeof(buffer) - (q - buffer), "%-3d %-4d", i, p->hits);
                      PrintedStats = true;
                      }
                   q += snprintf(q, sizeof(buffer) - (q - buffer), "%s%s", delim, Channel->Name());
                   delim = ", ";
                   if (q - buffer > 80) {
                      q += snprintf(q, sizeof(buffer) - (q - buffer), "%s...", delim);
                      break;
                      }
                   }
                }
            if (*buffer)
               dsyslog("%s", buffer);
            }
         p->hits = p->n = 0;
         }
     if (GotHits)
        dsyslog("=====================");
     }
}

static void StripControlCharacters(char *s)
{
  if (s) {
     int len = strlen(s);
     while (len > 0) {
           int l = Utf8CharLen(s);
           uchar *p = (uchar *)s;
           if (l == 2 && *p == 0xC2) // UTF-8 sequence
              p++;
           if (*p == 0x86 || *p == 0x87 || *p == 0x0D) {
              memmove(s, p + 1, len - l + 1); // we also copy the terminating 0!
              len -= l;
              l = 0;
              }
           s += l;
           len -= l;
           }
     }
}

void cEvent::FixEpgBugs(void)
{
  if (isempty(title)) {
     // we don't want any "(null)" titles
     title = strcpyrealloc(title, tr("No title"));
     EpgBugFixStat(12, ChannelID());
     }

  if (Setup.EPGBugfixLevel == 0)
     goto Final;

  // Some TV stations apparently have their own idea about how to fill in the
  // EPG data. Let's fix their bugs as good as we can:

  // Some channels put the ShortText in quotes and use either the ShortText
  // or the Description field, depending on how long the string is:
  //
  // Title
  // "ShortText". Description
  //
  if ((shortText == NULL) != (description == NULL)) {
     char *p = shortText ? shortText : description;
     if (*p == '"') {
        const char *delim = "\".";
        char *e = strstr(p + 1, delim);
        if (e) {
           *e = 0;
           char *s = strdup(p + 1);
           char *d = strdup(e + strlen(delim));
           free(shortText);
           free(description);
           shortText = s;
           description = d;
           EpgBugFixStat(1, ChannelID());
           }
        }
     }

  // Some channels put the Description into the ShortText (preceded
  // by a blank) if there is no actual ShortText and the Description
  // is short enough:
  //
  // Title
  //  Description
  //
  if (shortText && !description) {
     if (*shortText == ' ') {
        memmove(shortText, shortText + 1, strlen(shortText));
        description = shortText;
        shortText = NULL;
        EpgBugFixStat(2, ChannelID());
        }
     }

  // Sometimes they repeat the Title in the ShortText:
  //
  // Title
  // Title
  //
  if (shortText && strcmp(title, shortText) == 0) {
     free(shortText);
     shortText = NULL;
     EpgBugFixStat(3, ChannelID());
     }

  // Some channels put the ShortText between double quotes, which is nothing
  // but annoying (some even put a '.' after the closing '"'):
  //
  // Title
  // "ShortText"[.]
  //
  if (shortText && *shortText == '"') {
     int l = strlen(shortText);
     if (l > 2 && (shortText[l - 1] == '"' || (shortText[l - 1] == '.' && shortText[l - 2] == '"'))) {
        memmove(shortText, shortText + 1, l);
        char *p = strrchr(shortText, '"');
        if (p)
           *p = 0;
        EpgBugFixStat(4, ChannelID());
        }
     }

  if (Setup.EPGBugfixLevel <= 1)
     goto Final;

  // Some channels apparently try to do some formatting in the texts,
  // which is a bad idea because they have no way of knowing the width
  // of the window that will actually display the text.
  // Remove excess whitespace:
  title = compactspace(title);
  shortText = compactspace(shortText);
  description = compactspace(description);

#define MAX_USEFUL_EPISODE_LENGTH 40
  // Some channels put a whole lot of information in the ShortText and leave
  // the Description totally empty. So if the ShortText length exceeds
  // MAX_USEFUL_EPISODE_LENGTH, let's put this into the Description
  // instead:
  if (!isempty(shortText) && isempty(description)) {
     if (strlen(shortText) > MAX_USEFUL_EPISODE_LENGTH) {
        free(description);
        description = shortText;
        shortText = NULL;
        EpgBugFixStat(6, ChannelID());
        }
     }

  // Some channels put the same information into ShortText and Description.
  // In that case we delete one of them:
  if (shortText && description && strcmp(shortText, description) == 0) {
     if (strlen(shortText) > MAX_USEFUL_EPISODE_LENGTH) {
        free(shortText);
        shortText = NULL;
        }
     else {
        free(description);
        description = NULL;
        }
     EpgBugFixStat(7, ChannelID());
     }

  // Some channels use the ` ("backtick") character, where a ' (single quote)
  // would be normally used. Actually, "backticks" in normal text don't make
  // much sense, so let's replace them:
  strreplace(title, '`', '\'');
  strreplace(shortText, '`', '\'');
  strreplace(description, '`', '\'');

  if (Setup.EPGBugfixLevel <= 2)
     goto Final;

  // The stream components have a "description" field which some channels
  // apparently have no idea of how to set correctly:
  if (components) {
     for (int i = 0; i < components->NumComponents(); i++) {
         tComponent *p = components->Component(i);
         switch (p->stream) {
           case 0x01: { // video
                if (p->description) {
                   if (strcasecmp(p->description, "Video") == 0 ||
                        strcasecmp(p->description, "Bildformat") == 0) {
                      // Yes, we know it's video - that's what the 'stream' code
                      // is for! But _which_ video is it?
                      free(p->description);
                      p->description = NULL;
                      EpgBugFixStat(8, ChannelID());
                      }
                   }
                if (!p->description) {
                   switch (p->type) {
                     case 0x01:
                     case 0x05: p->description = strdup("4:3"); break;
                     case 0x02:
                     case 0x03:
                     case 0x06:
                     case 0x07: p->description = strdup("16:9"); break;
                     case 0x04:
                     case 0x08: p->description = strdup(">16:9"); break;
                     case 0x09:
                     case 0x0D: p->description = strdup("HD 4:3"); break;
                     case 0x0A:
                     case 0x0B:
                     case 0x0E:
                     case 0x0F: p->description = strdup("HD 16:9"); break;
                     case 0x0C:
                     case 0x10: p->description = strdup("HD >16:9"); break;
                     default: ;
                     }
                   EpgBugFixStat(9, ChannelID());
                   }
                }
                break;
           case 0x02: { // audio
                if (p->description) {
                   if (strcasecmp(p->description, "Audio") == 0) {
                      // Yes, we know it's audio - that's what the 'stream' code
                      // is for! But _which_ audio is it?
                      free(p->description);
                      p->description = NULL;
                      EpgBugFixStat(10, ChannelID());
                      }
                   }
                if (!p->description) {
                   switch (p->type) {
                     case 0x05: p->description = strdup("Dolby Digital"); break;
                     default: ; // all others will just display the language
                     }
                   EpgBugFixStat(11, ChannelID());
                   }
                }
                break;
           default: ;
           }
         }
     }

Final:

  // And then there are the specially gifted people who put a literal "\n" string where there should be
  // a '\n' character:
  if (shortText) {
     if (char *p = strstr(shortText, "\\n")) {
        *p = 0;
        p += 2;
        char *s = strdup(shortText);
        char *d = strdup(cString::sprintf("%s\n\n%s", p, description));
        free(shortText);
        free(description);
        shortText = s;
        description = d;
        EpgBugFixStat(12, ChannelID());
        }
     }
  description = strreplace(description, "\\n", " \n");

  // VDR can't usefully handle newline characters in the title, shortText or component description of EPG
  // data, so let's always convert them to blanks (independent of the setting of EPGBugfixLevel):
  strreplace(title, '\n', ' ');
  strreplace(shortText, '\n', ' ');
  if (components) {
     for (int i = 0; i < components->NumComponents(); i++) {
         tComponent *p = components->Component(i);
         if (p->description)
            strreplace(p->description, '\n', ' ');
         }
     }
  // Same for control characters:
  StripControlCharacters(title);
  StripControlCharacters(shortText);
  StripControlCharacters(description);
}

// --- cSchedule -------------------------------------------------------------

cMutex cSchedule::numTimersMutex;

cSchedule::cSchedule(tChannelID ChannelID)
{
  channelID = ChannelID;
  events.SetUseGarbageCollector();
  numTimers = 0;
  modified = 0;
  onActualTp = false;
  presentSeen = 0;
}

void cSchedule::IncNumTimers(void) const
{
  numTimersMutex.Lock();
  numTimers++;
  numTimersMutex.Unlock();
}

void cSchedule::DecNumTimers(void) const
{
  numTimersMutex.Lock();
  numTimers--;
  numTimersMutex.Unlock();
}

bool cSchedule::OnActualTp(uchar TableId)
{
  if ((TableId & 0xF0) == 0x50)
     onActualTp = true;
  return onActualTp;
}

cEvent *cSchedule::AddEvent(cEvent *Event)
{
  events.Add(Event);
  Event->schedule = this;
  HashEvent(Event);
  return Event;
}

void cSchedule::DelEvent(cEvent *Event)
{
  if (Event->schedule == this) {
     UnhashEvent(Event);
     Event->schedule = NULL;
     // Removing the event from its schedule prevents it from decrementing the
     // schedule's timer counter, so we do it here:
     cEvent::numTimersMutex.Lock();
     numTimersMutex.Lock();
     numTimers -= Event->numTimers;
     numTimersMutex.Unlock();
     cEvent::numTimersMutex.Unlock();
     events.Del(Event);
     }
}

void cSchedule::HashEvent(cEvent *Event)
{
  if (cEvent *p = eventsHashID.Get(Event->EventID()))
     eventsHashID.Del(p, p->EventID());
  eventsHashID.Add(Event, Event->EventID());
  if (Event->StartTime() > 0) { // 'StartTime < 0' is apparently used with NVOD channels
     if (cEvent *p = eventsHashStartTime.Get(Event->StartTime()))
        eventsHashStartTime.Del(p, p->StartTime());
     eventsHashStartTime.Add(Event, Event->StartTime());
     }
}

void cSchedule::UnhashEvent(cEvent *Event)
{
  eventsHashID.Del(Event, Event->EventID());
  if (Event->StartTime() > 0) // 'StartTime < 0' is apparently used with NVOD channels
     eventsHashStartTime.Del(Event, Event->StartTime());
}

const cEvent *cSchedule::GetPresentEvent(void) const
{
  const cEvent *pe = NULL;
  time_t now = time(NULL);
  for (const cEvent *p = events.First(); p; p = events.Next(p)) {
      if (p->StartTime() <= now)
         pe = p;
      else if (p->StartTime() > now + 3600)
         break;
      if (p->SeenWithin(RUNNINGSTATUSTIMEOUT) && p->RunningStatus() >= SI::RunningStatusPausing)
         return p;
      }
  return pe;
}

const cEvent *cSchedule::GetFollowingEvent(void) const
{
  const cEvent *p = GetPresentEvent();
  if (p)
     p = events.Next(p);
  else {
     time_t now = time(NULL);
     for (p = events.First(); p; p = events.Next(p)) {
         if (p->StartTime() >= now)
            break;
         }
     }
  return p;
}

const cEvent *cSchedule::GetEventById(tEventID EventID) const
{
  return eventsHashID.Get(EventID);
}

const cEvent *cSchedule::GetEventByTime(time_t StartTime) const
{
  if (StartTime > 0) // 'StartTime < 0' is apparently used with NVOD channels
     return eventsHashStartTime.Get(StartTime);
  return NULL;
}

const cEvent *cSchedule::GetEventAround(time_t Time) const
{
  const cEvent *pe = NULL;
  time_t delta = INT_MAX;
  for (const cEvent *p = events.First(); p; p = events.Next(p)) {
      time_t dt = Time - p->StartTime();
      if (dt >= 0 && dt < delta && p->EndTime() >= Time) {
         delta = dt;
         pe = p;
         }
      }
  return pe;
}

void cSchedule::SetRunningStatus(cEvent *Event, int RunningStatus, const cChannel *Channel)
{
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      if (p == Event) {
         if (p->RunningStatus() > SI::RunningStatusNotRunning || RunningStatus > SI::RunningStatusNotRunning) {
            p->SetRunningStatus(RunningStatus, Channel);
            break;
            }
         }
      else if (RunningStatus >= SI::RunningStatusPausing && p->StartTime() < Event->StartTime())
         p->SetRunningStatus(SI::RunningStatusNotRunning, Channel);
      }
  SetPresentSeen();
}

void cSchedule::ClrRunningStatus(cChannel *Channel)
{
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      if (p->RunningStatus() >= SI::RunningStatusPausing) {
         p->SetRunningStatus(SI::RunningStatusNotRunning, Channel);
         SetModified();
         break;
         }
      }
}

void cSchedule::ResetVersions(void)
{
  for (cEvent *p = events.First(); p; p = events.Next(p))
      p->SetVersion(0xFF);
}

void cSchedule::Sort(void)
{
  events.Sort();
  SetModified();
}

void cSchedule::DropOutdated(time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{
  // Events are sorted by start time.
  if (SegmentStart > 0 && SegmentEnd > 0) {
     cEvent *p = events.First();
     while (p) {
           cEvent *n = events.Next(p);
           if (p->StartTime() >= SegmentStart) {
              if (p->StartTime() < SegmentEnd) {
                 // The event starts within the given time segment.
                 if ((p->TableID() > 0x4E || TableID == 0x4E) && (p->TableID() != TableID || p->Version() != Version)) {
                    // The segment overwrites all events from tables with other ids, and
                    // within the same table id all events must have the same version.
                    // Special consideration: table 0x4E can only be overwritten with the same id!
                    DelEvent(p);
                    }
                 }
              else
                 break;
              }
           p = n;
           }
     }
  // Make sure there are no two events with the same start time:
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      cEvent *n = events.Next(p);
      if (n && n->StartTime() == p->StartTime())
         DelEvent(n);
      }
  // Make sure there are no RunningStatusUndefined before the currently running event:
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      if (p->RunningStatus() >= SI::RunningStatusPausing) {
         for (p = events.Prev(p); p; p = events.Prev(p))
             p->SetRunningStatus(SI::RunningStatusNotRunning);
         break;
         }
      }
}

void cSchedule::Cleanup(void)
{
  Cleanup(time(NULL));
}

void cSchedule::Cleanup(time_t Time)
{
  cEvent *Event;
  while ((Event = events.First()) != NULL) {
        if (!Event->HasTimer() && Event->EndTime() + EPG_LINGER_TIME < Time)
           DelEvent(Event);
        else
           break;
        }
}

void cSchedule::Dump(const cChannels *Channels, FILE *f, const char *Prefix, eDumpMode DumpMode, time_t AtTime) const
{
  if (const cChannel *Channel = Channels->GetByChannelID(channelID, true)) {
     fprintf(f, "%sC %s %s\n", Prefix, *Channel->GetChannelID().ToString(), Channel->Name());
     const cEvent *p;
     switch (DumpMode) {
       case dmAll: {
            for (p = events.First(); p; p = events.Next(p))
                p->Dump(f, Prefix);
            }
            break;
       case dmPresent: {
            if ((p = GetPresentEvent()) != NULL)
               p->Dump(f, Prefix);
            }
            break;
       case dmFollowing: {
            if ((p = GetFollowingEvent()) != NULL)
               p->Dump(f, Prefix);
            }
            break;
       case dmAtTime: {
            if ((p = GetEventAround(AtTime)) != NULL)
               p->Dump(f, Prefix);
            }
            break;
       default: esyslog("ERROR: unknown DumpMode %d (%s %d)", DumpMode, __FUNCTION__, __LINE__);
       }
     fprintf(f, "%sc\n", Prefix);
     }
}

bool cSchedule::Read(FILE *f, cSchedules *Schedules)
{
  if (Schedules) {
     int Line = 0;
     cReadLine ReadLine;
     char *s;
     while ((s = ReadLine.Read(f)) != NULL) {
           Line++;
           if (*s == 'C') {
              s = skipspace(s + 1);
              char *p = strchr(s, ' ');
              if (p)
                 *p = 0; // strips optional channel name
              if (*s) {
                 tChannelID channelID = tChannelID::FromString(s);
                 if (channelID.Valid()) {
                    if (cSchedule *p = Schedules->AddSchedule(channelID)) {
                       if (!cEvent::Read(f, p, Line))
                          return false;
                       p->Sort();
                       }
                    }
                 else {
                    esyslog("ERROR: invalid channel ID: %s", s);
                    return false;
                    }
                 }
              }
           else {
              esyslog("ERROR: unexpected tag in line %d while reading EPG data: %s", Line, s);
              return false;
              }
           }
     return true;
     }
  return false;
}

// --- cEpgDataWriter --------------------------------------------------------

class cEpgDataWriter : public cThread {
private:
  cMutex mutex;
  bool dump;
protected:
  virtual void Action(void) override;
public:
  cEpgDataWriter(void);
  void SetDump(bool Dump) { dump = Dump; }
  void Perform(void);
  };

cEpgDataWriter::cEpgDataWriter(void)
:cThread("epg data writer", true)
{
  dump = false;
}

void cEpgDataWriter::Action(void)
{
  Perform();
}

void cEpgDataWriter::Perform(void)
{
  cMutexLock MutexLock(&mutex); // to make sure fore- and background calls don't cause parellel dumps!
  {
    cStateKey StateKey;
    if (cSchedules *Schedules = cSchedules::GetSchedulesWrite(StateKey, 1000)) {
       time_t now = time(NULL);
       for (cSchedule *p = Schedules->First(); p; p = Schedules->Next(p))
           p->Cleanup(now);
       StateKey.Remove();
       }
  }
  if (dump)
     cSchedules::Dump();
}

static cEpgDataWriter EpgDataWriter;

// --- cSchedules ------------------------------------------------------------

cSchedules cSchedules::schedules;
char *cSchedules::epgDataFileName = NULL;
time_t cSchedules::lastDump = time(NULL);

cSchedules::cSchedules(void)
:cList<cSchedule>("5 Schedules")
{
}

const cSchedules *cSchedules::GetSchedulesRead(cStateKey &StateKey, int TimeoutMs)
{
  return schedules.Lock(StateKey, false, TimeoutMs) ? &schedules : NULL;
}

cSchedules *cSchedules::GetSchedulesWrite(cStateKey &StateKey, int TimeoutMs)
{
  return schedules.Lock(StateKey, true, TimeoutMs) ? &schedules : NULL;
}

void cSchedules::SetEpgDataFileName(const char *FileName)
{
  free(epgDataFileName);
  epgDataFileName = FileName ? strdup(FileName) : NULL;
  EpgDataWriter.SetDump(epgDataFileName != NULL);
}

void cSchedules::Cleanup(bool Force)
{
  if (Force)
     lastDump = 0;
  time_t now = time(NULL);
  if (now - lastDump > EPGDATAWRITEDELTA) {
     if (Force)
        EpgDataWriter.Perform();
     else if (!EpgDataWriter.Active())
        EpgDataWriter.Start();
     lastDump = now;
     }
}

void cSchedules::ResetVersions(void)
{
  LOCK_SCHEDULES_WRITE;
  for (cSchedule *Schedule = Schedules->First(); Schedule; Schedule = Schedules->Next(Schedule))
      Schedule->ResetVersions();
}

bool cSchedules::Dump(FILE *f, const char *Prefix, eDumpMode DumpMode, time_t AtTime)
{
  cSafeFile *sf = NULL;
  if (!f) {
     sf = new cSafeFile(epgDataFileName);
     if (sf->Open())
        f = *sf;
     else {
        LOG_ERROR;
        delete sf;
        return false;
        }
     }
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_READ;
  for (const cSchedule *p = Schedules->First(); p; p = Schedules->Next(p))
      p->Dump(Channels, f, Prefix, DumpMode, AtTime);
  if (sf) {
     sf->Close();
     delete sf;
     }
  return true;
}

bool cSchedules::Read(FILE *f)
{
  bool OwnFile = f == NULL;
  if (OwnFile) {
     if (epgDataFileName && access(epgDataFileName, R_OK) == 0) {
        dsyslog("reading EPG data from %s", epgDataFileName);
        if ((f = fopen(epgDataFileName, "r")) == NULL) {
           LOG_ERROR;
           return false;
           }
        }
     else
        return false;
     }
  LOCK_CHANNELS_WRITE;
  LOCK_SCHEDULES_WRITE;
  bool result = cSchedule::Read(f, Schedules);
  if (OwnFile)
     fclose(f);
  if (result) {
     // Initialize the channels' schedule pointers, so that the first WhatsOn menu will come up faster:
     for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
         if (const cSchedule *Schedule = Channel->schedule) {
            if (!Schedule->ChannelID().Valid()) // this is the DummySchedule
               Channel->schedule = NULL;
            }
         Schedules->GetSchedule(Channel);
         }
     }
  return result;
}

cSchedule *cSchedules::AddSchedule(tChannelID ChannelID)
{
  ChannelID.ClrRid();
  cSchedule *p = (cSchedule *)GetSchedule(ChannelID);
  if (!p) {
     p = new cSchedule(ChannelID);
     Add(p);
     }
  return p;
}

const cSchedule *cSchedules::GetSchedule(tChannelID ChannelID) const
{
  ChannelID.ClrRid();
  for (const cSchedule *p = First(); p; p = Next(p)) {
      if (p->ChannelID() == ChannelID)
         return p;
      }
  return NULL;
}

const cSchedule *cSchedules::GetSchedule(const cChannel *Channel, bool AddIfMissing) const
{
  // This is not very beautiful, but it dramatically speeds up the
  // "What's on now/next?" menus.
  if (!Channel)
     return NULL;
  static cSchedule DummySchedule(tChannelID::InvalidID);
  if (!Channel->schedule)
     Channel->schedule = GetSchedule(Channel->GetChannelID());
  if (!Channel->schedule)
     Channel->schedule = &DummySchedule;
  if (Channel->schedule == &DummySchedule && AddIfMissing) {
     cSchedule *Schedule = new cSchedule(Channel->GetChannelID());
     ((cSchedules *)this)->Add(Schedule);
     Channel->schedule = Schedule;
     }
  return Channel->schedule != &DummySchedule? Channel->schedule : NULL;
}

// --- cEpgDataReader --------------------------------------------------------

cEpgDataReader::cEpgDataReader(void)
:cThread("epg data reader")
{
}

void cEpgDataReader::Action(void)
{
  cSchedules::Read();
}

// --- cEpgHandler -----------------------------------------------------------

cEpgHandler::cEpgHandler(void)
{
  EpgHandlers.Add(this);
}

cEpgHandler::~cEpgHandler()
{
  EpgHandlers.Del(this, false);
}

// --- cEpgHandlers ----------------------------------------------------------

cEpgHandlers EpgHandlers;

bool cEpgHandlers::IgnoreChannel(const cChannel *Channel)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->IgnoreChannel(Channel))
         return true;
      }
  return false;
}

bool cEpgHandlers::HandleEitEvent(cSchedule *Schedule, const SI::EIT::Event *EitEvent, uchar TableID, uchar Version)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->HandleEitEvent(Schedule, EitEvent, TableID, Version))
         return true;
      }
  return false;
}

bool cEpgHandlers::HandledExternally(const cChannel *Channel)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->HandledExternally(Channel))
         return true;
      }
  return false;
}

bool cEpgHandlers::IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->IsUpdate(EventID, StartTime, TableID, Version))
         return true;
      }
  return false;
}

void cEpgHandlers::SetEventID(cEvent *Event, tEventID EventID)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetEventID(Event, EventID))
         return;
      }
  Event->SetEventID(EventID);
}

void cEpgHandlers::SetTitle(cEvent *Event, const char *Title)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetTitle(Event, Title))
         return;
      }
  Event->SetTitle(Title);
}

void cEpgHandlers::SetShortText(cEvent *Event, const char *ShortText)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetShortText(Event, ShortText))
         return;
      }
  Event->SetShortText(ShortText);
}

void cEpgHandlers::SetDescription(cEvent *Event, const char *Description)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetDescription(Event, Description))
         return;
      }
  Event->SetDescription(Description);
}

void cEpgHandlers::SetContents(cEvent *Event, uchar *Contents)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetContents(Event, Contents))
         return;
      }
  Event->SetContents(Contents);
}

void cEpgHandlers::SetParentalRating(cEvent *Event, int ParentalRating)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetParentalRating(Event, ParentalRating))
         return;
      }
  Event->SetParentalRating(ParentalRating);
}

void cEpgHandlers::SetStartTime(cEvent *Event, time_t StartTime)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetStartTime(Event, StartTime))
         return;
      }
  Event->SetStartTime(StartTime);
}

void cEpgHandlers::SetDuration(cEvent *Event, int Duration)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetDuration(Event, Duration))
         return;
      }
  Event->SetDuration(Duration);
}

void cEpgHandlers::SetVps(cEvent *Event, time_t Vps)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetVps(Event, Vps))
         return;
      }
  Event->SetVps(Vps);
}

void cEpgHandlers::SetComponents(cEvent *Event, cComponents *Components)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SetComponents(Event, Components))
         return;
      }
  Event->SetComponents(Components);
}

void cEpgHandlers::FixEpgBugs(cEvent *Event)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->FixEpgBugs(Event))
         return;
      }
  Event->FixEpgBugs();
}

void cEpgHandlers::HandleEvent(cEvent *Event)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->HandleEvent(Event))
         break;
      }
}

void cEpgHandlers::SortSchedule(cSchedule *Schedule)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->SortSchedule(Schedule))
         return;
      }
  Schedule->Sort();
}

void cEpgHandlers::DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->DropOutdated(Schedule, SegmentStart, SegmentEnd, TableID, Version))
         return;
      }
  Schedule->DropOutdated(SegmentStart, SegmentEnd, TableID, Version);
}

bool cEpgHandlers::BeginSegmentTransfer(const cChannel *Channel)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (!eh->BeginSegmentTransfer(Channel, false))
         return false;
      }
  return true;
}

void cEpgHandlers::EndSegmentTransfer(bool Modified)
{
  for (cEpgHandler *eh = First(); eh; eh = Next(eh)) {
      if (eh->EndSegmentTransfer(Modified, false))
         return;
      }
}
