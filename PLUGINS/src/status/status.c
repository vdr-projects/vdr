/*
 * status.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: status.c 1.7 2002/12/13 15:01:53 kls Exp $
 */

#include <vdr/plugin.h>
#include <vdr/status.h>

static const char *VERSION        = "0.1.0";
static const char *DESCRIPTION    = "Status monitor test";
static const char *MAINMENUENTRY  = NULL;

// ---

class cStatusTest : public cStatus {
protected:
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
  virtual void Recording(const cDevice *Device, const char *Name);
  virtual void Replaying(const cControl *Control, const char *Name);
  virtual void SetVolume(int Volume, bool Absolute);
  virtual void OsdClear(void);
  virtual void OsdTitle(const char *Title);
  virtual void OsdStatusMessage(const char *Message);
  virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue);
  virtual void OsdCurrentItem(const char *Text);
  virtual void OsdTextItem(const char *Text, bool Scroll);
  virtual void OsdChannel(const char *Text);
  virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle);
  };

void cStatusTest::ChannelSwitch(const cDevice *Device, int ChannelNumber)
{
  dsyslog("status: cStatusTest::ChannelSwitch  %d %d", Device->CardIndex(), ChannelNumber);
}

void cStatusTest::Recording(const cDevice *Device, const char *Name)
{
  dsyslog("status: cStatusTest::Recording  %d %s", Device->CardIndex(), Name);
}

void cStatusTest::Replaying(const cControl *Control, const char *Name)
{
  dsyslog("status: cStatusTest::Replaying  %s", Name);
}

void cStatusTest::SetVolume(int Volume, bool Absolute)
{
  dsyslog("status: cStatusTest::SetVolume  %d %d", Volume, Absolute);
}

void cStatusTest::OsdClear(void)
{
  dsyslog("status: cStatusTest::OsdClear");
}

void cStatusTest::OsdTitle(const char *Title)
{
  dsyslog("status: cStatusTest::OsdTitle '%s'", Title);
}

void cStatusTest::OsdStatusMessage(const char *Message)
{
  dsyslog("status: cStatusTest::OsdStatusMessage '%s'", Message);
}

void cStatusTest::OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  dsyslog("status: cStatusTest::OsdHelpKeys %s - %s - %s - %s", Red, Green, Yellow, Blue);
}

void cStatusTest::OsdCurrentItem(const char *Text)
{
  dsyslog("status: cStatusTest::OsdCurrentItem %s", Text);
}

void cStatusTest::OsdTextItem(const char *Text, bool Scroll)
{
  dsyslog("status: cStatusTest::OsdTextItem %s %d", Text, Scroll);
}

void cStatusTest::OsdChannel(const char *Text)
{
  dsyslog("status: cStatusTest::OsdChannel %s", Text);
}

void cStatusTest::OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
  char buffer[25];
  struct tm tm_r;
  dsyslog("status: cStatusTest::OsdProgramme");
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&PresentTime, &tm_r));
  dsyslog("%5s %s", buffer, PresentTitle);
  dsyslog("%5s %s", "", PresentSubtitle);
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&FollowingTime, &tm_r));
  dsyslog("%5s %s", buffer, FollowingTitle);
  dsyslog("%5s %s", "", FollowingSubtitle);
}

// ---

class cPluginStatus : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cStatusTest *statusTest;
public:
  cPluginStatus(void);
  virtual ~cPluginStatus();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Start(void);
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

cPluginStatus::cPluginStatus(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  statusTest = NULL;
}

cPluginStatus::~cPluginStatus()
{
  // Clean up after yourself!
  delete statusTest;
}

const char *cPluginStatus::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginStatus::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginStatus::Start(void)
{
  // Start any background activities the plugin shall perform.
  statusTest = new cStatusTest;
  return true;
}

void cPluginStatus::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginStatus::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return NULL;
}

cMenuSetupPage *cPluginStatus::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginStatus::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginStatus); // Don't touch this!
