/*
 * timers.h: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.h 5.5 2021/04/10 10:09:50 kls Exp $
 */

#ifndef __TIMERS_H
#define __TIMERS_H

#include "channels.h"
#include "config.h"
#include "epg.h"
#include "tools.h"

enum eTimerFlags { tfNone      = 0x0000,
                   tfActive    = 0x0001,
                   tfInstant   = 0x0002,
                   tfVps       = 0x0004,
                   tfRecording = 0x0008,
                   tfSpawned   = 0x0010,
                   tfAvoid     = 0x0020,
                   tfAll       = 0xFFFF,
                 };
enum eTimerMatch { tmNone, tmPartial, tmFull };

class cTimers;

class cTimer : public cListObject {
  friend class cMenuEditTimer;
private:
  int id;
  mutable time_t startTime, stopTime;
  int scheduleStateSet;
  int scheduleStateSpawn;
  int scheduleStateAdjust;
  mutable time_t deferred; ///< Matches(time_t, ...) will return false if the current time is before this value
  bool pending, inVpsMargin;
  uint flags;
  const cChannel *channel;
  mutable time_t day; ///< midnight of the day this timer shall hit, or of the first day it shall hit in case of a repeating timer
  int weekdays;       ///< bitmask, lowest bits: SSFTWTM  (the 'M' is the LSB)
  int start;
  int stop;
  int priority;
  int lifetime;
  mutable char pattern[NAME_MAX * 2 + 1]; // same size as 'file', to be able to initially fill 'pattern' with 'file' in the 'Edit timer' menu
  mutable char file[NAME_MAX * 2 + 1]; // *2 to be able to hold 'title' and 'episode', which can each be up to 255 characters long
  char *aux;
  char *remote;
  const cEvent *event;
public:
  cTimer(bool Instant = false, bool Pause = false, const cChannel *Channel = NULL);
  cTimer(const cEvent *Event, const char *FileName = NULL, const cTimer *PatternTimer = NULL);
  cTimer(const cTimer &Timer);
  virtual ~cTimer();
  cTimer& operator= (const cTimer &Timer);
  void CalcMargins(int &MarginStart, int &MarginStop, const cEvent *Event);
  virtual int Compare(const cListObject &ListObject) const;
  int Id(void) const { return id; }
  bool Recording(void) const { return HasFlags(tfRecording); }
  bool Pending(void) const { return pending; }
  bool InVpsMargin(void) const { return inVpsMargin; }
  uint Flags(void) const { return flags; }
  const cChannel *Channel(void) const { return channel; }
  time_t Day(void) const { return day; }
  int WeekDays(void) const { return weekdays; }
  int Start(void) const { return start; }
  int Stop(void) const { return stop; }
  int Priority(void) const { return priority; }
  int Lifetime(void) const { return lifetime; }
  const char *Pattern(void) const { return pattern; }
  const char *File(void) const { return file; }
  time_t FirstDay(void) const { return weekdays ? day : 0; }
  const char *Aux(void) const { return aux; }
  const char *Remote(void) const { return remote; }
  bool Local(void) const { return !remote; } // convenience
  time_t Deferred(void) const { return deferred; }
  cString PatternAndFile(void) const;
  cString ToText(bool UseChannelID = false) const;
  cString ToDescr(void) const;
  const cEvent *Event(void) const { return event; }
  bool Parse(const char *s);
  bool Save(FILE *f);
  bool IsSingleEvent(void) const;
  static int GetMDay(time_t t);
  static int GetWDay(time_t t);
  bool DayMatches(time_t t) const;
  static time_t IncDay(time_t t, int Days);
  static time_t SetTime(time_t t, int SecondsFromMidnight);
  void SetPattern(const char *Pattern);
  void SetFile(const char *File);
  bool IsPatternTimer(void) const { return *pattern; }
  bool Matches(time_t t = 0, bool Directly = false, int Margin = 0) const;
  eTimerMatch Matches(const cEvent *Event, int *Overlap = NULL) const;
  bool Expired(void) const;
  time_t StartTime(void) const;
  time_t StopTime(void) const;
  void SetId(int Id);
  void SpawnPatternTimer(const cEvent *Event, cTimers *Timers);
  bool SpawnPatternTimers(const cSchedules *Schedules, cTimers *Timers);
  bool AdjustSpawnedTimer(void);
  void TriggerRespawn(void);
  bool SetEventFromSchedule(const cSchedules *Schedules);
  bool SetEvent(const cEvent *Event);
  void SetRecording(bool Recording);
  void SetPending(bool Pending);
  void SetInVpsMargin(bool InVpsMargin);
  void SetDay(time_t Day);
  void SetWeekDays(int WeekDays);
  void SetStart(int Start);
  void SetStop(int Stop);
  void SetPriority(int Priority);
  void SetLifetime(int Lifetime);
  void SetAux(const char *Aux);
  void SetRemote(const char *Remote);
  void SetDeferred(int Seconds);
  void SetFlags(uint Flags);
  void ClrFlags(uint Flags);
  void InvFlags(uint Flags);
  bool HasFlags(uint Flags) const;
  void Skip(void);
  void OnOff(void);
  cString PrintFirstDay(void) const;
  static int TimeToInt(int t);
  static bool ParseDay(const char *s, time_t &Day, int &WeekDays);
  static cString PrintDay(time_t Day, int WeekDays, bool SingleByteChars);
  };

class cTimers : public cConfig<cTimer> {
private:
  static cTimers timers;
  static int lastTimerId;
  time_t lastDeleteExpired;
public:
  cTimers(void);
  static const cTimers *GetTimersRead(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of timers for read access. If TimeoutMs is given,
      ///< it will wait that long to get a read lock before giving up.
      ///< Otherwise it will wait indefinitely. If no read lock can be
      ///< obtained within the given timeout, NULL will be returned.
      ///< The list is locked and a pointer to it is returned if the state
      ///< of the list is different than the state of the given StateKey.
      ///< If both states are equal, the list of timers has not been modified
      ///< since the last call with the same StateKey, and NULL will be
      ///< returned (and the list is not locked). After the returned list of
      ///< timers is no longer needed, the StateKey's Remove() function must
      ///< be called to release the list. The time between calling
      ///< cTimers::GetTimersRead() and StateKey.Remove() should be as short
      ///< as possible. After calling StateKey.Remove() the list returned from
      ///< this call must not be accessed any more. If you need to access the
      ///< timers again later, a new call to GetTimersRead() must be made.
      ///< A typical code sequence would look like this:
      ///< cStateKey StateKey;
      ///< if (const cTimers *Timers = cTimers::GetTimersRead(StateKey)) {
      ///<    // access the timers
      ///<    StateKey.Remove();
      ///<    }
  static cTimers *GetTimersWrite(cStateKey &StateKey, int TimeoutMs = 0);
      ///< Gets the list of timers for write access. If TimeoutMs is given,
      ///< it will wait that long to get a write lock before giving up.
      ///< Otherwise it will wait indefinitely. If no write lock can be
      ///< obtained within the given timeout, NULL will be returned.
      ///< If a write lock can be obtained, the list of timers will be
      ///< returned, regardless of the state values of the timers or the
      ///< given StateKey. After the returned list of timers is no longer
      ///< needed, the StateKey's Remove() function must be called to release
      ///< the list. The time between calling cTimers::GetTimersWrite() and
      ///< StateKey.Remove() should be as short as possible. After calling
      ///< StateKey.Remove() the list returned from this call must not be
      ///< accessed any more. If you need to access the timers again later,
      ///< a new call to GetTimersWrite() must be made. The call
      ///< to StateKey.Remove() will increment the state of the list of
      ///< timers and will copy the new state value to the StateKey. You can
      ///< suppress this by using 'false' as the parameter to the call, in
      ///< which case the state values are left untouched.
      ///< A typical code sequence would look like this:
      ///< cStateKey StateKey;
      ///< if (cTimers *Timers = cTimers::GetTimersWrite(StateKey)) {
      ///<    // access the timers
      ///<    StateKey.Remove();
      ///<    }
  static bool Load(const char *FileName);
  static int NewTimerId(void);
  const cTimer *GetById(int Id, const char *Remote = NULL) const;
  cTimer *GetById(int Id, const char *Remote = NULL) { return const_cast<cTimer *>(static_cast<const cTimers *>(this)->GetById(Id, Remote)); };
  const cTimer *GetTimer(const cTimer *Timer) const;
  cTimer *GetTimer(const cTimer *Timer) { return const_cast<cTimer *>(static_cast<const cTimers *>(this)->GetTimer(Timer)); };
  const cTimer *GetMatch(time_t t) const;
  cTimer *GetMatch(time_t t) { return const_cast<cTimer *>(static_cast<const cTimers *>(this)->GetMatch(t)); };
  const cTimer *GetMatch(const cEvent *Event, eTimerMatch *Match = NULL) const;
  cTimer *GetMatch(const cEvent *Event, eTimerMatch *Match = NULL) { return const_cast<cTimer *>(static_cast<const cTimers *>(this)->GetMatch(Event, Match)); }
  const cTimer *GetTimerForEvent(const cEvent *Event, eTimerFlags Flags = tfNone);
  int GetMaxPriority(void) const;
      ///< Returns the maximum priority of all local timers that are currently recording.
      ///< If there is no local timer currently recording, -1 is returned.
  const cTimer *GetNextActiveTimer(void) const;
  const cTimer *UsesChannel(const cChannel *Channel) const;
  bool SetEvents(const cSchedules *Schedules);
  bool SpawnPatternTimers(const cSchedules *Schedules);
  bool AdjustSpawnedTimers(void);
  bool DeleteExpired(void);
  void Add(cTimer *Timer, cTimer *After = NULL);
  void Ins(cTimer *Timer, cTimer *Before = NULL);
  void Del(cTimer *Timer, bool DeleteObject = true);
  bool StoreRemoteTimers(const char *ServerName = NULL, const cStringList *RemoteTimers = NULL);
      ///< Stores the given list of RemoteTimers, which come from the VDR ServerName, in
      ///< this list. If no ServerName is given, all remote timers from all peer machines
      ///< will be removed from this list. If no RemoteTimers are given, only the remote
      ///< timers from ServerName will be removed from this list.
      ///< The given list of RemoteTimers must be sorted numerically (by a call to its
      ///< SortNumerically() function).
      ///< Returns true if any remote timers have been added, deleted or modified.
  };

bool HandleRemoteTimerModifications(cTimer *NewTimer, cTimer *OldTimer = NULL, cString *Msg = NULL);
     ///< Performs any operations necessary to synchronize changes to a timer
     ///< between peer VDR machines. OldTimer must point to the old version
     ///< of the timer, while NewTimer points to the new version. If either
     ///< of the two is a remote timer, the necessary SVDRP commands are executed
     ///< to reflect the changes on the remote machine(s). If NewTimer is NULL,
     ///< OldTimer will be removed from the remote machine (if applicable).
     ///< If OldTimer is NULL, NewTimer will be added to the remote machine (if
     ///< applicable). If anything goes wrong, an error message is generated in the
     ///< optional Msg string, which should be presented to the user.
     ///< Any necessary local operations (like adding/deleting the timer to the
     ///< local list of timers etc.) must be done before and/or after the call to this
     ///< function. Proper log messages will be generated by this function, even
     ///< if no remote operations are required.
     ///< Returns true if successful.

// Provide lock controlled access to the list:

DEF_LIST_LOCK(Timers);

// These macros provide a convenient way of locking the global timers list
// and making sure the lock is released as soon as the current scope is left
// (note that these macros wait forever to obtain the lock!):

#define LOCK_TIMERS_READ  USE_LIST_LOCK_READ(Timers)
#define LOCK_TIMERS_WRITE USE_LIST_LOCK_WRITE(Timers)

class cSortedTimers : public cVector<const cTimer *> {
public:
  cSortedTimers(const cTimers *Timers);
  };

#endif //__TIMERS_H
