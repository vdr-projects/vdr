/*
 * channels.c: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.c 1.14 2003/09/09 18:55:26 kls Exp $
 */

#include "channels.h"
#include <linux/dvb/frontend.h>
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
  { 999, BANDWIDTH_AUTO },
  { -1 }
  };

const tChannelParameterMap CoderateValues[] = {
  {   0, FEC_NONE },
  {  12, FEC_1_2 },
  {  23, FEC_2_3 },
  {  34, FEC_3_4 },
  {  45, FEC_4_5 },
  {  56, FEC_5_6 },
  {  67, FEC_6_7 },
  {  78, FEC_7_8 },
  {  89, FEC_8_9 },
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
  { 999, QAM_AUTO },
  { -1 }
  };

const tChannelParameterMap TransmissionValues[] = {
  {   2, TRANSMISSION_MODE_2K },
  {   8, TRANSMISSION_MODE_8K },
  { 999, TRANSMISSION_MODE_AUTO },
  { -1 }
  };

const tChannelParameterMap GuardValues[] = {
  {   4, GUARD_INTERVAL_1_4 },
  {   8, GUARD_INTERVAL_1_8 },
  {  16, GUARD_INTERVAL_1_16 },
  {  32, GUARD_INTERVAL_1_32 },
  { 999, GUARD_INTERVAL_AUTO },
  { -1 }
  };

const tChannelParameterMap HierarchyValues[] = {
  {   0, HIERARCHY_NONE },
  {   1, HIERARCHY_1 },
  {   2, HIERARCHY_2 },
  {   4, HIERARCHY_4 },
  { 999, HIERARCHY_AUTO },
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

// -- tChannelID -------------------------------------------------------------

const tChannelID tChannelID::InvalidID;

bool tChannelID::operator== (const tChannelID &arg) const
{
  return source == arg.source && nid == arg.nid && tid == arg.tid && sid == arg.sid && rid == arg.rid;
}

tChannelID tChannelID::FromString(const char *s)
{
  char *sourcebuf = NULL;
  int nid;
  int tid;
  int sid;
  int rid = 0;
  int fields = sscanf(s, "%a[^-]-%d-%d-%d-%d", &sourcebuf, &nid, &tid, &sid, &rid);
  if (fields == 4 || fields == 5) {
     int source = cSource::FromString(sourcebuf);
     free(sourcebuf);
     if (source >= 0)
        return tChannelID(source, nid, tid, sid, rid);
     }
  return tChannelID::InvalidID;
}

const char *tChannelID::ToString(void)
{
  static char buffer[256];
  snprintf(buffer, sizeof(buffer), rid ? "%s-%d-%d-%d-%d" : "%s-%d-%d-%d", cSource::ToString(source), nid, tid, sid, rid);
  return buffer;
}

// -- cChannel ---------------------------------------------------------------

char *cChannel::buffer = NULL;

cChannel::cChannel(void)
{
  strcpy(name,   "Pro7");
  frequency    = 12480;
  source       = cSource::FromString("S19.2E");
  srate        = 27500;
  vpid         = 255;
  ppid         = 0;
  apid1        = 256;
  apid2        = 0;
  dpid1        = 257;
  dpid2        = 0;
  tpid         = 32;
  ca           = 0;
  nid          = 0;
  tid          = 0;
  sid          = 888;
  rid          = 0;
  number       = 0;
  groupSep     = false;
  polarization = 'v';
  inversion    = INVERSION_AUTO;
  bandwidth    = BANDWIDTH_AUTO;
  coderateH    = FEC_AUTO;
  coderateL    = FEC_AUTO;
  modulation   = QAM_AUTO;
  transmission = TRANSMISSION_MODE_AUTO;
  guard        = GUARD_INTERVAL_AUTO;
  hierarchy    = HIERARCHY_AUTO;
}

cChannel& cChannel::operator= (const cChannel &Channel)
{
  memcpy(&__BeginData__, &Channel.__BeginData__, (char *)&Channel.__EndData__ - (char *)&Channel.__BeginData__);
  return *this;
}

static int MHz(int frequency)
{
  while (frequency > 20000)
        frequency /= 1000;
  return frequency;
}

tChannelID cChannel::GetChannelID(void) const
{
  return tChannelID(source, nid, nid ? tid : MHz(frequency), sid, rid);
}

static int PrintParameter(char *p, char Name, int Value)
{
  return Value >= 0 && Value != 999 ? sprintf(p, "%c%d", Name, Value) : 0;
}

const char *cChannel::ParametersToString(void)
{
  char type = *cSource::ToString(source);
  if (isdigit(type))
     type = 'S';
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
        Value = MapToDriver(n, Map);
        if (Value >= 0)
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
     char vpidbuf[32];
     char *q = vpidbuf;
     q += snprintf(q, sizeof(vpidbuf), "%d", Channel->vpid);
     if (Channel->ppid)
        q += snprintf(q, sizeof(vpidbuf) - (q - vpidbuf), "+%d", Channel->ppid);
     *q = 0;
     char apidbuf[32];
     q = apidbuf;
     q += snprintf(q, sizeof(apidbuf), "%d", Channel->apid1);
     if (Channel->apid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->apid2);
     if (Channel->dpid1 || Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ";%d", Channel->dpid1);
     if (Channel->dpid2)
        q += snprintf(q, sizeof(apidbuf) - (q - apidbuf), ",%d", Channel->dpid2);
     *q = 0;
     asprintf(&buffer, "%s:%d:%s:%s:%d:%s:%s:%d:%d:%d:%d:%d:%d\n", s, Channel->frequency, Channel->ParametersToString(), cSource::ToString(Channel->source), Channel->srate, vpidbuf, apidbuf, Channel->tpid, Channel->ca, Channel->sid, Channel->nid, Channel->tid, Channel->rid);
     }
  return buffer;
}

const char *cChannel::ToText(void)
{
  return ToText(this);
}

bool cChannel::Parse(const char *s, bool AllowNonUniqueID)
{
  bool ok = true;
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
     char *vpidbuf = NULL;
     char *apidbuf = NULL;
     int fields = sscanf(s, "%a[^:]:%d :%a[^:]:%a[^:] :%d :%a[^:]:%a[^:]:%d :%d :%d :%d :%d :%d ", &namebuf, &frequency, &parambuf, &sourcebuf, &srate, &vpidbuf, &apidbuf, &tpid, &ca, &sid, &nid, &tid, &rid);
     if (fields >= 9) {
        if (fields == 9) {
           // allow reading of old format
           sid = ca;
           ca = tpid;
           tpid = 0;
           }
        vpid  = ppid  = 0;
        apid1 = apid2 = 0;
        dpid1 = dpid2 = 0;
        ok = false;
        if (parambuf && sourcebuf && vpidbuf && apidbuf) {
           ok = StringToParameters(parambuf) && (source = cSource::FromString(sourcebuf)) >= 0;
           char *p = strchr(vpidbuf, '+');
           if (p)
              *p++ = 0;
           sscanf(vpidbuf, "%d", &vpid);
           if (p)
              sscanf(p, "%d", &ppid);
           p = strchr(apidbuf, ';');
           if (p)
              *p++ = 0;
           sscanf(apidbuf, "%d ,%d ", &apid1, &apid2);
           if (p)
              sscanf(p, "%d ,%d ", &dpid1, &dpid2);
           }
        strn0cpy(name, namebuf, MaxChannelName);
        free(parambuf);
        free(sourcebuf);
        free(vpidbuf);
        free(apidbuf);
        free(namebuf);
        if (!AllowNonUniqueID && Channels.GetByChannelID(GetChannelID())) {
           esyslog("ERROR: channel data not unique!");
           return false;
           }
        }
     else
        return false;
     }
  strreplace(name, '|', ':');
  return ok;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

// -- cChannels --------------------------------------------------------------

cChannels Channels;

bool cChannels::Load(const char *FileName, bool AllowComments, bool MustExist)
{
  if (cConfig<cChannel>::Load(FileName, AllowComments, MustExist)) {
     ReNumber();
     return true;
     }
  return false;
}

int cChannels::GetNextGroup(int Idx)
{
  cChannel *channel = Get(++Idx);
  while (channel && !(channel->GroupSep() && *channel->Name()))
        channel = Get(++Idx);
  return channel ? Idx : -1;
}

int cChannels::GetPrevGroup(int Idx)
{
  cChannel *channel = Get(--Idx);
  while (channel && !(channel->GroupSep() && *channel->Name()))
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

cChannel *cChannels::GetByServiceID(int Source, unsigned short ServiceID)
{
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (!channel->GroupSep() && channel->Source() == Source && channel->Sid() == ServiceID)
         return channel;
      }
  return NULL;
}

cChannel *cChannels::GetByChannelID(tChannelID ChannelID, bool TryWithoutRid)
{
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (!channel->GroupSep() && channel->GetChannelID() == ChannelID)
         return channel;
      }
  if (TryWithoutRid) {
     ChannelID.ClrRid();
     for (cChannel *channel = First(); channel; channel = Next(channel)) {
         if (!channel->GroupSep() && channel->GetChannelID().ClrRid() == ChannelID)
            return channel;
         }
     }
  return NULL;
}

bool cChannels::HasUniqueChannelID(cChannel *NewChannel, cChannel *OldChannel)
{
  tChannelID NewChannelID = NewChannel->GetChannelID();
  for (cChannel *channel = First(); channel; channel = Next(channel)) {
      if (!channel->GroupSep() && channel != OldChannel && channel->GetChannelID() == NewChannelID)
         return false;
      }
  return true;
}

bool cChannels::SwitchTo(int Number)
{
  cChannel *channel = GetByNumber(Number);
  return channel && cDevice::PrimaryDevice()->SwitchChannel(channel, true);
}
