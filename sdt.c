/*
 * sdt.c: SDT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sdt.c 5.3 2026/02/04 10:28:16 kls Exp $
 */

#include "sdt.h"
#include "channels.h"
#include "config.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

// Set to 'true' for debug output:
static bool DebugSdt = false;

#define dbgsdt(a...) if (DebugSdt) fprintf(stderr, a)

// --- cSdtFilter ------------------------------------------------------------

cSdtFilter::cSdtFilter(cPatFilter *PatFilter)
{
  source = cSource::stNone;
  lastSource = cSource::stNone;
  lastTransponder = 0;
  lastNid = 0;
  lastTid = 0;
  patFilter = PatFilter;
  transponderState = tsUnknown;
  Set(0x11, 0x42);  // SDT actual TS
}

void cSdtFilter::SetStatus(bool On)
{
  cMutexLock MutexLock(&mutex);
  cFilter::SetStatus(On);
  sectionSyncer.Reset();
  if (!On)
     source = cSource::stNone;
  transponderState = tsUnknown;
}

void cSdtFilter::Trigger(int Source)
{
  cMutexLock MutexLock(&mutex);
  source = Source;
}

void cSdtFilter::Process(u_short /*Pid*/, u_char /*Tid*/, const u_char *Data, int /*Length*/)
{
  cMutexLock MutexLock(&mutex);
  SI::SDT sdt(Data, false);
  if (!sdt.CheckCRCAndParse())
     return;
  if (transponderState == tsUnknown) {
     // The transponder can be verified with any section, no sync required:
     int Nid = sdt.getOriginalNetworkId();
     int Tid = sdt.getTransportStreamId();
     if (Source() != lastSource || !ISTRANSPONDER(Transponder(), lastTransponder)) {
        // We expect a change in NID/TID:
        if (Nid && Tid && Nid == lastNid && Tid == lastTid) {
           transponderState = tsWrong;
           dsyslog("SDT: channel %d NID/TID (%d/%d) not found, got %d/%d", Channel()->Number(), Channel()->Nid(), Channel()->Tid(), Nid, Tid);
           return;
           }
        }
     // NID/TID is acceptable:
     lastSource = Source();
     lastTransponder = Transponder();
     lastNid = Nid;
     lastTid = Tid;
     if (Nid == Channel()->Nid() && Tid == Channel()->Tid()) {
        // NID/TID correspond with the channel data:
        transponderState = tsVerified;
        }
     else {
        // NID/TID differ from the channel data, but we accept it, since this *is* the data for this transponder:
        transponderState = tsAccepted;
        }
     }
  if (!(source && Transponder()))
     return;
  if (!sectionSyncer.Check(sdt.getVersionNumber(), sdt.getSectionNumber()))
     return;
  cStateKey StateKey;
  cChannels *Channels = cChannels::GetChannelsWrite(StateKey, 10);
  if (!Channels)
     return;
  dbgsdt("SDT: %2d %2d %2d %s %d\n", sdt.getVersionNumber(), sdt.getSectionNumber(), sdt.getLastSectionNumber(), *cSource::ToString(source), Transponder());
  bool ChannelsModified = false;
  bool TriggerPat = false;
  SI::SDT::Service SiSdtService;
  for (SI::Loop::Iterator it; sdt.serviceLoop.getNext(SiSdtService, it); ) {
      cChannel *Channel = Channels->GetByChannelID(tChannelID(source, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId()));
      if (!Channel)
         Channel = Channels->GetByChannelID(tChannelID(source, 0, Transponder(), SiSdtService.getServiceId()));
      if (Channel)
         Channel->SetSeen();

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
                   case 0x0A: // advanced codec digital radio sound service
                   case 0x16: // digital SD television service
                   case 0x19: // digital HD television service
                   case 0x1F: // HEVC digital television service
                   case 0x20: // HEVC UHD digital television service
                        {
                        char NameBuf[Utf8BufSize(1024)];
                        char ShortNameBuf[Utf8BufSize(1024)];
                        char ProviderNameBuf[Utf8BufSize(1024)];
                        sd->serviceName.getText(NameBuf, ShortNameBuf, sizeof(NameBuf), sizeof(ShortNameBuf));
                        char *pn = compactspace(NameBuf);
                        char *ps = compactspace(ShortNameBuf);
                        if (!*ps && cSource::IsCable(source)) {
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
                        if (Channel) {
                           ChannelsModified |= Channel->SetId(Channels, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           if (Setup.UpdateChannels == 1 || Setup.UpdateChannels >= 3)
                              ChannelsModified |= Channel->SetName(pn, ps, pp);
                           // Using SiSdtService.getFreeCaMode() is no good, because some
                           // tv stations set this flag even for non-encrypted channels :-(
                           // The special value 0xFFFF was supposed to mean "unknown encryption"
                           // and would have been overwritten with real CA values later:
                           // Channel->SetCa(SiSdtService.getFreeCaMode() ? 0xFFFF : 0);
                           }
                        else if (*pn && Setup.UpdateChannels >= 4) {
                           dbgsdt("    %5d %5d %5d %s/%s %d %s\n", sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId(), *cSource::ToString(this->Channel()->Source()), *cSource::ToString(source), this->Channel()->Transponder(), pn);
                           Channel = Channels->NewChannel(this->Channel(), pn, ps, pp, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           Channel->SetSource(source); // in case this comes from a satellite with a slightly different position
                           ChannelsModified = true;
                           TriggerPat = true;
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
                 if (Channel) {
                    for (SI::Loop::Iterator it; cid->identifiers.hasNext(it); )
                        Channel->SetCa(Channels, cid->identifiers.getNext(it));
                    }
                 }
                 break;
            */
            case SI::NVODReferenceDescriptorTag: {
                 SI::NVODReferenceDescriptor *nrd = (SI::NVODReferenceDescriptor *)d;
                 SI::NVODReferenceDescriptor::Service Service;
                 for (SI::Loop::Iterator it; nrd->serviceLoop.getNext(Service, it); ) {
                     cChannel *link = Channels->GetByChannelID(tChannelID(source, Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId()));
                     if (!link && Setup.UpdateChannels >= 4) {
                        link = Channels->NewChannel(this->Channel(), "NVOD", "", "", Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId());
                        ChannelsModified = true;
                        TriggerPat = true;
                        }
                     if (link) {
                        if (!LinkChannels)
                           LinkChannels = new cLinkChannels;
                        LinkChannels->Add(new cLinkChannel(link));
                        ChannelsModified = true;
                        }
                     }
                 }
                 break;
            default: ;
            }
          delete d;
          }
      if (LinkChannels) {
         if (Channel)
            ChannelsModified |= Channel->SetLinkChannels(LinkChannels);
         else
            delete LinkChannels;
         }
      }
  if (TriggerPat)
     patFilter->Trigger();
  if (sectionSyncer.Processed(sdt.getSectionNumber(), sdt.getLastSectionNumber())) {
     if (Setup.UpdateChannels == 1 || Setup.UpdateChannels >= 3) {
        ChannelsModified |= Channels->MarkObsoleteChannels(source, sdt.getOriginalNetworkId(), sdt.getTransportStreamId());
        if (source != Source())
           ChannelsModified |= Channels->MarkObsoleteChannels(Source(), sdt.getOriginalNetworkId(), sdt.getTransportStreamId());
        }
     }
  StateKey.Remove(ChannelsModified);
}
