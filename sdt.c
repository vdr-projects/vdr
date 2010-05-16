/*
 * sdt.c: SDT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sdt.c 2.5 2010/05/16 14:23:21 kls Exp $
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
  for (SI::Loop::Iterator it; sdt.serviceLoop.getNext(SiSdtService, it); ) {
      cChannel *channel = Channels.GetByChannelID(tChannelID(Source(), sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId()));
      if (!channel)
         channel = Channels.GetByChannelID(tChannelID(Source(), 0, Transponder(), SiSdtService.getServiceId()));

      cLinkChannels *LinkChannels = NULL;
      SI::Descriptor *d;
      for (SI::Loop::Iterator it2; (d = SiSdtService.serviceDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ServiceDescriptorTag: {
                 SI::ServiceDescriptor *sd = (SI::ServiceDescriptor *)d;
                 switch (sd->getServiceType()) {
                   case 0x01: // digital television service
                   case 0x02: // digital radio sound service
                   case 0x04: // NVOD reference service
                   case 0x05: // NVOD time-shifted service
                   case 0x16: // digital SD television service
                   case 0x19: // digital HD television service
                        {
                        char NameBuf[Utf8BufSize(1024)];
                        char ShortNameBuf[Utf8BufSize(1024)];
                        char ProviderNameBuf[Utf8BufSize(1024)];
                        sd->serviceName.getText(NameBuf, ShortNameBuf, sizeof(NameBuf), sizeof(ShortNameBuf));
                        char *pn = compactspace(NameBuf);
                        char *ps = compactspace(ShortNameBuf);
                        if (!*ps && cSource::IsCable(Source())) {
                           // Some cable providers don't mark short channel names according to the
                           // standard, but rather go their own way and use "name>short name":
                           char *p = strchr(pn, '>'); // fix for UPC Wien
                           if (p && p > pn) {
                              *p++ = 0;
                              strcpy(ShortNameBuf, skipspace(p));
                              }
                           }
                        // Avoid ',' in short name (would cause trouble in channels.conf):
                        for (char *p = ShortNameBuf; *p; p++) {
                            if (*p == ',')
                               *p = '.';
                            }
                        sd->providerName.getText(ProviderNameBuf, sizeof(ProviderNameBuf));
                        char *pp = compactspace(ProviderNameBuf);
                        if (channel) {
                           channel->SetId(sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           if (Setup.UpdateChannels == 1 || Setup.UpdateChannels >= 3)
                              channel->SetName(pn, ps, pp);
                           // Using SiSdtService.getFreeCaMode() is no good, because some
                           // tv stations set this flag even for non-encrypted channels :-(
                           // The special value 0xFFFF was supposed to mean "unknown encryption"
                           // and would have been overwritten with real CA values later:
                           // channel->SetCa(SiSdtService.getFreeCaMode() ? 0xFFFF : 0);
                           }
                        else if (*pn && Setup.UpdateChannels >= 4) {
                           channel = Channels.NewChannel(Channel(), pn, ps, pp, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           patFilter->Trigger();
                           }
                        }
                   default: ;
                   }
                 }
                 break;
            // Using the CaIdentifierDescriptor is no good, because some tv stations
            // just don't use it. The actual CA values are collected in pat.c:
            /*
            case SI::CaIdentifierDescriptorTag: {
                 SI::CaIdentifierDescriptor *cid = (SI::CaIdentifierDescriptor *)d;
                 if (channel) {
                    for (SI::Loop::Iterator it; cid->identifiers.hasNext(it); )
                        channel->SetCa(cid->identifiers.getNext(it));
                    }
                 }
                 break;
            */
            case SI::NVODReferenceDescriptorTag: {
                 SI::NVODReferenceDescriptor *nrd = (SI::NVODReferenceDescriptor *)d;
                 SI::NVODReferenceDescriptor::Service Service;
                 for (SI::Loop::Iterator it; nrd->serviceLoop.getNext(Service, it); ) {
                     cChannel *link = Channels.GetByChannelID(tChannelID(Source(), Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId()));
                     if (!link && Setup.UpdateChannels >= 4) {
                        link = Channels.NewChannel(Channel(), "NVOD", "", "", Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId());
                        patFilter->Trigger();
                        }
                     if (link) {
                        if (!LinkChannels)
                           LinkChannels = new cLinkChannels;
                        LinkChannels->Add(new cLinkChannel(link));
                        }
                     }
                 }
                 break;
            default: ;
            }
          delete d;
          }
      if (LinkChannels) {
         if (channel)
            channel->SetLinkChannels(LinkChannels);
         else
            delete LinkChannels;
         }
      }
  Channels.Unlock();
}
