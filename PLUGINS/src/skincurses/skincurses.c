/*
 * skincurses.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: skincurses.c 5.4 2026/01/21 20:54:47 kls Exp $
 */

#include <ncurses.h>
#include <vdr/osd.h>
#include <vdr/plugin.h>
#include <vdr/skins.h>
#include <vdr/videodir.h>

static const char *VERSION        = "2.6.0";
static const char *DESCRIPTION    = trNOOP("A text only skin");
static const char *MAINMENUENTRY  = NULL;

// --- cCursesFont -----------------------------------------------------------

class cCursesFont : public cFont {
public:
  virtual int Width(void) const override { return 1; }
  virtual int Width(uint c) const override { return 1; }
  virtual int Width(const char *s) const override { return s ? Utf8StrLen(s) : 0; }
  virtual int Height(void) const override { return 1; }
  virtual void DrawText(cBitmap *Bitmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const override {}
  virtual void DrawText(cPixmap *Pixmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const override {}
  };

static const cCursesFont Font = cCursesFont(); // w/o the '= cCursesFont()' gcc 4.6 complains - can anybody explain why this is necessary?

// --- cCursesOsd ------------------------------------------------------------

#define clrBackground   COLOR_BLACK
#define clrTransparent  clrBackground
#define clrBlack        clrBackground
#define clrRed          COLOR_RED
#define clrGreen        COLOR_GREEN
#define clrYellow       COLOR_YELLOW
#define clrBlue         COLOR_BLUE
#define clrMagenta      COLOR_MAGENTA
#define clrCyan         COLOR_CYAN
#define clrWhite        COLOR_WHITE

static int clrMessage[] = {
  clrBlack,
  clrCyan,
  clrBlack,
  clrGreen,
  clrBlack,
  clrYellow,
  clrWhite,
  clrRed
  };

static int ScOsdWidth = 50;
static int ScOsdHeight = 20;

class cCursesOsd : public cOsd {
private:
  WINDOW *savedRegion;
  cVector<int> colorPairs;
  void SetColor(int colorFg, int colorBg = clrBackground);
public:
  cCursesOsd(int Left, int Top);
  virtual ~cCursesOsd() override;
  virtual void SaveRegion(int x1, int y1, int x2, int y2) override;
  virtual void RestoreRegion(void) override;
  virtual void DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width = 0, int Height = 0, int Alignment = taDefault) override;
  virtual void DrawRectangle(int x1, int y1, int x2, int y2, tColor Color) override;
  virtual void Flush(void) override;
  };

cCursesOsd::cCursesOsd(int Left, int Top)
:cOsd(Left, Top, 0)
{
  savedRegion = NULL;

  start_color();
  leaveok(stdscr, true);
  refresh(); // to react on changes to screen size

  int begy, begx;
  int maxy, maxx;
  getmaxyx(stdscr, maxy, maxx);
  getbegyx(stdscr, begy, begx);
  ScOsdWidth  = maxx - begx;
  ScOsdHeight = maxy - begy;
}

cCursesOsd::~cCursesOsd()
{
  erase();
  Flush();
}

void cCursesOsd::SetColor(int colorFg, int colorBg)
{
  int color = (colorBg << 16) | colorFg | 0x80000000;
  int i = colorPairs.IndexOf(color);
  if (i < 0) {
     colorPairs.Append(color);
     i = colorPairs.Size() - 1;
     init_pair(i + 1, colorFg, colorBg);
     }
  attrset(COLOR_PAIR(i + 1));
}

void cCursesOsd::SaveRegion(int x1, int y1, int x2, int y2)
{
  if (savedRegion) {
     delwin(savedRegion);
     savedRegion = NULL;
     }
  savedRegion = newwin(y2 - y1 + 1, x2 - x1 + 1, y1, x1);
  if (savedRegion)
     copywin(stdscr, savedRegion, y1, x1, 0, 0, y2 - y1, x2 - x1, false);
}

void cCursesOsd::RestoreRegion(void)
{
  if (savedRegion) {
     overwrite(savedRegion, stdscr);
     delwin(savedRegion);
     savedRegion = NULL;
     }
}

void cCursesOsd::DrawText(int x, int y, const char *s, tColor ColorFg, tColor ColorBg, const cFont *Font, int Width, int Height, int Alignment)
{
  int w = Font->Width(s);
  int h = Font->Height();
  if (Width || Height) {
     int cw = Width ? Width : w;
     int ch = Height ? Height : h;
     DrawRectangle(x, y, x + cw - 1, y + ch - 1, ColorBg);
     if (Width) {
        if ((Alignment & taLeft) != 0)
           ;
        else if ((Alignment & taRight) != 0) {
           if (w < Width)
              x += Width - w;
           }
        else { // taCentered
           if (w < Width)
              x += (Width - w) / 2;
           }
        }
     if (Height) {
        if ((Alignment & taTop) != 0)
           ;
        else if ((Alignment & taBottom) != 0) {
           if (h < Height)
              y += Height - h;
           }
        else { // taCentered
           if (h < Height)
              y += (Height - h) / 2;
           }
        }
     }
  SetColor(ColorFg, ColorBg);
  mvaddnstr(y, x, s, Width ? Width : ScOsdWidth - x);
}

void cCursesOsd::DrawRectangle(int x1, int y1, int x2, int y2, tColor Color)
{
  SetColor(Color, Color);
  int dx = x2 - x1;
  int dy = y2 - y1;
  if (dx >= dy) {
     for (int y = y1; y <= y2; y++)
         mvhline(y, x1, ' ', dx + 1);
     }
  else {
     for (int x = x1; x <= x2; x++)
         mvvline(y1, x, ' ', dy + 1);
     }
}

void cCursesOsd::Flush(void)
{
  refresh();
}

// --- cSkinCursesDisplayChannel ---------------------------------------------

class cSkinCursesDisplayChannel : public cSkinDisplayChannel {
private:
  cOsd *osd;
  int timeWidth;
  bool message;
public:
  cSkinCursesDisplayChannel(bool WithInfo);
  virtual ~cSkinCursesDisplayChannel() override;
  virtual void SetChannel(const cChannel *Channel, int Number) override;
  virtual void SetEvents(const cEvent *Present, const cEvent *Following) override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinCursesDisplayChannel::cSkinCursesDisplayChannel(bool WithInfo)
{
  int Lines = WithInfo ? 5 : 1;
  message = false;
  osd = new cCursesOsd(0, Setup.ChannelInfoPos ? 0 : ScOsdHeight - Lines);
  timeWidth = strlen("00:00");
  osd->DrawRectangle(0, 0, ScOsdWidth - 1, Lines - 1, clrBackground);
}

cSkinCursesDisplayChannel::~cSkinCursesDisplayChannel()
{
  delete osd;
}

void cSkinCursesDisplayChannel::SetChannel(const cChannel *Channel, int Number)
{
  osd->DrawRectangle(0, 0, ScOsdWidth - 1, 0, clrBackground);
  osd->DrawText(0, 0, ChannelString(Channel, Number), clrWhite, clrBackground, &Font);
}

void cSkinCursesDisplayChannel::SetEvents(const cEvent *Present, const cEvent *Following)
{
  osd->DrawRectangle(0, 1, timeWidth - 1, 4, clrRed);
  osd->DrawRectangle(timeWidth, 1, ScOsdWidth - 1, 4, clrBackground);
  for (int i = 0; i < 2; i++) {
      const cEvent *e = !i ? Present : Following;
      if (e) {
         osd->DrawText(            0, 2 * i + 1, e->GetTimeString(), clrWhite, clrRed, &Font);
         osd->DrawText(timeWidth + 1, 2 * i + 1, e->Title(), clrCyan, clrBackground, &Font);
         osd->DrawText(timeWidth + 1, 2 * i + 2, e->ShortText(), clrYellow, clrBackground, &Font);
         }
      }
}

void cSkinCursesDisplayChannel::SetMessage(eMessageType Type, const char *Text)
{
  if (Text) {
     osd->SaveRegion(0, 0, ScOsdWidth - 1, 0);
     osd->DrawText(0, 0, Text, clrMessage[2 * Type], clrMessage[2 * Type + 1], &Font, ScOsdWidth, 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinCursesDisplayChannel::Flush(void)
{
  if (!message) {
     cString date = DayDateTime();
     osd->DrawText(ScOsdWidth - Utf8StrLen(date), 0, date, clrWhite, clrBackground, &Font);
     }
  osd->Flush();
}

// --- cSkinCursesDisplayMenu ------------------------------------------------

class cSkinCursesDisplayMenu : public cSkinDisplayMenu {
private:
  cOsd *osd;
  cString title;
  int lastDiskUsageState;
  void DrawTitle(void);
  void DrawScrollbar(int Total, int Offset, int Shown, int Top, int Height, bool CanScrollUp, bool CanScrollDown);
  void SetTextScrollbar(void);
public:
  cSkinCursesDisplayMenu(void);
  virtual ~cSkinCursesDisplayMenu() override;
  virtual void Scroll(bool Up, bool Page) override;
  virtual int MaxItems(void) override;
  virtual void Clear(void) override;
  virtual void SetTitle(const char *Title) override;
  virtual void SetButtons(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL) override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void SetItem(const char *Text, int Index, bool Current, bool Selectable) override;
  virtual void SetScrollbar(int Total, int Offset) override;
  virtual void SetEvent(const cEvent *Event) override;
  virtual void SetRecording(const cRecording *Recording) override;
  virtual void SetText(const char *Text, bool FixedFont) override;
  virtual const cFont *GetTextAreaFont(bool FixedFont) const override { return &Font; }
  virtual void Flush(void) override;
  };

cSkinCursesDisplayMenu::cSkinCursesDisplayMenu(void)
{
  osd = new cCursesOsd(0, 0);
  lastDiskUsageState = -1;
  osd->DrawRectangle(0, 0, ScOsdWidth - 1, ScOsdHeight - 1, clrBackground);
}

cSkinCursesDisplayMenu::~cSkinCursesDisplayMenu()
{
  delete osd;
}

void cSkinCursesDisplayMenu::DrawScrollbar(int Total, int Offset, int Shown, int Top, int Height, bool CanScrollUp, bool CanScrollDown)
{
  if (Total > 0 && Total > Shown) {
     int yt = Top;
     int yb = yt + Height;
     int st = yt;
     int sb = yb;
     int th = max(int((sb - st) * double(Shown) / Total + 0.5), 1);
     int tt = min(int(st + (sb - st) * double(Offset) / Total + 0.5), sb - th);
     int tb = min(tt + th, sb);
     int xl = ScOsdWidth - 1;
     osd->DrawRectangle(xl, st, xl, sb - 1, clrWhite);
     osd->DrawRectangle(xl, tt, xl, tb - 1, clrCyan);
     }
}

void cSkinCursesDisplayMenu::SetTextScrollbar(void)
{
  if (textScroller.CanScroll())
     DrawScrollbar(textScroller.Total(), textScroller.Offset(), textScroller.Shown(), textScroller.Top(), textScroller.Height(), textScroller.CanScrollUp(), textScroller.CanScrollDown());
}

void cSkinCursesDisplayMenu::Scroll(bool Up, bool Page)
{
  cSkinDisplayMenu::Scroll(Up, Page);
  SetTextScrollbar();
}

int cSkinCursesDisplayMenu::MaxItems(void)
{
  return ScOsdHeight - 4;
}

void cSkinCursesDisplayMenu::Clear(void)
{
  osd->DrawRectangle(0, 1, ScOsdWidth - 1, ScOsdHeight - 2, clrBackground);
  textScroller.Reset();
}

void cSkinCursesDisplayMenu::DrawTitle(void)
{
  bool WithDisk = MenuCategory() == mcMain || MenuCategory() == mcRecording;
  osd->DrawText(0, 0, WithDisk ? cString::sprintf("%s  -  %s", *title, *cVideoDiskUsage::String()) : title, clrBlack, clrCyan, &Font, ScOsdWidth);
}

void cSkinCursesDisplayMenu::SetTitle(const char *Title)
{
  title = Title;
  DrawTitle();
}

void cSkinCursesDisplayMenu::SetButtons(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  int w = ScOsdWidth;
  int t0 = 0;
  int t1 = 0 + w / 4;
  int t2 = 0 + w / 2;
  int t3 = w - w / 4;
  int t4 = w;
  int y = ScOsdHeight - 1;
  osd->DrawText(t0, y, Red,    clrWhite, Red    ? clrRed    : clrBackground, &Font, t1 - t0, 0, taCenter);
  osd->DrawText(t1, y, Green,  clrBlack, Green  ? clrGreen  : clrBackground, &Font, t2 - t1, 0, taCenter);
  osd->DrawText(t2, y, Yellow, clrBlack, Yellow ? clrYellow : clrBackground, &Font, t3 - t2, 0, taCenter);
  osd->DrawText(t3, y, Blue,   clrWhite, Blue   ? clrBlue   : clrBackground, &Font, t4 - t3, 0, taCenter);
}

void cSkinCursesDisplayMenu::SetMessage(eMessageType Type, const char *Text)
{
  if (Text)
     osd->DrawText(0, ScOsdHeight - 2, Text, clrMessage[2 * Type], clrMessage[2 * Type + 1], &Font, ScOsdWidth, 0, taCenter);
  else
     osd->DrawRectangle(0, ScOsdHeight - 2, ScOsdWidth - 1, ScOsdHeight - 2, clrBackground);
}

void cSkinCursesDisplayMenu::SetItem(const char *Text, int Index, bool Current, bool Selectable)
{
  int y = 2 + Index;
  int ColorFg, ColorBg;
  if (Current) {
     ColorFg = clrBlack;
     ColorBg = clrCyan;
     }
  else {
     ColorFg = Selectable ? clrWhite : clrCyan;
     ColorBg = clrBackground;
     }
  for (int i = 0; i < MaxTabs; i++) {
      const char *s = GetTabbedText(Text, i);
      if (s) {
         int xt = Tab(i) / AvgCharWidth();// Tab() is in "pixel" - see also skins.c!!!
         osd->DrawText(xt, y, s, ColorFg, ColorBg, &Font, ScOsdWidth - 2 - xt);
         }
      if (!Tab(i + 1))
         break;
      }
  SetEditableWidth(ScOsdWidth - 2 - Tab(1) / AvgCharWidth()); // Tab() is in "pixel" - see also skins.c!!!
}

void cSkinCursesDisplayMenu::SetScrollbar(int Total, int Offset)
{
  DrawScrollbar(Total, Offset, MaxItems(), 2, MaxItems(), Offset > 0, Offset + MaxItems() < Total);
}

void cSkinCursesDisplayMenu::SetEvent(const cEvent *Event)
{
  if (!Event)
     return;
  int y = 2;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s - %s", *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString());
  ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, t, &Font, clrYellow, clrBackground);
  if (Event->Vps() && Event->Vps() != Event->StartTime()) {
     cString buffer = cString::sprintf(" VPS: %s", *Event->GetVpsString());
     osd->DrawText(ScOsdWidth - Utf8StrLen(buffer), y, buffer, clrBlack, clrYellow, &Font);
     }
  y += ts.Height();
  if (Event->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Event->GetParentalRatingString());
     osd->DrawText(ScOsdWidth - Utf8StrLen(buffer), y, buffer, clrBlack, clrYellow, &Font);
     }
  y += 1;
  ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, Event->Title(), &Font, clrCyan, clrBackground);
  y += ts.Height();
  if (!isempty(Event->ShortText())) {
     ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, Event->ShortText(), &Font, clrYellow, clrBackground);
     y += ts.Height();
     }
  for (int i = 0; Event->Contents(i); i++) {
      const char *s = Event->ContentToString(Event->Contents(i));
      if (!isempty(s)) {
         ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, s, &Font, clrYellow, clrBackground);
         y += 1;
         }
      }
  y += 1;
  if (!isempty(Event->Description())) {
     textScroller.Set(osd, 0, y, ScOsdWidth - 2, ScOsdHeight - y - 2, Event->Description(), &Font, clrCyan, clrBackground);
     SetTextScrollbar();
     }
}

void cSkinCursesDisplayMenu::SetRecording(const cRecording *Recording)
{
  if (!Recording)
     return;
  const cRecordingInfo *Info = Recording->Info();
  int y = 2;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s  %s", *DateString(Recording->Start()), *TimeString(Recording->Start()), Info->ChannelName() ? Info->ChannelName() : "");
  ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, t, &Font, clrYellow, clrBackground);
  y += ts.Height();
  int xt = ScOsdWidth;
  if (Info->GetEvent()->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Info->GetEvent()->GetParentalRatingString());
     int w = Utf8StrLen(buffer);
     osd->DrawText(xt - w, y, buffer, clrBlack, clrYellow, &Font);
     xt -= w + 1;
     }
  if (Info->Errors() > 0) {
     cString buffer = cString::sprintf(" %d %s ", Info->Errors(), tr("errors"));
     int w = Utf8StrLen(buffer);
     osd->DrawText(xt - w, y, buffer, clrBlack, clrYellow, &Font);
     xt -= w + 1;
     }
  y += 1;
  const char *Title = Info->Title();
  if (isempty(Title))
     Title = Recording->Name();
  ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, Title, &Font, clrCyan, clrBackground);
  y += ts.Height();
  if (!isempty(Info->ShortText())) {
     ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, Info->ShortText(), &Font, clrYellow, clrBackground);
     y += ts.Height();
     }
  for (int i = 0; Info->GetEvent()->Contents(i); i++) {
      const char *s = Info->GetEvent()->ContentToString(Info->GetEvent()->Contents(i));
      if (!isempty(s)) {
         ts.Set(osd, 0, y, ScOsdWidth, ScOsdHeight - y - 2, s, &Font, clrYellow, clrBackground);
         y += 1;
         }
      }
  y += 1;
  if (!isempty(Info->Description())) {
     textScroller.Set(osd, 0, y, ScOsdWidth - 2, ScOsdHeight - y - 2, Info->Description(), &Font, clrCyan, clrBackground);
     SetTextScrollbar();
     }
}

void cSkinCursesDisplayMenu::SetText(const char *Text, bool FixedFont)
{
  textScroller.Set(osd, 0, 2, ScOsdWidth - 2, ScOsdHeight - 4, Text, &Font, clrWhite, clrBackground);
  SetTextScrollbar();
}

void cSkinCursesDisplayMenu::Flush(void)
{
  if (cVideoDiskUsage::HasChanged(lastDiskUsageState))
     DrawTitle();
  cString date = DayDateTime();
  osd->DrawText(ScOsdWidth - Utf8StrLen(date) - 2, 0, date, clrBlack, clrCyan, &Font);
  osd->Flush();
}

// --- cSkinCursesDisplayReplay ----------------------------------------------

class cSkinCursesDisplayReplay : public cSkinDisplayReplay {
private:
  cOsd *osd;
  bool message;
public:
  cSkinCursesDisplayReplay(bool ModeOnly);
  virtual ~cSkinCursesDisplayReplay() override;
  virtual void SetTitle(const char *Title) override;
  virtual void SetMode(bool Play, bool Forward, int Speed) override;
  virtual void SetProgress(int Current, int Total) override;
  virtual void SetCurrent(const char *Current) override;
  virtual void SetTotal(const char *Total) override;
  virtual void SetJump(const char *Jump) override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinCursesDisplayReplay::cSkinCursesDisplayReplay(bool ModeOnly)
{
  message = false;
  osd = new cCursesOsd(0, ScOsdHeight - 3);
  osd->DrawRectangle(0, 0, ScOsdWidth - 1, 2, ModeOnly ? clrTransparent : clrBackground);
}

cSkinCursesDisplayReplay::~cSkinCursesDisplayReplay()
{
  delete osd;
}

void cSkinCursesDisplayReplay::SetTitle(const char *Title)
{
  osd->DrawText(0, 0, Title, clrWhite, clrBackground, &Font, ScOsdWidth);
}

void cSkinCursesDisplayReplay::SetMode(bool Play, bool Forward, int Speed)
{
  if (Setup.ShowReplayMode) {
     const char *Mode;
     if (Speed == -1) Mode = Play    ? "  >  " : " ||  ";
     else if (Play)   Mode = Forward ? " X>> " : " <<X ";
     else             Mode = Forward ? " X|> " : " <|X ";
     char buf[16];
     strn0cpy(buf, Mode, sizeof(buf));
     char *p = strchr(buf, 'X');
     if (p)
        *p = Speed > 0 ? '1' + Speed - 1 : ' ';
     SetJump(buf);
     }
}

void cSkinCursesDisplayReplay::SetProgress(int Current, int Total)
{
  int p = Total > 0 ? ScOsdWidth * Current / Total : 0;
  osd->DrawRectangle(0, 1, p, 1, clrGreen);
  osd->DrawRectangle(p, 1, ScOsdWidth, 1, clrWhite);
}

void cSkinCursesDisplayReplay::SetCurrent(const char *Current)
{
  osd->DrawText(0, 2, Current, clrWhite, clrBackground, &Font, Utf8StrLen(Current) + 3);
}

void cSkinCursesDisplayReplay::SetTotal(const char *Total)
{
  osd->DrawText(ScOsdWidth - Utf8StrLen(Total), 2, Total, clrWhite, clrBackground, &Font);
}

void cSkinCursesDisplayReplay::SetJump(const char *Jump)
{
  osd->DrawText(ScOsdWidth / 4, 2, Jump, clrWhite, clrBackground, &Font, ScOsdWidth / 2, 0, taCenter);
}

void cSkinCursesDisplayReplay::SetMessage(eMessageType Type, const char *Text)
{
  if (Text) {
     osd->SaveRegion(0, 2, ScOsdWidth - 1, 2);
     osd->DrawText(0, 2, Text, clrMessage[2 * Type], clrMessage[2 * Type + 1], &Font, ScOsdWidth, 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinCursesDisplayReplay::Flush(void)
{
  osd->Flush();
}

// --- cSkinCursesDisplayVolume ----------------------------------------------

class cSkinCursesDisplayVolume : public cSkinDisplayVolume {
private:
  cOsd *osd;
public:
  cSkinCursesDisplayVolume(void);
  virtual ~cSkinCursesDisplayVolume() override;
  virtual void SetVolume(int Current, int Total, bool Mute) override;
  virtual void Flush(void) override;
  };

cSkinCursesDisplayVolume::cSkinCursesDisplayVolume(void)
{
  osd = new cCursesOsd(0, ScOsdHeight - 1);
}

cSkinCursesDisplayVolume::~cSkinCursesDisplayVolume()
{
  delete osd;
}

void cSkinCursesDisplayVolume::SetVolume(int Current, int Total, bool Mute)
{
  if (Mute) {
     osd->DrawRectangle(0, 0, ScOsdWidth - 1, 0, clrTransparent);
     osd->DrawText(0, 0, tr("Key$Mute"), clrGreen, clrBackground, &Font);
     }
  else {
     // TRANSLATORS: note the trailing blank!
     const char *Prompt = tr("Volume ");
     int l = Utf8StrLen(Prompt);
     int p = (ScOsdWidth - l) * Current / Total;
     osd->DrawText(0, 0, Prompt, clrGreen, clrBackground, &Font);
     osd->DrawRectangle(l, 0, l + p - 1, 0, clrGreen);
     osd->DrawRectangle(l + p, 0, ScOsdWidth - 1, 0, clrWhite);
     }
}

void cSkinCursesDisplayVolume::Flush(void)
{
  osd->Flush();
}

// --- cSkinCursesDisplayTracks ----------------------------------------------

class cSkinCursesDisplayTracks : public cSkinDisplayTracks {
private:
  cOsd *osd;
  int itemsWidth;
  int currentIndex;
  void SetItem(const char *Text, int Index, bool Current);
public:
  cSkinCursesDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks);
  virtual ~cSkinCursesDisplayTracks() override;
  virtual void SetTrack(int Index, const char * const *Tracks) override;
  virtual void SetAudioChannel(int AudioChannel) override {}
  virtual void Flush(void) override;
  };

cSkinCursesDisplayTracks::cSkinCursesDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  currentIndex = -1;
  itemsWidth = Font.Width(Title);
  for (int i = 0; i < NumTracks; i++)
      itemsWidth = max(itemsWidth, Font.Width(Tracks[i]));
  itemsWidth = min(itemsWidth, ScOsdWidth);
  osd = new cCursesOsd(0, 0);
  osd->DrawRectangle(0, 0, ScOsdWidth - 1, ScOsdHeight - 1, clrBackground);
  osd->DrawText(0, 0, Title, clrBlack, clrCyan, &Font, itemsWidth);
  for (int i = 0; i < NumTracks; i++)
      SetItem(Tracks[i], i, false);
}

cSkinCursesDisplayTracks::~cSkinCursesDisplayTracks()
{
  delete osd;
}

void cSkinCursesDisplayTracks::SetItem(const char *Text, int Index, bool Current)
{
  int y = 1 + Index;
  int ColorFg, ColorBg;
  if (Current) {
     ColorFg = clrBlack;
     ColorBg = clrCyan;
     currentIndex = Index;
     }
  else {
     ColorFg = clrWhite;
     ColorBg = clrBackground;
     }
  osd->DrawText(0, y, Text, ColorFg, ColorBg, &Font, itemsWidth);
}

void cSkinCursesDisplayTracks::SetTrack(int Index, const char * const *Tracks)
{
  if (currentIndex >= 0)
     SetItem(Tracks[currentIndex], currentIndex, false);
  SetItem(Tracks[Index], Index, true);
}

void cSkinCursesDisplayTracks::Flush(void)
{
  osd->Flush();
}

// --- cSkinCursesDisplayMessage ---------------------------------------------

class cSkinCursesDisplayMessage : public cSkinDisplayMessage {
private:
  cOsd *osd;
public:
  cSkinCursesDisplayMessage(void);
  virtual ~cSkinCursesDisplayMessage() override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinCursesDisplayMessage::cSkinCursesDisplayMessage(void)
{
  osd = new cCursesOsd(0, ScOsdHeight - 1);
}

cSkinCursesDisplayMessage::~cSkinCursesDisplayMessage()
{
  delete osd;
}

void cSkinCursesDisplayMessage::SetMessage(eMessageType Type, const char *Text)
{
  osd->DrawText(0, 0, Text, clrMessage[2 * Type], clrMessage[2 * Type + 1], &Font, ScOsdWidth, 0, taCenter);
}

void cSkinCursesDisplayMessage::Flush(void)
{
  osd->Flush();
}

// --- cSkinCurses -----------------------------------------------------------

class cSkinCurses : public cSkin {
public:
  cSkinCurses(void);
  virtual const char *Description(void) override;
  virtual cSkinDisplayChannel *DisplayChannel(bool WithInfo) override;
  virtual cSkinDisplayMenu *DisplayMenu(void) override;
  virtual cSkinDisplayReplay *DisplayReplay(bool ModeOnly) override;
  virtual cSkinDisplayVolume *DisplayVolume(void) override;
  virtual cSkinDisplayTracks *DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks) override;
  virtual cSkinDisplayMessage *DisplayMessage(void) override;
  };

cSkinCurses::cSkinCurses(void)
:cSkin("curses")
{
}

const char *cSkinCurses::Description(void)
{
  return tr("Text mode");
}

cSkinDisplayChannel *cSkinCurses::DisplayChannel(bool WithInfo)
{
  return new cSkinCursesDisplayChannel(WithInfo);
}

cSkinDisplayMenu *cSkinCurses::DisplayMenu(void)
{
  return new cSkinCursesDisplayMenu;
}

cSkinDisplayReplay *cSkinCurses::DisplayReplay(bool ModeOnly)
{
  return new cSkinCursesDisplayReplay(ModeOnly);
}

cSkinDisplayVolume *cSkinCurses::DisplayVolume(void)
{
  return new cSkinCursesDisplayVolume;
}

cSkinDisplayTracks *cSkinCurses::DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  return new cSkinCursesDisplayTracks(Title, NumTracks, Tracks);
}

cSkinDisplayMessage *cSkinCurses::DisplayMessage(void)
{
  return new cSkinCursesDisplayMessage;
}

// --- cPluginSkinCurses -----------------------------------------------------

class cPluginSkinCurses : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginSkinCurses(void);
  virtual ~cPluginSkinCurses() override;
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void) override;
  virtual bool ProcessArgs(int argc, char *argv[]) override;
  virtual bool Initialize(void) override;
  virtual bool Start(void) override;
  virtual void Housekeeping(void) override;
  virtual const char *MainMenuEntry(void) override { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void) override;
  virtual cMenuSetupPage *SetupMenu(void) override;
  virtual bool SetupParse(const char *Name, const char *Value) override;
  };

cPluginSkinCurses::cPluginSkinCurses(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginSkinCurses::~cPluginSkinCurses()
{
  // Clean up after yourself!
  endwin();
}

const char *cPluginSkinCurses::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return NULL;
}

bool cPluginSkinCurses::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginSkinCurses::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  if (initscr())
     return true;
  return false;
}

bool cPluginSkinCurses::Start(void)
{
  // Start any background activities the plugin shall perform.
  cSkin *Skin = new cSkinCurses;
  // This skin is normally used for debugging, so let's make it the current one:
  Skins.SetCurrent(Skin->Name());
  return true;
}

void cPluginSkinCurses::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginSkinCurses::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  return NULL;
}

cMenuSetupPage *cPluginSkinCurses::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return NULL;
}

bool cPluginSkinCurses::SetupParse(const char *Name, const char *Value)
{
  // Parse your own setup parameters and store their values.
  return false;
}

VDRPLUGINCREATOR(cPluginSkinCurses); // Don't touch this!
