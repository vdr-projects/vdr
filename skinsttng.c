/*
 * skinsttng.c: A VDR skin with ST:TNG Panels
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinsttng.c 1.15 2005/08/15 11:14:59 kls Exp $
 */

// Star Trek: The Next Generation® is a registered trademark of Paramount Pictures
// registered in the United States Patent and Trademark Office.
// No infringement intended.

#include "skinsttng.h"
#include "font.h"
#include "osd.h"
#include "menu.h"
#include "themes.h"

#include "symbols/arrowdown.xpm"
#include "symbols/arrowup.xpm"
#include "symbols/audio.xpm"
#include "symbols/audioleft.xpm"
#include "symbols/audioright.xpm"
#include "symbols/audiostereo.xpm"
#include "symbols/dolbydigital.xpm"
#include "symbols/encrypted.xpm"
#include "symbols/ffwd.xpm"
#include "symbols/ffwd1.xpm"
#include "symbols/ffwd2.xpm"
#include "symbols/ffwd3.xpm"
#include "symbols/frew.xpm"
#include "symbols/frew1.xpm"
#include "symbols/frew2.xpm"
#include "symbols/frew3.xpm"
#include "symbols/mute.xpm"
#include "symbols/pause.xpm"
#include "symbols/play.xpm"
#include "symbols/radio.xpm"
#include "symbols/recording.xpm"
#include "symbols/sfwd.xpm"
#include "symbols/sfwd1.xpm"
#include "symbols/sfwd2.xpm"
#include "symbols/sfwd3.xpm"
#include "symbols/srew.xpm"
#include "symbols/srew1.xpm"
#include "symbols/srew2.xpm"
#include "symbols/srew3.xpm"
#include "symbols/teletext.xpm"
#include "symbols/volume.xpm"

#define Roundness   10
#define Gap          5
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
THEME_CLR(Theme, clrMessageFrame,           clrYellow);
THEME_CLR(Theme, clrMessageStatusFg,        clrBlack);
THEME_CLR(Theme, clrMessageStatusBg,        clrCyan);
THEME_CLR(Theme, clrMessageInfoFg,          clrBlack);
THEME_CLR(Theme, clrMessageInfoBg,          clrGreen);
THEME_CLR(Theme, clrMessageWarningFg,       clrBlack);
THEME_CLR(Theme, clrMessageWarningBg,       clrYellow);
THEME_CLR(Theme, clrMessageErrorFg,         clrWhite);
THEME_CLR(Theme, clrMessageErrorBg,         clrRed);
THEME_CLR(Theme, clrVolumeFrame,            clrYellow);
THEME_CLR(Theme, clrVolumeSymbol,           clrBlack);
THEME_CLR(Theme, clrVolumeBarUpper,         0xFFBC8024);
THEME_CLR(Theme, clrVolumeBarLower,         0xFF248024);
THEME_CLR(Theme, clrChannelFrame,           clrYellow);
THEME_CLR(Theme, clrChannelName,            clrBlack);
THEME_CLR(Theme, clrChannelDate,            clrBlack);
THEME_CLR(Theme, clrChannelSymbolOn,        clrBlack);
THEME_CLR(Theme, clrChannelSymbolOff,       0xFFBC8024);
THEME_CLR(Theme, clrChannelSymbolRecFg,     clrWhite);
THEME_CLR(Theme, clrChannelSymbolRecBg,     clrRed);
THEME_CLR(Theme, clrChannelEpgTime,         clrBlack);
THEME_CLR(Theme, clrChannelEpgTitle,        clrCyan);
THEME_CLR(Theme, clrChannelEpgShortText,    clrYellow);
THEME_CLR(Theme, clrChannelTimebarSeen,     clrYellow);
THEME_CLR(Theme, clrChannelTimebarRest,     clrGray50);
THEME_CLR(Theme, clrMenuFrame,              clrYellow);
THEME_CLR(Theme, clrMenuTitle,              clrBlack);
THEME_CLR(Theme, clrMenuDate,               clrBlack);
THEME_CLR(Theme, clrMenuItemCurrentFg,      clrBlack);
THEME_CLR(Theme, clrMenuItemCurrentBg,      clrYellow);
THEME_CLR(Theme, clrMenuItemSelectable,     clrYellow);
THEME_CLR(Theme, clrMenuItemNonSelectable,  clrCyan);
THEME_CLR(Theme, clrMenuEventTime,          clrYellow);
THEME_CLR(Theme, clrMenuEventVps,           clrBlack);
THEME_CLR(Theme, clrMenuEventTitle,         clrCyan);
THEME_CLR(Theme, clrMenuEventShortText,     clrYellow);
THEME_CLR(Theme, clrMenuEventDescription,   clrCyan);
THEME_CLR(Theme, clrMenuScrollbarTotal,     clrYellow);
THEME_CLR(Theme, clrMenuScrollbarShown,     clrCyan);
THEME_CLR(Theme, clrMenuScrollbarArrow,     clrBlack);
THEME_CLR(Theme, clrMenuText,               clrCyan);
THEME_CLR(Theme, clrReplayFrame,            clrYellow);
THEME_CLR(Theme, clrReplayTitle,            clrBlack);
THEME_CLR(Theme, clrReplayMode,             clrBlack);
THEME_CLR(Theme, clrReplayCurrent,          clrBlack);
THEME_CLR(Theme, clrReplayTotal,            clrBlack);
THEME_CLR(Theme, clrReplayJump,             clrBlack);
THEME_CLR(Theme, clrReplayProgressSeen,     clrGreen);
THEME_CLR(Theme, clrReplayProgressRest,     clrWhite);
THEME_CLR(Theme, clrReplayProgressSelected, clrRed);
THEME_CLR(Theme, clrReplayProgressMark,     clrBlack);
THEME_CLR(Theme, clrReplayProgressCurrent,  clrRed);

// --- cSkinSTTNGDisplayChannel ----------------------------------------------

class cSkinSTTNGDisplayChannel : public cSkinDisplayChannel {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1, y2, y3, y4, y5, y6, y7;
  bool withInfo;
  int lineHeight;
  tColor frameColor;
  bool message;
  const cEvent *present;
  int lastSeen;
  tTrackId lastTrackId;
  static cBitmap bmTeletext, bmRadio, bmAudio, bmDolbyDigital, bmEncrypted, bmRecording;
public:
  cSkinSTTNGDisplayChannel(bool WithInfo);
  virtual ~cSkinSTTNGDisplayChannel();
  virtual void SetChannel(const cChannel *Channel, int Number);
  virtual void SetEvents(const cEvent *Present, const cEvent *Following);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cBitmap cSkinSTTNGDisplayChannel::bmTeletext(teletext_xpm);
cBitmap cSkinSTTNGDisplayChannel::bmRadio(radio_xpm);
cBitmap cSkinSTTNGDisplayChannel::bmAudio(audio_xpm);
cBitmap cSkinSTTNGDisplayChannel::bmDolbyDigital(dolbydigital_xpm);
cBitmap cSkinSTTNGDisplayChannel::bmEncrypted(encrypted_xpm);
cBitmap cSkinSTTNGDisplayChannel::bmRecording(recording_xpm);

cSkinSTTNGDisplayChannel::cSkinSTTNGDisplayChannel(bool WithInfo)
{
  present = NULL;
  lastSeen = -1;
  memset(&lastTrackId, 0, sizeof(lastTrackId));
  const cFont *font = cFont::GetFont(fontOsd);
  withInfo = WithInfo;
  lineHeight = font->Height();
  frameColor = Theme.Color(clrChannelFrame);
  message = false;
  if (withInfo) {
     x0 = 0;
     x1 = x0 + font->Width("00:00") + 4;
     x2 = x1 + Roundness;
     x3 = x2 + Gap;
     x7 = Setup.OSDWidth;
     x6 = x7 - lineHeight / 2;
     x5 = x6 - lineHeight / 2;
     x4 = x5 - Gap;
     y0 = 0;
     y1 = lineHeight;
     y2 = y1 + Roundness;
     y3 = y2 + Gap;
     y4 = y3 + 4 * lineHeight;
     y5 = y4 + Gap;
     y6 = y5 + Roundness;
     y7 = y6 + cFont::GetFont(fontSml)->Height();
     int yt = (y0 + y1) / 2;
     int yb = (y6 + y7) / 2;
     osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + (Setup.ChannelInfoPos ? 0 : Setup.OSDHeight - y7));
     tArea Areas[] = { { 0, 0, x7 - 1, y7 - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     osd->DrawRectangle(x0, y0, x7 - 1, y7 - 1, Theme.Color(clrBackground));
     osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
     osd->DrawRectangle(x0, y6, x1 - 1, y7 - 1, clrTransparent);
     osd->DrawRectangle(x6, y0, x7 - 1, yt - 1, clrTransparent);
     osd->DrawRectangle(x6, yb, x7 - 1, y7 - 1, clrTransparent);
     osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 2);
     osd->DrawRectangle(x1, y0, x4 - 1, y1 - 1, frameColor);
     osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
     osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
     osd->DrawRectangle(x0, y1, x1 - 1, y2 - 1, frameColor);
     osd->DrawEllipse  (x1, y1, x2 - 1, y2 - 1, frameColor, -2);
     osd->DrawRectangle(x0, y3, x1 - 1, y4 - 1, frameColor);
     osd->DrawRectangle(x0, y5, x1 - 1, y6 - 1, frameColor);
     osd->DrawEllipse  (x1, y5, x2 - 1, y6 - 1, frameColor, -3);
     osd->DrawEllipse  (x0, y6, x1 - 1, y7 - 1, frameColor, 3);
     osd->DrawRectangle(x1, y6, x4 - 1, y7 - 1, frameColor);
     osd->DrawRectangle(x5, y6, x6 - 1, y7 - 1, frameColor);
     osd->DrawEllipse  (x6, y6, x7 - 1, y7 - 1, frameColor, 5);
     }
  else {
     x0 = 0;
     x1 = lineHeight / 2;
     x2 = lineHeight;
     x3 = x2 + Gap;
     x7 = Setup.OSDWidth;
     x6 = x7 - lineHeight / 2;
     x5 = x6 - lineHeight / 2;
     x4 = x5 - Gap;
     y0 = 0;
     y1 = lineHeight;
     osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + (Setup.ChannelInfoPos ? 0 : Setup.OSDHeight - y1));
     tArea Areas[] = { { x0, y0, x7 - 1, y1 - 1, 4 } };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     osd->DrawRectangle(x0, y0, x7 - 1, y1 - 1, clrTransparent);
     osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 7);
     osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
     osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
     osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
     }
}

cSkinSTTNGDisplayChannel::~cSkinSTTNGDisplayChannel()
{
  delete osd;
}

void cSkinSTTNGDisplayChannel::SetChannel(const cChannel *Channel, int Number)
{
  osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
  int x = x4 - 5;
  if (Channel && !Channel->GroupSep()) {
     int d = 3;
     bool rec = cRecordControls::Active();
     x -= bmRecording.Width() + d;
     osd->DrawBitmap(x, y0 + (y1 - y0 - bmRecording.Height()) / 2, bmRecording, Theme.Color(rec ? clrChannelSymbolRecFg : clrChannelSymbolOff), rec ? Theme.Color(clrChannelSymbolRecBg) : frameColor);
     x -= bmEncrypted.Width() + d;
     osd->DrawBitmap(x, y0 + (y1 - y0 - bmEncrypted.Height()) / 2, bmEncrypted, Theme.Color(Channel->Ca() ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     x -= bmDolbyDigital.Width() + d;
     osd->DrawBitmap(x, y0 + (y1 - y0 - bmDolbyDigital.Height()) / 2, bmDolbyDigital, Theme.Color(Channel->Dpid(0) ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     x -= bmAudio.Width() + d;
     osd->DrawBitmap(x, y0 + (y1 - y0 - bmAudio.Height()) / 2, bmAudio, Theme.Color(Channel->Apid(1) ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     if (Channel->Vpid()) {
        x -= bmTeletext.Width() + d;
        osd->DrawBitmap(x, y0 + (y1 - y0 - bmTeletext.Height()) / 2, bmTeletext, Theme.Color(Channel->Tpid() ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
        }
     else if (Channel->Apid(0)) {
        x -= bmRadio.Width() + d;
        osd->DrawBitmap(x, y0 + (y1 - y0 - bmRadio.Height()) / 2, bmRadio, Theme.Color(clrChannelSymbolOn), frameColor);
        }
     }
  osd->DrawText(x3 + 2, y0, ChannelString(Channel, Number), Theme.Color(clrChannelName), frameColor, cFont::GetFont(fontOsd), x - x3 - 2);
}

void cSkinSTTNGDisplayChannel::SetEvents(const cEvent *Present, const cEvent *Following)
{
  if (!withInfo)
     return;
  if (present != Present)
     lastSeen = -1;
  present = Present;
  osd->DrawRectangle(x0, y3, x1 - 1, y4 - 1, frameColor);
  osd->DrawRectangle(x3, y3, x7 - 1, y4 - 1, Theme.Color(clrBackground));
  for (int i = 0; i < 2; i++) {
      const cEvent *e = !i ? Present : Following;
      if (e) {
         osd->DrawText(x0 + 2, y3 + 2 * i * lineHeight, e->GetTimeString(), Theme.Color(clrChannelEpgTime), frameColor, cFont::GetFont(fontOsd));
         osd->DrawText(x3 + 2, y3 + 2 * i * lineHeight, e->Title(), Theme.Color(clrChannelEpgTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd), x4 - x3 - 2);
         osd->DrawText(x3 + 2, y3 + (2 * i + 1) * lineHeight, e->ShortText(), Theme.Color(clrChannelEpgShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml), x4 - x3 - 2);
         }
      }
}

void cSkinSTTNGDisplayChannel::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(withInfo ? fontSml : fontOsd);
  if (Text) {
     int yt = withInfo ? y6 : y0;
     int yb = withInfo ? y7 : y1;
     osd->SaveRegion(x2, yt, x4 - 1, yb - 1);
     if (withInfo)
        osd->DrawRectangle(x2, yt, x3 - 1, yb - 1, Theme.Color(clrBackground));
     osd->DrawText(x3, yt, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x4 - x3, 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinSTTNGDisplayChannel::Flush(void)
{
  if (withInfo) {
     if (!message) {
        const cFont *font = cFont::GetFont(fontSml);
        cString date = DayDateTime();
        int w = font->Width(date);
        osd->DrawText(x4 - w - 2, y7 - font->Height(date), date, Theme.Color(clrChannelDate), frameColor, font);
        cDevice *Device = cDevice::PrimaryDevice();
        const tTrackId *Track = Device->GetTrack(Device->GetCurrentAudioTrack());
        if (!Track && *lastTrackId.description || Track && strcmp(lastTrackId.description, Track->description)) {
           osd->DrawText(x3 + 2, y6, Track ? Track->description : "", Theme.Color(clrChannelName), frameColor, font, x4 - x3 - w - 4);
           strn0cpy(lastTrackId.description, Track ? Track->description : "", sizeof(lastTrackId.description));
           }
        }

     int seen = 0;
     if (present) {
        time_t t = time(NULL);
        if (t > present->StartTime())
           seen = min(y4 - y3 - 1, int((y4 - y3) * double(t - present->StartTime()) / present->Duration()));
        }
     if (seen != lastSeen) {
        osd->DrawRectangle(x1 + Gap, y3, x1 + Gap + ScrollWidth - 1, y4 - 1, Theme.Color(clrChannelTimebarRest));
        if (seen)
        osd->DrawRectangle(x1 + Gap, y3, x1 + Gap + ScrollWidth - 1, y3 + seen, Theme.Color(clrChannelTimebarSeen));
        lastSeen = seen;
        }
     }
  osd->Flush();
}

// --- cSkinSTTNGDisplayMenu -------------------------------------------------

class cSkinSTTNGDisplayMenu : public cSkinDisplayMenu {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1, y2, y3, y4, y5, y6, y7;
  int lineHeight;
  tColor frameColor;
  int currentIndex;
  bool message;
  void SetScrollbar(void);
public:
  cSkinSTTNGDisplayMenu(void);
  virtual ~cSkinSTTNGDisplayMenu();
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

cSkinSTTNGDisplayMenu::cSkinSTTNGDisplayMenu(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  frameColor = Theme.Color(clrMenuFrame);
  currentIndex = -1;
  message = false;
  x0 = 0;
  x1 = lineHeight / 2;
  x3 = (x1 + Roundness + Gap + 7) & ~0x07; // must be multiple of 8
  x2 = x3 - Gap;
  x7 = Setup.OSDWidth;
  x6 = x7 - lineHeight / 2;
  x4 = (x6 - lineHeight / 2 - Gap) & ~0x07; // must be multiple of 8
  x5 = x4 + Gap;
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + Roundness;
  y3 = y2 + Gap;
  y7 = Setup.OSDHeight;
  y6 = y7 - cFont::GetFont(fontSml)->Height();
  y5 = y6 - Roundness;
  y4 = y5 - Gap;
  int yt = (y0 + y1) / 2;
  int yb = (y6 + y7) / 2;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop);
  tArea Areas[] = { { x0, y0, x7 - 1, y7 - 1, 4 } };
  if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x7 - 1, y3 - 1, 2 },
                       { x0, y3, x3 - 1, y4 - 1, 1 },
                       { x3, y3, x4 - 1, y4 - 1, 2 },
                       { x4, y3, x7 - 1, y4 - 1, 2 },
                       { x0, y4, x7 - 1, y7 - 1, 4 }
                     };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
  osd->DrawRectangle(x0, y0, x7 - 1, y7 - 1, Theme.Color(clrBackground));
  osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
  osd->DrawRectangle(x0, y6, x1 - 1, y7 - 1, clrTransparent);
  osd->DrawRectangle(x6, y0, x7 - 1, yt - 1, clrTransparent);
  osd->DrawRectangle(x6, yb, x7 - 1, y7 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 2);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
  osd->DrawRectangle(x0, y1, x1 - 1, y6 - 1, frameColor);
  osd->DrawEllipse  (x1, y1, x2 - 1, y2 - 1, frameColor, -2);
  osd->DrawEllipse  (x1, y5, x2 - 1, y6 - 1, frameColor, -3);
  osd->DrawEllipse  (x0, y6, x1 - 1, y7 - 1, frameColor, 3);
  osd->DrawRectangle(x1, y6, x2 - 1, y7 - 1, frameColor);
  osd->DrawRectangle(x3, y6, x4 - 1, y7 - 1, frameColor);
  osd->DrawRectangle(x5, y6, x6 - 1, y7 - 1, frameColor);
  osd->DrawEllipse  (x6, y6, x7 - 1, y7 - 1, frameColor, 5);
}

cSkinSTTNGDisplayMenu::~cSkinSTTNGDisplayMenu()
{
  delete osd;
}

void cSkinSTTNGDisplayMenu::SetScrollbar(void)
{
  if (textScroller.CanScroll()) {
     int h  = lineHeight;
     int yt = textScroller.Top();
     int yb = yt + textScroller.Height();
     int st = yt + h + Gap;
     int sb = yb - h - Gap;
     int tt = st + (sb - st) * textScroller.Offset() / textScroller.Total();
     int tb = tt + (sb - st) * textScroller.Shown() / textScroller.Total();
     osd->DrawRectangle(x5, st, x5 + ScrollWidth - 1, sb, Theme.Color(clrMenuScrollbarTotal));
     osd->DrawRectangle(x5, tt, x5 + ScrollWidth - 1, tb, Theme.Color(clrMenuScrollbarShown));
     osd->DrawRectangle(x5, yt, x6 - 1, yt + h - 1, frameColor);
     osd->DrawEllipse  (x6, yt, x7 - 1, yt + h - 1, frameColor, 5);
     osd->DrawRectangle(x5, yb - h, x6 - 1, yb - 1, frameColor);
     osd->DrawEllipse  (x6, yb - h, x7 - 1, yb - 1, frameColor, 5);
     if (textScroller.CanScrollUp()) {
        cBitmap bm(arrowup_xpm);
        osd->DrawBitmap(x5 + (x7 - x5 - bm.Width()) / 2 - 2, yt + (h - bm.Height()) / 2, bm, Theme.Color(clrMenuScrollbarArrow), frameColor);
        }
     if (textScroller.CanScrollDown()) {
        cBitmap bm(arrowdown_xpm);
        osd->DrawBitmap(x5 + (x7 - x5 - bm.Width()) / 2 - 2, yb - h + (h - bm.Height()) / 2, bm, Theme.Color(clrMenuScrollbarArrow), frameColor);
        }
     }
}

void cSkinSTTNGDisplayMenu::Scroll(bool Up, bool Page)
{
  cSkinDisplayMenu::Scroll(Up, Page);
  SetScrollbar();
}

int cSkinSTTNGDisplayMenu::MaxItems(void)
{
  return (y4 - y3 - 2 * Roundness) / lineHeight;
}

void cSkinSTTNGDisplayMenu::Clear(void)
{
  textScroller.Reset();
  osd->DrawRectangle(x1, y3, x7 - 1, y4 - 1, Theme.Color(clrBackground));
}

void cSkinSTTNGDisplayMenu::SetTitle(const char *Title)
{
  const cFont *font = cFont::GetFont(fontOsd);
  const char *VDR = " VDR";
  int w = font->Width(VDR);
  osd->DrawText(x3 + 5, y0, Title, Theme.Color(clrMenuTitle), frameColor, font, x4 - w - x3 - 5);
  osd->DrawText(x4 - w, y0, VDR, frameColor, clrBlack, font);
}

void cSkinSTTNGDisplayMenu::SetButtons(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  cString date = DayDateTime();
  const cFont *font = cFont::GetFont(fontSml);
  int d = 10;
  int d2 = d / 2;
  int t4 = x4 - font->Width(date) - 2;
  int w = t4 - x3;
  int t0 = x3 + d2;
  int t1 = x3 + w / 4;
  int t2 = x3 + w / 2;
  int t3 = t4 - w / 4;
  osd->DrawRectangle(t0 + d2, y6, t1 - d2, y7 - 1, clrBlack);
  osd->DrawRectangle(t1 + d2, y6, t2 - d2, y7 - 1, clrBlack);
  osd->DrawRectangle(t2 + d2, y6, t3 - d2, y7 - 1, clrBlack);
  osd->DrawRectangle(t3 + d2, y6, t4 - d2, y7 - 1, clrBlack);
  osd->DrawText(t0 + d, y6, Red,    Theme.Color(clrButtonRedFg),    Theme.Color(clrButtonRedBg),    font, t1 - t0 - 2 * d, 0, taCenter);
  osd->DrawText(t1 + d, y6, Green,  Theme.Color(clrButtonGreenFg),  Theme.Color(clrButtonGreenBg),  font, t2 - t1 - 2 * d, 0, taCenter);
  osd->DrawText(t2 + d, y6, Yellow, Theme.Color(clrButtonYellowFg), Theme.Color(clrButtonYellowBg), font, t3 - t2 - 2 * d, 0, taCenter);
  osd->DrawText(t3 + d, y6, Blue,   Theme.Color(clrButtonBlueFg),   Theme.Color(clrButtonBlueBg),   font, t4 - t3 - 2 * d, 0, taCenter);
}

void cSkinSTTNGDisplayMenu::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontSml);
  if (Text) {
     osd->SaveRegion(x3, y6, x4 - 1, y7 - 1);
     osd->DrawText(x3, y6, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x4 - x3, 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinSTTNGDisplayMenu::SetItem(const char *Text, int Index, bool Current, bool Selectable)
{
  int y = y3 + Roundness + Index * lineHeight;
  tColor ColorFg, ColorBg;
  if (Current) {
     ColorFg = Theme.Color(clrMenuItemCurrentFg);
     ColorBg = Theme.Color(clrMenuItemCurrentBg);
     osd->DrawEllipse  (x1, y - Roundness,  x2 - 1, y - 1, frameColor, -3);
     osd->DrawRectangle(x1, y,              x2 - 1, y + lineHeight - 1, frameColor);
     osd->DrawEllipse  (x1, y + lineHeight, x2 - 1, y + lineHeight + Roundness - 1, frameColor, -2);
     osd->DrawRectangle(x3, y,              x4 - 1, y + lineHeight - 1, ColorBg);
     currentIndex = Index;
     }
  else {
     ColorFg = Theme.Color(Selectable ? clrMenuItemSelectable : clrMenuItemNonSelectable);
     ColorBg = Theme.Color(clrBackground);
     if (currentIndex == Index) {
        osd->DrawRectangle(x1, y - Roundness,  x2 - 1, y + lineHeight + Roundness - 1, Theme.Color(clrBackground));
        osd->DrawRectangle(x3, y,              x4 - 1, y + lineHeight - 1, Theme.Color(clrBackground));
        }
     }
  const cFont *font = cFont::GetFont(fontOsd);
  for (int i = 0; i < MaxTabs; i++) {
      const char *s = GetTabbedText(Text, i);
      if (s) {
         int xt = x3 + 5 + Tab(i);
         osd->DrawText(xt, y, s, ColorFg, ColorBg, font, x4 - xt);
         }
      if (!Tab(i + 1))
         break;
      }
  SetEditableWidth(x4 - x3 - 5 - Tab(1));
}

void cSkinSTTNGDisplayMenu::SetEvent(const cEvent *Event)
{
  if (!Event)
     return;
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = x3 + 5;
  int y = y3;
  cTextScroller ts;
  char t[32];
  snprintf(t, sizeof(t), "%s  %s - %s", *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString());
  ts.Set(osd, xl, y, x4 - xl, y4 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  if (Event->Vps() && Event->Vps() != Event->StartTime()) {
     char *buffer;
     asprintf(&buffer, " VPS: %s", *Event->GetVpsString());
     const cFont *font = cFont::GetFont(fontSml);
     osd->DrawText(x4 - font->Width(buffer), y, buffer, Theme.Color(clrMenuEventVps), frameColor, font);
     int yb = y + font->Height();
     osd->DrawRectangle(x5, y, x6 - 1, yb - 1, frameColor);
     osd->DrawEllipse  (x6, y, x7 - 1, yb - 1, frameColor, 5);
     free(buffer);
     }
  y += ts.Height();
  y += font->Height();
  ts.Set(osd, xl, y, x4 - xl, y4 - y, Event->Title(), font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Event->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, x4 - xl, y4 - y, Event->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Event->Description())) {
     int yt = y;
     int yb = y4 - Roundness;
     textScroller.Set(osd, xl, yt, x4 - xl, yb - yt, Event->Description(), font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     yb = yt + textScroller.Height();
     osd->DrawEllipse  (x1, yt - Roundness, x2, yt,             frameColor, -3);
     osd->DrawRectangle(x1, yt,             x2, yb,             frameColor);
     osd->DrawEllipse  (x1, yb,             x2, yb + Roundness, frameColor, -2);
     SetScrollbar();
     }
}

void cSkinSTTNGDisplayMenu::SetRecording(const cRecording *Recording)
{
  if (!Recording)
     return;
  const cRecordingInfo *Info = Recording->Info();
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = x3 + 5;
  int y = y3;
  cTextScroller ts;
  char t[32];
  snprintf(t, sizeof(t), "%s  %s", *DateString(Recording->start), *TimeString(Recording->start));
  ts.Set(osd, xl, y, x4 - xl, y4 - y, t, font, Theme.Color(clrMenuEventTime), Theme.Color(clrBackground));
  y += ts.Height();
  y += font->Height();
  const char *Title = Info->Title();
  if (isempty(Title))
     Title = Recording->Name();
  ts.Set(osd, xl, y, x4 - xl, y4 - y, Title, font, Theme.Color(clrMenuEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Info->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, x4 - xl, y4 - y, Info->ShortText(), font, Theme.Color(clrMenuEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Info->Description())) {
     int yt = y;
     int yb = y4 - Roundness;
     textScroller.Set(osd, xl, yt, x4 - xl, yb - yt, Info->Description(), font, Theme.Color(clrMenuEventDescription), Theme.Color(clrBackground));
     yb = yt + textScroller.Height();
     osd->DrawEllipse  (x1, yt - Roundness, x2, yt,             frameColor, -3);
     osd->DrawRectangle(x1, yt,             x2, yb,             frameColor);
     osd->DrawEllipse  (x1, yb,             x2, yb + Roundness, frameColor, -2);
     SetScrollbar();
     }
}

void cSkinSTTNGDisplayMenu::SetText(const char *Text, bool FixedFont)
{
  const cFont *font = cFont::GetFont(FixedFont ? fontFix : fontOsd);
  font = cFont::GetFont(fontSml);//XXX -> make a way to let the text define which font to use
  textScroller.Set(osd, x3, y3, x4 - x3, y4 - y3, Text, font, Theme.Color(clrMenuText), Theme.Color(clrBackground));
  SetScrollbar();
}

void cSkinSTTNGDisplayMenu::Flush(void)
{
  if (!message) {
     cString date = DayDateTime();
     const cFont *font = cFont::GetFont(fontSml);
     osd->DrawText(x4 - font->Width(date) - 2, y7 - font->Height(date), date, Theme.Color(clrMenuDate), frameColor, font);
     }
  osd->Flush();
}

// --- cSkinSTTNGDisplayReplay -----------------------------------------------

class cSkinSTTNGDisplayReplay : public cSkinDisplayReplay {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1, y2, y3, y4, y5, y6, y7;
  tColor frameColor;
  int lastCurrentWidth;
  bool message;
public:
  cSkinSTTNGDisplayReplay(bool ModeOnly);
  virtual ~cSkinSTTNGDisplayReplay();
  virtual void SetTitle(const char *Title);
  virtual void SetMode(bool Play, bool Forward, int Speed);
  virtual void SetProgress(int Current, int Total);
  virtual void SetCurrent(const char *Current);
  virtual void SetTotal(const char *Total);
  virtual void SetJump(const char *Jump);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

#define SymbolWidth 30
#define SymbolHeight 30

cSkinSTTNGDisplayReplay::cSkinSTTNGDisplayReplay(bool ModeOnly)
{
  const cFont *font = cFont::GetFont(fontSml);
  int lineHeight = font->Height();
  frameColor = Theme.Color(clrReplayFrame);
  lastCurrentWidth = 0;
  message = false;
  cBitmap bm(play_xpm);
  x0 = 0;
  x1 = max(SymbolWidth, bm.Width());
  x2 = x1 + Roundness;
  x3 = x2 + Gap;
  x7 = Setup.OSDWidth;
  x6 = x7 - lineHeight / 2;
  x5 = x6 - lineHeight / 2;
  x4 = x5 - Gap;
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + Roundness;
  y3 = y2 + Gap;
  y4 = y3 + max(SymbolHeight, bm.Height());
  y5 = y4 + Gap;
  y6 = y5 + Roundness;
  y7 = y6 + font->Height();
  int yt = (y0 + y1) / 2;
  int yb = (y6 + y7) / 2;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y7);
  tArea Areas[] = { { 0, 0, x7 - 1, y7 - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawRectangle(x0, y0, x7 - 1, y7 - 1, ModeOnly ? clrTransparent : Theme.Color(clrBackground));
  if (!ModeOnly) {
     osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
     osd->DrawRectangle(x0, y6, x1 - 1, y7 - 1, clrTransparent);
     osd->DrawRectangle(x6, y0, x7 - 1, yt - 1, clrTransparent);
     osd->DrawRectangle(x6, yb, x7 - 1, y7 - 1, clrTransparent);
     osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 2);
     osd->DrawRectangle(x1, y0, x4 - 1, y1 - 1, frameColor);
     osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
     osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
     osd->DrawRectangle(x0, y1, x1 - 1, y2 - 1, frameColor);
     osd->DrawEllipse  (x1, y1, x2 - 1, y2 - 1, frameColor, -2);
     }
  osd->DrawRectangle(x0, y3, x1 - 1, y4 - 1, frameColor);
  if (!ModeOnly) {
     osd->DrawRectangle(x0, y5, x1 - 1, y6 - 1, frameColor);
     osd->DrawEllipse  (x1, y5, x2 - 1, y6 - 1, frameColor, -3);
     osd->DrawEllipse  (x0, y6, x1 - 1, y7 - 1, frameColor, 3);
     osd->DrawRectangle(x1, y6, x4 - 1, y7 - 1, frameColor);
     osd->DrawRectangle(x5, y6, x6 - 1, y7 - 1, frameColor);
     osd->DrawEllipse  (x6, y6, x7 - 1, y7 - 1, frameColor, 5);
     }
}

cSkinSTTNGDisplayReplay::~cSkinSTTNGDisplayReplay()
{
  delete osd;
}

void cSkinSTTNGDisplayReplay::SetTitle(const char *Title)
{
  osd->DrawText(x3 + 5, y0, Title, Theme.Color(clrReplayTitle), frameColor, cFont::GetFont(fontSml), x4 - x3 - 5);
}

static char **ReplaySymbols[2][2][5] = {
  { { pause_xpm, srew_xpm, srew1_xpm, srew2_xpm, srew3_xpm },
    { pause_xpm, sfwd_xpm, sfwd1_xpm, sfwd2_xpm, sfwd3_xpm }, },
  { { play_xpm,  frew_xpm, frew1_xpm, frew2_xpm, frew3_xpm },
    { play_xpm,  ffwd_xpm, ffwd1_xpm, ffwd2_xpm, ffwd3_xpm } }
  };

void cSkinSTTNGDisplayReplay::SetMode(bool Play, bool Forward, int Speed)
{
  if (Speed < -1)
     Speed = -1;
  if (Speed > 3)
     Speed = 3;
  cBitmap bm(ReplaySymbols[Play][Forward][Speed + 1]);
  osd->DrawBitmap(x0 + (x1 - x0 - bm.Width()) / 2, y3 + (y4 - y3 - bm.Height()) / 2, bm, Theme.Color(clrReplayMode), frameColor);
}

void cSkinSTTNGDisplayReplay::SetProgress(int Current, int Total)
{
  cProgressBar pb(x4 - x3, y4 - y3, Current, Total, marks, Theme.Color(clrReplayProgressSeen), Theme.Color(clrReplayProgressRest), Theme.Color(clrReplayProgressSelected), Theme.Color(clrReplayProgressMark), Theme.Color(clrReplayProgressCurrent));
  osd->DrawBitmap(x3, y3, pb);
}

void cSkinSTTNGDisplayReplay::SetCurrent(const char *Current)
{
  const cFont *font = cFont::GetFont(fontSml);
  int w = font->Width(Current);
  osd->DrawText(x3, y6, Current, Theme.Color(clrReplayCurrent), frameColor, font, lastCurrentWidth > w ? lastCurrentWidth : 0);
  lastCurrentWidth = w;
}

void cSkinSTTNGDisplayReplay::SetTotal(const char *Total)
{
  const cFont *font = cFont::GetFont(fontSml);
  osd->DrawText(x4 - font->Width(Total) - 5, y6, Total, Theme.Color(clrReplayTotal), frameColor, font);
}

void cSkinSTTNGDisplayReplay::SetJump(const char *Jump)
{
  osd->DrawText(x0 + (x4 - x0) / 4, y6, Jump, Theme.Color(clrReplayJump), frameColor, cFont::GetFont(fontSml), (x4 - x3) / 2, 0, taCenter);
}

void cSkinSTTNGDisplayReplay::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontSml);
  if (Text) {
     osd->SaveRegion(x2, y6, x4 - 1, y7 - 1);
     osd->DrawRectangle(x2, y6, x3 - 1, y7 - 1, Theme.Color(clrBackground));
     osd->DrawText(x3, y6, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x4 - x3, 0, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinSTTNGDisplayReplay::Flush(void)
{
  osd->Flush();
}

// --- cSkinSTTNGDisplayVolume -----------------------------------------------

class cSkinSTTNGDisplayVolume : public cSkinDisplayVolume {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1;
  tColor frameColor;
  int mute;
public:
  cSkinSTTNGDisplayVolume(void);
  virtual ~cSkinSTTNGDisplayVolume();
  virtual void SetVolume(int Current, int Total, bool Mute);
  virtual void Flush(void);
  };

cSkinSTTNGDisplayVolume::cSkinSTTNGDisplayVolume(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  frameColor = Theme.Color(clrVolumeFrame);
  mute = -1;
  x0 = 0;
  x1 = lineHeight / 2;
  x2 = lineHeight;
  x3 = x2 + Gap;
  x7 = Setup.OSDWidth;
  x6 = x7 - lineHeight / 2;
  x5 = x6 - lineHeight / 2;
  x4 = x5 - Gap;
  y0 = 0;
  y1 = lineHeight;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y1);
  tArea Areas[] = { { x0, y0, x7 - 1, y1 - 1, 4 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawRectangle(x0, y0, x7 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 7);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
}

cSkinSTTNGDisplayVolume::~cSkinSTTNGDisplayVolume()
{
  delete osd;
}

void cSkinSTTNGDisplayVolume::SetVolume(int Current, int Total, bool Mute)
{
  int xl = x3 + 5;
  int xr = x4 - 5;
  int yt = y0 + 3;
  int yb = y1 - 3;
  if (mute != Mute) {
     osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
     mute = Mute;
     }
  cBitmap bm(Mute ? mute_xpm : volume_xpm);
  osd->DrawBitmap(xl, y0 + (y1 - y0 - bm.Height()) / 2, bm, Theme.Color(clrVolumeSymbol), frameColor);
  if (!Mute) {
     xl += bm.Width() + 5;
     int w = (y1 - y0) / 3;
     int d = 3;
     int n = (xr - xl + d) / (w + d);
     int x = xr - n * (w + d);
     tColor Color = Theme.Color(clrVolumeBarLower);
     for (int i = 0; i < n; i++) {
         if (Total * i >= Current * n)
            Color = Theme.Color(clrVolumeBarUpper);
         osd->DrawRectangle(x, yt, x + w - 1, yb - 1, Color);
         x += w + d;
         }
     }
}

void cSkinSTTNGDisplayVolume::Flush(void)
{
  osd->Flush();
}

// --- cSkinSTTNGDisplayTracks -----------------------------------------------

class cSkinSTTNGDisplayTracks : public cSkinDisplayTracks {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1, y2, y3, y4, y5, y6, y7;
  int lineHeight;
  tColor frameColor;
  int currentIndex;
  static cBitmap bmAudioLeft, bmAudioRight, bmAudioStereo;
  void SetItem(const char *Text, int Index, bool Current);
public:
  cSkinSTTNGDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks);
  virtual ~cSkinSTTNGDisplayTracks();
  virtual void SetTrack(int Index, const char * const *Tracks);
  virtual void SetAudioChannel(int AudioChannel);
  virtual void Flush(void);
  };

cBitmap cSkinSTTNGDisplayTracks::bmAudioLeft(audioleft_xpm);
cBitmap cSkinSTTNGDisplayTracks::bmAudioRight(audioright_xpm);
cBitmap cSkinSTTNGDisplayTracks::bmAudioStereo(audiostereo_xpm);

cSkinSTTNGDisplayTracks::cSkinSTTNGDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  frameColor = Theme.Color(clrMenuFrame);
  currentIndex = -1;
  int ItemsWidth = font->Width(Title);
  for (int i = 0; i < NumTracks; i++)
      ItemsWidth = max(ItemsWidth, font->Width(Tracks[i]));
  ItemsWidth += 10;
  x0 = 0;
  x1 = lineHeight / 2;
  x3 = (x1 + Roundness + Gap + 7) & ~0x07; // must be multiple of 8
  x2 = x3 - Gap;
  x7 = Setup.OSDWidth;
  x6 = x7 - lineHeight / 2;
  x4 = (x6 - lineHeight / 2 - Gap) & ~0x07; // must be multiple of 8
  x5 = x4 + Gap;
  int d = x4 - x3;
  if (d > ItemsWidth) {
     d = (d - ItemsWidth) & ~0x07; // must be multiple of 8
     x4 -= d;
     x5 -= d;
     x6 -= d;
     x7 -= d;
     }
  y0 = 0;
  y1 = lineHeight;
  y2 = y1 + Roundness;
  y3 = y2 + Gap;
  // limit to Setup.OSDHeight? - what if height is too big???
  y4 = y3 + NumTracks * lineHeight + 2 * Roundness;
  y5 = y4 + Gap;
  y6 = y5 + Roundness;
  y7 = y6 + cFont::GetFont(fontSml)->Height();
  int yt = (y0 + y1) / 2;
  int yb = (y6 + y7) / 2;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y7);
  tArea Areas[] = { { x0, y0, x7 - 1, y7 - 1, 4 } };
  if (osd->CanHandleAreas(Areas, sizeof(Areas) / sizeof(tArea)) == oeOk)
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  else {
     tArea Areas[] = { { x0, y0, x7 - 1, y3 - 1, 2 },
                       { x0, y3, x3 - 1, y4 - 1, 1 },
                       { x3, y3, x4 - 1, y4 - 1, 2 },
                       { x4, y3, x7 - 1, y4 - 1, 2 },
                       { x0, y4, x7 - 1, y7 - 1, 4 }
                     };
     osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
     }
  osd->DrawRectangle(x0, y0, x7 - 1, y7 - 1, Theme.Color(clrBackground));
  osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
  osd->DrawRectangle(x0, y6, x1 - 1, y7 - 1, clrTransparent);
  osd->DrawRectangle(x6, y0, x7 - 1, yt - 1, clrTransparent);
  osd->DrawRectangle(x6, yb, x7 - 1, y7 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 2);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
  osd->DrawRectangle(x0, y1, x1 - 1, y6 - 1, frameColor);
  osd->DrawEllipse  (x1, y1, x2 - 1, y2 - 1, frameColor, -2);
  osd->DrawEllipse  (x1, y5, x2 - 1, y6 - 1, frameColor, -3);
  osd->DrawEllipse  (x0, y6, x1 - 1, y7 - 1, frameColor, 3);
  osd->DrawRectangle(x1, y6, x2 - 1, y7 - 1, frameColor);
  osd->DrawRectangle(x3, y6, x4 - 1, y7 - 1, frameColor);
  osd->DrawRectangle(x5, y6, x6 - 1, y7 - 1, frameColor);
  osd->DrawEllipse  (x6, y6, x7 - 1, y7 - 1, frameColor, 5);
  osd->DrawText(x3 + 5, y0, Title, Theme.Color(clrMenuTitle), frameColor, font, x4 - x3 - 5);
  for (int i = 0; i < NumTracks; i++)
      SetItem(Tracks[i], i, false);
}

cSkinSTTNGDisplayTracks::~cSkinSTTNGDisplayTracks()
{
  delete osd;
}

void cSkinSTTNGDisplayTracks::SetItem(const char *Text, int Index, bool Current)
{
  int y = y3 + Roundness + Index * lineHeight;
  tColor ColorFg, ColorBg;
  if (Current) {
     ColorFg = Theme.Color(clrMenuItemCurrentFg);
     ColorBg = Theme.Color(clrMenuItemCurrentBg);
     osd->DrawEllipse  (x1, y - Roundness,  x2 - 1, y - 1, frameColor, -3);
     osd->DrawRectangle(x1, y,              x2 - 1, y + lineHeight - 1, frameColor);
     osd->DrawEllipse  (x1, y + lineHeight, x2 - 1, y + lineHeight + Roundness - 1, frameColor, -2);
     osd->DrawRectangle(x3, y,              x4 - 1, y + lineHeight - 1, ColorBg);
     currentIndex = Index;
     }
  else {
     ColorFg = Theme.Color(clrMenuItemSelectable);
     ColorBg = Theme.Color(clrBackground);
     if (currentIndex == Index) {
        osd->DrawRectangle(x1, y - Roundness,  x2 - 1, y + lineHeight + Roundness - 1, Theme.Color(clrBackground));
        osd->DrawRectangle(x3, y,              x4 - 1, y + lineHeight - 1, Theme.Color(clrBackground));
        }
     }
  const cFont *font = cFont::GetFont(fontOsd);
  int xt = x3 + 5;
  osd->DrawText(xt, y, Text, ColorFg, ColorBg, font, x4 - xt);
}

void cSkinSTTNGDisplayTracks::SetTrack(int Index, const char * const *Tracks)
{
  if (currentIndex >= 0)
     SetItem(Tracks[currentIndex], currentIndex, false);
  SetItem(Tracks[Index], Index, true);
}

void cSkinSTTNGDisplayTracks::SetAudioChannel(int AudioChannel)
{
  cBitmap *bm = NULL;
  switch (AudioChannel) {
    case 0: bm = &bmAudioStereo; break;
    case 1: bm = &bmAudioLeft;   break;
    case 2: bm = &bmAudioRight;  break;
    }
  if (bm)
     osd->DrawBitmap(x3 + 5, y6 + (y7 - y6 - bm->Height()) / 2, *bm, Theme.Color(clrChannelSymbolOn), frameColor);
  else
     osd->DrawRectangle(x3, y6, x4 - 1, y7 - 1, frameColor);
}

void cSkinSTTNGDisplayTracks::Flush(void)
{
  osd->Flush();
}

// --- cSkinSTTNGDisplayMessage ----------------------------------------------

class cSkinSTTNGDisplayMessage : public cSkinDisplayMessage {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1;
public:
  cSkinSTTNGDisplayMessage(void);
  virtual ~cSkinSTTNGDisplayMessage();
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinSTTNGDisplayMessage::cSkinSTTNGDisplayMessage(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  tColor frameColor = Theme.Color(clrMessageFrame);
  x0 = 0;
  x1 = lineHeight / 2;
  x2 = lineHeight;
  x3 = x2 + Gap;
  x7 = Setup.OSDWidth;
  x6 = x7 - lineHeight / 2;
  x5 = x6 - lineHeight / 2;
  x4 = x5 - Gap;
  y0 = 0;
  y1 = lineHeight;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop + Setup.OSDHeight - y1);
  tArea Areas[] = { { x0, y0, x7 - 1, y1 - 1, 2 } };
  osd->SetAreas(Areas, sizeof(Areas) / sizeof(tArea));
  osd->DrawRectangle(x0, y0, x7 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 7);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
}

cSkinSTTNGDisplayMessage::~cSkinSTTNGDisplayMessage()
{
  delete osd;
}

void cSkinSTTNGDisplayMessage::SetMessage(eMessageType Type, const char *Text)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(x3, y0, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), font, x4 - x3, 0, taCenter);
}

void cSkinSTTNGDisplayMessage::Flush(void)
{
  osd->Flush();
}

// --- cSkinSTTNG ------------------------------------------------------------

cSkinSTTNG::cSkinSTTNG(void)
:cSkin("sttng", &::Theme)//XXX naming problem???
{
}

const char *cSkinSTTNG::Description(void)
{
  return tr("ST:TNG Panels");
}

cSkinDisplayChannel *cSkinSTTNG::DisplayChannel(bool WithInfo)
{
  return new cSkinSTTNGDisplayChannel(WithInfo);
}

cSkinDisplayMenu *cSkinSTTNG::DisplayMenu(void)
{
  return new cSkinSTTNGDisplayMenu;
}

cSkinDisplayReplay *cSkinSTTNG::DisplayReplay(bool ModeOnly)
{
  return new cSkinSTTNGDisplayReplay(ModeOnly);
}

cSkinDisplayVolume *cSkinSTTNG::DisplayVolume(void)
{
  return new cSkinSTTNGDisplayVolume;
}

cSkinDisplayTracks *cSkinSTTNG::DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  return new cSkinSTTNGDisplayTracks(Title, NumTracks, Tracks);
}

cSkinDisplayMessage *cSkinSTTNG::DisplayMessage(void)
{
  return new cSkinSTTNGDisplayMessage;
}
