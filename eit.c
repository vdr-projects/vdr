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
 * $Id: eit.c 2.12 2010/05/14 14:08:35 kls Exp $
 */

#include "eit.h"
#include "epg.h"
#include "i18n.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"

#define VALID_TIME (31536000 * 2) // two years

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
  time_t Now = time(NULL);
  struct tm tm_r;
  struct tm t = *localtime_r(&Now, &tm_r); // this initializes the time zone in 't'

  if (Now < VALID_TIME)
     return; // we need the current time for handling PDC descriptors

  SI::EIT::Event SiEitEvent;
  for (SI::Loop::Iterator it; eventLoop.getNext(SiEitEvent, it); ) {
      bool ExternalData = false;
      time_t StartTime = SiEitEvent.getStartTime();
      int Duration = SiEitEvent.getDuration();
      // Drop bogus events - but keep NVOD reference events, where all bits of the start time field are set to 1, resulting in a negative number.
      if (StartTime == 0 || StartTime > 0 && Duration == 0)
         continue;
      Empty = false;
      if (!SegmentStart)
         SegmentStart = StartTime;
      SegmentEnd = StartTime + Duration;
      cEvent *newEvent = NULL;
      cEvent *rEvent = NULL;
      cEvent *pEvent = (cEvent *)pSchedule->GetEvent(SiEitEvent.getEventId(), StartTime);
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
         uchar TableID = pEvent->TableID();
         if (TableID == 0x00) {
            if (pEvent->Version() == getVersionNumber())
               continue;
            HasExternalData = ExternalData = true;
            }
         // If the new event has a higher table ID, let's skip it.
         // The lower the table ID, the more "current" the information.
         else if (Tid > TableID)
            continue;
         // If the new event comes from the same table and has the same version number
         // as the existing one, let's skip it to avoid unnecessary work.
         // Unfortunately some stations (like, e.g. "Premiere") broadcast their EPG data on several transponders (like
         // the actual Premiere transponder and the Sat.1/Pro7 transponder), but use different version numbers on
         // each of them :-( So if one DVB card is tuned to the Premiere transponder, while an other one is tuned
         // to the Sat.1/Pro7 transponder, events will keep toggling because of the bogus version numbers.
         else if (Tid == TableID && pEvent->Version() == getVersionNumber())
            continue;
         }
      if (!ExternalData) {
         pEvent->SetEventID(SiEitEvent.getEventId()); // unfortunately some stations use different event ids for the same event in different tables :-(
         pEvent->SetTableID(Tid);
         pEvent->SetStartTime(StartTime);
         pEvent->SetDuration(Duration);
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
            case SI::ContentDescriptorTag: {
                 SI::ContentDescriptor *cd = (SI::ContentDescriptor *)d;
                 SI::ContentDescriptor::Nibble Nibble;
                 int NumContents = 0;
                 uchar Contents[MaxEventContents] = { 0 };
                 for (SI::Loop::Iterator it3; cd->nibbleLoop.getNext(Nibble, it3); ) {
                     if (NumContents < MaxEventContents) {
                        Contents[NumContents] = ((Nibble.getContentNibbleLevel1() & 0xF) << 4) | (Nibble.getContentNibbleLevel2() & 0xF);
                        NumContents++;
                        }
                     }
                 pEvent->SetContents(Contents);
                 }
                 break;
            case SI::ParentalRatingDescriptorTag: {
                 int LanguagePreferenceRating = -1;
                 SI::ParentalRatingDescriptor *prd = (SI::ParentalRatingDescriptor *)d;
                 SI::ParentalRatingDescriptor::Rating Rating;
                 for (SI::Loop::Iterator it3; prd->ratingLoop.getNext(Rating, it3); ) {
                     if (I18nIsPreferredLanguage(Setup.EPGLanguages, Rating.languageCode, LanguagePreferenceRating)) {
                        int ParentalRating = (Rating.getRating() & 0xFF);
                        switch (ParentalRating) {
                          // values defined by the DVB standard (minimum age = rating + 3 years):
                          case 0x01 ... 0x0F: ParentalRating += 3; break;
                          // values defined by broadcaster CSAT (now why didn't they just use 0x07, 0x09 and 0x0D?):
                          case 0x11:          ParentalRating = 10; break;
                          case 0x12:          ParentalRating = 12; break;
                          case 0x13:          ParentalRating = 16; break;
                          default:            ParentalRating = 0;
                          }
                        pEvent->SetParentalRating(ParentalRating);
                        }
                     }
                 }
                 break;
            case SI::PDCDescriptorTag: {
                 SI::PDCDescriptor *pd = (SI::PDCDescriptor *)d;
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
                    bool hit = StartTime <= Now && Now < StartTime + Duration;
                    if (hit) {
                       char linkName[ld->privateData.getLength() + 1];
                       strn0cpy(linkName, (const char *)ld->privateData.getData(), sizeof(linkName));
                       // TODO is there a standard way to determine the character set of this string?
                       cChannel *link = Channels.GetByChannelID(linkID);
                       if (link != channel) { // only link to other channels, not the same one
                          //fprintf(stderr, "Linkage %s %4d %4d %5d %5d %5d %5d  %02X  '%s'\n", hit ? "*" : "", channel->Number(), link ? link->Number() : -1, SiEitEvent.getEventId(), ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId(), ld->getLinkageType(), linkName);//XXX
                          if (link) {
                             if (Setup.UpdateChannels == 1 || Setup.UpdateChannels >= 3)
                                link->SetName(linkName, "", "");
                             }
                          else if (Setup.UpdateChannels >= 4) {
                             cChannel *transponder = channel;
                             if (channel->Tid() != ld->getTransportStreamId())
                                transponder = Channels.GetByTransponderID(linkID);
                             link = Channels.NewChannel(transponder, linkName, "", "", ld->getOriginalNetworkId(), ld->getTransportStreamId(), ld->getServiceId());
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
                 if (1 <= Stream && Stream <= 6 && Type != 0) { // 1=MPEG2-video, 2=MPEG1-audio, 3=subtitles, 4=AC3-audio, 5=H.264-video, 6=HEAAC-audio
                    if (!Components)
                       Components = new cComponents;
                    char buffer[Utf8BufSize(256)];
                    Components->SetComponent(Components->NumComponents(), Stream, Type, I18nNormalizeLanguageCode(cd->languageCode), cd->description.getText(buffer, sizeof(buffer)));
                    }
                 }
                 break;
            default: ;
            }
          delete d;
          }

      if (!rEvent) {
         if (ShortEventDescriptor) {
            char buffer[Utf8BufSize(256)];
            pEvent->SetTitle(ShortEventDescriptor->name.getText(buffer, sizeof(buffer)));
            pEvent->SetShortText(ShortEventDescriptor->text.getText(buffer, sizeof(buffer)));
            }
         else if (!HasExternalData) {
            pEvent->SetTitle(NULL);
            pEvent->SetShortText(NULL);
            }
         if (ExtendedEventDescriptors) {
            char buffer[Utf8BufSize(ExtendedEventDescriptors->getMaximumTextLength(": ")) + 1];
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
  if (Tid == 0x4E) {
     if (Empty && getSectionNumber() == 0)
        // ETR 211: an empty entry in section 0 of table 0x4E means there is currently no event running
        pSchedule->ClrRunningStatus(channel);
     pSchedule->SetPresentSeen();
     }
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
        if (stime(&sattim) == 0)
           isyslog("system time changed from %s (%ld) to %s (%ld)", *TimeToString(loctim), loctim, *TimeToString(sattim), sattim);
        else
           esyslog("ERROR while setting system time: %m");
        }
     lastDiff = diff;
     mutex.Unlock();
     }
}

// --- cEitFilter ------------------------------------------------------------

time_t cEitFilter::disableUntil = 0;

cEitFilter::cEitFilter(void)
{
  Set(0x12, 0x40, 0xC0);  // event info now&next actual/other TS (0x4E/0x4F), future actual/other TS (0x5X/0x6X)
  if (Setup.SetSystemTime && Setup.TimeTransponder)
     Set(0x14, 0x70);     // TDT
}

void cEitFilter::SetDisableUntil(time_t Time)
{
  disableUntil = Time;
}

void cEitFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (disableUntil) {
     if (time(NULL) > disableUntil)
        disableUntil = 0;
     else
        return;
     }
  switch (Pid) {
    case 0x12: {
         if (Tid >= 0x4E && Tid <= 0x6F) {
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
         }
         break;
    case 0x14: {
         if (Setup.SetSystemTime && Setup.TimeTransponder && ISTRANSPONDER(Transponder(), Setup.TimeTransponder))
            cTDT TDT(Data);
         }
         break;
    default: ;
    }
}
