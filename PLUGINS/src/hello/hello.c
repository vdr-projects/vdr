/*
 * hello.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: hello.c 5.2 2025/04/12 20:40:48 kls Exp $
 */

#include <getopt.h>
#include <stdlib.h>
#include <vdr/i18n.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>

static const char *VERSION        = "2.4.0";
static const char *DESCRIPTION    = trNOOP("A friendly greeting");
static const char *MAINMENUENTRY  = trNOOP("Hello");

class cPluginHello : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  const char *option_a;
  bool option_b;
public:
  cPluginHello(void);
  virtual ~cPluginHello() override;
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void) override;
  virtual bool ProcessArgs(int argc, char *argv[]) override;
  virtual bool Start(void) override;
  virtual void Housekeeping(void) override;
  virtual const char *MainMenuEntry(void) override { return tr(MAINMENUENTRY);}
  virtual cOsdObject *MainMenuAction(void) override;
  virtual cMenuSetupPage *SetupMenu(void) override;
  virtual bool SetupParse(const char *Name, const char *Value) override;
  };

// Global variables that control the overall behaviour:

int GreetingTime = 3;
int UseAlternateGreeting = false;

// --- cMenuSetupHello -------------------------------------------------------

class cMenuSetupHello : public cMenuSetupPage {
private:
  int newGreetingTime;
  int newUseAlternateGreeting;
protected:
  virtual void Store(void) override;
public:
  cMenuSetupHello(void);
  };

cMenuSetupHello::cMenuSetupHello(void)
{
  newGreetingTime = GreetingTime;
  newUseAlternateGreeting = UseAlternateGreeting;
  Add(new cMenuEditIntItem( tr("Greeting time (s)"),      &newGreetingTime));
  Add(new cMenuEditBoolItem(tr("Use alternate greeting"), &newUseAlternateGreeting));
}

void cMenuSetupHello::Store(void)
{
  SetupStore("GreetingTime", GreetingTime = newGreetingTime);
  SetupStore("UseAlternateGreeting", UseAlternateGreeting = newUseAlternateGreeting);
}

// --- cPluginHello ----------------------------------------------------------

cPluginHello::cPluginHello(void)
{
  // Initialize any member variables here.
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
       { NULL,       no_argument,       NULL,  0  }
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

bool cPluginHello::Start(void)
{
  // Start any background activities the plugin shall perform.
  return true;
}

void cPluginHello::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginHello::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  Interface->Confirm(UseAlternateGreeting ? tr("Howdy folks!") : tr("Hello world!"), GreetingTime);
  return NULL;
}

cMenuSetupPage *cPluginHello::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cMenuSetupHello;
}

bool cPluginHello::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  if      (!strcasecmp(Name, "GreetingTime"))         GreetingTime = atoi(Value);
  else if (!strcasecmp(Name, "UseAlternateGreeting")) UseAlternateGreeting = atoi(Value);
  else
     return false;
  return true;
}

VDRPLUGINCREATOR(cPluginHello); // Don't touch this!
