/*
 * channels.c: Channel handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: channels.c 5.4 2025/03/02 11:03:35 kls Exp $
 */

#include "channels.h"
#include <ctype.h>
#include "device.h"
#include "libsi/si.h"

// IMPORTANT NOTE: in the 'sscanf()' calls there is a blank after the '%d'
// format characters in order to allow any number of blanks after a numeric
// value!

// --- tChannelID ------------------------------------------------------------

const tChannelID tChannelID::InvalidID;

tChannelID tChannelID::FromString(const char *s)
{
  char *sourcebuf = NULL;
  int nid;
  int tid;
  int sid;
  int rid = 0;
  int fields = sscanf(s, "%m[^-]-%d-%d-%d-%d", &sourcebuf, &nid, &tid, &sid, &rid);
  if (fields == 4 || fields == 5) {
     int source = cSource::FromString(sourcebuf);
     free(sourcebuf);
     if (source >= 0)
        return tChannelID(source, nid, tid, sid, rid);
     }
  return tChannelID::InvalidID;
}

cString tChannelID::ToString(void) const
{
  char buffer[256];
  snprintf(buffer, sizeof(buffer), rid ? "%s-%d-%d-%d-%d" : "%s-%d-%d-%d", *cSource::ToString(source), nid, tid, sid, rid);
  return buffer;
}

tChannelID &tChannelID::ClrPolarization(void)
{
  while (tid > 100000)
        tid -= 100000;
  return *this;
}

// --- cChannel --------------------------------------------------------------

cChannel::cChannel(void)
{
  name = strdup("");
  shortName = strdup("");
  provider = strdup("");
  portalName = strdup("");
  memset(&__BeginData__, 0, (char *)&__EndData__ - (char *)&__BeginData__);
  parameters = "";
  modification = CHANNELMOD_NONE;
  seen         = 0;
  schedule     = NULL;
  linkChannels = NULL;
  refChannel   = NULL;
}

cChannel::cChannel(const cChannel &Channel)
{
  name = NULL;
  shortName = NULL;
  provider = NULL;
  portalName = NULL;
  schedule     = NULL;
  linkChannels = NULL;
  refChannel   = NULL;
  seen         = 0;
  *this = Channel;
}

cChannel::~cChannel()
{
  delete linkChannels; // any links from other channels pointing to this one have been deleted in cChannels::Del()
  free(name);
  free(shortName);
  free(provider);
  free(portalName);
}

cChannel& cChannel::operator= (const cChannel &Channel)
{
  name = strcpyrealloc(name, Channel.name);
  shortName = strcpyrealloc(shortName, Channel.shortName);
  provider = strcpyrealloc(provider, Channel.provider);
  portalName = strcpyrealloc(portalName, Channel.portalName);
  memcpy(&__BeginData__, &Channel.__BeginData__, (char *)&Channel.__EndData__ - (char *)&Channel.__BeginData__);
  UpdateNameSource();
  parameters = Channel.parameters;
  return *this;
}

void cChannel::UpdateNameSource(void)
{
  if (Setup.ShowChannelNamesWithSource == 0) {
     nameSource = NULL;
     shortNameSource = NULL;
     return;
     }

  if (Setup.ShowChannelNamesWithSource == 1)
     nameSource = cString::sprintf("%s (%c)", name, cSource::ToChar(source));
  else
     nameSource = cString::sprintf("%s (%s)", name, *cSource::ToString(source));

  shortNameSource = cString::sprintf("%s (%c)", shortName, cSource::ToChar(source));
}

const char *cChannel::Name(void) const
{
  if (Setup.ShowChannelNamesWithSource && !groupSep) {
     if (!isempty(nameSource))
        return nameSource;
     }
  return name;
}

const char *cChannel::ShortName(bool OrName) const
{
  if (OrName && isempty(shortName))
     return Name();
  if (Setup.ShowChannelNamesWithSource && !groupSep) {
     if (!isempty(shortNameSource))
        return shortNameSource;
     }
  return shortName;
}

int cChannel::Transponder(int Frequency, char Polarization)
{
  // some satellites have transponders at the same frequency, just with different polarization:
  switch (toupper(Polarization)) {
    case 'H': Frequency += 100000; break;
    case 'V': Frequency += 200000; break;
    case 'L': Frequency += 300000; break;
    case 'R': Frequency += 400000; break;
    default: esyslog("ERROR: invalid value for Polarization '%c'", Polarization);
    }
  return Frequency;
}

int cChannel::Transponder(void) const
{
  if (!transponder) {
     int tf = frequency;
     while (tf > 20000)
           tf /= 1000;
     if (IsSat()) {
        const char *p = strpbrk(parameters, "HVLRhvlr"); // lowercase for backwards compatibility
        if (p)
           tf = Transponder(tf, *p);
        }
     transponder = tf;
     }
  return transponder;
}

int cChannel::Modification(int Mask) const
{
  int Result = modification & Mask;
  modification = CHANNELMOD_NONE;
  return Result;
}

void cChannel::CopyTransponderData(const cChannel *Channel)
{
  if (Channel) {
     frequency    = Channel->frequency;
     transponder  = Channel->transponder;
     source       = Channel->source;
     srate        = Channel->srate;
     parameters   = Channel->parameters;
     }
}

bool cChannel::SetTransponderData(int Source, int Frequency, int Srate, const char *Parameters, bool Quiet)
{
  if (strchr(Parameters, ':')) {
     esyslog("ERROR: parameter string '%s' contains ':'", Parameters);
     return false;
     }
  // Workarounds for broadcaster stupidity:
  // Some providers broadcast the transponder frequency of their channels with two different
  // values (like 12551 and 12552), so we need to allow for a little tolerance here
  if (abs(frequency - Frequency) <= 1)
     Frequency = frequency;
  // Sometimes the transponder frequency is set to 0, which is just wrong
  if (Frequency == 0)
     return false;
  // Sometimes the symbol rate is off by one
  if (abs(srate - Srate) <= 1)
     Srate = srate;

  if (source != Source || frequency != Frequency || srate != Srate || strcmp(parameters, Parameters)) {
     cString OldTransponderData = TransponderDataToString();
     source = Source;
     frequency = Frequency;
     transponder = 0;
     srate = Srate;
     parameters = Parameters;
     schedule = NULL;
     UpdateNameSource();
     if (Number() && !Quiet) {
        dsyslog("changing transponder data of channel %d (%s) from %s to %s", Number(), name, *OldTransponderData, *TransponderDataToString());
        modification |= CHANNELMOD_TRANSP;
        }
     return true;
     }
  return false;
}

bool cChannel::SetSource(int Source)
{
  if (source != Source) {
     if (Number()) {
        dsyslog("changing source of channel %d (%s) from %s to %s", Number(), name, *cSource::ToString(source), *cSource::ToString(Source));
        modification |= CHANNELMOD_TRANSP;
        }
     source = Source;
     return true;
     }
  return false;
}

bool cChannel::SetId(cChannels *Channels, int Nid, int Tid, int Sid, int Rid)
{
  if (nid != Nid || tid != Tid || sid != Sid || rid != Rid) {
     if (Channels && Number()) {
        dsyslog("changing id of channel %d (%s) from %d-%d-%d-%d to %d-%d-%d-%d", Number(), name, nid, tid, sid, rid, Nid, Tid, Sid, Rid);
        modification |= CHANNELMOD_ID;
        Channels->UnhashChannel(this);
        }
     nid = Nid;
     tid = Tid;
     sid = Sid;
     rid = Rid;
     if (Channels)
        Channels->HashChannel(this);
     schedule = NULL;
     return true;
     }
  return false;
}

bool cChannel::SetLcn(int Lcn)
{
  if (lcn != Lcn) {
     if (Number())
        dsyslog("changing lcn of channel %d (%s) from %d to %d\n", Number(), name, lcn, Lcn);
     lcn = Lcn;
     return true;
     }
  return false;
}

bool cChannel::SetName(const char *Name, const char *ShortName, const char *Provider)
{
  if (!isempty(Name)) {
     bool nn = strcmp(name, Name) != 0;
     bool ns = strcmp(shortName, ShortName) != 0;
     bool np = strcmp(provider, Provider) != 0;
     if (nn || ns || np) {
        if (Number()) {
           dsyslog("changing name of channel %d from '%s,%s;%s' to '%s,%s;%s'", Number(), name, shortName, provider, Name, ShortName, Provider);
           modification |= CHANNELMOD_NAME;
           }
        if (nn)
           name = strcpyrealloc(name, Name);
        if (ns)
           shortName = strcpyrealloc(shortName, ShortName);
        if (nn || ns)
           UpdateNameSource();
        if (np)
           provider = strcpyrealloc(provider, Provider);
        return true;
        }
     }
  return false;
}

bool cChannel::SetPortalName(const char *PortalName)
{
  if (!isempty(PortalName) && strcmp(portalName, PortalName) != 0) {
     if (Number()) {
        dsyslog("changing portal name of channel %d (%s) from '%s' to '%s'", Number(), name, portalName, PortalName);
        modification |= CHANNELMOD_NAME;
        }
     portalName = strcpyrealloc(portalName, PortalName);
     return true;
     }
  return false;
}

#define STRDIFF 0x01
#define VALDIFF 0x02

static int IntArraysDiffer(const int *a, const int *b, const char na[][MAXLANGCODE2] = NULL, const char nb[][MAXLANGCODE2] = NULL)
{
  int result = 0;
  for (int i = 0; a[i] || b[i]; i++) {
      if (!a[i] || !b[i]) {
         result |= VALDIFF;
         break;
         }
      if (na && nb && strcmp(na[i], nb[i]) != 0)
         result |= STRDIFF;
      if (a[i] != b[i])
         result |= VALDIFF;
      }
  return result;
}

static int IntArrayToString(char *s, const int *a, int Base = 10, const char n[][MAXLANGCODE2] = NULL, const int *t = NULL)
{
  char *q = s;
  int i = 0;
  while (a[i] || i == 0) {
        q += sprintf(q, Base == 16 ? "%s%X" : "%s%d", i ? "," : "", a[i]);
        const char *Delim = "=";
        if (a[i]) {
           if (n && *n[i]) {
              q += sprintf(q, "%s%s", Delim, n[i]);
              Delim = "";
              }
           if (t && t[i])
              q += sprintf(q, "%s@%d", Delim, t[i]);
           }
        if (!a[i])
           break;
        i++;
        }
  *q = 0;
  return q - s;
}

bool cChannel::SetPids(int Vpid, int Ppid, int Vtype, int *Apids, int *Atypes, char ALangs[][MAXLANGCODE2], int *Dpids, int *Dtypes, char DLangs[][MAXLANGCODE2], int *Spids, char SLangs[][MAXLANGCODE2], int Tpid)
{
  int mod = CHANNELMOD_NONE;
  if (vpid != Vpid || ppid != Ppid || vtype != Vtype)
     mod |= CHANNELMOD_PIDS;
  if (tpid != Tpid)
     mod |= CHANNELMOD_AUX;
  int m = IntArraysDiffer(apids, Apids, alangs, ALangs) | IntArraysDiffer(atypes, Atypes) | IntArraysDiffer(dpids, Dpids, dlangs, DLangs) | IntArraysDiffer(dtypes, Dtypes) | IntArraysDiffer(spids, Spids, slangs, SLangs);
  if (m & STRDIFF)
     mod |= CHANNELMOD_LANGS;
  if (m & VALDIFF)
     mod |= CHANNELMOD_PIDS;
  if (mod) {
     const int BufferSize = (MAXAPIDS + MAXDPIDS) * (5 + 1 + MAXLANGCODE2 + 5) + 10; // 5 digits plus delimiting ',' or ';' plus optional '=cod+cod@type', +10: paranoia
     char OldApidsBuf[BufferSize];
     char NewApidsBuf[BufferSize];
     char *q = OldApidsBuf;
     q += IntArrayToString(q, apids, 10, alangs, atypes);
     if (dpids[0]) {
        *q++ = ';';
        q += IntArrayToString(q, dpids, 10, dlangs, dtypes);
        }
     *q = 0;
     q = NewApidsBuf;
     q += IntArrayToString(q, Apids, 10, ALangs, Atypes);
     if (Dpids[0]) {
        *q++ = ';';
        q += IntArrayToString(q, Dpids, 10, DLangs, Dtypes);
        }
     *q = 0;
     const int SBufferSize = MAXSPIDS * (5 + 1 + MAXLANGCODE2) + 10; // 5 digits plus delimiting ',' or ';' plus optional '=cod', +10: paranoia
     char OldSpidsBuf[SBufferSize];
     char NewSpidsBuf[SBufferSize];
     q = OldSpidsBuf;
     q += IntArrayToString(q, spids, 10, slangs);
     *q = 0;
     q = NewSpidsBuf;
     q += IntArrayToString(q, Spids, 10, SLangs);
     *q = 0;
     if (Number())
        dsyslog("changing pids of channel %d (%s) from %d+%d=%d:%s:%s:%d to %d+%d=%d:%s:%s:%d", Number(), name, vpid, ppid, vtype, OldApidsBuf, OldSpidsBuf, tpid, Vpid, Ppid, Vtype, NewApidsBuf, NewSpidsBuf, Tpid);
     vpid = Vpid;
     ppid = Ppid;
     vtype = Vtype;
     for (int i = 0; i < MAXAPIDS; i++) {
         apids[i] = Apids[i];
         atypes[i] = Atypes[i];
         strn0cpy(alangs[i], ALangs[i], MAXLANGCODE2);
         }
     apids[MAXAPIDS] = 0;
     for (int i = 0; i < MAXDPIDS; i++) {
         dpids[i] = Dpids[i];
         dtypes[i] = Dtypes[i];
         strn0cpy(dlangs[i], DLangs[i], MAXLANGCODE2);
         }
     dpids[MAXDPIDS] = 0;
     for (int i = 0; i < MAXSPIDS; i++) {
         spids[i] = Spids[i];
         strn0cpy(slangs[i], SLangs[i], MAXLANGCODE2);
         }
     spids[MAXSPIDS] = 0;
     tpid = Tpid;
     modification |= mod;
     return true;
     }
  return false;
}

bool cChannel::SetSubtitlingDescriptors(uchar *SubtitlingTypes, uint16_t *CompositionPageIds, uint16_t *AncillaryPageIds)
{
  bool Modified = false;
  if (SubtitlingTypes) {
     for (int i = 0; i < MAXSPIDS; i++) {
         Modified = subtitlingTypes[i] != SubtitlingTypes[i];
         subtitlingTypes[i] = SubtitlingTypes[i];
         }
     }
  if (CompositionPageIds) {
     for (int i = 0; i < MAXSPIDS; i++) {
         Modified = compositionPageIds[i] != CompositionPageIds[i];
         compositionPageIds[i] = CompositionPageIds[i];
         }
     }
  if (AncillaryPageIds) {
     for (int i = 0; i < MAXSPIDS; i++) {
         Modified = ancillaryPageIds[i] != AncillaryPageIds[i];
         ancillaryPageIds[i] = AncillaryPageIds[i];
         }
     }
  return Modified;
}

void cChannel::SetSeen(void)
{
  seen = time(NULL);
}

void cChannel::DelLinkChannel(cChannel *LinkChannel)
{
  if (linkChannels) {
     for (cLinkChannel *lc = linkChannels->First(); lc; lc = linkChannels->Next(lc)) {
         if (lc->Channel() == LinkChannel) {
            linkChannels->Del(lc);
            break;
            }
         }
     if (linkChannels->Count() == 0) {
        delete linkChannels;
        linkChannels = NULL;
        }
     }
}

bool cChannel::SetCaIds(const int *CaIds)
{
  if (caids[0] && caids[0] <= CA_USER_MAX)
     return false; // special values will not be overwritten
  if (IntArraysDiffer(caids, CaIds)) {
     char OldCaIdsBuf[MAXCAIDS * 5 + 10]; // 5: 4 digits plus delimiting ',', 10: paranoia
     char NewCaIdsBuf[MAXCAIDS * 5 + 10];
     IntArrayToString(OldCaIdsBuf, caids, 16);
     IntArrayToString(NewCaIdsBuf, CaIds, 16);
     if (Number())
        dsyslog("changing caids of channel %d (%s) from %s to %s", Number(), name, OldCaIdsBuf, NewCaIdsBuf);
     for (int i = 0; i <= MAXCAIDS; i++) { // <= to copy the terminating 0
         caids[i] = CaIds[i];
         if (!CaIds[i])
            break;
         }
     modification |= CHANNELMOD_CA;
     return true;
     }
  return false;
}

bool cChannel::SetCaDescriptors(int Level)
{
  if (Level > 0) {
     modification |= CHANNELMOD_CA;
     if (Number() && Level > 1)
        dsyslog("changing ca descriptors of channel %d (%s)", Number(), name);
     return true;
     }
  return false;
}

bool cChannel::SetLinkChannels(cLinkChannels *LinkChannels)
{
  if (!linkChannels && !LinkChannels)
     return false;
  if (linkChannels && LinkChannels) {
     cLinkChannel *lca = linkChannels->First();
     cLinkChannel *lcb = LinkChannels->First();
     while (lca && lcb) {
           if (lca->Channel() != lcb->Channel()) {
              lca = NULL;
              break;
              }
           lca = linkChannels->Next(lca);
           lcb = LinkChannels->Next(lcb);
           }
     if (!lca && !lcb) {
        delete LinkChannels;
        return false; // linkage has not changed
        }
     }
  char buffer[((linkChannels ? linkChannels->Count() : 0) + (LinkChannels ? LinkChannels->Count() : 0)) * 6 + 256]; // 6: 5 digit channel number plus blank, 256: other texts (see below) plus reserve
  char *q = buffer;
  q += sprintf(q, "linking channel %d (%s) from", Number(), name);
  if (linkChannels) {
     for (cLinkChannel *lc = linkChannels->First(); lc; lc = linkChannels->Next(lc)) {
         lc->Channel()->SetRefChannel(NULL);
         q += sprintf(q, " %d", lc->Channel()->Number());
         }
     delete linkChannels;
     }
  else
     q += sprintf(q, " none");
  q += sprintf(q, " to");
  linkChannels = LinkChannels;
  if (linkChannels) {
     for (cLinkChannel *lc = linkChannels->First(); lc; lc = linkChannels->Next(lc)) {
         lc->Channel()->SetRefChannel(this);
         q += sprintf(q, " %d", lc->Channel()->Number());
         //dsyslog("link %4d -> %4d: %s", Number(), lc->Channel()->Number(), lc->Channel()->Name());
         }
     }
  else
     q += sprintf(q, " none");
  if (Number())
     dsyslog("%s", buffer);
  return true;
}

void cChannel::SetRefChannel(cChannel *RefChannel)
{
  refChannel = RefChannel;
}

cString cChannel::TransponderDataToString(void) const
{
  if (cSource::IsTerr(source))
     return cString::sprintf("%d:%s:%s", frequency, *parameters, *cSource::ToString(source));
  return cString::sprintf("%d:%s:%s:%d", frequency, *parameters, *cSource::ToString(source), srate);
}

cString cChannel::ToText(const cChannel *Channel)
{
  char FullName[strlen(Channel->name) + 1 + strlen(Channel->shortName) + 1 + strlen(Channel->provider) + 1 + 10]; // +10: paranoia
  char *q = FullName;
  q += sprintf(q, "%s", Channel->name);
  if (!Channel->groupSep) {
     if (!isempty(Channel->shortName))
        q += sprintf(q, ",%s", Channel->shortName);
     else if (strchr(Channel->name, ','))
        q += sprintf(q, ",");
     if (!isempty(Channel->provider))
        q += sprintf(q, ";%s", Channel->provider);
     }
  *q = 0;
  strreplace(FullName, ':', '|');
  cString buffer;
  if (Channel->groupSep) {
     if (Channel->number)
        buffer = cString::sprintf(":@%d %s", Channel->number, FullName);
     else
        buffer = cString::sprintf(":%s", FullName);
     }
  else {
     char vpidbuf[32];
     char *q = vpidbuf;
     q += snprintf(q, sizeof(vpidbuf), "%d", Channel->vpid);
     if (Channel->ppid && Channel->ppid != Channel->vpid)
        q += snprintf(q, sizeof(vpidbuf) - (q - vpidbuf), "+%d", Channel->ppid);
     if (Channel->vpid && Channel->vtype)
        q += snprintf(q, sizeof(vpidbuf) - (q - vpidbuf), "=%d", Channel->vtype);
     *q = 0;
     const int ABufferSize = (MAXAPIDS + MAXDPIDS) * (5 + 1 + MAXLANGCODE2 + 5) + 10; // 5 digits plus delimiting ',' or ';' plus optional '=cod+cod@type', +10: paranoia
     char apidbuf[ABufferSize];
     q = apidbuf;
     q += IntArrayToString(q, Channel->apids, 10, Channel->alangs, Channel->atypes);
     if (Channel->dpids[0]) {
        *q++ = ';';
        q += IntArrayToString(q, Channel->dpids, 10, Channel->dlangs, Channel->dtypes);
        }
     *q = 0;
     const int TBufferSize = MAXSPIDS * (5 + 1 + MAXLANGCODE2) + 10; // 5 digits plus delimiting ',' or ';' plus optional '=cod+cod', +10: paranoia and tpid
     char tpidbuf[TBufferSize];
     q = tpidbuf;
     q += snprintf(q, sizeof(tpidbuf), "%d", Channel->tpid);
     if (Channel->spids[0]) {
        *q++ = ';';
        q += IntArrayToString(q, Channel->spids, 10, Channel->slangs);
        }
     char caidbuf[MAXCAIDS * 5 + 10]; // 5: 4 digits plus delimiting ',', 10: paranoia
     q = caidbuf;
     q += IntArrayToString(q, Channel->caids, 16);
     *q = 0;
     buffer = cString::sprintf("%s:%d:%s:%s:%d:%s:%s:%s:%s:%d:%d:%d:%d", FullName, Channel->frequency, *Channel->parameters, *cSource::ToString(Channel->source), Channel->srate, vpidbuf, apidbuf, tpidbuf, caidbuf, Channel->sid, Channel->nid, Channel->tid, Channel->rid);
     }
  return buffer;
}

cString cChannel::ToText(void) const
{
  return ToText(this);
}

bool cChannel::Parse(const char *s)
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
     name = strcpyrealloc(name, skipspace(s));
     strreplace(name, '|', ':');
     }
  else {
     groupSep = false;
     char *namebuf = NULL;
     char *sourcebuf = NULL;
     char *parambuf = NULL;
     char *vpidbuf = NULL;
     char *apidbuf = NULL;
     char *tpidbuf = NULL;
     char *caidbuf = NULL;
     int fields = sscanf(s, "%m[^:]:%d :%m[^:]:%m[^:] :%d :%m[^:]:%m[^:]:%m[^:]:%m[^:]:%d :%d :%d :%d ", &namebuf, &frequency, &parambuf, &sourcebuf, &srate, &vpidbuf, &apidbuf, &tpidbuf, &caidbuf, &sid, &nid, &tid, &rid);
     if (fields >= 9) {
        if (fields == 9) {
           // allow reading of old format
           sid = atoi(caidbuf);
           delete caidbuf;
           caidbuf = NULL;
           if (sscanf(tpidbuf, "%d", &tpid) != 1)
              return false;
           caids[0] = tpid;
           caids[1] = 0;
           tpid = 0;
           }
        vpid = ppid = 0;
        vtype = 0;
        apids[0] = 0;
        atypes[0] = 0;
        dpids[0] = 0;
        dtypes[0] = 0;
        spids[0] = 0;
        ok = false;
        if (parambuf && sourcebuf && vpidbuf && apidbuf) {
           parameters = parambuf;
           ok = (source = cSource::FromString(sourcebuf)) >= 0;
           transponder = 0;

           char *p;
           if ((p = strchr(vpidbuf, '=')) != NULL) {
              *p++ = 0;
              if (sscanf(p, "%d", &vtype) != 1)
                 return false;
              }
           if ((p = strchr(vpidbuf, '+')) != NULL) {
              *p++ = 0;
              if (sscanf(p, "%d", &ppid) != 1)
                 return false;
              }
           if (sscanf(vpidbuf, "%d", &vpid) != 1)
              return false;
           if (!ppid)
              ppid = vpid;
           if (vpid && !vtype)
              vtype = 2; // default is MPEG-2

           char *dpidbuf = strchr(apidbuf, ';');
           if (dpidbuf)
              *dpidbuf++ = 0;
           p = apidbuf;
           char *q;
           int NumApids = 0;
           char *strtok_next;
           while ((q = strtok_r(p, ",", &strtok_next)) != NULL) {
                 if (NumApids < MAXAPIDS) {
                    atypes[NumApids] = 4; // backwards compatibility
                    char *l = strchr(q, '=');
                    if (l) {
                       *l++ = 0;
                       char *t = strchr(l, '@');
                       if (t) {
                          *t++ = 0;
                          atypes[NumApids] = strtol(t, NULL, 10);
                          }
                       strn0cpy(alangs[NumApids], l, MAXLANGCODE2);
                       }
                    else
                       *alangs[NumApids] = 0;
                    if ((apids[NumApids] = strtol(q, NULL, 10)) != 0)
                       NumApids++;
                    }
                 else
                    esyslog("ERROR: too many APIDs!"); // no need to set ok to 'false'
                 p = NULL;
                 }
           apids[NumApids] = 0;
           atypes[NumApids] = 0;
           if (dpidbuf) {
              char *p = dpidbuf;
              char *q;
              int NumDpids = 0;
              char *strtok_next;
              while ((q = strtok_r(p, ",", &strtok_next)) != NULL) {
                    if (NumDpids < MAXDPIDS) {
                       dtypes[NumDpids] = SI::AC3DescriptorTag; // backwards compatibility
                       char *l = strchr(q, '=');
                       if (l) {
                          *l++ = 0;
                          char *t = strchr(l, '@');
                          if (t) {
                             *t++ = 0;
                             dtypes[NumDpids] = strtol(t, NULL, 10);
                             }
                          strn0cpy(dlangs[NumDpids], l, MAXLANGCODE2);
                          }
                       else
                          *dlangs[NumDpids] = 0;
                       if ((dpids[NumDpids] = strtol(q, NULL, 10)) != 0)
                          NumDpids++;
                       }
                    else
                       esyslog("ERROR: too many DPIDs!"); // no need to set ok to 'false'
                    p = NULL;
                    }
              dpids[NumDpids] = 0;
              dtypes[NumDpids] = 0;
              }
           int NumSpids = 0;
           if ((p = strchr(tpidbuf, ';')) != NULL) {
              *p++ = 0;
              char *q;
              char *strtok_next;
              while ((q = strtok_r(p, ",", &strtok_next)) != NULL) {
                    if (NumSpids < MAXSPIDS) {
                       char *l = strchr(q, '=');
                       if (l) {
                          *l++ = 0;
                          strn0cpy(slangs[NumSpids], l, MAXLANGCODE2);
                          }
                       else
                          *slangs[NumSpids] = 0;
                       spids[NumSpids++] = strtol(q, NULL, 10);
                       }
                    else
                       esyslog("ERROR: too many SPIDs!"); // no need to set ok to 'false'
                    p = NULL;
                    }
              spids[NumSpids] = 0;
              }
           if (sscanf(tpidbuf, "%d", &tpid) != 1)
              return false;
           if (caidbuf) {
              char *p = caidbuf;
              char *q;
              int NumCaIds = 0;
              char *strtok_next;
              while ((q = strtok_r(p, ",", &strtok_next)) != NULL) {
                    if (NumCaIds < MAXCAIDS) {
                       caids[NumCaIds++] = strtol(q, NULL, 16) & 0xFFFF;
                       if (NumCaIds == 1 && caids[0] <= CA_USER_MAX)
                          break;
                       }
                    else
                       esyslog("ERROR: too many CA ids!"); // no need to set ok to 'false'
                    p = NULL;
                    }
              caids[NumCaIds] = 0;
              }
           }
        strreplace(namebuf, '|', ':');

        char *p = strchr(namebuf, ';');
        if (p) {
           *p++ = 0;
           provider = strcpyrealloc(provider, p);
           }
        p = strrchr(namebuf, ','); // long name might contain a ',', so search for the rightmost one
        if (p) {
           *p++ = 0;
           shortName = strcpyrealloc(shortName, p);
           }
        name = strcpyrealloc(name, namebuf);

        free(parambuf);
        free(sourcebuf);
        free(vpidbuf);
        free(apidbuf);
        free(tpidbuf);
        free(caidbuf);
        free(namebuf);
        UpdateNameSource();
        if (!GetChannelID().Valid()) {
           esyslog("ERROR: channel data results in invalid ID!");
           return false;
           }
        }
     else
        return false;
     }
  return ok;
}

bool cChannel::Save(FILE *f)
{
  return fprintf(f, "%s\n", *ToText()) > 0;
}

// --- cChannelSorter --------------------------------------------------------

class cChannelSorter : public cListObject {
public:
  cChannel *channel;
  tChannelID channelID;
  cChannelSorter(cChannel *Channel) {
    channel = Channel;
    channelID = channel->GetChannelID();
    }
  virtual int Compare(const cListObject &ListObject) const override {
    cChannelSorter *cs = (cChannelSorter *)&ListObject;
    return memcmp(&channelID, &cs->channelID, sizeof(channelID));
    }
  };

// --- cChannels -------------------------------------------------------------

cChannels cChannels::channels;
int cChannels::maxNumber = 0;
int cChannels::maxChannelNameLength = 0;
int cChannels::maxShortChannelNameLength = 0;

cChannels::cChannels(void)
:cConfig<cChannel>("2 Channels")
{
  modifiedByUser = 0;
}

const cChannels *cChannels::GetChannelsRead(cStateKey &StateKey, int TimeoutMs)
{
  return channels.Lock(StateKey, false, TimeoutMs) ? &channels : NULL;
}

cChannels *cChannels::GetChannelsWrite(cStateKey &StateKey, int TimeoutMs)
{
  return channels.Lock(StateKey, true, TimeoutMs) ? &channels : NULL;
}

void cChannels::DeleteDuplicateChannels(void)
{
  cList<cChannelSorter> ChannelSorter;
  for (cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (!Channel->GroupSep())
         ChannelSorter.Add(new cChannelSorter(Channel));
      }
  ChannelSorter.Sort();
  cChannelSorter *cs = ChannelSorter.First();
  while (cs) {
        cChannelSorter *Next = ChannelSorter.Next(cs);
        if (Next && cs->channelID == Next->channelID) {
           dsyslog("deleting duplicate channel %s", *Next->channel->ToText());
           Del(Next->channel);
           }
        cs = Next;
        }
}

bool cChannels::Load(const char *FileName, bool AllowComments, bool MustExist)
{
  LOCK_CHANNELS_WRITE;
  if (channels.cConfig<cChannel>::Load(FileName, AllowComments, MustExist)) {
     channels.DeleteDuplicateChannels();
     channels.ReNumber();
     return true;
     }
  return false;
}

void cChannels::HashChannel(cChannel *Channel)
{
  channelsHashSid.Add(Channel, Channel->Sid());
}

void cChannels::UnhashChannel(cChannel *Channel)
{
  channelsHashSid.Del(Channel, Channel->Sid());
}

int cChannels::GetNextGroup(int Idx) const
{
  const cChannel *Channel = Get(++Idx);
  while (Channel && !(Channel->GroupSep() && *Channel->Name()))
        Channel = Get(++Idx);
  return Channel ? Idx : -1;
}

int cChannels::GetPrevGroup(int Idx) const
{
  const cChannel *Channel = Get(--Idx);
  while (Channel && !(Channel->GroupSep() && *Channel->Name()))
        Channel = Get(--Idx);
  return Channel ? Idx : -1;
}

int cChannels::GetNextNormal(int Idx) const
{
  const cChannel *Channel = Get(++Idx);
  while (Channel && Channel->GroupSep())
        Channel = Get(++Idx);
  return Channel ? Idx : -1;
}

int cChannels::GetPrevNormal(int Idx) const
{
  const cChannel *Channel = Get(--Idx);
  while (Channel && Channel->GroupSep())
        Channel = Get(--Idx);
  return Channel ? Idx : -1;
}

void cChannels::ReNumber(void)
{
  channelsHashSid.Clear();
  maxNumber = 0;
  int Number = 1;
  for (cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (Channel->GroupSep()) {
         if (Channel->Number() > Number)
            Number = Channel->Number();
         }
      else {
         HashChannel(Channel);
         maxNumber = Number;
         Channel->SetNumber(Number++);
         }
      }
}

bool cChannels::MoveNeedsDecrement(cChannel *From, cChannel *To)
{
  int Number = From->Number();
  if (Number < To->Number()) {
     for (cChannel *Channel = Next(From); Channel; Channel = Next(Channel)) {
         if (Channel == To)
            break;
         if (Channel->GroupSep()) {
            if (Channel->Number() > Number)
               Number = Channel->Number();
            }
         else
            Number++;
         }
     return Number == To->Number();
     }
  return false;
}

void cChannels::Del(cChannel *Channel)
{
  UnhashChannel(Channel);
  for (cChannel *ch = First(); ch; ch = Next(ch))
      ch->DelLinkChannel(Channel);
  cList<cChannel>::Del(Channel);
}

const cChannel *cChannels::GetByNumber(int Number, int SkipGap) const
{
  const cChannel *Previous = NULL;
  for (const cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (!Channel->GroupSep()) {
         if (Channel->Number() == Number)
            return Channel;
         else if (SkipGap && Channel->Number() > Number)
            return SkipGap > 0 ? Channel : Previous;
         Previous = Channel;
         }
      }
  return NULL;
}

const cChannel *cChannels::GetByServiceID(int Source, int Transponder, unsigned short ServiceID) const
{
  cList<cHashObject> *list = channelsHashSid.GetList(ServiceID);
  if (list) {
     for (cHashObject *hobj = list->First(); hobj; hobj = list->Next(hobj)) {
         cChannel *Channel = (cChannel *)hobj->Object();
         if (Channel->Sid() == ServiceID && Channel->Source() == Source && ISTRANSPONDER(Channel->Transponder(), Transponder))
            return Channel;
         }
     }
  return NULL;
}

const cChannel *cChannels::GetByChannelID(tChannelID ChannelID, bool TryWithoutRid, bool TryWithoutPolarization) const
{
  int sid = ChannelID.Sid();
  cList<cHashObject> *list = channelsHashSid.GetList(sid);
  if (list) {
     for (cHashObject *hobj = list->First(); hobj; hobj = list->Next(hobj)) {
         cChannel *Channel = (cChannel *)hobj->Object();
         if (Channel->Sid() == sid && Channel->GetChannelID() == ChannelID)
            return Channel;
         }
     if (TryWithoutRid) {
        ChannelID.ClrRid();
        for (cHashObject *hobj = list->First(); hobj; hobj = list->Next(hobj)) {
            cChannel *Channel = (cChannel *)hobj->Object();
            if (Channel->Sid() == sid && Channel->GetChannelID().ClrRid() == ChannelID)
               return Channel;
            }
        }
     if (TryWithoutPolarization) {
        ChannelID.ClrPolarization();
        for (cHashObject *hobj = list->First(); hobj; hobj = list->Next(hobj)) {
            cChannel *Channel = (cChannel *)hobj->Object();
            if (Channel->Sid() == sid && Channel->GetChannelID().ClrPolarization() == ChannelID)
               return Channel;
            }
        }
     }
  return NULL;
}

const cChannel *cChannels::GetByTransponderID(tChannelID ChannelID) const
{
  int source = ChannelID.Source();
  int nid = ChannelID.Nid();
  int tid = ChannelID.Tid();
  for (const cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (Channel->Tid() == tid && Channel->Nid() == nid && Channel->Source() == source)
         return Channel;
      }
  return NULL;
}

bool cChannels::HasUniqueChannelID(const cChannel *NewChannel, const cChannel *OldChannel) const
{
  tChannelID NewChannelID = NewChannel->GetChannelID();
  for (const cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (!Channel->GroupSep() && Channel != OldChannel && Channel->GetChannelID() == NewChannelID)
         return false;
      }
  return true;
}

bool cChannels::SwitchTo(int Number) const
{
  const cChannel *Channel = GetByNumber(Number);
  return Channel && cDevice::PrimaryDevice()->SwitchChannel(Channel, true);
}

int cChannels::MaxChannelNameLength(void)
{
  if (!maxChannelNameLength) {
     LOCK_CHANNELS_READ;
     for (const cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
         if (!Channel->GroupSep())
            maxChannelNameLength = max(Utf8StrLen(Channel->Name()), maxChannelNameLength);
         }
     }
  return maxChannelNameLength;
}

int cChannels::MaxShortChannelNameLength(void)
{
  if (!maxShortChannelNameLength) {
     LOCK_CHANNELS_READ;
     for (const cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
         if (!Channel->GroupSep())
            maxShortChannelNameLength = max(Utf8StrLen(Channel->ShortName(true)), maxShortChannelNameLength);
         }
     }
  return maxShortChannelNameLength;
}

void cChannels::SetModifiedByUser(void)
{
  modifiedByUser++;
  maxChannelNameLength = maxShortChannelNameLength = 0;
}

bool cChannels::ModifiedByUser(int &State) const
{
  int Result = State != modifiedByUser;
  State = modifiedByUser;
  return Result;
}

cChannel *cChannels::NewChannel(const cChannel *Transponder, const char *Name, const char *ShortName, const char *Provider, int Nid, int Tid, int Sid, int Rid)
{
  if (Transponder) {
     dsyslog("creating new channel '%s,%s;%s' on %s transponder %d with id %d-%d-%d-%d", Name, ShortName, Provider, *cSource::ToString(Transponder->Source()), Transponder->Transponder(), Nid, Tid, Sid, Rid);
     cChannel *NewChannel = new cChannel;
     NewChannel->CopyTransponderData(Transponder);
     NewChannel->SetId(this, Nid, Tid, Sid, Rid);
     NewChannel->SetName(Name, ShortName, Provider);
     NewChannel->SetSeen();
     Add(NewChannel);
     ReNumber();
     return NewChannel;
     }
  return NULL;
}

#define CHANNELMARKOBSOLETE "OBSOLETE"
#define CHANNELTIMEOBSOLETE 3600 // seconds to wait before declaring a channel obsolete (in case it has actually been seen before)

bool cChannels::MarkObsoleteChannels(int Source, int Nid, int Tid)
{
  bool ChannelsModified = false;
  for (cChannel *Channel = First(); Channel; Channel = Next(Channel)) {
      if (time(NULL) - Channel->Seen() > CHANNELTIMEOBSOLETE && Channel->Source() == Source && Channel->Nid() == Nid && Channel->Tid() == Tid && Channel->Rid() == 0) {
         int OldShowChannelNamesWithSource = Setup.ShowChannelNamesWithSource;
         Setup.ShowChannelNamesWithSource = 0;
         if (!endswith(Channel->Name(), CHANNELMARKOBSOLETE))
            ChannelsModified |= Channel->SetName(cString::sprintf("%s %s", Channel->Name(), CHANNELMARKOBSOLETE), Channel->ShortName(), cString::sprintf("%s %s", CHANNELMARKOBSOLETE, Channel->Provider()));
         Setup.ShowChannelNamesWithSource = OldShowChannelNamesWithSource;
         }
      }
  return ChannelsModified;
}

cString ChannelString(const cChannel *Channel, int Number)
{
  char buffer[256];
  if (Channel) {
     if (Channel->GroupSep())
        snprintf(buffer, sizeof(buffer), "%s", Channel->Name());
     else
        snprintf(buffer, sizeof(buffer), "%d%s  %s", Channel->Number(), Number ? "-" : "", Channel->Name());
     }
  else if (Number)
     snprintf(buffer, sizeof(buffer), "%d-", Number);
  else
     snprintf(buffer, sizeof(buffer), "%s", tr("*** Invalid Channel ***"));
  return buffer;
}
