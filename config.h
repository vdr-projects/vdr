/*
 * config.h: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.h 1.224 2005/08/20 10:29:35 kls Exp $
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
#include "i18n.h"
#include "tools.h"

#define VDRVERSION  "1.3.30"
#define VDRVERSNUM   10330  // Version * 10000 + Major * 100 + Minor

#define MAXPRIORITY 99
#define MAXLIFETIME 99

#define MINOSDWIDTH  480
#define MAXOSDWIDTH  672
#define MINOSDHEIGHT 324
#define MAXOSDHEIGHT 567

#define MaxFileName 256
#define MaxSkinName 16
#define MaxThemeName 16

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
  bool Load(const char *FileName = NULL, bool AllowComments = false, bool MustExist = false)
  {
    Clear();
    if (FileName) {
       free(fileName);
       fileName = strdup(FileName);
       allowComments = AllowComments;
       }
    bool result = !MustExist;
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
       else {
          LOG_ERROR_STR(fileName);
          result = false;
          }
       }
    if (!result)
       fprintf(stderr, "vdr: error while reading '%s'\n", fileName);
    return result;
  }
  bool Save(void)
  {
    bool result = true;
    T *l = (T *)this->First();
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

class cCommands : public cConfig<cCommand> {};

class cSVDRPhosts : public cConfig<cSVDRPhost> {
public:
  bool Acceptable(in_addr_t Address);
  };

class cCaDefinitions : public cConfig<cCaDefinition> {
public:
  const cCaDefinition *Get(int Number);
  };

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
  virtual int Compare(const cListObject &ListObject) const;
  const char *Plugin(void) { return plugin; }
  const char *Name(void) { return name; }
  const char *Value(void) { return value; }
  bool Parse(char *s);
  bool Save(FILE *f);
  };

class cSetup : public cConfig<cSetupLine> {
  friend class cPlugin; // needs to be able to call Store()
private:
  void StoreLanguages(const char *Name, int *Values);
  bool ParseLanguages(const char *Value, int *Values);
  bool Parse(const char *Name, const char *Value);
  cSetupLine *Get(const char *Name, const char *Plugin = NULL);
  void Store(const char *Name, const char *Value, const char *Plugin = NULL, bool AllowMultiple = false);
  void Store(const char *Name, int Value, const char *Plugin = NULL);
public:
  // Also adjust cMenuSetup (menu.c) when adding parameters here!
  int __BeginData__;
  int OSDLanguage;
  char OSDSkin[MaxSkinName];
  char OSDTheme[MaxThemeName];
  int PrimaryDVB;
  int ShowInfoOnChSwitch;
  int MenuScrollPage;
  int MenuScrollWrap;
  int MarkInstantRecord;
  char NameInstantRecord[MaxFileName];
  int InstantRecordTime;
  int LnbSLOF;
  int LnbFrequLo;
  int LnbFrequHi;
  int DiSEqC;
  int SetSystemTime;
  int TimeSource;
  int TimeTransponder;
  int MarginStart, MarginStop;
  int AudioLanguages[I18nNumLanguages + 1];
  int EPGLanguages[I18nNumLanguages + 1];
  int EPGScanTimeout;
  int EPGBugfixLevel;
  int EPGLinger;
  int SVDRPTimeout;
  int ZapTimeout;
  int SortTimers;
  int PrimaryLimit;
  int DefaultPriority, DefaultLifetime;
  int PausePriority, PauseLifetime;
  int UseSubtitle;
  int UseVps;
  int VpsMargin;
  int RecordingDirs;
  int VideoDisplayFormat;
  int VideoFormat;
  int UpdateChannels;
  int UseDolbyDigital;
  int ChannelInfoPos;
  int ChannelInfoTime;
  int OSDLeft, OSDTop, OSDWidth, OSDHeight;
  int OSDMessageTime;
  int UseSmallFont;
  int MaxVideoFileSize;
  int SplitEditedFiles;
  int MinEventTimeout, MinUserInactivity;
  int MultiSpeedMode;
  int ShowReplayMode;
  int ResumeID;
  int CurrentChannel;
  int CurrentVolume;
  int CurrentDolby;
  int __EndData__;
  cSetup(void);
  cSetup& operator= (const cSetup &s);
  bool Load(const char *FileName);
  bool Save(void);
  };

extern cSetup Setup;

#endif //__CONFIG_H
