/*
 * epg.c: Electronic Program Guide
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version (as used in VDR before 1.3.0) written by
 * Robert Schneider <Robert.Schneider@web.de> and Rolf Hakenes <hakenes@hippomi.de>.
 *
 * $Id: epg.c 2.12 2012/03/10 13:14:27 kls Exp $
 */

#include "epg.h"
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "libsi/si.h"
#include "timers.h"

#define RUNNINGSTATUSTIMEOUT 30 // seconds before the running status is considered unknown

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
  int n = sscanf(s, "%X %02X %7s %a[^\n]", &Stream, &Type, language, &description); // 7 = MAXLANGCODE2 - 1
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

cEvent::cEvent(tEventID EventID)
{
  schedule = NULL;
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
  SetSeen();
}

cEvent::~cEvent()
{
  free(title);
  free(shortText);
  free(description);
  delete components;
}

int cEvent::Compare(const cListObject &ListObject) const
{
  cEvent *e = (cEvent *)&ListObject;
  return startTime - e->startTime;
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

void cEvent::SetRunningStatus(int RunningStatus, cChannel *Channel)
{
  if (Channel && runningStatus != RunningStatus && (RunningStatus > SI::RunningStatusNotRunning || runningStatus > SI::RunningStatusUndefined) && Channel->HasTimer())
     isyslog("channel %d (%s) event %s status %d", Channel->Number(), Channel->Name(), *ToDescr(), RunningStatus);
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

cString cEvent::ToDescr(void) const
{
  char vpsbuf[64] = "";
  if (Vps())
     sprintf(vpsbuf, "(VPS: %s) ", *GetVpsString());
  return cString::sprintf("%s %s-%s %s'%s'", *GetDateString(), *GetTimeString(), *GetEndTimeString(), vpsbuf, Title());
}

bool cEvent::HasTimer(void) const
{
  for (cTimer *t = Timers.First(); t; t = Timers.Next(t)) {
      if (t->Event() == this)
         return true;
      }
  return false;
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
           case 0x04: return tr("Content$Discussion/Inverview/Debate");
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
           case 0x03: return tr("Content$Folk/Tradional Music");
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
  if (InfoOnly || startTime + duration + Setup.EPGLinger * 60 >= time(NULL)) {
     fprintf(f, "%sE %u %ld %d %X %X\n", Prefix, eventID, startTime, duration, tableID, version);
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
        fprintf(f, "%sV %ld\n", Prefix, vps);
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
    case 'V': SetVps(atoi(t));
              break;
    default:  esyslog("ERROR: unexpected tag while reading EPG data: %s", s);
              return false;
    }
  return true;
}

bool cEvent::Read(FILE *f, cSchedule *Schedule)
{
  if (Schedule) {
     cEvent *Event = NULL;
     char *s;
     int line = 0;
     cReadLine ReadLine;
     while ((s = ReadLine.Read(f)) != NULL) {
           line++;
           char *t = skipspace(s + 1);
           switch (*s) {
             case 'E': if (!Event) {
                          unsigned int EventID;
                          time_t StartTime;
                          int Duration;
                          unsigned int TableID = 0;
                          unsigned int Version = 0xFF; // actual value is ignored
                          int n = sscanf(t, "%u %ld %d %X %X", &EventID, &StartTime, &Duration, &TableID, &Version);
                          if (n >= 3 && n <= 5) {
                             Event = (cEvent *)Schedule->GetEvent(EventID, StartTime);
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
                          esyslog("ERROR: EPG data problem in line %d", line);
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

void ReportEpgBugFixStats(bool Reset)
{
  if (Setup.EPGBugfixLevel > 0) {
     bool GotHits = false;
     char buffer[1024];
     for (int i = 0; i < MAXEPGBUGFIXSTATS; i++) {
         const char *delim = " ";
         tEpgBugFixStats *p = &EpgBugFixStats[i];
         if (p->hits) {
            bool PrintedStats = false;
            char *q = buffer;
            *buffer = 0;
            for (int c = 0; c < p->n; c++) {
                cChannel *channel = Channels.GetByChannelID(p->channelIDs[c], true);
                if (channel) {
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
                   q += snprintf(q, sizeof(buffer) - (q - buffer), "%s%s", delim, channel->Name());
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
         if (Reset)
            p->hits = p->n = 0;
         }
     if (GotHits)
        dsyslog("=====================");
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
  /* TODO adapt to UTF-8
  // Same for control characters:
  strreplace(title, '\x86', ' ');
  strreplace(title, '\x87', ' ');
  strreplace(shortText, '\x86', ' ');
  strreplace(shortText, '\x87', ' ');
  strreplace(description, '\x86', ' ');
  strreplace(description, '\x87', ' ');
  XXX*/
}

// --- cSchedule -------------------------------------------------------------

cSchedule::cSchedule(tChannelID ChannelID)
{
  channelID = ChannelID;
  hasRunning = false;
  modified = 0;
  presentSeen = 0;
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
     if (hasRunning && Event->IsRunning())
        ClrRunningStatus();
     UnhashEvent(Event);
     events.Del(Event);
     }
}

void cSchedule::HashEvent(cEvent *Event)
{
  eventsHashID.Add(Event, Event->EventID());
  if (Event->StartTime() > 0) // 'StartTime < 0' is apparently used with NVOD channels
     eventsHashStartTime.Add(Event, Event->StartTime());
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
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
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

const cEvent *cSchedule::GetEvent(tEventID EventID, time_t StartTime) const
{
  // Returns the event info with the given StartTime or, if no actual StartTime
  // is given, the one with the given EventID.
  if (StartTime > 0) // 'StartTime < 0' is apparently used with NVOD channels
     return eventsHashStartTime.Get(StartTime);
  else
     return eventsHashID.Get(EventID);
}

const cEvent *cSchedule::GetEventAround(time_t Time) const
{
  const cEvent *pe = NULL;
  time_t delta = INT_MAX;
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      time_t dt = Time - p->StartTime();
      if (dt >= 0 && dt < delta && p->EndTime() >= Time) {
         delta = dt;
         pe = p;
         }
      }
  return pe;
}

void cSchedule::SetRunningStatus(cEvent *Event, int RunningStatus, cChannel *Channel)
{
  hasRunning = false;
  for (cEvent *p = events.First(); p; p = events.Next(p)) {
      if (p == Event) {
         if (p->RunningStatus() > SI::RunningStatusNotRunning || RunningStatus > SI::RunningStatusNotRunning) {
            p->SetRunningStatus(RunningStatus, Channel);
            break;
            }
         }
      else if (RunningStatus >= SI::RunningStatusPausing && p->StartTime() < Event->StartTime())
         p->SetRunningStatus(SI::RunningStatusNotRunning);
      if (p->RunningStatus() >= SI::RunningStatusPausing)
         hasRunning = true;
      }
}

void cSchedule::ClrRunningStatus(cChannel *Channel)
{
  if (hasRunning) {
     for (cEvent *p = events.First(); p; p = events.Next(p)) {
         if (p->RunningStatus() >= SI::RunningStatusPausing) {
            p->SetRunningStatus(SI::RunningStatusNotRunning, Channel);
            hasRunning = false;
            break;
            }
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
  // Make sure there are no RunningStatusUndefined before the currently running event:
  if (hasRunning) {
     for (cEvent *p = events.First(); p; p = events.Next(p)) {
         if (p->RunningStatus() >= SI::RunningStatusPausing)
            break;
         p->SetRunningStatus(SI::RunningStatusNotRunning);
         }
     }
}

void cSchedule::DropOutdated(time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version)
{
  if (SegmentStart > 0 && SegmentEnd > 0) {
     for (cEvent *p = events.First(); p; p = events.Next(p)) {
         if (p->EndTime() > SegmentStart) {
            if (p->StartTime() < SegmentEnd) {
               // The event overlaps with the given time segment.
               if (p->TableID() > TableID || p->TableID() == TableID && p->Version() != Version) {
                  // The segment overwrites all events from tables with higher ids, and
                  // within the same table id all events must have the same version.
                  // We can't delete the event right here because a timer might have
                  // a pointer to it, so let's set its id and start time to 0 to have it
                  // "phased out":
                  if (hasRunning && p->IsRunning())
                     ClrRunningStatus();
                  UnhashEvent(p);
                  p->eventID = 0;
                  p->startTime = 0;
                  }
               }
            else
               break;
            }
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
        if (!Event->HasTimer() && Event->EndTime() + Setup.EPGLinger * 60 + 3600 < Time) // adding one hour for safety
           DelEvent(Event);
        else
           break;
        }
}

void cSchedule::Dump(FILE *f, const char *Prefix, eDumpMode DumpMode, time_t AtTime) const
{
  cChannel *channel = Channels.GetByChannelID(channelID, true);
  if (channel) {
     fprintf(f, "%sC %s %s\n", Prefix, *channel->GetChannelID().ToString(), channel->Name());
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
     cReadLine ReadLine;
     char *s;
     while ((s = ReadLine.Read(f)) != NULL) {
           if (*s == 'C') {
              s = skipspace(s + 1);
              char *p = strchr(s, ' ');
              if (p)
                 *p = 0; // strips optional channel name
              if (*s) {
                 tChannelID channelID = tChannelID::FromString(s);
                 if (channelID.Valid()) {
                    cSchedule *p = Schedules->AddSchedule(channelID);
                    if (p) {
                       if (!cEvent::Read(f, p))
                          return false;
                       p->Sort();
                       Schedules->SetModified(p);
                       }
                    }
                 else {
                    esyslog("ERROR: invalid channel ID: %s", s);
                    return false;
                    }
                 }
              }
           else {
              esyslog("ERROR: unexpected tag while reading EPG data: %s", s);
              return false;
              }
           }
     return true;
     }
  return false;
}

// --- cSchedulesLock --------------------------------------------------------

cSchedulesLock::cSchedulesLock(bool WriteLock, int TimeoutMs)
{
  locked = cSchedules::schedules.rwlock.Lock(WriteLock, TimeoutMs);
}

cSchedulesLock::~cSchedulesLock()
{
  if (locked)
     cSchedules::schedules.rwlock.Unlock();
}

// --- cSchedules ------------------------------------------------------------

cSchedules cSchedules::schedules;
const char *cSchedules::epgDataFileName = NULL;
time_t cSchedules::lastCleanup = time(NULL);
time_t cSchedules::lastDump = time(NULL);
time_t cSchedules::modified = 0;

const cSchedules *cSchedules::Schedules(cSchedulesLock &SchedulesLock)
{
  return SchedulesLock.Locked() ? &schedules : NULL;
}

void cSchedules::SetEpgDataFileName(const char *FileName)
{
  delete epgDataFileName;
  epgDataFileName = FileName ? strdup(FileName) : NULL;
}

void cSchedules::SetModified(cSchedule *Schedule)
{
  Schedule->SetModified();
  modified = time(NULL);
}

void cSchedules::Cleanup(bool Force)
{
  if (Force)
     lastDump = 0;
  time_t now = time(NULL);
  struct tm tm_r;
  struct tm *ptm = localtime_r(&now, &tm_r);
  if (now - lastCleanup > 3600) {
     isyslog("cleaning up schedules data");
     cSchedulesLock SchedulesLock(true, 1000);
     cSchedules *s = (cSchedules *)Schedules(SchedulesLock);
     if (s) {
        for (cSchedule *p = s->First(); p; p = s->Next(p))
            p->Cleanup(now);
        }
     lastCleanup = now;
     if (ptm->tm_hour == 5)
        ReportEpgBugFixStats(true);
     }
  if (epgDataFileName && now - lastDump > 600) {
     cSafeFile f(epgDataFileName);
     if (f.Open()) {
        Dump(f);
        f.Close();
        }
     else
        LOG_ERROR;
     lastDump = now;
     }
}

void cSchedules::ResetVersions(void)
{
  cSchedulesLock SchedulesLock(true);
  cSchedules *s = (cSchedules *)Schedules(SchedulesLock);
  if (s) {
     for (cSchedule *Schedule = s->First(); Schedule; Schedule = s->Next(Schedule))
         Schedule->ResetVersions();
     }
}

bool cSchedules::ClearAll(void)
{
  cSchedulesLock SchedulesLock(true, 1000);
  cSchedules *s = (cSchedules *)Schedules(SchedulesLock);
  if (s) {
     for (cTimer *Timer = Timers.First(); Timer; Timer = Timers.Next(Timer))
         Timer->SetEvent(NULL);
     for (cSchedule *Schedule = s->First(); Schedule; Schedule = s->Next(Schedule))
         Schedule->Cleanup(INT_MAX);
     return true;
     }
  return false;
}

bool cSchedules::Dump(FILE *f, const char *Prefix, eDumpMode DumpMode, time_t AtTime)
{
  cSchedulesLock SchedulesLock;
  cSchedules *s = (cSchedules *)Schedules(SchedulesLock);
  if (s) {
     for (cSchedule *p = s->First(); p; p = s->Next(p))
         p->Dump(f, Prefix, DumpMode, AtTime);
     return true;
     }
  return false;
}

bool cSchedules::Read(FILE *f)
{
  cSchedulesLock SchedulesLock(true, 1000);
  cSchedules *s = (cSchedules *)Schedules(SchedulesLock);
  if (s) {
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
     bool result = cSchedule::Read(f, s);
     if (OwnFile)
        fclose(f);
     if (result) {
        // Initialize the channels' schedule pointers, so that the first WhatsOn menu will come up faster:
        for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel))
            s->GetSchedule(Channel);
        }
     return result;
     }
  return false;
}

cSchedule *cSchedules::AddSchedule(tChannelID ChannelID)
{
  ChannelID.ClrRid();
  cSchedule *p = (cSchedule *)GetSchedule(ChannelID);
  if (!p) {
     p = new cSchedule(ChannelID);
     Add(p);
     cChannel *channel = Channels.GetByChannelID(ChannelID);
     if (channel)
        channel->schedule = p;
     }
  return p;
}

const cSchedule *cSchedules::GetSchedule(tChannelID ChannelID) const
{
  ChannelID.ClrRid();
  for (cSchedule *p = First(); p; p = Next(p)) {
      if (p->ChannelID() == ChannelID)
         return p;
      }
  return NULL;
}

const cSchedule *cSchedules::GetSchedule(const cChannel *Channel, bool AddIfMissing) const
{
  // This is not very beautiful, but it dramatically speeds up the
  // "What's on now/next?" menus.
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
