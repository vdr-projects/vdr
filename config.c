/*
 * config.c: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 1.117 2003/10/17 14:11:27 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include "i18n.h"
#include "interface.h"
#include "plugin.h"
#include "recording.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// --- cCommand -------------------------------------------------------------

char *cCommand::result = NULL;

cCommand::cCommand(void)
{
  title = command = NULL;
  confirm = false;
}

cCommand::~cCommand()
{
  free(title);
  free(command);
}

bool cCommand::Parse(const char *s)
{
  const char *p = strchr(s, ':');
  if (p) {
     int l = p - s;
     if (l > 0) {
        title = MALLOC(char, l + 1);
        stripspace(strn0cpy(title, s, l + 1));
        if (!isempty(title)) {
           int l = strlen(title);
           if (l > 1 && title[l - 1] == '?') {
              confirm = true;
              title[l - 1] = 0;
              }
           command = stripspace(strdup(skipspace(p + 1)));
           return !isempty(command);
           }
        }
     }
  return false;
}

const char *cCommand::Execute(const char *Parameters)
{
  free(result);
  result = NULL;
  char *cmdbuf = NULL;
  if (Parameters)
     asprintf(&cmdbuf, "%s %s", command, Parameters);
  const char *cmd = cmdbuf ? cmdbuf : command;
  dsyslog("executing command '%s'", cmd);
  FILE *p = popen(cmd, "r");
  if (p) {
     int l = 0;
     int c;
     while ((c = fgetc(p)) != EOF) {
           if (l % 20 == 0)
              result = (char *)realloc(result, l + 21);
           result[l++] = c;
           }
     if (result)
        result[l] = 0;
     pclose(p);
     }
  else
     esyslog("ERROR: can't open pipe for command '%s'", cmd);
  free(cmdbuf);
  return result;
}

// -- cSVDRPhost -------------------------------------------------------------

cSVDRPhost::cSVDRPhost(void)
{
  addr.s_addr = 0;
  mask = 0;
}

bool cSVDRPhost::Parse(const char *s)
{
  mask = 0xFFFFFFFF;
  const char *p = strchr(s, '/');
  if (p) {
     char *error = NULL;
     int m = strtoul(p + 1, &error, 10);
     if (error && *error && !isspace(*error) || m > 32)
        return false;
     *(char *)p = 0; // yes, we know it's 'const' - will be restored!
     if (m == 0)
        mask = 0;
     else
        mask >>= (32 - m);
     }
  int result = inet_aton(s, &addr);
  if (p)
     *(char *)p = '/'; // there it is again
  return result != 0 && (mask != 0 || addr.s_addr == 0);
}

bool cSVDRPhost::Accepts(in_addr_t Address)
{
  return (Address & mask) == addr.s_addr;
}

// -- cCaDefinition ----------------------------------------------------------

cCaDefinition::cCaDefinition(void)
{
  number = 0;
  description = NULL;
}

cCaDefinition::~cCaDefinition()
{
  free(description);
}

bool cCaDefinition::Parse(const char *s)
{
  return 2 == sscanf(s, "%d %a[^\n]", &number, &description) && description && *description;
}

// -- cCommands --------------------------------------------------------------

cCommands Commands;
cCommands RecordingCommands;

// -- cSVDRPhosts ------------------------------------------------------------

cSVDRPhosts SVDRPhosts;

bool cSVDRPhosts::Acceptable(in_addr_t Address)
{
  cSVDRPhost *h = First();
  while (h) {
        if (h->Accepts(Address))
           return true;
        h = (cSVDRPhost *)h->Next();
        }
  return false;
}

// -- cCaDefinitions ---------------------------------------------------------

cCaDefinitions CaDefinitions;

const cCaDefinition *cCaDefinitions::Get(int Number)
{
  cCaDefinition *p = First();
  while (p) {
        if (p->Number() == Number)
           return p;
        p = (cCaDefinition *)p->Next();
        }
  return NULL;
}

// -- cSetupLine -------------------------------------------------------------

cSetupLine::cSetupLine(void)
{
  plugin = name = value = NULL;
}

cSetupLine::cSetupLine(const char *Name, const char *Value, const char *Plugin)
{
  name = strdup(Name);
  value = strdup(Value);
  plugin = Plugin ? strdup(Plugin) : NULL;
}

cSetupLine::~cSetupLine()
{
  free(plugin);
  free(name);
  free(value);
}

bool cSetupLine::operator< (const cListObject &ListObject)
{
  const cSetupLine *sl = (cSetupLine *)&ListObject;
  if (!plugin && !sl->plugin)
     return strcasecmp(name, sl->name) < 0;
  if (!plugin)
     return true;
  if (!sl->plugin)
     return false;
  int result = strcasecmp(plugin, sl->plugin);
  if (result == 0)
     result = strcasecmp(name, sl->name);
  return result < 0;
}

bool cSetupLine::Parse(char *s)
{
  char *p = strchr(s, '=');
  if (p) {
     *p = 0;
     char *Name  = compactspace(s);
     char *Value = compactspace(p + 1);
     if (*Name) { // value may be an empty string
        p = strchr(Name, '.');
        if (p) {
           *p = 0;
           char *Plugin = compactspace(Name);
           Name = compactspace(p + 1);
           if (!(*Plugin && *Name))
              return false;
           plugin = strdup(Plugin);
           }
        name = strdup(Name);
        value = strdup(Value);
        return true;
        }
     }
  return false;
}

bool cSetupLine::Save(FILE *f)
{
  return fprintf(f, "%s%s%s = %s\n", plugin ? plugin : "", plugin ? "." : "", name, value) > 0;
}

// -- cSetup -----------------------------------------------------------------

cSetup Setup;

cSetup::cSetup(void)
{
  OSDLanguage = 0;
  PrimaryDVB = 1;
  ShowInfoOnChSwitch = 1;
  MenuScrollPage = 1;
  MarkInstantRecord = 1;
  strcpy(NameInstantRecord, "TITLE EPISODE");
  InstantRecordTime = 180;
  LnbSLOF    = 11700;
  LnbFrequLo =  9750;
  LnbFrequHi = 10600;
  DiSEqC = 0;
  SetSystemTime = 0;
  TimeTransponder = 0;
  MarginStart = 2;
  MarginStop = 10;
  EPGScanTimeout = 5;
  EPGBugfixLevel = 2;
  SVDRPTimeout = 300;
  ZapTimeout = 3;
  SortTimers = 1;
  PrimaryLimit = 0;
  DefaultPriority = 50;
  DefaultLifetime = 99;
  PausePriority = 10;
  PauseLifetime = 1;
  UseSubtitle = 1;
  RecordingDirs = 1;
  VideoFormat = 0;
  RecordDolbyDigital = 1;
  ChannelInfoPos = 0;
  OSDwidth = 52;
  OSDheight = 18;
  OSDMessageTime = 1;
  MaxVideoFileSize = MAXVIDEOFILESIZE;
  SplitEditedFiles = 0;
  MinEventTimeout = 30;
  MinUserInactivity = 120;
  MultiSpeedMode = 0;
  ShowReplayMode = 0;
  ResumeID = 0;
  memset(CaCaps, sizeof(CaCaps), 0);
  CurrentChannel = -1;
  CurrentVolume = MAXVOLUME;
}

cSetup& cSetup::operator= (const cSetup &s)
{
  memcpy(&__BeginData__, &s.__BeginData__, (char *)&s.__EndData__ - (char *)&s.__BeginData__);
  return *this;
}

cSetupLine *cSetup::Get(const char *Name, const char *Plugin)
{
  for (cSetupLine *l = First(); l; l = Next(l)) {
      if ((l->Plugin() == NULL) == (Plugin == NULL)) {
         if ((!Plugin || strcasecmp(l->Plugin(), Plugin) == 0) && strcasecmp(l->Name(), Name) == 0)
            return l;
         }
      }
  return NULL;
}

void cSetup::Store(const char *Name, const char *Value, const char *Plugin, bool AllowMultiple)
{
  if (Name && *Name) {
     cSetupLine *l = Get(Name, Plugin);
     if (l && !AllowMultiple)
        Del(l);
     if (Value)
        Add(new cSetupLine(Name, Value, Plugin));
     }
}

void cSetup::Store(const char *Name, int Value, const char *Plugin)
{
  char *buffer = NULL;
  asprintf(&buffer, "%d", Value);
  Store(Name, buffer, Plugin);
  free(buffer);
}

bool cSetup::Load(const char *FileName)
{
  if (cConfig<cSetupLine>::Load(FileName, true)) {
     bool result = true;
     for (cSetupLine *l = First(); l; l = Next(l)) {
         bool error = false;
         if (l->Plugin()) {
            cPlugin *p = cPluginManager::GetPlugin(l->Plugin());
            if (p && !p->SetupParse(l->Name(), l->Value()))
               error = true;
            }
         else {
            if (!Parse(l->Name(), l->Value()))
               error = true;
            }
         if (error) {
            esyslog("ERROR: unknown config parameter: %s%s%s = %s", l->Plugin() ? l->Plugin() : "", l->Plugin() ? "." : "", l->Name(), l->Value());
            result = false;
            }
         }
     return result;
     }
  return false;
}

void cSetup::StoreCaCaps(const char *Name)
{
  cSetupLine *l;
  while ((l = Get(Name)) != NULL)
        Del(l);
  for (int d = 0; d < MAXDEVICES; d++) {
      char buffer[MAXPARSEBUFFER];
      char *q = buffer;
      *buffer = 0;
      for (int i = 0; i < MAXCACAPS; i++) {
          if (CaCaps[d][i]) {
             if (!*buffer)
                q += snprintf(buffer, sizeof(buffer), "%d", d + 1);
             q += snprintf(q, sizeof(buffer) - (q - buffer), " %d", CaCaps[d][i]);
             }
          }
      if (*buffer)
         Store(Name, buffer, NULL, true);
      }
}

bool cSetup::ParseCaCaps(const char *Value)
{
  char *p;
  int d = strtol(Value, &p, 10);
  if (d > 0 && d <= MAXDEVICES) {
     d--;
     int i = 0;
     while (p != Value && p && *p) {
           if (i < MAXCACAPS) {
              int c = strtol(p, &p, 10);
              if (c > 0)
                 CaCaps[d][i++] = c;
              else
                 return false;
              }
           else
              return false;
           }
     return true;
     }
  return false;
}

bool cSetup::Parse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "OSDLanguage"))         OSDLanguage        = atoi(Value);
  else if (!strcasecmp(Name, "PrimaryDVB"))          PrimaryDVB         = atoi(Value);
  else if (!strcasecmp(Name, "ShowInfoOnChSwitch"))  ShowInfoOnChSwitch = atoi(Value);
  else if (!strcasecmp(Name, "MenuScrollPage"))      MenuScrollPage     = atoi(Value);
  else if (!strcasecmp(Name, "MarkInstantRecord"))   MarkInstantRecord  = atoi(Value);
  else if (!strcasecmp(Name, "NameInstantRecord"))   strn0cpy(NameInstantRecord, Value, MaxFileName);
  else if (!strcasecmp(Name, "InstantRecordTime"))   InstantRecordTime  = atoi(Value);
  else if (!strcasecmp(Name, "LnbSLOF"))             LnbSLOF            = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequLo"))          LnbFrequLo         = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequHi"))          LnbFrequHi         = atoi(Value);
  else if (!strcasecmp(Name, "DiSEqC"))              DiSEqC             = atoi(Value);
  else if (!strcasecmp(Name, "SetSystemTime"))       SetSystemTime      = atoi(Value);
  else if (!strcasecmp(Name, "TimeTransponder"))     TimeTransponder    = atoi(Value);
  else if (!strcasecmp(Name, "MarginStart"))         MarginStart        = atoi(Value);
  else if (!strcasecmp(Name, "MarginStop"))          MarginStop         = atoi(Value);
  else if (!strcasecmp(Name, "EPGScanTimeout"))      EPGScanTimeout     = atoi(Value);
  else if (!strcasecmp(Name, "EPGBugfixLevel"))      EPGBugfixLevel     = atoi(Value);
  else if (!strcasecmp(Name, "SVDRPTimeout"))        SVDRPTimeout       = atoi(Value);
  else if (!strcasecmp(Name, "ZapTimeout"))          ZapTimeout         = atoi(Value);
  else if (!strcasecmp(Name, "SortTimers"))          SortTimers         = atoi(Value);
  else if (!strcasecmp(Name, "PrimaryLimit"))        PrimaryLimit       = atoi(Value);
  else if (!strcasecmp(Name, "DefaultPriority"))     DefaultPriority    = atoi(Value);
  else if (!strcasecmp(Name, "DefaultLifetime"))     DefaultLifetime    = atoi(Value);
  else if (!strcasecmp(Name, "PausePriority"))       PausePriority      = atoi(Value);
  else if (!strcasecmp(Name, "PauseLifetime"))       PauseLifetime      = atoi(Value);
  else if (!strcasecmp(Name, "UseSubtitle"))         UseSubtitle        = atoi(Value);
  else if (!strcasecmp(Name, "RecordingDirs"))       RecordingDirs      = atoi(Value);
  else if (!strcasecmp(Name, "VideoFormat"))         VideoFormat        = atoi(Value);
  else if (!strcasecmp(Name, "RecordDolbyDigital"))  RecordDolbyDigital = atoi(Value);
  else if (!strcasecmp(Name, "ChannelInfoPos"))      ChannelInfoPos     = atoi(Value);
  else if (!strcasecmp(Name, "OSDwidth"))            OSDwidth           = atoi(Value);
  else if (!strcasecmp(Name, "OSDheight"))           OSDheight          = atoi(Value);
  else if (!strcasecmp(Name, "OSDMessageTime"))      OSDMessageTime     = atoi(Value);
  else if (!strcasecmp(Name, "MaxVideoFileSize"))    MaxVideoFileSize   = atoi(Value);
  else if (!strcasecmp(Name, "SplitEditedFiles"))    SplitEditedFiles   = atoi(Value);
  else if (!strcasecmp(Name, "MinEventTimeout"))     MinEventTimeout    = atoi(Value);
  else if (!strcasecmp(Name, "MinUserInactivity"))   MinUserInactivity  = atoi(Value);
  else if (!strcasecmp(Name, "MultiSpeedMode"))      MultiSpeedMode     = atoi(Value);
  else if (!strcasecmp(Name, "ShowReplayMode"))      ShowReplayMode     = atoi(Value);
  else if (!strcasecmp(Name, "ResumeID"))            ResumeID           = atoi(Value);
  else if (!strcasecmp(Name, "CaCaps"))              return ParseCaCaps(Value);
  else if (!strcasecmp(Name, "CurrentChannel"))      CurrentChannel     = atoi(Value);
  else if (!strcasecmp(Name, "CurrentVolume"))       CurrentVolume      = atoi(Value);
  else
     return false;
  return true;
}

bool cSetup::Save(void)
{
  Store("OSDLanguage",        OSDLanguage);
  Store("PrimaryDVB",         PrimaryDVB);
  Store("ShowInfoOnChSwitch", ShowInfoOnChSwitch);
  Store("MenuScrollPage",     MenuScrollPage);
  Store("MarkInstantRecord",  MarkInstantRecord);
  Store("NameInstantRecord",  NameInstantRecord);
  Store("InstantRecordTime",  InstantRecordTime);
  Store("LnbSLOF",            LnbSLOF);
  Store("LnbFrequLo",         LnbFrequLo);
  Store("LnbFrequHi",         LnbFrequHi);
  Store("DiSEqC",             DiSEqC);
  Store("SetSystemTime",      SetSystemTime);
  Store("TimeTransponder",    TimeTransponder);
  Store("MarginStart",        MarginStart);
  Store("MarginStop",         MarginStop);
  Store("EPGScanTimeout",     EPGScanTimeout);
  Store("EPGBugfixLevel",     EPGBugfixLevel);
  Store("SVDRPTimeout",       SVDRPTimeout);
  Store("ZapTimeout",         ZapTimeout);
  Store("SortTimers",         SortTimers);
  Store("PrimaryLimit",       PrimaryLimit);
  Store("DefaultPriority",    DefaultPriority);
  Store("DefaultLifetime",    DefaultLifetime);
  Store("PausePriority",      PausePriority);
  Store("PauseLifetime",      PauseLifetime);
  Store("UseSubtitle",        UseSubtitle);
  Store("RecordingDirs",      RecordingDirs);
  Store("VideoFormat",        VideoFormat);
  Store("RecordDolbyDigital", RecordDolbyDigital);
  Store("ChannelInfoPos",     ChannelInfoPos);
  Store("OSDwidth",           OSDwidth);
  Store("OSDheight",          OSDheight);
  Store("OSDMessageTime",     OSDMessageTime);
  Store("MaxVideoFileSize",   MaxVideoFileSize);
  Store("SplitEditedFiles",   SplitEditedFiles);
  Store("MinEventTimeout",    MinEventTimeout);
  Store("MinUserInactivity",  MinUserInactivity);
  Store("MultiSpeedMode",     MultiSpeedMode);
  Store("ShowReplayMode",     ShowReplayMode);
  Store("ResumeID",           ResumeID);
  StoreCaCaps("CaCaps");
  Store("CurrentChannel",     CurrentChannel);
  Store("CurrentVolume",      CurrentVolume);

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     isyslog("saved setup to %s", FileName());
     return true;
     }
  return false;
}
