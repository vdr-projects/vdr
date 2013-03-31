/*
 * dvbsddevice.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbsddevice.c 1.10 2013/03/31 09:30:18 kls Exp $
 */

#include <getopt.h>
#include <vdr/plugin.h>
#include "dvbsdffdevice.h"

static const char *VERSION        = "2.0.0";
static const char *DESCRIPTION    = "SD Full Featured DVB device";

class cPluginDvbsddevice : public cPlugin {
private:
  cDvbSdFfDeviceProbe *probe;
public:
  cPluginDvbsddevice(void);
  virtual ~cPluginDvbsddevice();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return DESCRIPTION; }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  };

cPluginDvbsddevice::cPluginDvbsddevice(void)
{
  probe = new cDvbSdFfDeviceProbe;
}

cPluginDvbsddevice::~cPluginDvbsddevice()
{
  delete probe;
}

const char *cPluginDvbsddevice::CommandLineHelp(void)
{
  return "  -o        --outputonly   do not receive, just use as output device\n";
}

bool cPluginDvbsddevice::ProcessArgs(int argc, char *argv[])
{
  static struct option long_options[] = {
       { "outputonly", no_argument, NULL, 'o' },
       { NULL,         no_argument, NULL,  0  }
     };

  int c;
  while ((c = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (c) {
          case 'o': probe->SetOutputOnly(true);
                    break;
          default:  return false;
          }
        }
  return true;
}

VDRPLUGINCREATOR(cPluginDvbsddevice); // Don't touch this!
