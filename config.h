/*
 * config.h: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.h 1.74 2001/09/08 14:12:30 kls Exp $
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "dvbapi.h"
#include "eit.h"
#include "tools.h"

#define VDRVERSION "0.95"

#define MaxBuffer 10000

#define MAXPRIORITY 99
#define MAXLIFETIME 99

#define MINOSDWIDTH  40
#define MAXOSDWIDTH  56
#define MINOSDHEIGHT 12
#define MAXOSDHEIGHT 21

enum eKeys { // "Up" and "Down" must be the first two keys!
             kUp,
             kDown,
             kMenu,
             kOk,
             kBack,
             kLeft,
             kRight,
             kRed,
             kGreen,
             kYellow,
             kBlue,
             k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,
             kPower,
             kNone,
             // The following flags are OR'd with the above codes:
             k_Repeat  = 0x8000,
             k_Release = 0x4000,
             k_Flags   = k_Repeat | k_Release,
           };

// This is in preparation for having more key codes:
#define kMarkToggle      k0
#define kMarkMoveBack    k4
#define kMarkMoveForward k6
#define kMarkJumpBack    k7
#define kMarkJumpForward k9
#define kEditCut         k2
#define kEditTest        k8

#define RAWKEY(k)    (eKeys((k) & ~k_Flags))
#define ISRAWKEY(k)  ((k) != kNone && ((k) & k_Flags) == 0)
#define NORMALKEY(k) (eKeys((k) & ~k_Repeat))

struct tKey {
  eKeys type;
  char *name;
  unsigned int code;
  };

class cKeys {
private:
  char *fileName;
public:
  unsigned char code;
  unsigned short address;
  tKey *keys;
  cKeys(void);
  void Clear(void);
  void SetDummyValues(void);
  bool Load(const char *FileName = NULL);
  bool Save(void);
  eKeys Translate(const char *Command);
  unsigned int Encode(const char *Command);
  eKeys Get(unsigned int Code);
  void Set(eKeys Key, unsigned int Code);
  };

class cChannel : public cListObject {
private:
  static char *buffer;
  static const char *ToText(cChannel *Channel);
public:
  enum { MaxChannelName = 32 }; // 31 chars + terminating 0!
  char name[MaxChannelName];
  int frequency; // MHz
  char polarization;
  int diseqc;
  int srate;
  int vpid;
  int apid1, apid2;
  int dpid1, dpid2;
  int tpid;
  int ca;
  int pnr;
  int number;    // Sequence number assigned on load
  bool groupSep;
  cChannel(void);
  cChannel(const cChannel *Channel);
  const char *ToText(void);
  bool Parse(const char *s);
  bool Save(FILE *f);
  bool Switch(cDvbApi *DvbApi = NULL, bool Log = true);
  };

class cTimer : public cListObject {
private:
  time_t startTime, stopTime;
  static char *buffer;
  static const char *ToText(cTimer *Timer);
public:
  enum { MaxFileName = 256 };
  bool recording, pending;
  int active;
  int channel;
  int day;
  int start;
  int stop;
//TODO VPS???
  int priority;
  int lifetime;
  char file[MaxFileName];
  char *summary;
  cTimer(bool Instant = false);
  cTimer(const cEventInfo *EventInfo);
  virtual ~cTimer();
  cTimer& operator= (const cTimer &Timer);
  bool operator< (const cTimer &Timer);
  const char *ToText(void);
  bool Parse(const char *s);
  bool Save(FILE *f);
  bool IsSingleEvent(void);
  int GetMDay(time_t t);
  int GetWDay(time_t t);
  bool DayMatches(time_t t);
  time_t IncDay(time_t t, int Days);
  time_t SetTime(time_t t, int SecondsFromMidnight);
  bool Matches(time_t t = 0);
  time_t StartTime(void);
  time_t StopTime(void);
  void SetRecording(bool Recording);
  void SetPending(bool Pending);
  static int TimeToInt(int t);
  static int ParseDay(const char *s);
  static const char *PrintDay(int d);
  };

class cCommand : public cListObject {
private:
  char *title;
  char *command;
  static char *result;
public:
  cCommand(void);
  virtual ~cCommand();
  bool Parse(const char *s);
  const char *Title(void) { return title; }
  const char *Execute(void);
  };

template<class T> class cConfig : public cList<T> {
private:
  char *fileName;
  void Clear(void)
  {
    delete fileName;
    cList<T>::Clear();
  }
public:
  cConfig(void) { fileName = NULL; }
  virtual ~cConfig() { delete fileName; }
  virtual bool Load(const char *FileName)
  {
    Clear();
    fileName = strdup(FileName);
    bool result = false;
    if (access(FileName, F_OK) == 0) {
       isyslog(LOG_INFO, "loading %s", FileName);
       FILE *f = fopen(fileName, "r");
       if (f) {
          int line = 0;
          char buffer[MaxBuffer];
          result = true;
          while (fgets(buffer, sizeof(buffer), f) > 0) {
                line++;
                if (!isempty(buffer)) {
                   T *l = new T;
                   if (l->Parse(buffer))
                      Add(l);
                   else {
                      esyslog(LOG_ERR, "error in %s, line %d\n", fileName, line);
                      delete l;
                      result = false;
                      break;
                      }
                   }
                }
          fclose(f);
          }
       else
          LOG_ERROR_STR(fileName);
       }
    return result;
  }
  bool Save(void)
  {
    bool result = true;
    T *l = (T *)First();
    cSafeFile f(fileName);
    if (f.Open()) {
       while (l) {
             if (!l->Save(f)) {
                result = false;
                break;
                }
             l = (T *)l->Next();
             }
       f.Close();
       }
    else
       result = false;
    return result;
  }
  };

class cChannels : public cConfig<cChannel> {
protected:
  int maxNumber;
public:
  cChannels(void) { maxNumber = 0; }
  virtual bool Load(const char *FileName);
  int GetNextGroup(int Idx);   // Get next channel group
  int GetPrevGroup(int Idx);   // Get previous channel group
  int GetNextNormal(int Idx);  // Get next normal channel (not group)
  void ReNumber(void);         // Recalculate 'number' based on channel type
  cChannel *GetByNumber(int Number);
  cChannel *GetByServiceID(unsigned short ServiceId);
  const char *GetChannelNameByNumber(int Number);
  bool SwitchTo(int Number, cDvbApi *DvbApi = NULL);
  int MaxNumber(void) { return maxNumber; }
  };

class cTimers : public cConfig<cTimer> {
public:
  cTimer *GetTimer(cTimer *Timer);
  cTimer *GetMatch(time_t t);
  cTimer *GetNextActiveTimer(void);
  };

class cCommands : public cConfig<cCommand> {};

extern cChannels Channels;
extern cTimers Timers;
extern cKeys Keys;
extern cCommands Commands;

class cSetup {
private:
  static char *fileName;
  bool Parse(char *s);
public:
  // Also adjust cMenuSetup (menu.c) when adding parameters here!
  int OSDLanguage;
  int PrimaryDVB;
  int ShowInfoOnChSwitch;
  int MenuScrollPage;
  int MarkInstantRecord;
  int LnbSLOF;
  int LnbFrequLo;
  int LnbFrequHi;
  int DiSEqC;
  int SetSystemTime;
  int MarginStart, MarginStop;
  int EPGScanTimeout;
  int EPGBugfixLevel;
  int SVDRPTimeout;
  int SortTimers;
  int PrimaryLimit;
  int DefaultPriority, DefaultLifetime;
  int UseSubtitle;
  int VideoFormat;
  int ChannelInfoPos;
  int OSDwidth, OSDheight;
  int OSDMessageTime;
  int MaxVideoFileSize;
  int MinEventTimeout, MinUserInactivity;
  int CurrentChannel;
  cSetup(void);
  bool Load(const char *FileName);
  bool Save(const char *FileName = NULL);
  };

extern cSetup Setup;

#endif //__CONFIG_H
