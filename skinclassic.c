/*
 * skinclassic.c: The 'classic' VDR skin
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinclassic.c 1.12 2005/05/16 10:45:07 kls Exp $
 */

#include "skinclassic.h"
#include "font.h"
#include "i18n.h"
#include "osd.h"
#include "themes.h"

#define ScrollWidth  5

static cTheme Theme;

THEME_CLR(Theme, clrBackground,             clrGray50);
THEME_CLR(Theme, clrButtonRedFg,            clrWhite);
THEME_CLR(Theme, clrButtonRedBg,            clrRed);
THEME_CLR(Theme, clrButtonGreenFg,          clrBlack);
THEME_CLR(Theme, clrButtonGreenBg,          clrGreen);
THEME_CLR(Theme, clrButtonYellowFg,         clrBlack);
THEME_CLR(Theme, clrButtonYellowBg,         clrYellow);
THEME_CLR(Theme, clrButtonBlueFg,           clrWhite);
THEME_CLR(Theme, clrButtonBlueBg,           clrBlue);
THEME_CLR(Theme, clrMessageStatusFg,        clrBlack);
THEME_CLR(Theme, clrMessageStatusBg,        clrCyan);
THEME_CLR(Theme, clrMessageInfoFg,          clrBlack);
THEME_CLR(Theme, clrMessageInfoBg,          clrGreen);
THEME_CLR(Theme, clrMessageWarningFg,       clrBlack);
THEME_CLR(Theme, clrMessageWarningBg,       clrYellow);
THEME_CLR(Theme, clrMessageErrorFg,         clrWhite);
THEME_CLR(Theme, clrMessageErrorBg,         clrRed);
THEME_CLR(Theme, clrVolumePrompt,           clrGreen);
THEME_CLR(Theme, clrVolumeBarUpper,         clrWhite);
THEME_CLR(Theme, clrVolumeBarLower,         clrGreen);
THEME_CLR(Theme, clrChannelName,            clrWhite);
THEME_CLR(Theme, clrChannelDate,            clrWhite);
THEME_CLR(Theme, clrChannelEpgTimeFg,       clrWhite);
THEME_CLR(Theme, clrChannelEpgTimeBg,       clrRed);
THEME_CLR(Theme, clrChannelEpgTitle,        clrCyan);
THEME_CLR(Theme, clrChannelEpgShortText,    clrYellow);
THEME_CLR(Theme, clrMenuTitleFg,            clrBlack);
THEME_CLR(Theme, clrMenuTitleBg,            clrCyan);
THEME_CLR(Theme, clrMenuDate,               clrBlack);
THEME_CLR(Theme, clrMenuItemCurrentFg,      clrBlack);
THEME_CLR(Theme, clrMenuItemCurrentBg,      clrCyan);
THEME_CLR(Theme, clrMenuItemSelectable,     clrWhite);
THEME_CLR(Theme, clrMenuItemNonSelectable,  clrCyan);
THEME_CLR(Theme, clrMenuEventTime,          clrWhite);
THEME_CLR(Theme, clrMenuEventVpsFg,         clrBlack);
THEME_CLR(Theme, clrMenuEventVpsBg,         clrWhite);
THEME_CLR(Theme, clrMenuEventTitle,         clrCyan);
THEME_CLR(Theme, clrMenuEventShortText,     clrWhite);
THEME_CLR(Theme, clrMenuEventDescription,   clrCyan);
THEME_CLR(Theme, clrMenuScrollbarTotal,     clrWhite);
THEME_CLR(Theme, clrMenuScrollbarShown,     clrCyan);
THEME_CLR(Theme, clrMenuText,               clrWhite);
THEME_CLR(Theme, clrReplayTitle,            clrWhite);
THEME_CLR(Theme, clrReplayCurrent,          clrWhite);
THEME_CLR(Theme, clrReplayTotal,            clrWhite);
THEME_CLR(Theme, clrReplayModeJump,         clrWhite);
THEME_CLR(Theme, clrReplayProgressSeen,     clrGreen);
THEME_CLR(Theme, clrReplayProgressRest,     clrWhite);
THEME_CLR(Theme, clrReplayProgressSelected, clrRed);
THEME_CLR(Theme, clrReplayProgressMark,     clrBlack);
THEME_CLR(Theme, clrReplayProgressCurrent,  clrRed);

// --- cSkinClassicDisplayChannel --------------------------------------------

class cSkinClassicDisplayChannel : public cSkinDisplayChannel {
private:
  cOsd *osd;
  int lineHeight;
  int timeWidth;
  bool message;
public:
  cSkinClassicDisplayChannel(bool WithInfo);
  virtual ~cSkinClassicDisplayChannel();
  virtual void SetChannel(const cChannel *Channel, int Number);
  virtual void SetEvents(const cEvent *Present, const cEvent *Following);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinClassicDisplayChannel::cSkinClassicDisplayChannel(bool WithInfo)
{
  int Lines = WithInfo ? 5 : 1;
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  message = false;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + (Setup.ChannelInfoPos ? 0 : Setup.OSDHeight - Lines * lineHeight));
  timeWidth = font->Width("00:00") + 4;
  tArea Areas[] = { { 0, 0, Setup.OSDWidth - 1, Lines * lineHeight - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawRectangle(0, 0, osd->Width() - 1, osd->Height() - 1, Theme.Color(clrBackground));
}

cSkinClassicDisplayChannel::~cSkinClassicDisplayChannel()
{
  delete osd;
}

void cSkinClassicDisplayChannel::SetChannel(const cChannel *Channel, int Number)
{
  osd->DrawRectangle(0, 0, osd->Width() - 1, lineHeight - 1, Theme.Color(clrBackground));
  osd->DrawText(2, 0, ChannelString(Channel, Number), Theme.Color(clrChannelName), Theme.Color(clrBackground), cFont::GetFont(fontOsd));
}

void cSkinClassicDisplayChannel::SetEvents(const cEvent *Present, const cEvent *Following)
{
  osd->DrawRectangle(0, lineHeight, timeWidth - 1, osd->Height(), Theme.Color(clrChannelEpgTimeBg));
  osd->DrawRectangle(timeWidth, lineHeight, osd->Width() - 1, osd->Height(), Theme.Color(clrBackground));
  for (int i = 0; i < 2; i++) {
      const cEvent *e = !i ? Present : Following;
      if (e) {
         osd->DrawText(             2, (2 * i + 1) * lineHeight, e->GetTimeString(), Theme.Color(clrChannelEpgTimeFg), Theme.Color(clrChannelEpgTimeBg), cFont::GetFont(fontOsd));
         osd->DrawText(timeWidth + 10, (2 * i + 1) * lineHeight, e->Title(), Theme.Color(clrChannelEpgTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd));
         osd->DrawText(timeWidth + 10, (2 * i + 2) * lineHeight, e->ShortText(), Theme.Color(clrChannelEpgShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml));
         }
      }
}

void cSkinClassicDisplayChannel::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  if (Text) {
     osd->SaveRegion(0, 0, osd->Width() - 1, lineHeight - 1);
     osd->DrawText(0, 0, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, osd->Width(), 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinClassicDisplayChannel::Flush(void)
{
  if (!message) {
     cString date = DayDateTime();
     osd->DrawText(osd->Width() - cFont::GetFont(fontSml)->Width(date) - 2, 0, date, Theme.Color(clrChannelDate), Theme.Color(clrBackground), cFont::GetFont(fontSml));
     }
  osd->Flush();
}

// --- cSkinClassicDisplayMenu -----------------------------------------------

class cSkinClassicDisplayMenu : public cSkinDisplayMenu {
private:
  cOsd *osd;
  int x0, x1;
  int y0, y1, y2, y3, y4, y5;
  int lineHeight;
  void SetScrollbar(void);
public:
  cSkinClassicDisplayMenu(void);
  virtual ~cSkinClassicDisplayMenu();
  virtual void Scroll(bool Up, bool Page);
  virtual int MaxItems(void);
  virtual void Clear(void);
  virtual void SetTitle(const char *Title);
  virtual void SetButtons(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void SetItem(const char *Text, int Index, bool Current, bool Selectable);
  virtual void SetEvent(const cEvent *Event);
  virtual void SetRecording(const cRecording *Recording);
  virtual void SetText(const char *Text, bool FixedFont);
  virtual void Flush(void);
  };

cSkinClassicDisplayMenu::cSkinClassicDisplayMenu(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  x0 = 0;
  x1 = Setup.OSDWidth;
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + lineHeight;
  y5 = Setup.OSDHeight;
  y4 = y5 - lineHeight;
  y3 = y4 - lineHeight;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop);
  tArea Areas[] = { { x0, y0, x1 - 1, y5 - 1, 4 } };
  if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x1 - 1, y1 - 1, 2 },
                       { x0, y1, x1 - 1, y3 - 1, 2 },
                       { x0, y3, x1 - 1, y5 - 1, 4 }
                     };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
  osd->DrawRectangle(x0, y0, x1 - 1, y5 - 1, Theme.Color(clrBackground));
}

cSkinClassicDisplayMenu::~cSkinClassicDisplayMenu()
{
  delete osd;
}

void cSkinClassicDisplayMenu::SetScrollbar(void)
{
  if (textScroller.CanScroll()) {
     int yt = textScroller.Top();
     int yb = yt + textScroller.Height();
     int st = yt;
     int sb = yb;
     int tt = st + (sb - st) * textScroller.Offset() / textScroller.Total();
     int tb = tt + (sb - st) * textScroller.Shown() / textScroller.Total();
     int xl = x1 - ScrollWidth;
     osd->DrawRectangle(xl, st, x1 - 1, sb, Theme.Color(clrMenuScrollbarTotal));
     osd->DrawRectangle(xl, tt, x1 - 1, tb, Theme.Color(clrMenuScrollbarShown));
     }
}

void cSkinClassicDisplayMenu::Scroll(bool Up, bool Page)
{
  cSkinDisplayMenu::Scroll(Up, Page);
  SetScrollbar();
}

int cSkinClassicDisplayMenu::MaxItems(void)
{
  return (y3 - y2) / lineHeight;
}

void cSkinClassicDisplayMenu::Clear(void)
{
  textScroller.Reset();
  osd->DrawRectangle(x0, y1, x1 - 1, y4 - 1, Theme.Color(clrBackground));
}

void cSkinClassicDisplayMenu::SetTitle(const char *Title)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(x0, y0, Title, Theme.Color(clrMenuTitleFg), Theme.Color(clrMenuTitleBg), font, x1 - x0);
}

void cSkinClassicDisplayMenu::SetButtons(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = x1 - x0;
  int t0 = x0;
  int t1 = x0 + w / 4;
  int t2 = x0 + w / 2;
  int t3 = x1 - w / 4;
  int t4 = x1;
  osd->DrawText(t0, y4, Red,    Theme.Color(clrButtonRedFg),    Red    ? Theme.Color(clrButtonRedBg)    : Theme.Color(clrBackground), font, t1 - t0, 0, taCenter);
  osd->DrawText(t1, y4, Green,  Theme.Color(clrButtonGreenFg),  Green  ? Theme.Color(clrButtonGreenBg)  : Theme.Color(clrBackground), font, t2 - t1, 0, taCenter);
  osd->DrawText(t2, y4, Yellow, Theme.Color(clrButtonYellowFg), Yellow ? Theme.Color(clrButtonYellowBg) : Theme.Color(clrBackground), font, t3 - t2, 0, taCenter);
  osd->DrawText(t3, y4, Blue,   Theme.Color(clrButtonBlueFg),   Blue   ? Theme.Color(clrButtonBlueBg)   : Theme.Color(clrBackground), font, t4 - t3, 0, taCenter);
}

void cSkinClassicDisplayMenu::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  if (Text)
     osd->DrawText(x0, y3, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x1 - x0, 0, taCenter);
  else
     osd->DrawRectangle(x0, y3, x1 - 1, y4 - 1, Theme.Color(clrBackground));
}

void cSkinClassicDisplayMenu::SetItem(const char *Text, int Index, bool Current, bool Selectable)
{
  int y = y2 + Index * lineHeight;
  tColor ColorFg, ColorBg;
  if (Current) {
     ColorFg = Theme.Color(clrMenuItemCurrentFg);
     ColorBg = Theme.Color(clrMenuItemCurrentBg);
     }
  else {
     ColorFg = Theme.Color(Selectable ? clrMenuItemSelectable : clrMenuItemNonSelectable);
     ColorBg = Theme.Color(clrBackground);
     }
  const cFont *font = cFont::GetFont(fontOsd);
  for (int i = 0; i < MaxTabs; i++) {
      const char *s = GetTabbedText(Text, i);
      if (s) {
         int xt = x0 + Tab(i);
         osd->DrawText(xt, y, s, ColorFg, ColorBg, font, x1 - xt);
         }
      if (!Tab(i + 1))
         break;
      }
  SetEditableWidth(x1 - x0 - Tab(1));
}

void cSkinClassicDisplayMenu::SetEvent(const cEvent *Event)
{
  if (!Event)
     return;
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = x0 + 10;
  int y = y2;
  cTextScroller ts;
  char t[32];
  snprintf(t, sizeof(t), "%s  %s - %s", *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString());
  ts.Set(osd, xl, y, x1 - xl, y3 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  if (Event->Vps() && Event->Vps() != Event->StartTime()) {
     char *buffer;
     asprintf(&buffer, " VPS: %s", *Event->GetVpsString());
     const cFont *font = cFont::GetFont(fontSml);
     osd->DrawText(x1 - font->Width(buffer), y, buffer, Theme.Color(clrMenuEventVpsFg), Theme.Color(clrMenuEventVpsBg), font);
     free(buffer);
     }
  y += ts.Height();
  y += font->Height();
  ts.Set(osd, xl, y, x1 - xl, y3 - y, Event->Title(), font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Event->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, x1 - xl, y3 - y, Event->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Event->Description())) {
     textScroller.Set(osd, xl, y, x1 - xl - 2 * ScrollWidth, y3 - y, Event->Description(), font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     SetScrollbar();
     }
}

void cSkinClassicDisplayMenu::SetRecording(const cRecording *Recording)
{
  if (!Recording)
     return;
  const cRecordingInfo *Info = Recording->Info();
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = x0 + 10;
  int y = y2;
  cTextScroller ts;
  char t[32];
  snprintf(t, sizeof(t), "%s  %s", *DateString(Recording->start), *TimeString(Recording->start));
  ts.Set(osd, xl, y, x1 - xl, y3 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  y += ts.Height();
  y += font->Height();
  const char *Title = Info->Title();
  if (isempty(Title))
     Title = Recording->Name();
  ts.Set(osd, xl, y, x1 - xl, y3 - y, Title, font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Info->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, x1 - xl, y3 - y, Info->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Info->Description())) {
     textScroller.Set(osd, xl, y, x1 - xl - 2 * ScrollWidth, y3 - y, Info->Description(), font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     SetScrollbar();
     }
}

void cSkinClassicDisplayMenu::SetText(const char *Text, bool FixedFont)
{
  const cFont *font = cFont::GetFont(FixedFont ? fontFix : fontOsd);
  textScroller.Set(osd, x0, y2, x1 - x0 - 2 * ScrollWidth, y3 - y2, Text, font, Theme.Color(clrMenuText), Theme.Color(clrBackground));
  SetScrollbar();
}

void cSkinClassicDisplayMenu::Flush(void)
{
  cString date = DayDateTime();
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(x1 - font->Width(date) - 2, y0, date, Theme.Color(clrMenuDate), Theme.Color(clrMenuTitleBg), font);
  osd->Flush();
}

// --- cSkinClassicDisplayReplay ---------------------------------------------

class cSkinClassicDisplayReplay : public cSkinDisplayReplay {
private:
  cOsd *osd;
  int x0, x1;
  int y0, y1, y2, y3;
  int lastCurrentWidth;
  bool message;
public:
  cSkinClassicDisplayReplay(bool ModeOnly);
  virtual ~cSkinClassicDisplayReplay();
  virtual void SetTitle(const char *Title);
  virtual void SetMode(bool Play, bool Forward, int Speed);
  virtual void SetProgress(int Current, int Total);
  virtual void SetCurrent(const char *Current);
  virtual void SetTotal(const char *Total);
  virtual void SetJump(const char *Jump);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinClassicDisplayReplay::cSkinClassicDisplayReplay(bool ModeOnly)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  lastCurrentWidth = 0;
  message = false;
  x0 = 0;
  x1 = Setup.OSDWidth;
  y0 = 0;
  y1 = lineHeight;
  y2 = 2 * lineHeight;
  y3 = 3 * lineHeight;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y3);
  tArea Areas[] = { { x0, y0, x1 - 1, y3 - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawRectangle(x0, y0, x1 - 1, y3 - 1, ModeOnly ? clrTransparent : Theme.Color(clrBackground));
}

cSkinClassicDisplayReplay::~cSkinClassicDisplayReplay()
{
  delete osd;
}

void cSkinClassicDisplayReplay::SetTitle(const char *Title)
{
  osd->DrawText(x0, y0, Title, Theme.Color(clrReplayTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd), x1 - x0);
}

void cSkinClassicDisplayReplay::SetMode(bool Play, bool Forward, int Speed)
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

void cSkinClassicDisplayReplay::SetProgress(int Current, int Total)
{
  cProgressBar pb(x1 - x0, y2 - y1, Current, Total, marks, Theme.Color(clrReplayProgressSeen), Theme.Color(clrReplayProgressRest), Theme.Color(clrReplayProgressSelected), Theme.Color(clrReplayProgressMark), Theme.Color(clrReplayProgressCurrent));
  osd->DrawBitmap(x0, y1, pb);
}

void cSkinClassicDisplayReplay::SetCurrent(const char *Current)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = font->Width(Current);
  osd->DrawText(x0, y2, Current, Theme.Color(clrReplayCurrent), Theme.Color(clrBackground), font, lastCurrentWidth > w ? lastCurrentWidth : 0);
  lastCurrentWidth = w;
}

void cSkinClassicDisplayReplay::SetTotal(const char *Total)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(x1 - font->Width(Total), y2, Total, Theme.Color(clrReplayTotal), Theme.Color(clrBackground), font);
}

void cSkinClassicDisplayReplay::SetJump(const char *Jump)
{
  osd->DrawText(x0 + (x1 - x0) / 4, y2, Jump, Theme.Color(clrReplayModeJump), Theme.Color(clrBackground), cFont::GetFont(fontOsd), (x1 - x0) / 2, 0, taCenter);
}

void cSkinClassicDisplayReplay::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  if (Text) {
     osd->SaveRegion(x0, y2, x1 - 1, y3 - 1);
     osd->DrawText(x0, y2, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x1 - x0, y3 - y2, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinClassicDisplayReplay::Flush(void)
{
  osd->Flush();
}

// --- cSkinClassicDisplayVolume ---------------------------------------------

class cSkinClassicDisplayVolume : public cSkinDisplayVolume {
private:
  cOsd *osd;
public:
  cSkinClassicDisplayVolume(void);
  virtual ~cSkinClassicDisplayVolume();
  virtual void SetVolume(int Current, int Total, bool Mute);
  virtual void Flush(void);
  };

cSkinClassicDisplayVolume::cSkinClassicDisplayVolume(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - lineHeight);
  tArea Areas[] = { { 0, 0, Setup.OSDWidth - 1, lineHeight - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
}

cSkinClassicDisplayVolume::~cSkinClassicDisplayVolume()
{
  delete osd;
}

void cSkinClassicDisplayVolume::SetVolume(int Current, int Total, bool Mute)
{
  const cFont *font = cFont::GetFont(fontOsd);
  if (Mute) {
     osd->DrawRectangle(0, 0, osd->Width() - 1, osd->Height() - 1, clrTransparent);
     osd->DrawText(0, 0, tr("Mute"), Theme.Color(clrVolumePrompt), Theme.Color(clrBackground), font);
     }
  else {
     const char *Prompt = tr("Volume ");
     int l = font->Width(Prompt);
     int p = (osd->Width() - l) * Current / Total;
     osd->DrawText(0, 0, Prompt, Theme.Color(clrVolumePrompt), Theme.Color(clrBackground), font);
     osd->DrawRectangle(l, 0, l + p - 1, osd->Height() - 1, Theme.Color(clrVolumeBarLower));
     osd->DrawRectangle(l + p, 0, osd->Width() - 1, osd->Height() - 1, Theme.Color(clrVolumeBarUpper));
     }
}

void cSkinClassicDisplayVolume::Flush(void)
{
  osd->Flush();
}

// --- cSkinClassicDisplayTracks ---------------------------------------------

class cSkinClassicDisplayTracks : public cSkinDisplayTracks {
private:
  cOsd *osd;
  int x0, x1;
  int y0, y1, y2;
  int lineHeight;
  int currentIndex;
  void SetItem(const char *Text, int Index, bool Current);
public:
  cSkinClassicDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks);
  virtual ~cSkinClassicDisplayTracks();
  virtual void SetTrack(int Index, const char * const *Tracks);
  virtual void SetAudioChannel(int AudioChannel) {}
  virtual void Flush(void);
  };

cSkinClassicDisplayTracks::cSkinClassicDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  currentIndex = -1;
  int ItemsWidth = font->Width(Title);
  for (int i = 0; i < NumTracks; i++)
      ItemsWidth = max(ItemsWidth, font->Width(Tracks[i]));
  ItemsWidth += 10;
  x0 = 0;
  x1 = Setup.OSDWidth;
  int d = x1 - x0;
  if (d > ItemsWidth) {
     d = (d - ItemsWidth) & ~0x07; // must be multiple of 8
     x1 -= d;
     }
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + NumTracks * lineHeight;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y2);
  tArea Areas[] = { { x0, y0, x1 - 1, y2 - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawText(x0, y0, Title, Theme.Color(clrMenuTitleFg), Theme.Color(clrMenuTitleBg), font, x1 - x0);
  for (int i = 0; i < NumTracks; i++)
      SetItem(Tracks[i], i, false);
}

cSkinClassicDisplayTracks::~cSkinClassicDisplayTracks()
{
  delete osd;
}

void cSkinClassicDisplayTracks::SetItem(const char *Text, int Index, bool Current)
{
  int y = y1 + Index * lineHeight;
  tColor ColorFg, ColorBg;
  if (Current) {
     ColorFg = Theme.Color(clrMenuItemCurrentFg);
     ColorBg = Theme.Color(clrMenuItemCurrentBg);
     currentIndex = Index;
     }
  else {
     ColorFg = Theme.Color(clrMenuItemSelectable);
     ColorBg = Theme.Color(clrBackground);
     }
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(x0, y, Text, ColorFg, ColorBg, font, x1 - x0);
}

void cSkinClassicDisplayTracks::SetTrack(int Index, const char * const *Tracks)
{
  if (currentIndex >= 0)
     SetItem(Tracks[currentIndex], currentIndex, false);
  SetItem(Tracks[Index], Index, true);
}

void cSkinClassicDisplayTracks::Flush(void)
{
  osd->Flush();
}

// --- cSkinClassicDisplayMessage --------------------------------------------

class cSkinClassicDisplayMessage : public cSkinDisplayMessage {
private:
  cOsd *osd;
public:
  cSkinClassicDisplayMessage(void);
  virtual ~cSkinClassicDisplayMessage();
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinClassicDisplayMessage::cSkinClassicDisplayMessage(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - lineHeight);
  tArea Areas[] = { { 0, 0, Setup.OSDWidth - 1, lineHeight - 1, 2 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
}

cSkinClassicDisplayMessage::~cSkinClassicDisplayMessage()
{
  delete osd;
}

void cSkinClassicDisplayMessage::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(0, 0, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, Setup.OSDWidth, 0, taCenter);
}

void cSkinClassicDisplayMessage::Flush(void)
{
  osd->Flush();
}

// --- cSkinClassic ----------------------------------------------------------

cSkinClassic::cSkinClassic(void)
:cSkin("classic", &::Theme)//XXX naming problem???
{
}

const char *cSkinClassic::Description(void)
{
  return tr("Classic VDR");
}

cSkinDisplayChannel *cSkinClassic::DisplayChannel(bool WithInfo)
{
  return new cSkinClassicDisplayChannel(WithInfo);
}

cSkinDisplayMenu *cSkinClassic::DisplayMenu(void)
{
  return new cSkinClassicDisplayMenu;
}

cSkinDisplayReplay *cSkinClassic::DisplayReplay(bool ModeOnly)
{
  return new cSkinClassicDisplayReplay(ModeOnly);
}

cSkinDisplayVolume *cSkinClassic::DisplayVolume(void)
{
  return new cSkinClassicDisplayVolume;
}


cSkinDisplayTracks *cSkinClassic::DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  return new cSkinClassicDisplayTracks(Title, NumTracks, Tracks);
}

cSkinDisplayMessage *cSkinClassic::DisplayMessage(void)
{
  return new cSkinClassicDisplayMessage;
}
