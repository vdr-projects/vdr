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
 * $Id: eit.h 1.8 2001/08/11 09:06:17 kls Exp $
 ***************************************************************************/

#ifndef __EIT_H
#define __EIT_H

#include "thread.h"
#include "tools.h"

class cEventInfo : public cListObject {
  friend class cSchedule;
  friend class cEIT;
private:
  unsigned short uServiceID;        // Service ID of program for that event
  bool bIsFollowing;                // true if this is the next event on this channel
  bool bIsPresent;                  // true if this is the present event running
  char *pExtendedDescription;       // Extended description of this event
  char *pSubtitle;                  // Subtitle of event
  char *pTitle;                     // Title of event
  unsigned short uEventID;          // Event ID of this event
  long lDuration;                   // duration of event in seconds
  time_t tTime;                     // Start time
  u_char cExtendedDescriptorNumber; // current extended descriptor number that has to be inserted
  int nChannelNumber;               // the actual channel number from VDR's channel list (used in cMenuSchedule for sorting by channel number)
protected:
  void SetFollowing(bool foll);
  void SetPresent(bool pres);
  bool SetTitle(char *string);
  void SetServiceID(unsigned short servid);
  void SetEventID(unsigned short evid);
  void SetDuration(long l);
  void SetTime(time_t t);
  bool AddExtendedDescription(char *string);
  bool SetSubtitle(char *string);
  void IncreaseExtendedDescriptorNumber(void);
  cEventInfo(unsigned short serviceid, unsigned short eventid);
public:
  ~cEventInfo();
  const char *GetTimeString(void) const;
  const char *GetEndTimeString(void) const;
  const char *GetDate(void) const;
  bool IsFollowing(void) const;
  bool IsPresent(void) const;
  const char *GetExtendedDescription(void) const;
  const char *GetSubtitle(void) const;
  const char *GetTitle(void) const;
  unsigned short GetEventID(void) const;
  long GetDuration(void) const;
  time_t GetTime(void) const;
  u_char GetExtendedDescriptorNumber(void) const;
  unsigned short GetServiceID(void) const;
  int GetChannelNumber(void) const { return nChannelNumber; }
  void SetChannelNumber(int ChannelNumber) const { ((cEventInfo *)this)->nChannelNumber = ChannelNumber; } // doesn't modify the EIT data, so it's ok to make it 'const'
  void Dump(FILE *f, const char *Prefix = "") const;
  };

class cSchedule : public cListObject  {
  friend class cSchedules;
  friend class cEIT;
private:
  cEventInfo *pPresent;
  cEventInfo *pFollowing;
  unsigned short uServiceID;
  cList<cEventInfo> Events;
protected:
  void SetServiceID(unsigned short servid);
  bool SetFollowingEvent(cEventInfo *pEvent);
  bool SetPresentEvent(cEventInfo *pEvent);
  void Cleanup(time_t tTime);
  void Cleanup(void);
  cSchedule(unsigned short servid = 0);
public:
  ~cSchedule();
  const cEventInfo *GetPresentEvent(void) const;
  const cEventInfo *GetFollowingEvent(void) const;
  unsigned short GetServiceID(void) const;
  const cEventInfo *GetEvent(unsigned short uEventID) const;
  const cEventInfo *GetEvent(time_t tTime) const;
  const cEventInfo *GetEventNumber(int n) const { return Events.Get(n); }
  int NumEvents(void) const { return Events.Count(); }
  void Dump(FILE *f, const char *Prefix = "") const;
  };

class cSchedules : public cList<cSchedule> {
  friend class cSIProcessor;
private:
  const cSchedule *pCurrentSchedule;
  unsigned short uCurrentServiceID;
protected:
  bool SetCurrentServiceID(unsigned short servid);
  void Cleanup();
public:
  cSchedules(void);
  ~cSchedules();
  const cSchedule *GetSchedule(unsigned short servid) const;
  const cSchedule *GetSchedule(void) const;
  void Dump(FILE *f, const char *Prefix = "") const;
};

typedef struct sip_filter {

  u_char pid;
  u_char tid;
  int handle;
  bool inuse;

}SIP_FILTER;

class cSIProcessor : public cThread {
private:
  static int numSIProcessors;
  static cSchedules *schedules;
  static cMutex schedulesMutex;
  static const char *epgDataFileName;
  bool masterSIProcessor;
  bool useTStime;
  SIP_FILTER *filters;
  char *fileName;
  bool active;
  void Action(void);
  bool AddFilter(u_char pid, u_char tid);
  bool ShutDownFilters(void);
public:
  cSIProcessor(const char *FileName);
  ~cSIProcessor();
  static void SetEpgDataFileName(const char *FileName);
  static const char *GetEpgDataFileName(void);
  void SetStatus(bool On);
  bool SetUseTSTime(bool use);
  bool SetCurrentServiceID(unsigned short servid);
  const cSchedules *Schedules(void) { return schedules; }
  };

#endif
