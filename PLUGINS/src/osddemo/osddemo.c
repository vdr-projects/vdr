/*
 * osddemo.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: osddemo.c 2.6 2012/03/13 15:17:33 kls Exp $
 */

#include <vdr/osd.h>
#include <vdr/plugin.h>

static const char *VERSION        = "0.2.3";
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
  virtual ~cLineGame();
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
  osd = cOsdProvider::NewOsd(100, 50, 50);
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

// --- cTrueColorDemo --------------------------------------------------------

class cTrueColorDemo : public cOsdObject, public cThread {
private:
  cOsd *osd;
  cPoint cursor;
  cRect cursorLimits;
  bool clockwise;
  cPixmap *destroyablePixmap;
  cPixmap *toggleablePixmap;
  virtual void Action(void);
  cPixmap *CreateTextPixmap(const char *s, int Line, int Layer, tColor ColorFg, tColor ColorBg, const cFont *Font);
public:
  cTrueColorDemo(void);
  virtual ~cTrueColorDemo();
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cTrueColorDemo::cTrueColorDemo(void)
{
  osd = NULL;
  clockwise = true;
  destroyablePixmap = NULL;
  toggleablePixmap = NULL;
}

cTrueColorDemo::~cTrueColorDemo()
{
  Cancel(3);
  delete osd;
}

cPixmap *cTrueColorDemo::CreateTextPixmap(const char *s, int Line, int Layer, tColor ColorFg, tColor ColorBg, const cFont *Font)
{
  const int h = Font->Height(s);
  int w = Font->Width(s);
  cPixmap *Pixmap = osd->CreatePixmap(Layer, cRect((osd->Width() - w) / 2, Line, w, h));
  if (Pixmap) {
     Pixmap->Clear();
     Pixmap->SetAlpha(0);
     Pixmap->DrawText(cPoint(0, 0), s, ColorFg, ColorBg, Font);
     }
  return Pixmap;
}

void cTrueColorDemo::Action(void)
{
  cPixmap *FadeInPixmap = NULL;
  cPixmap *FadeOutPixmap = NULL;
  cPixmap *MovePixmap = NULL;
  cPixmap *NextPixmap = NULL;
  cPixmap *TilePixmap = NULL;
  cPixmap *ScrollPixmap = NULL;
  cPixmap *AnimPixmap = NULL;
  cFont *OsdFont = cFont::CreateFont(Setup.FontOsd, Setup.FontOsdSize);
  cFont *SmlFont = cFont::CreateFont(Setup.FontSml, Setup.FontSmlSize);
  cFont *LrgFont = cFont::CreateFont(Setup.FontOsd, osd->Height() / 10);
  int FrameTime = 40; // ms
  int FadeTime = 1000; // ms
  int MoveTime = 4000; // ms
  int TileTime = 6000; // ms
  int ScrollWaitTime = 1000; // ms
  int ScrollLineTime = 200; // ms
  int ScrollTotalTime = 8000; // ms
  uint64_t Start = 0;
  uint64_t ScrollStartTime = 0;
  int ScrollLineNumber = 0;
  cPoint MoveStart, MoveEnd;
  cPoint TileStart, TileEnd;
  cPoint ScrollStart, ScrollEnd;
  int Line = osd->Height() / 20;
  int StartLine = Line;
  cPoint OldCursor;
  int State = 0;
  while (Running()) {
        cPixmap::Lock();
        bool Animated = false;
        uint64_t Now = cTimeMs::Now();
        if (FadeInPixmap) {
           double t = min(double(Now - Start) / FadeTime, 1.0);
           int Alpha = t * ALPHA_OPAQUE;
           FadeInPixmap->SetAlpha(Alpha);
           if (t >= 1)
              FadeInPixmap = NULL;
           Animated = true;
           }
        if (FadeOutPixmap) {
           double t = min(double(Now - Start) / FadeTime, 1.0);
           int Alpha = ALPHA_OPAQUE - t * ALPHA_OPAQUE;
           FadeOutPixmap->SetAlpha(Alpha);
           if (t >= 1)
              FadeOutPixmap = NULL;
           Animated = true;
           }
        if (MovePixmap) {
           double t = min(double(Now - Start) / MoveTime, 1.0);
           int x = MoveStart.X() + t * (MoveEnd.X() - MoveStart.X());
           int y = MoveStart.Y() + t * (MoveEnd.Y() - MoveStart.Y());
           cRect r = MovePixmap->ViewPort();
           r.SetPoint(x, y);
           MovePixmap->SetViewPort(r);
           if (t >= 1)
              MovePixmap = NULL;
           Animated = true;
           }
        if (TilePixmap) {
           double t = min(double(Now - Start) / TileTime, 1.0);
           int x = TileStart.X() + t * (TileEnd.X() - TileStart.X());
           int y = TileStart.Y() + t * (TileEnd.Y() - TileStart.Y());
           TilePixmap->SetDrawPortPoint(cPoint(x, y));
           if (t >= 1) {
              destroyablePixmap = TilePixmap;
              TilePixmap = NULL;
              }
           Animated = true;
           }
        if (ScrollPixmap) {
           if (int(Now - Start) > ScrollWaitTime) {
              if (ScrollStartTime) {
                 double t = min(double(Now - ScrollStartTime) / ScrollLineTime, 1.0);
                 int x = ScrollStart.X() + t * (ScrollEnd.X() - ScrollStart.X());
                 int y = ScrollStart.Y() + t * (ScrollEnd.Y() - ScrollStart.Y());
                 ScrollPixmap->SetDrawPortPoint(cPoint(x, y));
                 if (t >= 1) {
                    if (int(Now - Start) < ScrollTotalTime) {
                       cRect r = ScrollPixmap->DrawPort();
                       r.SetPoint(-r.X(), -r.Y());
                       ScrollPixmap->Pan(cPoint(0, 0), r);
                       cString s = cString::sprintf("Line %d", ++ScrollLineNumber);
                       ScrollPixmap->DrawRectangle(cRect(0, ScrollPixmap->ViewPort().Height(), ScrollPixmap->DrawPort().Width(), ScrollPixmap->DrawPort().Height()), clrTransparent);
                       ScrollPixmap->DrawText(cPoint(0, ScrollPixmap->ViewPort().Height()), s, clrYellow, clrTransparent, OsdFont);
                       ScrollStartTime = Now;
                       }
                    else {
                       FadeOutPixmap = ScrollPixmap;
                       ScrollPixmap = NULL;
                       Start = cTimeMs::Now();
                       }
                    }
                 }
              else
                 ScrollStartTime = Now;
              }
           Animated = true;
           }
        if (AnimPixmap) {
           int d = AnimPixmap->ViewPort().Height();
           if (clockwise)
              d = -d;
           cPoint p = AnimPixmap->DrawPort().Point().Shifted(0, d);
           if (clockwise && p.Y() <= -AnimPixmap->DrawPort().Height())
              p.SetY(0);
           else if (!clockwise && p.Y() > 0)
              p.SetY(-(AnimPixmap->DrawPort().Height() - AnimPixmap->ViewPort().Height()));
           AnimPixmap->SetDrawPortPoint(p);
           }
        if (!Animated) {
           switch (State) {
             case 0: {
                       FadeInPixmap = CreateTextPixmap("VDR", Line, 1, clrYellow, clrTransparent, LrgFont);
                       if (FadeInPixmap)
                          Line += FadeInPixmap->DrawPort().Height();
                       Start = cTimeMs::Now();
                       State++;
                     }
                     break;
             case 1: { 
                       FadeInPixmap = CreateTextPixmap("Video Disk Recorder", Line, 3, clrYellow, clrTransparent, OsdFont);
                       if (FadeInPixmap)
                          Line += FadeInPixmap->DrawPort().Height();
                       Start = cTimeMs::Now();
                       State++;
                     }
                     break;
             case 2: {
                       FadeInPixmap = CreateTextPixmap("True Color OSD Demo", Line, 1, clrYellow, clrTransparent, OsdFont);
                       if (FadeInPixmap)
                          Line += FadeInPixmap->DrawPort().Height();
                       Start = cTimeMs::Now();
                       State++;
                     }
                     break;
             case 3: {
                       NextPixmap = CreateTextPixmap("Millions of colors", Line, 1, clrYellow, clrTransparent, LrgFont);
                       if (NextPixmap) {
                          FadeInPixmap = NextPixmap;
                          Start = cTimeMs::Now();
                          StartLine = Line;
                          Line += NextPixmap->DrawPort().Height();
                          }
                       State++;
                     }
                     break;
             case 4: {
                       Line += osd->Height() / 10;
                       int w = osd->Width() / 2;
                       int h = osd->Height() - Line - osd->Height() / 10;
                       cImage Image(cSize(w, h));
                       for (int y = 0; y < h; y++) {
                           for (int x = 0; x < w; x++)
                               Image.SetPixel(cPoint(x, y), HsvToColor(360 * double(x) / w, 1 - double(y) / h, 1) | 0xDF000000);
                           }
                       if (cPixmap *Pixmap = osd->CreatePixmap(2, cRect((osd->Width() - w) / 2, Line, w, h))) {
                          Pixmap->DrawImage(cPoint(0, 0), Image);
                          toggleablePixmap = Pixmap;
                          }
                       State++;
                     }
                     break;
             case 5: {
                       if (NextPixmap) {
                          MovePixmap = NextPixmap;
                          MoveStart = MovePixmap->ViewPort().Point();
                          MoveEnd.Set(osd->Width() - MovePixmap->ViewPort().Width(), osd->Height() - MovePixmap->ViewPort().Height());
                          Start = cTimeMs::Now();
                          }
                       State++;
                     }
                     break;
             case 6: {
                       TilePixmap = CreateTextPixmap("Tiled Pixmaps", StartLine, 1, clrRed, clrWhite, OsdFont);
                       if (TilePixmap) {
                          TilePixmap->SetViewPort(TilePixmap->ViewPort().Grown(TilePixmap->DrawPort().Width(), TilePixmap->DrawPort().Height()));
                          TilePixmap->SetAlpha(200);
                          TilePixmap->SetTile(true);
                          TileStart = TilePixmap->DrawPort().Point();
                          TileEnd = TileStart.Shifted(TilePixmap->ViewPort().Width(), TilePixmap->ViewPort().Height());
                          MovePixmap = TilePixmap;
                          MoveStart = MovePixmap->ViewPort().Point();
                          MoveEnd.Set(10, osd->Height() - MovePixmap->ViewPort().Height() - 10);
                          Start = cTimeMs::Now();
                          }
                       State++;
                     }
                     break;
             case 7: {
                       const char *Text = "Scrolling Pixmaps";
                       int w = OsdFont->Width(Text);
                       int h = OsdFont->Height();
                       if (cPixmap *Pixmap = osd->CreatePixmap(2, cRect((osd->Width() - w) / 2, StartLine, w, 2 * h), cRect(0, 0, w, 3 * h))) {
                          Pixmap->Clear();
                          Pixmap->DrawText(cPoint(0, 0), Text, clrYellow, clrTransparent, OsdFont);
                          cString s = cString::sprintf("Line %d", ++ScrollLineNumber);
                          Pixmap->DrawText(cPoint(0, Pixmap->ViewPort().Height()), s, clrYellow, clrTransparent, OsdFont);
                          ScrollPixmap = Pixmap;
                          ScrollStart.Set(0, 0);
                          ScrollEnd.Set(0, -h);
                          Start = cTimeMs::Now();
                          }
                       State++;
                     }
                     break;
             case 8: {
                       const char *Text = "Animation";
                       const int Size = SmlFont->Width(Text) + 10;
                       const int NumDots = 12;
                       const int AnimFrames = NumDots;
                       // Temporarily using pixmap layer 0 to have the text alpha blended:
                       AnimPixmap = osd->CreatePixmap(0, cRect((osd->Width() - Size) / 2, StartLine, Size, Size), cRect(0, 0, Size, Size * AnimFrames));
                       if (AnimPixmap) {
                          AnimPixmap->SetAlpha(0);
                          AnimPixmap->Clear();
                          const int Diameter = Size / 5;
                          int xc = Size / 2 - Diameter / 2;
                          for (int Frame = 0; Frame < AnimFrames; Frame++) {
                              AnimPixmap->DrawEllipse(cRect(0, Frame * Size, Size, Size), 0xDDFFFFFF);
                              int yc = Frame * Size + Size / 2 - Diameter / 2;
                              int Color = 0xFF;
                              int Delta = Color / NumDots / 3;
                              for (int a = 0; a < NumDots; a++) {
                                  double t = 2 * M_PI * (Frame + a) / NumDots;
                                  int x = xc + ((Size - Diameter) / 2 - 5) * cos(t);
                                  int y = yc + ((Size - Diameter) / 2 - 5) * sin(t);
                                  AnimPixmap->DrawEllipse(cRect(x, y, Diameter, Diameter), ArgbToColor(0xFF, Color, Color, Color));
                                  Color -= Delta;
                                  }
                              AnimPixmap->DrawText(cPoint(0, Frame * Size), Text, clrBlack, clrTransparent, SmlFont, Size, Size, taCenter);
                              }
                          AnimPixmap->SetLayer(3); // now setting the actual pixmap layer
                          FadeInPixmap = AnimPixmap;
                          LOCK_THREAD;
                          OldCursor = cursor = AnimPixmap->ViewPort().Point();
                          cursorLimits.Set(0, 0, osd->Width(), osd->Height());
                          cursorLimits.SetRight(cursorLimits.Right() - Size);
                          cursorLimits.SetBottom(cursorLimits.Bottom() - Size);
                          cursorLimits.Grow(-10, -10);
                          Start = cTimeMs::Now();
                          }
                       State++;
                     }
                     break;
             case 9: {
                       LOCK_THREAD;
                       if (cursor != OldCursor) {
                          MovePixmap = AnimPixmap;
                          MoveStart = MovePixmap->ViewPort().Point();
                          MoveEnd = OldCursor = cursor;
                          MoveTime = 500;
                          Start = cTimeMs::Now();
                          }
                     }
                     break;
             }
           }
        osd->Flush();
        cPixmap::Unlock();
        int Delta = cTimeMs::Now() - Now;
        if (Delta < FrameTime)
           cCondWait::SleepMs(FrameTime - Delta);
        }
  delete OsdFont;
  delete SmlFont;
  delete LrgFont;
}

void cTrueColorDemo::Show(void)
{
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop(), 50);
  if (osd) {
     tArea Area = { 0, 0, cOsd::OsdWidth() - 1, cOsd::OsdHeight() - 1,  32 };
     if (osd->SetAreas(&Area, 1) == oeOk) {
        osd->DrawRectangle(0, 0, osd->Width() -1 , osd->Height() - 1, clrGray50);
        osd->Flush();
        Start();
        }
     }
}

eOSState cTrueColorDemo::ProcessKey(eKeys Key)
{
  eOSState state = cOsdObject::ProcessKey(Key);
  if (state == osUnknown) {
     LOCK_PIXMAPS;
     LOCK_THREAD;
     const int d = 80;
     switch (Key & ~k_Repeat) {
       case kUp:     cursor.SetY(max(cursorLimits.Top(),    cursor.Y() - d)); clockwise = false; break;
       case kDown:   cursor.SetY(min(cursorLimits.Bottom(), cursor.Y() + d)); clockwise = true; break;
       case kLeft:   cursor.SetX(max(cursorLimits.Left(),   cursor.X() - d)); clockwise = false; break;
       case kRight:  cursor.SetX(min(cursorLimits.Right(),  cursor.X() + d)); clockwise = true; break;
       case kRed:    if (destroyablePixmap) {
                        osd->DestroyPixmap(destroyablePixmap);
                        destroyablePixmap = NULL;
                        }
                     break;
       case kGreen:  if (toggleablePixmap)
                        toggleablePixmap->SetLayer(-toggleablePixmap->Layer());
                     break;
       case kOk:     return osEnd;
       default: return state;
       }
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
  if (cOsdProvider::SupportsTrueColor())
     return new cTrueColorDemo;
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
