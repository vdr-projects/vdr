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
 * $Id: eit.c 1.4 2000/10/01 14:09:05 kls Exp $
 ***************************************************************************/

#include "eit.h"
#include <iostream.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <dvb_comcode.h>
#include "tools.h"

typedef struct {
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

#define EIT_SIZE 14

struct eit_loop_struct1 {
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

	u_char	dur_hour_ten					: 4;
	u_char	dur_hour 						: 4;
	u_char	dur_minute_ten 				: 4;
	u_char	dur_minute						: 4;
	u_char	dur_second_ten 				: 4;
	u_char	dur_second						: 4;

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
};

#define EIT_SHORT_EVENT_DESCRIPTOR 0x4d
#define EIT_SHORT_EVENT_DESCRIPTOR_SIZE 6

struct eit_short_event_descriptor_struct {
	u_char	descriptor_tag 				: 8;
	u_char	descriptor_length 			: 8;

	u_char	language_code_1				: 8;
	u_char	language_code_2				: 8;
	u_char	language_code_3				: 8;

	u_char	event_name_length 			: 8;
};

#define EIT_EXTENDED_EVENT_DESCRIPOR 0x4e

#define EIT_DESCRIPTOR_SIZE

typedef struct eit_event_struct {
	u_char	event_id_hi						: 8;
	u_char	event_id_lo						: 8;

	u_char	start_time_1					: 8;
	u_char	start_time_2					: 8;
	u_char	start_time_3					: 8;
	u_char	start_time_4					: 8;
	u_char	start_time_5					: 8;

	u_char	duration_1						: 8;
	u_char	duration_2						: 8;
	u_char	duration_3						: 8;

#if BYTE_ORDER == BIG_ENDIAN
	u_char	running_status 				: 3;
	u_char	free_CA_mode					: 1;
	u_char	descriptors_loop_length_hi : 4;
#else
	u_char	descriptors_loop_length_hi : 4;
	u_char	free_CA_mode					: 1;
	u_char	running_status 				: 3;
#endif

	u_char	descriptors_loop_length_lo : 8;

} eit_event_t;
#define EIT_LOOP_SIZE 12


typedef struct tot_t {
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

	u_char	date_hi							: 8;
	u_char	date_lo							: 8;
	u_char	time_hour						: 4;
	u_char	time_hour_ten					: 4;
	u_char	time_minute						: 4;
	u_char	time_minute_ten				: 4;
	u_char	time_second						: 4;
	u_char	time_second_ten				: 4;

#if BYTE_ORDER == BIG_ENDIAN
	u_char										: 4;
	u_char	descriptor_loop_length_hi 	: 4;
#else
	u_char	descriptor_loop_length_hi 	: 4;
	u_char										: 4;
#endif

	u_char	descriptor_loop_length_lo 	: 8;	
} tot_t;

typedef struct local_time_offset {

	u_char	descriptor_tag 				: 8;
	u_char	descriptor_length 			: 8;

	u_char	language_code_1				: 8;
	u_char	language_code_2				: 8;
	u_char	language_code_3				: 8;

	u_char										: 8;
	
	u_char	offset_hour						: 4;
	u_char	offset_hour_ten				: 4;
	u_char	offset_minute					: 4;
	u_char	offset_minute_ten				: 4;

	u_char	change_date_hi					: 8;
	u_char	change_date_lo					: 8;
	u_char	change_time_hour				: 4;
	u_char	change_time_hour_ten			: 4;
	u_char	change_time_minute			: 4;
	u_char	change_time_minute_ten		: 4;
	u_char	change_time_second			: 4;
	u_char	change_time_second_ten		: 4;

	u_char	next_offset_hour				: 4;
	u_char	next_offset_hour_ten			: 4;
	u_char	next_offset_minute			: 4;
	u_char	next_offset_minute_ten		: 4;
} local_time_offset;

cEIT::cEIT()
{
	cszBitFilter = "/dev/vbi";
	if((fsvbi = open(cszBitFilter, O_RDWR))<0)
	{
		fsvbi = 0;
		esyslog(LOG_ERR, "Failed to open DVB bitfilter device: %s", cszBitFilter);
		return;
	}
}

cEIT::~cEIT()
{
	if (fsvbi != 0)
		close(fsvbi);
	fsvbi = 0;
}

/** Set the bitfilter in vbi device to return
correct tables */
int cEIT::SetBitFilter(unsigned short pid, unsigned short section, unsigned short mode)
{
	struct bitfilter filt = {
		pid,
		{ section, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
		  mode,0,
		  FILTER_MEM,
		  {},
		};

	if (ioctl(fsvbi, VIDIOCSBITFILTER, &filt) < 0)
		return 0xffff;
	return 0;
}
/**  */
int cEIT::GetSection(unsigned char *buf, ushort PID, unsigned char sec)
{
	int seclen=0;
	unsigned short handle, pid;
	unsigned char section, sectionnum=0xff, maxsec=0;
		
	if ((handle = SetBitFilter(PID, (sec<<8)|0x00ff, SECTION_CONTINUOS))==0xffff)
		return -1;
		
	seclen=0;
	if (!cFile::AnyFileReady(fsvbi, 20000))
	{
		//cerr << "Timeout\n";
		return -1;
	}
	
	read(fsvbi, buf, 8);
	seclen=(buf[6]<<8)|buf[7];
	pid=(buf[4]<<8)|buf[5];
	
	read(fsvbi, buf, seclen);
	section=buf[0];
	sectionnum=buf[6];
	maxsec=buf[7];
	
	//cerr << "secnum: " << HEX(2) << (int)sectionnum
	//     << ", secmax: " << HEX(2) << (int) msecnum << "\n";
	
	CloseFilter(handle);
	
	return seclen;
}

/**  */
int cEIT::CloseFilter(unsigned short handle)
{
	if (ioctl(fsvbi, VIDIOCSSHUTDOWNFILTER, &handle)<0)
		return -1;
	return 0;
}

/**  */
char * cEIT::mjd2string(unsigned short mjd)
{
	int y, m, d, k;
	static char buf[20];
	
	y = (int) ((mjd - 15078.2) / 365.25);
	m = (int) ((mjd - 14956.1 - (int)(y * 365.25)) / 30.6001);
	d = (int) (mjd - 14956 - (int)(y * 365.25) - (int)(m * 30.6001));
	k = (m == 14 || m == 15) ? 1 : 0;
	y = y + k;
	m = m - 1 - k * 12;
	sprintf(buf, "%d.%d.%4d", d, m, y + 1900);

	return(buf);
}

/**  */
int cEIT::GetEIT()
{
	unsigned char buf[4096+1]; // max. allowed size for any EIT section (+1 for safety ;-)
	eit_t *eit;
	struct eit_loop_struct1 *eitloop;
	struct eit_short_event_descriptor_struct *eitevt;
	unsigned int seclen;
	unsigned short handle, pid;
	eit_event * pevt = (eit_event *)0;
	time_t tstart;
	
	if ((handle = SetBitFilter(0x12, (0x4e << 8) | 0x00ff, SECTION_CONTINUOS))==0xffff)
	{
		return -1;
	}
/*	
	pid_t process = fork();
	if (process < 0)
	{
		cerr << "GetEIT -1" << endl;
		return -1;
	}
	
	if (process != 0)
	{
		cerr << "GetEIT 0" << endl;
		return 0;
	}
*/	
	int nReceivedEITs = 0;
	tstart = time(NULL);
	while ((!evtRunning.bIsValid || !evtNext.bIsValid) && nReceivedEITs < 20 && difftime(time(NULL), tstart) < 4)
	{
	        if (!cFile::AnyFileReady(fsvbi, 5000))
		{
			//cerr << "Timeout\n";
			CloseFilter(handle);
			return -1;
		}
	
		read(fsvbi, buf, 8);
		seclen=(buf[6]<<8)|buf[7];
		pid=(buf[4]<<8)|buf[5];
	
                if (seclen >= sizeof(buf))
                   seclen = sizeof(buf) - 1;
		read(fsvbi, buf, seclen);
	
		if (seclen < (int)(sizeof(eit_t)
							  + sizeof(struct eit_loop_struct1)
							  + sizeof(struct eit_short_event_descriptor_struct)))
			continue;
	
		eit = (eit_t *)buf;
		eitloop = (struct eit_loop_struct1 *)&eit[1];
		eitevt = (struct eit_short_event_descriptor_struct *)&eitloop[1];
	
		if (eitevt->descriptor_tag != EIT_SHORT_EVENT_DESCRIPTOR)
		{
			// printf("Tag = '%c'\n", eitevt->descriptor_tag);
			continue;
		}
	
		if (((eit->service_id_hi << 8) | eit->service_id_lo) != uProgramNumber)
		{
			// printf("Wrong program %04x need %04x\n", (eit->service_id_hi << 8) | eit->service_id_lo, uProgramNumber);
			continue;
		}
		
		nReceivedEITs++;
	
		pevt = (eit_event *)0;
		if (eitloop->running_status == 4 | eitloop->running_status == 3)
			pevt = (eit_event *)&evtRunning;
		else if (eitloop->running_status == 1 || eitloop->running_status == 2 || eitloop->running_status == 0)
			pevt = (eit_event *)&evtNext;

		if (pevt)
		{
			unsigned char *p = (unsigned char *)&eitevt[1];
			strdvbcpy((unsigned char *)pevt->szTitle, p, eitevt->event_name_length);
			pevt->szSubTitle[0] = 0;
			strdvbcpy((unsigned char *)pevt->szSubTitle, &p[eitevt->event_name_length+1], (int)p[eitevt->event_name_length]);
			strcpy(pevt->szDate, mjd2string((eitloop->date_hi << 8) + eitloop->date_lo));
			int hr = eitloop->time_hour + (eitloop->time_hour_ten * 10);
			hr += 2;
			if (hr >=24)
			{
				hr -= 24;
				// need to switch date one day ahead here
			}
			sprintf(pevt->szTime, "%d:%c%c", hr,
														eitloop->time_minute_ten + '0',
														eitloop->time_minute + '0');
			pevt->bIsValid = true;
		}
	}

	CloseFilter(handle);	

	return 1;
}

/**  */
int cEIT::SetProgramNumber(unsigned short pnr)
{
	if (pnr == 0)
	{
		evtRunning.bIsValid = false;
		evtNext.bIsValid = false;
		return -1;
	}

	if (pnr != uProgramNumber)
	{
		evtRunning.bIsValid = false;
		evtNext.bIsValid = false;
		uProgramNumber = pnr;
	}
	return 1;
}

/** retrieves the string for the running title */
char * cEIT::GetRunningTitle()
{
	if (evtRunning.bIsValid)
		return evtRunning.szTitle;
	else
		return "---";
}
/** Retrieves the string for the running subtitle */
char * cEIT::GetRunningSubtitle()
{
	if (evtRunning.bIsValid)
		return evtRunning.szSubTitle;
	else
		return "---";
}
/** Retrieves the string representing the
date of the current event
 */
char * cEIT::GetRunningDate()
{
	if (evtRunning.bIsValid)
		return evtRunning.szDate;
	else
		return "---";
}
/** Retrieves the string representing the
time of the current event */
char * cEIT::GetRunningTime()
{
	if (evtRunning.bIsValid)
		return evtRunning.szTime;
	else
		return "---";
}
/** retrieves the string for the running title */
char * cEIT::GetNextTitle()
{
	if (evtNext.bIsValid)
		return evtNext.szTitle;
	else
		return "---";
}
/** Retrieves the string for the running subtitle */
char * cEIT::GetNextSubtitle()
{
	if (evtNext.bIsValid)
		return evtNext.szSubTitle;
	else
		return "---";
}
/** Retrieves the string representing the
date of the current event
 */
char * cEIT::GetNextDate()
{
	if (evtNext.bIsValid)
		return evtNext.szDate;
	else
		return "---";
}
/** Retrieves the string representing the
time of the current event */
char * cEIT::GetNextTime()
{
	if (evtNext.bIsValid)
		return evtNext.szTime;
	else
		return "---";
}

/**  */
bool cEIT::IsValid()
{
	GetEIT();
	return (evtRunning.bIsValid && evtNext.bIsValid);
}

/**  */
int cEIT::strdvbcpy(unsigned char *dst, unsigned char *src, int max)
{
	int a;
	for (a = 0; a < max; a++)
	{
		if (*src == 0)
			break;
		
		if ((*src >= ' ' && *src <= '~') || (*src >= 0xa0 && *src <= 0xff))
			*dst++ = *src++;
		else
			src++;
	}
	*dst = 0;
	return a;
}
