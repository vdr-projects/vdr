/***************************************************************************
                          eit.c  -  description
                             -------------------
    begin                : Fri Aug 25 2000
    copyright            : (C) 2000 by Robert Schneider
    email                : Robert.Schneider@web.de

    2001-08-15: Adapted to 'libdtv' by Rolf Hakenes <hakenes@hippomi.de>

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 * $Id: eit.c 1.44 2002/04/06 13:58:59 kls Exp $
 ***************************************************************************/

#include "eit.h"
#include <ctype.h>
#include <fcntl.h>
#include <fstream.h>
#include <iomanip.h>
#include <iostream.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "libdtv/libdtv.h"
#include "videodir.h"

// --- cMJD ------------------------------------------------------------------

class cMJD {
public:
   cMJD();
   cMJD(u_char date_hi, u_char date_lo);
   cMJD(u_char date_hi, u_char date_lo, u_char timehr, u_char timemi, u_char timese);
   ~cMJD();
  /**  */
  void ConvertToTime();
  /**  */
  bool SetSystemTime();
  /**  */
  time_t GetTime_t();
protected: // Protected attributes
  /**  */
  time_t mjdtime;
protected: // Protected attributes
  /**  */
  u_char time_second;
protected: // Protected attributes
  /**  */
  u_char time_minute;
protected: // Protected attributes
  /**  */
  u_char time_hour;
protected: // Protected attributes
  /**  */
  u_short mjd;
};

cMJD::cMJD()
{
}

cMJD::cMJD(u_char date_hi, u_char date_lo)
{
   mjd = date_hi << 8 | date_lo;
   time_hour = time_minute = time_second = 0;
   ConvertToTime();
}

cMJD::cMJD(u_char date_hi, u_char date_lo, u_char timehr, u_char timemi, u_char timese)
{
   mjd = date_hi << 8 | date_lo;
   time_hour = timehr;
   time_minute = timemi;
   time_second = timese;
   ConvertToTime();
}

cMJD::~cMJD()
{
}

/**  */
void cMJD::ConvertToTime()
{
   struct tm t;

   t.tm_sec = time_second;
   t.tm_min = time_minute;
   t.tm_hour = time_hour;
   int k;

   t.tm_year = (int) ((mjd - 15078.2) / 365.25);
   t.tm_mon = (int) ((mjd - 14956.1 - (int)(t.tm_year * 365.25)) / 30.6001);
   t.tm_mday = (int) (mjd - 14956 - (int)(t.tm_year * 365.25) - (int)(t.tm_mon * 30.6001));
   k = (t.tm_mon == 14 || t.tm_mon == 15) ? 1 : 0;
   t.tm_year = t.tm_year + k;
   t.tm_mon = t.tm_mon - 1 - k * 12;
   t.tm_mon--;

   t.tm_isdst = -1;
   t.tm_gmtoff = 0;

   mjdtime = timegm(&t);

   //isyslog(LOG_INFO, "Time parsed = %s\n", ctime(&mjdtime));
}

/**  */
bool cMJD::SetSystemTime()
{
   struct tm *ptm;
   time_t loctim;

   struct tm tm_r;
   ptm = localtime_r(&mjdtime, &tm_r);
   loctim = time(NULL);

   if (abs(mjdtime - loctim) > 2)
   {
      isyslog(LOG_INFO, "System Time = %s (%ld)\n", ctime(&loctim), loctim);
      isyslog(LOG_INFO, "Local Time  = %s (%ld)\n", ctime(&mjdtime), mjdtime);
      if (stime(&mjdtime) < 0)
         esyslog(LOG_ERR, "ERROR while setting system time: %m");
      return true;
   }

   return false;
}
/**  */
time_t cMJD::GetTime_t()
{
   return mjdtime;
}

// --- cTDT ------------------------------------------------------------------

class cTDT {
public:
   cTDT(tdt_t *ptdt);
   ~cTDT();
  /**  */
  bool SetSystemTime();
protected: // Protected attributes
  /**  */
  tdt_t tdt;
  /**  */
  cMJD mjd; // kls 2001-03-02: made this a member instead of a pointer (it wasn't deleted in the destructor!)
};

#define BCD2DEC(b) (((b >> 4) & 0x0F) * 10 + (b & 0x0F))

cTDT::cTDT(tdt_t *ptdt)
:tdt(*ptdt)
,mjd(tdt.utc_mjd_hi, tdt.utc_mjd_lo, BCD2DEC(tdt.utc_time_h), BCD2DEC(tdt.utc_time_m), BCD2DEC(tdt.utc_time_s))
{
}

cTDT::~cTDT()
{
}
/**  */
bool cTDT::SetSystemTime()
{
   return mjd.SetSystemTime();
}

// --- cEventInfo ------------------------------------------------------------

cEventInfo::cEventInfo(unsigned short serviceid, unsigned short eventid)
{
   pTitle = NULL;
   pSubtitle = NULL;
   pExtendedDescription = NULL;
   bIsPresent = bIsFollowing = false;
   lDuration = 0;
   tTime = 0;
   uTableID = 0;
   uEventID = eventid;
   uServiceID = serviceid;
   nChannelNumber = 0;
}

cEventInfo::~cEventInfo()
{
   delete pTitle;
   delete pSubtitle;
   delete pExtendedDescription;
}

/**  */
const char * cEventInfo::GetTitle() const
{
   return pTitle;
}
/**  */
const char * cEventInfo::GetSubtitle() const
{
   return pSubtitle;
}
/**  */
const char * cEventInfo::GetExtendedDescription() const
{
   return pExtendedDescription;
}
/**  */
bool cEventInfo::IsPresent() const
{
   return bIsPresent;
}
/**  */
void cEventInfo::SetPresent(bool pres)
{
   bIsPresent = pres;
}
/**  */
bool cEventInfo::IsFollowing() const
{
   return bIsFollowing;
}

void cEventInfo::SetTableID(unsigned char tableid)
{
   uTableID = tableid;
}

/**  */
void cEventInfo::SetFollowing(bool foll)
{
   bIsFollowing = foll;
}
/**  */
const char * cEventInfo::GetDate() const
{
   static char szDate[25];

   struct tm tm_r;
   strftime(szDate, sizeof(szDate), "%d.%m.%Y", localtime_r(&tTime, &tm_r));

   return szDate;
}

const unsigned char cEventInfo::GetTableID(void) const
{
   return uTableID;
}

/**  */
const char * cEventInfo::GetTimeString() const
{
   static char szTime[25];

   struct tm tm_r;
   strftime(szTime, sizeof(szTime), "%R", localtime_r(&tTime, &tm_r));

   return szTime;
}
/**  */
const char * cEventInfo::GetEndTimeString() const
{
   static char szEndTime[25];
   time_t tEndTime = tTime + lDuration;

   struct tm tm_r;
   strftime(szEndTime, sizeof(szEndTime), "%R", localtime_r(&tEndTime, &tm_r));

   return szEndTime;
}
/**  */
time_t cEventInfo::GetTime() const
{
   return tTime;
}
/**  */
long cEventInfo::GetDuration() const
{
   return lDuration;
}
/**  */
unsigned short cEventInfo::GetEventID() const
{
   return uEventID;
}
/**  */
void cEventInfo::SetTitle(const char *string)
{
   pTitle = strcpyrealloc(pTitle, string);
}
/**  */
void cEventInfo::SetSubtitle(const char *string)
{
   pSubtitle = strcpyrealloc(pSubtitle, string);
}
/**  */
void cEventInfo::SetExtendedDescription(const char *string)
{
   pExtendedDescription = strcpyrealloc(pExtendedDescription, string);
}
/**  */
void cEventInfo::SetTime(time_t t)
{
   tTime = t;
}
/**  */
void cEventInfo::SetDuration(long l)
{
   lDuration = l;
}
/**  */
void cEventInfo::SetEventID(unsigned short evid)
{
   uEventID = evid;
}
/**  */
void cEventInfo::SetServiceID(unsigned short servid)
{
   uServiceID = servid;
}

/**  */
unsigned short cEventInfo::GetServiceID() const
{
   return uServiceID;
}

/**  */
void cEventInfo::Dump(FILE *f, const char *Prefix) const
{
   if (tTime + lDuration >= time(NULL)) {
      fprintf(f, "%sE %u %ld %ld %X\n", Prefix, uEventID, tTime, lDuration, uTableID);
      if (!isempty(pTitle))
         fprintf(f, "%sT %s\n", Prefix, pTitle);
      if (!isempty(pSubtitle))
         fprintf(f, "%sS %s\n", Prefix, pSubtitle);
      if (!isempty(pExtendedDescription))
         fprintf(f, "%sD %s\n", Prefix, pExtendedDescription);
      fprintf(f, "%se\n", Prefix);
      }
}

bool cEventInfo::Read(FILE *f, cSchedule *Schedule)
{
  if (Schedule) {
     cEventInfo *pEvent = NULL;
     char *s;
     while ((s = readline(f)) != NULL) {
           char *t = skipspace(s + 1);
           switch (*s) {
             case 'E': if (!pEvent) {
                          unsigned int uEventID;
                          time_t tTime;
                          long lDuration;
                          unsigned int uTableID = 0;
                          int n = sscanf(t, "%u %ld %ld %X", &uEventID, &tTime, &lDuration, &uTableID);
                          if (n == 3 || n == 4) {
                             pEvent = (cEventInfo *)Schedule->GetEvent(uEventID, tTime);
                             if (!pEvent)
                                pEvent = Schedule->AddEvent(new cEventInfo(Schedule->GetServiceID(), uEventID));
                             if (pEvent) {
                                pEvent->SetTableID(uTableID);
                                pEvent->SetTime(tTime);
                                pEvent->SetDuration(lDuration);
                                }
                             }
                          }
                       break;
             case 'T': if (pEvent)
                          pEvent->SetTitle(t);
                       break;
             case 'S': if (pEvent)
                          pEvent->SetSubtitle(t);
                       break;
             case 'D': if (pEvent)
                          pEvent->SetExtendedDescription(t);
                       break;
             case 'e': pEvent = NULL;
                       break;
             case 'c': // to keep things simple we react on 'c' here
                       return true;
             default:  esyslog(LOG_ERR, "ERROR: unexpected tag while reading EPG data: %s", s);
                       return false;
             }
           }
     esyslog(LOG_ERR, "ERROR: unexpected end of file while reading EPG data");
     }
  return false;
}

#define MAXEPGBUGFIXSTATS 5
#define MAXEPGBUGFIXCHANS 50
struct tEpgBugFixStats {
  int hits;
  int n;
  unsigned short serviceIDs[MAXEPGBUGFIXCHANS];
  tEpgBugFixStats(void) { hits = n = 0; }
  };

tEpgBugFixStats EpgBugFixStats[MAXEPGBUGFIXSTATS];

static void EpgBugFixStat(int Number, unsigned int ServiceID)
{
  if (0 <= Number && Number < MAXEPGBUGFIXSTATS) {
     tEpgBugFixStats *p = &EpgBugFixStats[Number];
     p->hits++;
     int i = 0;
     for (; i < p->n; i++) {
         if (p->serviceIDs[i] == ServiceID)
            break;
         }
     if (i == p->n && p->n < MAXEPGBUGFIXCHANS)
        p->serviceIDs[p->n++] = ServiceID;
     }
}

static void ReportEpgBugFixStats(bool Reset = false)
{
  if (Setup.EPGBugfixLevel > 0) {
     bool GotHits = false;
     char buffer[1024];
     for (int i = 0; i < MAXEPGBUGFIXSTATS; i++) {
         const char *delim = "\t";
         tEpgBugFixStats *p = &EpgBugFixStats[i];
         if (p->hits) {
            if (!GotHits) {
               dsyslog(LOG_INFO, "=====================");
               dsyslog(LOG_INFO, "EPG bugfix statistics");
               dsyslog(LOG_INFO, "=====================");
               dsyslog(LOG_INFO, "IF SOMEBODY WHO IS IN CHARGE OF THE EPG DATA FOR ONE OF THE LISTED");
               dsyslog(LOG_INFO, "CHANNELS READS THIS: PLEASE TAKE A LOOK AT THE FUNCTION cEventInfo::FixEpgBugs()");
               dsyslog(LOG_INFO, "IN VDR/eit.c TO LEARN WHAT'S WRONG WITH YOUR DATA, AND FIX IT!");
               dsyslog(LOG_INFO, "=====================");
               dsyslog(LOG_INFO, "Fix\tHits\tChannels");
               GotHits = true;
               }
            char *q = buffer;
            q += snprintf(q, sizeof(buffer) - (q - buffer), "%d\t%d", i, p->hits);
            for (int c = 0; c < p->n; c++) {
                cChannel *channel = Channels.GetByServiceID(p->serviceIDs[c]);
                if (channel) {
                   q += snprintf(q, sizeof(buffer) - (q - buffer), "%s%s", delim, channel->name);
                   delim = ", ";
                   }
                }
            dsyslog(LOG_INFO, "%s", buffer);
            }
         if (Reset)
            p->hits = p->n = 0;
         }
     if (GotHits)
        dsyslog(LOG_INFO, "=====================");
     }
}

void cEventInfo::FixEpgBugs(void)
{
  // VDR can't usefully handle newline characters in the EPG data, so let's
  // always convert them to blanks (independent of the setting of EPGBugfixLevel):
  strreplace(pTitle, '\n', ' ');
  strreplace(pSubtitle, '\n', ' ');
  strreplace(pExtendedDescription, '\n', ' ');

  if (Setup.EPGBugfixLevel == 0)
     return;

  // Some TV stations apparently have their own idea about how to fill in the
  // EPG data. Let's fix their bugs as good as we can:
  if (pTitle) {

     // VOX and VIVA put the Subtitle in quotes and use either the Subtitle
     // or the Extended Description field, depending on how long the string is:
     //
     // Title
     // "Subtitle". Extended Description
     //
     if ((pSubtitle == NULL) != (pExtendedDescription == NULL)) {
        char *p = pSubtitle ? pSubtitle : pExtendedDescription;
        if (*p == '"') {
           const char *delim = "\".";
           char *e = strstr(p + 1, delim);
           if (e) {
              *e = 0;
              char *s = strdup(p + 1);
              char *d = strdup(e + strlen(delim));
              delete pSubtitle;
              delete pExtendedDescription;
              pSubtitle = s;
              pExtendedDescription = d;
              EpgBugFixStat(0, GetServiceID());
              }
           }
        }

     // VOX and VIVA put the Extended Description into the Subtitle (preceeded
     // by a blank) if there is no actual Subtitle and the Extended Description
     // is short enough:
     //
     // Title
     //  Extended Description
     //
     if (pSubtitle && !pExtendedDescription) {
        if (*pSubtitle == ' ') {
           memmove(pSubtitle, pSubtitle + 1, strlen(pSubtitle));
           pExtendedDescription = pSubtitle;
           pSubtitle = NULL;
           EpgBugFixStat(1, GetServiceID());
           }
        }

     // Pro7 sometimes repeats the Title in the Subtitle:
     //
     // Title
     // Title
     //
     if (pSubtitle && strcmp(pTitle, pSubtitle) == 0) {
        delete pSubtitle;
        pSubtitle = NULL;
        EpgBugFixStat(2, GetServiceID());
        }

     // ZDF.info puts the Subtitle between double quotes, which is nothing
     // but annoying (some even put a '.' after the closing '"'):
     //
     // Title
     // "Subtitle"[.]
     //
     if (pSubtitle && *pSubtitle == '"') {
        int l = strlen(pSubtitle);
        if (l > 2 && (pSubtitle[l - 1] == '"' || (pSubtitle[l - 1] == '.' && pSubtitle[l - 2] == '"'))) {
           memmove(pSubtitle, pSubtitle + 1, l);
           char *p = strrchr(pSubtitle, '"');
           if (p)
              *p = 0;
           EpgBugFixStat(3, GetServiceID());
           }
        }

     if (Setup.EPGBugfixLevel <= 1)
        return;

     // Some channels apparently try to do some formatting in the texts,
     // which is a bad idea because they have no way of knowing the width
     // of the window that will actually display the text.
     // Remove excess whitespace:
     pTitle = compactspace(pTitle);
     pSubtitle = compactspace(pSubtitle);
     pExtendedDescription = compactspace(pExtendedDescription);
     // Remove superfluous hyphens:
     if (pExtendedDescription) {
        char *p = pExtendedDescription;
        while (*p && *(p + 1) && *(p + 2)) {
              if (*p == '-' && *(p + 1) == ' ' && *(p + 2) && islower(*(p - 1)) && islower(*(p + 2))) {
                 if (!startswith(p + 2, "und ")) { // special case in German, as in "Lach- und Sachgeschichten"
                    memmove(p, p + 2, strlen(p + 2) + 1);
                    EpgBugFixStat(4, GetServiceID());
                    }
                 }
              p++;
              }
        }

     // Some channels use the ` ("backtick") character, where a ' (single quote)
     // would be normally used. Actually, "backticks" in normal text don't make
     // much sense, so let's replace them:
     strreplace(pTitle, '`', '\'');
     strreplace(pSubtitle, '`', '\'');
     strreplace(pExtendedDescription, '`', '\'');
     }
}

// --- cSchedule -------------------------------------------------------------

cSchedule::cSchedule(unsigned short servid)
{
   pPresent = pFollowing = NULL;
   uServiceID = servid;
}


cSchedule::~cSchedule()
{
}

cEventInfo *cSchedule::AddEvent(cEventInfo *EventInfo)
{
  Events.Add(EventInfo);
  return EventInfo;
}

/**  */
const cEventInfo * cSchedule::GetPresentEvent() const
{
   // checking temporal sanity of present event (kls 2000-11-01)
   time_t now = time(NULL);
//XXX   if (pPresent && !(pPresent->GetTime() <= now && now <= pPresent->GetTime() + pPresent->GetDuration()))
   {
      cEventInfo *pe = Events.First();
      while (pe != NULL)
      {
         if (pe->GetTime() <= now && now <= pe->GetTime() + pe->GetDuration())
            return pe;
         pe = Events.Next(pe);
      }
   }
   return NULL;//XXX
   return pPresent;
}
/**  */
const cEventInfo * cSchedule::GetFollowingEvent() const
{
   // checking temporal sanity of following event (kls 2000-11-01)
   time_t now = time(NULL);
   const cEventInfo *pr = GetPresentEvent(); // must have it verified!
if (pr)//XXX   if (pFollowing && !(pr && pr->GetTime() + pr->GetDuration() <= pFollowing->GetTime()))
   {
      int minDt = INT_MAX;
      cEventInfo *pe = Events.First(), *pf = NULL;
      while (pe != NULL)
      {
         int dt = pe->GetTime() - now;
         if (dt > 0 && dt < minDt)
         {
            minDt = dt;
            pf = pe;
         }
         pe = Events.Next(pe);
      }
      return pf;
   }
   return NULL;//XXX
   return pFollowing;
}
/**  */
void cSchedule::SetServiceID(unsigned short servid)
{
   uServiceID = servid;
}
/**  */
unsigned short cSchedule::GetServiceID() const
{
   return uServiceID;
}
/**  */
const cEventInfo * cSchedule::GetEvent(unsigned short uEventID, time_t tTime) const
{
   // Returns either the event info with the given uEventID or, if that one can't
   // be found, the one with the given tTime (or NULL if neither can be found)
   cEventInfo *pe = Events.First();
   cEventInfo *pt = NULL;
   while (pe != NULL)
   {
      if (pe->GetEventID() == uEventID)
         return pe;
      if (tTime > 0 && pe->GetTime() == tTime) // 'tTime < 0' is apparently used with NVOD channels
         pt = pe;

      pe = Events.Next(pe);
   }

   return pt;
}
/**  */
const cEventInfo * cSchedule::GetEventAround(time_t tTime) const
{
   cEventInfo *pe = Events.First();
   while (pe != NULL)
   {
      if (pe->GetTime() <= tTime && tTime <= pe->GetTime() + pe->GetDuration())
         return pe;

      pe = Events.Next(pe);
   }

   return NULL;
}
/**  */
bool cSchedule::SetPresentEvent(cEventInfo *pEvent)
{
   if (pPresent != NULL)
      pPresent->SetPresent(false);
   pPresent = pEvent;
   pPresent->SetPresent(true);

   return true;
}

/**  */
bool cSchedule::SetFollowingEvent(cEventInfo *pEvent)
{
   if (pFollowing != NULL)
      pFollowing->SetFollowing(false);
   pFollowing = pEvent;
   pFollowing->SetFollowing(true);

   return true;
}

/**  */
void cSchedule::Cleanup()
{
   Cleanup(time(NULL));
}

/**  */
void cSchedule::Cleanup(time_t tTime)
{
   cEventInfo *pEvent;
   for (int a = 0; true ; a++)
   {
      pEvent = Events.Get(a);
      if (pEvent == NULL)
         break;
      if (pEvent->GetTime() + pEvent->GetDuration() + 3600 < tTime) // adding one hour for safety
      {
         Events.Del(pEvent);
         a--;
      }
   }
}

/**  */
void cSchedule::Dump(FILE *f, const char *Prefix) const
{
   cChannel *channel = Channels.GetByServiceID(uServiceID);
   if (channel)
   {
      fprintf(f, "%sC %u %s\n", Prefix, uServiceID, channel->name);
      for (cEventInfo *p = Events.First(); p; p = Events.Next(p))
         p->Dump(f, Prefix);
      fprintf(f, "%sc\n", Prefix);
   }
}

bool cSchedule::Read(FILE *f, cSchedules *Schedules)
{
  if (Schedules) {
     char *s;
     while ((s = readline(f)) != NULL) {
           if (*s == 'C') {
              unsigned int uServiceID;
              if (1 == sscanf(s + 1, "%u", &uServiceID)) {
                 cSchedule *p = (cSchedule *)Schedules->SetCurrentServiceID(uServiceID);
                 if (p) {
                    if (!cEventInfo::Read(f, p))
                       return false;
                    }
                 }
              }
           else {
              esyslog(LOG_ERR, "ERROR: unexpected tag while reading EPG data: %s", s);
              return false;
              }
           }
     return true;
     }
  return false;
}

// --- cSchedules ------------------------------------------------------------

cSchedules::cSchedules()
{
   pCurrentSchedule = NULL;
   uCurrentServiceID = 0;
}

cSchedules::~cSchedules()
{
}
/**  */
const cSchedule *cSchedules::SetCurrentServiceID(unsigned short servid)
{
   pCurrentSchedule = GetSchedule(servid);
   if (pCurrentSchedule == NULL)
   {
      Add(new cSchedule(servid));
      pCurrentSchedule = GetSchedule(servid);
      if (pCurrentSchedule == NULL)
         return NULL;
   }

   uCurrentServiceID = servid;

   return pCurrentSchedule;
}
/**  */
const cSchedule * cSchedules::GetSchedule() const
{
   return pCurrentSchedule;
}
/**  */
const cSchedule * cSchedules::GetSchedule(unsigned short servid) const
{
   cSchedule *p;

   p = First();
   while (p != NULL)
   {
      if (p->GetServiceID() == servid)
         return p;
      p = Next(p);
   }

   return NULL;
}

/**  */
void cSchedules::Cleanup()
{
   cSchedule *p;

   p = First();
   while (p != NULL)
   {
      p->Cleanup(time(NULL));
      p = Next(p);
   }
}

/**  */
void cSchedules::Dump(FILE *f, const char *Prefix) const
{
   for (cSchedule *p = First(); p; p = Next(p))
      p->Dump(f, Prefix);
}

/**  */
bool cSchedules::Read(FILE *f)
{
   cMutexLock MutexLock;
   return cSchedule::Read(f, (cSchedules *)cSIProcessor::Schedules(MutexLock));
}

// --- cEIT ------------------------------------------------------------------

class cEIT {
private:
   cSchedules *schedules;
public:
   cEIT(unsigned char *buf, int length, cSchedules *Schedules);
   ~cEIT();
  /**  */
  int ProcessEIT(unsigned char *buffer);

protected: // Protected methods
  /** returns true if this EIT covers a
present/following information, false if it's
schedule information */
  bool IsPresentFollowing();
protected: // Protected attributes
  /** Table ID of this EIT struct */
  u_char tid;
};

cEIT::cEIT(unsigned char * buf, int length, cSchedules *Schedules)
{
   tid = buf[0];
   schedules = Schedules;
}

cEIT::~cEIT()
{
}

/**  */
int cEIT::ProcessEIT(unsigned char *buffer)
{
   cEventInfo *pEvent, *rEvent = NULL;
   cSchedule *pSchedule, *rSchedule = NULL;
   struct LIST *VdrProgramInfos;
   struct VdrProgramInfo *VdrProgramInfo;

   if (!buffer)
      return -1;

   VdrProgramInfos = createVdrProgramInfos(buffer);

   if (VdrProgramInfos) {
      for (VdrProgramInfo = (struct VdrProgramInfo *) VdrProgramInfos->Head; VdrProgramInfo; VdrProgramInfo = (struct VdrProgramInfo *) xSucc (VdrProgramInfo)) {
          pSchedule = (cSchedule *)schedules->GetSchedule(VdrProgramInfo->ServiceID);
          if (!pSchedule) {
             schedules->Add(new cSchedule(VdrProgramInfo->ServiceID));
             pSchedule = (cSchedule *)schedules->GetSchedule(VdrProgramInfo->ServiceID);
             if (!pSchedule)
                break;
             }
          if (VdrProgramInfo->ReferenceServiceID) {
             rSchedule = (cSchedule *)schedules->GetSchedule(VdrProgramInfo->ReferenceServiceID);
             if (!rSchedule)
                break;
             rEvent = (cEventInfo *)rSchedule->GetEvent((unsigned short)VdrProgramInfo->ReferenceEventID);
             if (!rEvent)
                break;
             }
          pEvent = (cEventInfo *)pSchedule->GetEvent((unsigned short)VdrProgramInfo->EventID, VdrProgramInfo->StartTime);
          if (!pEvent) {
             // If we don't have that event ID yet, we create a new one.
             // Otherwise we copy the information into the existing event anyway, because the data might have changed.
             pEvent = pSchedule->AddEvent(new cEventInfo(VdrProgramInfo->ServiceID, VdrProgramInfo->EventID));
             if (!pEvent)
                break;
             pEvent->SetTableID(tid);
             }
          else {
             // We have found an existing event, either through its event ID or its start time.
             // If the existing event has a zero table ID it was defined externally and shall
             // not be overwritten.
             if (pEvent->GetTableID() == 0x00)
                continue;
             // If the new event comes from a table that belongs to an "other TS" and the existing
             // one comes from an "actual TS" table, lets skip it.
             if ((tid == 0x4F || tid == 0x60 || tid == 0x61) && (pEvent->GetTableID() == 0x4E || pEvent->GetTableID() == 0x50 || pEvent->GetTableID() == 0x51))
                continue;
             }
          if (rEvent) {
             pEvent->SetTitle(rEvent->GetTitle());
             pEvent->SetSubtitle(rEvent->GetSubtitle());
             pEvent->SetExtendedDescription(rEvent->GetExtendedDescription());
             }
          else {
             pEvent->SetTableID(tid);
             pEvent->SetTitle(VdrProgramInfo->ShortName);
             pEvent->SetSubtitle(VdrProgramInfo->ShortText);
             pEvent->SetExtendedDescription(VdrProgramInfo->ExtendedName);
             //XXX kls 2001-09-22:
             //XXX apparently this never occurred, so I have simpified ExtendedDescription handling
             //XXX pEvent->AddExtendedDescription(VdrProgramInfo->ExtendedText);
             }
          pEvent->SetTime(VdrProgramInfo->StartTime);
          pEvent->SetDuration(VdrProgramInfo->Duration);
          pEvent->FixEpgBugs();
          if (IsPresentFollowing()) {
             if ((GetRunningStatus(VdrProgramInfo->Status) == RUNNING_STATUS_PAUSING) || (GetRunningStatus(VdrProgramInfo->Status) == RUNNING_STATUS_RUNNING))
                pSchedule->SetPresentEvent(pEvent);
             else if (GetRunningStatus(VdrProgramInfo->Status) == RUNNING_STATUS_AWAITING)
                pSchedule->SetFollowingEvent(pEvent);
             }
          }
      }

   xMemFreeAll(NULL);
   return 0;
}

/** returns true if this EIT covers a
present/following information, false if it's
schedule information */
bool cEIT::IsPresentFollowing()
{
   if (tid == 0x4e || tid == 0x4f)
      return true;

   return false;
}

// --- cSIProcessor ----------------------------------------------------------

#define MAX_FILTERS 20
#define EPGDATAFILENAME "epg.data"

int cSIProcessor::numSIProcessors = 0;
cSchedules *cSIProcessor::schedules = NULL;
cMutex cSIProcessor::schedulesMutex;
const char *cSIProcessor::epgDataFileName = EPGDATAFILENAME;

/**  */
cSIProcessor::cSIProcessor(const char *FileName)
{
   fileName = strdup(FileName);
   masterSIProcessor = numSIProcessors == 0; // the first one becomes the 'master'
   filters = NULL;
   if (!numSIProcessors++) // the first one creates it
      schedules = new cSchedules;
   filters = (SIP_FILTER *)calloc(MAX_FILTERS, sizeof(SIP_FILTER));
   SetStatus(true);
   Start();
}

cSIProcessor::~cSIProcessor()
{
   if (masterSIProcessor)
      ReportEpgBugFixStats();
   active = false;
   Cancel(3);
   ShutDownFilters();
   delete filters;
   if (!--numSIProcessors) // the last one deletes it
      delete schedules;
   delete fileName;
}

const cSchedules *cSIProcessor::Schedules(cMutexLock &MutexLock)
{
  if (MutexLock.Lock(&schedulesMutex))
     return schedules;
  return NULL;
}

bool cSIProcessor::Read(FILE *f)
{
  bool OwnFile = f == NULL;
  if (OwnFile) {
     const char *FileName = GetEpgDataFileName();
     if (access(FileName, R_OK) == 0) {
        dsyslog(LOG_INFO, "reading EPG data from %s", FileName);
        if ((f = fopen(FileName, "r")) == NULL) {
           LOG_ERROR;
           return false;
           }
        }
     else
        return false;
     }
  bool result = cSchedules::Read(f);
  if (OwnFile)
     fclose(f);
  return result;
}

void cSIProcessor::SetEpgDataFileName(const char *FileName)
{
  epgDataFileName = NULL;
  if (FileName)
     epgDataFileName = strdup(DirectoryOk(FileName) ? AddDirectory(FileName, EPGDATAFILENAME) : FileName);
}

const char *cSIProcessor::GetEpgDataFileName(void)
{
  if (epgDataFileName)
     return *epgDataFileName == '/' ? epgDataFileName : AddDirectory(VideoDirectory, epgDataFileName);
  return NULL;
}

void cSIProcessor::SetStatus(bool On)
{
   ShutDownFilters();
   if (On)
   {
      AddFilter(0x14, 0x70);  // TDT
      AddFilter(0x14, 0x73);  // TOT
      AddFilter(0x12, 0x4e);  // event info, actual TS, present/following
      AddFilter(0x12, 0x4f);  // event info, other TS, present/following
      AddFilter(0x12, 0x50);  // event info, actual TS, schedule
      AddFilter(0x12, 0x60);  // event info, other TS, schedule
      AddFilter(0x12, 0x51);  // event info, actual TS, schedule for another 4 days
      AddFilter(0x12, 0x61);  // event info, other TS, schedule for another 4 days
   }
}

/** use the vbi device to parse all relevant SI
information and let the classes corresponding
to the tables write their information to the disk */
void cSIProcessor::Action()
{
   dsyslog(LOG_INFO, "EIT processing thread started (pid=%d)%s", getpid(), masterSIProcessor ? " - master" : "");

   time_t lastCleanup = time(NULL);
   time_t lastDump = time(NULL);

   active = true;

   while(active)
   {
      if (masterSIProcessor)
      {
         time_t now = time(NULL);
         struct tm tm_r;
         struct tm *ptm = localtime_r(&now, &tm_r);
         if (now - lastCleanup > 3600 && ptm->tm_hour == 5)
         {
            cMutexLock MutexLock(&schedulesMutex);
            isyslog(LOG_INFO, "cleaning up schedules data");
            schedules->Cleanup();
            lastCleanup = now;
            ReportEpgBugFixStats(true);
         }
         if (epgDataFileName && now - lastDump > 600)
         {
            cMutexLock MutexLock(&schedulesMutex);
            cSafeFile f(GetEpgDataFileName());
            if (f.Open()) {
               schedules->Dump(f);
               f.Close();
               }
            else
               LOG_ERROR;
            lastDump = now;
         }
      }

      // set up pfd structures for all active filter
      pollfd pfd[MAX_FILTERS];
      int NumUsedFilters = 0;
      for (int a = 0; a < MAX_FILTERS ; a++)
      {
         if (filters[a].inuse)
         {
            pfd[NumUsedFilters].fd = filters[a].handle;
            pfd[NumUsedFilters].events = POLLIN;
            NumUsedFilters++;
         }
      }

      // wait until data becomes ready from the bitfilter
      if (poll(pfd, NumUsedFilters, 1000) != 0)
      {
         for (int a = 0; a < NumUsedFilters ; a++)
         {
            if (pfd[a].revents & POLLIN)
            {
               /* read section */
               unsigned char buf[4096+1]; // max. allowed size for any EIT section (+1 for safety ;-)
               if (safe_read(filters[a].handle, buf, 3) == 3)
               {
                  int seclen = ((buf[1] & 0x0F) << 8) | (buf[2] & 0xFF);
                  int pid = filters[a].pid;
                  int n = safe_read(filters[a].handle, buf + 3, seclen);
                  if (n == seclen)
                  {
                     seclen += 3;
                     //dsyslog(LOG_INFO, "Received pid 0x%02x with table ID 0x%02x and length of %04d\n", pid, buf[0], seclen);
                     switch (pid)
                     {
                        case 0x14:
                           if (buf[0] == 0x70)
                           {
                              if (Setup.SetSystemTime && Setup.TimeTransponder && ISTRANSPONDER(currentTransponder, Setup.TimeTransponder))
                              {
                                 cTDT ctdt((tdt_t *)buf);
                                 ctdt.SetSystemTime();
                              }
                           }
                              /*XXX this comes pretty often:
                           else
                              dsyslog(LOG_INFO, "Time packet was not 0x70 but 0x%02x\n", (int)buf[0]);
                              XXX*/
                           break;

                        case 0x12:
                           if (buf[0] != 0x72)
                           {
                              cMutexLock MutexLock(&schedulesMutex);
                              cEIT ceit(buf, seclen, schedules);
                              ceit.ProcessEIT(buf);
                           }
                           else
                              dsyslog(LOG_INFO, "Received stuffing section in EIT\n");
                           break;

                        default:
                           break;
                     }
                  }
                  /*XXX this just fills up the log file - shouldn't we rather try to re-sync?
                  else
                     dsyslog(LOG_INFO, "read incomplete section - seclen = %d, n = %d", seclen, n);
                  XXX*/
               }
            }
         }
      }
   }

   dsyslog(LOG_INFO, "EIT processing thread ended (pid=%d)%s", getpid(), masterSIProcessor ? " - master" : "");
}

/** Add a filter with packet identifier pid and
table identifer tid */
bool cSIProcessor::AddFilter(u_char pid, u_char tid)
{
   dmxSctFilterParams sctFilterParams;
   sctFilterParams.pid = pid;
   memset(&sctFilterParams.filter.filter, 0, DMX_FILTER_SIZE);
   memset(&sctFilterParams.filter.mask, 0, DMX_FILTER_SIZE);
   sctFilterParams.timeout = 0;
   sctFilterParams.flags = DMX_IMMEDIATE_START;
   sctFilterParams.filter.filter[0] = tid;
   sctFilterParams.filter.mask[0] = 0xFF;

   for (int a = 0; a < MAX_FILTERS; a++)
   {
      if (!filters[a].inuse)
      {
         filters[a].pid = pid;
         filters[a].tid = tid;
         if ((filters[a].handle = open(fileName, O_RDWR | O_NONBLOCK)) >= 0)
         {
            if (ioctl(filters[a].handle, DMX_SET_FILTER, &sctFilterParams) >= 0)
               filters[a].inuse = true;
            else
            {
               esyslog(LOG_ERR, "ERROR: can't set filter");
               close(filters[a].handle);
               return false;
            }
            // dsyslog(LOG_INFO, "  Registered filter handle %04x, pid = %02d, tid = %02d", filters[a].handle, filters[a].pid, filters[a].tid);
         }
         else
         {
            esyslog(LOG_ERR, "ERROR: can't open filter handle");
            return false;
         }
         return true;
      }
   }
   esyslog(LOG_ERR, "ERROR: too many filters");

   return false;
}

/**  */
bool cSIProcessor::ShutDownFilters(void)
{
   for (int a = 0; a < MAX_FILTERS; a++)
   {
      if (filters[a].inuse)
      {
         close(filters[a].handle);
         // dsyslog(LOG_INFO, "Deregistered filter handle %04x, pid = %02d, tid = %02d", filters[a].handle, filters[a].pid, filters[a].tid);
         filters[a].inuse = false;
      }
   }

   return true; // there's no real 'boolean' to return here...
}

/** */
void cSIProcessor::SetCurrentTransponder(int CurrentTransponder)
{
  currentTransponder = CurrentTransponder;
}

/** */
bool cSIProcessor::SetCurrentServiceID(unsigned short servid)
{
  cMutexLock MutexLock(&schedulesMutex);
  return schedules ? schedules->SetCurrentServiceID(servid) : false;
}
