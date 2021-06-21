/*
 * pat.c: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.c 5.4 2021/06/21 20:13:55 kls Exp $
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
  int count; // the number of SIDs currently requested from this PID
  int state; // adding/deleting PIDs to/from the filter may only be done from within the Process() function,
             // otherwise there could be a deadlock between cPatFilter::mutex and cSectionHandler::mutex;
             // this member tells whether this PID needs to be added to (>0) or deleted from (<0) the filter
  bool complete; // true if all SIDs on this PID have been received
public:
  cPmtPidEntry(int Pid);
  int Pid(void) { return pid; }
  int Count(void) { return count; }
  int State(void) { int s = state; state = 0; return s; } // returns the current state and resets it
  void SetState(int State) { state = State; } // 1 = add the PID, -1 = delete the PID, 0 = do nothing
  void Inc(void) { if (++count == 1) state = 1; }
  void Dec(void) { if (--count == 0) state = -1; }
  int Complete(void) { return complete; }
  void SetComplete(bool State) { complete = State; }
  };

cPmtPidEntry::cPmtPidEntry(int Pid)
{
  pid = Pid;
  count = 0;
  state = 0;
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
  cPmtSidEntry(int Sid, cPmtPidEntry *PidEntry);
  int Sid(void) { return sid; }
  int Pid(void) { return pid; }
  cPmtPidEntry *PidEntry(void) { return pidEntry; }
  int Version(void) { return version; }
  int Received(void) { return received; }
  void SetVersion(int Version) { version = Version; }
  void SetReceived(bool State) { received = State; }
  };

cPmtSidEntry::cPmtSidEntry(int Sid, cPmtPidEntry *PidEntry)
{
  sid = Sid;
  pid = PidEntry->Pid();
  pidEntry = PidEntry;
  version = -1;
  received = false;
}

// --- cPmtSidRequest --------------------------------------------------------

class cPmtSidRequest : public cListObject {
private:
  int sid;
  int count; // the number of requests for this SID
public:
  cPmtSidRequest(int Sid) { sid = Sid; count = 1; }
  int Sid(void) { return sid; }
  int Count(void) { return count; }
  void Inc(void) { count++; }
  void Dec(void) { count--; }
  };

// --- cPatFilter ------------------------------------------------------------

//#define DEBUG_PAT_PMT
#ifdef DEBUG_PAT_PMT
#define DBGLOG(a...) { cString s = cString::sprintf(a); fprintf(stderr, "%s\n", *s); dsyslog("%s", *s); }
#else
#define DBGLOG(a...) void()
#endif

cPatFilter::cPatFilter(void)
{
  patVersion = -1;
  activePmt = NULL;
  transponder = 0;
  source = 0;
  Set(0x00, 0x00);  // PAT
}

void cPatFilter::SetStatus(bool On)
{
  cMutexLock MutexLock(&mutex);
  if (On) { // restart all requested PMT Pids
     for (cPmtPidEntry *pPid = pmtPidList.First(); pPid; pPid = pmtPidList.Next(pPid))
         pPid->SetState(pPid->Count() > 0);
     if (activePmt && activePmt->Count() == 0) {
        activePmt->SetState(1);
        timer.Set(PMT_SCAN_TIMEOUT);
        }
     }
  DBGLOG("PAT filter set status %d", On);
  cFilter::SetStatus(On);
}

bool cPatFilter::TransponderChanged(void)
{
  if (source != Source() || transponder != Transponder()) {
     DBGLOG("PAT filter transponder changed from %d/%d to %d/%d", source, transponder, Source(), Transponder());
     source = Source();
     transponder = Transponder();
     return true;
     }
  return false;
}

void cPatFilter::Trigger(int)
{
  cMutexLock MutexLock(&mutex);
  DBGLOG("PAT filter trigger");
  patVersion = -1;
  sectionSyncer.Reset();
}

void cPatFilter::Request(int Sid)
{
  cMutexLock MutexLock(&mutex);
  DBGLOG("PAT filter request SID %d", Sid);
  for (cPmtSidRequest *sr = pmtSidRequestList.First(); sr; sr = pmtSidRequestList.Next(sr)) {
      if (sr->Sid() == Sid) {
         sr->Inc();
         DBGLOG("PAT filter add SID request %d (%d)", Sid, sr->Count());
         return;
         }
      }
  DBGLOG("PAT filter new SID request %d", Sid);
  pmtSidRequestList.Add(new cPmtSidRequest(Sid));
  for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se)) {
      if (se->Sid() == Sid) {
         cPmtPidEntry *pPid = se->PidEntry();
         pPid->Inc();
         DBGLOG("    PMT pid %5d  SID %5d (%d)", pPid->Pid(), se->Sid(), pPid->Count());
         break;
         }
      }
}

void cPatFilter::Release(int Sid)
{
  cMutexLock MutexLock(&mutex);
  DBGLOG("PAT filter release SID %d", Sid);
  for (cPmtSidRequest *sr = pmtSidRequestList.First(); sr; sr = pmtSidRequestList.Next(sr)) {
      if (sr->Sid() == Sid) {
         sr->Dec();
         DBGLOG("PAT filter del SID request %d (%d)", Sid, sr->Count());
         if (sr->Count() == 0) {
            pmtSidRequestList.Del(sr);
            for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se)) {
                if (se->Sid() == Sid) {
                   cPmtPidEntry *pPid = se->PidEntry();
                   pPid->Dec();
                   DBGLOG("    PMT pid %5d  SID %5d (%d)", pPid->Pid(), se->Sid(), pPid->Count());
                   break;
                   }
                }
            }
         break;
         }
      }
}

int cPatFilter::NumSidRequests(int Sid)
{
  for (cPmtSidRequest *sr = pmtSidRequestList.First(); sr; sr = pmtSidRequestList.Next(sr)) {
      if (sr->Sid() == Sid)
         return sr->Count();
      }
  return 0;
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
      if (se->Pid() == PmtPid) {
         se->SetReceived(false);
         se->PidEntry()->SetComplete(false);
         }
      }
}

bool cPatFilter::PmtVersionChanged(int PmtPid, int Sid, int Version, bool SetNewVersion)
{
  for (cPmtSidEntry *se = pmtSidList.First(); se; se = pmtSidList.Next(se)) {
      if (se->Sid() == Sid && se->Pid() == PmtPid) {
         if (!se->Received()) {
            se->SetReceived(true);
            se->PidEntry()->SetComplete(PmtPidComplete(PmtPid));
            }
         if (se->Version() != Version) {
            if (SetNewVersion)
               se->SetVersion(Version);
            else
               DBGLOG("PMT %d  %5d/%5d  %2d -> %2d  %d", Transponder(), PmtPid, Sid, se->Version(), Version, NumSidRequests(Sid));
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
     if (activePmt->Count() == 0)
        Del(activePmt->Pid(), SI::TableIdPMT);
     for (;;) {
         activePmt = pmtPidList.Next(activePmt);
         if (!activePmt || activePmt->Count() == 0)
            break;
         }
     if (activePmt) {
        PmtPidReset(activePmt->Pid());
        Add(activePmt->Pid(), SI::TableIdPMT);
        timer.Set(PMT_SCAN_TIMEOUT);
        }
     }
}

void cPatFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  cMutexLock MutexLock(&mutex);
  if (TransponderChanged()) {
     patVersion = -1;
     sectionSyncer.Reset();
     }
  if (patVersion >= 0) {
     for (cPmtPidEntry *pPid = pmtPidList.First(); pPid; pPid = pmtPidList.Next(pPid)) {
         int State = pPid->State();
         if (State > 0)
            Add(pPid->Pid(), SI::TableIdPMT);
         else if (State < 0)
            Del(pPid->Pid(), SI::TableIdPMT);
         }
     }
  else if (Pid != 0x00)
     return;
  if (Pid == 0x00) {
     if (Tid == SI::TableIdPAT) {
        SI::PAT pat(Data, false);
        if (!pat.CheckCRCAndParse())
           return;
        if (sectionSyncer.Check(pat.getVersionNumber(), pat.getSectionNumber())) {
           DBGLOG("PAT %d %d -> %d %d/%d", Transponder(), patVersion, pat.getVersionNumber(), pat.getSectionNumber(), pat.getLastSectionNumber());
           bool NeedsSetStatus = patVersion >= 0;
           if (pat.getVersionNumber() != patVersion) {
              if (NeedsSetStatus)
                 SetStatus(false); // deletes all PIDs from the filter
              activePmt = NULL;
              pmtSidList.Clear();
              pmtPidList.Clear();
              patVersion = pat.getVersionNumber();
              }
           SI::PAT::Association assoc;
           for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it); ) {
               if (!assoc.isNITPid()) {
                  int PmtPid = assoc.getPid();
                  int PmtSid = assoc.getServiceId();
                  cPmtPidEntry *pPid = NULL;
                  for (pPid = pmtPidList.First(); pPid; pPid = pmtPidList.Next(pPid)) {
                      if (pPid->Pid() == PmtPid)
                         break;
                      }
                  int SidRequest = NumSidRequests(PmtSid);
                  DBGLOG("    PMT pid %5d  SID %5d%s%s", PmtPid, PmtSid, SidRequest ? " R" : "", pPid ? " S" : "");
                  if (!pPid) { // new PMT Pid
                     pPid = new cPmtPidEntry(PmtPid);
                     pmtPidList.Add(pPid);
                     }
                  pmtSidList.Add(new cPmtSidEntry(PmtSid, pPid));
                  if (SidRequest > 0)
                     pPid->Inc();
                  }
               }
           if (sectionSyncer.Processed(pat.getSectionNumber(), pat.getLastSectionNumber())) { // all PAT sections done
              for (cPmtPidEntry *pPid = pmtPidList.First(); pPid; pPid = pmtPidList.Next(pPid)) {
                  if (pPid->Count() == 0) {
                     pPid->SetState(1);
                     activePmt = pPid;
                     timer.Set(PMT_SCAN_TIMEOUT);
                     break;
                     }
                  }
              if (NeedsSetStatus)
                 SetStatus(true);
              }
           }
        }
     }
  else if (Tid == SI::TableIdPMT && Source() && Transponder()) {
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        return;
     if (!PmtVersionChanged(Pid, pmt.getTableIdExtension(), pmt.getVersionNumber(), false)) {
        if (activePmt && activePmt->Complete())
           SwitchToNextPmtPid();
        return;
        }
     cStateKey StateKey;
     cChannels *Channels = cChannels::GetChannelsWrite(StateKey, 10);
     if (!Channels)
        return;
     PmtVersionChanged(Pid, pmt.getTableIdExtension(), pmt.getVersionNumber(), true);
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
     }
}
