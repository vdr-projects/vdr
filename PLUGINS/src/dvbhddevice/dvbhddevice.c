/*
 * dvbhddevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbhddevice.c 1.12 2011/04/17 11:20:22 kls Exp $
 */

#include <vdr/plugin.h>
#include "dvbhdffdevice.h"
#include "setup.h"

static const char *VERSION        = "0.0.3";
static const char *DESCRIPTION    = "HD Full Featured DVB device";

class cPluginDvbhddevice : public cPlugin {
private:
  cDvbHdFfDeviceProbe *probe;
public:
  cPluginDvbhddevice(void);
  virtual ~cPluginDvbhddevice();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

cPluginDvbhddevice::cPluginDvbhddevice(void)
{
  probe = new cDvbHdFfDeviceProbe;
}

cPluginDvbhddevice::~cPluginDvbhddevice()
{
  delete probe;
}

cMenuSetupPage *cPluginDvbhddevice::SetupMenu(void)
{
  return new cHdffSetupPage(cDvbHdFfDevice::GetHdffCmdHandler());
}

bool cPluginDvbhddevice::SetupParse(const char *Name, const char *Value)
{
  return gHdffSetup.SetupParse(Name, Value);
}

VDRPLUGINCREATOR(cPluginDvbhddevice); // Don't touch this!
