/*
 * skinlcars.c: A VDR skin with Star Trek's "LCARS" layout
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinlcars.c 5.6 2024/09/19 09:49:02 kls Exp $
 */

// "Star Trek: The Next Generation"(R) is a registered trademark of Paramount Pictures,
// registered in the United States Patent and Trademark Office, all rights reserved.
// The LCARS system is based upon the designs of Michael Okuda and his Okudagrams.
//
// "LCARS" is short for "Library Computer Access and Retrieval System".
// Some resources used for writing this skin can be found at
// http://www.lcars.org.uk
// http://www.lcarsdeveloper.com
// http://www.lcarscom.net
// http://lds-jedi.deviantart.com/art/LCARS-Swept-Tutorial-213936938
// http://lds-jedi.deviantart.com/art/LCARS-Button-Tutorial-210783437
// http://zelldenver.deviantart.com/art/LCARS-Color-Standard-179565780
// http://www.lcars47.com
// http://www.bracercom.com/tutorial/content/CoherentLCARSInterface/LCARSCoherentInterface.html
// http://www.bracercom.com/tutorial/content/lcars_manifesto/the_lcars_manifesto.html

#include "skinlcars.h"
#include "font.h"
#include "menu.h"
#include "osd.h"
#include "positioner.h"
#include "themes.h"
#include "videodir.h"

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

#define Gap            (Setup.FontOsdSize / 5 & ~1) // must be even
#define TextFrame      (Setup.FontOsdSize / TEXT_ALIGN_BORDER)
#define TextSpacing    (2 * TextFrame)
#define SymbolSpacing  TextSpacing
#define ShowSeenExtent (Setup.FontOsdSize / 5) // pixels by which the "seen" bar extends out of the frame

#define DISKUSAGEALERTLIMIT 95 // percent of disk usage above which the display goes into alert mode
#define SIGNALDISPLAYDELTA   2 // seconds between subsequent device signal displays

static cTheme Theme;

// Color domains:

#define CLR_BACKGROUND      0x99000000
#define CLR_MAIN_FRAME      0xFFFF9966
#define CLR_CHANNEL_FRAME   0xFF8A9EC9
#define CLR_REPLAY_FRAME    0xFFCC6666
#define CLR_DATE            0xFF99CCFF
#define CLR_MENU_ITEMS      0xFF9999FF
#define CLR_TIMER           0xFF99CCFF
#define CLR_DEVICE          0xFFF1B1AF
#define CLR_CHANNEL_NAME    0xFF99CCFF
#define CLR_EVENT_TITLE     0xFF99CCFF
#define CLR_EVENT_TIME      0xFFFFCC66
#define CLR_EVENT_SHORTTEXT 0xFFFFCC66
#define CLR_TEXT            0xFF99CCFF
#define CLR_TRACK           0xFFFFCC66
#define CLR_SEEN            0xFFCC99CC
#define CLR_ALERT           0xFFFF0000
#define CLR_EXPOSED         0xFF990000
#define CLR_WHITE           0xFFFFFFFF
#define CLR_RED             0xFFCC6666
#define CLR_GREEN           0xFFA0FF99
#define CLR_YELLOW          0xFFF1DF60
#define CLR_BLUE            0xFF9A99FF
#define CLR_BLACK           0xFF000000

// General colors:

THEME_CLR(Theme, clrBackground,             CLR_BACKGROUND);
THEME_CLR(Theme, clrDateFg,                 CLR_BLACK);
THEME_CLR(Theme, clrDateBg,                 CLR_DATE);
THEME_CLR(Theme, clrTimerFg,                CLR_BLACK);
THEME_CLR(Theme, clrTimerBg,                CLR_TIMER);
THEME_CLR(Theme, clrDeviceFg,               CLR_BLACK);
THEME_CLR(Theme, clrDeviceBg,               CLR_DEVICE);
THEME_CLR(Theme, clrSignalValue,            CLR_GREEN);
THEME_CLR(Theme, clrSignalRest,             CLR_RED);
THEME_CLR(Theme, clrSeen,                   CLR_SEEN);
THEME_CLR(Theme, clrTrackName,              CLR_TRACK);
THEME_CLR(Theme, clrAlertFg,                CLR_WHITE);
THEME_CLR(Theme, clrAlertBg,                CLR_ALERT);
THEME_CLR(Theme, clrChannelName,            CLR_CHANNEL_NAME);
THEME_CLR(Theme, clrEventTitle,             CLR_EVENT_TITLE);
THEME_CLR(Theme, clrEventTime,              CLR_EVENT_TIME);
THEME_CLR(Theme, clrEventShortText,         CLR_EVENT_SHORTTEXT);
THEME_CLR(Theme, clrEventDescription,       CLR_TEXT);

// Buttons:

THEME_CLR(Theme, clrButtonRedFg,            CLR_BLACK);
THEME_CLR(Theme, clrButtonRedBg,            CLR_RED);
THEME_CLR(Theme, clrButtonGreenFg,          CLR_BLACK);
THEME_CLR(Theme, clrButtonGreenBg,          CLR_GREEN);
THEME_CLR(Theme, clrButtonYellowFg,         CLR_BLACK);
THEME_CLR(Theme, clrButtonYellowBg,         CLR_YELLOW);
THEME_CLR(Theme, clrButtonBlueFg,           CLR_BLACK);
THEME_CLR(Theme, clrButtonBlueBg,           CLR_BLUE);

// Messages:

THEME_CLR(Theme, clrMessageStatusFg,        CLR_BLACK);
THEME_CLR(Theme, clrMessageStatusBg,        CLR_BLUE);
THEME_CLR(Theme, clrMessageInfoFg,          CLR_BLACK);
THEME_CLR(Theme, clrMessageInfoBg,          CLR_GREEN);
THEME_CLR(Theme, clrMessageWarningFg,       CLR_BLACK);
THEME_CLR(Theme, clrMessageWarningBg,       CLR_YELLOW);
THEME_CLR(Theme, clrMessageErrorFg,         CLR_BLACK);
THEME_CLR(Theme, clrMessageErrorBg,         CLR_RED);

// Volume:

THEME_CLR(Theme, clrVolumeFrame,            CLR_MAIN_FRAME);
THEME_CLR(Theme, clrVolumeSymbol,           CLR_BLACK);
THEME_CLR(Theme, clrVolumeBarUpper,         RgbShade(CLR_MAIN_FRAME, -0.2));
THEME_CLR(Theme, clrVolumeBarLower,         CLR_GREEN);

// Channel display:

THEME_CLR(Theme, clrChannelFrameFg,         CLR_BLACK);
THEME_CLR(Theme, clrChannelFrameBg,         CLR_CHANNEL_FRAME);
THEME_CLR(Theme, clrChannelSymbolOn,        CLR_BLACK);
THEME_CLR(Theme, clrChannelSymbolOff,       RgbShade(CLR_CHANNEL_FRAME, -0.2));
THEME_CLR(Theme, clrChannelSymbolRecFg,     CLR_WHITE);
THEME_CLR(Theme, clrChannelSymbolRecBg,     CLR_RED);

// Menu:

THEME_CLR(Theme, clrMenuFrameFg,            CLR_BLACK);
THEME_CLR(Theme, clrMenuFrameBg,            CLR_MAIN_FRAME);
THEME_CLR(Theme, clrMenuTitle,              CLR_MAIN_FRAME);
THEME_CLR(Theme, clrMenuMainBracket,        CLR_MENU_ITEMS);
THEME_CLR(Theme, clrMenuTimerRecording,     CLR_DEVICE);
THEME_CLR(Theme, clrMenuDeviceRecording,    CLR_TIMER);
THEME_CLR(Theme, clrMenuItemCurrentFg,      CLR_MAIN_FRAME);
THEME_CLR(Theme, clrMenuItemCurrentBg,      RgbShade(CLR_MENU_ITEMS, -0.5));
THEME_CLR(Theme, clrMenuItemSelectable,     CLR_MENU_ITEMS);
THEME_CLR(Theme, clrMenuItemNonSelectable,  CLR_TEXT);
THEME_CLR(Theme, clrMenuScrollbarTotal,     RgbShade(CLR_MAIN_FRAME, 0.2));
THEME_CLR(Theme, clrMenuScrollbarShown,     CLR_SEEN);
THEME_CLR(Theme, clrMenuScrollbarArrow,     CLR_BLACK);
THEME_CLR(Theme, clrMenuText,               CLR_TEXT);

// Replay display:

THEME_CLR(Theme, clrReplayFrameFg,          CLR_BLACK);
THEME_CLR(Theme, clrReplayFrameBg,          CLR_REPLAY_FRAME);
THEME_CLR(Theme, clrReplayPosition,         CLR_SEEN);
THEME_CLR(Theme, clrReplayJumpFg,           CLR_BLACK);
THEME_CLR(Theme, clrReplayJumpBg,           CLR_SEEN);
THEME_CLR(Theme, clrReplayProgressSeen,     CLR_SEEN);
THEME_CLR(Theme, clrReplayProgressRest,     RgbShade(CLR_WHITE, -0.2));
THEME_CLR(Theme, clrReplayProgressSelected, CLR_EXPOSED);
THEME_CLR(Theme, clrReplayProgressMark,     CLR_BLACK);
THEME_CLR(Theme, clrReplayProgressCurrent,  CLR_EXPOSED);
THEME_CLR(Theme, clrReplayProgressError,    CLR_BLACK);

// Track display:

THEME_CLR(Theme, clrTrackFrameFg,           CLR_BLACK);
THEME_CLR(Theme, clrTrackFrameBg,           CLR_TRACK);
THEME_CLR(Theme, clrTrackItemFg,            CLR_BLACK);
THEME_CLR(Theme, clrTrackItemBg,            RgbShade(CLR_TRACK, 0.5));
THEME_CLR(Theme, clrTrackItemCurrentFg,     CLR_BLACK);
THEME_CLR(Theme, clrTrackItemCurrentBg,     CLR_TRACK);

// --- Helper functions ------------------------------------------------------

static bool TwoColors = false;

static cOsd *CreateOsd(int Left, int Top, int x0, int y0, int x1, int y1)
{
  cOsd *Osd = cOsdProvider::NewOsd(Left, Top);
  int Bpp[] = { 32, 8, 4, 2, 1 };
  tArea Area = { x0, y0, x1, y1, 0 };
  for (unsigned int i = 0; i < sizeof(Bpp) / sizeof(int); i++) {
      Area.bpp = Bpp[i];
      if (Osd->CanHandleAreas(&Area, 1) == oeOk) {
         Osd->SetAreas(&Area, 1);
         Osd->SetAntiAliasGranularity(20, 16);
         TwoColors = Area.bpp == 1;
         break;
         }
      }
  return Osd;
}

static cFont *CreateTinyFont(int LineHeight)
{
  // Creates a font that is not higher than half of LineHeight.
  LineHeight /= 2;
  int Height = LineHeight;
  for (;;) {
      cFont *TinyFont = cFont::CreateFont(Setup.FontOsd, Height);
      if (Height < 2 || TinyFont->Height() <= LineHeight)
         return TinyFont;
      delete TinyFont;
      Height -= 1;
      }
}

static bool DrawDeviceData(cOsd *Osd, const cDevice *Device, int x0, int y0, int x1, int y1, int &xs, const cFont *TinyFont, cString &LastDeviceType, cCamSlot *&LastCamSlot, bool Initial)
{
  cString DeviceType = Device->DeviceType();
  cCamSlot *CamSlot = Device->CamSlot();
  if (Initial || strcmp(DeviceType, LastDeviceType) || CamSlot != LastCamSlot) {
     const cFont *font = cFont::GetFont(fontOsd);
     tColor ColorFg = Theme.Color(clrDeviceFg);
     tColor ColorBg = Theme.Color(clrDeviceBg);
     Osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, ColorBg);
     int x = x0;
     // Device number:
     cString Nr = itoa(Device->DeviceNumber() + 1);
     int w = max(font->Width(Nr), y1 - y0);
     Osd->DrawText(x, y0, Nr, ColorFg, ColorBg, font, w, y1 - y0, taCenter);
     x += w;
     // Device type:
     Osd->DrawText(x, y0, DeviceType, ColorFg, ColorBg, TinyFont);
     xs = max(xs, x + TinyFont->Width(DeviceType));
     LastDeviceType = DeviceType;
     // CAM:
     if (CamSlot) {
        cString s = cString::sprintf("CAM %d", CamSlot->MasterSlotNumber());
        Osd->DrawText(x, y1 - TinyFont->Height(), s, ColorFg, ColorBg, TinyFont);
        xs = max(xs, x + TinyFont->Width(s));
        }
     LastCamSlot = CamSlot;
     return true;
     }
  return false;
}

static void DrawDeviceSignal(cOsd *Osd, const cDevice *Device, int x0, int y0, int x1, int y1, int &LastSignalStrength, int &LastSignalQuality, bool Initial)
{
  int SignalStrength = Device->SignalStrength();
  int SignalQuality = Device->SignalQuality();
  int d = max((y1 - y0) / 10, 1);
  int x00 = x0 + d;
  int x01 = x1 - d;
  int h = (y1 - y0 - 3 * d) / 2;
  int w = x01 - x00;
  int y00 = y0 + d;
  int y01 = y00 + h;
  int y03 = y1 - d;
  int y02 = y03 - h;
  tColor ColorSignalValue, ColorSignalRest;
  if (TwoColors) {
     ColorSignalValue = Theme.Color(clrBackground);
     ColorSignalRest = Theme.Color(clrMenuFrameBg);
     }
  else {
     ColorSignalValue = Theme.Color(clrSignalValue);
     ColorSignalRest = Theme.Color(clrSignalRest);
     }
  if (SignalStrength >= 0 && (Initial || SignalStrength != LastSignalStrength)) {
     int s = SignalStrength * w / 100;
     Osd->DrawRectangle(x00, y00, x00 + s - 1, y01 - 1, ColorSignalValue);
     Osd->DrawRectangle(x00 + s, y00, x01 - 1, y01 - 1, ColorSignalRest);
     LastSignalStrength = SignalStrength;
     }
  if (SignalQuality >= 0 && (Initial || SignalQuality != LastSignalQuality)) {
     int q = SignalQuality * w / 100;
     Osd->DrawRectangle(x00, y02, x00 + q - 1, y03 - 1, ColorSignalValue);
     Osd->DrawRectangle(x00 + q, y02, x01 - 1, y03 - 1, ColorSignalRest);
     LastSignalQuality = SignalQuality;
     }
}

static void DrawDevicePosition(cOsd *Osd, const cPositioner *Positioner, int x0, int y0, int x1, int y1, int &LastCurrent)
{
  int HorizonLeft = Positioner->HorizonLongitude(cPositioner::pdLeft);
  int HorizonRight = Positioner->HorizonLongitude(cPositioner::pdRight);
  int HardLimitLeft = cPositioner::NormalizeAngle(HorizonLeft - Positioner->HardLimitLongitude(cPositioner::pdLeft));
  int HardLimitRight = cPositioner::NormalizeAngle(Positioner->HardLimitLongitude(cPositioner::pdRight) - HorizonRight);
  int HorizonDelta = cPositioner::NormalizeAngle(HorizonLeft - HorizonRight);
  int Current = cPositioner::NormalizeAngle(HorizonLeft - Positioner->CurrentLongitude());
  int Target = cPositioner::NormalizeAngle(HorizonLeft - Positioner->TargetLongitude());
  int d = (y1 - y0) / 2;
  int w = x1 - x0 - 2 * d;
  int l = max(x0 + d, x0 + d + w * HardLimitLeft / HorizonDelta);
  int r = min(x1 - d, x1 - d - w * HardLimitRight / HorizonDelta) - 1;
  int c = constrain(x0 + d + w * Current / HorizonDelta, l, r);
  int t = constrain(x0 + d + w * Target / HorizonDelta, l, r);
  if (c == LastCurrent)
     return;
  if (c > t)
     swap(c, t);
  tColor ColorRange, ColorMove;
  if (TwoColors) {
     ColorRange = Theme.Color(clrChannelFrameBg);
     ColorMove = Theme.Color(clrBackground);
     }
  else {
     ColorRange = Theme.Color(clrChannelFrameBg);
     ColorMove = Theme.Color(clrDeviceBg);
     }
  Osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, Theme.Color(clrBackground));
  Osd->DrawEllipse(l - d, y0, l, y1 - 1, ColorRange, 7);
  Osd->DrawRectangle(l, y0, r, y1 - 1, ColorRange);
  Osd->DrawEllipse(r, y0, r + d, y1 - 1, ColorRange, 5);
  Osd->DrawEllipse(c - d, y0, c, y1 - 1, ColorMove, 7);
  Osd->DrawRectangle(c, y0, t, y1 - 1, ColorMove);
  Osd->DrawEllipse(t, y0, t + d, y1 - 1, ColorMove, 5);
  LastCurrent = c;
}

// --- cSkinLCARSDisplayChannel ----------------------------------------------

class cSkinLCARSDisplayChannel : public cSkinDisplayChannel {
private:
  cOsd *osd;
  int xc00, xc01, xc02, xc03, xc04, xc05, xc06, xc07, xc08, xc09, xc10, xc11, xc12, xc13, xc14, xc15;
  int yc00, yc01, yc02, yc03, yc04, yc05, yc06, yc07, yc08, yc09, yc10, yc11, yc12;
  int xs; // starting column for signal display
  bool withInfo;
  int lineHeight;
  cFont *tinyFont;
  cFont *tallFont;
  tColor frameColor;
  bool message;
  const cEvent *present;
  bool initial;
  cString lastDate;
  int lastSeen;
  int lastCurrentPosition;
  int lastDeviceNumber;
  cString lastDeviceType;
  cCamSlot *lastCamSlot;
  int lastSignalStrength;
  int lastSignalQuality;
  time_t lastSignalDisplay;
  tTrackId lastTrackId;
  static cBitmap bmTeletext, bmRadio, bmAudio, bmDolbyDigital, bmEncrypted, bmRecording;
  void DrawDate(void);
  void DrawTrack(void);
  void DrawSeen(int Current, int Total);
  void DrawDevice(void);
  void DrawSignal(void);
public:
  cSkinLCARSDisplayChannel(bool WithInfo);
  virtual ~cSkinLCARSDisplayChannel();
  virtual void SetChannel(const cChannel *Channel, int Number);
  virtual void SetEvents(const cEvent *Present, const cEvent *Following);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void SetPositioner(const cPositioner *Positioner);
  virtual void Flush(void);
  };

cBitmap cSkinLCARSDisplayChannel::bmTeletext(teletext_xpm);
cBitmap cSkinLCARSDisplayChannel::bmRadio(radio_xpm);
cBitmap cSkinLCARSDisplayChannel::bmAudio(audio_xpm);
cBitmap cSkinLCARSDisplayChannel::bmDolbyDigital(dolbydigital_xpm);
cBitmap cSkinLCARSDisplayChannel::bmEncrypted(encrypted_xpm);
cBitmap cSkinLCARSDisplayChannel::bmRecording(recording_xpm);

cSkinLCARSDisplayChannel::cSkinLCARSDisplayChannel(bool WithInfo)
{
  tallFont = cFont::CreateFont(Setup.FontOsd, Setup.FontOsdSize * 1.8);
  initial = true;
  present = NULL;
  lastSeen = -1;
  lastCurrentPosition = -1;
  lastDeviceNumber = -1;
  lastCamSlot = NULL;
  lastSignalStrength = -1;
  lastSignalQuality = -1;
  lastSignalDisplay = 0;
  memset(&lastTrackId, 0, sizeof(lastTrackId));
  const cFont *font = cFont::GetFont(fontOsd);
  withInfo = WithInfo;
  lineHeight = font->Height();
  tinyFont = CreateTinyFont(lineHeight);
  frameColor = Theme.Color(clrChannelFrameBg);
  message = false;
  int d = 5 * lineHeight;
  xc00 = 0;
  xc01 = xc00 + d / 2;
  xc02 = xc00 + d;
  xc03 = xc02 + lineHeight;
  xc04 = xc02 + d / 4;
  xc05 = xc02 + d;
  xc06 = xc05 + Gap;
  xc15 = cOsd::OsdWidth();
  xc14 = xc15 - lineHeight;
  xc13 = xc14 - Gap;
  xc07 = (xc15 + xc00) / 2;
  xc08 = xc07 + Gap;
  xc09 = xc08 + lineHeight;
  xc10 = xc09 + Gap;
  xc11 = (xc10 + xc13 + Gap) / 2;
  xc12 = xc11 + Gap;

  yc00 = 0;
  yc01 = yc00 + lineHeight;
  yc02 = yc01 + lineHeight;
  yc03 = yc02 + Gap;
  yc04 = yc03 + 2 * lineHeight;
  yc05 = yc04 + Gap;
  yc06 = yc05 + 2 * lineHeight;

  yc07 = yc06 + Gap;
  yc12 = yc07 + 3 * lineHeight + Gap / 2;
  yc11 = yc12 - lineHeight;
  yc10 = yc11 - lineHeight;
  yc09 = yc11 - d / 4;
  yc08 = yc12 - d / 2;

  xs = 0;

  int y1 = withInfo ? yc12 : yc02;
  int y0 = cOsd::OsdTop() + (Setup.ChannelInfoPos ? 0 : cOsd::OsdHeight() - y1);
  osd = CreateOsd(cOsd::OsdLeft(), y0, xc00, yc00, xc15 - 1, y1 - 1);
  osd->DrawRectangle(xc00, yc00, xc15 - 1, y1 - 1, Theme.Color(clrBackground));
  // Rectangles:
  osd->DrawRectangle(xc00, yc00, xc02 - 1, yc02 - 1, frameColor);
  if (withInfo) {
     osd->DrawRectangle(xc00, yc03, xc02 - 1, yc04 - 1, frameColor);
     osd->DrawRectangle(xc00, yc05, xc02 - 1, yc06 - 1, frameColor);
     // Elbow:
     osd->DrawRectangle(xc00, yc07, xc01 - 1, yc08 - 1, frameColor);
     osd->DrawRectangle(xc00, yc08, xc01 - 1, yc12 - 1, clrTransparent);
     osd->DrawEllipse  (xc00, yc08, xc01 - 1, yc12 - 1, frameColor, 3);
     osd->DrawRectangle(xc01, yc07, xc02 - 1, yc12 - 1, frameColor);
     osd->DrawEllipse  (xc02, yc09, xc04 - 1, yc11 - 1, frameColor, -3);
     osd->DrawRectangle(xc02, yc11, xc05 - 1, yc12 - 1, frameColor);
     // Status area:
     osd->DrawRectangle(xc06, yc11 + lineHeight / 2, xc07 - 1, yc12 - 1, frameColor);
     osd->DrawRectangle(xc08, yc11, xc09 - 1, yc12 - 1, frameColor);
     osd->DrawRectangle(xc10, yc11, xc11 - 1, yc12 - 1, Theme.Color(clrDeviceBg));
     osd->DrawRectangle(xc12, yc11, xc13 - 1, yc12 - 1, Theme.Color(clrDateBg));
     osd->DrawRectangle(xc14, yc11, xc14 + lineHeight / 2 - 1, yc12 - 1, frameColor);
     osd->DrawRectangle(xc14 + lineHeight / 2, yc11 + lineHeight / 2, xc15 - 1, yc12 - 1, clrTransparent);
     osd->DrawEllipse  (xc14 + lineHeight / 2, yc11, xc15 - 1, yc12 - 1, frameColor, 5);
     }
  // Icons:
  osd->DrawRectangle(xc14, yc00, xc14 + lineHeight / 2 - 1, yc01 - 1, frameColor);
  osd->DrawRectangle(xc14 + lineHeight / 2, yc00, xc15 - 1, yc00 + lineHeight / 2 - 1, clrTransparent);
  osd->DrawEllipse  (xc14 + lineHeight / 2, yc00, xc15 - 1, yc01 - 1, frameColor, 5);
}

cSkinLCARSDisplayChannel::~cSkinLCARSDisplayChannel()
{
  delete tallFont;
  delete tinyFont;
  delete osd;
}

void cSkinLCARSDisplayChannel::DrawDate(void)
{
  cString s = DayDateTime();
  if (initial || !*lastDate || strcmp(s, lastDate)) {
     osd->DrawText(xc12, yc11, s, Theme.Color(clrDateFg), Theme.Color(clrDateBg), cFont::GetFont(fontOsd), xc13 - xc12, lineHeight, taRight | taBorder);
     lastDate = s;
     }
}

void cSkinLCARSDisplayChannel::DrawTrack(void)
{
  cDevice *Device = cDevice::PrimaryDevice();
  const tTrackId *Track = Device->GetTrack(Device->GetCurrentAudioTrack());
  if (Track ? strcmp(lastTrackId.description, Track->description) : *lastTrackId.description) {
     osd->DrawText(xc03, yc07, Track ? Track->description : "", Theme.Color(clrTrackName), Theme.Color(clrBackground), cFont::GetFont(fontOsd), xc07 - xc03);
     strn0cpy(lastTrackId.description, Track ? Track->description : "", sizeof(lastTrackId.description));
     }
}

void cSkinLCARSDisplayChannel::DrawSeen(int Current, int Total)
{
  if (lastCurrentPosition >= 0)
     return; // to not interfere with SetPositioner()
  int Seen = (Total > 0) ? min(xc07 - xc06, int((xc07 - xc06) * double(Current) / Total)) : 0;
  if (initial || Seen != lastSeen) {
     int y0 = yc11 - ShowSeenExtent;
     int y1 = yc11 + lineHeight / 2 - Gap / 2;
     osd->DrawRectangle(xc06, y0, xc06 + Seen - 1, y1 - 1, Theme.Color(clrSeen));
     osd->DrawRectangle(xc06 + Seen, y0, xc07 - 1, y1 - 1, Theme.Color(clrBackground));
     lastSeen = Seen;
     }
}

void cSkinLCARSDisplayChannel::DrawDevice(void)
{
  const cDevice *Device = cDevice::ActualDevice();
  if (DrawDeviceData(osd, Device, xc10, yc11, xc11, yc12, xs, tinyFont, lastDeviceType, lastCamSlot, Device->DeviceNumber() != lastDeviceNumber)) {
     lastDeviceNumber = Device->DeviceNumber();
     // Make sure signal meters are redrawn:
     lastSignalStrength = -1;
     lastSignalQuality = -1;
     lastSignalDisplay = 0;
     }
}

void cSkinLCARSDisplayChannel::DrawSignal(void)
{
  time_t Now = time(NULL);
  if (Now != lastSignalDisplay) {
     DrawDeviceSignal(osd, cDevice::ActualDevice(), xs + lineHeight / 2, yc11, xc11, yc12, lastSignalStrength, lastSignalQuality, initial);
     lastSignalDisplay = Now;
     }
}

void cSkinLCARSDisplayChannel::SetChannel(const cChannel *Channel, int Number)
{
  int x = xc13;
  int xi = x - SymbolSpacing -
           bmRecording.Width() - SymbolSpacing -
           bmEncrypted.Width() - SymbolSpacing -
           bmDolbyDigital.Width() - SymbolSpacing -
           bmAudio.Width() - SymbolSpacing -
           max(bmTeletext.Width(), bmRadio.Width()) - SymbolSpacing;
  osd->DrawRectangle(xi, yc00, xc13 - 1, yc01 - 1, frameColor);
  if (Channel && !Channel->GroupSep()) {
     bool rec = cRecordControls::Active();
     x -= bmRecording.Width() + SymbolSpacing;
     osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmRecording.Height()) / 2, bmRecording, Theme.Color(rec ? clrChannelSymbolRecFg : clrChannelSymbolOff), rec ? Theme.Color(clrChannelSymbolRecBg) : frameColor);
     x -= bmEncrypted.Width() + SymbolSpacing;
     osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmEncrypted.Height()) / 2, bmEncrypted, Theme.Color(Channel->Ca() ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     x -= bmDolbyDigital.Width() + SymbolSpacing;
     osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmDolbyDigital.Height()) / 2, bmDolbyDigital, Theme.Color(Channel->Dpid(0) ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     x -= bmAudio.Width() + SymbolSpacing;
     osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmAudio.Height()) / 2, bmAudio, Theme.Color(Channel->Apid(1) ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
     if (Channel->Vpid()) {
        x -= bmTeletext.Width() + SymbolSpacing;
        osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmTeletext.Height()) / 2, bmTeletext, Theme.Color(Channel->Tpid() ? clrChannelSymbolOn : clrChannelSymbolOff), frameColor);
        }
     else if (Channel->Apid(0)) {
        x -= bmRadio.Width() + SymbolSpacing;
        osd->DrawBitmap(x, yc00 + (yc01 - yc00 - bmRadio.Height()) / 2, bmRadio, Theme.Color(clrChannelSymbolOn), frameColor);
        }
     }
  cString ChNumber("");
  cString ChName("");
  if (Channel) {
     ChName = Channel->Name();
     if (!Channel->GroupSep())
        ChNumber = cString::sprintf("%d%s", Channel->Number(), Number ? "-" : "");
     }
  else if (Number)
     ChNumber = cString::sprintf("%d-", Number);
  else
     ChName = ChannelString(NULL, 0);
  osd->DrawText(xc00, yc00, ChNumber, Theme.Color(clrChannelFrameFg), frameColor, tallFont, xc02 - xc00, yc02 - yc00, taTop | taRight | taBorder);
  osd->DrawText(xc03, yc00, ChName, Theme.Color(clrChannelName), Theme.Color(clrBackground), tallFont, xi - xc03 - lineHeight, 0, taTop | taLeft);
  lastSignalDisplay = 0;
  if (withInfo) {
     if (Channel) {
        int x = xc00 + (yc10 - yc09); // compensate for the arc
        osd->DrawText(x, yc07, cSource::ToString(Channel->Source()), Theme.Color(clrChannelFrameFg), frameColor, cFont::GetFont(fontOsd), xc02 - x, yc10 - yc07, taTop | taRight | taBorder);
        }
     DrawDevice();
     }
}

void cSkinLCARSDisplayChannel::SetEvents(const cEvent *Present, const cEvent *Following)
{
  if (!withInfo)
     return;
  if (present != Present)
     lastSeen = -1;
  present = Present;
  for (int i = 0; i < 2; i++) {
      const cEvent *e = !i ? Present : Following;
      int y = !i ? yc03 : yc05;
      if (e) {
         osd->DrawText(xc00, y, e->GetTimeString(), Theme.Color(clrChannelFrameFg), frameColor, cFont::GetFont(fontOsd), xc02 - xc00, 0, taRight | taBorder);
         osd->DrawText(xc03, y, e->Title(), Theme.Color(clrEventTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd), xc13 - xc03);
         osd->DrawText(xc03, y + lineHeight, e->ShortText(), Theme.Color(clrEventShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml), xc13 - xc03);
         }
      else {
         osd->DrawRectangle(xc00, y, xc02 - 1, y + lineHeight, frameColor);
         osd->DrawRectangle(xc02, y, xc13 - 1, y + 2 * lineHeight, Theme.Color(clrBackground));
         }
      }
}

void cSkinLCARSDisplayChannel::SetMessage(eMessageType Type, const char *Text)
{
  if (Text) {
     int x0, x1, y0, y1, y2;
     if (withInfo) {
        x0 = xc06;
        x1 = xc13;
        y0 = yc11 - ShowSeenExtent;
        y1 = yc11;
        y2 = yc12;
        }
     else {
        x0 = xc03;
        x1 = xc13;
        y0 = y1 = yc00;
        y2 = yc02;
        }
     osd->SaveRegion(x0, y0, x1 - 1, y2 - 1);
     if (withInfo)
        osd->DrawRectangle(xc06, y0, xc07, y1 - 1, Theme.Color(clrBackground)); // clears the "seen" bar
     osd->DrawText(x0, y1, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), cFont::GetFont(fontSml), x1 - x0, y2 - y1, taCenter);
     message = true;
     }
  else {
     osd->RestoreRegion();
     message = false;
     }
}

void cSkinLCARSDisplayChannel::SetPositioner(const cPositioner *Positioner)
{
  if (Positioner) {
     int y0 = yc11 - ShowSeenExtent;
     int y1 = yc11 + lineHeight / 2 - Gap / 2;
     DrawDevicePosition(osd, Positioner, xc06, y0, xc07, y1, lastCurrentPosition);
     }
  else {
     lastCurrentPosition = -1;
     initial = true; // to have DrawSeen() refresh the progress bar
     }
  return;
}

void cSkinLCARSDisplayChannel::Flush(void)
{
  if (withInfo) {
     if (!message) {
        DrawDate();
        DrawTrack();
        DrawDevice();
        DrawSignal();
        int Current = 0;
        int Total = 0;
        if (present) {
           time_t t = time(NULL);
           if (t > present->StartTime())
              Current = t - present->StartTime();
           Total = present->Duration();
           }
        DrawSeen(Current, Total);
        }
     }
  osd->Flush();
  initial = false;
}

// --- cSkinLCARSDisplayMenu -------------------------------------------------

class cSkinLCARSDisplayMenu : public cSkinDisplayMenu {
private:
  cOsd *osd;
  int xa00, xa01, xa02, xa03, xa04, xa05, xa06, xa07, xa08, xa09;
  int yt00, yt01, yt02, yt03, yt04, yt05, yt06;
  int yc00, yc01, yc02, yc03, yc04, yc05, yc06, yc07, yc08, yc09, yc10, yc11;
  int yb00, yb01, yb02, yb03, yb04, yb05, yb06, yb07, yb08, yb09, yb10, yb11, yb12, yb13, yb14, yb15;
  int xm00, xm01, xm02, xm03, xm04, xm05, xm06, xm07, xm08;
  int ym00, ym01, ym02, ym03, ym04, ym05, ym06, ym07;
  int xs00, xs01, xs02, xs03, xs04, xs05, xs06, xs07, xs08, xs09, xs10, xs11, xs12, xs13;
  int ys00, ys01, ys02, ys03, ys04, ys05;
  int xi00, xi01, xi02, xi03;
  int yi00, yi01;
  int xb00, xb01, xb02, xb03, xb04, xb05, xb06, xb07, xb08, xb09, xb10, xb11, xb12, xb13, xb14, xb15;
  int xd00, xd01, xd02, xd03, xd04, xd05, xd06, xd07;
  int yd00, yd01, yd02, yd03, yd04, yd05;
  int xs; // starting column for signal display
  int lineHeight;
  cFont *tinyFont;
  cFont *tallFont;
  tColor frameColor;
  int currentIndex;
  cVector<int> deviceOffset;
  cVector<bool> deviceRecording;
  cString lastDeviceType[MAXDEVICES];
  cVector<cCamSlot *> lastCamSlot;
  cVector<int> lastSignalStrength;
  cVector<int> lastSignalQuality;
  bool initial;
  enum eCurrentMode { cmUnknown, cmLive, cmPlay };
  eCurrentMode lastMode;
  cString lastDate;
  int lastDiskUsageState;
  bool lastDiskAlert;
  double lastSystemLoad;
  cStateKey timersStateKey;
  time_t lastSignalDisplay;
  int lastLiveIndicatorY;
  bool lastLiveIndicatorTransferring;
  const cChannel *lastChannel;
  cString lastChannelName;
  const cEvent *lastEvent;
  const cRecording *lastRecording;
  cString lastHeader;
  int lastSeen;
  static cBitmap bmArrowUp, bmArrowDown, bmTransferMode;
  void DrawMainFrameUpper(tColor Color);
  void DrawMainFrameLower(void);
  void DrawMainButton(const char *Text, int x0, int x1, int x2, int x3, int y0, int y1, tColor ColorFg, tColor ColorBg, const cFont *Font);
  void DrawMenuFrame(void);
  void DrawMainBracket(void);
  void DrawStatusElbows(void);
  void DrawDate(void);
  void DrawDisk(void);
  void DrawLoad(void);
  void DrawFrameDisplay(void);
  void DrawScrollbar(int Total, int Offset, int Shown, bool CanScrollUp, bool CanScrollDown);
  void DrawTimer(const cTimer *Timer, int y, bool MultiRec);
  void DrawTimers(void);
  void DrawDevice(const cDevice *Device);
  void DrawDevices(void);
  void DrawLiveIndicator(void);
  void DrawSignals(void);
  void DrawLive(const cChannel *Channel);
  void DrawPlay(cControl *Control);
  void DrawInfo(const cEvent *Event, bool WithTime);
  void DrawSeen(int Current, int Total);
  void DrawTextScrollbar(void);
public:
  cSkinLCARSDisplayMenu(void);
  virtual ~cSkinLCARSDisplayMenu();
  virtual void Scroll(bool Up, bool Page);
  virtual int MaxItems(void);
  virtual void Clear(void);
  virtual void SetMenuCategory(eMenuCategory MenuCategory);
  virtual void SetTitle(const char *Title);
  virtual void SetButtons(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void SetItem(const char *Text, int Index, bool Current, bool Selectable);
  virtual void SetScrollbar(int Total, int Offset);
  virtual void SetEvent(const cEvent *Event);
  virtual void SetRecording(const cRecording *Recording);
  virtual void SetText(const char *Text, bool FixedFont);
  virtual int GetTextAreaWidth(void) const;
  virtual const cFont *GetTextAreaFont(bool FixedFont) const;
  virtual void Flush(void);
  };

cBitmap cSkinLCARSDisplayMenu::bmArrowUp(arrowup_xpm);
cBitmap cSkinLCARSDisplayMenu::bmArrowDown(arrowdown_xpm);
cBitmap cSkinLCARSDisplayMenu::bmTransferMode(play_xpm);

cSkinLCARSDisplayMenu::cSkinLCARSDisplayMenu(void)
{
  tallFont = cFont::CreateFont(Setup.FontOsd, Setup.FontOsdSize * 1.8);
  initial = true;
  lastMode = cmUnknown;
  lastChannel = NULL;
  lastEvent = NULL;
  lastRecording = NULL;
  lastSeen = -1;
  lastSignalDisplay = 0;
  lastLiveIndicatorY = -1;
  lastLiveIndicatorTransferring = false;
  lastDiskUsageState = -1;
  lastDiskAlert = false;
  lastSystemLoad = -1;
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  tinyFont = CreateTinyFont(lineHeight);
  frameColor = Theme.Color(clrMenuFrameBg);
  currentIndex = -1;
  // The outer frame:
  int d = 5 * lineHeight;
  xa00 = 0;
  xa01 = xa00 + d / 2;
  xa02 = xa00 + d;
  xa03 = xa02 + lineHeight;
  xa04 = xa02 + d / 4;
  xa05 = xa02 + d;
  xa06 = xa05 + Gap;
  xa09 = cOsd::OsdWidth();
  xa08 = xa09 - lineHeight;
  xa07 = xa08 - Gap;

  yt00 = 0;
  yt01 = yt00 + lineHeight;
  yt02 = yt01 + lineHeight;
  yt03 = yt01 + d / 4;
  yt04 = yt02 + Gap;
  yt05 = yt00 + d / 2;
  yt06 = yt04 + 2 * lineHeight;

  yc00 = yt06 + Gap;
  yc05 = yc00 + 3 * lineHeight + Gap / 2;
  yc04 = yc05 - lineHeight;
  yc03 = yc04 - lineHeight;
  yc02 = yc04 - d / 4;
  yc01 = yc05 - d / 2;

  yc06 = yc05 + Gap;
  yc07 = yc06 + lineHeight;
  yc08 = yc07 + lineHeight;
  yc09 = yc07 + d / 4;
  yc10 = yc06 + d / 2;
  yc11 = yc06 + 3 * lineHeight + Gap / 2;

  yb00 = yc11 + Gap;
  yb01 = yb00 + 2 * lineHeight;
  yb02 = yb01 + Gap;
  yb03 = yb02 + 2 * lineHeight;
  yb04 = yb03 + Gap;
  yb05 = yb04 + 2 * lineHeight;
  yb06 = yb05 + Gap;
  yb07 = yb06 + 2 * lineHeight;
  yb08 = yb07 + Gap;

  yb15 = cOsd::OsdHeight();
  yb14 = yb15 - lineHeight;
  yb13 = yb14 - lineHeight;
  yb12 = yb14 - d / 4;
  yb11 = yb15 - d / 2;
  yb10 = yb13 - Gap - 2 * lineHeight;
  yb09 = yb10 - Gap;

  // Compensate for large font size:
  if (yb09 - yb08 < 2 * lineHeight) {
     yb08 = yb06;
     yb06 = 0; // drop empty rectangle
     }
  if (yb09 - yb08 < 2 * lineHeight) {
     yb05 = yb09;
     yb08 = 0; // drop "LCARS" display
     }
  if (yb05 - yb04 < 2 * lineHeight) {
     yb03 = yb09;
     yb04 = 0; // drop "LOAD" display
     }
  if (yb03 - yb02 < 2 * lineHeight) {
     yb01 = yb09;
     yb02 = 0; // drop "DISK" display
     }
  // Anything else is just insanely large...

  // The main command menu:
  xm00 = xa03;
  xm01 = xa05;
  xm02 = xa06;
  xm08 = (xa09 + xa00) / 2;
  xm07 = xm08 - lineHeight;
  xm06 = xm07 - lineHeight / 2;
  xm05 = xm06 - lineHeight / 2;
  xm04 = xm05 - lineHeight;
  xm03 = xm04 - Gap;
  ym00 = yc08;
  ym01 = ym00 + lineHeight / 2;
  ym02 = ym01 + lineHeight / 2;
  ym03 = ym02 + Gap;
  ym07 = yb15;
  ym06 = ym07 - lineHeight / 2;
  ym05 = ym06 - lineHeight / 2;
  ym04 = ym05 - Gap;

  // The status area:
  xs00 = xm08 + Gap + lineHeight + Gap;
  xs13 = xa09;
  xs12 = xa08;
  xs11 = xa07;
  xs05 = (xs00 + xs11 + Gap) / 2;
  xs04 = xs05 - lineHeight / 2;
  xs03 = xs04 - lineHeight / 2;
  xs02 = xs03 - 2 * lineHeight;
  xs01 = xs02 - Gap;
  xs06 = xs05 + Gap;
  xs07 = xs06 + lineHeight / 2;
  xs08 = xs07 + lineHeight / 2;
  xs09 = xs08 + 2 * lineHeight;
  xs10 = xs09 + Gap;
  ys00 = yc06;
  ys01 = ys00 + lineHeight;
  ys02 = ys01 + lineHeight / 2;
  ys04 = ys01 + lineHeight;
  ys03 = ys04 - Gap;
  ys05 = yb15;

  // The item area (just to have them initialized, actual setting will be done in SetMenuCategory():

  xi00 = 0;
  xi01 = 0;
  xi02 = 0;
  xi03 = 1;
  yi00 = 0;
  yi01 = 1;

  // The color buttons in submenus:
  xb00 = xa06;
  xb15 = xa07;
  int w = (xa08 - xa06) / 4;
  xb01 = xb00 + lineHeight / 2;
  xb02 = xb01 + Gap;
  xb04 = xb00 + w;
  xb03 = xb04 - Gap;
  xb05 = xb04 + lineHeight / 2;
  xb06 = xb05 + Gap;
  xb08 = xb04 + w;
  xb07 = xb08 - Gap;
  xb09 = xb08 + lineHeight / 2;
  xb10 = xb09 + Gap;
  xb12 = xb08 + w;
  xb11 = xb12 - Gap;
  xb13 = xb12 + lineHeight / 2;
  xb14 = xb13 + Gap;

  // The color buttons in the main menu:
  int r = lineHeight;
  xd07 = xa09;
  xd06 = xd07 - r;
  xd05 = xd06 - 4 * r;
  xd04 = xd05 - r;
  xd03 = xd04 - Gap;
  xd02 = xd03 - r;
  xd01 = xd02 - 4 * r;
  xd00 = xd01 - r;
  yd00 = yt00;
  yd05 = yc04 - Gap;
  yd04 = yd05 - 2 * r;
  yd03 = yd04 - Gap;
  yd02 = yd03 - 2 * r;
  yd01 = yd02 - Gap;

  xs = 0;

  osd = CreateOsd(cOsd::OsdLeft(), cOsd::OsdTop(), xa00, yt00, xa09 - 1, yb15 - 1);
}

cSkinLCARSDisplayMenu::~cSkinLCARSDisplayMenu()
{
  delete tallFont;
  delete tinyFont;
  delete osd;
}

void cSkinLCARSDisplayMenu::SetMenuCategory(eMenuCategory MenuCategory)
{
  if (initial || MenuCategory != cSkinDisplayMenu::MenuCategory()) {
     cSkinDisplayMenu::SetMenuCategory(MenuCategory);
     initial = true;
     osd->DrawRectangle(xa00, yt00, xa09 - 1, yb15 - 1, Theme.Color(clrBackground));
     if (MenuCategory == mcMain) {
        yi00 = ym03;
        yi01 = ym04;
        xi00 = xm00;
        xi01 = xm03;
        xi02 = xm04;
        xi03 = xm05;
        timersStateKey.Reset();
        DrawMainFrameLower();
        DrawMainBracket();
        DrawStatusElbows();
        }
     else {
        yi00 = yt02;
        yi01 = yb13;
        xi00 = xa03;
        xi01 = xa07;
        xi02 = xa08;
        xi03 = xa09;
        DrawMenuFrame();
        }
     }
}

void cSkinLCARSDisplayMenu::DrawMainFrameUpper(tColor Color)
{
  // Top left rectangles:
  osd->DrawRectangle(xa00, yt00, xa02 - 1, yt02 - 1, Color);
  osd->DrawRectangle(xa00, yt04, xa02 - 1, yt06 - 1, Color);
  // Upper elbow:
  osd->DrawRectangle(xa00, yc00, xa01 - 1, yc01 - 1, Color);
  osd->DrawEllipse  (xa00, yc01, xa01 - 1, yc05 - 1, Color, 3);
  osd->DrawRectangle(xa01, yc00, xa02 - 1, yc05 - 1, Color);
  osd->DrawEllipse  (xa02, yc02, xa04 - 1, yc04 - 1, Color, -3);
  osd->DrawRectangle(xa02, yc04, xa05 - 1, yc05 - 1, Color);
  // Upper delimiter:
  osd->DrawRectangle(xa06, yc04 + lineHeight / 2, xm08 - 1, yc05 - 1, Color);
  osd->DrawRectangle(xm08 + Gap, yc04, xs00 - Gap - 1, yc05 - 1, Color);
  osd->DrawRectangle(xs00, yc04, xs05 - 1, yc05 - 1, Color);
  osd->DrawRectangle(xs06, yc04, xa07 - 1, yc05 - 1, Color);
  osd->DrawRectangle(xa08, yc04, xa09 - 1, yc05 - 1, Color);
}

void cSkinLCARSDisplayMenu::DrawMainFrameLower(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  // Lower elbow:
  osd->DrawRectangle(xa00, yc10, xa01 - 1, yc11 - 1, frameColor);
  osd->DrawEllipse  (xa00, yc06, xa01 - 1, yc10 - 1, frameColor, 2);
  osd->DrawRectangle(xa01, yc06, xa02 - 1, yc11 - 1, frameColor);
  osd->DrawEllipse  (xa02, yc07, xa04 - 1, yc09 - 1, frameColor, -2);
  osd->DrawRectangle(xa02, yc06, xa05 - 1, yc07 - 1, frameColor);
  // Lower delimiter:
  osd->DrawRectangle(xa06, yc06, xm08 - 1, yc07 - lineHeight / 2 - 1, frameColor);
  osd->DrawRectangle(xm08 + Gap, yc06, xs00 - Gap - 1, yc07 - 1, frameColor);
  osd->DrawRectangle(xa08, yc06, xa09 - 1, yc07 - 1, frameColor);
  // VDR version:
  osd->DrawRectangle(xa00, yb10, xa02 - 1, yb15 - 1, frameColor);
  osd->DrawText(xa00, yb10, "VDR", Theme.Color(clrMenuFrameFg), frameColor, tallFont, xa02 - xa00, yb11 - yb10, taTop | taRight | taBorder);
  osd->DrawText(xa00, yb15 - lineHeight, VDRVERSION, Theme.Color(clrMenuFrameFg), frameColor, font, xa02 - xa00, lineHeight, taBottom | taRight | taBorder);
}

void cSkinLCARSDisplayMenu::DrawMainButton(const char *Text, int x0, int x1, int x2, int x3, int y0, int y1, tColor ColorFg, tColor ColorBg, const cFont *Font)
{
  int h = y1 - y0;
  osd->DrawEllipse(x0, y0, x1 - 1, y1 - 1, ColorBg, 7);
  osd->DrawText(x1, y0, Text, ColorFg, ColorBg, Font, x2 - x1, h, taBottom | taRight);
  osd->DrawEllipse(x2, y0, x3 - 1, y1 - 1, ColorBg, 5);
}

void cSkinLCARSDisplayMenu::DrawMenuFrame(void)
{
  // Upper elbow:
  osd->DrawRectangle(xa00, yt05, xa01 - 1, yt06 - 1, frameColor);
  osd->DrawRectangle(xa00, yt00, xa01 - 1, yt05 - 1, clrTransparent);
  osd->DrawEllipse  (xa00, yt00, xa01 - 1, yt05 - 1, frameColor, 2);
  osd->DrawRectangle(xa01, yt00, xa02 - 1, yt06 - 1, frameColor);
  osd->DrawEllipse  (xa02, yt01, xa04 - 1, yt03 - 1, frameColor, -2);
  osd->DrawRectangle(xa02, yt00, xa05 - 1, yt01 - 1, frameColor);
  osd->DrawRectangle(xa06, yt00, xa07 - 1, yt01 - 1, frameColor);
  osd->DrawRectangle(xa08, yt00, xa08 + lineHeight / 2 - 1, yt01 - 1, frameColor);
  osd->DrawRectangle(xa08 + lineHeight / 2, yt00, xa09 - 1, yt00 + lineHeight / 2 - 1, clrTransparent);
  osd->DrawEllipse  (xa08 + lineHeight / 2, yt00, xa09 - 1, yt01 - 1, frameColor, 5);
  // Center part:
  osd->DrawRectangle(xa00, yc00, xa02 - 1, yc11 - 1, frameColor);
  // Lower elbow:
  osd->DrawRectangle(xa00, yb10, xa02 - 1, yb11 - 1, frameColor);
  osd->DrawRectangle(xa00, yb11, xa01 - 1, yb15 - 1, clrTransparent);
  osd->DrawEllipse  (xa00, yb11, xa01 - 1, yb15 - 1, frameColor, 3);
  osd->DrawRectangle(xa01, yb11, xa02 - 1, yb15 - 1, frameColor);
  osd->DrawEllipse  (xa02, yb12, xa04 - 1, yb14 - 1, frameColor, -3);
  osd->DrawRectangle(xa02, yb14, xa05 - 1, yb15 - 1, frameColor);
  osd->DrawRectangle(xa08, yb14, xa08 + lineHeight / 2 - 1, yb15 - 1, frameColor);
  osd->DrawRectangle(xa08 + lineHeight / 2, yb14 + lineHeight / 2, xa09 - 1, yb15 - 1, clrTransparent);
  osd->DrawEllipse  (xa08 + lineHeight / 2, yb14, xa09 - 1, yb15 - 1, frameColor, 5);
  osd->DrawText(xa00, yb10, "VDR", Theme.Color(clrMenuFrameFg), frameColor, tallFont, xa02 - xa00, yb11 - yb10, taTop | taRight | taBorder);
  // Color buttons:
  tColor lutBg[] = { clrButtonRedBg, clrButtonGreenBg, clrButtonYellowBg, clrButtonBlueBg };
  osd->DrawRectangle(xb00, yb14, xb01 - 1, yb15 - 1, Theme.Color(lutBg[Setup.ColorKey0]));
  osd->DrawRectangle(xb04, yb14, xb05 - 1, yb15 - 1, Theme.Color(lutBg[Setup.ColorKey1]));
  osd->DrawRectangle(xb08, yb14, xb09 - 1, yb15 - 1, Theme.Color(lutBg[Setup.ColorKey2]));
  osd->DrawRectangle(xb12, yb14, xb13 - 1, yb15 - 1, Theme.Color(lutBg[Setup.ColorKey3]));
}

void cSkinLCARSDisplayMenu::DrawDate(void)
{
  cString s = DayDateTime();
  if (initial || !*lastDate || strcmp(s, lastDate)) {
     const cFont *font = cFont::GetFont(fontOsd);
     tColor ColorFg = Theme.Color(clrDateFg);
     tColor ColorBg = Theme.Color(clrDateBg);
     lastDate = s;
     const char *t = strrchr(s, ' ');
     osd->DrawText(xa00, yb01 - lineHeight, t, ColorFg, ColorBg, font, xa02 - xa00, lineHeight, taBottom | taRight | taBorder);
     s.Truncate(t - s);
     osd->DrawText(xa00, yb00, s, ColorFg, ColorBg, font, xa02 - xa00, yb01 - yb00 - lineHeight, taTop | taRight | taBorder);
     }
}

void cSkinLCARSDisplayMenu::DrawDisk(void)
{
  if (yb02) {
     if (cVideoDiskUsage::HasChanged(lastDiskUsageState) || initial) { // must call HasChanged() first, or it shows an outdated value in the 'initial' case!
        const cFont *font = cFont::GetFont(fontOsd);
        int DiskUsage = cVideoDiskUsage::UsedPercent();
        bool DiskAlert = DiskUsage > DISKUSAGEALERTLIMIT;
        tColor ColorFg = DiskAlert ? Theme.Color(clrAlertFg) : Theme.Color(clrMenuFrameFg);
        tColor ColorBg = DiskAlert ? Theme.Color(clrAlertBg) : frameColor;
        if (initial || DiskAlert != lastDiskAlert)
           osd->DrawText(xa00, yb02, tr("DISK"), ColorFg, ColorBg, tinyFont, xa02 - xa00, yb03 - yb02, taTop | taLeft | taBorder);
        osd->DrawText(xa01, yb02, itoa(DiskUsage), ColorFg, ColorBg, font, xa02 - xa01, lineHeight, taBottom | taRight | taBorder);
        osd->DrawText(xa00, yb03 - lineHeight, cString::sprintf("%02d:%02d", cVideoDiskUsage::FreeMinutes() / 60, cVideoDiskUsage::FreeMinutes() % 60), ColorFg, ColorBg, font, xa02 - xa00, 0, taBottom | taRight | taBorder);
        lastDiskAlert = DiskAlert;
        }
     }
}

void cSkinLCARSDisplayMenu::DrawLoad(void)
{
  if (yb04) {
     tColor ColorFg = Theme.Color(clrMenuFrameFg);
     tColor ColorBg = frameColor;
     if (initial)
        osd->DrawText(xa00, yb04, tr("LOAD"), ColorFg, ColorBg, tinyFont, xa02 - xa00, yb05 - yb04, taTop | taLeft | taBorder);
     double SystemLoad;
     if (getloadavg(&SystemLoad, 1) > 0) {
        if (initial || SystemLoad != lastSystemLoad) {
           osd->DrawText(xa00, yb05 - lineHeight, cString::sprintf("%.1f", SystemLoad), ColorFg, ColorBg, cFont::GetFont(fontOsd), xa02 - xa00, lineHeight, taBottom | taRight | taBorder);
           lastSystemLoad = SystemLoad;
           }
        }
     }
}

void cSkinLCARSDisplayMenu::DrawMainBracket(void)
{
  tColor Color = Theme.Color(clrMenuMainBracket);
  osd->DrawRectangle(xm00, ym00, xm01 - 1, ym01 - 1, Color);
  osd->DrawRectangle(xm02, ym00, xm07 - 1, ym01 - 1, Color);
  osd->DrawEllipse  (xm07, ym00, xm08 - 1, ym02 - 1, Color, 1);
  osd->DrawEllipse  (xm06, ym01, xm07 - 1, ym02 - 1, Color, -1);
  osd->DrawRectangle(xm07, ym03, xm08 - 1, ym04 - 1, Color);
  osd->DrawEllipse  (xm06, ym05, xm07 - 1, ym06 - 1, Color, -4);
  osd->DrawEllipse  (xm07, ym05, xm08 - 1, ym07 - 1, Color, 4);
  osd->DrawRectangle(xm02, ym06, xm07 - 1, ym07 - 1, Color);
  osd->DrawRectangle(xm00, ym06, xm01 - 1, ym07 - 1, Color);
}

void cSkinLCARSDisplayMenu::DrawStatusElbows(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText     (xs00, ys00, tr("TIMERS"), Theme.Color(clrMenuFrameFg), frameColor, font, xs01 - xs00, lineHeight, taBottom | taLeft | taBorder);
  osd->DrawRectangle(xs02, ys00, xs03 - 1, ys01 - 1, frameColor);
  osd->DrawEllipse  (xs03, ys00, xs05 - 1, ys01 - 1, frameColor, 1);
  osd->DrawEllipse  (xs03, ys01, xs04 - 1, ys02 - 1, frameColor, -1);
  osd->DrawRectangle(xs04, ys01, xs05 - 1, ys03 - 1, frameColor);
  osd->DrawRectangle(xs04, ys04, xs05 - 1, ys05 - 1, frameColor);
  osd->DrawText     (xs10, ys00, tr("DEVICES"), Theme.Color(clrMenuFrameFg), frameColor, font, xs11 - xs10, lineHeight, taBottom | taRight | taBorder);
  osd->DrawRectangle(xs08, ys00, xs09 - 1, ys01 - 1, frameColor);
  osd->DrawEllipse  (xs06, ys00, xs08 - 1, ys01 - 1, frameColor, 2);
  osd->DrawEllipse  (xs07, ys01, xs08 - 1, ys02 - 1, frameColor, -2);
  osd->DrawRectangle(xs06, ys01, xs07 - 1, ys03 - 1, frameColor);
  osd->DrawRectangle(xs06, ys04, xs07 - 1, ys05 - 1, frameColor);
  osd->DrawRectangle(xs12, ys00, xs13 - 1, ys01 - 1, frameColor);
}

void cSkinLCARSDisplayMenu::DrawFrameDisplay(void)
{
  DrawDate();
  DrawDisk();
  DrawLoad();
  if (initial) {
     if (yb06)
        osd->DrawRectangle(xa00, yb06, xa02 - 1, yb07 - 1, frameColor);
     if (yb08) {
        const cFont *font = cFont::GetFont(fontOsd);
        osd->DrawRectangle(xa00, yb08, xa02 - 1, yb09 - 1, frameColor);
        osd->DrawText(xa00, yb09 - lineHeight, "LCARS", Theme.Color(clrMenuFrameFg), frameColor, font, xa02 - xa00, lineHeight, taBottom | taRight | taBorder);
        }
     }
}

void cSkinLCARSDisplayMenu::DrawScrollbar(int Total, int Offset, int Shown, bool CanScrollUp, bool CanScrollDown)
{
  int x0, x1, tt, tb;
  tColor ClearColor;
  if (MenuCategory() == mcMain) {
     x0 = xm07;
     x1 = xm08;
     tt = ym03;
     tb = ym04;
     ClearColor = Theme.Color(clrMenuMainBracket);
     }
  else {
     x0 = xa02 + Gap;
     x1 = x0 + lineHeight / 2;
     tt = yc00;
     tb = yc11;
     ClearColor = Theme.Color(clrBackground);
     int d = TextFrame;
     if (CanScrollUp)
        osd->DrawBitmap(xa02 - bmArrowUp.Width() - d, yc00 + d, bmArrowUp, Theme.Color(clrMenuScrollbarArrow), frameColor);
     else
        osd->DrawRectangle(xa02 - bmArrowUp.Width() - d, yc00 + d, xa02 - d - 1, yc00 + d + bmArrowUp.Height() - 1, frameColor);
     if (CanScrollDown)
        osd->DrawBitmap(xa02 - bmArrowDown.Width() - d, yc11 - d - bmArrowDown.Height(), bmArrowDown, Theme.Color(clrMenuScrollbarArrow), frameColor);
     else
        osd->DrawRectangle(xa02 - bmArrowDown.Width() - d, yc11 - d - bmArrowDown.Height(), xa02 - d - 1, yc11 - d - 1, frameColor);
     }
  if (Total > 0 && Total > Shown) {
     int sw = x1 - x0;
     int sh = max(int((tb - tt) * double(Shown) / Total + 0.5), sw);
     int st = min(int(tt + (tb - tt) * double(Offset) / Total + 0.5), tb - sh);
     int sb = min(st + sh, tb);
     osd->DrawRectangle(x0, tt, x1 - 1, tb - 1, Theme.Color(clrMenuScrollbarTotal));
     osd->DrawRectangle(x0, st, x1 - 1, sb - 1, Theme.Color(clrMenuScrollbarShown));
     }
  else if (MenuCategory() != mcMain)
     osd->DrawRectangle(x0, tt, x1 - 1, tb - 1, ClearColor);
}

void cSkinLCARSDisplayMenu::DrawTimer(const cTimer *Timer, int y, bool MultiRec)
{
  // The timer data:
  bool Alert = !Timer->Recording() && Timer->Pending();
  tColor ColorFg = Alert ? Theme.Color(clrAlertFg) : Theme.Color(clrTimerFg);
  tColor ColorBg = Alert ? Theme.Color(clrAlertBg) : Theme.Color(clrTimerBg);
  osd->DrawRectangle(xs00, y, xs03 - 1, y + lineHeight - 1, ColorBg);
  cString Date;
  if (Timer->Recording())
     Date = cString::sprintf("-%s", *TimeString(Timer->StopTimeEvent()));
  else {
     time_t Now = time(NULL);
     time_t StartTime = Timer->StartTimeEvent();
     cString Today = WeekDayName(Now);
     cString Time = TimeString(StartTime);
     cString Day = WeekDayName(StartTime);
     if (StartTime > Now + 6 * SECSINDAY)
        Date = DayDateTime(StartTime);
     else if (strcmp(Day, Today) != 0)
        Date = cString::sprintf("%s %s", *Day, *Time);
     else
        Date = Time;
     }
  if (Timer->Flags() & tfVps)
     Date = cString::sprintf("VPS %s", *Date);
  const cChannel *Channel = Timer->Channel();
  const cEvent *Event = Timer->Event();
  int d = max(TextFrame / 2, 1);
  if (Channel) {
     osd->DrawText(xs00 + d, y, Channel->Name(), ColorFg, ColorBg, tinyFont, xs03 - xs00 - d);
     osd->DrawText(xs03 - tinyFont->Width(Date) - d, y, Date, ColorFg, ColorBg, tinyFont);
     }
  if (Event)
     osd->DrawText(xs00 + d, y + lineHeight - tinyFont->Height(), Event->Title(), ColorFg, ColorBg, tinyFont, xs03 - xs00 - 2 * d);
  // The remote timer indicator:
  if (Timer->Remote())
     osd->DrawRectangle(xs00 - (lineHeight - Gap) / 2, y, xs00 - Gap - 1, y + lineHeight - 1, Timer->Recording() ? Theme.Color(clrMenuTimerRecording) : ColorBg);
  // The timer recording indicator:
  else if (Timer->Recording())
     osd->DrawRectangle(xs03 + Gap, y - (MultiRec ? Gap : 0), xs04 - Gap / 2 - 1, y + lineHeight - 1, Theme.Color(clrMenuTimerRecording));
}

void cSkinLCARSDisplayMenu::DrawTimers(void)
{
  if (const cTimers *Timers = cTimers::GetTimersRead(timersStateKey)) {
     deviceRecording.Clear();
     const cFont *font = cFont::GetFont(fontOsd);
     osd->DrawRectangle(xs00 - (lineHeight - Gap) / 2, ys04, xs04 - 1, ys05 - 1, Theme.Color(clrBackground));
     osd->DrawRectangle(xs07, ys04, xs13 - 1, ys05 - 1, Theme.Color(clrBackground));
     cSortedTimers SortedTimers(Timers);
     cVector<int> FreeDeviceSlots;
     int NumDevices = 0;
     int y = ys04;
     // Timers and recording devices:
     while (1) {
           int NumTimers = 0;
           const cDevice *Device = NULL;
           for (int i = 0; i < SortedTimers.Size(); i++) {
               if (y + lineHeight > ys05)
                  break;
               if (const cTimer *Timer = SortedTimers[i]) {
                  if (Timer->IsPatternTimer())
                     SortedTimers[i] = NULL;
                  else if (Timer->Recording()) {
                     if (Timer->Remote()) {
                        if (!Device && Timer->HasFlags(tfActive)) {
                           DrawTimer(Timer, y, false);
                           FreeDeviceSlots.Append(y);
                           y += lineHeight + Gap;
                           }
                        else
                           continue;
                        }
                     else if (cRecordControl *RecordControl = cRecordControls::GetRecordControl(Timer)) {
                        if (!Device || Device == RecordControl->Device()) {
                           DrawTimer(Timer, y, NumTimers > 0);
                           NumTimers++;
                           if (!Device) {
                              Device = RecordControl->Device();
                              deviceOffset[Device->DeviceNumber()] = y;
                              deviceRecording[Device->DeviceNumber()] = true;
                              NumDevices++;
                              }
                           else
                              FreeDeviceSlots.Append(y);
                           y += lineHeight + Gap;
                           }
                        else
                           continue;
                        }
                     SortedTimers[i] = NULL;
                     }
                  else if (!Device && Timer->HasFlags(tfActive)) {
                     DrawTimer(Timer, y, false);
                     FreeDeviceSlots.Append(y);
                     y += lineHeight + Gap;
                     SortedTimers[i] = NULL;
                     }
                  }
               }
           if (!Device)
              break;
           }
     // Devices currently not recording:
     int Slot = 0;
     for (int i = 0; i < cDevice::NumDevices(); i++) {
         if (const cDevice *Device = cDevice::GetDevice(i)) {
            if (Device->NumProvidedSystems()) {
               if (!deviceRecording[Device->DeviceNumber()]) {
                  if (Slot < FreeDeviceSlots.Size()) {
                     y = FreeDeviceSlots[Slot];
                     Slot++;
                     }
                  if (y + lineHeight > ys05)
                     break;
                  deviceOffset[Device->DeviceNumber()] = y;
                  y += lineHeight + Gap;
                  NumDevices++;
                  }
               }
            }
         }
     // Total number of active timers:
     int NumTimers = 0;
     for (const cTimer *Timer = Timers->First(); Timer; Timer = Timers->Next(Timer)) {
         if (Timer->HasFlags(tfActive))
            NumTimers++;
         }
     osd->DrawText(xs02, ys00, itoa(NumTimers), Theme.Color(clrMenuFrameFg), frameColor, font, xs03 - xs02, ys01 - ys00, taBottom | taLeft | taBorder);
     osd->DrawText(xs08, ys00, itoa(NumDevices), Theme.Color(clrMenuFrameFg), frameColor, font, xs09 - xs08, ys01 - ys00, taBottom | taRight | taBorder);
     lastSignalDisplay = 0;
     initial = true; // forces redrawing of devices
     timersStateKey.Remove();
     }
}

void cSkinLCARSDisplayMenu::DrawDevice(const cDevice *Device)
{
  int dn = Device->DeviceNumber();
  int y = deviceOffset[dn];
  if (y + lineHeight <= ys05) {
     if (DrawDeviceData(osd, Device, xs08, y, xs11, y + lineHeight, xs, tinyFont, lastDeviceType[dn], lastCamSlot[dn], initial)) {
        // Make sure signal meters are redrawn:
        lastSignalStrength[dn] = -1;
        lastSignalQuality[dn] = -1;
        lastSignalDisplay = 0;
        }
     // The device recording indicator:
     if (deviceRecording[dn])
        osd->DrawRectangle(xs07 + Gap / 2, y, xs08 - Gap - 1, y + lineHeight - 1, Theme.Color(clrMenuDeviceRecording));
     }
}

void cSkinLCARSDisplayMenu::DrawDevices(void)
{
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      if (const cDevice *Device = cDevice::GetDevice(i)) {
         if (Device->NumProvidedSystems())
            DrawDevice(Device);
         }
      }
}

void cSkinLCARSDisplayMenu::DrawLiveIndicator(void)
{
  cDevice *Device = cDevice::PrimaryDevice();
  int y = -1;
  bool Transferring = Device->Transferring();
  if (!Device->Replaying() || Transferring)
     y = deviceOffset[cDevice::ActualDevice()->DeviceNumber()];
  if (initial || y != lastLiveIndicatorY || Transferring != lastLiveIndicatorTransferring) {
     if (lastLiveIndicatorY >= 0)
        osd->DrawRectangle(xs12, lastLiveIndicatorY, xs13 - 1, lastLiveIndicatorY + lineHeight - 1, Theme.Color(clrBackground));
     if (y > 0) {
        tColor ColorBg = Theme.Color(clrChannelFrameBg);
        osd->DrawRectangle(xs12, y, xs12 + lineHeight / 2 - 1, y + lineHeight - 1, ColorBg);
        osd->DrawEllipse  (xs12 + lineHeight / 2, y, xs13 - 1, y + lineHeight - 1, ColorBg, 5);
        if (Transferring) {
           int w = bmTransferMode.Width();
           int h = bmTransferMode.Height();
           int b = w * w + h * h; // the diagonal of the bitmap (squared)
           int c = lineHeight * lineHeight; // the diameter of the circle (squared)
           const cBitmap *bm = &bmTransferMode;
           if (b > c) {
              // the bitmap doesn't fit, so scale it down:
              double f = sqrt(double(c) / (2 * b));
              bm = bmTransferMode.Scaled(f, f);
              }
           osd->DrawBitmap((xs12 + xs13 - bm->Width()) / 2, y + (lineHeight - bm->Height()) / 2, *bm, Theme.Color(clrChannelFrameFg), ColorBg);
           if (bm != &bmTransferMode)
              delete bm;
           }
        }
     lastLiveIndicatorY = y;
     lastLiveIndicatorTransferring = Transferring;
     }
}

void cSkinLCARSDisplayMenu::DrawSignals(void)
{
  time_t Now = time(NULL);
  if (initial || Now - lastSignalDisplay >= SIGNALDISPLAYDELTA) {
     for (int i = 0; i < cDevice::NumDevices(); i++) {
         if (const cDevice *Device = cDevice::GetDevice(i)) {
            if (Device->NumProvidedSystems()) {
               if (int y = deviceOffset[i])
                  DrawDeviceSignal(osd, Device, xs + lineHeight / 2, y, xs11, y + lineHeight, lastSignalStrength[i], lastSignalQuality[i], initial);
               }
            }
         }
     lastSignalDisplay = Now;
     }
}

void cSkinLCARSDisplayMenu::DrawLive(const cChannel *Channel)
{
  if (lastMode != cmLive) {
     initial = true;
     lastMode = cmLive;
     }
  if (initial) {
     DrawMainFrameUpper(Theme.Color(clrChannelFrameBg));
     osd->DrawText(xd00, yd00, tr("LIVE"), Theme.Color(clrChannelFrameBg), Theme.Color(clrBackground), tallFont, xd07 - xd00, yd01 - yd00, taTop | taRight | taBorder);
     }
  if (!Channel)
     return;
  if (initial || Channel != lastChannel || strcmp(Channel->Name(), lastChannelName)) {
     osd->DrawText(xa00, yt00, itoa(Channel->Number()), Theme.Color(clrChannelFrameFg), Theme.Color(clrChannelFrameBg), tallFont, xa02 - xa00, yt02 - yt00, taTop | taRight | taBorder);
     osd->DrawText(xa03, yt00, Channel->Name(), Theme.Color(clrChannelName), Theme.Color(clrBackground), tallFont, xd00 - xa03, yd01 - yd00, taTop | taLeft);
     int x = xa00 + (yc03 - yc02); // compensate for the arc
     osd->DrawText(x, yc00, cSource::ToString(Channel->Source()), Theme.Color(clrChannelFrameFg), Theme.Color(clrChannelFrameBg), cFont::GetFont(fontOsd), xa02 - x, yc03 - yc00, taTop | taRight | taBorder);
     lastChannel = Channel;
     lastChannelName = Channel->Name();
     DrawSeen(0, 0);
     }
  // The current programme:
  LOCK_SCHEDULES_READ;
  if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
     const cEvent *Event = Schedule->GetPresentEvent();
     if (initial || Event != lastEvent) {
        DrawInfo(Event, true);
        lastEvent = Event;
        lastSeen = -1;
        }
     int Current = 0;
     int Total = 0;
     if (Event) {
        time_t t = time(NULL);
        if (t > Event->StartTime())
           Current = t - Event->StartTime();
        Total = Event->Duration();
        }
     DrawSeen(Current, Total);
     }
}

void cSkinLCARSDisplayMenu::DrawPlay(cControl *Control)
{
  if (lastMode != cmPlay) {
     initial = true;
     lastMode = cmPlay;
     }
  if (initial) {
     DrawMainFrameUpper(Theme.Color(clrReplayFrameBg));
     osd->DrawText(xd00, yd00, tr("PLAY"), Theme.Color(clrReplayFrameBg), Theme.Color(clrBackground), tallFont, xd07 - xd00, yd01 - yd00, taTop | taRight | taBorder);
     }
  // The current progress:
  int Current = 0;
  int Total = 0;
  if (Control->GetIndex(Current, Total))
     DrawSeen(Current, Total);
  // The current programme:
  if (const cRecording *Recording = Control->GetRecording()) {
     if (initial || Recording != lastRecording) {
        const cFont *font = cFont::GetFont(fontOsd);
        if (const cRecordingInfo *Info = Recording->Info()) {
           osd->DrawText(xa03, yt00, Info->ChannelName(), Theme.Color(clrChannelName), Theme.Color(clrBackground), tallFont, xd00 - xa03, yd01 - yd00, taTop | taLeft);
           DrawInfo(Info->GetEvent(), false);
           }
        else
           osd->DrawText(xa03, yt04, Recording->Name(), Theme.Color(clrEventTitle), Theme.Color(clrBackground), font, xd00 - xa03, 0, taTop | taLeft);
        osd->DrawText(xa00, yt04, ShortDateString(Recording->Start()), Theme.Color(clrReplayFrameFg), Theme.Color(clrReplayFrameBg), font, xa02 - xa00, 0, taTop | taRight | taBorder);
        osd->DrawText(xa00, yt06 - lineHeight, TimeString(Recording->Start()), Theme.Color(clrReplayFrameFg), Theme.Color(clrReplayFrameBg), font, xa02 - xa00, 0, taBottom | taRight | taBorder);
        lastRecording = Recording;
        }
     }
  else {
     cString Header = Control->GetHeader();
     if (!*lastHeader || strcmp(Header, lastHeader)) {
        osd->DrawText(xa03, yt00, Header, Theme.Color(clrMenuText), Theme.Color(clrBackground), tallFont, xd00 - xa03, yd01 - yd00, taTop | taLeft);
        lastHeader = Header;
        }
     }
}

void cSkinLCARSDisplayMenu::DrawInfo(const cEvent *Event, bool WithTime)
{
  if (Event) {
     const cFont *font = cFont::GetFont(fontOsd);
     int y = yt04;
     osd->DrawText(xa03, y, Event->Title(), Theme.Color(clrEventTitle), Theme.Color(clrBackground), font, xd00 - xa03 - lineHeight, lineHeight, taBottom | taLeft);
     y += lineHeight;
     osd->DrawText(xa03, y, Event->ShortText(), Theme.Color(clrEventShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml), xd00 - xa03 - lineHeight, lineHeight, taTop | taLeft);
     if (WithTime) {
        osd->DrawText(xa00, yt04, Event->GetTimeString(), Theme.Color(clrChannelFrameFg), Theme.Color(clrChannelFrameBg), font, xa02 - xa00, lineHeight, taTop | taRight | taBorder);
        osd->DrawText(xa00, yt06 - lineHeight, cString::sprintf("-%s", *Event->GetEndTimeString()), Theme.Color(clrChannelFrameFg), Theme.Color(clrChannelFrameBg), font, xa02 - xa00, lineHeight, taBottom | taRight | taBorder);
        }
     }
}

void cSkinLCARSDisplayMenu::DrawSeen(int Current, int Total)
{
  int Seen = (Total > 0) ? min(xm08 - xm02, int((xm08 - xm02) * double(Current) / Total)) : 0;
  if (initial || Seen != lastSeen) {
     int y0 = yc04 - ShowSeenExtent;
     int y1 = yc04 + lineHeight / 2 - Gap / 2;
     osd->DrawRectangle(xm02, y0, xm02 + Seen - 1, y1 - 1, Theme.Color(clrSeen));
     osd->DrawRectangle(xm02 + Seen, y0, xm08 - 1, y1 - 1, Theme.Color(clrBackground));
     lastSeen = Seen;
     }
}

void cSkinLCARSDisplayMenu::DrawTextScrollbar(void)
{
  if (textScroller.CanScroll())
     DrawScrollbar(textScroller.Total(), textScroller.Offset(), textScroller.Shown(), textScroller.CanScrollUp(), textScroller.CanScrollDown());
}

void cSkinLCARSDisplayMenu::Scroll(bool Up, bool Page)
{
  cSkinDisplayMenu::Scroll(Up, Page);
  DrawTextScrollbar();
}

int cSkinLCARSDisplayMenu::MaxItems(void)
{
  if (MenuCategory() == mcMain)
     return (ym04 - ym03) / lineHeight;
  else
     return (yb13 - yt02) / lineHeight;
}

void cSkinLCARSDisplayMenu::Clear(void)
{
  textScroller.Reset();
  osd->DrawRectangle(xi00, yi00, xi03 - 1, yi01 - 1, Theme.Color(clrBackground));
}

void cSkinLCARSDisplayMenu::SetTitle(const char *Title)
{
  if (MenuCategory() != mcMain) {
     const cFont *font = cFont::GetFont(fontOsd);
     int w = min(font->Width(Title), xa07 - xa06 - Gap);
     osd->DrawRectangle(xa06, yt00, xa07 - w - Gap - 1, yt01 - 1, frameColor);
     osd->DrawText(xa07 - w - Gap, yt00, Title, Theme.Color(clrMenuTitle), Theme.Color(clrBackground), font, w + Gap, yt01 - yt00, taRight);
     }
}

void cSkinLCARSDisplayMenu::SetButtons(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
  const char *lutText[] = { Red, Green, Yellow, Blue };
  tColor lutFg[] = { clrButtonRedFg, clrButtonGreenFg, clrButtonYellowFg, clrButtonBlueFg };
  tColor lutBg[] = { clrButtonRedBg, clrButtonGreenBg, clrButtonYellowBg, clrButtonBlueBg };
  const cFont *font = cFont::GetFont(fontSml);
  if (MenuCategory() == mcMain) {
     DrawMainButton(lutText[Setup.ColorKey0], xd00, xd01, xd02, xd03, yd02, yd03, Theme.Color(lutFg[Setup.ColorKey0]), Theme.Color(lutBg[Setup.ColorKey0]), font);
     DrawMainButton(lutText[Setup.ColorKey1], xd04, xd05, xd06, xd07, yd02, yd03, Theme.Color(lutFg[Setup.ColorKey1]), Theme.Color(lutBg[Setup.ColorKey1]), font);
     DrawMainButton(lutText[Setup.ColorKey2], xd00, xd01, xd02, xd03, yd04, yd05, Theme.Color(lutFg[Setup.ColorKey2]), Theme.Color(lutBg[Setup.ColorKey2]), font);
     DrawMainButton(lutText[Setup.ColorKey3], xd04, xd05, xd06, xd07, yd04, yd05, Theme.Color(lutFg[Setup.ColorKey3]), Theme.Color(lutBg[Setup.ColorKey3]), font);
     }
  else {
     int h = yb15 - yb14;
     osd->DrawText(xb02, yb14, lutText[Setup.ColorKey0], Theme.Color(lutFg[Setup.ColorKey0]), Theme.Color(lutBg[Setup.ColorKey0]), font, xb03 - xb02, h, taLeft | taBorder);
     osd->DrawText(xb06, yb14, lutText[Setup.ColorKey1], Theme.Color(lutFg[Setup.ColorKey1]), Theme.Color(lutBg[Setup.ColorKey1]), font, xb07 - xb06, h, taLeft | taBorder);
     osd->DrawText(xb10, yb14, lutText[Setup.ColorKey2], Theme.Color(lutFg[Setup.ColorKey2]), Theme.Color(lutBg[Setup.ColorKey2]), font, xb11 - xb10, h, taLeft | taBorder);
     osd->DrawText(xb14, yb14, lutText[Setup.ColorKey3], Theme.Color(lutFg[Setup.ColorKey3]), Theme.Color(lutBg[Setup.ColorKey3]), font, xb15 - xb14, h, taLeft | taBorder);
     }
}

void cSkinLCARSDisplayMenu::SetMessage(eMessageType Type, const char *Text)
{
  if (Text) {
     osd->SaveRegion(xb00, yb14, xb15 - 1, yb15 - 1);
     osd->DrawText(xb00, yb14, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), cFont::GetFont(fontSml), xb15 - xb00, yb15 - yb14, taCenter);
     }
  else
     osd->RestoreRegion();
}

void cSkinLCARSDisplayMenu::SetItem(const char *Text, int Index, bool Current, bool Selectable)
{
  int y = yi00 + Index * lineHeight;
  tColor ColorFg, ColorBg;
  if (Current) {
     if (TwoColors) {
        ColorFg = Theme.Color(clrBackground);
        ColorBg = Theme.Color(clrMenuFrameBg);
        }
     else {
        ColorFg = Theme.Color(clrMenuItemCurrentFg);
        ColorBg = Theme.Color(clrMenuItemCurrentBg);
        }
     osd->DrawRectangle(xi00, y, xi01 - 1, y + lineHeight - 1, ColorBg);
     osd->DrawRectangle(xi02, y, xi02 + lineHeight / 2 - 1, y + lineHeight - 1, ColorBg);
     osd->DrawEllipse  (xi02 + lineHeight / 2, y, xi03 - 1, y + lineHeight - 1, ColorBg, 5);
     currentIndex = Index;
     }
  else {
     ColorFg = Theme.Color(Selectable ? clrMenuItemSelectable : clrMenuItemNonSelectable);
     ColorBg = Theme.Color(clrBackground);
     if (currentIndex == Index)
        osd->DrawRectangle(xi00, y, xi03 - 1, y + lineHeight - 1, Theme.Color(clrBackground));
     }
  const cFont *font = cFont::GetFont(fontOsd);
  for (int i = 0; i < MaxTabs; i++) {
      const char *s = GetTabbedText(Text, i);
      if (s) {
         int xt = xi00 + TextSpacing + Tab(i);
         osd->DrawText(xt, y, s, ColorFg, ColorBg, font, xi01 - xt);
         }
      if (!Tab(i + 1))
         break;
      }
  SetEditableWidth(xi01 - xi00 - TextSpacing - Tab(1));
}

void cSkinLCARSDisplayMenu::SetScrollbar(int Total, int Offset)
{
  DrawScrollbar(Total, Offset, MaxItems(), Offset > 0, Offset + MaxItems() < Total);
}

void cSkinLCARSDisplayMenu::SetEvent(const cEvent *Event)
{
  if (!Event)
     return;
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = xi00;
  int y = yi00;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s - %s", *Event->GetDateString(), *Event->GetTimeString(), *Event->GetEndTimeString());
  ts.Set(osd, xl, y, xi01 - xl, yi01 - y, t, font, Theme.Color(clrEventTime), Theme.Color(clrBackground));
  if (Event->Vps() && Event->Vps() != Event->StartTime()) {
     cString buffer = cString::sprintf(" VPS: %s ", *Event->GetVpsString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xi01 - w, y, buffer, Theme.Color(clrMenuFrameFg), frameColor, font, w);
     int yb = y + font->Height();
     osd->DrawRectangle(xi02, y, xi02 + lineHeight / 2 - 1, yb - 1, frameColor);
     osd->DrawEllipse  (xi02 + lineHeight / 2, y, xi03 - 1, yb - 1, frameColor, 5);
     }
  y += ts.Height();
  if (Event->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Event->GetParentalRatingString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xi01 - w, y, buffer, Theme.Color(clrMenuFrameFg), frameColor, font, w);
     int yb = y + font->Height();
     osd->DrawRectangle(xi02, y, xi02 + lineHeight / 2 - 1, yb - 1, frameColor);
     osd->DrawEllipse  (xi02 + lineHeight / 2, y, xi03 - 1, yb - 1, frameColor, 5);
     }
  y += font->Height();
  ts.Set(osd, xl, y, xi01 - xl, yi01 - y, Event->Title(), font, Theme.Color(clrEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Event->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, xi01 - xl, yi01 - y, Event->ShortText(), font, Theme.Color(clrEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Event->Description())) {
     int yt = y;
     int yb = yi01;
     textScroller.Set(osd, xl, yt, xi01 - xl, yb - yt, Event->Description(), font, Theme.Color(clrEventDescription), Theme.Color(clrBackground));
     DrawTextScrollbar();
     }
}

void cSkinLCARSDisplayMenu::SetRecording(const cRecording *Recording)
{
  if (!Recording)
     return;
  const cRecordingInfo *Info = Recording->Info();
  const cFont *font = cFont::GetFont(fontOsd);
  int xl = xi00;
  int y = yi00;
  cTextScroller ts;
  cString t = cString::sprintf("%s  %s  %s", *DateString(Recording->Start()), *TimeString(Recording->Start()), Info->ChannelName() ? Info->ChannelName() : "");
  ts.Set(osd, xl, y, xi01 - xl, yi01 - y, t, font, Theme.Color(clrEventTime), Theme.Color(clrBackground));
  y += ts.Height();
  int xt = xi01;
  if (Info->GetEvent()->ParentalRating()) {
     cString buffer = cString::sprintf(" %s ", *Info->GetEvent()->GetParentalRatingString());
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xt - w, y, buffer, Theme.Color(clrMenuFrameFg), frameColor, font, w);
     xt -= w + xi02 - xi01;
     }
  if (Info->Errors() > 0) {
     cString buffer = cString::sprintf(" %d %s ", Info->Errors(), tr("errors"));
     const cFont *font = cFont::GetFont(fontSml);
     int w = font->Width(buffer);
     osd->DrawText(xt - w, y, buffer, Theme.Color(clrMenuFrameFg), frameColor, font, w);
     xt -= w + xi02 - xi01;
     }
  if (xt != xi01) {
     const cFont *font = cFont::GetFont(fontSml);
     int yb = y + font->Height();
     osd->DrawRectangle(xi02, y, xi02 + lineHeight / 2 - 1, yb - 1, frameColor);
     osd->DrawEllipse  (xi02 + lineHeight / 2, y, xi03 - 1, yb - 1, frameColor, 5);
     }
  y += font->Height();
  const char *Title = Info->Title();
  if (isempty(Title))
     Title = Recording->Name();
  ts.Set(osd, xl, y, xi01 - xl, yi01 - y, Title, font, Theme.Color(clrEventTitle), Theme.Color(clrBackground));
  y += ts.Height();
  if (!isempty(Info->ShortText())) {
     const cFont *font = cFont::GetFont(fontSml);
     ts.Set(osd, xl, y, xi01 - xl, yi01 - y, Info->ShortText(), font, Theme.Color(clrEventShortText), Theme.Color(clrBackground));
     y += ts.Height();
     }
  y += font->Height();
  if (!isempty(Info->Description())) {
     int yt = y;
     int yb = yi01;
     cString d = Info->Description();
     cString f = Info->FrameParams();
     if (*f) {
        d.Append("\n\n");
        d.Append(f);
        }
     textScroller.Set(osd, xl, yt, xi01 - xl, yb - yt, d, font, Theme.Color(clrEventDescription), Theme.Color(clrBackground));
     DrawTextScrollbar();
     }
}

void cSkinLCARSDisplayMenu::SetText(const char *Text, bool FixedFont)
{
  textScroller.Set(osd, xi00, yi00, GetTextAreaWidth(), yi01 - yi00, Text, GetTextAreaFont(FixedFont), Theme.Color(clrMenuText), Theme.Color(clrBackground));
  DrawTextScrollbar();
}

int cSkinLCARSDisplayMenu::GetTextAreaWidth(void) const
{
  return xi01 - xi00;
}

const cFont *cSkinLCARSDisplayMenu::GetTextAreaFont(bool FixedFont) const
{
  const cFont *font = cFont::GetFont(FixedFont ? fontFix : fontOsd);
  //XXX -> make a way to let the text define which font to use
  return font;
}

void cSkinLCARSDisplayMenu::Flush(void)
{
  if (MenuCategory() == mcMain) {
     cDevice *Device = cDevice::PrimaryDevice();
     cMutexLock ControlMutexLock;
     if (!Device->Replaying() || Device->Transferring()) {
        LOCK_CHANNELS_READ;
        const cChannel *Channel = Channels->GetByNumber(cDevice::PrimaryDevice()->CurrentChannel());
        DrawLive(Channel);
        }
     else if (cControl *Control = cControl::Control(ControlMutexLock, true))
        DrawPlay(Control);
     DrawTimers();
     DrawDevices();
     DrawLiveIndicator();
     DrawSignals();
     }
  DrawFrameDisplay();
  osd->Flush();
  initial = false;
}

// --- cSkinLCARSDisplayReplay -----------------------------------------------

class cSkinLCARSDisplayReplay : public cSkinDisplayReplay {
private:
  cOsd *osd;
  int xp00, xp01, xp02, xp03, xp04, xp05, xp06, xp07, xp08, xp09, xp10, xp11, xp12, xp13, xp14, xp15;
  int yp00, yp01, yp02, yp03, yp04, yp05, yp06, yp07, yp08, yp09;
  bool modeOnly;
  int lineHeight;
  tColor frameColor;
  int lastCurrentWidth;
  int lastTotalWidth;
  cString lastDate;
  tTrackId lastTrackId;
  void DrawDate(void);
  void DrawTrack(void);
public:
  cSkinLCARSDisplayReplay(bool ModeOnly);
  virtual ~cSkinLCARSDisplayReplay();
  virtual void SetRecording(const cRecording *Recording);
  virtual void SetTitle(const char *Title);
  virtual void SetMode(bool Play, bool Forward, int Speed);
  virtual void SetProgress(int Current, int Total);
  virtual void SetCurrent(const char *Current);
  virtual void SetTotal(const char *Total);
  virtual void SetJump(const char *Jump);
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinLCARSDisplayReplay::cSkinLCARSDisplayReplay(bool ModeOnly)
{
  const cFont *font = cFont::GetFont(fontOsd);
  modeOnly = ModeOnly;
  lineHeight = font->Height();
  frameColor = Theme.Color(clrReplayFrameBg);
  lastCurrentWidth = 0;
  lastTotalWidth = 0;
  memset(&lastTrackId, 0, sizeof(lastTrackId));
  int d = 5 * lineHeight;
  xp00 = 0;
  xp01 = xp00 + d / 2;
  xp02 = xp00 + d;
  xp03 = xp02 + lineHeight;
  xp04 = xp02 + d / 4;
  xp05 = xp02 + d;
  xp06 = xp05 + Gap;
  xp15 = cOsd::OsdWidth();
  xp14 = xp15 - lineHeight;
  xp13 = xp14 - Gap;
  xp07 = (xp15 + xp00) / 2;
  xp08 = xp07 + Gap;
  xp09 = xp08 + lineHeight;
  xp10 = xp09 + Gap;
  xp11 = (xp10 + xp13 + Gap) / 2;
  xp12 = xp11 + Gap;

  yp00 = 0;
  yp01 = yp00 + 2 * lineHeight;
  yp02 = yp01 + Gap;
  yp03 = yp02 + 2 * lineHeight;

  yp04 = yp03 + Gap;
  yp09 = yp04 + 3 * lineHeight + Gap / 2;
  yp08 = yp09 - lineHeight;
  yp07 = yp08 - lineHeight;
  yp06 = yp08 - d / 4;
  yp05 = yp09 - d / 2;

  osd = CreateOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - yp09, xp00, yp00, xp15 - 1, yp09 - 1);
  osd->DrawRectangle(xp00, yp00, xp15 - 1, yp09 - 1, modeOnly ? clrTransparent : Theme.Color(clrBackground));
  // Rectangles:
  if (!modeOnly)
     osd->DrawRectangle(xp00, yp00, xp02 - 1, yp01 - 1, frameColor);
  osd->DrawRectangle(xp00, yp02, xp02 - 1, yp03 - 1, frameColor);
  if (!modeOnly) {
     // Elbow:
     osd->DrawRectangle(xp00, yp04, xp01 - 1, yp05 - 1, frameColor);
     osd->DrawRectangle(xp00, yp05, xp01 - 1, yp09 - 1, clrTransparent);
     osd->DrawEllipse  (xp00, yp05, xp01 - 1, yp09 - 1, frameColor, 3);
     osd->DrawRectangle(xp01, yp04, xp02 - 1, yp09 - 1, frameColor);
     osd->DrawEllipse  (xp02, yp06, xp04 - 1, yp08 - 1, frameColor, -3);
     osd->DrawRectangle(xp02, yp08, xp05 - 1, yp09 - 1, frameColor);
     // Status area:
     osd->DrawRectangle(xp06, yp08, xp07 - 1, yp09 - 1, frameColor);
     osd->DrawRectangle(xp08, yp08, xp09 - 1, yp09 - 1, frameColor);
     osd->DrawRectangle(xp10, yp08, xp11 - 1, yp09 - 1, frameColor);
     osd->DrawRectangle(xp12, yp08, xp13 - 1, yp09 - 1, Theme.Color(clrDateBg));
     osd->DrawRectangle(xp14, yp08, xp14 + lineHeight / 2 - 1, yp09 - 1, frameColor);
     osd->DrawRectangle(xp14 + lineHeight / 2, yp08 + lineHeight / 2, xp15 - 1, yp09 - 1, clrTransparent);
     osd->DrawEllipse  (xp14 + lineHeight / 2, yp08, xp15 - 1, yp09 - 1, frameColor, 5);
     }
}

cSkinLCARSDisplayReplay::~cSkinLCARSDisplayReplay()
{
  delete osd;
}

void cSkinLCARSDisplayReplay::DrawDate(void)
{
  cString s = DayDateTime();
  if (!*lastDate || strcmp(s, lastDate)) {
     osd->DrawText(xp12, yp08, s, Theme.Color(clrDateFg), Theme.Color(clrDateBg), cFont::GetFont(fontOsd), xp13 - xp12, lineHeight, taRight | taBorder);
     lastDate = s;
     }
}

void cSkinLCARSDisplayReplay::DrawTrack(void)
{
  cDevice *Device = cDevice::PrimaryDevice();
  const tTrackId *Track = Device->GetTrack(Device->GetCurrentAudioTrack());
  if (Track ? strcmp(lastTrackId.description, Track->description) : *lastTrackId.description) {
     osd->DrawText(xp03, yp04, Track ? Track->description : "", Theme.Color(clrTrackName), Theme.Color(clrBackground), cFont::GetFont(fontOsd), xp07 - xp03);
     strn0cpy(lastTrackId.description, Track ? Track->description : "", sizeof(lastTrackId.description));
     }
}

void cSkinLCARSDisplayReplay::SetRecording(const cRecording *Recording)
{
  const cRecordingInfo *RecordingInfo = Recording->Info();
  SetTitle(RecordingInfo->Title());
  osd->DrawText(xp03, yp01 - lineHeight, RecordingInfo->ShortText(), Theme.Color(clrEventShortText), Theme.Color(clrBackground), cFont::GetFont(fontSml), xp13 - xp03);
  osd->DrawText(xp00, yp00, ShortDateString(Recording->Start()), Theme.Color(clrReplayFrameFg), frameColor, cFont::GetFont(fontOsd), xp02 - xp00, 0, taTop | taRight | taBorder);
  osd->DrawText(xp00, yp01 - lineHeight, TimeString(Recording->Start()), Theme.Color(clrReplayFrameFg), frameColor, cFont::GetFont(fontOsd), xp02 - xp00, 0, taBottom | taRight | taBorder);
}

void cSkinLCARSDisplayReplay::SetTitle(const char *Title)
{
  osd->DrawText(xp03, yp00, Title, Theme.Color(clrEventTitle), Theme.Color(clrBackground), cFont::GetFont(fontOsd), xp13 - xp03);
}

static const char *const *ReplaySymbols[2][2][5] = {
  { { pause_xpm, srew_xpm, srew1_xpm, srew2_xpm, srew3_xpm },
    { pause_xpm, sfwd_xpm, sfwd1_xpm, sfwd2_xpm, sfwd3_xpm }, },
  { { play_xpm,  frew_xpm, frew1_xpm, frew2_xpm, frew3_xpm },
    { play_xpm,  ffwd_xpm, ffwd1_xpm, ffwd2_xpm, ffwd3_xpm } }
  };

void cSkinLCARSDisplayReplay::SetMode(bool Play, bool Forward, int Speed)
{
  Speed = constrain(Speed, -1, 3);
  cBitmap bm(ReplaySymbols[Play][Forward][Speed + 1]);
  osd->DrawBitmap(xp01 - bm.Width() / 2, (yp02 + yp03 - bm.Height()) / 2, bm, Theme.Color(clrReplayFrameFg), frameColor);
}

void cSkinLCARSDisplayReplay::SetProgress(int Current, int Total)
{
  cProgressBar pb(xp13 - xp03, lineHeight, Current, Total, marks, errors, Theme.Color(clrReplayProgressSeen), Theme.Color(clrReplayProgressRest), Theme.Color(clrReplayProgressSelected), Theme.Color(clrReplayProgressMark), Theme.Color(clrReplayProgressCurrent), Theme.Color(clrReplayProgressError));
  osd->DrawBitmap(xp03, yp02, pb);
}

void cSkinLCARSDisplayReplay::SetCurrent(const char *Current)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = font->Width(Current);
  osd->DrawText(xp03, yp03 - lineHeight, Current, Theme.Color(clrReplayPosition), Theme.Color(clrBackground), font, max(lastCurrentWidth, w), 0, taLeft);
  lastCurrentWidth = w;
}

void cSkinLCARSDisplayReplay::SetTotal(const char *Total)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int w = font->Width(Total);
  osd->DrawText(xp13 - w, yp03 - lineHeight, Total, Theme.Color(clrReplayPosition), Theme.Color(clrBackground), font, max(lastTotalWidth, w), 0, taRight);
  lastTotalWidth = w;
}

void cSkinLCARSDisplayReplay::SetJump(const char *Jump)
{
  osd->DrawText(xp06, yp08, Jump, Theme.Color(clrReplayJumpFg), Jump ? Theme.Color(clrReplayJumpBg) : frameColor, cFont::GetFont(fontOsd), xp07 - xp06, 0, taCenter);
}

void cSkinLCARSDisplayReplay::SetMessage(eMessageType Type, const char *Text)
{
  if (Text) {
     osd->SaveRegion(xp06, yp08, xp13 - 1, yp09 - 1);
     osd->DrawText(xp06, yp08, Text, Theme.Color(clrMessageStatusFg + 2 * Type), Theme.Color(clrMessageStatusBg + 2 * Type), cFont::GetFont(fontSml), xp13 - xp06, yp09 - yp08, taCenter);
     }
  else
     osd->RestoreRegion();
}

void cSkinLCARSDisplayReplay::Flush(void)
{
  if (!modeOnly) {
     DrawDate();
     DrawTrack();
     }
  osd->Flush();
}

// --- cSkinLCARSDisplayVolume -----------------------------------------------

class cSkinLCARSDisplayVolume : public cSkinDisplayVolume {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1;
  tColor frameColor;
  int mute;
public:
  cSkinLCARSDisplayVolume(void);
  virtual ~cSkinLCARSDisplayVolume();
  virtual void SetVolume(int Current, int Total, bool Mute);
  virtual void Flush(void);
  };

cSkinLCARSDisplayVolume::cSkinLCARSDisplayVolume(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  frameColor = Theme.Color(clrVolumeFrame);
  mute = -1;
  x0 = 0;
  x1 = lineHeight / 2;
  x2 = lineHeight;
  x3 = x2 + Gap;
  x7 = cOsd::OsdWidth();
  x6 = x7 - lineHeight / 2;
  x5 = x6 - lineHeight / 2;
  x4 = x5 - Gap;
  y0 = 0;
  y1 = lineHeight;
  osd = CreateOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - y1, x0, y0, x7 - 1, y1 - 1);
  osd->DrawRectangle(x0, y0, x7 - 1, y1 - 1, Theme.Color(clrBackground));
  osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, frameColor, 7);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, frameColor);
  osd->DrawRectangle(x6, y0, x7 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, frameColor, 5);
}

cSkinLCARSDisplayVolume::~cSkinLCARSDisplayVolume()
{
  delete osd;
}

void cSkinLCARSDisplayVolume::SetVolume(int Current, int Total, bool Mute)
{
  int xl = x3 + TextSpacing;
  int xr = x4 - TextSpacing;
  int yt = y0 + TextFrame;
  int yb = y1 - TextFrame;
  if (mute != Mute) {
     osd->DrawRectangle(x3, y0, x4 - 1, y1 - 1, frameColor);
     mute = Mute;
     }
  cBitmap bm(Mute ? mute_xpm : volume_xpm);
  osd->DrawBitmap(xl, y0 + (y1 - y0 - bm.Height()) / 2, bm, Theme.Color(clrVolumeSymbol), frameColor);
  if (!Mute) {
     xl += bm.Width() + TextSpacing;
     int w = (y1 - y0) / 3;
     int d = TextFrame;
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

void cSkinLCARSDisplayVolume::Flush(void)
{
  osd->Flush();
}

// --- cSkinLCARSDisplayTracks -----------------------------------------------

class cSkinLCARSDisplayTracks : public cSkinDisplayTracks {
private:
  cOsd *osd;
  int xt00, xt01, xt02, xt03, xt04, xt05, xt06, xt07, xt08, xt09, xt10, xt11, xt12;
  int yt00, yt01, yt02, yt03, yt04, yt05, yt06, yt07;
  int lineHeight;
  tColor frameColor;
  int currentIndex;
  static cBitmap bmAudioLeft, bmAudioRight, bmAudioStereo;
  void SetItem(const char *Text, int Index, bool Current);
public:
  cSkinLCARSDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks);
  virtual ~cSkinLCARSDisplayTracks();
  virtual void SetTrack(int Index, const char * const *Tracks);
  virtual void SetAudioChannel(int AudioChannel);
  virtual void Flush(void);
  };

cBitmap cSkinLCARSDisplayTracks::bmAudioLeft(audioleft_xpm);
cBitmap cSkinLCARSDisplayTracks::bmAudioRight(audioright_xpm);
cBitmap cSkinLCARSDisplayTracks::bmAudioStereo(audiostereo_xpm);

cSkinLCARSDisplayTracks::cSkinLCARSDisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  const cFont *font = cFont::GetFont(fontOsd);
  lineHeight = font->Height();
  frameColor = Theme.Color(clrTrackFrameBg);
  currentIndex = -1;
  xt00 = 0;
  xt01 = xt00 + lineHeight / 2;
  xt02 = xt01 + Gap;
  xt03 = xt00 + 2 * lineHeight;
  int ItemsWidth = font->Width(Title) + xt03 - xt02;
  for (int i = 0; i < NumTracks; i++)
      ItemsWidth = max(ItemsWidth, font->Width(Tracks[i]) + 2 * TextFrame);
  xt04 = xt02 + ItemsWidth;
  xt05 = xt04 + Gap;
  xt06 = xt04 + lineHeight;
  xt07 = xt05 + lineHeight;
  xt08 = xt07 + lineHeight;
  xt09 = xt08 + Gap;
  xt10 = xt09 + lineHeight / 2;
  xt11 = xt10 + Gap;
  xt12 = xt11 + lineHeight;
  yt00 = 0;
  yt01 = yt00 + lineHeight;
  yt02 = yt01 + lineHeight;
  yt03 = yt02 + Gap;
  yt04 = yt03 + NumTracks * lineHeight + (NumTracks - 1) * Gap;
  yt05 = yt04 + Gap;
  yt06 = yt05 + lineHeight;
  yt07 = yt06 + lineHeight;
  while (yt07 > cOsd::OsdHeight()) {
        yt04 -= lineHeight + Gap;
        yt05 = yt04 + Gap;
        yt06 = yt05 + lineHeight;
        yt07 = yt06 + lineHeight;
        }
  osd = CreateOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - yt07, xt00, yt00, xt12 - 1, yt07 - 1);
  // The upper elbow:
  osd->DrawRectangle(xt00, yt00, xt12 - 1, yt07 - 1, Theme.Color(clrBackground));
  osd->DrawRectangle(xt00, yt00, xt03 - 1, yt02 - 1, clrTransparent);
  osd->DrawEllipse  (xt00, yt00, xt03 - 1, yt02 - 1, frameColor, 2);
  osd->DrawRectangle(xt03, yt00, xt04 - 1, yt02 - 1, frameColor);
  osd->DrawRectangle(xt04, yt00, xt08 - 1, yt01 - 1, frameColor);
  osd->DrawEllipse  (xt04, yt01, xt06 - 1, yt02 - 1, frameColor, -2);
  osd->DrawRectangle(xt09, yt00, xt10 - 1, yt01 - 1, frameColor);
  osd->DrawRectangle(xt11, yt00, xt11 + lineHeight / 2 - 1, yt01 - 1, frameColor);
  osd->DrawRectangle(xt11 + lineHeight / 2, yt00, xt12 - 1, yt00 + lineHeight / 2 - 1, clrTransparent);
  osd->DrawEllipse  (xt11 + lineHeight / 2, yt00, xt12 - 1, yt01 - 1, frameColor, 5);
  osd->DrawText(xt03, yt00, Title, Theme.Color(clrTrackFrameFg), frameColor, font, xt04 - xt03, 0, taTop | taRight);
  // The items:
  for (int i = 0; i < NumTracks; i++)
      SetItem(Tracks[i], i, false);
  // The lower elbow:
  osd->DrawRectangle(xt00, yt05, xt03 - 1, yt07 - 1, clrTransparent);
  osd->DrawEllipse  (xt00, yt05, xt03 - 1, yt07 - 1, frameColor, 3);
  osd->DrawRectangle(xt03, yt05, xt04 - 1, yt07 - 1, frameColor);
  osd->DrawRectangle(xt04, yt06, xt08 - 1, yt07 - 1, frameColor);
  osd->DrawEllipse  (xt04, yt05, xt06 - 1, yt06 - 1, frameColor, -3);
  osd->DrawRectangle(xt09, yt06, xt10 - 1, yt07 - 1, frameColor);
  osd->DrawRectangle(xt11, yt06, xt11 + lineHeight / 2 - 1, yt07 - 1, frameColor);
  osd->DrawRectangle(xt11 + lineHeight / 2, yt06 + lineHeight / 2, xt12 - 1, yt07 - 1, clrTransparent);
  osd->DrawEllipse  (xt11 + lineHeight / 2, yt06, xt12 - 1, yt07 - 1, frameColor, 5);
}

cSkinLCARSDisplayTracks::~cSkinLCARSDisplayTracks()
{
  delete osd;
}

void cSkinLCARSDisplayTracks::SetItem(const char *Text, int Index, bool Current)
{
  int y0 = yt03 + Index * (lineHeight + Gap);
  int y1 = y0 + lineHeight;
  if (y1 > yt04)
     return;
  tColor ColorFg, ColorBg;
  if (Current) {
     ColorFg = Theme.Color(clrTrackItemCurrentFg);
     ColorBg = Theme.Color(clrTrackItemCurrentBg);
     osd->DrawRectangle(xt00, y0, xt01 - 1, y1 - 1, frameColor);
     osd->DrawRectangle(xt02, y0, xt04 - 1, y1 - 1, ColorBg);
     osd->DrawRectangle(xt05, y0, xt05 + lineHeight / 2 - 1, y1 - 1, ColorBg);
     osd->DrawEllipse  (xt05 + lineHeight / 2, y0, xt07 - 1, y1 - 1, ColorBg, 5);
     currentIndex = Index;
     }
  else {
     ColorFg = Theme.Color(clrTrackItemFg);
     ColorBg = Theme.Color(clrTrackItemBg);
     osd->DrawRectangle(xt00, y0, xt01 - 1, y1 - 1, frameColor);
     osd->DrawRectangle(xt02, y0, xt04 - 1, y1 - 1, ColorBg);
     if (currentIndex == Index)
        osd->DrawRectangle(xt05, y0, xt07 - 1, y1 - 1, Theme.Color(clrBackground));
     }
  const cFont *font = cFont::GetFont(fontOsd);
  osd->DrawText(xt02, y0, Text, ColorFg, ColorBg, font, xt04 - xt02, y1 - y0, taTop | taLeft | taBorder);
}

void cSkinLCARSDisplayTracks::SetTrack(int Index, const char * const *Tracks)
{
  if (currentIndex >= 0)
     SetItem(Tracks[currentIndex], currentIndex, false);
  SetItem(Tracks[Index], Index, true);
}

void cSkinLCARSDisplayTracks::SetAudioChannel(int AudioChannel)
{
  cBitmap *bm = NULL;
  switch (AudioChannel) {
    case 0: bm = &bmAudioStereo; break;
    case 1: bm = &bmAudioLeft;   break;
    case 2: bm = &bmAudioRight;  break;
    default: ;
    }
  if (bm)
     osd->DrawBitmap(xt04 - bm->Width(), (yt06 + yt07 - bm->Height()) / 2, *bm, Theme.Color(clrTrackFrameFg), frameColor);
  else
     osd->DrawRectangle(xt03, yt06, xt04 - 1, yt07 - 1, frameColor);
}

void cSkinLCARSDisplayTracks::Flush(void)
{
  osd->Flush();
}

// --- cSkinLCARSDisplayMessage ----------------------------------------------

class cSkinLCARSDisplayMessage : public cSkinDisplayMessage {
private:
  cOsd *osd;
  int x0, x1, x2, x3, x4, x5, x6, x7;
  int y0, y1;
public:
  cSkinLCARSDisplayMessage(void);
  virtual ~cSkinLCARSDisplayMessage();
  virtual void SetMessage(eMessageType Type, const char *Text);
  virtual void Flush(void);
  };

cSkinLCARSDisplayMessage::cSkinLCARSDisplayMessage(void)
{
  const cFont *font = cFont::GetFont(fontOsd);
  int lineHeight = font->Height();
  x0 = 0;
  x1 = lineHeight / 2;
  x2 = lineHeight;
  x3 = x2 + Gap;
  x7 = cOsd::OsdWidth();
  x6 = x7 - lineHeight / 2;
  x5 = x6 - lineHeight / 2;
  x4 = x5 - Gap;
  y0 = 0;
  y1 = lineHeight;
  osd = CreateOsd(cOsd::OsdLeft(), cOsd::OsdTop() + cOsd::OsdHeight() - y1, x0, y0, x7 - 1, y1 - 1);
}

cSkinLCARSDisplayMessage::~cSkinLCARSDisplayMessage()
{
  delete osd;
}

void cSkinLCARSDisplayMessage::SetMessage(eMessageType Type, const char *Text)
{
  tColor ColorFg = Theme.Color(clrMessageStatusFg + 2 * Type);
  tColor ColorBg = Theme.Color(clrMessageStatusBg + 2 * Type);
  osd->DrawRectangle(x0, y0, x7 - 1, y1 - 1, Theme.Color(clrBackground));
  osd->DrawRectangle(x0, y0, x1 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x0, y0, x1 - 1, y1 - 1, ColorBg, 7);
  osd->DrawRectangle(x1, y0, x2 - 1, y1 - 1, ColorBg);
  osd->DrawText(x3, y0, Text, ColorFg, ColorBg, cFont::GetFont(fontSml), x4 - x3, y1 - y0, taCenter);
  osd->DrawRectangle(x5, y0, x6 - 1, y1 - 1, ColorBg);
  osd->DrawRectangle(x6, y0, x7 - 1, y1 - 1, clrTransparent);
  osd->DrawEllipse  (x6, y0, x7 - 1, y1 - 1, ColorBg, 5);
}

void cSkinLCARSDisplayMessage::Flush(void)
{
  osd->Flush();
}

// --- cSkinLCARS ------------------------------------------------------------

cSkinLCARS::cSkinLCARS(void)
:cSkin("lcars", &::Theme)
{
}

const char *cSkinLCARS::Description(void)
{
  return "LCARS";
}

cSkinDisplayChannel *cSkinLCARS::DisplayChannel(bool WithInfo)
{
  return new cSkinLCARSDisplayChannel(WithInfo);
}

cSkinDisplayMenu *cSkinLCARS::DisplayMenu(void)
{
  return new cSkinLCARSDisplayMenu;
}

cSkinDisplayReplay *cSkinLCARS::DisplayReplay(bool ModeOnly)
{
  return new cSkinLCARSDisplayReplay(ModeOnly);
}

cSkinDisplayVolume *cSkinLCARS::DisplayVolume(void)
{
  return new cSkinLCARSDisplayVolume;
}

cSkinDisplayTracks *cSkinLCARS::DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks)
{
  return new cSkinLCARSDisplayTracks(Title, NumTracks, Tracks);
}

cSkinDisplayMessage *cSkinLCARS::DisplayMessage(void)
{
  return new cSkinLCARSDisplayMessage;
}
