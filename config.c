/*
 * config.c: Configuration file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: config.c 5.1 2024/03/04 21:13:58 kls Exp $
 */

#include "config.h"
#include <ctype.h>
#include <stdlib.h>
#include "device.h"
#include "i18n.h"
#include "interface.h"
#include "menu.h"
#include "plugin.h"
#include "recording.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

#define ChkDoublePlausibility(Variable, Default) { if (Variable < 0.00001) Variable = Default; }

// --- cSVDRPhost ------------------------------------------------------------

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
     else {
        mask <<= (32 - m);
        mask = htonl(mask);
        }
     }
  int result = inet_aton(s, &addr);
  if (p)
     *(char *)p = '/'; // there it is again
  return result != 0 && (mask != 0 || addr.s_addr == 0);
}

bool cSVDRPhost::IsLocalhost(void)
{
  return addr.s_addr == htonl(INADDR_LOOPBACK);
}

bool cSVDRPhost::Accepts(in_addr_t Address)
{
  return (Address & mask) == (addr.s_addr & mask);
}

// --- cSatCableNumbers ------------------------------------------------------

cSatCableNumbers::cSatCableNumbers(int Size, const char *s)
{
  size = Size;
  array = MALLOC(int, size);
  FromString(s);
}

cSatCableNumbers::~cSatCableNumbers()
{
  free(array);
}

bool cSatCableNumbers::FromString(const char *s)
{
  char *t;
  int i = 0;
  const char *p = s;
  while (p && *p) {
        int n = strtol(p, &t, 10);
        if (t != p) {
           if (i < size)
              array[i++] = n;
           else {
              esyslog("ERROR: too many sat cable numbers in '%s'", s);
              return false;
              }
           }
        else {
           esyslog("ERROR: invalid sat cable number in '%s'", s);
           return false;
           }
        p = skipspace(t);
        }
  for ( ; i < size; i++)
      array[i] = 0;
  return true;
}

cString cSatCableNumbers::ToString(void)
{
  cString s("");
  for (int i = 0; i < size; i++) {
      s = cString::sprintf("%s%d ", *s, array[i]);
      }
  return s;
}

int cSatCableNumbers::FirstDeviceIndex(int DeviceIndex) const
{
  if (0 <= DeviceIndex && DeviceIndex < size) {
     if (int CableNr = array[DeviceIndex]) {
        for (int i = 0; i < size; i++) {
            if (i < DeviceIndex && array[i] == CableNr)
               return i;
            }
        }
     }
  return -1;
}

// --- cNestedItem -----------------------------------------------------------

cNestedItem::cNestedItem(const char *Text, bool WithSubItems)
{
  text = strdup(Text ? Text : "");
  subItems = WithSubItems ? new cList<cNestedItem> : NULL;
}

cNestedItem::~cNestedItem()
{
  delete subItems;
  free(text);
}

int cNestedItem::Compare(const cListObject &ListObject) const
{
  return strcasecmp(text, ((cNestedItem *)&ListObject)->text);
}

void cNestedItem::AddSubItem(cNestedItem *Item)
{
  if (!subItems)
     subItems = new cList<cNestedItem>;
  if (Item)
     subItems->Add(Item);
}

void cNestedItem::SetText(const char *Text)
{
  free(text);
  text = strdup(Text ? Text : "");
}

void cNestedItem::SetSubItems(bool On)
{
  if (On && !subItems)
     subItems = new cList<cNestedItem>;
  else if (!On && subItems) {
     delete subItems;
     subItems = NULL;
     }
}

// --- cNestedItemList -------------------------------------------------------

cNestedItemList::cNestedItemList(void)
{
  fileName = NULL;
}

cNestedItemList::~cNestedItemList()
{
  free(fileName);
}

bool cNestedItemList::Parse(FILE *f, cList<cNestedItem> *List, int &Line)
{
  char *s;
  cReadLine ReadLine;
  while ((s = ReadLine.Read(f)) != NULL) {
        Line++;
        char *p = strchr(s, '#');
        if (p)
           *p = 0;
        s = skipspace(stripspace(s));
        if (!isempty(s)) {
           p = s + strlen(s) - 1;
           if (*p == '{') {
              *p = 0;
              stripspace(s);
              cNestedItem *Item = new cNestedItem(s, true);
              List->Add(Item);
              if (!Parse(f, Item->SubItems(), Line))
                 return false;
              }
           else if (*s == '}')
              break;
           else
              List->Add(new cNestedItem(s));
           }
        }
  return true;
}

bool cNestedItemList::Write(FILE *f, cList<cNestedItem> *List, int Indent)
{
  for (cNestedItem *Item = List->First(); Item; Item = List->Next(Item)) {
      if (Item->SubItems()) {
         fprintf(f, "%*s%s {\n", Indent, "", Item->Text());
         Write(f, Item->SubItems(), Indent + 2);
         fprintf(f, "%*s}\n", Indent + 2, "");
         }
      else
         fprintf(f, "%*s%s\n", Indent, "", Item->Text());
      }
  return true;
}

void cNestedItemList::Clear(void)
{
  free(fileName);
  fileName = NULL;
  cList<cNestedItem>::Clear();
}

bool cNestedItemList::Load(const char *FileName)
{
  cList<cNestedItem>::Clear();
  if (FileName) {
     free(fileName);
     fileName = strdup(FileName);
     }
  bool result = false;
  if (fileName && access(fileName, F_OK) == 0) {
     isyslog("loading %s", fileName);
     FILE *f = fopen(fileName, "r");
     if (f) {
        int Line = 0;
        result = Parse(f, this, Line);
        fclose(f);
        }
     else {
        LOG_ERROR_STR(fileName);
        result = false;
        }
     }
  return result;
}

bool cNestedItemList::Save(void)
{
  bool result = true;
  cSafeFile f(fileName);
  if (f.Open()) {
     result = Write(f, this);
     if (!f.Close())
        result = false;
     }
  else
     result = false;
  return result;
}

// --- Folders and Commands --------------------------------------------------

cNestedItemList Folders;
cNestedItemList Commands;
cNestedItemList RecordingCommands;

// --- cSVDRPhosts -----------------------------------------------------------

cSVDRPhosts SVDRPhosts;

bool cSVDRPhosts::LocalhostOnly(void)
{
  cSVDRPhost *h = First();
  while (h) {
        if (!h->IsLocalhost())
           return false;
        h = (cSVDRPhost *)h->Next();
        }
  return true;
}

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

// --- cSetupLine ------------------------------------------------------------

cSetupLine::cSetupLine(void)
{
  plugin = name = value = NULL;
}

cSetupLine::cSetupLine(const char *Name, const char *Value, const char *Plugin)
{
  name = strreplace(strdup(Name), '\n', 0);
  value = strreplace(strdup(Value), '\n', 0);
  plugin = Plugin ? strreplace(strdup(Plugin), '\n', 0) : NULL;
}

cSetupLine::~cSetupLine()
{
  free(plugin);
  free(name);
  free(value);
}

int cSetupLine::Compare(const cListObject &ListObject) const
{
  const cSetupLine *sl = (cSetupLine *)&ListObject;
  if (!plugin && !sl->plugin)
     return strcasecmp(name, sl->name);
  if (!plugin)
     return -1;
  if (!sl->plugin)
     return 1;
  int result = strcasecmp(plugin, sl->plugin);
  if (result == 0)
     result = strcasecmp(name, sl->name);
  return result;
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

// --- cSetup ----------------------------------------------------------------

cSetup Setup;

cSetup::cSetup(void)
{
  strcpy(OSDLanguage, ""); // default is taken from environment
  strcpy(OSDSkin, "lcars");
  strcpy(OSDTheme, "default");
  PrimaryDVB = 1;
  ShowInfoOnChSwitch = 1;
  TimeoutRequChInfo = 1;
  MenuScrollPage = 1;
  MenuScrollWrap = 0;
  MenuKeyCloses = 0;
  MarkInstantRecord = 1;
  strcpy(NameInstantRecord, TIMERMACRO_TITLE " " TIMERMACRO_EPISODE);
  InstantRecordTime = DEFINSTRECTIME;
  LnbSLOF    = 11700;
  LnbFrequLo =  9750;
  LnbFrequHi = 10600;
  DiSEqC = 0;
  UsePositioner = 0;
  SiteLat = 0;
  SiteLon = 0;
  PositionerSpeed = 15;
  PositionerSwing = 650;
  PositionerLastLon = 0;
  SetSystemTime = 0;
  TimeSource = 0;
  TimeTransponder = 0;
  StandardCompliance = STANDARD_DVB;
  MarginStart = 2;
  MarginStop = 10;
  AudioLanguages[0] = -1;
  DisplaySubtitles = 0;
  SubtitleLanguages[0] = -1;
  SubtitleOffset = 0;
  SubtitleFgTransparency = 0;
  SubtitleBgTransparency = 0;
  EPGLanguages[0] = -1;
  EPGScanTimeout = 5;
  EPGScanMaxChannel = 0;
  EPGPauseAfterScan = 0;
  EPGBugfixLevel = 3;
  EPGLinger = 0;
  SVDRPTimeout = 300;
  SVDRPPeering = 0;
  strn0cpy(SVDRPHostName, GetHostName(), sizeof(SVDRPHostName));
  strcpy(SVDRPDefaultHost, "");
  ZapTimeout = 3;
  ChannelEntryTimeout = 1000;
  RcRepeatDelay = 300;
  RcRepeatDelta = 100;
  DefaultPriority = 50;
  DefaultLifetime = MAXLIFETIME;
  RecordKeyHandling = 2;
  PauseKeyHandling = 2;
  PausePriority = 10;
  PauseLifetime = 1;
  UseSubtitle = 1;
  UseVps = 0;
  VpsMargin = 120;
  RecordingDirs = 1;
  FoldersInTimerMenu = 1;
  AlwaysSortFoldersFirst = 1;
  DefaultSortModeRec = rsmTime;
  RecSortingDirection = rsdAscending;
  NumberKeysForChars = 1;
  ColorKey0 = 0;
  ColorKey1 = 1;
  ColorKey2 = 2;
  ColorKey3 = 3;
  VideoDisplayFormat = 1;
  VideoFormat = 0;
  UpdateChannels = 5;
  UseDolbyDigital = 1;
  ChannelInfoPos = 0;
  ChannelInfoTime = 5;
  OSDLeftP = 0.08;
  OSDTopP = 0.08;
  OSDWidthP = 0.87;
  OSDHeightP = 0.84;
  OSDLeft = 54;
  OSDTop = 45;
  OSDWidth = 624;
  OSDHeight = 486;
  OSDAspect = 1.0;
  OSDMessageTime = 1;
  UseSmallFont = 1;
  AntiAlias = 1;
  strcpy(FontOsd, DefaultFontOsd);
  strcpy(FontSml, DefaultFontSml);
  strcpy(FontFix, DefaultFontFix);
  FontOsdSizeP = 0.031;
  FontSmlSizeP = 0.028;
  FontFixSizeP = 0.030;
  FontOsdSize = 22;
  FontSmlSize = 18;
  FontFixSize = 20;
  MaxVideoFileSize = MAXVIDEOFILESIZEDEFAULT;
  SplitEditedFiles = 0;
  DelTimeshiftRec = 0;
  MinEventTimeout = 30;
  MinUserInactivity = 300;
  NextWakeupTime = 0;
  MultiSpeedMode = 0;
  ShowReplayMode = 0;
  ShowRemainingTime = 0;
  ProgressDisplayTime = 0;
  PauseOnMarkSet = 0;
  PauseOnMarkJump = 1;
  SkipEdited = 0;
  PauseAtLastMark = 0;
  AdaptiveSkipInitial = 120;
  AdaptiveSkipTimeout = 3;
  AdaptiveSkipAlternate = 0;
  AdaptiveSkipPrevNext = 0;
  SkipSeconds = 60;
  SkipSecondsRepeat = 60;
  ResumeID = 0;
  CurrentChannel = -1;
  CurrentVolume = MAXVOLUME;
  VolumeSteps = 51;
  VolumeLinearize = 0;
  CurrentDolby = 0;
  InitialChannel = "";
  DeviceBondings = "";
  InitialVolume = -1;
  ChannelsWrap = 0;
  ShowChannelNamesWithSource = 0;
  EmergencyExit = 1;
}

cSetup& cSetup::operator= (const cSetup &s)
{
  memcpy(&__BeginData__, &s.__BeginData__, (char *)&s.__EndData__ - (char *)&s.__BeginData__);
  InitialChannel = s.InitialChannel;
  DeviceBondings = s.DeviceBondings;
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
  Store(Name, cString::sprintf("%d", Value), Plugin);
}

void cSetup::Store(const char *Name, double &Value, const char *Plugin)
{
  Store(Name, dtoa(Value), Plugin);
}

bool cSetup::Load(const char *FileName)
{
  if (cConfig<cSetupLine>::Load(FileName)) {
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

void cSetup::StoreLanguages(const char *Name, int *Values)
{
  char buffer[I18nLanguages()->Size() * 4];
  char *q = buffer;
  for (int i = 0; i < I18nLanguages()->Size(); i++) {
      if (Values[i] < 0)
         break;
      const char *s = I18nLanguageCode(Values[i]);
      if (s) {
         if (q > buffer)
            *q++ = ' ';
         strncpy(q, s, 3);
         q += 3;
         }
      }
  *q = 0;
  Store(Name, buffer);
}

bool cSetup::ParseLanguages(const char *Value, int *Values)
{
  int n = 0;
  while (Value && *Value && n < I18nLanguages()->Size()) {
        char buffer[4];
        strn0cpy(buffer, Value, sizeof(buffer));
        int i = I18nLanguageIndex(buffer);
        if (i >= 0)
           Values[n++] = i;
        if ((Value = strchr(Value, ' ')) != NULL)
           Value++;
        }
  Values[n] = -1;
  return true;
}

bool cSetup::Parse(const char *Name, const char *Value)
{
  if      (!strcasecmp(Name, "OSDLanguage"))       { strn0cpy(OSDLanguage, Value, sizeof(OSDLanguage)); I18nSetLocale(OSDLanguage); }
  else if (!strcasecmp(Name, "OSDSkin"))             Utf8Strn0Cpy(OSDSkin, Value, MaxSkinName);
  else if (!strcasecmp(Name, "OSDTheme"))            Utf8Strn0Cpy(OSDTheme, Value, MaxThemeName);
  else if (!strcasecmp(Name, "PrimaryDVB"))          PrimaryDVB         = atoi(Value);
  else if (!strcasecmp(Name, "ShowInfoOnChSwitch"))  ShowInfoOnChSwitch = atoi(Value);
  else if (!strcasecmp(Name, "TimeoutRequChInfo"))   TimeoutRequChInfo  = atoi(Value);
  else if (!strcasecmp(Name, "MenuScrollPage"))      MenuScrollPage     = atoi(Value);
  else if (!strcasecmp(Name, "MenuScrollWrap"))      MenuScrollWrap     = atoi(Value);
  else if (!strcasecmp(Name, "MenuKeyCloses"))       MenuKeyCloses      = atoi(Value);
  else if (!strcasecmp(Name, "MarkInstantRecord"))   MarkInstantRecord  = atoi(Value);
  else if (!strcasecmp(Name, "NameInstantRecord"))   Utf8Strn0Cpy(NameInstantRecord, Value, sizeof(NameInstantRecord));
  else if (!strcasecmp(Name, "InstantRecordTime"))   InstantRecordTime  = atoi(Value);
  else if (!strcasecmp(Name, "LnbSLOF"))             LnbSLOF            = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequLo"))          LnbFrequLo         = atoi(Value);
  else if (!strcasecmp(Name, "LnbFrequHi"))          LnbFrequHi         = atoi(Value);
  else if (!strcasecmp(Name, "DiSEqC"))              DiSEqC             = atoi(Value);
  else if (!strcasecmp(Name, "UsePositioner"))       UsePositioner      = atoi(Value);
  else if (!strcasecmp(Name, "SiteLat"))             SiteLat            = atoi(Value);
  else if (!strcasecmp(Name, "SiteLon"))             SiteLon            = atoi(Value);
  else if (!strcasecmp(Name, "PositionerSpeed"))     PositionerSpeed    = atoi(Value);
  else if (!strcasecmp(Name, "PositionerSwing"))     PositionerSwing    = atoi(Value);
  else if (!strcasecmp(Name, "PositionerLastLon"))   PositionerLastLon  = atoi(Value);
  else if (!strcasecmp(Name, "SetSystemTime"))       SetSystemTime      = atoi(Value);
  else if (!strcasecmp(Name, "TimeSource"))          TimeSource         = cSource::FromString(Value);
  else if (!strcasecmp(Name, "TimeTransponder"))     TimeTransponder    = atoi(Value);
  else if (!strcasecmp(Name, "StandardCompliance"))  StandardCompliance = atoi(Value);
  else if (!strcasecmp(Name, "MarginStart"))         MarginStart        = atoi(Value);
  else if (!strcasecmp(Name, "MarginStop"))          MarginStop         = atoi(Value);
  else if (!strcasecmp(Name, "AudioLanguages"))      return ParseLanguages(Value, AudioLanguages);
  else if (!strcasecmp(Name, "DisplaySubtitles"))    DisplaySubtitles   = atoi(Value);
  else if (!strcasecmp(Name, "SubtitleLanguages"))   return ParseLanguages(Value, SubtitleLanguages);
  else if (!strcasecmp(Name, "SubtitleOffset"))      SubtitleOffset     = atoi(Value);
  else if (!strcasecmp(Name, "SubtitleFgTransparency")) SubtitleFgTransparency = atoi(Value);
  else if (!strcasecmp(Name, "SubtitleBgTransparency")) SubtitleBgTransparency = atoi(Value);
  else if (!strcasecmp(Name, "EPGLanguages"))        return ParseLanguages(Value, EPGLanguages);
  else if (!strcasecmp(Name, "EPGScanTimeout"))      EPGScanTimeout     = atoi(Value);
  else if (!strcasecmp(Name, "EPGScanMaxChannel"))   EPGScanMaxChannel  = atoi(Value);
  else if (!strcasecmp(Name, "EPGPauseAfterScan"))   EPGPauseAfterScan  = atoi(Value);
  else if (!strcasecmp(Name, "EPGBugfixLevel"))      EPGBugfixLevel     = atoi(Value);
  else if (!strcasecmp(Name, "EPGLinger"))           EPGLinger          = atoi(Value);
  else if (!strcasecmp(Name, "SVDRPTimeout"))        SVDRPTimeout       = atoi(Value);
  else if (!strcasecmp(Name, "SVDRPPeering"))        SVDRPPeering       = atoi(Value);
  else if (!strcasecmp(Name, "SVDRPHostName"))     { if (*Value) strn0cpy(SVDRPHostName, Value, sizeof(SVDRPHostName)); }
  else if (!strcasecmp(Name, "SVDRPDefaultHost"))    strn0cpy(SVDRPDefaultHost, Value, sizeof(SVDRPDefaultHost));
  else if (!strcasecmp(Name, "ZapTimeout"))          ZapTimeout         = atoi(Value);
  else if (!strcasecmp(Name, "ChannelEntryTimeout")) ChannelEntryTimeout= atoi(Value);
  else if (!strcasecmp(Name, "RcRepeatDelay"))       RcRepeatDelay      = atoi(Value);
  else if (!strcasecmp(Name, "RcRepeatDelta"))       RcRepeatDelta      = atoi(Value);
  else if (!strcasecmp(Name, "DefaultPriority"))     DefaultPriority    = atoi(Value);
  else if (!strcasecmp(Name, "DefaultLifetime"))     DefaultLifetime    = atoi(Value);
  else if (!strcasecmp(Name, "RecordKeyHandling"))   RecordKeyHandling  = atoi(Value);
  else if (!strcasecmp(Name, "PauseKeyHandling"))    PauseKeyHandling   = atoi(Value);
  else if (!strcasecmp(Name, "PausePriority"))       PausePriority      = atoi(Value);
  else if (!strcasecmp(Name, "PauseLifetime"))       PauseLifetime      = atoi(Value);
  else if (!strcasecmp(Name, "UseSubtitle"))         UseSubtitle        = atoi(Value);
  else if (!strcasecmp(Name, "UseVps"))              UseVps             = atoi(Value);
  else if (!strcasecmp(Name, "VpsMargin"))           VpsMargin          = atoi(Value);
  else if (!strcasecmp(Name, "RecordingDirs"))       RecordingDirs      = atoi(Value);
  else if (!strcasecmp(Name, "FoldersInTimerMenu"))  FoldersInTimerMenu = atoi(Value);
  else if (!strcasecmp(Name, "AlwaysSortFoldersFirst")) AlwaysSortFoldersFirst = atoi(Value);
  else if (!strcasecmp(Name, "RecSortingDirection")) RecSortingDirection= atoi(Value);
  else if (!strcasecmp(Name, "DefaultSortModeRec"))  DefaultSortModeRec = atoi(Value);
  else if (!strcasecmp(Name, "NumberKeysForChars"))  NumberKeysForChars = atoi(Value);
  else if (!strcasecmp(Name, "ColorKey0"))           ColorKey0          = atoi(Value);
  else if (!strcasecmp(Name, "ColorKey1"))           ColorKey1          = atoi(Value);
  else if (!strcasecmp(Name, "ColorKey2"))           ColorKey2          = atoi(Value);
  else if (!strcasecmp(Name, "ColorKey3"))           ColorKey3          = atoi(Value);
  else if (!strcasecmp(Name, "VideoDisplayFormat"))  VideoDisplayFormat = atoi(Value);
  else if (!strcasecmp(Name, "VideoFormat"))         VideoFormat        = atoi(Value);
  else if (!strcasecmp(Name, "UpdateChannels"))      UpdateChannels     = atoi(Value);
  else if (!strcasecmp(Name, "UseDolbyDigital"))     UseDolbyDigital    = atoi(Value);
  else if (!strcasecmp(Name, "ChannelInfoPos"))      ChannelInfoPos     = atoi(Value);
  else if (!strcasecmp(Name, "ChannelInfoTime"))     ChannelInfoTime    = atoi(Value);
  else if (!strcasecmp(Name, "OSDLeftP"))            OSDLeftP           = atod(Value);
  else if (!strcasecmp(Name, "OSDTopP"))             OSDTopP            = atod(Value);
  else if (!strcasecmp(Name, "OSDWidthP"))         { OSDWidthP          = atod(Value); ChkDoublePlausibility(OSDWidthP, 0.87); }
  else if (!strcasecmp(Name, "OSDHeightP"))        { OSDHeightP         = atod(Value); ChkDoublePlausibility(OSDHeightP, 0.84); }
  else if (!strcasecmp(Name, "OSDLeft"))             OSDLeft            = atoi(Value);
  else if (!strcasecmp(Name, "OSDTop"))              OSDTop             = atoi(Value);
  else if (!strcasecmp(Name, "OSDWidth"))          { OSDWidth           = atoi(Value); OSDWidth &= ~0x07; } // OSD width must be a multiple of 8
  else if (!strcasecmp(Name, "OSDHeight"))           OSDHeight          = atoi(Value);
  else if (!strcasecmp(Name, "OSDAspect"))           OSDAspect          = atod(Value);
  else if (!strcasecmp(Name, "OSDMessageTime"))      OSDMessageTime     = atoi(Value);
  else if (!strcasecmp(Name, "UseSmallFont"))        UseSmallFont       = atoi(Value);
  else if (!strcasecmp(Name, "AntiAlias"))           AntiAlias          = atoi(Value);
  else if (!strcasecmp(Name, "FontOsd"))             Utf8Strn0Cpy(FontOsd, Value, MAXFONTNAME);
  else if (!strcasecmp(Name, "FontSml"))             Utf8Strn0Cpy(FontSml, Value, MAXFONTNAME);
  else if (!strcasecmp(Name, "FontFix"))             Utf8Strn0Cpy(FontFix, Value, MAXFONTNAME);
  else if (!strcasecmp(Name, "FontOsdSizeP"))      { FontOsdSizeP       = atod(Value); ChkDoublePlausibility(FontOsdSizeP, 0.038); }
  else if (!strcasecmp(Name, "FontSmlSizeP"))      { FontSmlSizeP       = atod(Value); ChkDoublePlausibility(FontSmlSizeP, 0.035); }
  else if (!strcasecmp(Name, "FontFixSizeP"))      { FontFixSizeP       = atod(Value); ChkDoublePlausibility(FontFixSizeP, 0.031); }
  else if (!strcasecmp(Name, "FontOsdSize"))         FontOsdSize        = atoi(Value);
  else if (!strcasecmp(Name, "FontSmlSize"))         FontSmlSize        = atoi(Value);
  else if (!strcasecmp(Name, "FontFixSize"))         FontFixSize        = atoi(Value);
  else if (!strcasecmp(Name, "MaxVideoFileSize"))    MaxVideoFileSize   = atoi(Value);
  else if (!strcasecmp(Name, "SplitEditedFiles"))    SplitEditedFiles   = atoi(Value);
  else if (!strcasecmp(Name, "DelTimeshiftRec"))     DelTimeshiftRec    = atoi(Value);
  else if (!strcasecmp(Name, "MinEventTimeout"))     MinEventTimeout    = atoi(Value);
  else if (!strcasecmp(Name, "MinUserInactivity"))   MinUserInactivity  = atoi(Value);
  else if (!strcasecmp(Name, "NextWakeupTime"))      NextWakeupTime     = atoi(Value);
  else if (!strcasecmp(Name, "MultiSpeedMode"))      MultiSpeedMode     = atoi(Value);
  else if (!strcasecmp(Name, "ShowReplayMode"))      ShowReplayMode     = atoi(Value);
  else if (!strcasecmp(Name, "ShowRemainingTime"))   ShowRemainingTime  = atoi(Value);
  else if (!strcasecmp(Name, "ProgressDisplayTime")) ProgressDisplayTime= atoi(Value);
  else if (!strcasecmp(Name, "PauseOnMarkSet"))      PauseOnMarkSet     = atoi(Value);
  else if (!strcasecmp(Name, "PauseOnMarkJump"))     PauseOnMarkJump    = atoi(Value);
  else if (!strcasecmp(Name, "SkipEdited"))          SkipEdited         = atoi(Value);
  else if (!strcasecmp(Name, "PauseAtLastMark"))     PauseAtLastMark    = atoi(Value);
  else if (!strcasecmp(Name, "AdaptiveSkipInitial")) AdaptiveSkipInitial= atoi(Value);
  else if (!strcasecmp(Name, "AdaptiveSkipTimeout")) AdaptiveSkipTimeout= atoi(Value);
  else if (!strcasecmp(Name, "AdaptiveSkipAlternate")) AdaptiveSkipAlternate = atoi(Value);
  else if (!strcasecmp(Name, "AdaptiveSkipPrevNext")) AdaptiveSkipPrevNext = atoi(Value);
  else if (!strcasecmp(Name, "SkipSeconds"))         SkipSeconds        = atoi(Value);
  else if (!strcasecmp(Name, "SkipSecondsRepeat"))   SkipSecondsRepeat  = atoi(Value);
  else if (!strcasecmp(Name, "ResumeID"))            ResumeID           = atoi(Value);
  else if (!strcasecmp(Name, "CurrentChannel"))      CurrentChannel     = atoi(Value);
  else if (!strcasecmp(Name, "CurrentVolume"))       CurrentVolume      = atoi(Value);
  else if (!strcasecmp(Name, "CurrentDolby"))        CurrentDolby       = atoi(Value);
  else if (!strcasecmp(Name, "InitialChannel"))      InitialChannel     = Value;
  else if (!strcasecmp(Name, "VolumeSteps"))         VolumeSteps        = atoi(Value);
  else if (!strcasecmp(Name, "VolumeLinearize"))     VolumeLinearize    = atoi(Value);
  else if (!strcasecmp(Name, "InitialVolume"))       InitialVolume      = atoi(Value);
  else if (!strcasecmp(Name, "DeviceBondings"))      DeviceBondings     = Value;
  else if (!strcasecmp(Name, "ChannelsWrap"))        ChannelsWrap       = atoi(Value);
  else if (!strcasecmp(Name, "ShowChannelNamesWithSource")) ShowChannelNamesWithSource = atoi(Value);
  else if (!strcasecmp(Name, "EmergencyExit"))       EmergencyExit      = atoi(Value);
  else if (!strcasecmp(Name, "LastReplayed"))        cReplayControl::SetRecording(Value);
  else
     return false;
  return true;
}

bool cSetup::Save(void)
{
  Store("OSDLanguage",        OSDLanguage);
  Store("OSDSkin",            OSDSkin);
  Store("OSDTheme",           OSDTheme);
  Store("PrimaryDVB",         PrimaryDVB);
  Store("ShowInfoOnChSwitch", ShowInfoOnChSwitch);
  Store("TimeoutRequChInfo",  TimeoutRequChInfo);
  Store("MenuScrollPage",     MenuScrollPage);
  Store("MenuScrollWrap",     MenuScrollWrap);
  Store("MenuKeyCloses",      MenuKeyCloses);
  Store("MarkInstantRecord",  MarkInstantRecord);
  Store("NameInstantRecord",  NameInstantRecord);
  Store("InstantRecordTime",  InstantRecordTime);
  Store("LnbSLOF",            LnbSLOF);
  Store("LnbFrequLo",         LnbFrequLo);
  Store("LnbFrequHi",         LnbFrequHi);
  Store("DiSEqC",             DiSEqC);
  Store("UsePositioner",      UsePositioner);
  Store("SiteLat",            SiteLat);
  Store("SiteLon",            SiteLon);
  Store("PositionerSpeed",    PositionerSpeed);
  Store("PositionerSwing",    PositionerSwing);
  Store("PositionerLastLon",  PositionerLastLon);
  Store("SetSystemTime",      SetSystemTime);
  Store("TimeSource",         cSource::ToString(TimeSource));
  Store("TimeTransponder",    TimeTransponder);
  Store("StandardCompliance", StandardCompliance);
  Store("MarginStart",        MarginStart);
  Store("MarginStop",         MarginStop);
  StoreLanguages("AudioLanguages", AudioLanguages);
  Store("DisplaySubtitles",   DisplaySubtitles);
  StoreLanguages("SubtitleLanguages", SubtitleLanguages);
  Store("SubtitleOffset",     SubtitleOffset);
  Store("SubtitleFgTransparency", SubtitleFgTransparency);
  Store("SubtitleBgTransparency", SubtitleBgTransparency);
  StoreLanguages("EPGLanguages", EPGLanguages);
  Store("EPGScanTimeout",     EPGScanTimeout);
  Store("EPGScanMaxChannel",  EPGScanMaxChannel);
  Store("EPGPauseAfterScan",  EPGPauseAfterScan);
  Store("EPGBugfixLevel",     EPGBugfixLevel);
  Store("EPGLinger",          EPGLinger);
  Store("SVDRPTimeout",       SVDRPTimeout);
  Store("SVDRPPeering",       SVDRPPeering);
  Store("SVDRPHostName",      strcmp(SVDRPHostName, GetHostName()) ? SVDRPHostName : "");
  Store("SVDRPDefaultHost",   SVDRPDefaultHost);
  Store("ZapTimeout",         ZapTimeout);
  Store("ChannelEntryTimeout",ChannelEntryTimeout);
  Store("RcRepeatDelay",      RcRepeatDelay);
  Store("RcRepeatDelta",      RcRepeatDelta);
  Store("DefaultPriority",    DefaultPriority);
  Store("DefaultLifetime",    DefaultLifetime);
  Store("RecordKeyHandling",  RecordKeyHandling);
  Store("PauseKeyHandling",   PauseKeyHandling);
  Store("PausePriority",      PausePriority);
  Store("PauseLifetime",      PauseLifetime);
  Store("UseSubtitle",        UseSubtitle);
  Store("UseVps",             UseVps);
  Store("VpsMargin",          VpsMargin);
  Store("RecordingDirs",      RecordingDirs);
  Store("FoldersInTimerMenu", FoldersInTimerMenu);
  Store("AlwaysSortFoldersFirst", AlwaysSortFoldersFirst);
  Store("RecSortingDirection",RecSortingDirection);
  Store("DefaultSortModeRec", DefaultSortModeRec);
  Store("NumberKeysForChars", NumberKeysForChars);
  Store("ColorKey0",          ColorKey0);
  Store("ColorKey1",          ColorKey1);
  Store("ColorKey2",          ColorKey2);
  Store("ColorKey3",          ColorKey3);
  Store("VideoDisplayFormat", VideoDisplayFormat);
  Store("VideoFormat",        VideoFormat);
  Store("UpdateChannels",     UpdateChannels);
  Store("UseDolbyDigital",    UseDolbyDigital);
  Store("ChannelInfoPos",     ChannelInfoPos);
  Store("ChannelInfoTime",    ChannelInfoTime);
  Store("OSDLeftP",           OSDLeftP);
  Store("OSDTopP",            OSDTopP);
  Store("OSDWidthP",          OSDWidthP);
  Store("OSDHeightP",         OSDHeightP);
  Store("OSDLeft",            OSDLeft);
  Store("OSDTop",             OSDTop);
  Store("OSDWidth",           OSDWidth);
  Store("OSDHeight",          OSDHeight);
  Store("OSDAspect",          OSDAspect);
  Store("OSDMessageTime",     OSDMessageTime);
  Store("UseSmallFont",       UseSmallFont);
  Store("AntiAlias",          AntiAlias);
  Store("FontOsd",            FontOsd);
  Store("FontSml",            FontSml);
  Store("FontFix",            FontFix);
  Store("FontOsdSizeP",       FontOsdSizeP);
  Store("FontSmlSizeP",       FontSmlSizeP);
  Store("FontFixSizeP",       FontFixSizeP);
  Store("FontOsdSize",        FontOsdSize);
  Store("FontSmlSize",        FontSmlSize);
  Store("FontFixSize",        FontFixSize);
  Store("MaxVideoFileSize",   MaxVideoFileSize);
  Store("SplitEditedFiles",   SplitEditedFiles);
  Store("DelTimeshiftRec",    DelTimeshiftRec);
  Store("MinEventTimeout",    MinEventTimeout);
  Store("MinUserInactivity",  MinUserInactivity);
  Store("NextWakeupTime",     NextWakeupTime);
  Store("MultiSpeedMode",     MultiSpeedMode);
  Store("ShowReplayMode",     ShowReplayMode);
  Store("ShowRemainingTime",  ShowRemainingTime);
  Store("ProgressDisplayTime",ProgressDisplayTime);
  Store("PauseOnMarkSet",     PauseOnMarkSet);
  Store("PauseOnMarkJump",    PauseOnMarkJump);
  Store("SkipEdited",         SkipEdited);
  Store("PauseAtLastMark",    PauseAtLastMark);
  Store("AdaptiveSkipInitial",AdaptiveSkipInitial);
  Store("AdaptiveSkipTimeout",AdaptiveSkipTimeout);
  Store("AdaptiveSkipAlternate", AdaptiveSkipAlternate);
  Store("AdaptiveSkipPrevNext", AdaptiveSkipPrevNext);
  Store("SkipSeconds",        SkipSeconds);
  Store("SkipSecondsRepeat",  SkipSecondsRepeat);
  Store("ResumeID",           ResumeID);
  Store("CurrentChannel",     CurrentChannel);
  Store("CurrentVolume",      CurrentVolume);
  Store("CurrentDolby",       CurrentDolby);
  Store("InitialChannel",     InitialChannel);
  Store("VolumeSteps",        VolumeSteps);
  Store("VolumeLinearize",    VolumeLinearize);
  Store("InitialVolume",      InitialVolume);
  Store("DeviceBondings",     DeviceBondings);
  Store("ChannelsWrap",       ChannelsWrap);
  Store("ShowChannelNamesWithSource", ShowChannelNamesWithSource);
  Store("EmergencyExit",      EmergencyExit);
  Store("LastReplayed",       cReplayControl::LastReplayed());

  Sort();

  if (cConfig<cSetupLine>::Save()) {
     isyslog("saved setup to %s", FileName());
     return true;
     }
  return false;
}
