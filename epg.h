/*
 * epg.h: Electronic Program Guide
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version (as used in VDR before 1.3.0) written by
 * Robert Schneider <Robert.Schneider@web.de> and Rolf Hakenes <hakenes@hippomi.de>.
 *
 * $Id: epg.h 5.7 2024/10/08 08:09:48 kls Exp $
 */

#ifndef __EPG_H
#define __EPG_H

#include "channels.h"
#include "libsi/section.h"
#include "thread.h"
#include "tools.h"

#define MAXEPGBUGFIXLEVEL 3

#define EPG_LINGER_TIME (max(Setup.EPGLinger, 180) * 60) // seconds to keep old EPG data (internal, must be at least Setup.EPGLinger)

enum { MaxEventContents = 4 };

enum eEventContentGroup {
  ecgMovieDrama               = 0x10,
  ecgNewsCurrentAffairs       = 0x20,
  ecgShow                     = 0x30,
  ecgSports                   = 0x40,
  ecgChildrenYouth            = 0x50,
  ecgMusicBalletDance         = 0x60,
  ecgArtsCulture              = 0x70,
  ecgSocialPoliticalEconomics = 0x80,
  ecgEducationalScience       = 0x90,
  ecgLeisureHobbies           = 0xA0,
  ecgSpecial                  = 0xB0,
  ecgUserDefined              = 0xF0
  };

enum eDumpMode { dmAll, dmPresent, dmFollowing, dmAtTime };

struct tComponent {
  uchar stream;
  uchar type;
  char language[MAXLANGCODE2];
  char *description;
  cString ToString(void);
  bool FromString(const char *s);
  };

class cComponents {
private:
  int numComponents;
  tComponent *components;
  bool Realloc(int Index);
public:
  cComponents(void);
  ~cComponents(void);
  int NumComponents(void) const { return numComponents; }
  void SetComponent(int Index, const char *s);
  void SetComponent(int Index, uchar Stream, uchar Type, const char *Language, const char *Description);
  tComponent *Component(int Index) const { return (Index < numComponents) ? &components[Index] : NULL; }
  tComponent *GetComponent(int Index, uchar Stream, uchar Type); // Gets the Index'th component of Stream and Type, skipping other components
                                                                 // In case of an audio stream the 'type' check actually just distinguishes between "normal" and "Dolby Digital"
  };

class cSchedule;

typedef u_int32_t tEventID;

class cEvent : public cListObject {
  friend class cSchedule;
private:
  static cMutex numTimersMutex; // Protects numTimers, because it might be accessed from parallel read locks
  // The sequence of these parameters is optimized for minimal memory waste!
  cSchedule *schedule;     // The Schedule this event belongs to
  mutable u_int16_t numTimers;// The number of timers that use this event
  tEventID eventID;        // Event ID of this event
  uchar tableID;           // Table ID this event came from
  uchar version;           // Version number of section this event came from
  uchar runningStatus;     // 0=undefined, 1=not running, 2=starts in a few seconds, 3=pausing, 4=running
  uchar parentalRating;    // Parental rating of this event
  char *title;             // Title of this event
  char *shortText;         // Short description of this event (typically the episode name in case of a series)
  char *description;       // Description of this event
  cComponents *components; // The stream components of this event
  time_t startTime;        // Start time of this event
  int duration;            // Duration of this event in seconds
  uchar contents[MaxEventContents]; // Contents of this event
  time_t vps;              // Video Programming Service timestamp (VPS, aka "Programme Identification Label", PIL)
  time_t seen;             // When this event was last seen in the data stream
  char *aux;               // Auxiliary data, for use with plugins
public:
  cEvent(tEventID EventID);
  ~cEvent();
  virtual int Compare(const cListObject &ListObject) const;
  tChannelID ChannelID(void) const;
  const cSchedule *Schedule(void) const { return schedule; }
  tEventID EventID(void) const { return eventID; }
  uchar TableID(void) const { return tableID; }
  uchar Version(void) const { return version; }
  int RunningStatus(void) const { return runningStatus; }
  const char *Title(void) const { return title; }
  const char *ShortText(void) const { return shortText; }
  const char *Description(void) const { return description; }
  const cComponents *Components(void) const { return components; }
  uchar Contents(int i = 0) const { return (0 <= i && i < MaxEventContents) ? contents[i] : uchar(0); }
  int ParentalRating(void) const { return parentalRating; }
  time_t StartTime(void) const { return startTime; }
  time_t EndTime(void) const { return startTime + duration; }
  int Duration(void) const { return duration; }
  time_t Vps(void) const { return vps; }
  time_t Seen(void) const { return seen; }
  bool SeenWithin(int Seconds) const { return time(NULL) - seen < Seconds; }
  const char *Aux(void) const { return aux; }
  void IncNumTimers(void) const;
  void DecNumTimers(void) const;
  bool HasTimer(void) const { return numTimers > 0; }
  bool IsRunning(bool OrAboutToStart = false) const;
  static const char *ContentToString(uchar Content);
  cString GetParentalRatingString(void) const;
  cString GetDateString(void) const;
  cString GetTimeString(void) const;
  cString GetEndTimeString(void) const;
  cString GetVpsString(void) const;
  void SetEventID(tEventID EventID);
  void SetTableID(uchar TableID);
  void SetVersion(uchar Version);
  void SetRunningStatus(int RunningStatus, const cChannel *Channel = NULL);
  void SetTitle(const char *Title);
  void SetShortText(const char *ShortText);
  void SetDescription(const char *Description);
  void SetComponents(cComponents *Components); // Will take ownership of Components!
  void SetContents(uchar *Contents);
  void SetParentalRating(int ParentalRating);
  void SetStartTime(time_t StartTime);
  void SetDuration(int Duration);
  void SetVps(time_t Vps);
  void SetSeen(void);
  void SetAux(const char *Aux);
  cString ToDescr(void) const;
  void Dump(FILE *f, const char *Prefix = "", bool InfoOnly = false) const;
  bool Parse(char *s);
  static bool Read(FILE *f, cSchedule *Schedule, int &Line);
  void FixEpgBugs(void);
  };

class cSchedules;

class cSchedule : public cListObject  {
private:
  static cMutex numTimersMutex; // Protects numTimers, because it might be accessed from parallel read locks
  tChannelID channelID;
  cList<cEvent> events;
  cHash<cEvent> eventsHashID;
  cHash<cEvent> eventsHashStartTime;
  mutable u_int16_t numTimers;// The number of timers that use this schedule
  bool onActualTp;
  int modified;
  time_t presentSeen;
public:
  cSchedule(tChannelID ChannelID);
  tChannelID ChannelID(void) const { return channelID; }
  bool Modified(int &State) const { bool Result = State != modified; State = modified; return Result; }
  bool OnActualTp(uchar TableId);
  time_t PresentSeen(void) const { return presentSeen; }
  bool PresentSeenWithin(int Seconds) const { return time(NULL) - presentSeen < Seconds; }
  void SetModified(void) { modified++; }
  void SetPresentSeen(void) { presentSeen = time(NULL); }
  void SetRunningStatus(cEvent *Event, int RunningStatus, const cChannel *Channel = NULL);
  void ClrRunningStatus(cChannel *Channel = NULL);
  void ResetVersions(void);
  void Sort(void);
  void DropOutdated(time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version);
  void Cleanup(time_t Time);
  void Cleanup(void);
  void IncNumTimers(void) const;
  void DecNumTimers(void) const;
  bool HasTimer(void) const { return numTimers > 0; }
  cEvent *AddEvent(cEvent *Event);
  void DelEvent(cEvent *Event);
  void HashEvent(cEvent *Event);
  void UnhashEvent(cEvent *Event);
  const cList<cEvent> *Events(void) const { return &events; }
  const cEvent *GetPresentEvent(void) const;
  const cEvent *GetFollowingEvent(void) const;
#if DEPRECATED_SCHEDULE_GET_EVENT
  [[deprecated("see HISTORY, version 2.5.2")]]
  const cEvent *GetEvent(tEventID EventID, time_t StartTime = 0) const;
#endif
  const cEvent *GetEventById(tEventID EventID) const;
  const cEvent *GetEventByTime(time_t StartTime) const;
  const cEvent *GetEventAround(time_t Time) const;
  void Dump(const cChannels *Channels, FILE *f, const char *Prefix = "", eDumpMode DumpMode = dmAll, time_t AtTime = 0) const;
  static bool Read(FILE *f, cSchedules *Schedules);
  };

class cSchedules : public cList<cSchedule> {
  friend class cSchedule;
private:
  static cSchedules schedules;
  static char *epgDataFileName;
  static time_t lastDump;
public:
  cSchedules(void);
  static const cSchedules *GetSchedulesRead(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of schedules for read access.
      ///< See cTimers::GetTimersRead() for details.
  static cSchedules *GetSchedulesWrite(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of schedules for write access.
      ///< See cTimers::GetTimersWrite() for details.
  static void SetEpgDataFileName(const char *FileName);
  static void Cleanup(bool Force = false);
  static void ResetVersions(void);
  static bool Dump(FILE *f = NULL, const char *Prefix = "", eDumpMode DumpMode = dmAll, time_t AtTime = 0);
  static bool Read(FILE *f = NULL);
  cSchedule *AddSchedule(tChannelID ChannelID);
  const cSchedule *GetSchedule(tChannelID ChannelID) const;
  const cSchedule *GetSchedule(const cChannel *Channel, bool AddIfMissing = false) const;
  };

// Provide lock controlled access to the list:

DEF_LIST_LOCK(Schedules);

// These macros provide a convenient way of locking the global schedules list
// and making sure the lock is released as soon as the current scope is left
// (note that these macros wait forever to obtain the lock!):

#define LOCK_SCHEDULES_READ  USE_LIST_LOCK_READ(Schedules);
#define LOCK_SCHEDULES_WRITE USE_LIST_LOCK_WRITE(Schedules);

class cEpgDataReader : public cThread {
public:
  cEpgDataReader(void);
  virtual void Action(void);
  };

void ReportEpgBugFixStats(bool Force = false);

class cEpgHandler : public cListObject {
public:
  cEpgHandler(void);
          ///< Constructs a new EPG handler and adds it to the list of EPG handlers.
          ///< Whenever an event is received from the EIT data stream, the EPG
          ///< handlers are queried in the order they have been created.
          ///< As soon as one of the EPG handlers returns true in a member function,
          ///< none of the remaining handlers will be queried. If none of the EPG
          ///< handlers returns true in a particular call, the default processing
          ///< will take place.
          ///< EPG handlers will be deleted automatically at the end of the program.
  virtual ~cEpgHandler();
  virtual bool IgnoreChannel(const cChannel *Channel) { return false; }
          ///< Before any EIT data for the given Channel is processed, the EPG handlers
          ///< are asked whether this Channel shall be completely ignored. If any of
          ///< the EPG handlers returns true in this function, no EIT data at all will
          ///< be processed for this Channel.
  virtual bool HandleEitEvent(cSchedule *Schedule, const SI::EIT::Event *EitEvent, uchar TableID, uchar Version) { return false; }
          ///< Before the raw EitEvent for the given Schedule is processed, the
          ///< EPG handlers are queried to see if any of them would like to do the
          ///< complete processing by itself. TableID and Version are from the
          ///< incoming section data.
  virtual bool HandledExternally(const cChannel *Channel) { return false; }
          ///< If any EPG handler returns true in this function, it is assumed that
          ///< the EPG for the given Channel is handled completely from some external
          ///< source. Incoming EIT data is processed as usual, but any new EPG event
          ///< will not be added to the respective schedule. It's up to the EPG
          ///< handler to take care of this.
  virtual bool IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version) { return false; }
          ///< VDR can't perform the update check (version, tid) for externally handled events,
          ///< therefore the EPG handlers have to take care of this. Otherwise the parsing of
          ///< non-updates will waste a lot of resources.
  virtual bool SetEventID(cEvent *Event, tEventID EventID) { return false; }
          ///< Important note: if you want VPS to work, do not mess with the event ids!
  virtual bool SetTitle(cEvent *Event, const char *Title) { return false; }
  virtual bool SetShortText(cEvent *Event, const char *ShortText) { return false; }
  virtual bool SetDescription(cEvent *Event, const char *Description) { return false; }
  virtual bool SetContents(cEvent *Event, uchar *Contents) { return false; }
  virtual bool SetParentalRating(cEvent *Event, int ParentalRating) { return false; }
  virtual bool SetStartTime(cEvent *Event, time_t StartTime) { return false; }
  virtual bool SetDuration(cEvent *Event, int Duration) { return false; }
  virtual bool SetVps(cEvent *Event, time_t Vps) { return false; }
  virtual bool SetComponents(cEvent *Event, cComponents *Components) { return false; }
  virtual bool FixEpgBugs(cEvent *Event) { return false; }
          ///< Fixes some known problems with EPG data.
  virtual bool HandleEvent(cEvent *Event) { return false; }
          ///< After all modifications of the Event have been done, the EPG handler
          ///< can take a final look at it.
  virtual bool SortSchedule(cSchedule *Schedule) { return false; }
          ///< Sorts the Schedule after the complete table has been processed.
  virtual bool DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version) { return false; }
          ///< Takes a look at all EPG events between SegmentStart and SegmentEnd and
          ///< drops outdated events.
  virtual bool BeginSegmentTransfer(const cChannel *Channel, bool Dummy) { return true; } // TODO remove obsolete Dummy
          ///< Called directly after IgnoreChannel() before any other handler method is called.
          ///< Designed to give handlers the possibility to prepare a database transaction.
          ///< If any EPG handler returns false in this function, it is assumed that
          ///< the EPG for the given Channel has to be handled later due to some transaction problems,
          ///> therefore the processing will be aborted.
          ///< Dummy is for backward compatibility and may be removed in a future version.
  virtual bool EndSegmentTransfer(bool Modified, bool Dummy) { return false; } // TODO remove obsolete Dummy
          ///< Called after the segment data has been processed.
          ///< At this point handlers should close/commit/rollback any pending database transactions.
          ///< Dummy is for backward compatibility and may be removed in a future version.
  };

class cEpgHandlers : public cList<cEpgHandler> {
public:
  bool IgnoreChannel(const cChannel *Channel);
  bool HandleEitEvent(cSchedule *Schedule, const SI::EIT::Event *EitEvent, uchar TableID, uchar Version);
  bool HandledExternally(const cChannel *Channel);
  bool IsUpdate(tEventID EventID, time_t StartTime, uchar TableID, uchar Version);
  void SetEventID(cEvent *Event, tEventID EventID);
  void SetTitle(cEvent *Event, const char *Title);
  void SetShortText(cEvent *Event, const char *ShortText);
  void SetDescription(cEvent *Event, const char *Description);
  void SetContents(cEvent *Event, uchar *Contents);
  void SetParentalRating(cEvent *Event, int ParentalRating);
  void SetStartTime(cEvent *Event, time_t StartTime);
  void SetDuration(cEvent *Event, int Duration);
  void SetVps(cEvent *Event, time_t Vps);
  void SetComponents(cEvent *Event, cComponents *Components);
  void FixEpgBugs(cEvent *Event);
  void HandleEvent(cEvent *Event);
  void SortSchedule(cSchedule *Schedule);
  void DropOutdated(cSchedule *Schedule, time_t SegmentStart, time_t SegmentEnd, uchar TableID, uchar Version);
  bool BeginSegmentTransfer(const cChannel *Channel);
  void EndSegmentTransfer(bool Modified);
  };

extern cEpgHandlers EpgHandlers;

#endif //__EPG_H
