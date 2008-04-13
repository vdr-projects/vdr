/*
 * nit.c: NIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: nit.c 2.1 2008/04/12 12:06:40 kls Exp $
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
  numNits = 0;
  networkId = 0;
  Set(0x10, 0x40);  // NIT
}

void cNitFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  numNits = 0;
  networkId = 0;
  sectionSyncer.Reset();
}

void cNitFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  SI::NIT nit(Data, false);
  if (!nit.CheckCRCAndParse())
     return;
  // Some broadcasters send more than one NIT, with no apparent way of telling which
  // one is the right one to use. This is an attempt to find the NIT that contains
  // the transponder it was transmitted on and use only that one:
  int ThisNIT = -1;
  if (!networkId) {
     for (int i = 0; i < numNits; i++) {
         if (nits[i].networkId == nit.getNetworkId()) {
            if (nit.getSectionNumber() == 0) {
               // all NITs have passed by
               for (int j = 0; j < numNits; j++) {
                   if (nits[j].hasTransponder) {
                      networkId = nits[j].networkId;
                      //printf("taking NIT with network ID %d\n", networkId);
                      //XXX what if more than one NIT contains this transponder???
                      break;
                      }
                   }
               if (!networkId) {
                  //printf("none of the NITs contains transponder %d\n", Transponder());
                  return;
                  }
               }
            else {
               ThisNIT = i;
               break;
               }
            }
         }
     if (!networkId && ThisNIT < 0 && numNits < MAXNITS) {
        if (nit.getSectionNumber() == 0) {
           *nits[numNits].name = 0;
           SI::Descriptor *d;
           for (SI::Loop::Iterator it; (d = nit.commonDescriptors.getNext(it)); ) {
               switch (d->getDescriptorTag()) {
                 case SI::NetworkNameDescriptorTag: {
                      SI::NetworkNameDescriptor *nnd = (SI::NetworkNameDescriptor *)d;
                      nnd->name.getText(nits[numNits].name, MAXNETWORKNAME);
                      }
                      break;
                 default: ;
                 }
               delete d;
               }
           nits[numNits].networkId = nit.getNetworkId();
           nits[numNits].hasTransponder = false;
           //printf("NIT[%d] %5d '%s'\n", numNits, nits[numNits].networkId, nits[numNits].name);
           ThisNIT = numNits;
           numNits++;
           }
        }
     }
  else if (networkId != nit.getNetworkId())
     return; // ignore all other NITs
  else if (!sectionSyncer.Sync(nit.getVersionNumber(), nit.getSectionNumber(), nit.getLastSectionNumber()))
     return;
  if (!Channels.Lock(true, 10))
     return;
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
                  }
                Frequencies[n++] = f;
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
                 int Source = cSource::FromData(cSource::stSat, BCD2INT(sd->getOrbitalPosition()), sd->getWestEastFlag());
                 int Frequency = Frequencies[0] = BCD2INT(sd->getFrequency()) / 100;
                 static char Polarizations[] = { 'h', 'v', 'l', 'r' };
                 char Polarization = Polarizations[sd->getPolarization()];
                 static int CodeRates[] = { DVBFE_FEC_NONE, DVBFE_FEC_1_2, DVBFE_FEC_2_3, DVBFE_FEC_3_4, DVBFE_FEC_5_6, DVBFE_FEC_7_8, DVBFE_FEC_8_9, DVBFE_FEC_3_5, DVBFE_FEC_4_5, DVBFE_FEC_9_10, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_NONE };
                 int CodeRate = CodeRates[sd->getFecInner()];
                 static int Modulations[] = { DVBFE_MOD_AUTO, DVBFE_MOD_QPSK, DVBFE_MOD_8PSK, DVBFE_MOD_QAM16 };
                 int Modulation = Modulations[sd->getModulationType()];
                 int System = sd->getModulationSystem() ? DVBFE_DELSYS_DVBS2 : DVBFE_DELSYS_DVBS;
                 static int RollOffs[] = { DVBFE_ROLLOFF_35, DVBFE_ROLLOFF_25, DVBFE_ROLLOFF_20, DVBFE_ROLLOFF_UNKNOWN };
                 int RollOff = sd->getModulationSystem() ? RollOffs[sd->getRollOff()] : DVBFE_ROLLOFF_UNKNOWN;
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 if (ThisNIT >= 0) {
                    for (int n = 0; n < NumFrequencies; n++) {
                        if (ISTRANSPONDER(cChannel::Transponder(Frequencies[n], Polarization), Transponder())) {
                           nits[ThisNIT].hasTransponder = true;
                           //printf("has transponder %d\n", Transponder());
                           break;
                           }
                        }
                    break;
                    }
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
                        if (!Channel->GroupSep() && Channel->Source() == Source && Channel->Nid() == ts.getOriginalNetworkId() && Channel->Tid() == ts.getTransportStreamId()) {
                           int transponder = Channel->Transponder();
                           found = true;
                           if (!ISTRANSPONDER(cChannel::Transponder(Frequency, Polarization), transponder)) {
                              for (int n = 0; n < NumFrequencies; n++) {
                                  if (ISTRANSPONDER(cChannel::Transponder(Frequencies[n], Polarization), transponder)) {
                                     Frequency = Frequencies[n];
                                     break;
                                     }
                                  }
                              }
                           if (ISTRANSPONDER(cChannel::Transponder(Frequency, Polarization), Transponder())) // only modify channels if we're actually receiving this transponder
                              Channel->SetSatTransponderData(Source, Frequency, Polarization, SymbolRate, CodeRate, Modulation, System, RollOff);
                           }
                        }
                    if (!found) {
                       for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetSatTransponderData(Source, Frequencies[n], Polarization, SymbolRate, CodeRate, Modulation, System, RollOff))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
                           }
                       }
                    }
                 }
                 break;
            case SI::CableDeliverySystemDescriptorTag: {
                 SI::CableDeliverySystemDescriptor *sd = (SI::CableDeliverySystemDescriptor *)d;
                 int Source = cSource::FromData(cSource::stCable);
                 int Frequency = Frequencies[0] = BCD2INT(sd->getFrequency()) / 10;
                 //XXX FEC_outer???
                 static int CodeRates[] = { DVBFE_FEC_NONE, DVBFE_FEC_1_2, DVBFE_FEC_2_3, DVBFE_FEC_3_4, DVBFE_FEC_5_6, DVBFE_FEC_7_8, DVBFE_FEC_8_9, DVBFE_FEC_3_5, DVBFE_FEC_4_5, DVBFE_FEC_9_10, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_NONE };
                 int CodeRate = CodeRates[sd->getFecInner()];
                 static int Modulations[] = { DVBFE_MOD_NONE, DVBFE_MOD_QAM16, DVBFE_MOD_QAM32, DVBFE_MOD_QAM64, DVBFE_MOD_QAM128, DVBFE_MOD_QAM256, QAM_AUTO };
                 int Modulation = Modulations[min(sd->getModulation(), 6)];
                 int SymbolRate = BCD2INT(sd->getSymbolRate()) / 10;
                 if (ThisNIT >= 0) {
                    for (int n = 0; n < NumFrequencies; n++) {
                        if (ISTRANSPONDER(Frequencies[n] / 1000, Transponder())) {
                           nits[ThisNIT].hasTransponder = true;
                           //printf("has transponder %d\n", Transponder());
                           break;
                           }
                        }
                    break;
                    }
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
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
                              Channel->SetCableTransponderData(Source, Frequency, Modulation, SymbolRate, CodeRate);
                           }
                        }
                    if (!found) {
                        for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetCableTransponderData(Source, Frequencies[n], Modulation, SymbolRate, CodeRate))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
                           }
                       }
                    }
                 }
                 break;
            case SI::TerrestrialDeliverySystemDescriptorTag: {
                 SI::TerrestrialDeliverySystemDescriptor *sd = (SI::TerrestrialDeliverySystemDescriptor *)d;
                 int Source = cSource::FromData(cSource::stTerr);
                 int Frequency = Frequencies[0] = sd->getFrequency() * 10;
                 static int Bandwidths[] = { DVBFE_BANDWIDTH_8_MHZ, DVBFE_BANDWIDTH_7_MHZ, DVBFE_BANDWIDTH_6_MHZ, DVBFE_BANDWIDTH_5_MHZ, DVBFE_BANDWIDTH_AUTO, DVBFE_BANDWIDTH_AUTO, DVBFE_BANDWIDTH_AUTO, DVBFE_BANDWIDTH_AUTO };
                 int Bandwidth = Bandwidths[sd->getBandwidth()];
                 static int Constellations[] = { DVBFE_MOD_QPSK, DVBFE_MOD_QAM16, DVBFE_MOD_QAM64, DVBFE_MOD_AUTO };
                 int Constellation = Constellations[sd->getConstellation()];
                 static int CodeRates[] = { DVBFE_FEC_1_2, DVBFE_FEC_2_3, DVBFE_FEC_3_4, DVBFE_FEC_5_6, DVBFE_FEC_7_8, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO, DVBFE_FEC_AUTO };
                 int CodeRateHP = CodeRates[sd->getCodeRateHP()];
                 int CodeRateLP = CodeRates[sd->getCodeRateLP()];
                 static int GuardIntervals[] = { DVBFE_GUARD_INTERVAL_1_32, DVBFE_GUARD_INTERVAL_1_16, DVBFE_GUARD_INTERVAL_1_8, DVBFE_GUARD_INTERVAL_1_4 };
                 int GuardInterval = GuardIntervals[sd->getGuardInterval()];
                 static int TransmissionModes[] = { DVBFE_TRANSMISSION_MODE_2K, DVBFE_TRANSMISSION_MODE_8K, DVBFE_TRANSMISSION_MODE_4K, DVBFE_TRANSMISSION_MODE_AUTO };
                 int TransmissionMode = TransmissionModes[sd->getTransmissionMode()];
                 static int Priorities[] = { DVBFE_STREAM_PRIORITY_LP, DVBFE_STREAM_PRIORITY_HP };
                 int Priority = Priorities[sd->getPriority()];
                 static int Alphas[] = { 0, DVBFE_ALPHA_1, DVBFE_ALPHA_2, DVBFE_ALPHA_4 };
                 int Alpha = Alphas[sd->getHierarchy() & 3];
                 int Hierarchy = Alpha ? DVBFE_HIERARCHY_ON : DVBFE_HIERARCHY_OFF;
                 if (ThisNIT >= 0) {
                    for (int n = 0; n < NumFrequencies; n++) {
                        if (ISTRANSPONDER(Frequencies[n] / 1000000, Transponder())) {
                           nits[ThisNIT].hasTransponder = true;
                           //printf("has transponder %d\n", Transponder());
                           break;
                           }
                        }
                    break;
                    }
                 if (Setup.UpdateChannels >= 5) {
                    bool found = false;
                    for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
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
                              Channel->SetTerrTransponderData(Source, Frequency, Bandwidth, Constellation, Hierarchy, CodeRateHP, CodeRateLP, GuardInterval, TransmissionMode, Alpha, Priority);
                           }
                        }
                    if (!found) {
                       for (int n = 0; n < NumFrequencies; n++) {
                           cChannel *Channel = new cChannel;
                           Channel->SetId(ts.getOriginalNetworkId(), ts.getTransportStreamId(), 0, 0);
                           if (Channel->SetTerrTransponderData(Source, Frequencies[n], Bandwidth, Constellation, Hierarchy, CodeRateHP, CodeRateLP, GuardInterval, TransmissionMode, Alpha, Priority))
                              EITScanner.AddTransponder(Channel);
                           else
                              delete Channel;
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
  Channels.Unlock();
}
