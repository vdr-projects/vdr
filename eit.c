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
 * $Id: eit.c 1.122 2006/10/09 16:14:36 kls Exp $
 */

#include "eit.h"
#include "epg.h"
#include "i18n.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

// --- cEIT ------------------------------------------------------------------

class cEIT : public SI::EIT {
public:
  cEIT(cSchedules *Schedules, int Source, u_char Tid, const u_char *Data, bool OnlyRunningStatus = false);
  };

cEIT::cEIT(cSchedules *Schedules, int Source, u_char Tid, const u_char *Data, bool OnlyRunningStatus)
:SI::EIT(Data, false)
{
  if (!CheckCRCAndParse())
     return;

  tChannelID channelID(Source, getOriginalNetworkId(), getTransportStreamId(), getServiceId());
  cChannel *channel = Channels.GetByChannelID(channelID, true);
  if (!channel)
     return; // only collect data for known channels

  cSchedule *pSchedule = (cSchedule *)Schedules->GetSchedule(channel, true);

  bool Empty = true;
  bool Modified = false;
  bool HasExternalData = false;
  time_t SegmentStart = 0;
  time_t SegmentEnd = 0;

  SI::EIT::Event SiEitEvent;
  for (SI::Loop::Iterator it; eventLoop.getNext(SiEitEvent, it); ) {
      bool ExternalData = false;
      // Drop bogus events - but keep NVOD reference events, where all bits of the start time field are set to 1, resulting in a negative number.
      if (SiEitEvent.getStartTime() == 0 || SiEitEvent.getStartTime() > 0 && SiEitEvent.getDuration() == 0)
         continue;
      Empty = false;
      if (!SegmentStart)
         SegmentStart = SiEitEvent.getStartTime();
      SegmentEnd = SiEitEvent.getStartTime() + SiEitEvent.getDuration();
      cEvent *newEvent = NULL;
      cEvent *rEvent = NULL;
      cEvent *pEvent = (cEvent *)pSchedule->GetEvent(SiEitEvent.getEventId(), SiEitEvent.getStartTime());
      if (!pEvent) {
         if (OnlyRunningStatus)
            continue;
         // If we don't have that event yet, we create a new one.
         // Otherwise we copy the information into the existing event anyway, because the data might have changed.
         pEvent = newEvent = new cEvent(SiEitEvent.getEventId());
         if (!pEvent)
            continue;
         }
      else {
         // We have found an existing event, either through its event ID or its start time.
         pEvent->SetSeen();
         // If the existing event has a zero table ID it was defined externally and shall
         // not be overwritten.
         if (pEvent->TableID() == 0x00) {
            if (pEvent->Version() == getVersionNumber())
               continue;
            HasExternalData = ExternalData = true;
            }
         // If the new event has a higher table ID, let's skip it.
         // The lower the table ID, the more "current" the information.
         else if (Tid > pEvent->TableID())
            continue;
         // If the new event comes from the same table and has the same version number
         // as the existing one, let's skip it to avoid unnecessary work.
         // Unfortunately some stations (like, e.g. "Premiere") broadcast their EPG data on several transponders (like
         // the actual Premiere transponder and the Sat.1/Pro7 transponder), but use different version numbers on
         // each of them :-( So if one DVB card is tuned to the Premiere transponder, while an other one is tuned
         // to the Sat.1/Pro7 transponder, events will keep toggling because of the bogus version numbers.
         else if (Tid == pEvent->TableID() && pEvent->Version() == getVersionNumber())
            continue;
         }
      if (!ExternalData) {
         pEvent->SetEventID(SiEitEvent.getEventId()); // unfortunately some stations use different event ids for the same event in different tables :-(
         pEvent->SetTableID(Tid);
         pEvent->SetStartTime(SiEitEvent.getStartTime());
         pEvent->SetDuration(SiEitEvent.getDuration());
         }
      if (newEvent)
         pSchedule->AddEvent(newEvent);
      if (Tid == 0x4E) { // we trust only the present/following info on the actual TS
         if (SiEitEvent.getRunningStatus() >= SI::RunningStatusNotRunning)
            pSchedule->SetRunningStatus(pEvent, SiEitEvent.getRunningStatus(), channel);
         }
      if (OnlyRunningStatus)
         continue; // do this before setting the version, so that the full update can be done later
      pEvent->SetVersion(getVersionNumber());

      int LanguagePreferenceShort = -1;
      int LanguagePreferenceExt = -1;
      bool UseExtendedEventDescriptor = false;
      SI::Descriptor *d;
      SI::ExtendedEventDescriptors *ExtendedEventDescriptors = NULL;
      SI::ShortEventDescriptor *ShortEventDescriptor = NULL;
      cLinkChannels *LinkChannels = NULL;
      cComponents *Components = NULL;
      for (SI::Loop::Iterator it2; (d = SiEitEvent.eventDescriptors.getNext(it2)); ) {
          if (ExternalData && d->getDescriptorTag() != SI::ComponentDescriptorTag) {
             delete d;
             continue;
             }
          switch (d->getDescriptorTag()) {
            case SI::ExtendedEventDescriptorTag: {
                 SI::ExtendedEventDescriptor *eed = (SI::ExtendedEventDescriptor *)d;
                 if (I18nIsPreferredLanguage(Setup.EPGLanguages, eed->languageCode, LanguagePreferenceExt) || !ExtendedEventDescriptors) {
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
                 if (I18nIsPreferredLanguage(Setup.EPGLanguages, sed->languageCode, LanguagePreferenceShort) || !ShortEventDescriptor) {
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
            case SI::PDCDescriptorTag: {
                 SI::PDCDescriptor *pd = (SI::PDCDescriptor *)d;
                 time_t now = time(NULL);
                 struct tm tm_r;
                 struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
                 t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
                 int month = t.tm_mon;
                 t.tm_mon = pd->getMonth() - 1;
                 t.tm_mday = pd->getDay();
                 t.tm_hour = pd->getHour();
                 t.tm_min = pd->getMinute();
                 t.tm_sec = 0;
                 if (month == 11 && t.tm_mon == 0) // current month is dec, but event is in jan
                    t.tm_year++;
                 else if (month == 0 && t.tm_mon == 11) // current month is jan, but event is in dec
                    t.tm_year--;
                 time_t vps = mktime(&t);
                 pEvent->SetVps(vps);
                 }
                 break;
            case SI::TimeShiftedEventDescriptorTag: {
                 SI::TimeShiftedEventDescriptor *tsed = (SI::TimeShiftedEventDescriptor *)d;
                 cSchedule *rSchedule = (cSchedule *)Schedules->GetSchedule(tChannelID(Source, channel->Nid(), channel->Tid(), tsed->getReferenceServiceId()));
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
                       char linkName[ld->privateData.getLength() + 1];
                       strn0cpy(linkName, (const char *)ld->privateData.getData(), sizeof(linkName));
                       cChannel *link = Channels.GetByChannelID(linkID);
                       if (link != channel) { // only link to other channels, not the same one
                          //fprintf(stderr, "Linkage %s %4d %4d %5d %5d %5d %5d  %02X  '%s'\n", hit ? "*" : "", channel->Number(), link ? link->Number() : -1, SiEitEvent.getEventId(), ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId(), ld->getLinkageType(), linkName);//XXX
                          if (link) {
                             if (Setup.UpdateChannels == 1 || Setup.UpdateChannels >= 3)
                                link->SetName(linkName, "", "");
                             }
                          else if (Setup.UpdateChannels >= 4) {
                             link = Channels.NewChannel(channel, linkName, "", "", ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId());
                             //XXX patFilter->Trigger();
                             }
                          if (link) {
                             if (!LinkChannels)
                                LinkChannels = new cLinkChannels;
                             LinkChannels->Add(new cLinkChannel(link));
                             }
                          }
                       else
                          channel->SetPortalName(linkName);
                       }
                    }
                 }
                 break;
            case SI::ComponentDescriptorTag: {
                 SI::ComponentDescriptor *cd = (SI::ComponentDescriptor *)d;
                 uchar Stream = cd->getStreamContent();
                 uchar Type = cd->getComponentType();
                 if (1 <= Stream && Stream <= 2 && Type != 0) {
                    if (!Components)
                       Components = new cComponents;
                    char buffer[256];
                    Components->SetComponent(Components->NumComponents(), cd->getStreamContent(), cd->getComponentType(), I18nNormalizeLanguageCode(cd->languageCode), cd->description.getText(buffer, sizeof(buffer)));
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
            pEvent->SetTitle(ShortEventDescriptor->name.getText(buffer, sizeof(buffer)));
            pEvent->SetShortText(ShortEventDescriptor->text.getText(buffer, sizeof(buffer)));
            }
         else if (!HasExternalData) {
            pEvent->SetTitle(NULL);
            pEvent->SetShortText(NULL);
            }
         if (ExtendedEventDescriptors) {
            char buffer[ExtendedEventDescriptors->getMaximumTextLength(": ") + 1];
            pEvent->SetDescription(ExtendedEventDescriptors->getText(buffer, sizeof(buffer), ": "));
            }
         else if (!HasExternalData)
            pEvent->SetDescription(NULL);
         }
      delete ExtendedEventDescriptors;
      delete ShortEventDescriptor;

      pEvent->SetComponents(Components);

      if (!HasExternalData)
         pEvent->FixEpgBugs();
      if (LinkChannels)
         channel->SetLinkChannels(LinkChannels);
      Modified = true;
      }
  if (Empty && Tid == 0x4E && getSectionNumber() == 0)
     // ETR 211: an empty entry in section 0 of table 0x4E means there is currently no event running
     pSchedule->ClrRunningStatus(channel);
  if (Tid == 0x4E)
     pSchedule->SetPresentSeen();
  if (OnlyRunningStatus)
     return;
  if (Modified) {
     pSchedule->Sort();
     if (!HasExternalData)
        pSchedule->DropOutdated(SegmentStart, SegmentEnd, Tid, getVersionNumber());
     Schedules->SetModified(pSchedule);
     }
}

// --- cTDT ------------------------------------------------------------------

class cTDT : public SI::TDT {
private:
  static cMutex mutex;
  static int lastDiff;
public:
  cTDT(const u_char *Data);
  };

cMutex cTDT::mutex;
int cTDT::lastDiff = 0;

cTDT::cTDT(const u_char *Data)
:SI::TDT(Data, false)
{
  CheckParse();

  time_t sattim = getTime();
  time_t loctim = time(NULL);

  int diff = abs(sattim - loctim);
  if (diff > 2) {
     mutex.Lock();
     if (abs(diff - lastDiff) < 3) {
        isyslog("System Time = %s (%ld)", *TimeToString(loctim), loctim);
        isyslog("Local Time  = %s (%ld)", *TimeToString(sattim), sattim);
        if (stime(&sattim) < 0)
           esyslog("ERROR while setting system time: %m");
        }
     lastDiff = diff;
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
         else {
            // If we don't get a write lock, let's at least get a read lock, so
            // that we can set the running status and 'seen' timestamp (well, actually
            // with a read lock we shouldn't be doing that, but it's only integers that
            // get changed, so it should be ok)
            cSchedulesLock SchedulesLock;
            cSchedules *Schedules = (cSchedules *)cSchedules::Schedules(SchedulesLock);
            if (Schedules)
               cEIT EIT(Schedules, Source(), Tid, Data, true);
            }
         }
         break;
    case 0x14: {
         if (Setup.SetSystemTime && Setup.TimeTransponder && ISTRANSPONDER(Transponder(), Setup.TimeTransponder))
            cTDT TDT(Data);
         }
         break;
    }
}
