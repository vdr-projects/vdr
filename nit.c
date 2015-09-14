/*
 * nit.c: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.c 4.3 2015/07/26 09:24:36 kls Exp $
 */

#include "nit.h"
#include <linux/dvb/frontend.h>
#include "channels.h"
#include "dvbdevice.h"
#include "eitscan.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"
#include "tools.h"

#define DVB_SYSTEM_1 0 // see also dvbdevice.c
#define DVB_SYSTEM_2 1

#define MAXNETWORKNAME Utf8BufSize(256)

// Set to 'true' for debug output:
static bool DebugNit = false;

#define dbgnit(a...) if (DebugNit) fprintf(stderr, a)

cNitFilter::cNitFilter(cSdtFilter *SdtFilter)
{
  sdtFilter = SdtFilter;
  Set(0x10, SI::TableIdNIT);
}

void cNitFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  sectionSyncer.Reset();
}

void cNitFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  SI::NIT nit(Data, false);
  if (!nit.CheckCRCAndParse())
     return;
  if (!sectionSyncer.Sync(nit.getVersionNumber(), nit.getSectionNumber(), nit.getLastSectionNumber()))
     return;
  if (DebugNit) {
     char NetworkName[MAXNETWORKNAME] = "";
     SI::Descriptor *d;
     for (SI::Loop::Iterator it; (d = nit.commonDescriptors.getNext(it)); ) {
         switch (d->getDescriptorTag()) {
           case SI::NetworkNameDescriptorTag: {
                SI::NetworkNameDescriptor *nnd = (SI::NetworkNameDescriptor *)d;
                nnd->name.getText(NetworkName, MAXNETWORKNAME);
                }
                break;
           default: ;
           }
         delete d;
         }
     dbgnit("NIT: %02X %2d %2d %2d %s %d %d '%s'\n", Tid, nit.getVersionNumber(), nit.getSectionNumber(), nit.getLastSectionNumber(), *cSource::ToString(Source()), nit.getNetworkId(), Transponder(), NetworkName);
     }
  cStateKey StateKey;
  cChannels *Channels = cChannels::GetChannelsWrite(StateKey, 10);
  if (!Channels) {
     sectionSyncer.Repeat(); // let's not miss any section of the NIT
     return;
     }
  bool ChannelsModified = false;
  SI::NIT::TransportStream ts;
  for (SI::Loop::Iterator it; nit.transportStreamLoop.getNext(ts, it); ) {
      SI::Descriptor *d;

      SI::Loop::Iterator it2;
      SI::FrequencyListDescriptor *fld = (SI::FrequencyListDescriptor *)ts.transportStreamDescriptors.getNext(it2, SI::FrequencyListDescriptorTag);
      int NumFrequencies = fld ? fld->frequencies.getCount() + 1 : 1;
      int Frequencies[NumFrequencies];
      if (fld) {
         int ct = fld->getCodingType();
         if (ct > 0) {
            int n = 1;
            for (SI::Loop::Iterator it3; fld->frequencies.hasNext(it3); ) {
                int f = fld->frequencies.getNext(it3);
                switch (ct) {
                  case 1: f = BCD2INT(f) / 100; break;
                  case 2: f = BCD2INT(f) / 10; break;
                  case 3: f = f * 10;  break;
                  default: ;
                  }
                Frequencies[n++] = f;
                dbgnit("    Frequencies[%d] = %d\n", n - 1, f);
                }
            }
         else
            NumFrequencies = 1;
         }
      delete fld;

      for (SI::Loop::Iterator it2; (d = ts.transportStreamDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::SatelliteDeliverySystemDescriptorTag: {
                 SI::SatelliteDeliverySystemDescriptor *sd = (SI::SatelliteDeliverySystemDescriptor *)d;
                 cDvbTransponderParameters dtp;
                 int Source = cSource::FromData(cSource::stSat, BCD2INT(sd->getOrbitalPosition()), sd->getWestEastFlag());
                 int Frequency = Frequencies[0] = BCD2INT(sd->getFrequency()) / 100;
                 static char Polarizations[] = { 'H', 'V', 'L', 'R' };
                 dtp.SetPolarization(Polarizations[sd->getPolarization()]);
                 static int CodeRates[] = { FEC_NONE, FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_8_9, FEC_3_5, FEC_4_5, FEC_9_10, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_NONE };
                 dtp.SetCoderateH(CodeRates[sd->getFecInner()]);
                 static int Modulations[] = { QAM_AUTO, QPSK, PSK_8, QAM_16 };
                 dtp.SetModulation(Modulations[sd->getModulationType()]);
                 dtp.SetSystem(sd->getModulationSystem() ? DVB_SYSTEM_2 : DVB_SYSTEM_1);
                 static int RollOffs[] = { ROLLOFF_35, ROLLOFF_25, ROLLOFF_20, ROLLOFF_AUTO };
                 dtp.SetRollOff(sd->getModulationSystem() ? RollOffs[sd->getRollOff()] : ROLLOFF_AUTO);
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 dbgnit("    %s %d %c %d %d\n", *cSource::ToString(Source), Frequency, Polarizations[sd->getPolarization()], SymbolRate, cChannel::Transponder(Frequency, Polarizations[sd->getPolarization()]));
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    bool forceTransponderUpdate = false;
                    for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                        if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                           int transponder = Channel->Transponder();
                           found = true;
                           if (!ISTRANSPONDER(cChannel::Transponder(Frequency, dtp.Polarization()), transponder)) {
                              for (int n = 0; n < NumFrequencies; n++) {
                                  if (ISTRANSPONDER(cChannel::Transponder(Frequencies[n], dtp.Polarization()), transponder)) {
                                     Frequency = Frequencies[n];
                                     break;
                                     }
                                  }
                              }
                           if (ISTRANSPONDER(cChannel::Transponder(Frequency, dtp.Polarization()), Transponder())) // only modify channels if we're actually receiving this transponder
                              ChannelsModified |= Channel->SetTransponderData(Source, Frequency, SymbolRate, dtp.ToString('S'));
                           else if (Channel->Srate() != SymbolRate || strcmp(Channel->Parameters(), dtp.ToString('S')))
                              forceTransponderUpdate = true; // get us receiving this transponder
                           }
                        }
                    if (!found || forceTransponderUpdate) {
                       for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(NULL, ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetTransponderData(Source, Frequencies[n], SymbolRate, dtp.ToString('S')))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
                           }
                       }
                    }
                 if (ISTRANSPONDER(cChannel::Transponder(Frequency, dtp.Polarization()), Transponder()))
                    sdtFilter->Trigger(Source);
                 }
                 break;
            case SI::S2SatelliteDeliverySystemDescriptorTag: {
                 if (Setup.UpdateChannels >= 5) {
                    for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                        if (!Channel->GroupSep() && cSource::IsSat(Channel->Source()) && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                           SI::S2SatelliteDeliverySystemDescriptor *sd = (SI::S2SatelliteDeliverySystemDescriptor *)d;
                           cDvbTransponderParameters dtp(Channel->Parameters());
                           dtp.SetSystem(DVB_SYSTEM_2);
                           dtp.SetStreamId(sd->getInputStreamIdentifier());
                           ChannelsModified |= Channel->SetTransponderData(Channel->Source(), Channel->Frequency(), Channel->Srate(), dtp.ToString('S'));
                           break;
                           }
                        }
                    }
                 }
                 break;
            case SI::CableDeliverySystemDescriptorTag: {
                 SI::CableDeliverySystemDescriptor *sd = (SI::CableDeliverySystemDescriptor *)d;
                 cDvbTransponderParameters dtp;
                 int Source = cSource::FromData(cSource::stCable);
                 int Frequency = Frequencies[0] = BCD2INT(sd->getFrequency()) / 10;
                 //XXX FEC_outer???
                 static int CodeRates[] = { FEC_NONE, FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_8_9, FEC_3_5, FEC_4_5, FEC_9_10, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_NONE };
                 dtp.SetCoderateH(CodeRates[sd->getFecInner()]);
                 static int Modulations[] = { QPSK, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256, QAM_AUTO };
                 dtp.SetModulation(Modulations[min(sd->getModulation(), 6)]);
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 dbgnit("    %s %d %d %d %d\n", *cSource::ToString(Source), Frequency, CodeRates[sd->getFecInner()], Modulations[min(sd->getModulation(), 6)], SymbolRate);
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    bool forceTransponderUpdate = false;
                    for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                        if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                           int transponder = Channel->Transponder();
                           found = true;
                           if (!ISTRANSPONDER(Frequency / 1000, transponder)) {
                              for (int n = 0; n < NumFrequencies; n++) {
                                  if (ISTRANSPONDER(Frequencies[n] / 1000, transponder)) {
                                     Frequency = Frequencies[n];
                                     break;
                                     }
                                  }
                              }
                           if (ISTRANSPONDER(Frequency / 1000, Transponder())) // only modify channels if we're actually receiving this transponder
                              ChannelsModified |= Channel->SetTransponderData(Source, Frequency, SymbolRate, dtp.ToString('C'));
                           else if (Channel->Srate() != SymbolRate || strcmp(Channel->Parameters(), dtp.ToString('C')))
                              forceTransponderUpdate = true; // get us receiving this transponder
                           }
                        }
                    if (!found || forceTransponderUpdate) {
                       for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(NULL, ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetTransponderData(Source, Frequencies[n], SymbolRate, dtp.ToString('C')))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
                           }
                       }
                    }
                 if (ISTRANSPONDER(Frequency / 1000, Transponder()))
                    sdtFilter->Trigger(Source);
                 }
                 break;
            case SI::TerrestrialDeliverySystemDescriptorTag: {
                 SI::TerrestrialDeliverySystemDescriptor *sd = (SI::TerrestrialDeliverySystemDescriptor *)d;
                 cDvbTransponderParameters dtp;
                 int Source = cSource::FromData(cSource::stTerr);
                 int Frequency = Frequencies[0] = sd->getFrequency() * 10;
                 static int Bandwidths[] = { 8000000, 7000000, 6000000, 5000000, 0, 0, 0, 0 };
                 dtp.SetBandwidth(Bandwidths[sd->getBandwidth()]);
                 static int Constellations[] = { QPSK, QAM_16, QAM_64, QAM_AUTO };
                 dtp.SetModulation(Constellations[sd->getConstellation()]);
                 dtp.SetSystem(DVB_SYSTEM_1);
                 static int Hierarchies[] = { HIERARCHY_NONE, HIERARCHY_1, HIERARCHY_2, HIERARCHY_4, HIERARCHY_AUTO, HIERARCHY_AUTO, HIERARCHY_AUTO, HIERARCHY_AUTO };
                 dtp.SetHierarchy(Hierarchies[sd->getHierarchy()]);
                 static int CodeRates[] = { FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_AUTO, FEC_AUTO, FEC_AUTO };
                 dtp.SetCoderateH(CodeRates[sd->getCodeRateHP()]);
                 dtp.SetCoderateL(CodeRates[sd->getCodeRateLP()]);
                 static int GuardIntervals[] = { GUARD_INTERVAL_1_32, GUARD_INTERVAL_1_16, GUARD_INTERVAL_1_8, GUARD_INTERVAL_1_4 };
                 dtp.SetGuard(GuardIntervals[sd->getGuardInterval()]);
                 static int TransmissionModes[] = { TRANSMISSION_MODE_2K, TRANSMISSION_MODE_8K, TRANSMISSION_MODE_4K, TRANSMISSION_MODE_AUTO };
                 dtp.SetTransmission(TransmissionModes[sd->getTransmissionMode()]);
                 dbgnit("    %s %d %d %d %d %d %d %d %d\n", *cSource::ToString(Source), Frequency, Bandwidths[sd->getBandwidth()], Constellations[sd->getConstellation()], Hierarchies[sd->getHierarchy()], CodeRates[sd->getCodeRateHP()], CodeRates[sd->getCodeRateLP()], GuardIntervals[sd->getGuardInterval()], TransmissionModes[sd->getTransmissionMode()]);
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    bool forceTransponderUpdate = false;
                    for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                        if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                           int transponder = Channel->Transponder();
                           found = true;
                           if (!ISTRANSPONDER(Frequency / 1000000, transponder)) {
                              for (int n = 0; n < NumFrequencies; n++) {
                                  if (ISTRANSPONDER(Frequencies[n] / 1000000, transponder)) {
                                     Frequency = Frequencies[n];
                                     break;
                                     }
                                  }
                              }
                           if (ISTRANSPONDER(Frequency / 1000000, Transponder())) // only modify channels if we're actually receiving this transponder
                              ChannelsModified |= Channel->SetTransponderData(Source, Frequency, 0, dtp.ToString('T'));
                           else if (strcmp(Channel->Parameters(), dtp.ToString('T')))
                              forceTransponderUpdate = true; // get us receiving this transponder
                           }
                        }
                    if (!found || forceTransponderUpdate) {
                       for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(NULL, ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetTransponderData(Source, Frequencies[n], 0, dtp.ToString('T')))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
                           }
                       }
                    }
                 if (ISTRANSPONDER(Frequency / 1000000, Transponder()))
                    sdtFilter->Trigger(Source);
                 }
                 break;
            case SI::ExtensionDescriptorTag: {
                 SI::ExtensionDescriptor *sd = (SI::ExtensionDescriptor *)d;
                 switch (sd->getExtensionDescriptorTag()) {
                   case SI::T2DeliverySystemDescriptorTag: {
                        if (Setup.UpdateChannels >= 5) {
                           for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                               int Source = cSource::FromData(cSource::stTerr);
                               if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                                  SI::T2DeliverySystemDescriptor *td = (SI::T2DeliverySystemDescriptor *)d;
                                  int Frequency = Channel->Frequency();
                                  int SymbolRate = Channel->Srate();
                                  cDvbTransponderParameters dtp(Channel->Parameters());
                                  dtp.SetSystem(DVB_SYSTEM_2);
                                  dtp.SetStreamId(td->getPlpId());
                                  dtp.SetT2SystemId(td->getT2SystemId());
                                  if (td->getExtendedDataFlag()) {
                                     dtp.SetSisoMiso(td->getSisoMiso());
                                     static int T2Bandwidths[] = { 8000000, 7000000, 6000000, 5000000, 10000000, 1712000, 0, 0 };
                                     dtp.SetBandwidth(T2Bandwidths[td->getBandwidth()]);
                                     static int T2GuardIntervals[] = { GUARD_INTERVAL_1_32, GUARD_INTERVAL_1_16, GUARD_INTERVAL_1_8, GUARD_INTERVAL_1_4, GUARD_INTERVAL_1_128, GUARD_INTERVAL_19_128, GUARD_INTERVAL_19_256, 0 };
                                     dtp.SetGuard(T2GuardIntervals[td->getGuardInterval()]);
                                     static int T2TransmissionModes[] = { TRANSMISSION_MODE_2K, TRANSMISSION_MODE_8K, TRANSMISSION_MODE_4K, TRANSMISSION_MODE_1K, TRANSMISSION_MODE_16K, TRANSMISSION_MODE_32K, TRANSMISSION_MODE_AUTO, TRANSMISSION_MODE_AUTO };
                                     dtp.SetTransmission(T2TransmissionModes[td->getTransmissionMode()]);
                                     //TODO add parsing of frequencies
                                     }
                                  ChannelsModified |= Channel->SetTransponderData(Source, Frequency, SymbolRate, dtp.ToString('T'));
                                  }
                               }
                           }
                        }
                        break;
                   default: ;
                   }
                 }
                 break;
            case SI::LogicalChannelDescriptorTag:
                 if (Setup.StandardCompliance == STANDARD_NORDIG) {
                    SI::LogicalChannelDescriptor *lcd = (SI::LogicalChannelDescriptor *)d;
                    SI::LogicalChannelDescriptor::LogicalChannel LogicalChannel;
                    for (SI::Loop::Iterator it4; lcd->logicalChannelLoop.getNext(LogicalChannel, it4); ) {
                        int lcn = LogicalChannel.getLogicalChannelNumber();
                        int sid = LogicalChannel.getServiceId();
                        if (LogicalChannel.getVisibleServiceFlag()) {
                           for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                               if (!Channel->GroupSep() && Channel->Sid() == sid && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                                  ChannelsModified |= Channel->SetLcn(lcn);
                                  break;
                                  }
                               }
                           }
                        }
                    }
                 break;
            case SI::HdSimulcastLogicalChannelDescriptorTag:
                 if (Setup.StandardCompliance == STANDARD_NORDIG) {
                    SI::HdSimulcastLogicalChannelDescriptor *lcd = (SI::HdSimulcastLogicalChannelDescriptor *)d;
                    SI::HdSimulcastLogicalChannelDescriptor::HdSimulcastLogicalChannel HdSimulcastLogicalChannel;
                    for (SI::Loop::Iterator it4; lcd->hdSimulcastLogicalChannelLoop.getNext(HdSimulcastLogicalChannel, it4); ) {
                        int lcn = HdSimulcastLogicalChannel.getLogicalChannelNumber();
                        int sid = HdSimulcastLogicalChannel.getServiceId();
                        if (HdSimulcastLogicalChannel.getVisibleServiceFlag()) {
                           for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
                               if (!Channel->GroupSep() && Channel->Sid() == sid && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                                  ChannelsModified |= Channel->SetLcn(lcn);
                                  break;
                                  }
                               }
                           }
                        }
                    }
                 break;
            default: ;
            }
          delete d;
          }
      }
  StateKey.Remove(ChannelsModified);
}
