/*
 * config.h: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.h 1.136 2002/10/19 11:29:46 kls Exp $
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "device.h"
#include "eit.h"
#include "tools.h"

#define VDRVERSION "1.1.14"

#define MAXPRIORITY 99
#define MAXLIFETIME 99

#define MINOSDWIDTH  40
#define MAXOSDWIDTH  56
#define MINOSDHEIGHT 12
#define MAXOSDHEIGHT 21

#define MaxFileName 256

enum eTimerActive { taInactive = 0,
                    taActive   = 1,
                    taInstant  = 2,
                    taActInst  = (taActive | taInstant)
                  };

class cTimer : public cListObject {
private:
  time_t startTime, stopTime;
  static char *buffer;
  static const char *ToText(cTimer *Timer);
public:
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
  time_t firstday;
  char *summary;
  cTimer(bool Instant = false);
  cTimer(const cEventInfo *EventInfo);
  virtual ~cTimer();
  cTimer& operator= (const cTimer &Timer);
  virtual bool operator< (const cListObject &ListObject);
  const char *ToText(void);
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
  void Skip(void);
  const char *PrintFirstDay(void);
  static int TimeToInt(int t);
  static int ParseDay(const char *s, time_t *FirstDay = NULL);
  static const char *PrintDay(int d, time_t FirstDay = 0);
  };

class cCommand : public cListObject {
private:
  char *title;
  char *command;
  bool confirm;
  static char *result;
public:
  cCommand(void);
  virtual ~cCommand();
  bool Parse(const char *s);
  const char *Title(void) { return title; }
  bool Confirm(void) { return confirm; }
  const char *Execute(const char *Parameters = NULL);
  };

typedef uint32_t in_addr_t; //XXX from /usr/include/netinet/in.h (apparently this is not defined on systems with glibc < 2.2)

class cSVDRPhost : public cListObject {
private:
  struct in_addr addr;
  in_addr_t mask;
public:
  cSVDRPhost(void);
  bool Parse(const char *s);
  bool Accepts(in_addr_t Address);
  };

#define CACONFBASE 100

class cCaDefinition : public cListObject {
private:
  int number;
  char *description;
public:
  cCaDefinition(void);
  ~cCaDefinition();
  bool Parse(const char *s);
  int Number(void) const { return number; }
  const char *Description(void) const { return description; }
  };

template<class T> class cConfig : public cList<T> {
private:
  char *fileName;
  bool allowComments;
  void Clear(void)
  {
    free(fileName);
    fileName = NULL;
    cList<T>::Clear();
  }
public:
  cConfig(void) { fileName = NULL; }
  virtual ~cConfig() { free(fileName); }
  const char *FileName(void) { return fileName; }
  bool Load(const char *FileName = NULL, bool AllowComments = false)
  {
    Clear();
    if (FileName) {
       free(fileName);
       fileName = strdup(FileName);
       allowComments = AllowComments;
       }
    bool result = false;
    if (fileName && access(fileName, F_OK) == 0) {
       isyslog("loading %s", fileName);
       FILE *f = fopen(fileName, "r");
       if (f) {
          int line = 0;
          char buffer[MAXPARSEBUFFER];
          result = true;
          while (fgets(buffer, sizeof(buffer), f) > 0) {
                line++;
                if (allowComments) {
                   char *p = strchr(buffer, '#');
                   if (p)
                      *p = 0;
                   }
                stripspace(buffer);
                if (!isempty(buffer)) {
                   T *l = new T;
                   if (l->Parse(buffer))
                      Add(l);
                   else {
                      esyslog("ERROR: error in %s, line %d\n", fileName, line);
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
       if (!f.Close())
          result = false;
       }
    else
       result = false;
    return result;
  }
  };

class cTimers : public cConfig<cTimer> {
public:
  cTimer *GetTimer(cTimer *Timer);
  cTimer *GetMatch(time_t t);
  cTimer *GetNextActiveTimer(void);
  };

class cCommands : public cConfig<cCommand> {};

class cSVDRPhosts : public cConfig<cSVDRPhost> {
public:
  bool Acceptable(in_addr_t Address);
  };

class cCaDefinitions : public cConfig<cCaDefinition> {
public:
  const cCaDefinition *Get(int Number);
  };

extern cTimers Timers;
extern cCommands Commands;
extern cCommands RecordingCommands;
extern cSVDRPhosts SVDRPhosts;
extern cCaDefinitions CaDefinitions;

class cSetupLine : public cListObject {
private:
  char *plugin;
  char *name;
  char *value;
public:
  cSetupLine(void);
  cSetupLine(const char *Name, const char *Value, const char *Plugin = NULL);
  virtual ~cSetupLine();
  virtual bool operator< (const cListObject &ListObject);
  const char *Plugin(void) { return plugin; }
  const char *Name(void) { return name; }
  const char *Value(void) { return value; }
  bool Parse(char *s);
  bool Save(FILE *f);
  };

class cSetup : public cConfig<cSetupLine> {
  friend class cPlugin; // needs to be able to call Store()
private:
  void StoreCaCaps(const char *Name);
  bool ParseCaCaps(const char *Value);
  bool Parse(const char *Name, const char *Value);
  cSetupLine *Get(const char *Name, const char *Plugin = NULL);
  void Store(const char *Name, const char *Value, const char *Plugin = NULL, bool AllowMultiple = false);
  void Store(const char *Name, int Value, const char *Plugin = NULL);
public:
  // Also adjust cMenuSetup (menu.c) when adding parameters here!
  int __BeginData__;
  int OSDLanguage;
  int PrimaryDVB;
  int ShowInfoOnChSwitch;
  int MenuScrollPage;
  int MarkInstantRecord;
  char NameInstantRecord[MaxFileName];
  int InstantRecordTime;
  int LnbSLOF;
  int LnbFrequLo;
  int LnbFrequHi;
  int DiSEqC;
  int SetSystemTime;
  int TimeTransponder;
  int MarginStart, MarginStop;
  int EPGScanTimeout;
  int EPGBugfixLevel;
  int SVDRPTimeout;
  int SortTimers;
  int PrimaryLimit;
  int DefaultPriority, DefaultLifetime;
  int UseSubtitle;
  int RecordingDirs;
  int VideoFormat;
  int RecordDolbyDigital;
  int ChannelInfoPos;
  int OSDwidth, OSDheight;
  int OSDMessageTime;
  int MaxVideoFileSize;
  int SplitEditedFiles;
  int MinEventTimeout, MinUserInactivity;
  int MultiSpeedMode;
  int ShowReplayMode;
  int CaCaps[MAXDEVICES][MAXCACAPS];
  int CurrentChannel;
  int CurrentVolume;
  int __EndData__;
  cSetup(void);
  cSetup& operator= (const cSetup &s);
  bool Load(const char *FileName);
  bool Save(void);
  };

extern cSetup Setup;

#endif //__CONFIG_H
