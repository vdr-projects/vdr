/*
 * timers.h: Timer handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: timers.h 1.5 2003/05/11 13:35:53 kls Exp $
 */

#ifndef __TIMERS_H
#define __TIMERS_H

#include "channels.h"
#include "config.h"
#include "eit.h"
#include "tools.h"

enum eTimerActive { taInactive = 0,
                    taActive   = 1,
                    taInstant  = 2,
                    taActInst  = (taActive | taInstant)
                  };

class cTimer : public cListObject {
  friend class cMenuEditTimer;
private:
  time_t startTime, stopTime;
  static char *buffer;
  bool recording, pending;
  int active;
  cChannel *channel;
  int day;
  int start;
  int stop;
  int priority;
  int lifetime;
  char file[MaxFileName];
  time_t firstday;
  char *summary;
public:
  cTimer(bool Instant = false, bool Pause = false);
  cTimer(const cEventInfo *EventInfo);
  virtual ~cTimer();
  cTimer& operator= (const cTimer &Timer);
  virtual bool operator< (const cListObject &ListObject);
  bool Recording(void) { return recording; }
  bool Pending(void) { return pending; }
  int Active(void) { return active; }
  const cChannel *Channel(void) { return channel; }
  int Day(void) { return day; }
  int Start(void) { return start; }
  int Stop(void) { return stop; }
  int Priority(void) { return priority; }
  int Lifetime(void) { return lifetime; }
  const char *File(void) { return file; }
  time_t FirstDay(void) { return firstday; }
  const char *Summary(void) { return summary; }
  const char *ToText(bool UseChannelID = false);
  bool Parse(const char *s);
  bool Save(FILE *f);
  bool IsSingleEvent(void);
  int GetMDay(time_t t);
  int GetWDay(time_t t);
  bool DayMatches(time_t t);
  static time_t IncDay(time_t t, int Days);
  static time_t SetTime(time_t t, int SecondsFromMidnight);
  char *SetFile(const char *File);
  bool Matches(time_t t = 0);
  time_t StartTime(void);
  time_t StopTime(void);
  void SetRecording(bool Recording);
  void SetPending(bool Pending);
  void SetActive(int Active);
  void Skip(void);
  void OnOff(void);
  const char *PrintFirstDay(void);
  static int TimeToInt(int t);
  static int ParseDay(const char *s, time_t *FirstDay = NULL);
  static const char *PrintDay(int d, time_t FirstDay = 0);
  };

class cTimers : public cConfig<cTimer> {
private:
  int beingEdited;
public:
  cTimer *GetTimer(cTimer *Timer);
  cTimer *GetMatch(time_t t);
  cTimer *GetNextActiveTimer(void);
  int BeingEdited(void) { return beingEdited; }
  void IncBeingEdited(void) { beingEdited++; }
  void DecBeingEdited(void) { beingEdited--; }
  };

extern cTimers Timers;

#endif //__TIMERS_H
