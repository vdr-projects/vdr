/*
 * channels.c: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.c 1.5 2002/10/20 11:50:47 kls Exp $
 */

#include "channels.h"
#ifdef NEWSTRUCT
#include <linux/dvb/frontend.h>
#else
#include <ost/frontend.h>
#endif
#include <ctype.h>

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// -- Channel Parameter Maps -------------------------------------------------

const tChannelParameterMap InversionValues[] = {
  {   0, INVERSION_OFF },
  {   1, INVERSION_ON },
  { 999, INVERSION_AUTO },
  { -1 }
  };

const tChannelParameterMap BandwidthValues[] = {
  {   6, BANDWIDTH_6_MHZ },
  {   7, BANDWIDTH_7_MHZ },
  {   8, BANDWIDTH_8_MHZ },
#ifdef NEWSTRUCT
  { 999, BANDWIDTH_AUTO },
#endif
  { -1 }
  };

const tChannelParameterMap CoderateValues[] = {
  {   0, FEC_NONE },
  {  12, FEC_1_2 },
  {  23, FEC_2_3 },
  {  34, FEC_3_4 },
#ifdef NEWSTRUCT
  {  45, FEC_4_5 },
#endif
  {  56, FEC_5_6 },
#ifdef NEWSTRUCT
  {  67, FEC_6_7 },
#endif
  {  78, FEC_7_8 },
#ifdef NEWSTRUCT
  {  89, FEC_8_9 },
#endif
  { 999, FEC_AUTO },
  { -1 }
  };

const tChannelParameterMap ModulationValues[] = {
  {   0, QPSK },
  {  16, QAM_16 },
  {  32, QAM_32 },
  {  64, QAM_64 },
  { 128, QAM_128 },
  { 256, QAM_256 },
#ifdef NEWSTRUCT
  { 999, QAM_AUTO },
#endif
  { -1 }
  };

const tChannelParameterMap TransmissionValues[] = {
  {   2, TRANSMISSION_MODE_2K },
  {   8, TRANSMISSION_MODE_8K },
#ifdef NEWSTRUCT
  { 999, TRANSMISSION_MODE_AUTO },
#endif
  { -1 }
  };

const tChannelParameterMap GuardValues[] = {
  {   4, GUARD_INTERVAL_1_4 },
  {   8, GUARD_INTERVAL_1_8 },
  {  16, GUARD_INTERVAL_1_16 },
  {  32, GUARD_INTERVAL_1_32 },
#ifdef NEWSTRUCT
  { 999, GUARD_INTERVAL_AUTO },
#endif
  { -1 }
  };

const tChannelParameterMap HierarchyValues[] = {
  {   0, HIERARCHY_NONE },
  {   1, HIERARCHY_1 },
  {   2, HIERARCHY_2 },
  {   4, HIERARCHY_4 },
#ifdef NEWSTRUCT
  { 999, HIERARCHY_AUTO },
#endif
  { -1 }
  };

int UserIndex(int Value, const tChannelParameterMap *Map)
{
  const tChannelParameterMap *map = Map;
  while (map && map->userValue != -1) {
        if (map->userValue == Value)
           return map - Map;
        map++;
        }
  return -1;
}

int DriverIndex(int Value, const tChannelParameterMap *Map)
{
  const tChannelParameterMap *map = Map;
  while (map && map->userValue != -1) {
        if (map->driverValue == Value)
           return map - Map;
        map++;
        }
  return -1;
}

int MapToUser(int Value, const tChannelParameterMap *Map)
{
  int n = DriverIndex(Value, Map);
  if (n >= 0)
     return Map[n].userValue;
  return -1;
}

int MapToDriver(int Value, const tChannelParameterMap *Map)
{
  int n = UserIndex(Value, Map);
  if (n >= 0)
     return Map[n].driverValue;
  return -1;
}

// -- cChannel ---------------------------------------------------------------

char *cChannel::buffer = NULL;

cChannel::cChannel(void)
{
  *name = 0;
  frequency    = 0;
  source       = 0;
  srate        = 0;
  vpid         = 0;
  apid1        = 0;
  apid2        = 0;
  dpid1        = 0;
  dpid2        = 0;
  tpid         = 0;
  ca           = 0;
  sid          = 0;
  number       = 0;
  groupSep     = false;
  //XXX
  polarization = 'v';
  inversion    = INVERSION_AUTO;
  bandwidth    = BANDWIDTH_8_MHZ;
  coderateH    = FEC_AUTO;//XXX FEC_2_3
  coderateL    = FEC_1_2;//XXX
  modulation   = QAM_64;
  transmission = TRANSMISSION_MODE_2K;
  guard        = GUARD_INTERVAL_1_32;
  hierarchy    = HIERARCHY_NONE;
}

cChannel::cChannel(const cChannel *Channel)
{
  strcpy(name,   Channel ? Channel->name         : "Pro7");
  frequency    = Channel ? Channel->frequency    : 12480;
  source       = Channel ? Channel->source       : 0;
  srate        = Channel ? Channel->srate        : 27500;
  vpid         = Channel ? Channel->vpid         : 255;
  apid1        = Channel ? Channel->apid1        : 256;
  apid2        = Channel ? Channel->apid2        : 0;
  dpid1        = Channel ? Channel->dpid1        : 257;
  dpid2        = Channel ? Channel->dpid2        : 0;
  tpid         = Channel ? Channel->tpid         : 32;
  ca           = Channel ? Channel->ca           : 0;
  sid          = Channel ? Channel->sid          : 0;
  groupSep     = Channel ? Channel->groupSep     : false;
  //XXX
  polarization = Channel ? Channel->polarization : 'v';
  inversion    = Channel ? Channel->inversion    : INVERSION_AUTO;
  bandwidth    = Channel ? Channel->bandwidth    : BANDWIDTH_8_MHZ;
  coderateH    = Channel ? Channel->coderateH    : FEC_AUTO;//XXX FEC_2_3
  coderateL    = Channel ? Channel->coderateL    : FEC_1_2;//XXX
  modulation   = Channel ? Channel->modulation   : QAM_64;
  transmission = Channel ? Channel->transmission : TRANSMISSION_MODE_2K;
  guard        = Channel ? Channel->guard        : GUARD_INTERVAL_1_32;
  hierarchy    = Channel ? Channel->hierarchy    : HIERARCHY_NONE;
}

static int PrintParameter(char *p, char Name, int Value)
{
  //XXX return Value >= 0 && Value != 999 ? sprintf(p, "%c%d", Name, Value) : 0;
  //XXX let's store 999 for the moment, until we generally switch to the NEWSTRUCT
  //XXX driver (where the defaults will all be AUTO)
  return Value >= 0 && (Value != 999 || (Name != 'I' && Name != 'C')) ? sprintf(p, "%c%d", Name, Value) : 0;
}

const char *cChannel::ParametersToString(void)
{
  char type = *cSource::ToString(source);
#define ST(s) if (strchr(s, type))
  static char buffer[64];
  char *q = buffer;
  *q = 0;
  ST(" S ")  q += sprintf(q, "%c", polarization);
  ST("CST")  q += PrintParameter(q, 'I', MapToUser(inversion, InversionValues));
  ST("CST")  q += PrintParameter(q, 'C', MapToUser(coderateH, CoderateValues));
  ST("  T")  q += PrintParameter(q, 'D', MapToUser(coderateL, CoderateValues));
  ST("C T")  q += PrintParameter(q, 'M', MapToUser(modulation, ModulationValues));
  ST("  T")  q += PrintParameter(q, 'B', MapToUser(bandwidth, BandwidthValues));
  ST("  T")  q += PrintParameter(q, 'T', MapToUser(transmission, TransmissionValues));
  ST("  T")  q += PrintParameter(q, 'G', MapToUser(guard, GuardValues));
  ST("  T")  q += PrintParameter(q, 'Y', MapToUser(hierarchy, HierarchyValues));
  return buffer;
}

static const char *ParseParameter(const char *s, int &Value, const tChannelParameterMap *Map)
{
  if (*++s) {
     char *p = NULL;
     errno = 0;
     int n = strtol(s, &p, 10);
     if (!errno && p != s) {
        //XXX let's tolerate 999 for the moment, until we generally switch to the NEWSTRUCT
        //XXX driver (where the defaults will all be AUTO)
        //XXX Value = MapToDriver(n, Map);
        //XXX if (Value >= 0)
        //XXX return p;
        int v = MapToDriver(n, Map);
        if (v >= 0) {
           Value = v;
           return p;
           }
        else if (v == 999)
           return p;
        }
     }
  esyslog("ERROR: illegal value for parameter '%c'", *(s - 1));
  return NULL;
}

bool cChannel::StringToParameters(const char *s)
{
  while (s && *s) {
        switch (toupper(*s)) {
          case 'B': s = ParseParameter(s, bandwidth, BandwidthValues); break;
          case 'C': s = ParseParameter(s, coderateH, CoderateValues); break;
          case 'D': s = ParseParameter(s, coderateL, CoderateValues); break;
          case 'G': s = ParseParameter(s, guard, GuardValues); break;
          case 'H': polarization = *s++; break;
          case 'I': s = ParseParameter(s, inversion, InversionValues); break;
          // 'L' reserved for possible circular polarization
          case 'M': s = ParseParameter(s, modulation, ModulationValues); break;
          // 'R' reserved for possible circular polarization
          case 'T': s = ParseParameter(s, transmission, TransmissionValues); break;
          case 'V': polarization = *s++; break;
          case 'Y': s = ParseParameter(s, hierarchy, HierarchyValues); break;
          default: esyslog("ERROR: unknown parameter key '%c'", *s);
                   return false;
          }
        }
  return true;
}

const char *cChannel::ToText(cChannel *Channel)
{
  char buf[MaxChannelName * 2];
  char *s = Channel->name;
  if (strchr(s, ':')) {
     s = strcpy(buf, s);
     strreplace(s, ':', '|');
     }
  free(buffer);
  if (Channel->groupSep) {
     if (Channel->number)
        asprintf(&buffer, ":@%d %s\n", Channel->number, s);
     else
        asprintf(&buffer, ":%s\n", s);
     }
  else {
     char apidbuf[32];
     char *q = apidbuf;
     q += snprintf(q, sizeof(apidbuf), "%d", Channel->apid1);
     if (Channel->apid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->apid2);
     if (Channel->dpid1 || Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ";%d", Channel->dpid1);
     if (Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->dpid2);
     *q = 0;
     asprintf(&buffer, "%s:%d:%s:%s:%d:%d:%s:%d:%d:%d\n", s, Channel->frequency, Channel->ParametersToString(), cSource::ToString(Channel->source), Channel->srate, Channel->vpid, apidbuf, Channel->tpid, Channel->ca, Channel->sid);
     }
  return buffer;
}

const char *cChannel::ToText(void)
{
  return ToText(this);
}

bool cChannel::Parse(const char *s)
{
  if (*s == ':') {
     groupSep = true;
     if (*++s == '@' && *++s) {
        char *p = NULL;
        errno = 0;
        int n = strtol(s, &p, 10);
        if (!errno && p != s && n > 0) {
           number = n;
           s = p;
           }
        }
     strn0cpy(name, skipspace(s), MaxChannelName);
     }
  else {
     groupSep = false;
     char *namebuf = NULL;
     char *sourcebuf = NULL;
     char *parambuf = NULL;
     char *apidbuf = NULL;
     int fields = sscanf(s, "%a[^:]:%d :%a[^:]:%a[^:] :%d :%d :%a[^:]:%d :%d :%d ", &namebuf, &frequency, &parambuf, &sourcebuf, &srate, &vpid, &apidbuf, &tpid, &ca, &sid);
     if (fields >= 9) {
        if (fields == 9) {
           // allow reading of old format
           sid = ca;
           ca = tpid;
           tpid = 0;
           }
        apid1 = apid2 = 0;
        dpid1 = dpid2 = 0;
        bool ok = false;
        if (parambuf && sourcebuf && apidbuf) {
           ok = StringToParameters(parambuf) && (source = cSource::FromString(sourcebuf)) >= 0;
           char *p = strchr(apidbuf, ';');
           if (p)
              *p++ = 0;
           sscanf(apidbuf, "%d ,%d ", &apid1, &apid2);
           if (p)
              sscanf(p, "%d ,%d ", &dpid1, &dpid2);
           }
        strn0cpy(name, namebuf, MaxChannelName);
        free(parambuf);
        free(sourcebuf);
        free(apidbuf);
        free(namebuf);
        return ok;
        }
     else
        return false;
     }
  strreplace(name, '|', ':');
  return true;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

// -- cChannels --------------------------------------------------------------

cChannels Channels;

bool cChannels::Load(const char *FileName, bool AllowComments)
{
  if (cConfig<cChannel>::Load(FileName, AllowComments)) {
     ReNumber();
     return true;
     }
  return false;
}

int cChannels::GetNextGroup(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && !channel->GroupSep())
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

int cChannels::GetPrevGroup(int Idx)
{
  cChannel *channel = Get(--Idx);
  while (channel && !channel->GroupSep())
        channel = Get(--Idx);
  return channel ? Idx : -1;
}

int cChannels::GetNextNormal(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && channel->GroupSep())
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

void cChannels::ReNumber( void )
{
  int Number = 1;
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (channel->GroupSep()) {
         if (channel->Number() > Number)
            Number = channel->Number();
         }
      else
         channel->SetNumber(Number++);
      }
  maxNumber = Number - 1;
}

cChannel *cChannels::GetByNumber(int Number, int SkipGap)
{
  cChannel *previous = NULL;
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (!channel->GroupSep()) {
         if (channel->Number() == Number)
            return channel;
         else if (SkipGap && channel->Number() > Number)
            return SkipGap > 0 ? channel : previous;
         previous = channel;
         }
      }
  return NULL;
}

cChannel *cChannels::GetByServiceID(unsigned short ServiceId)
{
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (!channel->GroupSep() && channel->Sid() == ServiceId)
         return channel;
      }
  return NULL;
}

bool cChannels::SwitchTo(int Number)
{
  cChannel *channel = GetByNumber(Number);
  return channel && cDevice::PrimaryDevice()->SwitchChannel(channel, true);
}
