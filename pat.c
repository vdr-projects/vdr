/*
 * pat.c: PAT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pat.c 1.1 2003/12/21 15:28:28 kls Exp $
 */

#include "pat.h"
#include <malloc.h>
#include "libsi/section.h"
#include "libsi/descriptor.h"

#define PMT_SCAN_TIMEOUT  10 // seconds

// --- cCaDescriptor ---------------------------------------------------------

class cCaDescriptor : public cListObject {
  friend class cCaDescriptors;
private:
  int source;
  int transponder;
  int serviceId;
  int caSystem;
  int providerId;
  int caPid;
  int length;
  uchar *data;
public:
  cCaDescriptor(int Source, int Transponder, int ServiceId, int CaSystem, int ProviderId, int CaPid, int Length, const uchar *Data);
  virtual ~cCaDescriptor();
  int Length(void) const { return length; }
  const uchar *Data(void) const { return data; }
  };

cCaDescriptor::cCaDescriptor(int Source, int Transponder, int ServiceId, int CaSystem, int ProviderId, int CaPid, int Length, const uchar *Data)
{
  source = Source;
  transponder = Transponder;
  serviceId = ServiceId;
  caSystem = CaSystem;
  providerId = ProviderId;
  caPid = CaPid;
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
//#define DEBUG_CA_DESCRIPTORS 1
#ifdef DEBUG_CA_DESCRIPTORS
  char buffer[1024];
  char *q = buffer;
  q += sprintf(q, "CAM: %04X %5d %5d %04X %6X %04X -", source, transponder, serviceId, caSystem, providerId, caPid);
  for (int i = 0; i < length; i++)
      q += sprintf(q, " %02X", data[i]);
  dsyslog(buffer);
#endif
}

cCaDescriptor::~cCaDescriptor()
{
  free(data);
}

// --- cCaDescriptors --------------------------------------------------------

class cCaDescriptors : public cList<cCaDescriptor> {
private:
  cMutex mutex;
public:
  void NewCaDescriptor(int Source, int Transponder, int ServiceId, SI::CaDescriptor *d);
  int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data);
  };

void cCaDescriptors::NewCaDescriptor(int Source, int Transponder, int ServiceId, SI::CaDescriptor *d)
{
  // The code for determining the ProviderID was taken from 'libdtv'
  // written by Rolf Hakenes <hakenes@hippomi.de>.

  const uchar *Data = d->privateData.getData();
  int Length = d->privateData.getLength();
  int ProviderID = 0;

  switch (d->getCaType() >> 8) {
    case 0x01: // SECA
         ProviderID = (Data[0] << 8) | Data[1];
         break;
    case 0x05: // Viaccess ? (France Telecom)
         for (int i = 0; i < Length; i++) {
             if (Data[i] == 0x14 && Data[i + 1] == 0x03) {
                ProviderID = (Data[i + 2] << 16) |
                             (Data[i + 3] << 8) |
                             (Data[i + 4] & 0xf0);
                break;
                }
             }
         break;
     }

  cMutexLock MutexLock(&mutex);
  for (cCaDescriptor *ca = First(); ca; ca = Next(ca)) {
      if (ca->source == Source && ca->transponder == Transponder && ca->serviceId == ServiceId && ca->caSystem == d->getCaType() && ca->providerId == ProviderID && ca->caPid == d->getCaPid())
         return;
      }
  Add(new cCaDescriptor(Source, Transponder, ServiceId, d->getCaType(), ProviderID, d->getCaPid(), Length, Data));
  //XXX update???
}

int cCaDescriptors::GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data)
{
  if (!CaSystemIds || !*CaSystemIds)
     return 0;
  if (BufSize > 0 && Data) {
     cMutexLock MutexLock(&mutex);
     int length = 0;
     for (cCaDescriptor *d = First(); d; d = Next(d)) {
         if (d->source == Source && d->transponder == Transponder && d->serviceId == ServiceId) {
            const unsigned short *caids = CaSystemIds;
            do {
               if (d->caSystem == *caids) {
                  if (length + d->Length() <= BufSize) {
                     memcpy(Data + length, d->Data(), d->Length());
                     length += d->Length();
                     }
                  else
                     return -1;
                  }
               } while (*++caids);
            }
         }
     return length;
     }
  return -1;
}

cCaDescriptors CaDescriptors;

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data)
{
  return CaDescriptors.GetCaDescriptors(Source, Transponder, ServiceId, CaSystemIds, BufSize, Data);
}

// --- cPatFilter ------------------------------------------------------------

cPatFilter::cPatFilter(void)
{
  pmtIndex = 0;
  pmtPid = 0;
  lastPmtScan = 0;
  Set(0x00, 0x00);  // PAT
}

void cPatFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  pmtIndex = 0;
  pmtPid = 0;
  lastPmtScan = 0;
}

void cPatFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (Pid == 0x00) {
     if (Tid == 0x00) {
        if (pmtPid && time(NULL) - lastPmtScan > PMT_SCAN_TIMEOUT) {
           Del(pmtPid, 0x02);
           pmtPid = 0;
           pmtIndex++;
           lastPmtScan = time(NULL);
           }
        if (!pmtPid) {
           SI::PAT pat(Data, false);
           if (!pat.CheckCRCAndParse())
              return;
           SI::PAT::Association assoc;
           int Index = 0;
           for (SI::Loop::Iterator it; pat.associationLoop.hasNext(it); ) {
               assoc = pat.associationLoop.getNext(it);
               if (!assoc.isNITPid()) {
                  if (Index++ == pmtIndex) {
                     pmtPid = assoc.getPid();
                     Add(pmtPid, 0x02);
                     break;
                     }
                  }
               }
           if (!pmtPid)
              pmtIndex = 0;
           }
        }
     }
  else if (Pid == pmtPid && Tid == SI::TableIdPMT && Source() && Transponder()) {
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        return;
     SI::CaDescriptor *d;
     // Scan the common loop:
     for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)pmt.commonDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
         CaDescriptors.NewCaDescriptor(Source(), Transponder(), pmt.getServiceId(), d);
         delete d;
         }
     // Scan the stream-specific loop:
     SI::PMT::Stream stream;
     for (SI::Loop::Iterator it; pmt.streamLoop.hasNext(it); ) {
         stream = pmt.streamLoop.getNext(it);
         for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)stream.streamDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
             CaDescriptors.NewCaDescriptor(Source(), Transponder(), pmt.getServiceId(), d);
             delete d;
            }
         }
     lastPmtScan = 0; // this triggers the next scan
     }
}
