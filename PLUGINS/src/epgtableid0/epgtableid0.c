/*
 * epgtableid0.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: epgtableid0.c 5.1 2025/03/02 11:03:35 kls Exp $
 */

#include <vdr/epg.h>
#include <vdr/plugin.h>

static const char *VERSION        = "2.4.0";
static const char *DESCRIPTION    = "EPG handler for events with table id 0x00";

// --- cTable0Handler --------------------------------------------------------

class cTable0Handler : public cEpgHandler {
private:
  bool Ignore(cEvent *Event) { return Event->TableID() == 0x00; }
public:
  virtual bool SetEventID(cEvent *Event, tEventID EventID) override;
  virtual bool SetStartTime(cEvent *Event, time_t StartTime) override;
  virtual bool SetDuration(cEvent *Event, int Duration) override;
  virtual bool SetTitle(cEvent *Event, const char *Title) override;
  virtual bool SetShortText(cEvent *Event, const char *ShortText) override;
  virtual bool SetDescription(cEvent *Event, const char *Description) override;
  virtual bool SetContents(cEvent *Event, uchar *Contents) override;
  virtual bool SetParentalRating(cEvent *Event, int ParentalRating) override;
  virtual bool SetVps(cEvent *Event, time_t Vps) override;
  virtual bool FixEpgBugs(cEvent *Event) override;
  };

bool cTable0Handler::SetEventID(cEvent *Event, tEventID EventID)
{
  return Ignore(Event);
}

bool cTable0Handler::SetStartTime(cEvent *Event, time_t StartTime)
{
  return Ignore(Event);
}

bool cTable0Handler::SetDuration(cEvent *Event, int Duration)
{
  return Ignore(Event);
}

bool cTable0Handler::SetTitle(cEvent *Event, const char *Title)
{
  return Ignore(Event);
}

bool cTable0Handler::SetShortText(cEvent *Event, const char *ShortText)
{
  return Ignore(Event);
}

bool cTable0Handler::SetDescription(cEvent *Event, const char *Description)
{
  return Ignore(Event);
}

bool cTable0Handler::SetContents(cEvent *Event, uchar *Contents)
{
  return Ignore(Event);
}

bool cTable0Handler::SetParentalRating(cEvent *Event, int ParentalRating)
{
  return Ignore(Event);
}

bool cTable0Handler::SetVps(cEvent *Event, time_t Vps)
{
  return Ignore(Event);
}

bool cTable0Handler::FixEpgBugs(cEvent *Event)
{
  return Ignore(Event);
}

// --- cPluginEpgtableid0 ----------------------------------------------------

class cPluginEpgtableid0 : public cPlugin {
public:
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return DESCRIPTION; }
  virtual bool Initialize(void) override;
  };

bool cPluginEpgtableid0::Initialize(void)
{
  new cTable0Handler;
  return true;
}

VDRPLUGINCREATOR(cPluginEpgtableid0); // Don't touch this!
