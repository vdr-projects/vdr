/*
 * eit.c: EIT section filter
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original version (as used in VDR before 1.3.0) written by
 * Robert Schneider <Robert.Schneider@web.de> and Rolf Hakenes <hakenes@hippomi.de>.
 * Adapted to 'libsi' for VDR 1.3.0 by Marcel Wiesweg <marcel.wiesweg@gmx.de>.
 *
 * $Id: eit.c 1.87 2004/02/21 12:20:26 kls Exp $
 */

#include "eit.h"
#include "epg.h"
#include "i18n.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

// --- cEIT ------------------------------------------------------------------

class cEIT : public SI::EIT {
public:
  cEIT(cSchedules *Schedules, int Source, u_char Tid, const u_char *Data);
  };

cEIT::cEIT(cSchedules *Schedules, int Source, u_char Tid, const u_char *Data)
:SI::EIT(Data, false)
{
  if (!CheckCRCAndParse())
     return;

  tChannelID channelID(Source, getOriginalNetworkId(), getTransportStreamId(), getServiceId());
  cChannel *channel = Channels.GetByChannelID(channelID, true);
  if (!channel)
     return; // only collect data for known channels

  cEvent *rEvent = NULL;

  cSchedule *pSchedule = (cSchedule *)Schedules->GetSchedule(channelID);
  if (!pSchedule) {
     pSchedule = new cSchedule(channelID);
     Schedules->Add(pSchedule);
     }

  SI::EIT::Event SiEitEvent;
  for (SI::Loop::Iterator it; eventLoop.hasNext(it); ) {
      SiEitEvent = eventLoop.getNext(it);

      cEvent *pEvent = (cEvent *)pSchedule->GetEvent(SiEitEvent.getEventId(), SiEitEvent.getStartTime());
      if (!pEvent) {
         // If we don't have that event yet, we create a new one.
         // Otherwise we copy the information into the existing event anyway, because the data might have changed.
         pEvent = pSchedule->AddEvent(new cEvent(channelID, SiEitEvent.getEventId()));
         if (!pEvent)
            continue;
         }
      else {
         // We have found an existing event, either through its event ID or its start time.
         // If the existing event has a zero table ID it was defined externally and shall
         // not be overwritten.
         if (pEvent->TableID() == 0x00)
            continue;
         // If the new event has a higher table ID, let's skip it.
         // The lower the table ID, the more "current" the information.
         if (Tid > pEvent->TableID())
            continue;
         // If the new event comes from the same table and has the same version number
         // as the existing one, let's skip it to avoid unnecessary work.
         // Unfortunately some stations (like, e.g. "Premiere") broadcast their EPG data on several transponders (like
         // the actual Premiere transponder and the Sat.1/Pro7 transponder), but use different version numbers on
         // each of them :-( So if one DVB card is tuned to the Premiere transponder, while an other one is tuned
         // to the Sat.1/Pro7 transponder, events will keep toggling because of the bogus version numbers.
         if (Tid == pEvent->TableID() && pEvent->Version() == getVersionNumber())
            continue;
         }
      // XXX TODO log different (non-zero) event IDs for the same event???
      pEvent->SetEventID(SiEitEvent.getEventId()); // unfortunately some stations use different event ids for the same event in different tables :-(
      pEvent->SetTableID(Tid);
      pEvent->SetVersion(getVersionNumber());
      pEvent->SetStartTime(SiEitEvent.getStartTime());
      pEvent->SetDuration(SiEitEvent.getDuration());
      if (isPresentFollowing()) {
         if (SiEitEvent.getRunningStatus() > SI::RunningStatusNotRunning)
            pSchedule->SetRunningStatus(pEvent, SiEitEvent.getRunningStatus());
         }

      int LanguagePreferenceShort = -1;
      int LanguagePreferenceExt = -1;
      bool UseExtendedEventDescriptor = false;
      SI::Descriptor *d;
      SI::ExtendedEventDescriptors *ExtendedEventDescriptors = NULL;
      SI::ShortEventDescriptor *ShortEventDescriptor = NULL;
      cLinkChannels *LinkChannels = NULL;
      for (SI::Loop::Iterator it2; (d = SiEitEvent.eventDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ExtendedEventDescriptorTag: {
                 SI::ExtendedEventDescriptor *eed = (SI::ExtendedEventDescriptor *)d;
                 if (I18nIsPreferredLanguage(Setup.EPGLanguages, I18nLanguageIndex(eed->languageCode), LanguagePreferenceExt) || !ExtendedEventDescriptors) {
                    delete ExtendedEventDescriptors;
                    ExtendedEventDescriptors = new SI::ExtendedEventDescriptors;
                    UseExtendedEventDescriptor = true;
                    }
                 if (UseExtendedEventDescriptor) {
                    ExtendedEventDescriptors->Add(eed);
                    d = NULL; // so that it is not deleted
                    }
                 if (eed->getDescriptorNumber() == eed->getLastDescriptorNumber())
                    UseExtendedEventDescriptor = false;
                 }
                 break;
            case SI::ShortEventDescriptorTag: {
                 SI::ShortEventDescriptor *sed = (SI::ShortEventDescriptor *)d;
                 if (I18nIsPreferredLanguage(Setup.EPGLanguages, I18nLanguageIndex(sed->languageCode), LanguagePreferenceShort) || !ShortEventDescriptor) {
                    delete ShortEventDescriptor;
                    ShortEventDescriptor = sed;
                    d = NULL; // so that it is not deleted
                    }
                 }
                 break;
            case SI::ContentDescriptorTag:
                 break;
            case SI::ParentalRatingDescriptorTag:
                 break;
            case SI::TimeShiftedEventDescriptorTag: {
                 SI::TimeShiftedEventDescriptor *tsed = (SI::TimeShiftedEventDescriptor *)d;
                 cSchedule *rSchedule = (cSchedule *)Schedules->GetSchedule(tChannelID(Source, 0, 0, tsed->getReferenceServiceId()));
                 if (!rSchedule)
                    break;
                 rEvent = (cEvent *)rSchedule->GetEvent(tsed->getReferenceEventId());
                 if (!rEvent)
                    break;
                 pEvent->SetTitle(rEvent->Title());
                 pEvent->SetShortText(rEvent->ShortText());
                 pEvent->SetDescription(rEvent->Description());
                 }
                 break;
            case SI::LinkageDescriptorTag: {
                 SI::LinkageDescriptor *ld = (SI::LinkageDescriptor *)d;
                 tChannelID linkID(Source, ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId());
                 if (ld->getLinkageType() == 0xB0) { // Premiere World
                    time_t now = time(NULL);
                    bool hit = SiEitEvent.getStartTime() <= now && now < SiEitEvent.getStartTime() + SiEitEvent.getDuration();
                    if (hit) {
                       cChannel *link = Channels.GetByChannelID(linkID);
                       if (link != channel) { // only link to other channels, not the same one
                          char linkName[ld->privateData.getLength() + 1];
                          strn0cpy(linkName, (const char *)ld->privateData.getData(), sizeof(linkName));
                          //fprintf(stderr, "Linkage %s %4d %4d %5d %5d %5d %5d  %02X  '%s'\n", hit ? "*" : "", channel->Number(), link ? link->Number() : -1, SiEitEvent.getEventId(), ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId(), ld->getLinkageType(), linkName);//XXX
                          if (link) {
                             if (Setup.UpdateChannels >= 1)
                                link->SetName(linkName);
                             }
                          else if (Setup.UpdateChannels >= 3) {
                             link = Channels.NewChannel(channel, linkName, ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId());
                             //XXX patFilter->Trigger();
                             }
                          if (link) {
                             if (!LinkChannels)
                                LinkChannels = new cLinkChannels;
                             LinkChannels->Add(new cLinkChannel(link));
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

      if (!rEvent) {
         if (ShortEventDescriptor) {
            char buffer[256];
            pEvent->SetTitle(ShortEventDescriptor->name.getText(buffer));
            pEvent->SetShortText(ShortEventDescriptor->text.getText(buffer));
            }
         if (ExtendedEventDescriptors) {
            char buffer[ExtendedEventDescriptors->getMaximumTextLength()];
            pEvent->SetDescription(ExtendedEventDescriptors->getText(buffer));
            }
         }
      delete ExtendedEventDescriptors;
      delete ShortEventDescriptor;

      pEvent->FixEpgBugs();

      if (LinkChannels)
         channel->SetLinkChannels(LinkChannels);
      }
}

// --- cTDT ------------------------------------------------------------------

class cTDT : public SI::TDT {
private:
  static cMutex mutex;
public:
  cTDT(const u_char *Data);
  };

cMutex cTDT::mutex;

cTDT::cTDT(const u_char *Data)
:SI::TDT(Data, false)
{
  CheckParse();

  time_t sattim = getTime();
  time_t loctim = time(NULL);

  if (abs(sattim - loctim) > 2) {
     mutex.Lock();
     isyslog("System Time = %s (%ld)\n", ctime(&loctim), loctim);
     isyslog("Local Time  = %s (%ld)\n", ctime(&sattim), sattim);
     if (stime(&sattim) < 0)
        esyslog("ERROR while setting system time: %m");
     mutex.Unlock();
     }
}

// --- cEitFilter ------------------------------------------------------------

cEitFilter::cEitFilter(void)
{
  Set(0x12, 0x4E, 0xFE);  // event info, actual(0x4E)/other(0x4F) TS, present/following
  Set(0x12, 0x50, 0xF0);  // event info, actual TS, schedule(0x50)/schedule for future days(0x5X)
  Set(0x12, 0x60, 0xF0);  // event info, other  TS, schedule(0x60)/schedule for future days(0x6X)
  Set(0x14, 0x70);        // TDT
}

void cEitFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  switch (Pid) {
    case 0x12: {
         cSchedulesLock SchedulesLock(true, 10);
         cSchedules *Schedules = (cSchedules *)cSchedules::Schedules(SchedulesLock);
         if (Schedules)
            cEIT EIT(Schedules, Source(), Tid, Data);
         }
         break;
    case 0x14: {
         if (Setup.SetSystemTime && Setup.TimeTransponder && ISTRANSPONDER(Transponder(), Setup.TimeTransponder))
            cTDT TDT(Data);
         }
         break;
    }
}
