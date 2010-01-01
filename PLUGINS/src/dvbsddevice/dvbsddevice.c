/*
 * dvbsddevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbsddevice.c 1.2 2010/01/01 15:01:01 kls Exp $
 */

#include <vdr/plugin.h>
#include "dvbsdffdevice.h"

static const char *VERSION        = "0.0.2";
static const char *DESCRIPTION    = "SD Full Featured DVB device";

class cPluginDvbsddevice : public cPlugin {
private:
  cDvbSdFfDeviceProbe *probe;
public:
  cPluginDvbsddevice(void);
  virtual ~cPluginDvbsddevice();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  };

cPluginDvbsddevice::cPluginDvbsddevice(void)
{
  probe = new cDvbSdFfDeviceProbe;
}

cPluginDvbsddevice::~cPluginDvbsddevice()
{
  delete probe;
}

VDRPLUGINCREATOR(cPluginDvbsddevice); // Don't touch this!
