/***************************************************************************
                          eit.h  -  description
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
 * $Id: eit.h 1.1 2000/09/03 10:23:24 kls Exp $
 ***************************************************************************/

#ifndef __EIT_H
#define __EIT_H

#include <dvb_v4l.h>

typedef struct eit_event {

	bool	bIsValid;
	char	szTitle[512];
	char	szSubTitle[512];
	char	szDate[12];
	char	szTime[12];

}eit_event;

/**
  *@author Robert Schneider
  */

class cEIT {
public: 
	cEIT();
	~cEIT();
  /**  */
  int GetEIT();
  /**  */
  int SetProgramNumber(unsigned short pnr);
  /** Retrieves the string representing the time of the current event */
  char * GetRunningTime();
  /** Retrieves the string representing the date of the current event */
  char * GetRunningDate();
  /** Retrieves the string for the running subtitle */
  char * GetRunningSubtitle();
  /** retrieves the string for the running title */
  char * GetRunningTitle();
  /** Retrieves the string representing the time of the next event */
  char * GetNextTime();
  /** Retrieves the string representing the date of the next event */
  char * GetNextDate();
  /** Retrieves the string for the next subtitle */
  char * GetNextSubtitle();
  /** retrieves the string for the next title */
  char * GetNextTitle();
  /**  */
  bool IsValid();

protected: // Protected attributes
  /** Device name of VBI device */
  const char * cszBitFilter;
protected: // Protected attributes
  /** handle to VBI device (usually /dev/vbi) */
  int fsvbi;
  /** Describes the event next on */
  eit_event evtNext;
  /** Describes the running event */
  eit_event evtRunning;
protected: // Protected methods
  /** Set the bitfilter in vbi device to return
correct tables */
  int SetBitFilter(unsigned short pid, unsigned short section, unsigned short mode);
  /**  */
  int GetSection(unsigned char *buf, ushort PID, unsigned char sec);
  /**  */
  int CloseFilter(unsigned short handle);
  /**  */
  char * mjd2string(unsigned short mjd);
  /**  */
  int strdvbcpy(unsigned char *dst, unsigned char *src, int max);
public: // Public attributes
  /**  */
  unsigned short uProgramNumber;
};

#endif
