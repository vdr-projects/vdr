/*
 * pat.c: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.c 4.7 2020/12/09 21:42:26 kls Exp $
 */

#include "pat.h"
#include <malloc.h>
#include "channels.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

#define PMT_SCAN_TIMEOUT  1000 // ms

// --- cCaDescriptor ---------------------------------------------------------

class cCaDescriptor : public cListObject {
private:
  int caSystem;
  int caPid;
  int esPid;
  int length;
  uchar *data;
public:
  cCaDescriptor(int CaSystem, int CaPid, int EsPid, int Length, const uchar *Data);
  virtual ~cCaDescriptor();
  bool operator== (const cCaDescriptor &arg) const;
  int CaSystem(void) { return caSystem; }
  int CaPid(void) { return caPid; }
  int EsPid(void) { return esPid; }
  int Length(void) const { return length; }
  const uchar *Data(void) const { return data; }
  };

cCaDescriptor::cCaDescriptor(int CaSystem, int CaPid, int EsPid, int Length, const uchar *Data)
{
  caSystem = CaSystem;
  caPid = CaPid;
  esPid = EsPid;
  length = Length + 6;
  data = MALLOC(uchar, length);
  data[0] = SI::CaDescriptorTag;
  data[1] = length - 2;
  data[2] = (caSystem >> 8) & 0xFF;
  data[3] =  caSystem       & 0xFF;
  data[4] = ((CaPid   >> 8) & 0x1F) | 0xE0;
  data[5] =   CaPid         & 0xFF;
  if (Length)
     memcpy(&data[6], Data, Length);
}

cCaDescriptor::~cCaDescriptor()
{
  free(data);
}

bool cCaDescriptor::operator== (const cCaDescriptor &arg) const
{
  return esPid == arg.esPid && length == arg.length && memcmp(data, arg.data, length) == 0;
}

// --- cCaDescriptors --------------------------------------------------------

class cCaDescriptors : public cListObject {
private:
  int source;
  int transponder;
  int serviceId;
  int pmtPid; // needed for OctopusNet - otherwise irrelevant!
  int numCaIds;
  int caIds[MAXCAIDS + 1];
  cList<cCaDescriptor> caDescriptors;
  void AddCaId(int CaId);
public:
  cCaDescriptors(int Source, int Transponder, int ServiceId, int PmtPid);
  bool operator== (const cCaDescriptors &arg) const;
  bool Is(int Source, int Transponder, int ServiceId);
  bool Is(cCaDescriptors * CaDescriptors);
  bool Empty(void) { return caDescriptors.Count() == 0; }
  void AddCaDescriptor(SI::CaDescriptor *d, int EsPid);
  void GetCaDescriptors(const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid);
  int GetCaPids(const int *CaSystemIds, int BufSize, int *Pids);
  const int GetPmtPid(void) { return pmtPid; };
  const int *CaIds(void) { return caIds; }
  };

cCaDescriptors::cCaDescriptors(int Source, int Transponder, int ServiceId, int PmtPid)
{
  source = Source;
  transponder = Transponder;
  serviceId = ServiceId;
  pmtPid = PmtPid;
  numCaIds = 0;
  caIds[0] = 0;
}

bool cCaDescriptors::operator== (const cCaDescriptors &arg) const
{
  const cCaDescriptor *ca1 = caDescriptors.First();
  const cCaDescriptor *ca2 = arg.caDescriptors.First();
  while (ca1 && ca2) {
        if (!(*ca1 == *ca2))
           return false;
        ca1 = caDescriptors.Next(ca1);
        ca2 = arg.caDescriptors.Next(ca2);
        }
  return !ca1 && !ca2;
}

bool cCaDescriptors::Is(int Source, int Transponder, int ServiceId)
{
  return source == Source && transponder == Transponder && serviceId == ServiceId;
}

bool cCaDescriptors::Is(cCaDescriptors *CaDescriptors)
{
  return Is(CaDescriptors->source, CaDescriptors->transponder, CaDescriptors->serviceId);
}

void cCaDescriptors::AddCaId(int CaId)
{
  if (numCaIds < MAXCAIDS) {
     for (int i = 0; i < numCaIds; i++) {
         if (caIds[i] == CaId)
            return;
         }
     caIds[numCaIds++] = CaId;
     caIds[numCaIds] = 0;
     }
}

void cCaDescriptors::AddCaDescriptor(SI::CaDescriptor *d, int EsPid)
{
  cCaDescriptor *nca = new cCaDescriptor(d->getCaType(), d->getCaPid(), EsPid, d->privateData.getLength(), d->privateData.getData());
  for (cCaDescriptor *ca = caDescriptors.First(); ca; ca = caDescriptors.Next(ca)) {
      if (*ca == *nca) {
         delete nca;
         return;
         }
      }
  AddCaId(nca->CaSystem());
  caDescriptors.Add(nca);
//#define DEBUG_CA_DESCRIPTORS 1
#ifdef DEBUG_CA_DESCRIPTORS
  char buffer[1024];
  char *q = buffer;
  q += sprintf(q, "CAM: %04X %5d %5d %04X %04X -", source, transponder, serviceId, d->getCaType(), EsPid);
  for (int i = 0; i < nca->Length(); i++)
      q += sprintf(q, " %02X", nca->Data()[i]);
  dsyslog("%s", buffer);
#endif
}

// EsPid is to select the "type" of CaDescriptor to be returned
// >0 - CaDescriptor for the particular esPid
// =0 - common CaDescriptor
// <0 - all CaDescriptors regardless of type (old default)

void cCaDescriptors::GetCaDescriptors(const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid)
{
  Buffer.Clear();
  if (!CaSystemIds || !*CaSystemIds)
     return;
  for (cCaDescriptor *d = caDescriptors.First(); d; d = caDescriptors.Next(d)) {
      if (EsPid < 0 || d->EsPid() == EsPid) {
         const int *caids = CaSystemIds;
         do {
            if (*caids == 0xFFFF || d->CaSystem() == *caids)
               Buffer.Append(d->Data(), d->Length());
            } while (*++caids);
         }
      }
}

int cCaDescriptors::GetCaPids(const int *CaSystemIds, int BufSize, int *Pids)
{
  if (!CaSystemIds || !*CaSystemIds)
     return 0;
  if (BufSize > 0 && Pids) {
     int numPids = 0;
     for (cCaDescriptor *d = caDescriptors.First(); d; d = caDescriptors.Next(d)) {
         const int *caids = CaSystemIds;
         do {
            if (*caids == 0xFFFF || d->CaSystem() == *caids) {
               if (numPids + 1 < BufSize) {
                  Pids[numPids++] = d->CaPid();
                  Pids[numPids] = 0;
                  }
               else
                  return -1;
               }
            } while (*++caids);
         }
     return numPids;
     }
  return -1;
}

// --- cCaDescriptorHandler --------------------------------------------------

class cCaDescriptorHandler : public cList<cCaDescriptors> {
private:
  cMutex mutex;
public:
  int AddCaDescriptors(cCaDescriptors *CaDescriptors);
      // Returns 0 if this is an already known descriptor,
      // 1 if it is an all new descriptor with actual contents,
      // and 2 if an existing descriptor was changed.
  void GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid);
  int GetCaPids(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, int *Pids);
  int GetPmtPid(int Source, int Transponder, int ServiceId);
  };

int cCaDescriptorHandler::AddCaDescriptors(cCaDescriptors *CaDescriptors)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(CaDescriptors)) {
         if (*ca == *CaDescriptors) {
            delete CaDescriptors;
            return 0;
            }
         Del(ca);
         Add(CaDescriptors);
         return 2;
         }
      }
  Add(CaDescriptors);
  return CaDescriptors->Empty() ? 0 : 1;
}

void cCaDescriptorHandler::GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(Source, Transponder, ServiceId)) {
         ca->GetCaDescriptors(CaSystemIds, Buffer, EsPid);
         break;
         }
      }
}

int cCaDescriptorHandler::GetCaPids(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, int *Pids)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(Source, Transponder, ServiceId))
         return ca->GetCaPids(CaSystemIds, BufSize, Pids);
      }
  return 0;
}

int cCaDescriptorHandler::GetPmtPid(int Source, int Transponder, int ServiceId)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(Source, Transponder, ServiceId))
         return ca->GetPmtPid();
      }
  return 0;
}

cCaDescriptorHandler CaDescriptorHandler;

void GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, cDynamicBuffer &Buffer, int EsPid)
{
  CaDescriptorHandler.GetCaDescriptors(Source, Transponder, ServiceId, CaSystemIds, Buffer, EsPid);
}

int GetCaPids(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, int *Pids)
{
  return CaDescriptorHandler.GetCaPids(Source, Transponder, ServiceId, CaSystemIds, BufSize, Pids);
}

int GetPmtPid(int Source, int Transponder, int ServiceId)
{
  return CaDescriptorHandler.GetPmtPid(Source, Transponder, ServiceId);
}

// --- cPmtPidEntry ----------------------------------------------------------

class cPmtPidEntry : public cListObject {
private:
  int pid;
  bool complete;
public:
  cPmtPidEntry(int Pid);
  int Pid(void) { return pid; }
  int Complete(void) { return complete; }
  void SetComplete(bool State) { complete = State; }
  };

cPmtPidEntry::cPmtPidEntry(int Pid)
{
  pid = Pid;
  complete = false;
}

// --- cPmtSidEntry ----------------------------------------------------------

class cPmtSidEntry : public cListObject {
private:
  int sid;
  int pid;
  cPmtPidEntry *pidEntry;
  int version;
  bool received;
public:
  cPmtSidEntry(int Sid, int Pid, cPmtPidEntry *PidEntry);
  int Sid(void) { return sid; }
  int Pid(void) { return pid; }
  cPmtPidEntry *PidEntry(void) { return pidEntry; }
  int Version(void) { return version; }
  int Received(void) { return received; }
  void SetVersion(int Version) { version = Version; }
  void SetReceived(bool State) { received = State; }
  };

cPmtSidEntry::cPmtSidEntry(int Sid, int Pid, cPmtPidEntry *PidEntry)
{
  sid = Sid;
  pid = Pid;
  pidEntry = PidEntry;
  version = -1;
  received = false;
}

// --- cPatFilter ------------------------------------------------------------

//#define DEBUG_PAT_PMT
#ifdef DEBUG_PAT_PMT
#define DBGLOG(a...) { cString s = cString::sprintf(a); fprintf(stderr, "%s\n", *s); dsyslog("%s", *s); }
#else
#define DBGLOG(a...)
#endif

cPatFilter::cPatFilter(void)
{
  Trigger(0);
  Set(0x00, 0x00);  // PAT
}

void cPatFilter::SetStatus(bool On)
{
  cMutexLock MutexLock(&mutex);
  DBGLOG("PAT filter set status %d", On);
  cFilter::SetStatus(On);
  Trigger();
}

void cPatFilter::Trigger(int Sid)
{
  cMutexLock MutexLock(&mutex);
  patVersion = -1;
  sectionSyncer.Reset();
  if (Sid != 0 && activePmt)
     Del(activePmt->Pid(), SI::TableIdPMT);
  activePmt = NULL;
  if (Sid >= 0) {
     sid = Sid;
     DBGLOG("PAT filter trigger SID %d", Sid);
     }
}

bool cPatFilter::PmtPidComplete(int PmtPid)
{
  for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se)) {
      if (se->Pid() == PmtPid && !se->Received())
         return false;
      }
  return true;
}

void cPatFilter::PmtPidReset(int PmtPid)
{
  for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se)) {
      if (se->Pid() == PmtPid)
         se->SetReceived(false);
      }
}

bool cPatFilter::PmtVersionChanged(int PmtPid, int Sid, int Version, bool SetNewVersion)
{
  int i = 0;
  for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se), i++) {
      if (se->Sid() == Sid && se->Pid() == PmtPid) {
         if (!se->Received()) {
            se->SetReceived(true);
            se->PidEntry()->SetComplete(PmtPidComplete(PmtPid));
            }
         if (se->Version() != Version) {
            DBGLOG("PMT %d  %2d %5d/%d %2d -> %2d", Transponder(), i, PmtPid, Sid, se->Version(), Version);
            if (SetNewVersion)
               se->SetVersion(Version);
            return true;
            }
         break;
         }
      }
  return false;
}

void cPatFilter::SwitchToNextPmtPid(void)
{
  if (activePmt) {
     Del(activePmt->Pid(), SI::TableIdPMT);
     if (!(activePmt = pmtPidList.Next(activePmt)))
        activePmt = pmtPidList.First();
     PmtPidReset(activePmt->Pid());
     activePmt->SetComplete(false);
     Add(activePmt->Pid(), SI::TableIdPMT);
     }
}

void cPatFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  cMutexLock MutexLock(&mutex);
  if (Pid == 0x00) {
     if (Tid == SI::TableIdPAT) {
        SI::PAT pat(Data, false);
        if (!pat.CheckCRCAndParse())
           return;
        if (sectionSyncer.Sync(pat.getVersionNumber(), pat.getSectionNumber(), pat.getLastSectionNumber())) {
           DBGLOG("PAT %d %d -> %d %d/%d", Transponder(), patVersion, pat.getVersionNumber(), pat.getSectionNumber(), pat.getLastSectionNumber());
           if (pat.getVersionNumber() != patVersion) {
              if (pat.getLastSectionNumber() > 0)
                 DBGLOG("  PAT %d: %d sections", Transponder(), pat.getLastSectionNumber() + 1);
              if (activePmt) {
                 Del(activePmt->Pid(), SI::TableIdPMT);
                 activePmt = NULL;
                 }
              pmtSidList.Clear();
              pmtPidList.Clear();
              patVersion = pat.getVersionNumber();
              }
           SI::PAT::Association assoc;
           for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it); ) {
               if (!assoc.isNITPid()) {
                  int PmtPid = assoc.getPid();
                  cPmtPidEntry *pPid = NULL;
                  int PidIndex = 0;
                  for (pPid = pmtPidList.First(); pPid && pPid->Pid() != PmtPid; pPid = pmtPidList.Next(pPid))
                      PidIndex++;
                  if (!pPid) { // new PMT Pid
                     pPid = new cPmtPidEntry(PmtPid);
                     pmtPidList.Add(pPid);
                     }
                  pmtSidList.Add(new cPmtSidEntry(assoc.getServiceId(), PmtPid, pPid));
                  DBGLOG("    PMT pid %2d/%2d %5d  SID %5d", PidIndex, pmtSidList.Count() - 1, PmtPid, assoc.getServiceId());
                  if (sid == assoc.getServiceId()) {
                     activePmt = pPid;
                     DBGLOG("sid = %d pidIndex = %d", sid, PidIndex);
                     }
                  }
               }
           if (sectionSyncer.Complete()) { // all PAT sections done
              if (pmtPidList.Count() != pmtSidList.Count())
                 DBGLOG("  PAT %d: shared PMT PIDs", Transponder());
              if (pmtSidList.Count() && !activePmt)
                 activePmt = pmtPidList.First();
              if (activePmt)
                 Add(activePmt->Pid(), SI::TableIdPMT);
              timer.Set(PMT_SCAN_TIMEOUT);
              }
           }
        }
     }
  else if (Tid == SI::TableIdPMT && Source() && Transponder()) {
     timer.Set(PMT_SCAN_TIMEOUT);
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        return;
     if (!PmtVersionChanged(Pid, pmt.getTableIdExtension(), pmt.getVersionNumber(), true)) {
        if (activePmt && activePmt->Complete())
           SwitchToNextPmtPid();
        return;
        }
     cStateKey StateKey;
     cChannels *Channels = cChannels::GetChannelsWrite(StateKey, 10);
     if (!Channels)
        return;
     bool ChannelsModified = false;
     if (activePmt && activePmt->Complete())
        SwitchToNextPmtPid();
     cChannel *Channel = Channels->GetByServiceID(Source(), Transponder(), pmt.getServiceId());
     if (Channel) {
        SI::CaDescriptor *d;
        cCaDescriptors *CaDescriptors = new cCaDescriptors(Channel->Source(), Channel->Transponder(), Channel->Sid(), Pid);
        // Scan the common loop:
        for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)pmt.commonDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
            CaDescriptors->AddCaDescriptor(d, 0);
            delete d;
            }
        // Scan the stream-specific loop:
        SI::PMT::Stream stream;
        int Vpid = 0;
        int Ppid = 0;
        int Vtype = 0;
        int Apids[MAXAPIDS + 1] = { 0 }; // these lists are zero-terminated
        int Atypes[MAXAPIDS + 1] = { 0 };
        int Dpids[MAXDPIDS + 1] = { 0 };
        int Dtypes[MAXDPIDS + 1] = { 0 };
        int Spids[MAXSPIDS + 1] = { 0 };
        uchar SubtitlingTypes[MAXSPIDS + 1] = { 0 };
        uint16_t CompositionPageIds[MAXSPIDS + 1] = { 0 };
        uint16_t AncillaryPageIds[MAXSPIDS + 1] = { 0 };
        char ALangs[MAXAPIDS][MAXLANGCODE2] = { "" };
        char DLangs[MAXDPIDS][MAXLANGCODE2] = { "" };
        char SLangs[MAXSPIDS][MAXLANGCODE2] = { "" };
        int Tpid = 0;
        int NumApids = 0;
        int NumDpids = 0;
        int NumSpids = 0;
        for (SI::Loop::Iterator it; pmt.streamLoop.getNext(stream, it); ) {
            bool ProcessCaDescriptors = false;
            int esPid = stream.getPid();
            switch (stream.getStreamType()) {
              case 1: // STREAMTYPE_11172_VIDEO
              case 2: // STREAMTYPE_13818_VIDEO
              case 0x1B: // H.264
              case 0x24: // H.265
                      Vpid = esPid;
                      Ppid = pmt.getPCRPid();
                      Vtype = stream.getStreamType();
                      ProcessCaDescriptors = true;
                      break;
              case 3: // STREAMTYPE_11172_AUDIO
              case 4: // STREAMTYPE_13818_AUDIO
              case 0x0F: // ISO/IEC 13818-7 Audio with ADTS transport syntax
              case 0x11: // ISO/IEC 14496-3 Audio with LATM transport syntax
                      {
                      if (NumApids < MAXAPIDS) {
                         Apids[NumApids] = esPid;
                         Atypes[NumApids] = stream.getStreamType();
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    SI::ISO639LanguageDescriptor::Language l;
                                    char *s = ALangs[NumApids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; ld->languageLoop.getNext(l, it); ) {
                                        if (*ld->languageCode != '-') { // some use "---" to indicate "none"
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(l.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    }
                                    break;
                               default: ;
                               }
                             delete d;
                             }
                         NumApids++;
                         }
                      ProcessCaDescriptors = true;
                      }
                      break;
              case 5: // STREAMTYPE_13818_PRIVATE
              case 6: // STREAMTYPE_13818_PES_PRIVATE
              //XXX case 8: // STREAMTYPE_13818_DSMCC
                      {
                      int dpid = 0;
                      int dtype = 0;
                      char lang[MAXLANGCODE1] = { 0 };
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::AC3DescriptorTag:
                            case SI::EnhancedAC3DescriptorTag:
                                 dpid = esPid;
                                 dtype = d->getDescriptorTag();
                                 ProcessCaDescriptors = true;
                                 break;
                            case SI::SubtitlingDescriptorTag:
                                 if (NumSpids < MAXSPIDS) {
                                    Spids[NumSpids] = esPid;
                                    SI::SubtitlingDescriptor *sd = (SI::SubtitlingDescriptor *)d;
                                    SI::SubtitlingDescriptor::Subtitling sub;
                                    char *s = SLangs[NumSpids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; sd->subtitlingLoop.getNext(sub, it); ) {
                                        if (sub.languageCode[0]) {
                                           SubtitlingTypes[NumSpids] = sub.getSubtitlingType();
                                           CompositionPageIds[NumSpids] = sub.getCompositionPageId();
                                           AncillaryPageIds[NumSpids] = sub.getAncillaryPageId();
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(sub.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    NumSpids++;
                                    }
                                 break;
                            case SI::TeletextDescriptorTag:
                                 Tpid = esPid;
                                 break;
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 }
                                 break;
                            default: ;
                            }
                          delete d;
                          }
                      if (dpid) {
                         if (NumDpids < MAXDPIDS) {
                            Dpids[NumDpids] = dpid;
                            Dtypes[NumDpids] = dtype;
                            strn0cpy(DLangs[NumDpids], lang, MAXLANGCODE1);
                            NumDpids++;
                            }
                         }
                      }
                      break;
              case 0x80: // STREAMTYPE_USER_PRIVATE
                      if (Setup.StandardCompliance == STANDARD_ANSISCTE) { // DigiCipher II VIDEO (ANSI/SCTE 57)
                         Vpid = esPid;
                         Ppid = pmt.getPCRPid();
                         Vtype = 0x02; // compression based upon MPEG-2
                         ProcessCaDescriptors = true;
                         break;
                         }
                      // fall through
              case 0x81: // STREAMTYPE_USER_PRIVATE
              case 0x87: // eac3
                      if (Setup.StandardCompliance == STANDARD_ANSISCTE) { // ATSC A/53 AUDIO (ANSI/SCTE 57)
                         char lang[MAXLANGCODE1] = { 0 };
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                    }
                                    break;
                               default: ;
                               }
                            delete d;
                            }
                         if (NumDpids < MAXDPIDS) {
                            Dpids[NumDpids] = esPid;
                            Dtypes[NumDpids] = SI::AC3DescriptorTag;
                            strn0cpy(DLangs[NumDpids], lang, MAXLANGCODE1);
                            NumDpids++;
                            }
                         ProcessCaDescriptors = true;
                         break;
                         }
                      // fall through
              case 0x82: // STREAMTYPE_USER_PRIVATE
                      if (Setup.StandardCompliance == STANDARD_ANSISCTE) { // STANDARD SUBTITLE (ANSI/SCTE 27)
                         //TODO
                         break;
                         }
                      // fall through
              case 0x83 ... 0x86: // STREAMTYPE_USER_PRIVATE
              case 0x88 ... 0xFF: // STREAMTYPE_USER_PRIVATE
                      {
                      char lang[MAXLANGCODE1] = { 0 };
                      bool IsAc3 = false;
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::RegistrationDescriptorTag: {
                                 SI::RegistrationDescriptor *rd = (SI::RegistrationDescriptor *)d;
                                 // http://www.smpte-ra.org/mpegreg/mpegreg.html
                                 switch (rd->getFormatIdentifier()) {
                                   case 0x41432D33: // 'AC-3'
                                        IsAc3 = true;
                                        break;
                                   default:
                                        //printf("Format identifier: 0x%08X (pid: %d)\n", rd->getFormatIdentifier(), esPid);
                                        break;
                                   }
                                 }
                                 break;
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 }
                                 break;
                            default: ;
                            }
                         delete d;
                         }
                      if (IsAc3) {
                         if (NumDpids < MAXDPIDS) {
                            Dpids[NumDpids] = esPid;
                            Dtypes[NumDpids] = SI::AC3DescriptorTag;
                            strn0cpy(DLangs[NumDpids], lang, MAXLANGCODE1);
                            NumDpids++;
                            }
                         ProcessCaDescriptors = true;
                         }
                      }
                      break;
              default: ;//printf("PID: %5d %5d %2d %3d %3d\n", pmt.getServiceId(), stream.getPid(), stream.getStreamType(), pmt.getVersionNumber(), Channel->Number());
              }
            if (ProcessCaDescriptors) {
               for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)stream.streamDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
                   CaDescriptors->AddCaDescriptor(d, esPid);
                   delete d;
                   }
               }
            }
        if (Setup.UpdateChannels >= 2) {
           ChannelsModified |= Channel->SetPids(Vpid, Ppid, Vtype, Apids, Atypes, ALangs, Dpids, Dtypes, DLangs, Spids, SLangs, Tpid);
           ChannelsModified |= Channel->SetCaIds(CaDescriptors->CaIds());
           ChannelsModified |= Channel->SetSubtitlingDescriptors(SubtitlingTypes, CompositionPageIds, AncillaryPageIds);
           }
        ChannelsModified |= Channel->SetCaDescriptors(CaDescriptorHandler.AddCaDescriptors(CaDescriptors));
        }
     StateKey.Remove(ChannelsModified);
     }
  if (timer.TimedOut()) {
     if (activePmt)
        DBGLOG("PMT timeout Pid %d", activePmt->Pid());
     SwitchToNextPmtPid();
     timer.Set(PMT_SCAN_TIMEOUT);
     }
}
