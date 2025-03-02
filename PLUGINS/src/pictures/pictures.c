/*
 * pictures.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: pictures.c 5.2 2025/03/02 11:03:35 kls Exp $
 */

#include <getopt.h>
#include <vdr/plugin.h>
#include "menu.h"
#include "player.h"

static const char *VERSION       = "2.6.1";
static const char *DESCRIPTION   = trNOOP("A simple picture viewer");
static const char *MAINMENUENTRY = trNOOP("Pictures");

// --- cMenuSetupPictures ----------------------------------------------------

class cMenuSetupPictures : public cMenuSetupPage {
private:
  char newPictureDirectory[PATH_MAX];
  int newSlideShowDelay;
protected:
  virtual void Store(void) override;
public:
  cMenuSetupPictures(void);
  };

cMenuSetupPictures::cMenuSetupPictures(void)
{
  strn0cpy(newPictureDirectory, PictureDirectory, sizeof(newPictureDirectory));
  newSlideShowDelay = SlideShowDelay;
  Add(new cMenuEditStrItem(tr("Picture directory"),    newPictureDirectory, sizeof(newPictureDirectory)));
  Add(new cMenuEditIntItem(tr("Slide show delay (s)"), &newSlideShowDelay));
}

void cMenuSetupPictures::Store(void)
{
  SetupStore("PictureDirectory", strn0cpy(PictureDirectory, newPictureDirectory, sizeof(PictureDirectory)));
  SetupStore("SlideShowDelay",   SlideShowDelay = newSlideShowDelay);
}

// --- cPluginPictures -------------------------------------------------------

class cPluginPictures : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginPictures(void);
  virtual ~cPluginPictures() override;
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void) override;
  virtual bool ProcessArgs(int argc, char *argv[]) override;
  virtual const char *MainMenuEntry(void) override { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void) override;
  virtual cMenuSetupPage *SetupMenu(void) override;
  virtual bool SetupParse(const char *Name, const char *Value) override;
  };

cPluginPictures::cPluginPictures(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginPictures::~cPluginPictures()
{
  // Clean up after yourself!
}

const char *cPluginPictures::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -d DIR,   --dir=DIR      set the picture directory to DIR\n";
}

bool cPluginPictures::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  static struct option long_options[] = {
       { "dir",      required_argument, NULL, 'd' },
       { NULL,       no_argument,       NULL,  0  }
     };

  int c;
  while ((c = getopt_long(argc, argv, "d:", long_options, NULL)) != -1) {
        switch (c) {
          case 'd': strn0cpy(PictureDirectory, optarg, sizeof(PictureDirectory));
                    break;
          default:  return false;
          }
        }
  return true;
}

cOsdObject *cPluginPictures::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  if (*PictureDirectory)
     return cPictureMenu::CreatePictureMenu();
  Skins.Message(mtWarning, tr("No picture directory has been defined!"));
  return NULL;
}

cMenuSetupPage *cPluginPictures::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cMenuSetupPictures;
}

bool cPluginPictures::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  if      (!strcasecmp(Name, "PictureDirectory")) strn0cpy(PictureDirectory, Value, sizeof(PictureDirectory));
  else if (!strcasecmp(Name, "SlideShowDelay"))   SlideShowDelay = atoi(Value);
  else
     return false;
  return true;
}

VDRPLUGINCREATOR(cPluginPictures); // Don't touch this!
