/***************************************************************************
                          eit.c  -  description
                             -------------------
    begin                : Fri Aug 25 2000
    copyright            : (C) 2000 by Robert Schneider
    email                : Robert.Schneider@web.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 * $Id: eit.c 1.11 2000/12/03 15:33:37 kls Exp $
 ***************************************************************************/

#include "eit.h"
#include <ctype.h>
#include <dvb_comcode.h>
#include <dvb_v4l.h>
#include <fcntl.h>
#include <fstream.h>
#include <iomanip.h>
#include <iostream.h>
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
	
	ptm = localtime(&mjdtime);
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

typedef struct {
	u_char	table_id							: 8;

#if BYTE_ORDER == BIG_ENDIAN
	u_char	section_syntax_indicator	: 1;
	u_char										: 3;
	u_char	section_length_hi				: 4;
#else
	u_char	section_length_hi				: 4;
	u_char										: 3;
	u_char	section_syntax_indicator	: 1;
#endif

	u_char	section_length_lo				: 8;


	u_char	utc_date_hi						: 8;
	u_char	utc_date_lo						: 8;
	u_char	utc_hour							: 4;
	u_char	utc_hour_ten					: 4;
	u_char	utc_min							: 4;
	u_char	utc_min_ten						: 4;
	u_char	utc_sec							: 4;
	u_char	utc_sec_ten						: 4;
} tdt_t;

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
  cMJD * mjd;
};

cTDT::cTDT(tdt_t *ptdt)
{
	tdt = *ptdt;
	mjd = new cMJD(tdt.utc_date_hi, tdt.utc_date_lo,
						tdt.utc_hour_ten * 10 + tdt.utc_hour,
						tdt.utc_min_ten * 10 + tdt.utc_min,
						tdt.utc_sec_ten * 10 + tdt.utc_sec);
}

cTDT::~cTDT()
{
}
/**  */
bool cTDT::SetSystemTime()
{
	return mjd->SetSystemTime();
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
	uEventID = eventid;
	uServiceID = serviceid;
	cExtendedDescriptorNumber = 0;
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
/**  */
void cEventInfo::SetFollowing(bool foll)
{
	bIsFollowing = foll;
}
/**  */
const char * cEventInfo::GetDate() const
{
	static char szDate[25];

	strftime(szDate, sizeof(szDate), "%d.%m.%Y", localtime(&tTime));

	return szDate;
}
/**  */
const char * cEventInfo::GetTimeString() const
{
	static char szTime[25];
	
	strftime(szTime, sizeof(szTime), "%R", localtime(&tTime));
	
	return szTime;
}
/**  */
const char * cEventInfo::GetEndTimeString() const
{
	static char szEndTime[25];
   time_t tEndTime = tTime + lDuration;
	
	strftime(szEndTime, sizeof(szEndTime), "%R", localtime(&tEndTime));
	
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
bool cEventInfo::SetTitle(char *string)
{
	if (string == NULL)
		return false;
	
	pTitle = strdup(string);
	if (pTitle == NULL)
		return false;
		
	return true;
}
/**  */
bool cEventInfo::SetSubtitle(char *string)
{
	if (string == NULL)
		return false;
	
	pSubtitle = strdup(string);
	if (pSubtitle == NULL)
		return false;
		
	return true;
}
/**  */
bool cEventInfo::AddExtendedDescription(char *string)
{
	int size = 0;
	bool first = true;
	char *p;
	
	if (string == NULL)
		return false;
	
	if (pExtendedDescription)
	{
		first = false;
		size += strlen(pExtendedDescription);
	}

	size += (strlen(string) + 1);

	p = (char *)realloc(pExtendedDescription, size);
	if (p == NULL)
		return false;
	
	if (first)
		*p = 0;
	
	strcat(p, string);
	
	pExtendedDescription = p;

	return true;
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
u_char cEventInfo::GetExtendedDescriptorNumber() const
{
	return cExtendedDescriptorNumber;
}
/**  */
void cEventInfo::IncreaseExtendedDescriptorNumber()
{
	cExtendedDescriptorNumber++;
}

/**  */
unsigned short cEventInfo::GetServiceID() const
{
   return uServiceID;
}

/**  */
void cEventInfo::Dump(FILE *f) const
{
   if (tTime + lDuration >= time(NULL)) {
      fprintf(f, "E %u %ld %ld\n", uEventID, tTime, lDuration);
      if (!isempty(pTitle))
         fprintf(f, "T %s\n", pTitle);
      if (!isempty(pSubtitle))
         fprintf(f, "S %s\n", pSubtitle);
      if (!isempty(pExtendedDescription))
         fprintf(f, "D %s\n", pExtendedDescription);
      fprintf(f, "e\n");
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
/**  */
const cEventInfo * cSchedule::GetPresentEvent() const
{
   // checking temporal sanity of present event (kls 2000-11-01)
   time_t now = time(NULL);
   if (pPresent && !(pPresent->GetTime() <= now && now <= pPresent->GetTime() + pPresent->GetDuration()))
   {
	   cEventInfo *pe = Events.First();
	   while (pe != NULL)
	   {
		   if (pe->GetTime() <= now && now <= pe->GetTime() + pe->GetDuration())
            return pe;
         pe = Events.Next(pe);
	   }
   }
	return pPresent;
}
/**  */
const cEventInfo * cSchedule::GetFollowingEvent() const
{
   // checking temporal sanity of following event (kls 2000-11-01)
   time_t now = time(NULL);
   const cEventInfo *pr = GetPresentEvent(); // must have it verified!
   if (pFollowing && !(pr && pr->GetTime() + pr->GetDuration() <= pFollowing->GetTime()))
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
const cEventInfo * cSchedule::GetEvent(unsigned short uEventID) const
{
	cEventInfo *pe = Events.First();
	while (pe != NULL)
	{
		if (pe->GetEventID() == uEventID)
			return pe;
			
		pe = Events.Next(pe);
	}
	
	return NULL;
}
/**  */
const cEventInfo * cSchedule::GetEvent(time_t tTime) const
{
	cEventInfo *pe = Events.First();
	while (pe != NULL)
	{
		if (pe->GetTime() == tTime)
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
		if (pEvent->GetTime() + pEvent->GetDuration() < tTime)
		{
			Events.Del(pEvent);
			a--;
		}
	}
}

/**  */
void cSchedule::Dump(FILE *f) const
{
   cChannel *channel = Channels.GetByServiceID(uServiceID);
   if (channel)
   {
      fprintf(f, "C %u %s\n", uServiceID, channel->name);
      for (cEventInfo *p = Events.First(); p; p = Events.Next(p))
         p->Dump(f);
      fprintf(f, "c\n");
   }
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
bool cSchedules::SetCurrentServiceID(unsigned short servid)
{
	pCurrentSchedule = GetSchedule(servid);
	if (pCurrentSchedule == NULL)
	{
		Add(new cSchedule(servid));
		pCurrentSchedule = GetSchedule(servid);
		if (pCurrentSchedule == NULL)
			return false;
	}
	
	uCurrentServiceID = servid;
	
	return true;
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
void cSchedules::Dump(FILE *f) const
{
   for (cSchedule *p = First(); p; p = Next(p))
      p->Dump(f);
}

// --- cEIT ------------------------------------------------------------------

#define DEC(N) dec << setw(N) << setfill(int('0'))
#define HEX(N) hex << setw(N) << setfill(int('0'))

#define EIT_STUFFING_DESCRIPTOR						0x42
#define EIT_LINKAGE_DESCRIPTOR						0x4a
#define EIT_SHORT_EVENT_DESCRIPTOR 					0x4d
#define EIT_EXTENDED_EVENT_DESCRIPTOR 				0x4e
#define EIT_TIME_SHIFTED_EVENT_DESCRIPTOR			0x4f
#define EIT_COMPONENT_DESCRIPTOR						0x50
#define EIT_CA_IDENTIFIER_DESCRIPTOR				0x53
#define EIT_CONTENT_DESCRIPTOR						0x54
#define EIT_PARENTAL_RATING_DESCRIPTOR				0x55
#define EIT_TELEPHONE_DESCRIPTOR						0x57
#define EIT_MULTILINGUAL_COMPONENT_DESCRIPTOR	0x5e
#define EIT_PRIVATE_DATE_SPECIFIER_DESCRIPTOR	0x5f
#define EIT_SHORT_SMOOTHING_BUFFER_DESCRIPTOR	0x61
#define EIT_DATA_BROADCAST_DESCRIPTOR				0x64
#define EIT_PDC_DESCRIPTOR								0x69

typedef struct eit_struct {
	u_char	table_id 						: 8;

#if BYTE_ORDER == BIG_ENDIAN
	u_char	section_syntax_indicator	: 1;
	u_char										: 3;
	u_char	section_length_hi 			: 4;
#else
	u_char	section_length_hi 			: 4;
	u_char										: 3;
	u_char	section_syntax_indicator	: 1;
#endif

	u_char	section_length_lo 			: 8;

	u_char	service_id_hi					: 8;
	u_char	service_id_lo					: 8;

#if BYTE_ORDER == BIG_ENDIAN
	u_char										: 2;
	u_char	version_number 				: 5;
	u_char	current_next_indicator		: 1;
#else
	u_char	current_next_indicator		: 1;
	u_char	version_number 				: 5;
	u_char										: 2;
#endif

	u_char	section_number 				: 8;
	u_char	last_section_number			: 8;
	u_char	transport_stream_id_hi		: 8;
	u_char	transport_stream_id_lo		: 8;
	u_char	original_network_id_hi		: 8;
	u_char	original_network_id_lo		: 8;
	u_char	segment_last_section_number	: 8;
	u_char	segment_last_table_id		: 8;
} eit_t;

typedef struct eit_loop_struct {
	u_char	event_id_hi 					: 8;
	u_char	event_id_lo 					: 8;

	u_char	date_hi							: 8;
	u_char	date_lo							: 8;
	u_char	time_hour						: 4;
	u_char	time_hour_ten					: 4;
	u_char	time_minute 					: 4;
	u_char	time_minute_ten				: 4;
	u_char	time_second 					: 4;
	u_char	time_second_ten				: 4;

	u_char	dur_hour 						: 4;
	u_char	dur_hour_ten					: 4;
	u_char	dur_minute						: 4;
	u_char	dur_minute_ten 				: 4;
	u_char	dur_second						: 4;
	u_char	dur_second_ten 				: 4;

#if BYTE_ORDER == BIG_ENDIAN
	u_char	running_status 				: 3;
	u_char	free_ca_mode					: 1;
	u_char	descriptors_loop_length_hi : 4;
#else
	u_char	descriptors_loop_length_hi : 4;
	u_char	free_ca_mode					: 1;
	u_char	running_status 				: 3;
#endif

	u_char	descriptors_loop_length_lo : 8;
} eit_loop_t;

typedef struct eit_short_event_struct {
	u_char	descriptor_tag 				: 8;
	u_char	descriptor_length 			: 8;

	u_char	language_code_1				: 8;
	u_char	language_code_2				: 8;
	u_char	language_code_3				: 8;

	u_char	event_name_length 			: 8;
} eit_short_event_t;

typedef struct eit_extended_event_struct {
	u_char	descriptor_tag 				: 8;
	u_char	descriptor_length 			: 8;

	u_char	last_descriptor_number		: 4;
	u_char	descriptor_number				: 4;

	u_char	language_code_1				: 8;
	u_char	language_code_2				: 8;
	u_char	language_code_3				: 8;

	u_char	length_of_items 				: 8;
} eit_extended_event_t;

typedef struct eit_content_descriptor {
	u_char	descriptor_tag					: 8;
	u_char	descriptor_length				: 8;
} eit_content_descriptor_t;

typedef struct eit_content_loop {
	u_char	content_nibble_level_2		: 4;
	u_char 	content_nibble_level_1		: 4;
	u_char	user_nibble_2					: 4;
	u_char	user_nibble_1					: 4;
} eit_content_loop_t;

class cEIT {
private:
   cSchedules *schedules;
public: 
	cEIT(void *buf, int length, cSchedules *Schedules);
	~cEIT();
  /**  */
  int ProcessEIT();

protected: // Protected methods
  /**  */
  int strdvbcpy(unsigned char *dst, unsigned char *src, int max);
  /** returns true if this EIT covers a
present/following information, false if it's
schedule information */
  bool IsPresentFollowing();
  /**  */
  bool WriteShortEventDescriptor(unsigned short service, eit_loop_t *eitloop, u_char *buf);
  /**  */
  bool WriteExtEventDescriptor(unsigned short service, eit_loop_t *eitloop, u_char *buf);
protected: // Protected attributes
  int buflen;
protected: // Protected attributes
  /**  */
  u_char buffer[4097];
  /** Table ID of this EIT struct */
  u_char tid;
  /** EITs service id (program number) */
  u_short pid;
};

cEIT::cEIT(void * buf, int length, cSchedules *Schedules)
{
	buflen = length < int(sizeof(buffer)) ? length : sizeof(buffer);
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, buf, buflen);
	tid = buffer[0];
   schedules = Schedules;
}

cEIT::~cEIT()
{
}

/**  */
int cEIT::ProcessEIT()
{
	int bufact = 0;
	eit_t *eit;
	eit_loop_t *eitloop;
	u_char tmp[256];

	if (bufact + (int)sizeof(eit_t) > buflen)
		return 0;
	eit = (eit_t *)buffer;
	bufact += sizeof(eit_t);	

	unsigned int service = (eit->service_id_hi << 8) | eit->service_id_lo;

	while(bufact + (int)sizeof(eit_loop_t) <= buflen)
	{	
		eitloop = (eit_loop_t *)&buffer[bufact];
		bufact += sizeof(eit_loop_t);

		int descdatalen = (eitloop->descriptors_loop_length_hi << 8) + eitloop->descriptors_loop_length_lo;
		int descdataact = 0;

		while (descdataact < descdatalen && bufact < buflen)
		{
			switch (buffer[bufact])
			{
				eit_content_descriptor_t *cont;
				eit_content_loop_t *contloop;

				case EIT_STUFFING_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_STUFFING_DESCRIPTOR");
					break;
	
				case EIT_LINKAGE_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_LINKAGE_DESCRIPTOR");
					break;
	
				case EIT_SHORT_EVENT_DESCRIPTOR:
					WriteShortEventDescriptor(service, eitloop, &buffer[bufact]);
					break;
		
				case EIT_EXTENDED_EVENT_DESCRIPTOR:
					WriteExtEventDescriptor(service, eitloop, &buffer[bufact]);
					break;

				case EIT_TIME_SHIFTED_EVENT_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_TIME_SHIFTED_EVENT_DESCRIPTOR");
					break;
	
				case EIT_COMPONENT_DESCRIPTOR	:
					strdvbcpy(tmp, &buffer[bufact + 8], buffer[bufact + 1] - 6);
					//dsyslog(LOG_INFO, "Found EIT_COMPONENT_DESCRIPTOR %c%c%c 0x%02x/0x%02x/0x%02x '%s'\n", buffer[bufact + 5], buffer[bufact + 6], buffer[bufact + 7], buffer[2], buffer[3], buffer[4], tmp);
					break;
	
				case EIT_CA_IDENTIFIER_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_CA_IDENTIFIER_DESCRIPTOR");
					break;
	
				case EIT_CONTENT_DESCRIPTOR	:
					cont = (eit_content_descriptor_t *)buffer;
					contloop = (eit_content_loop_t *)&buffer[sizeof(eit_content_descriptor_t)];
					//dsyslog(LOG_INFO, "Found EIT_CONTENT_DESCRIPTOR 0x%02x/0x%02x\n", contloop->content_nibble_level_1, contloop->content_nibble_level_2);
					break;
	
				case EIT_PARENTAL_RATING_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_PARENTAL_RATING_DESCRIPTOR");
					break;
	
				case EIT_TELEPHONE_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_TELEPHONE_DESCRIPTOR");
					break;
	
				case EIT_MULTILINGUAL_COMPONENT_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_MULTILINGUAL_COMPONENT_DESCRIPTOR");
					break;
	
				case EIT_PRIVATE_DATE_SPECIFIER_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_PRIVATE_DATE_SPECIFIER_DESCRIPTOR");
					break;
	
				case EIT_SHORT_SMOOTHING_BUFFER_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_SHORT_SMOOTHING_BUFFER_DESCRIPTOR");
					break;
	
				case EIT_DATA_BROADCAST_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_DATA_BROADCAST_DESCRIPTOR");
					break;
	
				case EIT_PDC_DESCRIPTOR	:
					//dsyslog(LOG_INFO, "Found EIT_PDC_DESCRIPTOR");
					break;
			
				default:
					//dsyslog(LOG_INFO, "Found unhandled descriptor 0x%02x with length of %04d\n", (int)buffer[bufact], (int)buffer[bufact + 1]);
					break;
			}
			descdataact += (buffer[bufact + 1] + 2);
			bufact += (buffer[bufact + 1] + 2);
		}
	}

	return 0;
}

/**  */
int cEIT::strdvbcpy(unsigned char *dst, unsigned char *src, int max)
{
	int a = 0;
	
	if (*src == 0x05 || (*src >= 0x20 && *src <= 0xff))
	{
		for (a = 0; a < max; a++)
		{
			if (*src == 0)
				break;
		
			if ((*src >= ' ' && *src <= '~') || (*src >= 0xa0 && *src <= 0xff))
				*dst++ = *src++;
			else
			{
				// if ((*src > '~' && *src < 0xa0) || *src == 0xff)
				// cerr << "found special character 0x" << HEX(2) << (int)*src << endl;
				src++;
			}
		}
		*dst = 0;
	}
	else
	{
		const char *ret;

		switch (*src)
		{
			case 0x01: ret = "Coding according to character table 1"; break;
			case 0x02: ret = "Coding according to character table 2"; break;
			case 0x03: ret = "Coding according to character table 3"; break;
			case 0x04: ret = "Coding according to character table 4"; break;
			case 0x10: ret = "Coding according to ISO/IEC 8859"; break;
			case 0x11: ret = "Coding according to ISO/IEC 10646"; break;
			case 0x12: ret = "Coding according to KSC 5601"; break;
			default: ret = "Unknown coding"; break;
		}
		strncpy((char *)dst, ret, max);
	}
	return a;
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

/**  */
bool cEIT::WriteShortEventDescriptor(unsigned short service, eit_loop_t *eitloop, u_char *buf)
{
	u_char tmp[256];
	eit_short_event_t *evt = (eit_short_event_t *)buf;
	unsigned short eventid = (unsigned short)((eitloop->event_id_hi << 8) | eitloop->event_id_lo);
	cEventInfo *pEvent;

	//isyslog(LOG_INFO, "Found Short Event Descriptor");
	
	cSchedule *pSchedule = (cSchedule *)schedules->GetSchedule(service);
	if (pSchedule == NULL)
	{
		schedules->Add(new cSchedule(service));
		pSchedule = (cSchedule *)schedules->GetSchedule(service);
		if (pSchedule == NULL)
			return false;
	}

   /* cSchedule::GetPresentEvent() and cSchedule::GetFollowingEvent() verify
      the temporal sanity of these events, so calling them here appears to
      be a bad idea... (kls 2000-11-01)
	//
	// if we are working on a present/following info, let's see whether
	// we already have present/following info for this service and if yes
	// check whether it's the same eventid, if yes, just return, nothing
	// left to do.
	//
	if (IsPresentFollowing())
	{
		if (eitloop->running_status == 4 || eitloop->running_status == 3)
			pEvent = (cEventInfo *)pSchedule->GetPresentEvent();
		else
			pEvent = (cEventInfo *)pSchedule->GetFollowingEvent();

		if (pEvent != NULL)
			if (pEvent->GetEventID() == eventid)
				return true;
	}
   */

	//
	// let's see whether we have that eventid already
	// in case not, we have to create a new cEventInfo for it
	//
	pEvent = (cEventInfo *)pSchedule->GetEvent(eventid);
	if (pEvent == NULL)
	{
		pSchedule->Events.Add(new cEventInfo(service, eventid));
		pEvent = (cEventInfo *)pSchedule->GetEvent(eventid);
		if (pEvent == NULL)
			return false;
		
		strdvbcpy(tmp, &buf[sizeof(eit_short_event_t)], evt->event_name_length);
		pEvent->SetTitle((char *)tmp);
		strdvbcpy(tmp, &buf[sizeof(eit_short_event_t) + evt->event_name_length + 1],
					 (int)buf[sizeof(eit_short_event_t) + evt->event_name_length]);
		pEvent->SetSubtitle((char *)tmp);
		cMJD mjd(eitloop->date_hi, eitloop->date_lo,
					eitloop->time_hour_ten * 10 + eitloop->time_hour,
					eitloop->time_minute_ten * 10 + eitloop->time_minute,
					eitloop->time_second_ten * 10 + eitloop->time_second);
		pEvent->SetTime(mjd.GetTime_t());
		pEvent->SetDuration((long)((long)((eitloop->dur_hour_ten * 10 + eitloop->dur_hour) * 60l * 60l) +
											(long)((eitloop->dur_minute_ten * 10 + eitloop->dur_minute) * 60l) +
											(long)(eitloop->dur_second_ten * 10 + eitloop->dur_second)));
	}
	
	if (IsPresentFollowing())
	{
		if (eitloop->running_status == 4 || eitloop->running_status == 3)
			pSchedule->SetPresentEvent(pEvent);
		else if (eitloop->running_status == 1 || eitloop->running_status == 2 || eitloop->running_status == 0)
			pSchedule->SetFollowingEvent(pEvent);
	}

	return true;
}

/**  */
bool cEIT::WriteExtEventDescriptor(unsigned short service, eit_loop_t *eitloop, u_char *buf)
{
	u_char tmp[256];
	eit_extended_event_t *evt = (eit_extended_event_t *)buf;
	int bufact, buflen;
	unsigned short eventid = (unsigned short)((eitloop->event_id_hi << 8) | eitloop->event_id_lo);
	cEventInfo *pEvent;
	
	//isyslog(LOG_INFO, "Found Extended Event Descriptor");
	
	cSchedule *pSchedule = (cSchedule *)schedules->GetSchedule(service);
	if (pSchedule == NULL)
	{
		schedules->Add(new cSchedule(service));
		pSchedule = (cSchedule *)schedules->GetSchedule(service);
		if (pSchedule == NULL)
			return false;
	}

	pEvent = (cEventInfo *)pSchedule->GetEvent(eventid);
	if (pEvent == NULL)
		return false;
	
	if (evt->descriptor_number != pEvent->GetExtendedDescriptorNumber())
		return false;
	
	bufact = sizeof(eit_extended_event_t);
	buflen = buf[1] + 2;
	
	if (evt->length_of_items > 0)
	{
		while (bufact - sizeof(eit_extended_event_t) < evt->length_of_items)
		{
			strdvbcpy(tmp, &buf[bufact + 1], (int)buf[bufact]);
			// could use value in tmp now to do something,
			// haven't seen any items as of yet transmitted from satellite
			bufact += (buf[bufact] + 1);
		}
	}
	
	strdvbcpy(tmp, &buf[bufact + 1], (int)buf[bufact]);
	if (pEvent->AddExtendedDescription((char *)tmp))
	{
		pEvent->IncreaseExtendedDescriptorNumber();
		return true;
	}
	
	return false;
}

// --- cSIProcessor ----------------------------------------------------------

#define MAX_FILTERS 20

int cSIProcessor::numSIProcessors = 0;
cSchedules *cSIProcessor::schedules = NULL;
cMutex cSIProcessor::schedulesMutex;

/**  */
cSIProcessor::cSIProcessor(const char *FileName)
{
   masterSIProcessor = numSIProcessors == 0; // the first one becomes the 'master'
	useTStime = false;
   filters = NULL;
   if ((fsvbi = open(FileName, O_RDONLY)) >= 0)
   {
      if (!numSIProcessors++) // the first one creates it
         schedules = new cSchedules;
	   filters = (SIP_FILTER *)calloc(MAX_FILTERS, sizeof(SIP_FILTER));
   }
   else
      LOG_ERROR_STR(FileName);
}

cSIProcessor::~cSIProcessor()
{
   if (fsvbi >= 0)
   {
      Cancel();
   	ShutDownFilters();
      delete filters;
      if (!--numSIProcessors) // the last one deletes it
         delete schedules;
      close(fsvbi);
   }
}

/** use the vbi device to parse all relevant SI
information and let the classes corresponding
to the tables write their information to the disk */
void cSIProcessor::Action()
{
   if (fsvbi < 0) {
      esyslog(LOG_ERR, "cSIProcessor::Action() called without open file - returning");
      return;
      }

   dsyslog(LOG_INFO, "EIT processing thread started (pid=%d)%s", getpid(), masterSIProcessor ? " - master" : "");
	
   unsigned char buf[4096+1]; // max. allowed size for any EIT section (+1 for safety ;-)
	unsigned int seclen;
	unsigned int pid;
   time_t lastCleanup = time(NULL);
   time_t lastDump = time(NULL);
	struct pollfd pfd;
	
	while(true)
	{
      if (masterSIProcessor)
      {
         time_t now = time(NULL);
         struct tm *ptm = localtime(&now);
         if (now - lastCleanup > 3600 && ptm->tm_hour == 5)
         {
            LOCK_THREAD;

            schedulesMutex.Lock();
            isyslog(LOG_INFO, "cleaning up schedules data");
            schedules->Cleanup();
            schedulesMutex.Unlock();
            lastCleanup = now;
         }
         if (now - lastDump > 600)
         {
            LOCK_THREAD;

            schedulesMutex.Lock();
            FILE *f = fopen(AddDirectory(VideoDirectory, "epg.data"), "w");
            if (f) {
               schedules->Dump(f);
               fclose(f);
               }
            lastDump = now;
            schedulesMutex.Unlock();
         }
      }

		/* wait data become ready from the bitfilter */
		pfd.fd = fsvbi;
		pfd.events = POLLIN;
		if(poll(&pfd, 1, 1000) != 0) /* timeout is 5 secs */
		{
			// fprintf(stderr, "<data>\n");
			/* read section */
			read(fsvbi, buf, 8);
			seclen = (buf[6] << 8) | buf[7];
			pid = (buf[4] << 8) | buf[5];
			read(fsvbi, buf, seclen);

			//dsyslog(LOG_INFO, "Received pid 0x%02x with table ID 0x%02x and length of %04d\n", pid, buf[0], seclen);

			switch (pid)
			{
				case 0x14:
					if (buf[0] == 0x70)
					{
						if (useTStime)
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
                  LOCK_THREAD;

                  schedulesMutex.Lock();
	               cEIT ceit(buf, seclen, schedules);
						ceit.ProcessEIT();
                  schedulesMutex.Unlock();
					}
					else
						dsyslog(LOG_INFO, "Received stuffing section in EIT\n");
					break;
				
				default:
					break;
			}
		}
		else
		{
         LOCK_THREAD;

         //XXX this comes pretty often
			//isyslog(LOG_INFO, "Received timeout from poll, refreshing filters\n");
			RefreshFilters();
		}
//		WakeUp();
	}
}

/** Add a filter with packet identifier pid and
table identifer tid */
bool cSIProcessor::AddFilter(u_char pid, u_char tid)
{
   if (fsvbi < 0)
      return false;

	int section = ((int)tid << 8) | 0x00ff;
	
	struct bitfilter filt = {
		pid,
		{ section, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
		  SECTION_CONTINUOS, 0,
		  FILTER_MEM,
		  {},
		};

	if (ioctl(fsvbi, VIDIOCSBITFILTER, &filt) < 0)
		return false;
		
	for (int a = 0; a < MAX_FILTERS; a++)
	{
		if (filters[a].inuse == false)
		{
			filters[a].pid = pid;
			filters[a].tid = tid;
			filters[a].handle = filt.handle;
			filters[a].inuse = true;
			// dsyslog(LOG_INFO, "  Registered filter handle %04x, pid = %02d, tid = %02d", filters[a].handle, filters[a].pid, filters[a].tid);
			return true;
		}
	}
	
	return false;
}

/** set whether local systems time should be
set by the received TDT or TOT packets */
bool cSIProcessor::SetUseTSTime(bool use)
{
	useTStime = use;
	return useTStime;
}

/**  */
bool cSIProcessor::ShutDownFilters()
{
   if (fsvbi < 0)
      return false;

	bool ret = true;
	
	for (int a = 0; a < MAX_FILTERS; a++)
	{
		if (filters[a].inuse == true)
		{
			if (ioctl(fsvbi, VIDIOCSSHUTDOWNFILTER, &filters[a].handle) < 0)
				ret = false;

			// dsyslog(LOG_INFO, "Deregistered filter handle %04x, pid = %02d, tid = %02d", filters[a].handle, filters[a].pid, filters[a].tid);
			
			filters[a].inuse = false;
		}
	}

	return ret;
}

/** */
bool cSIProcessor::SetCurrentServiceID(unsigned short servid)
{
  LOCK_THREAD;
  return schedules ? schedules->SetCurrentServiceID(servid) : false;
}

/**  */
bool cSIProcessor::RefreshFilters()
{
   if (fsvbi < 0)
      return false;

	bool ret = true;
	
	ret = ShutDownFilters();
	
	for (int a = 0; a < MAX_FILTERS; a++)
	{
		if (filters[a].inuse == false && filters[a].pid != 0 && filters[a].tid != 0)
		{
			if (!AddFilter(filters[a].pid, filters[a].tid))
				ret = false;
		}
	}

	return ret;
}
