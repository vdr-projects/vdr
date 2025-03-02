/*
 * skinclassic.c: The 'classic' VDR skin
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinclassic.c 5.5 2025/03/02 11:03:35 kls Exp $
 */

#include "skinclassic.h"
#include "font.h"
#include "i18n.h"
#include "osd.h"
#include "themes.h"
#include "videodir.h"

#define ScrollWidth (Setup.FontOsdSize / 4)
#define TextFrame   (Setup.FontOsdSize / 10)
#define TextSpacing (Setup.FontOsdSize / 4)

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
THEME_CLR(Theme, clrReplayProgressError,    clrBlack);

// --- cSkinClassicDisplayChannel --------------------------------------------

class cSkinClassicDisplayChannel : public cSkinDisplayChannel {
private:
  cOsd *osd;
  int lineHeight;
  int timeWidth;
  bool message;
  cString lastDate;
public:
  cSkinClassicDisplayChannel(bool WithInfo);
  virtual ~cSkinClassicDisplayChannel() override;
  virtual void SetChannel(const cChannel *Channel, int Number) override;
  virtual void SetEvents(const cEvent *Present, const cEvent *Following) override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinClassicDisplayChannel::cSkinClassicDisplayChannel(bool WithInfo)
{
  int Lines = WithInfo ? 5 : 1;
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  message = false;
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + (Setup.ChannelInfoPos ? 0 : cOsd::OsdHeight() - Lines * lineHeight));
  timeWidth = font->Width("00:00") + 2 * TextFrame;
  tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, Lines * lineHeight - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, Lines * lineHeight - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
  osd->DrawRectangle(0, 0, osd->Width() - 1, osd->Height() - 1, Theme.Color(clrBackground));
}

cSkinClassicDisplayChannel::~cSkinClassicDisplayChannel()
{
  delete osd;
}

void cSkinClassicDisplayChannel::SetChannel(const cChannel *Channel, int Number)
{
  osd->DrawRectangle(0, 0, osd->Width() - 1, lineHeight - 1, Theme.Color(clrBackground));
  osd->DrawText(TextFrame, 0, ChannelString(Channel, Number), Theme.Color(clrChannelName), Theme.Color(clrBackground), cFont::GetFont(fontOsd));
  lastDate = NULL;
}

void cSkinClassicDisplayChannel::SetEvents(const cEvent *Present, const cEvent *Following)
{
  osd->DrawRectangle(0, lineHeight, timeWidth - 1, osd->Height(), Theme.Color(clrChannelEpgTimeBg));
  osd->DrawRectangle(timeWidth, lineHeight, osd->Width() - 1, osd->Height(), Theme.Color(clrBackground));
  for (int i = 0; i < 2; i++) {
      const cEvent *e = !i ? Present : Following;
      if (e) {
         osd->DrawText(                  TextFrame, (2 * i + 1) * lineHeight, e->GetTimeString(), Theme.Color(clrChannelEpgTimeFg), Theme.Color(clrChannelEpgTimeBg), cFont::GetFont(fontOsd));
         osd->DrawText(timeWidth + 2 * TextSpacing, (2 * i + 1) * lineHeight, e->Title(), Theme.Color(clrChannelEpgTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd));
         osd->DrawText(timeWidth + 2 * TextSpacing, (2 * i + 2) * lineHeight, e->ShortText(), Theme.Color(clrChannelEpgShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml));
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
     if (!*lastDate || strcmp(date, lastDate)) {
        const cFont *font = cFont::GetFont(fontSml);
        int w = font->Width(date);
        osd->DrawText(osd->Width() - w - TextFrame, 0, date, Theme.Color(clrChannelDate), Theme.Color(clrBackground), cFont::GetFont(fontSml), w);
        lastDate = date;
        }
     }
  osd->Flush();
}

// --- cSkinClassicDisplayMenu -----------------------------------------------

class cSkinClassicDisplayMenu : public cSkinDisplayMenu {
private:
  cOsd *osd;
  int x0, x1, x2, x3;
  int y0, y1, y2, y3, y4, y5;
  int lineHeight;
  int dateWidth;
  cString title;
  cString lastDate;
  int lastDiskUsageState;
  void DrawTitle(void);
  void DrawScrollbar(int Total, int Offset, int Shown, int Top, int Height, bool CanScrollUp, bool CanScrollDown);
  void SetTextScrollbar(void);
public:
  cSkinClassicDisplayMenu(void);
  virtual ~cSkinClassicDisplayMenu() override;
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
  virtual int GetTextAreaWidth(void) const;
  virtual const cFont *GetTextAreaFont(bool FixedFont) const;
  virtual void Flush(void) override;
  };

cSkinClassicDisplayMenu::cSkinClassicDisplayMenu(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  lastDiskUsageState = -1;
  dateWidth = 0;
  x0 = 0;
  x1 = x0 + 2 * TextSpacing;
  x3 = cOsd::OsdWidth();
  x2 = x3 - 2 * ScrollWidth;
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + lineHeight;
  y5 = cOsd::OsdHeight();
  y4 = y5 - lineHeight;
  y3 = y4 - lineHeight;
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop());
  tArea Areas[] = { { x0, y0, x3 - 1, y5 - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x3 - 1, y5 - 1, 4 } };
     if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
        osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     else {
        tArea Areas[] = { { x0, y0, x3 - 1, y1 - 1, 2 },
                          { x0, y1, x3 - 1, y3 - 1, 2 },
                          { x0, y3, x3 - 1, y5 - 1, 4 }
                        };
        osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
        }
     }
  osd->DrawRectangle(x0, y0, x3 - 1, y5 - 1, Theme.Color(clrBackground));
}

cSkinClassicDisplayMenu::~cSkinClassicDisplayMenu()
{
  delete osd;
}

void cSkinClassicDisplayMenu::DrawScrollbar(int Total, int Offset, int Shown, int Top, int Height, bool CanScrollUp, bool CanScrollDown)
{
  if (Total > 0 && Total > Shown) {
     int yt = Top;
     int yb = yt + Height;
     int st = yt;
     int sb = yb;
     int th = max(int((sb - st) * double(Shown) / Total + 0.5), ScrollWidth);
     int tt = min(int(st + (sb - st) * double(Offset) / Total + 0.5), sb - th);
     int tb = min(tt + th, sb);
     int xl = x3 - ScrollWidth;
     osd->DrawRectangle(xl, st, x3 - 1, sb - 1, Theme.Color(clrMenuScrollbarTotal));
     osd->DrawRectangle(xl, tt, x3 - 1, tb - 1, Theme.Color(clrMenuScrollbarShown));
     }
}

void cSkinClassicDisplayMenu::SetTextScrollbar(void)
{
  if (textScroller.CanScroll())
     DrawScrollbar(textScroller.Total(), textScroller.Offset(), textScroller.Shown(), textScroller.Top(), textScroller.Height(), textScroller.CanScrollUp(), textScroller.CanScrollDown());
}

void cSkinClassicDisplayMenu::Scroll(bool Up, bool Page)
{
  cSkinDisplayMenu::Scroll(Up, Page);
  SetTextScrollbar();
}

int cSkinClassicDisplayMenu::MaxItems(void)
{
  return (y3 - y2) / lineHeight;
}

void cSkinClassicDisplayMenu::Clear(void)
{
  textScroller.Reset();
  osd->DrawRectangle(x0, y1, x3 - 1, y4 - 1, Theme.Color(clrBackground));
}

void cSkinClassicDisplayMenu::DrawTitle(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  bool WithDisk = MenuCategory() == mcMain || MenuCategory() == mcRecording;
  osd->DrawText(x0, y0, WithDisk ? cString::sprintf("%s  -  %s", *title, *cVideoDiskUsage::String()) : title, Theme.Color(clrMenuTitleFg), Theme.Color(clrMenuTitleBg), font, x3 - x0 - dateWidth);
}

void cSkinClassicDisplayMenu::SetTitle(const char *Title)
{
  title = Title;
  DrawTitle();
}

void cSkinClassicDisplayMenu::SetButtons(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  const cFont *font = cFont::GetFont(fontOsd);
  const char *lutText[] = { Red, Green, Yellow, Blue };
  tColor lutFg[] = { clrButtonRedFg, clrButtonGreenFg, clrButtonYellowFg, clrButtonBlueFg };
  tColor lutBg[] = { clrButtonRedBg, clrButtonGreenBg, clrButtonYellowBg, clrButtonBlueBg };
  int w = x3 - x0;
  int t0 = x0;
  int t1 = x0 + w / 4;
  int t2 = x0 + w / 2;
  int t3 = x3 - w / 4;
  int t4 = x3;
  osd->DrawText(t0, y4, lutText[Setup.ColorKey0], Theme.Color(lutFg[Setup.ColorKey0]), lutText[Setup.ColorKey0] ? Theme.Color(lutBg[Setup.ColorKey0]) : Theme.Color(clrBackground), font, t1 - t0, 0, taCenter);
  osd->DrawText(t1, y4, lutText[Setup.ColorKey1], Theme.Color(lutFg[Setup.ColorKey1]), lutText[Setup.ColorKey1] ? Theme.Color(lutBg[Setup.ColorKey1]) : Theme.Color(clrBackground), font, t2 - t1, 0, taCenter);
  osd->DrawText(t2, y4, lutText[Setup.ColorKey2], Theme.Color(lutFg[Setup.ColorKey2]), lutText[Setup.ColorKey2] ? Theme.Color(lutBg[Setup.ColorKey2]) : Theme.Color(clrBackground), font, t3 - t2, 0, taCenter);
  osd->DrawText(t3, y4, lutText[Setup.ColorKey3], Theme.Color(lutFg[Setup.ColorKey3]), lutText[Setup.ColorKey3] ? Theme.Color(lutBg[Setup.ColorKey3]) : Theme.Color(clrBackground), font, t4 - t3, 0, taCenter);
}

void cSkinClassicDisplayMenu::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  if (Text)
     osd->DrawText(x0, y3, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x3 - x0, 0, taCenter);
  else
     osd->DrawRectangle(x0, y3, x3 - 1, y4 - 1, Theme.Color(clrBackground));
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
         osd->DrawText(xt, y, s, ColorFg, ColorBg, font, x2 - xt);
         }
      if (!Tab(i + 1))
         break;
      }
  SetEditableWidth(x2 - x0 - Tab(1));
}

void cSkinClassicDisplayMenu::SetScrollbar(int Total, int Offset)
{
  DrawScrollbar(Total, Offset, MaxItems(), y2, MaxItems() * lineHeight, Offset > 0, Offset + MaxItems() < Total);
}

void cSkinClassicDisplayMenu::SetEvent(const cEvent *Event)
{
  if (!Event)
     return;
  const cFont *font = cFont::GetFont(fontOsd);
  int y = y2;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s - %s", *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString());
  ts.Set(osd, x1, y, x2 - x1, y3 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  if (Event->Vps() && Event->Vps() != Event->StartTime()) {
     cString buffer = cString::sprintf(" VPS: %s ", *Event->GetVpsString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(x3 - w, y, buffer, Theme.Color(clrMenuEventVpsFg), Theme.Color(clrMenuEventVpsBg), font, w);
     }
  y += ts.Height();
  if (Event->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Event->GetParentalRatingString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(x3 - w, y, buffer, Theme.Color(clrMenuEventVpsFg), Theme.Color(clrMenuEventVpsBg), font, w);
     }
  y += font->Height();
  ts.Set(osd, x1, y, x2 - x1, y3 - y, Event->Title(), font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Event->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, x1, y, x2 - x1, y3 - y, Event->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Event->Description())) {
     textScroller.Set(osd, x1, y, x2 - x1, y3 - y, Event->Description(), font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     SetTextScrollbar();
     }
}

void cSkinClassicDisplayMenu::SetRecording(const cRecording *Recording)
{
  if (!Recording)
     return;
  const cRecordingInfo *Info = Recording->Info();
  const cFont *font = cFont::GetFont(fontOsd);
  int y = y2;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s  %s", *DateString(Recording->Start()), *TimeString(Recording->Start()), Info->ChannelName() ? Info->ChannelName() : "");
  ts.Set(osd, x1, y, x2 - x1, y3 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  y += ts.Height();
  int xt = x3;
  if (Info->GetEvent()->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Info->GetEvent()->GetParentalRatingString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xt - w, y, buffer, Theme.Color(clrMenuEventVpsFg), Theme.Color(clrMenuEventVpsBg), font, w);
     xt -= w + 5;
     }
  if (Info->Errors() > 0) {
     // TRANSLATORS: note the plural/singular!
     cString buffer = cString::sprintf(" %d %s ", Info->Errors(), Info->Errors() > 1 ? tr("errors") : tr("error"));
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xt - w, y, buffer, Theme.Color(clrMenuEventVpsFg), Theme.Color(clrMenuEventVpsBg), font, w);
     xt -= w + 5;
     }
  y += font->Height();
  const char *Title = Info->Title();
  if (isempty(Title))
     Title = Recording->Name();
  ts.Set(osd, x1, y, x2 - x1, y3 - y, Title, font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Info->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, x1, y, x2 - x1, y3 - y, Info->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Info->Description())) {
     cString d = Info->Description();
     cString f = Info->FrameParams();
     if (*f) {
        d.Append("\n\n");
        d.Append(f);
        }
     textScroller.Set(osd, x1, y, x2 - x1, y3 - y, d, font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     SetTextScrollbar();
     }
}

void cSkinClassicDisplayMenu::SetText(const char *Text, bool FixedFont)
{
  textScroller.Set(osd, x1, y2, GetTextAreaWidth(), y3 - y2, Text, GetTextAreaFont(FixedFont), Theme.Color(clrMenuText), Theme.Color(clrBackground));
  SetTextScrollbar();
}

int cSkinClassicDisplayMenu::GetTextAreaWidth(void) const
{
return x2 - x1;
}

const cFont *cSkinClassicDisplayMenu::GetTextAreaFont(bool FixedFont) const
{
  return cFont::GetFont(FixedFont ? fontFix : fontOsd);
}

void cSkinClassicDisplayMenu::Flush(void)
{
  if (cVideoDiskUsage::HasChanged(lastDiskUsageState))
     DrawTitle();
  cString date = DayDateTime();
  if (!*lastDate || strcmp(date, lastDate)) {
     const cFont *font = cFont::GetFont(fontOsd);
     int w = font->Width(date);
     osd->DrawText(x3 - w - TextFrame, y0, date, Theme.Color(clrMenuDate), Theme.Color(clrMenuTitleBg), font, w);
     lastDate = date;
     dateWidth = max(w + TextFrame, dateWidth);
     }
  osd->Flush();
}

// --- cSkinClassicDisplayReplay ---------------------------------------------

class cSkinClassicDisplayReplay : public cSkinDisplayReplay {
private:
  cOsd *osd;
  int x0, x1;
  int y0, y1, y2, y3;
  int lastCurrentWidth;
public:
  cSkinClassicDisplayReplay(bool ModeOnly);
  virtual ~cSkinClassicDisplayReplay() override;
  virtual void SetTitle(const char *Title) override;
  virtual void SetMode(bool Play, bool Forward, int Speed) override;
  virtual void SetProgress(int Current, int Total) override;
  virtual void SetCurrent(const char *Current) override;
  virtual void SetTotal(const char *Total) override;
  virtual void SetJump(const char *Jump) override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinClassicDisplayReplay::cSkinClassicDisplayReplay(bool ModeOnly)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  lastCurrentWidth = 0;
  x0 = 0;
  x1 = cOsd::OsdWidth();
  y0 = 0;
  y1 = lineHeight;
  y2 = 2 * lineHeight;
  y3 = 3 * lineHeight;
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - y3);
  tArea Areas[] = { { x0, y0, x1 - 1, y3 - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x1 - 1, y3 - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
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
  cProgressBar pb(x1 - x0, y2 - y1, Current, Total, marks, errors, Theme.Color(clrReplayProgressSeen), Theme.Color(clrReplayProgressRest), Theme.Color(clrReplayProgressSelected), Theme.Color(clrReplayProgressMark), Theme.Color(clrReplayProgressCurrent), Theme.Color(clrReplayProgressError));
  osd->DrawBitmap(x0, y1, pb);
}

void cSkinClassicDisplayReplay::SetCurrent(const char *Current)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = font->Width(Current);
  osd->DrawText(x0, y2, Current, Theme.Color(clrReplayCurrent), Theme.Color(clrBackground), font, lastCurrentWidth > w ? lastCurrentWidth : w);
  lastCurrentWidth = w;
}

void cSkinClassicDisplayReplay::SetTotal(const char *Total)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = font->Width(Total);
  osd->DrawText(x1 - font->Width(Total), y2, Total, Theme.Color(clrReplayTotal), Theme.Color(clrBackground), font, w);
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
     }
  else
     osd->RestoreRegion();
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
  virtual ~cSkinClassicDisplayVolume() override;
  virtual void SetVolume(int Current, int Total, bool Mute) override;
  virtual void Flush(void) override;
  };

cSkinClassicDisplayVolume::cSkinClassicDisplayVolume(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - lineHeight);
  tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, lineHeight - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, lineHeight - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
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
     osd->DrawText(0, 0, tr("Key$Mute"), Theme.Color(clrVolumePrompt), Theme.Color(clrBackground), font);
     }
  else {
     // TRANSLATORS: note the trailing blank!
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
  virtual ~cSkinClassicDisplayTracks() override;
  virtual void SetTrack(int Index, const char * const *Tracks) override;
  virtual void SetAudioChannel(int AudioChannel) {}
  virtual void Flush(void) override;
  };

cSkinClassicDisplayTracks::cSkinClassicDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  currentIndex = -1;
  int ItemsWidth = font->Width(Title);
  for (int i = 0; i < NumTracks; i++)
      ItemsWidth = max(ItemsWidth, font->Width(Tracks[i]));
  ItemsWidth += 2 * TextSpacing;
  x0 = 0;
  x1 = cOsd::OsdWidth();
  int d = x1 - x0;
  if (d > ItemsWidth) {
     d = (d - ItemsWidth) & ~0x07; // must be multiple of 8
     x1 -= d;
     }
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + NumTracks * lineHeight;
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - y2);
  tArea Areas[] = { { x0, y0, x1 - 1, y2 - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x1 - 1, y2 - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
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
  virtual ~cSkinClassicDisplayMessage() override;
  virtual void SetMessage(eMessageType Type, const char *Text) override;
  virtual void Flush(void) override;
  };

cSkinClassicDisplayMessage::cSkinClassicDisplayMessage(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  osd = cOsdProvider::NewOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - lineHeight);
  tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, lineHeight - 1, 8 } };
  if (Setup.AntiAlias && osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { 0, 0, cOsd::OsdWidth() - 1, lineHeight - 1, 2 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
}

cSkinClassicDisplayMessage::~cSkinClassicDisplayMessage()
{
  delete osd;
}

void cSkinClassicDisplayMessage::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(0, 0, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, cOsd::OsdWidth(), 0, taCenter);
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
