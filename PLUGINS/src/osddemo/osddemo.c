/*
 * osddemo.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: osddemo.c 1.3 2004/05/16 09:28:51 kls Exp $
 */

#include <vdr/plugin.h>

static const char *VERSION        = "0.1.1";
static const char *DESCRIPTION    = "Demo of arbitrary OSD setup";
static const char *MAINMENUENTRY  = "Osd Demo";

// --- cLineGame -------------------------------------------------------------

class cLineGame : public cOsdObject {
private:
  cOsd *osd;
  int x;
  int y;
  tColor color;
public:
  cLineGame(void);
  ~cLineGame();
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cLineGame::cLineGame(void)
{
  osd = NULL;
  x = y = 50;
  color = clrRed;
}

cLineGame::~cLineGame()
{
  delete osd;
}

void cLineGame::Show(void)
{
  osd = cOsdProvider::NewOsd(100, 50);
  if (osd) {
     tArea Area = { 0, 0, 99, 199,  4 };
     osd->SetAreas(&Area, 1);
     osd->DrawRectangle(0, 0, 99, 199, clrGray50);
     osd->Flush();
     }
}

eOSState cLineGame::ProcessKey(eKeys Key)
{
  eOSState state = cOsdObject::ProcessKey(Key);
  if (state == osUnknown) {
     switch (Key & ~k_Repeat) {
       case kUp:     if (y > 0)   y--; break;
       case kDown:   if (y < 196) y++; break;
       case kLeft:   if (x > 0)   x--; break;
       case kRight:  if (x < 96)  x++; break;
       case kRed:    color = clrRed; break;
       case kGreen:  color = clrGreen; break;
       case kYellow: color = clrYellow; break;
       case kBlue:   color = clrBlue; break;
       case kOk:     return osEnd;
       default: return state;
       }
     osd->DrawRectangle(x, y, x + 3, y + 3, color);
     osd->Flush();
     state = osContinue;
     }
  return state;
}

// --- cPluginOsddemo --------------------------------------------------------

class cPluginOsddemo : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginOsddemo(void);
  virtual ~cPluginOsddemo();
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

cPluginOsddemo::cPluginOsddemo(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginOsddemo::~cPluginOsddemo()
{
  // Clean up after yourself!
}

const char *cPluginOsddemo::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginOsddemo::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginOsddemo::Start(void)
{
  // Start any background activities the plugin shall perform.
  return true;
}

void cPluginOsddemo::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginOsddemo::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return new cLineGame;
}

cMenuSetupPage *cPluginOsddemo::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginOsddemo::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginOsddemo); // Don't touch this!
