/*
 * hello.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: hello.c 1.1 2002/05/09 15:28:51 kls Exp $
 */

#include <getopt.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>
#include "i18n.h"

static const char *VERSION        = "0.0.1";
static const char *DESCRIPTION    = "A friendly greeting";
static const char *MAINMENUENTRY  = "Hello";

class cPluginHello : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  const char *option_a;
  bool option_b;
public:
  cPluginHello(void);
  virtual ~cPluginHello();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual void Start(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdMenu *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  };

cPluginHello::cPluginHello(void)
{
  // Initialize any member varaiables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  option_a = NULL;
  option_b = false;
}

cPluginHello::~cPluginHello()
{
  // Clean up after yourself!
}

const char *cPluginHello::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -a ABC,   --aaa=ABC      do something nice with ABC\n"
         "  -b,       --bbb          activate 'plan B'\n";
}

bool cPluginHello::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  static struct option long_options[] = {
       { "aaa",      required_argument, NULL, 'a' },
       { "bbb",      no_argument,       NULL, 'b' },
       { NULL }
     };

  int c;
  while ((c = getopt_long(argc, argv, "a:b", long_options, NULL)) != -1) {
        switch (c) {
          case 'a': option_a = optarg;
                    break;
          case 'b': option_b = true;
                    break;
          default:  return false;
          }
        }
  return true;
}

void cPluginHello::Start(void)
{
  // Start any background activities the plugin shall perform.
  RegisterI18n(Phrases);
}

cOsdMenu *cPluginHello::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  Interface->Info(tr("Hello world!"));
  return NULL;
}

cMenuSetupPage *cPluginHello::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginHello::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginHello); // Don't touch this!
