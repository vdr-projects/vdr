/*
 * config.h: Configuration file handling
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.h 1.2 2000/03/05 14:58:23 kls Exp $
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "tools.h"

#define MaxBuffer 1000

enum eKeys { // "Up" and "Down" must be the first two keys!
             kUp,
             kDown,
             kMenu,
             kOk,
             kBack,
             kLeft,
             kRight,
             k0, k1, k2, k3, k4, k5, k6, k7, k8, k9,
             kRed,
             kGreen,
             kYellow,
             kBlue,
             kNone
           };

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
  bool Load(char *FileName = NULL);
  bool Save(void);
  eKeys Get(unsigned int Code);
  void Set(eKeys Key, unsigned int Code);
  };

class cChannel : public cListObject {
public:
  enum { MaxChannelName = 32 }; // 31 chars + terminating 0!
  char name[MaxChannelName];
  int frequency; // MHz
  char polarization;
  int diseqc;
  int srate;
  int vpid;
  int apid;
  cChannel(void);
  cChannel(const cChannel *Channel);
  bool Parse(char *s);
  bool Save(FILE *f);
  bool Switch(void);
  static bool SwitchTo(int i);
  };

class cTimer : public cListObject {
private:
  time_t startTime, stopTime;
public:
  enum { MaxFileName = 256 };
  bool recording;
  int active;
  int channel;
  int day;
  int start;
  int stop;
//TODO VPS???
  char quality;
  int priority;
  int lifetime;
  char file[MaxFileName];
  cTimer(void);
  bool Parse(char *s);
  bool Save(FILE *f);
  bool IsSingleEvent(void);
  bool Matches(void);
  time_t StartTime(void);
  time_t StopTime(void);
  void SetRecording(bool Recording);
  static cTimer *GetMatch(void);
  static int TimeToInt(int t);
  static time_t Day(time_t t);
  static int ParseDay(char *s);
  static char *PrintDay(int d);
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
  bool Load(char *FileName)
  {
    isyslog(LOG_INFO, "loading %s", FileName);
    bool result = true;
    Clear();
    fileName = strdup(FileName);
    FILE *f = fopen(fileName, "r");
    if (f) {
       int line = 0;
       char buffer[MaxBuffer];
       while (fgets(buffer, sizeof(buffer), f) > 0) {
             line++;
             T *l = new T;
             if (l->Parse(buffer))
                Add(l);
             else {
                fprintf(stderr, "error in %s, line %d\n", fileName, line);
                delete l;
                result = false;
                break;
                }
             }
       fclose(f);
       }
    else {
       fprintf(stderr, "can't open '%s'\n", fileName);
       result = false;
       }
    return result;
  }
  bool Save(void)
  {
  //TODO make backup copies???
    bool result = true;
    T *l = (T *)First();
    FILE *f = fopen(fileName, "w");
    if (f) {
       while (l) {
             if (!l->Save(f)) {
                result = false;
                break;
                }
             l = (T *)l->Next();
             }
       fclose(f);
       }
    else
       result = false;
    return result;
  }
  };

class cChannels : public cConfig<cChannel> {};
class cTimers : public cConfig<cTimer> {};

extern int CurrentChannel;
extern bool ChannelLocked;

extern cChannels Channels;
extern cTimers Timers;
extern cKeys Keys;

#endif //__CONFIG_H
