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
 * $Id: eit.c 1.82 2003/12/22 10:57:09 kls Exp $
 */

#include "eit.h"
#include "epg.h"
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

  //XXX TODO use complete channel ID
  cChannel *channel = Channels.GetByServiceID(Source, getServiceId());
  if (!channel)
     return; // only collect data for known channels
  tChannelID channelID = channel->GetChannelID();
  channelID.ClrRid();

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
         // If we don't have that event ID yet, we create a new one.
         // Otherwise we copy the information into the existing event anyway, because the data might have changed.
         pEvent = pSchedule->AddEvent(new cEvent(channelID, SiEitEvent.getEventId()));
         if (!pEvent)
            continue;
         pEvent->SetTableID(Tid);
         }
      else {
         // We have found an existing event, either through its event ID or its start time.
         // If the existing event has a zero table ID it was defined externally and shall
         // not be overwritten.
         if (pEvent->TableID() == 0x00)
            continue;
         // If the new event comes from a table that belongs to an "other TS" and the existing
         // one comes from an "actual TS" table, lets skip it.
         if ((!isActualTS()) && (pEvent->TableID() == 0x4E || pEvent->TableID() == 0x50 || pEvent->TableID() == 0x51))
            continue;
         }

      SI::Descriptor *d;
      SI::ExtendedEventDescriptors exGroup;
      char text[256];
      for (SI::Loop::Iterator it2; (d = SiEitEvent.eventDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ExtendedEventDescriptorTag:
                 exGroup.Add((SI::ExtendedEventDescriptor *)d);
                 d = NULL; //so that it is not deleted
                 break;
            case SI::ShortEventDescriptorTag: {
                 SI::ShortEventDescriptor *sed = (SI::ShortEventDescriptor *)d;
                 pEvent->SetTitle(sed->name.getText(text));
                 pEvent->SetShortText(sed->text.getText(text));
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
            default: ;
            }
          delete d;
          }

      if (!rEvent) {
         char buffer[exGroup.getMaximumTextLength()];
         pEvent->SetDescription(exGroup.getText(buffer));
         }

      pEvent->SetStartTime(SiEitEvent.getStartTime());
      pEvent->SetDuration(SiEitEvent.getDuration());
      pEvent->FixEpgBugs();

      if (isPresentFollowing()) {
         if (SiEitEvent.getRunningStatus() == SI::RunningStatusPausing || SiEitEvent.getRunningStatus() == SI::RunningStatusRunning)
            pSchedule->SetPresentEvent(pEvent);
         else if (SiEitEvent.getRunningStatus() == SI::RunningStatusStartsInAFewSeconds)
            pSchedule->SetFollowingEvent(pEvent);
         }
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
  Set(0x12, 0x50, 0xFE);  // event info, actual TS, schedule(0x50)/schedule for another 4 days(0x51)
  Set(0x12, 0x60, 0xFE);  // event info, other  TS, schedule(0x60)/schedule for another 4 days(0x61)
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
