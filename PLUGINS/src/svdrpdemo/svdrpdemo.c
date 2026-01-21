/*
 * svdrpdemo.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: svdrpdemo.c 5.2 2026/01/21 20:54:47 kls Exp $
 */

#include <vdr/plugin.h>

static const char *VERSION        = "2.6.0";
static const char *DESCRIPTION    = "How to add SVDRP support to a plugin";

class cPluginSvdrpdemo : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return DESCRIPTION; }
  virtual const char **SVDRPHelpPages(void) override;
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) override;
  };

const char **cPluginSvdrpdemo::SVDRPHelpPages(void)
{
  static const char *HelpPages[] = {
    "DATE\n"
    "    Print the current date.",
    "TIME [ raw ]\n"
    "    Print the current time.\n"
    "    If the optional keyword 'raw' is given, the result will be the\n"
    "    raw time_t data.",
    NULL
    };
  return HelpPages;
}

cString cPluginSvdrpdemo::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if (strcasecmp(Command, "DATE") == 0) {
     // we use the default reply code here
     return DateString(time(NULL));
     }
  else if (strcasecmp(Command, "TIME") == 0) {
     ReplyCode = 901;
     if (*Option) {
        if (strcasecmp(Option, "RAW") == 0)
           return cString::sprintf("%ld\nThis is the number of seconds since the epoch\nand a demo of a multi-line reply", time(NULL));
        else {
           ReplyCode = 504;
           return cString::sprintf("Unknown option: \"%s\"", Option);
           }
        }
     return TimeString(time(NULL));
     }
  return NULL;
}

VDRPLUGINCREATOR(cPluginSvdrpdemo); // Don't touch this!
