/***************************************************************************
                          eit.h  -  description
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
 * $Id: eit.h 1.29 2003/05/17 09:15:56 kls Exp $
 ***************************************************************************/

#ifndef __EIT_H
#define __EIT_H

#include "channels.h"
#include "thread.h"
#include "tools.h"

#define MAXEPGBUGFIXLEVEL 2

class cEventInfo : public cListObject {
  friend class cSchedule;
  friend class cEIT;
private:
  unsigned char uTableID;           // Table ID this event came from
  tChannelID channelID;             // Channel ID of program for that event
  bool bIsFollowing;                // true if this is the next event on this channel
  bool bIsPresent;                  // true if this is the present event running
  char *pExtendedDescription;       // Extended description of this event
  char *pSubtitle;                  // Subtitle of event
  char *pTitle;                     // Title of event
  unsigned short uEventID;          // Event ID of this event
  long lDuration;                   // duration of event in seconds
  time_t tTime;                     // Start time
  int nChannelNumber;               // the actual channel number from VDR's channel list (used in cMenuSchedule for sorting by channel number)
protected:
  void SetTableID(unsigned char tableid);
  void SetFollowing(bool foll);
  void SetPresent(bool pres);
  void SetTitle(const char *string);
  void SetChannelID(tChannelID channelid);
  void SetEventID(unsigned short evid);
  void SetDuration(long l);
  void SetTime(time_t t);
  void SetExtendedDescription(const char *string);
  void SetSubtitle(const char *string);
  cEventInfo(tChannelID channelid, unsigned short eventid);
public:
  ~cEventInfo();
  const unsigned char GetTableID(void) const;
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
  tChannelID GetChannelID(void) const;
  int GetChannelNumber(void) const { return nChannelNumber; }
  void SetChannelNumber(int ChannelNumber) const { ((cEventInfo *)this)->nChannelNumber = ChannelNumber; } // doesn't modify the EIT data, so it's ok to make it 'const'
  void Dump(FILE *f, const char *Prefix = "") const;
  static bool Read(FILE *f, cSchedule *Schedule);
  void FixEpgBugs(void);
  };

class cSchedule : public cListObject  {
  friend class cSchedules;
  friend class cEIT;
private:
  cEventInfo *pPresent;
  cEventInfo *pFollowing;
  tChannelID channelID;
  cList<cEventInfo> Events;
protected:
  void SetChannelID(tChannelID channelid);
  bool SetFollowingEvent(cEventInfo *pEvent);
  bool SetPresentEvent(cEventInfo *pEvent);
  void Cleanup(time_t tTime);
  void Cleanup(void);
  cSchedule(tChannelID channelid = tChannelID::InvalidID);
public:
  ~cSchedule();
  cEventInfo *AddEvent(cEventInfo *EventInfo);
  const cEventInfo *GetPresentEvent(void) const;
  const cEventInfo *GetFollowingEvent(void) const;
  tChannelID GetChannelID(void) const;
  const cEventInfo *GetEvent(unsigned short uEventID, time_t tTime = 0) const;
  const cEventInfo *GetEventAround(time_t tTime) const;
  const cEventInfo *GetEventNumber(int n) const { return Events.Get(n); }
  int NumEvents(void) const { return Events.Count(); }
  void Dump(FILE *f, const char *Prefix = "") const;
  static bool Read(FILE *f, cSchedules *Schedules);
  };

class cSchedules : public cList<cSchedule> {
  friend class cSchedule;
  friend class cSIProcessor;
private:
  const cSchedule *pCurrentSchedule;
  tChannelID currentChannelID;
protected:
  const cSchedule *AddChannelID(tChannelID channelid);
  const cSchedule *SetCurrentChannelID(tChannelID channelid);
  void Cleanup();
public:
  cSchedules(void);
  ~cSchedules();
  const cSchedule *GetSchedule(tChannelID channelid) const;
  const cSchedule *GetSchedule(void) const;
  void Dump(FILE *f, const char *Prefix = "") const;
  static bool Read(FILE *f);
};

typedef struct sip_filter {

  unsigned short pid;
  u_char tid;
  int handle;
  bool inuse;

}SIP_FILTER;

class cCaDescriptor;

class cSIProcessor : public cThread {
private:
  static int numSIProcessors;
  static cSchedules *schedules;
  static cMutex schedulesMutex;
  static cList<cCaDescriptor> caDescriptors;
  static cMutex caDescriptorsMutex;
  static const char *epgDataFileName;
  static time_t lastDump;
  bool masterSIProcessor;
  int currentSource;
  int currentTransponder;
  int statusCount;
  int pmtIndex;
  int pmtPid;
  SIP_FILTER *filters;
  char *fileName;
  bool active;
  void Action(void);
  bool AddFilter(unsigned short pid, u_char tid, u_char mask = 0xFF);
  bool DelFilter(unsigned short pid, u_char tid);
  bool ShutDownFilters(void);
  void NewCaDescriptor(struct Descriptor *d, int ServiceId);
public:
  cSIProcessor(const char *FileName);
  ~cSIProcessor();
  static void SetEpgDataFileName(const char *FileName);
  static const char *GetEpgDataFileName(void);
  static const cSchedules *Schedules(cMutexLock &MutexLock);
         // Caller must provide a cMutexLock which has to survive the entire
         // time the returned cSchedules is accessed. Once the cSchedules is no
         // longer used, the cMutexLock must be destroyed.
  static int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data);
         ///< Gets all CA descriptors for a given channel.
         ///< Copies all available CA descriptors for the given Source, Transponder and ServiceId
         ///< into the provided buffer at Data (at most BufSize bytes). Only those CA descriptors
         ///< are copied that match one of the given CA system IDs.
         ///< \return Returns the number of bytes copied into Data (0 if no CA descriptors are
         ///< available), or -1 if BufSize was too small to hold all CA descriptors.
  static bool Read(FILE *f = NULL);
  static void Clear(void);
  void SetStatus(bool On);
  void SetCurrentTransponder(int CurrentSource, int CurrentTransponder);
  static bool SetCurrentChannelID(tChannelID channelid);
  static void TriggerDump(void);
  };

#endif
