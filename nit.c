/*
 * nit.c: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.c 1.3 2004/01/18 10:09:47 kls Exp $
 */

#include "nit.h"
#include <linux/dvb/frontend.h>
#include "channels.h"
#include "eitscan.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"
#include "tools.h"

cNitFilter::cNitFilter(void)
{
  Set(0x10, 0x40);  // NIT
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
  if (!Channels.Lock(true, 10))
     return;
  for (SI::Loop::Iterator it; nit.transportStreamLoop.hasNext(it); ) {
      SI::NIT::TransportStream ts = nit.transportStreamLoop.getNext(it);
      SI::Descriptor *d;
      for (SI::Loop::Iterator it2; (d = ts.transportStreamDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::SatelliteDeliverySystemDescriptorTag: {
                 SI::SatelliteDeliverySystemDescriptor *sd = (SI::SatelliteDeliverySystemDescriptor *)d;
                 int Source = cSource::FromData(cSource::stSat, BCD2INT(sd->getOrbitalPosition()), sd->getWestEastFlag());
                 int Frequency = BCD2INT(sd->getFrequency()) / 100;
                 static char Polarizations[] = { 'h', 'v', 'l', 'r' };
                 char Polarization = Polarizations[sd->getPolarization()];
                 static int CodeRates[] = { FEC_NONE, FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_NONE };
                 int CodeRate = CodeRates[sd->getFecInner()];
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 bool found = false;
                 for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                     if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                        if (Setup.UpdateChannels >= 4)
                           Channel->SetSatTransponderData(Source, Frequency, Polarization, SymbolRate, CodeRate);
                        found = true;
                        }
                     }
                 if (!found && Setup.UpdateChannels >= 4) {
                    cChannel *Channel = new cChannel;
                    Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0, false);
                    if (Channel->SetSatTransponderData(Source, Frequency, Polarization, SymbolRate, CodeRate, false))
                       EITScanner.AddTransponder(Channel);
                    else
                       delete Channel;
                    }
                 }
                 break;
            case SI::CableDeliverySystemDescriptorTag: {
                 SI::CableDeliverySystemDescriptor *sd = (SI::CableDeliverySystemDescriptor *)d;
                 int Source = cSource::FromData(cSource::stCable);
                 int Frequency = BCD2INT(sd->getFrequency()) / 10;
                 //XXX FEC_outer???
                 static int CodeRates[] = { FEC_NONE, FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_AUTO, FEC_NONE };
                 int CodeRate = CodeRates[sd->getFecInner()];
                 static int Modulations[] = { QPSK, QAM_16, QAM_32, QAM_64, QAM_128, QAM_256, QAM_AUTO };
                 int Modulation = Modulations[min(sd->getModulation(), 6)];
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 bool found = false;
                 for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                     if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                        if (Setup.UpdateChannels >= 4)
                           Channel->SetCableTransponderData(Source, Frequency, Modulation, SymbolRate, CodeRate);
                        found = true;
                        }
                     }
                 if (!found && Setup.UpdateChannels >= 4) {
                    cChannel *Channel = new cChannel;
                    Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0, false);
                    if (Channel->SetCableTransponderData(Source, Frequency, Modulation, SymbolRate, CodeRate, false))
                       EITScanner.AddTransponder(Channel);
                    else
                       delete Channel;
                    }
                 }
                 break;
            case SI::TerrestrialDeliverySystemDescriptorTag: {
                 SI::TerrestrialDeliverySystemDescriptor *sd = (SI::TerrestrialDeliverySystemDescriptor *)d;
                 int Source = cSource::FromData(cSource::stCable);
                 int Frequency = sd->getFrequency() * 10;
                 static int Bandwidths[] = { BANDWIDTH_8_MHZ, BANDWIDTH_7_MHZ, BANDWIDTH_6_MHZ, BANDWIDTH_AUTO, BANDWIDTH_AUTO, BANDWIDTH_AUTO, BANDWIDTH_AUTO, BANDWIDTH_AUTO };
                 int Bandwidth = Bandwidths[sd->getBandwidth()];
                 static int Constellations[] = { QPSK, QAM_16, QAM_64, QAM_AUTO };
                 int Constellation = Constellations[sd->getConstellation()];
                 static int Hierarchies[] = { HIERARCHY_NONE, HIERARCHY_1, HIERARCHY_2, HIERARCHY_4, HIERARCHY_AUTO, HIERARCHY_AUTO, HIERARCHY_AUTO, HIERARCHY_AUTO };
                 int Hierarchy = Hierarchies[sd->getHierarchy()];
                 static int CodeRates[] = { FEC_1_2, FEC_2_3, FEC_3_4, FEC_5_6, FEC_7_8, FEC_AUTO, FEC_AUTO, FEC_AUTO };
                 int CodeRateHP = CodeRates[sd->getCodeRateHP()];
                 int CodeRateLP = CodeRates[sd->getCodeRateLP()];
                 static int GuardIntervals[] = { GUARD_INTERVAL_1_32, GUARD_INTERVAL_1_16, GUARD_INTERVAL_1_8, GUARD_INTERVAL_1_4 };
                 int GuardInterval = GuardIntervals[sd->getGuardInterval()];
                 static int TransmissionModes[] = { TRANSMISSION_MODE_2K, TRANSMISSION_MODE_8K, TRANSMISSION_MODE_AUTO, TRANSMISSION_MODE_AUTO };
                 int TransmissionMode = TransmissionModes[sd->getTransmissionMode()];
                 bool found = false;
                 for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                     if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                        if (Setup.UpdateChannels >= 4)
                           Channel->SetTerrTransponderData(Source, Frequency, Bandwidth, Constellation, Hierarchy, CodeRateHP, CodeRateLP, GuardInterval, TransmissionMode);
                        found = true;
                        }
                     }
                 if (!found && Setup.UpdateChannels >= 4) {
                    cChannel *Channel = new cChannel;
                    Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0, false);
                    if (Channel->SetTerrTransponderData(Source, Frequency, Bandwidth, Constellation, Hierarchy, CodeRateHP, CodeRateLP, GuardInterval, TransmissionMode, false))
                       EITScanner.AddTransponder(Channel);
                    else
                       delete Channel;
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
