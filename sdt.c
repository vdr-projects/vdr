/*
 * sdt.c: SDT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sdt.c 1.3 2004/01/05 14:30:31 kls Exp $
 */

#include "sdt.h"
#include "channels.h"
#include "config.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

// --- cSdtFilter ------------------------------------------------------------

cSdtFilter::cSdtFilter(cPatFilter *PatFilter)
{
  patFilter = PatFilter;
  Set(0x11, 0x42);  // SDT
}

void cSdtFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  sectionSyncer.Reset();
}

void cSdtFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (!(Source() && Transponder()))
     return;
  SI::SDT sdt(Data, false);
  if (!sdt.CheckCRCAndParse())
     return;
  if (!sectionSyncer.Sync(sdt.getVersionNumber(), sdt.getSectionNumber(), sdt.getLastSectionNumber()))
     return;
  if (!Channels.Lock(true, 10))
     return;
  SI::SDT::Service SiSdtService;
  for (SI::Loop::Iterator it; sdt.serviceLoop.hasNext(it); ) {
      SiSdtService = sdt.serviceLoop.getNext(it);

      cChannel *Channel = Channels.GetByChannelID(tChannelID(Source(), sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId()));
      if (!Channel)
         Channel = Channels.GetByChannelID(tChannelID(Source(), 0, Transponder(), SiSdtService.getServiceId()));

      SI::Descriptor *d;
      for (SI::Loop::Iterator it2; (d = SiSdtService.serviceDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ServiceDescriptorTag: {
                 SI::ServiceDescriptor *sd = (SI::ServiceDescriptor *)d;
                 switch (sd->getServiceType()) {
                   case 0x01: // digital television service
                   //XXX TODO case 0x02: // digital radio sound service
                   //XXX TODO case 0x04: // NVOD reference service
                   //XXX TODO case 0x05: // NVOD time-shifted service
                        {
                        char buffer[1024];
                        char *p = sd->serviceName.getText(buffer);
                        char NameBuf[1024];
                        char ShortNameBuf[1024];
                        char *pn = NameBuf;
                        char *ps = ShortNameBuf;
                        int IsShortName = 0;
                        while (*p) {
                              if ((uchar)*p == 0x86)
                                 IsShortName++;
                              else if ((uchar)*p == 0x87)
                                 IsShortName--;
                              else {
                                 *pn++ = *p;
                                 if (IsShortName)
                                    *ps++ = *p;
                                 }
                              p++;
                              }
                        *pn = *ps = 0;
                        pn = NameBuf;
                        if (*NameBuf && *ShortNameBuf) {
                           *ps++ = ',';
                           strcpy(ps, NameBuf);
                           pn = ShortNameBuf;
                           }
                        if (Channel) {
                           Channel->SetId(sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           if (Setup.UpdateChannels >= 1)
                              Channel->SetName(pn);
                           // Using SiSdtService.getFreeCaMode() is no good, because some
                           // tv stations set this flag even for non-encrypted channels :-(
                           // The special value 0xFFFF was supposed to mean "unknown encryption"
                           // and would have been overwritten with real CA values later:
                           // Channel->SetCa(SiSdtService.getFreeCaMode() ? 0xFFFF : 0);
                           }
                        else if (*pn && Setup.UpdateChannels >= 3) {
                           Channel = Channels.NewChannel(Source(), Transponder(), pn, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           patFilter->Trigger();
                           }
                        }
                   }
                 }
                 break;
            // Using the CaIdentifierDescriptor is no good, because some tv stations
            // just don't use it. The actual CA values are collected in pat.c:
            /*
            case SI::CaIdentifierDescriptorTag: {
                 SI::CaIdentifierDescriptor *cid = (SI::CaIdentifierDescriptor *)d;
                 if (Channel) {
                    for (SI::Loop::Iterator it; cid->identifiers.hasNext(it); )
                        Channel->SetCa(cid->identifiers.getNext(it));
                    }
                 }
                 break;
            */
            case SI::NVODReferenceDescriptorTag: {
                 SI::NVODReferenceDescriptor *nrd = (SI::NVODReferenceDescriptor *)d;
                 for (SI::Loop::Iterator it; nrd->serviceLoop.hasNext(it); ) {
                     SI::NVODReferenceDescriptor::Service Service = nrd->serviceLoop.getNext(it);
                     //printf(" %04X-%04X-%04X\n", Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId());//XXX TODO
                     }
                 }
                 break;
            default: ;
            }
          delete d;
          }
      }
  Channels.Unlock();
}
