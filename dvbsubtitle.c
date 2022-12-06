/*
 * dvbsubtitle.c: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schluessler <marco@lordzodiac.de>
 * With some input from the "subtitles plugin" by Pekka Virtanen <pekka.virtanen@sci.fi>
 *
 * $Id: dvbsubtitle.c 5.2 2022/12/06 16:57:01 kls Exp $
 */

#include "dvbsubtitle.h"
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include "device.h"
#include "libsi/si.h"

#define PAGE_COMPOSITION_SEGMENT    0x10
#define REGION_COMPOSITION_SEGMENT  0x11
#define CLUT_DEFINITION_SEGMENT     0x12
#define OBJECT_DATA_SEGMENT         0x13
#define DISPLAY_DEFINITION_SEGMENT  0x14
#define DISPARITY_SIGNALING_SEGMENT 0x15 // DVB BlueBook A156
#define END_OF_DISPLAY_SET_SEGMENT  0x80
#define STUFFING_SEGMENT            0xFF

#define PGS_PALETTE_SEGMENT         0x14
#define PGS_OBJECT_SEGMENT          0x15
#define PGS_PRESENTATION_SEGMENT    0x16
#define PGS_WINDOW_SEGMENT          0x17
#define PGS_DISPLAY_SEGMENT         0x80

// Set these to 'true' for debug output, which is written into the file dbg-log.htm
// in the current working directory. The HTML file shows the actual bitmaps (dbg-nnn.jpg)
// used to display the subtitles.
static bool DebugNormal    = false; // shows pages, regions and objects
static bool DebugVerbose   = false; // shows everything
static bool DebugDisplay   = DebugVerbose || DebugNormal;
static bool DebugPages     = DebugVerbose || DebugNormal;
static bool DebugRegions   = DebugVerbose || DebugNormal;
static bool DebugObjects   = DebugVerbose || DebugNormal;
static bool DebugConverter = DebugVerbose;
static bool DebugSegments  = DebugVerbose;
static bool DebugPixel     = DebugVerbose;
static bool DebugCluts     = DebugVerbose;
static bool DebugOutput    = DebugVerbose;

#define dbgdisplay(a...)   if (DebugDisplay)   SD.WriteHtml(a)
#define dbgpages(a...)     if (DebugPages)     SD.WriteHtml(a)
#define dbgregions(a...)   if (DebugRegions)   SD.WriteHtml(a)
#define dbgobjects(a...)   if (DebugObjects)   SD.WriteHtml(a)
#define dbgconverter(a...) if (DebugConverter) SD.WriteHtml(a)
#define dbgsegments(a...)  if (DebugSegments)  SD.WriteHtml(a)
#define dbgpixel(a...)     if (DebugPixel)     SD.WriteHtml(a)
#define dbgcluts(a...)     if (DebugCluts)     SD.WriteHtml(a)
#define dbgoutput(a...)    if (DebugOutput)    SD.WriteHtml(a)

#define DBGMAXBITMAPS  100 // debug output will be stopped after this many bitmaps
#define DBGBITMAPWIDTH 400

#define FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY // some don't properly handle version numbers, which renders them useless because subtitles are not displayed

// --- cSubtitleDebug --------------------------------------------------------

class cSubtitleDebug {
private:
  cMutex mutex;
  int imgCnt;
  int64_t firstPts;
  bool newFile;
  double factor;
public:
  cSubtitleDebug(void) { Reset(); }
  void Reset(void);
  bool Active(void) { return imgCnt < DBGMAXBITMAPS; }
  int64_t FirstPts(void) { return firstPts; }
  void SetFirstPts(int64_t FirstPts) { if (firstPts < 0) firstPts = FirstPts; }
  void SetFactor(double Factor) { factor = Factor; }
  cString WriteJpeg(const cBitmap *Bitmap, int MaxX = 0, int MaxY = 0);
  void WriteHtml(const char *Format, ...);
  };

void cSubtitleDebug::Reset(void)
{
  imgCnt = 0;
  firstPts = -1;
  newFile = true;
  factor = 1.0;
}

cString cSubtitleDebug::WriteJpeg(const cBitmap *Bitmap, int MaxX, int MaxY)
{
  if (!Active())
     return NULL;
  cMutexLock MutexLock(&mutex);
  cBitmap *Scaled = Bitmap->Scaled(factor, factor, true);
  int w = MaxX ? int(round(MaxX * factor)) : Scaled->Width();
  int h = MaxY ? int(round(MaxY * factor)) : Scaled->Height();
  uchar mem[w * h * 3];
  for (int x = 0; x < w; x++) {
      for (int y = 0; y < h; y++) {
          tColor c = Scaled->GetColor(x, y);
          int o = (y * w + x) * 3;
          mem[o++] = (c & 0x00FF0000) >> 16;
          mem[o++] = (c & 0x0000FF00) >> 8;
          mem[o]   = (c & 0x000000FF);
          }
      }
  delete Scaled;
  int Size = 0;
  uchar *Jpeg = RgbToJpeg(mem, w, h, Size);
  cString ImgName = cString::sprintf("dbg-%03d.jpg", imgCnt++);
  int f = open(ImgName, O_WRONLY | O_CREAT, DEFFILEMODE);
  if (f >= 0) {
     if (write(f, Jpeg, Size) < 0)
        LOG_ERROR_STR(*ImgName);
     close(f);
     }
  free(Jpeg);
  return ImgName;
}

void cSubtitleDebug::WriteHtml(const char *Format, ...)
{
  if (!Active())
     return;
  cMutexLock MutexLock(&mutex);
  if (FILE *f = fopen("dbg-log.htm", newFile ? "w" : "a")) {
     va_list ap;
     va_start(ap, Format);
     vfprintf(f, Format, ap);
     va_end(ap);
     fclose(f);
     newFile = false;
     }
}

static cSubtitleDebug SD;

// --- cSubtitleClut ---------------------------------------------------------

class cSubtitleClut : public cListObject {
private:
  int clutId;
  int clutVersionNumber;
  cPalette palette2;
  cPalette palette4;
  cPalette palette8;
  tColor yuv2rgb(int Y, int Cb, int Cr);
  void SetColor(int Bpp, int Index, tColor Color);
public:
  cSubtitleClut(int ClutId);
  void Parse(cBitStream &bs);
  void ParsePgs(cBitStream &bs);
  int ClutId(void) { return clutId; }
  int ClutVersionNumber(void) { return clutVersionNumber; }
  const cPalette *GetPalette(int Bpp);
  };

cSubtitleClut::cSubtitleClut(int ClutId)
:palette2(2)
,palette4(4)
,palette8(8)
{
  int a = 0, r = 0, g = 0, b = 0;
  clutId = ClutId;
  clutVersionNumber = -1;
  // ETSI EN 300 743 10.3: 4-entry CLUT default contents
  palette2.SetColor(0, ArgbToColor(  0,   0,   0,   0));
  palette2.SetColor(1, ArgbToColor(255, 255, 255, 255));
  palette2.SetColor(2, ArgbToColor(255,   0,   0,   0));
  palette2.SetColor(3, ArgbToColor(255, 127, 127, 127));
  // ETSI EN 300 743 10.2: 16-entry CLUT default contents
  palette4.SetColor(0, ArgbToColor(0, 0, 0, 0));
  for (int i = 1; i < 16; ++i) {
      if (i < 8) {
         r = (i & 1) ? 255 : 0;
         g = (i & 2) ? 255 : 0;
         b = (i & 4) ? 255 : 0;
         }
      else {
         r = (i & 1) ? 127 : 0;
         g = (i & 2) ? 127 : 0;
         b = (i & 4) ? 127 : 0;
         }
      palette4.SetColor(i, ArgbToColor(255, r, g, b));
      }
  // ETSI EN 300 743 10.1: 256-entry CLUT default contents
  palette8.SetColor(0, ArgbToColor(0, 0, 0, 0));
  for (int i = 1; i < 256; ++i) {
      if (i < 8) {
         r = (i & 1) ? 255 : 0;
         g = (i & 2) ? 255 : 0;
         b = (i & 4) ? 255 : 0;
         a = 63;
         }
      else {
         switch (i & 0x88) {
           case 0x00:
                r = ((i & 1) ? 85 : 0) + ((i & 0x10) ? 170 : 0);
                g = ((i & 2) ? 85 : 0) + ((i & 0x20) ? 170 : 0);
                b = ((i & 4) ? 85 : 0) + ((i & 0x40) ? 170 : 0);
                a = 255;
                break;
           case 0x08:
                r = ((i & 1) ? 85 : 0) + ((i & 0x10) ? 170 : 0);
                g = ((i & 2) ? 85 : 0) + ((i & 0x20) ? 170 : 0);
                b = ((i & 4) ? 85 : 0) + ((i & 0x40) ? 170 : 0);
                a = 127;
                break;
           case 0x80:
                r = 127 + ((i & 1) ? 43 : 0) + ((i & 0x10) ? 85 : 0);
                g = 127 + ((i & 2) ? 43 : 0) + ((i & 0x20) ? 85 : 0);
                b = 127 + ((i & 4) ? 43 : 0) + ((i & 0x40) ? 85 : 0);
                a = 255;
                break;
           case 0x88:
                r = ((i & 1) ? 43 : 0) + ((i & 0x10) ? 85 : 0);
                g = ((i & 2) ? 43 : 0) + ((i & 0x20) ? 85 : 0);
                b = ((i & 4) ? 43 : 0) + ((i & 0x40) ? 85 : 0);
                a = 255;
                break;
            }
         }
      palette8.SetColor(i, ArgbToColor(a, r, g, b));
      }
}

void cSubtitleClut::Parse(cBitStream &bs)
{
  int Version = bs.GetBits(4);
#ifndef FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY
  if (clutVersionNumber == Version)
     return; // no update
#endif
  clutVersionNumber = Version;
  bs.SkipBits(4); // reserved
  dbgcluts("<b>clut</b> id %d version %d<br>\n", clutId, clutVersionNumber);
  while (!bs.IsEOF()) {
        uchar clutEntryId = bs.GetBits(8);
        bool entryClut2Flag = bs.GetBit();
        bool entryClut4Flag = bs.GetBit();
        bool entryClut8Flag = bs.GetBit();
        bs.SkipBits(4); // reserved
        uchar yval;
        uchar crval;
        uchar cbval;
        uchar tval;
        if (bs.GetBit()) { // full_range_flag
           yval  = bs.GetBits(8);
           crval = bs.GetBits(8);
           cbval = bs.GetBits(8);
           tval  = bs.GetBits(8);
           }
        else {
           yval  = bs.GetBits(6) << 2;
           crval = bs.GetBits(4) << 4;
           cbval = bs.GetBits(4) << 4;
           tval  = bs.GetBits(2) << 6;
           }
        tColor value = 0;
        if (yval) {
           value = yuv2rgb(yval, cbval, crval);
           value |= ((10 - (clutEntryId ? Setup.SubtitleFgTransparency : Setup.SubtitleBgTransparency)) * (255 - tval) / 10) << 24;
           }
        dbgcluts("%2d %d %d %d %08X<br>\n", clutEntryId, entryClut2Flag ? 2 : 0, entryClut4Flag ? 4 : 0, entryClut8Flag ? 8 : 0, value);
        if (entryClut2Flag)
           SetColor(2, clutEntryId, value);
        if (entryClut4Flag)
           SetColor(4, clutEntryId, value);
        if (entryClut8Flag)
           SetColor(8, clutEntryId, value);
        }
}

void cSubtitleClut::ParsePgs(cBitStream &bs)
{
  int Version = bs.GetBits(8);
  if (clutVersionNumber == Version)
     return; // no update
  clutVersionNumber = Version;
  dbgcluts("<b>clut</b> id %d version %d<br>\n", clutId, clutVersionNumber);
  for (int i = 0; i < 256; ++i)
      SetColor(8, i, ArgbToColor(0, 0, 0, 0));
  while (!bs.IsEOF()) {
        uchar clutEntryId = bs.GetBits(8);
        uchar yval  = bs.GetBits(8);
        uchar crval = bs.GetBits(8);
        uchar cbval = bs.GetBits(8);
        uchar tval  = bs.GetBits(8);
        tColor value = 0;
        if (yval) {
           value = yuv2rgb(yval, cbval, crval);
           value |= ((10 - (clutEntryId ? Setup.SubtitleFgTransparency : Setup.SubtitleBgTransparency)) * tval / 10) << 24;
           }
        dbgcluts("%2d %08X<br>\n", clutEntryId, value);
        SetColor(8, clutEntryId, value);
        }
}

tColor cSubtitleClut::yuv2rgb(int Y, int Cb, int Cr)
{
  int Ey, Epb, Epr;
  int Eg, Eb, Er;

  Ey = (Y - 16);
  Epb = (Cb - 128);
  Epr = (Cr - 128);
  /* ITU-R 709 */
  Er = constrain((298 * Ey             + 460 * Epr) / 256, 0, 255);
  Eg = constrain((298 * Ey -  55 * Epb - 137 * Epr) / 256, 0, 255);
  Eb = constrain((298 * Ey + 543 * Epb            ) / 256, 0, 255);

  return (Er << 16) | (Eg << 8) | Eb;
}

void cSubtitleClut::SetColor(int Bpp, int Index, tColor Color)
{
  switch (Bpp) {
    case 2: palette2.SetColor(Index, Color); break;
    case 4: palette4.SetColor(Index, Color); break;
    case 8: palette8.SetColor(Index, Color); break;
    default: esyslog("ERROR: wrong Bpp in cSubtitleClut::SetColor(%d, %d, %08X)", Bpp, Index, Color);
    }
}

const cPalette *cSubtitleClut::GetPalette(int Bpp)
{
  switch (Bpp) {
    case 2: return &palette2;
    case 4: return &palette4;
    case 8: return &palette8;
    default: esyslog("ERROR: wrong Bpp in cSubtitleClut::GetPalette(%d)", Bpp);
    }
  return &palette8;
}

// --- cSubtitleObject -------------------------------------------------------

class cSubtitleObject : public cListObject {
private:
  int objectId;
  int objectVersionNumber;
  int objectCodingMethod;
  bool nonModifyingColorFlag;
  int topLength;
  int botLength;
  int topIndex;
  uchar *topData;
  uchar *botData;
  char *txtData;
  int lineHeight;
  void DrawLine(cBitmap *Bitmap, int x, int y, tIndex Index, int Length);
  bool Decode2BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int&x, int y, const uint8_t *MapTable);
  bool Decode4BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int&x, int y, const uint8_t *MapTable);
  bool Decode8BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int&x, int y);
  bool DecodePgsCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int&x, int y);
  void DecodeSubBlock(cBitmap *Bitmap, int px, int py, const uchar *Data, int Length, bool Even);
  void DecodeCharacterString(const uchar *Data, int NumberOfCodes);
public:
  cSubtitleObject(int ObjectId);
  ~cSubtitleObject();
  void Parse(cBitStream &bs);
  void ParsePgs(cBitStream &bs);
  int ObjectId(void) { return objectId; }
  int ObjectVersionNumber(void) { return objectVersionNumber; }
  int ObjectCodingMethod(void) { return objectCodingMethod; }
  bool NonModifyingColorFlag(void) { return nonModifyingColorFlag; }
  void Render(cBitmap *Bitmap, int px, int py, tIndex IndexFg, tIndex IndexBg);
  };

cSubtitleObject::cSubtitleObject(int ObjectId)
{
  objectId = ObjectId;
  objectVersionNumber = -1;
  objectCodingMethod = -1;
  nonModifyingColorFlag = false;
  topLength = 0;
  botLength = 0;
  topIndex = 0;
  topData = NULL;
  botData = NULL;
  txtData = NULL;
  lineHeight = 26; // configurable subtitling font size?
}

cSubtitleObject::~cSubtitleObject()
{
  free(topData);
  free(botData);
  free(txtData);
}

void cSubtitleObject::Parse(cBitStream &bs)
{
  int Version = bs.GetBits(4);
#ifndef FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY
  if (objectVersionNumber == Version)
     return; // no update
#endif
  objectVersionNumber = Version;
  objectCodingMethod = bs.GetBits(2);
  nonModifyingColorFlag = bs.GetBit();
  bs.SkipBit(); // reserved
  dbgobjects("<b>object</b> id %d version %d method %d modify %d", objectId, objectVersionNumber, objectCodingMethod, nonModifyingColorFlag); // no "<br>\n" here, DecodeCharacterString() may add data
  if (objectCodingMethod == 0) { // coding of pixels
     topLength = bs.GetBits(16);
     botLength = bs.GetBits(16);
     free(topData);
     if ((topData = MALLOC(uchar, topLength)) != NULL)
        memcpy(topData, bs.GetData(), topLength);
     else
        topLength = 0;
     free(botData);
     if ((botData = MALLOC(uchar, botLength)) != NULL)
        memcpy(botData, bs.GetData() + topLength, botLength);
     else
        botLength = 0;
     bs.WordAlign();
     }
  else if (objectCodingMethod == 1) { // coded as a string of characters
     int numberOfCodes = bs.GetBits(8);
     DecodeCharacterString(bs.GetData(), numberOfCodes);
     }
  dbgobjects("<br>\n");
  if (DebugObjects) {
     // We can't get the actual clut here, so we use a default one. This may lead to
     // funny colors, but we just want to get a rough idea of what's in the object, anyway.
     cSubtitleClut Clut(0);
     cBitmap b(1920, 1080, 8);
     b.Replace(*Clut.GetPalette(b.Bpp()));
     b.Clean();
     Render(&b, 0, 0, 0, 1);
     int x1, y1, x2, y2;
     if (b.Dirty(x1, y1, x2, y2)) {
        cString ImgName = SD.WriteJpeg(&b, x2, y2);
        dbgobjects("<img src=\"%s\"><br>\n", *ImgName);
        }
     }
}

void cSubtitleObject::ParsePgs(cBitStream &bs)
{
  int Version = bs.GetBits(8);
  if (objectVersionNumber == Version)
     return; // no update
  objectVersionNumber = Version;
  objectCodingMethod = 0;
  int sequenceDescriptor = bs.GetBits(8);
  if (!(sequenceDescriptor & 0x80) && topData != NULL) {
     memcpy(topData + topIndex, bs.GetData(), (bs.Length() - bs.Index()) / 8);
     topIndex += (bs.Length() - bs.Index()) / 8;
     return;
     }
  topLength = bs.GetBits(24) - 4 + 1; // exclude width / height, add sub block type
  bs.SkipBits(32);
  if ((topData = MALLOC(uchar, topLength)) != NULL) {
     topData[topIndex++] = 0xFF; // PGS end of line
     memcpy(topData + 1, bs.GetData(), (bs.Length() - bs.Index()) / 8);
     topIndex += (bs.Length() - bs.Index()) / 8 + 1;
     }
  dbgobjects("<b>object</b> id %d version %d method %d modify %d", objectId, objectVersionNumber, objectCodingMethod, nonModifyingColorFlag);
}

void cSubtitleObject::DecodeCharacterString(const uchar *Data, int NumberOfCodes)
{
  // "ETSI EN 300 743 V1.3.1 (2006-11)", chapter 7.2.5 "Object data segment" specifies
  // character_code to be a 16-bit index number into the character table identified
  // in the subtitle_descriptor. However, the "subtitling_descriptor" <sic> according to
  // "ETSI EN 300 468 V1.13.1 (2012-04)" doesn't contain a "character table identifier".
  // It only contains a three letter language code, without any specification as to how
  // this is related to a specific character table.
  // Apparently the first "code" in textual subtitles contains the character table
  // identifier, and all codes are 8-bit only. So let's first make Data a string of
  // 8-bit characters:
  if (NumberOfCodes > 0) {
     char txt[NumberOfCodes + 1];
     for (int i = 0; i < NumberOfCodes; i++)
         txt[i] = Data[i * 2 + 1];
     txt[NumberOfCodes] = 0;
     const uchar *from = (uchar *)txt;
     int len = NumberOfCodes;
     const char *CharacterTable = SI::getCharacterTable(from, len);
     dbgobjects(" table %s raw '%s'", CharacterTable, from);
     cCharSetConv conv(CharacterTable, cCharSetConv::SystemCharacterTable());
     const char *s = conv.Convert((const char *)from);
     dbgobjects(" conv '%s'", s);
     free(txtData);
     txtData = strdup(s);
     }
}

void cSubtitleObject::DecodeSubBlock(cBitmap *Bitmap, int px, int py, const uchar *Data, int Length, bool Even)
{
  int x = 0;
  int y = Even ? 0 : 1;
  uint8_t map2to4[ 4] = { 0x00, 0x07, 0x08, 0x0F };
  uint8_t map2to8[ 4] = { 0x00, 0x77, 0x88, 0xFF };
  uint8_t map4to8[16] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
  const uint8_t *mapTable = NULL;
  cBitStream bs(Data, Length * 8);
  while (!bs.IsEOF()) {
        switch (bs.GetBits(8)) {
          case 0x10:
               dbgpixel("2-bit / pixel code string<br>\n");
               switch (Bitmap->Bpp()) {
                 case 8:  mapTable = map2to8; break;
                 case 4:  mapTable = map2to4; break;
                 default: mapTable = NULL;    break;
                 }
               while (Decode2BppCodeString(Bitmap, px, py, &bs, x, y, mapTable) && !bs.IsEOF())
                     ;
               bs.ByteAlign();
               break;
          case 0x11:
               dbgpixel("4-bit / pixel code string<br>\n");
               switch (Bitmap->Bpp()) {
                 case 8:  mapTable = map4to8; break;
                 default: mapTable = NULL;    break;
                 }
               while (Decode4BppCodeString(Bitmap, px, py, &bs, x, y, mapTable) && !bs.IsEOF())
                     ;
               bs.ByteAlign();
               break;
          case 0x12:
               dbgpixel("8-bit / pixel code string<br>\n");
               while (Decode8BppCodeString(Bitmap, px, py, &bs, x, y) && !bs.IsEOF())
                     ;
               break;
          case 0x20:
               dbgpixel("sub block 2 to 4 map<br>\n");
               for (int i = 0; i < 4; ++i)
                   map2to4[i] = bs.GetBits(4);
               break;
          case 0x21:
               dbgpixel("sub block 2 to 8 map<br>\n");
               for (int i = 0; i < 4; ++i)
                   map2to8[i] = bs.GetBits(8);
               break;
          case 0x22:
               dbgpixel("sub block 4 to 8 map<br>\n");
               for (int i = 0; i < 16; ++i)
                   map4to8[i] = bs.GetBits(8);
               break;
          case 0xF0:
               dbgpixel("end of object line<br>\n");
               x = 0;
               y += 2;
               break;
          case 0xFF:
               dbgpixel("PGS code string, including EOLs<br>\n");
               while (DecodePgsCodeString(Bitmap, px, py, &bs, x, y) && !bs.IsEOF()) {
                     x = 0;
                     y++;
                     }
               break;
          default: dbgpixel("unknown sub block %s %d<br>\n", __FUNCTION__, __LINE__);
          }
        }
}

void cSubtitleObject::DrawLine(cBitmap *Bitmap, int x, int y, tIndex Index, int Length)
{
  if (nonModifyingColorFlag && Index == 1)
     return;
  for (int pos = x; pos < x + Length; pos++)
      Bitmap->SetIndex(pos, y, Index);
}

bool cSubtitleObject::Decode2BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int &x, int y, const uint8_t *MapTable)
{
  int rl = 0;
  int color = 0;
  uchar code = bs->GetBits(2);
  if (code) {
     color = code;
     rl = 1;
     }
  else if (bs->GetBit()) { // switch_1
     rl = bs->GetBits(3) + 3;
     color = bs->GetBits(2);
     }
  else if (bs->GetBit()) // switch_2
     rl = 1; //color 0
  else {
     switch (bs->GetBits(2)) { // switch_3
       case 0:
            return false;
       case 1:
            rl = 2; //color 0
            break;
       case 2:
            rl = bs->GetBits(4) + 12;
            color = bs->GetBits(2);
            break;
       case 3:
            rl = bs->GetBits(8) + 29;
            color = bs->GetBits(2);
            break;
       default: ;
       }
     }
  if (MapTable)
     color = MapTable[color];
  DrawLine(Bitmap, px + x, py + y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode4BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int &x, int y, const uint8_t *MapTable)
{
  int rl = 0;
  int color = 0;
  uchar code = bs->GetBits(4);
  if (code) {
     color = code;
     rl = 1;
     }
  else if (bs->GetBit() == 0) { // switch_1
     code = bs->GetBits(3);
     if (code)
        rl = code + 2; //color 0
     else
        return false;
     }
  else if (bs->GetBit() == 0) { // switch_2
     rl = bs->GetBits(2) + 4;
     color = bs->GetBits(4);
     }
  else {
     switch (bs->GetBits(2)) { // switch_3
       case 0: // color 0
            rl = 1;
            break;
       case 1: // color 0
            rl = 2;
            break;
       case 2:
            rl = bs->GetBits(4) + 9;
            color = bs->GetBits(4);
            break;
       case 3:
            rl = bs->GetBits(8) + 25;
            color = bs->GetBits(4);
            break;
       }
     }
  if (MapTable)
     color = MapTable[color];
  DrawLine(Bitmap, px + x, py + y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode8BppCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int &x, int y)
{
  int rl = 0;
  int color = 0;
  uchar code = bs->GetBits(8);
  if (code) {
     color = code;
     rl = 1;
     }
  else if (bs->GetBit()) {
     rl = bs->GetBits(7);
     color = bs->GetBits(8);
     }
  else {
     code = bs->GetBits(7);
     if (code)
        rl = code; // color 0
     else
        return false;
     }
  DrawLine(Bitmap, px + x, py + y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::DecodePgsCodeString(cBitmap *Bitmap, int px, int py, cBitStream *bs, int &x, int y)
{
  while (!bs->IsEOF()) {
        int color = bs->GetBits(8);
        int rl = 1;
        if (!color) {
           int flags = bs->GetBits(8);
           rl = flags & 0x3f;
           if (flags & 0x40)
              rl = (rl << 8) + bs->GetBits(8);
           color = flags & 0x80 ? bs->GetBits(8) : 0;
           }
        if (rl > 0) {
           DrawLine(Bitmap, px + x, py + y, color, rl);
           x += rl;
           }
        else if (!rl)
           return true;
        }
  return false;
}

void cSubtitleObject::Render(cBitmap *Bitmap, int px, int py, tIndex IndexFg, tIndex IndexBg)
{
  if (objectCodingMethod == 0) { // coding of pixels
     DecodeSubBlock(Bitmap, px, py, topData, topLength, true);
     if (botLength)
        DecodeSubBlock(Bitmap, px, py, botData, botLength, false);
     else
        DecodeSubBlock(Bitmap, px, py, topData, topLength, false);
     }
  else if (objectCodingMethod == 1) { // coded as a string of characters
     if (txtData) {
        //TODO couldn't we draw the text directly into Bitmap?
        cFont *font = cFont::CreateFont(Setup.FontOsd, Setup.FontOsdSize);
        cBitmap tmp(font->Width(txtData), font->Height(), Bitmap->Bpp());
        double factor = (double)lineHeight / font->Height();
        tmp.DrawText(0, 0, txtData, Bitmap->Color(IndexFg), Bitmap->Color(IndexBg), font);
        cBitmap *scaled = tmp.Scaled(factor, factor, true);
        Bitmap->DrawBitmap(px, py, *scaled);
        delete scaled;
        delete font;
        }
     }
}

// --- cSubtitleObjects ------------------------------------------------------

class cSubtitleObjects : public cList<cSubtitleObject> {
public:
  cSubtitleObject *GetObjectById(int ObjectId, bool New = false);
  };

cSubtitleObject *cSubtitleObjects::GetObjectById(int ObjectId, bool New)
{
  for (cSubtitleObject *so = First(); so; so = Next(so)) {
      if (so->ObjectId() == ObjectId)
         return so;
      }
  if (!New)
     return NULL;
  cSubtitleObject *Object = new cSubtitleObject(ObjectId);
  Add(Object);
  return Object;
}

// --- cSubtitleObjectRef ----------------------------------------------------

class cSubtitleObjectRef : public cListObject {
protected:
  int objectId;
  int objectType;
  int objectProviderFlag;
  int objectHorizontalPosition;
  int objectVerticalPosition;
  int foregroundPixelCode;
  int backgroundPixelCode;
public:
  cSubtitleObjectRef(void);
  cSubtitleObjectRef(cBitStream &bs);
  int ObjectId(void) { return objectId; }
  int ObjectType(void) { return objectType; }
  int ObjectProviderFlag(void) { return objectProviderFlag; }
  int ObjectHorizontalPosition(void) { return objectHorizontalPosition; }
  int ObjectVerticalPosition(void) { return objectVerticalPosition; }
  int ForegroundPixelCode(void) { return foregroundPixelCode; }
  int BackgroundPixelCode(void) { return backgroundPixelCode; }
  };

cSubtitleObjectRef::cSubtitleObjectRef(void)
{
  objectId = 0;
  objectType = 0;
  objectProviderFlag = 0;
  objectHorizontalPosition = 0;
  objectVerticalPosition = 0;
  foregroundPixelCode = 0;
  backgroundPixelCode = 0;
}

cSubtitleObjectRef::cSubtitleObjectRef(cBitStream &bs)
{
  objectId = bs.GetBits(16);
  objectType = bs.GetBits(2);
  objectProviderFlag = bs.GetBits(2);
  objectHorizontalPosition = bs.GetBits(12);
  bs.SkipBits(4); // reserved
  objectVerticalPosition = bs.GetBits(12);
  if (objectType == 0x01 || objectType == 0x02) {
     foregroundPixelCode = bs.GetBits(8);
     backgroundPixelCode = bs.GetBits(8);
     }
  else {
     foregroundPixelCode = 0;
     backgroundPixelCode = 0;
     }
  dbgregions("<b>objectref</b> id %d type %d flag %d x %d y %d fg %d bg %d<br>\n", objectId, objectType, objectProviderFlag, objectHorizontalPosition, objectVerticalPosition, foregroundPixelCode, backgroundPixelCode);
}

// --- cSubtitleObjectRefPgs - PGS variant of cSubtitleObjectRef -------------

class cSubtitleObjectRefPgs : public cSubtitleObjectRef {
private:
  int windowId;
  int compositionFlag;
  int cropX;
  int cropY;
  int cropW;
  int cropH;
public:
  cSubtitleObjectRefPgs(cBitStream &bs);
};

cSubtitleObjectRefPgs::cSubtitleObjectRefPgs(cBitStream &bs)
:cSubtitleObjectRef()
{
  objectId = bs.GetBits(16);
  windowId = bs.GetBits(8);
  compositionFlag = bs.GetBits(8);
  bs.SkipBits(32); // skip absolute position, object is aligned to region
  if ((compositionFlag & 0x80) != 0) {
     cropX = bs.GetBits(16);
     cropY = bs.GetBits(16);
     cropW = bs.GetBits(16);
     cropH = bs.GetBits(16);
     }
  else
     cropX = cropY = cropW = cropH = 0;
  dbgregions("<b>objectrefPgs</b> id %d flag %d x %d y %d cropX %d cropY %d cropW %d cropH %d<br>\n", objectId, compositionFlag, objectHorizontalPosition, objectVerticalPosition, cropX, cropY, cropW, cropH);
}

// --- cSubtitleRegion -------------------------------------------------------

class cSubtitleRegion : public cListObject {
private:
  int regionId;
  int regionVersionNumber;
  bool regionFillFlag;
  int regionWidth;
  int regionHeight;
  int regionLevelOfCompatibility;
  int regionDepth;
  int clutId;
  int region8bitPixelCode;
  int region4bitPixelCode;
  int region2bitPixelCode;
  cList<cSubtitleObjectRef> objectRefs;
public:
  cSubtitleRegion(int RegionId);
  void Parse(cBitStream &bs);
  void ParsePgs(cBitStream &bs);
  void SetDimensions(int Width, int Height);
  int RegionId(void) { return regionId; }
  int RegionVersionNumber(void) { return regionVersionNumber; }
  bool RegionFillFlag(void) { return regionFillFlag; }
  int RegionWidth(void) { return regionWidth; }
  int RegionHeight(void) { return regionHeight; }
  int RegionLevelOfCompatibility(void) { return regionLevelOfCompatibility; }
  int RegionDepth(void) { return regionDepth; }
  int ClutId(void) { return clutId; }
  void Render(cBitmap *Bitmap, cSubtitleObjects *Objects);
  };

cSubtitleRegion::cSubtitleRegion(int RegionId)
{
  regionId = RegionId;
  regionVersionNumber = -1;
  regionFillFlag = false;
  regionWidth = 0;
  regionHeight = 0;
  regionLevelOfCompatibility = 0;
  regionDepth = 0;
  clutId = -1;
  region8bitPixelCode = 0;
  region4bitPixelCode = 0;
  region2bitPixelCode = 0;
}

void cSubtitleRegion::Parse(cBitStream &bs)
{
  int Version = bs.GetBits(4);
#ifndef FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY
  if (regionVersionNumber == Version)
     return; // no update
#endif
  regionVersionNumber = Version;
  regionFillFlag = bs.GetBit();
  bs.SkipBits(3); // reserved
  regionWidth = bs.GetBits(16);
  regionHeight = bs.GetBits(16);
  regionLevelOfCompatibility = 1 << bs.GetBits(3); // stored as "number of bits per pixel"
  regionDepth = 1 << bs.GetBits(3); // stored as "number of bits per pixel"
  bs.SkipBits(2); // reserved
  clutId = bs.GetBits(8);
  region8bitPixelCode = bs.GetBits(8);
  region4bitPixelCode = bs.GetBits(4);
  region2bitPixelCode = bs.GetBits(2);
  bs.SkipBits(2); // reserved
  dbgregions("<b>region</b> id %d version %d fill %d width %d height %d level %d depth %d clutId %d<br>\n", regionId, regionVersionNumber, regionFillFlag, regionWidth, regionHeight, regionLevelOfCompatibility, regionDepth, clutId);
  // no objectRefs.Clear() here!
  while (!bs.IsEOF())
        objectRefs.Add(new cSubtitleObjectRef(bs));
}

void cSubtitleRegion::ParsePgs(cBitStream &bs)
{
  regionDepth = 8;
  bs.SkipBits(8); // skip palette update flag
  clutId = bs.GetBits(8);
  dbgregions("<b>region</b> id %d version %d clutId %d<br>\n", regionId, regionVersionNumber, clutId);
  int objects = bs.GetBits(8);
  while (objects--)
        objectRefs.Add(new cSubtitleObjectRefPgs(bs));
}

void cSubtitleRegion::SetDimensions(int Width, int Height)
{
  regionWidth = Width;
  regionHeight = Height;
  dbgregions("<b>region</b> id %d width %d height %d<br>\n", regionId, regionWidth, regionHeight);
}

void cSubtitleRegion::Render(cBitmap *Bitmap, cSubtitleObjects *Objects)
{
  if (regionFillFlag) {
     switch (Bitmap->Bpp()) {
       case 2: Bitmap->Fill(region2bitPixelCode); break;
       case 4: Bitmap->Fill(region4bitPixelCode); break;
       case 8: Bitmap->Fill(region8bitPixelCode); break;
       default: dbgregions("unknown bpp %d (%s %d)<br>\n", Bitmap->Bpp(), __FUNCTION__, __LINE__);
       }
     }
  for (cSubtitleObjectRef *sor = objectRefs.First(); sor; sor = objectRefs.Next(sor)) {
      if (cSubtitleObject *so = Objects->GetObjectById(sor->ObjectId())) {
         so->Render(Bitmap, sor->ObjectHorizontalPosition(), sor->ObjectVerticalPosition(), sor->ForegroundPixelCode(), sor->BackgroundPixelCode());
         }
      }
}

// --- cSubtitleRegionRef ----------------------------------------------------

class cSubtitleRegionRef : public cListObject {
private:
  int regionId;
  int regionHorizontalAddress;
  int regionVerticalAddress;
public:
  cSubtitleRegionRef(int id, int x, int y);
  cSubtitleRegionRef(cBitStream &bs);
  int RegionId(void) { return regionId; }
  int RegionHorizontalAddress(void) { return regionHorizontalAddress; }
  int RegionVerticalAddress(void) { return regionVerticalAddress; }
  };

cSubtitleRegionRef::cSubtitleRegionRef(int id, int x, int y)
{
  regionId = id;
  regionHorizontalAddress = x;
  regionVerticalAddress = y;
  dbgpages("<b>regionref</b> id %d tx %d y %d<br>\n", regionId, regionHorizontalAddress, regionVerticalAddress);
}
cSubtitleRegionRef::cSubtitleRegionRef(cBitStream &bs)
{
  regionId = bs.GetBits(8);
  bs.SkipBits(8); // reserved
  regionHorizontalAddress = bs.GetBits(16);
  regionVerticalAddress = bs.GetBits(16);
  dbgpages("<b>regionref</b> id %d tx %d y %d<br>\n", regionId, regionHorizontalAddress, regionVerticalAddress);
}

// --- cDvbSubtitlePage ------------------------------------------------------

class cDvbSubtitlePage : public cListObject {
private:
  int pageId;
  int pageTimeout;
  int pageVersionNumber;
  int pageState;
  int64_t pts;
  bool pending;
  cSubtitleObjects objects;
  cList<cSubtitleClut> cluts;
  cList<cSubtitleRegion> regions;
  cList<cSubtitleRegionRef> regionRefs;
public:
  cDvbSubtitlePage(int PageId);
  void Parse(int64_t Pts, cBitStream &bs);
  void ParsePgs(int64_t Pts, cBitStream &bs);
  int PageId(void) { return pageId; }
  int PageTimeout(void) { return pageTimeout; }
  int PageVersionNumber(void) { return pageVersionNumber; }
  int PageState(void) { return pageState; }
  int64_t Pts(void) const { return pts; }
  bool Pending(void) { return pending; }
  cSubtitleObjects *Objects(void) { return &objects; }
  tArea *GetAreas(int &NumAreas);
  tArea CombineAreas(int NumAreas, const tArea *Areas);
  tArea ScaleArea(const tArea &Area, double FactorX, double FactorY);
  cSubtitleObject *GetObjectById(int ObjectId, bool New = false);
  cSubtitleClut *GetClutById(int ClutId, bool New = false);
  cSubtitleRegion *GetRegionById(int RegionId, bool New = false);
  cSubtitleRegionRef *GetRegionRefByIndex(int RegionRefIndex) { return regionRefs.Get(RegionRefIndex); }
  void AddRegionRef(cSubtitleRegionRef *rf) { regionRefs.Add(rf); }
  void SetPending(bool Pending) { pending = Pending; }
  };

cDvbSubtitlePage::cDvbSubtitlePage(int PageId)
{
  pageId = PageId;
  pageTimeout = 0;
  pageVersionNumber = -1;
  pageState = -1;
  pts = -1;
  pending = false;
}

void cDvbSubtitlePage::Parse(int64_t Pts, cBitStream &bs)
{
  if (Pts >= 0)
     pts = Pts;
  pageTimeout = bs.GetBits(8);
  int Version = bs.GetBits(4);
#ifndef FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY
  if (pageVersionNumber == Version)
     return; // no update
#endif
  pageVersionNumber = Version;
  pageState = bs.GetBits(2);
  switch (pageState) {
    case 0: // normal case - page update
         break;
    case 1: // acquisition point - page refresh
         regions.Clear();
         objects.Clear();
         break;
    case 2: // mode change - new page
         regions.Clear();
         cluts.Clear();
         objects.Clear();
         break;
    case 3: // reserved
         break;
    default: dbgpages("unknown page state: %d<br>\n", pageState);
    }
  bs.SkipBits(2); // reserved
  dbgpages("<hr>\n<b>page</b> id %d version %d pts %" PRId64 " timeout %d state %d<br>\n", pageId, pageVersionNumber, pts, pageTimeout, pageState);
  regionRefs.Clear();
  while (!bs.IsEOF())
        regionRefs.Add(new cSubtitleRegionRef(bs));
  pending = true;
}

void cDvbSubtitlePage::ParsePgs(int64_t Pts, cBitStream &bs)
{
  if (Pts >= 0)
     pts = Pts;
  pageTimeout = 60000;
  int Version = bs.GetBits(16);
  if (pageVersionNumber == Version)
     return;
  pageVersionNumber = Version;
  pageState = bs.GetBits(2);
  switch (pageState) {
    case 0: // normal case - page update
         regions.Clear();
         break;
    case 1: // acquisition point - page refresh
    case 2: // epoch start - new page
    case 3: // epoch continue - new page
         regions.Clear();
         cluts.Clear();
         objects.Clear();
         break;
    default: dbgpages("unknown page state: %d<br>\n", pageState);
    }
  bs.SkipBits(6);
  dbgpages("<hr>\n<b>page</b> id %d version %d pts %" PRId64 " timeout %d state %d<br>\n", pageId, pageVersionNumber, pts, pageTimeout, pageState);
  regionRefs.Clear();
  pending = true;
}

tArea *cDvbSubtitlePage::GetAreas(int &NumAreas)
{
  if (regions.Count() > 0) {
     NumAreas = regionRefs.Count();
     tArea *Areas = new tArea[NumAreas];
     tArea *a = Areas;
     for (cSubtitleRegionRef *srr = regionRefs.First(); srr; srr = regionRefs.Next(srr)) {
         if (cSubtitleRegion *sr = GetRegionById(srr->RegionId())) {
            a->x1 = srr->RegionHorizontalAddress();
            a->y1 = srr->RegionVerticalAddress();
            a->x2 = srr->RegionHorizontalAddress() + sr->RegionWidth() - 1;
            a->y2 = srr->RegionVerticalAddress() + sr->RegionHeight() - 1;
            a->bpp = sr->RegionDepth();
            }
         else
            a->x1 = a->y1 = a->x2 = a->y2 = a->bpp = 0;
         a++;
         }
     return Areas;
     }
  NumAreas = 0;
  return NULL;
}

tArea cDvbSubtitlePage::CombineAreas(int NumAreas, const tArea *Areas)
{
  tArea a;
  a.x1 = INT_MAX;
  a.x2 = INT_MIN;
  a.y1 = INT_MAX;
  a.y2 = INT_MIN;
  a.bpp = 1;
  for (int i = 0; i < NumAreas; i++) {
      a.x1 = min(a.x1, Areas[i].x1);
      a.x2 = max(a.x2, Areas[i].x2);
      a.y1 = min(a.y1, Areas[i].y1);
      a.y2 = max(a.y2, Areas[i].y2);
      a.bpp = max(a.bpp, Areas[i].bpp);
      }
  return a;
}

tArea cDvbSubtitlePage::ScaleArea(const tArea &Area, double FactorX, double FactorY)
{
  tArea a;
  a.x1  = int(round(FactorX * Area.x1)    );
  a.x2  = int(round(FactorX * Area.x2) - 1);
  a.y1  = int(round(FactorY * Area.y1)    );
  a.y2  = int(round(FactorY * Area.y2) - 1);
  a.bpp = Area.bpp;
  while ((a.Width() & 3) != 0)
        a.x2++; // aligns width to a multiple of 4, so 2, 4 and 8 bpp will work
  return a;
}

cSubtitleClut *cDvbSubtitlePage::GetClutById(int ClutId, bool New)
{
  for (cSubtitleClut *sc = cluts.First(); sc; sc = cluts.Next(sc)) {
      if (sc->ClutId() == ClutId)
         return sc;
      }
  if (!New)
     return NULL;
  cSubtitleClut *Clut = new cSubtitleClut(ClutId);
  cluts.Add(Clut);
  return Clut;
}

cSubtitleRegion *cDvbSubtitlePage::GetRegionById(int RegionId, bool New)
{
  for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
      if (sr->RegionId() == RegionId)
         return sr;
      }
  if (!New)
     return NULL;
  cSubtitleRegion *Region = new cSubtitleRegion(RegionId);
  regions.Add(Region);
  return Region;
}

cSubtitleObject *cDvbSubtitlePage::GetObjectById(int ObjectId, bool New)
{
  return objects.GetObjectById(ObjectId, New);
}

// --- cDvbSubtitleAssembler -------------------------------------------------

class cDvbSubtitleAssembler {
private:
  uchar *data;
  int length;
  int pos;
  int size;
  bool Realloc(int Size);
public:
  cDvbSubtitleAssembler(void);
  virtual ~cDvbSubtitleAssembler();
  void Reset(void);
  unsigned char *Get(int &Length);
  void Put(const uchar *Data, int Length);
  };

cDvbSubtitleAssembler::cDvbSubtitleAssembler(void)
{
  data = NULL;
  size = 0;
  Reset();
}

cDvbSubtitleAssembler::~cDvbSubtitleAssembler()
{
  free(data);
}

void cDvbSubtitleAssembler::Reset(void)
{
  length = 0;
  pos = 0;
}

bool cDvbSubtitleAssembler::Realloc(int Size)
{
  if (Size > size) {
     Size = max(Size, 2048);
     if (uchar *NewBuffer = (uchar *)realloc(data, Size)) {
        size = Size;
        data = NewBuffer;
        }
     else {
        esyslog("ERROR: can't allocate memory for subtitle assembler");
        length = 0;
        size = 0;
        free(data);
        data = NULL;
        return false;
        }
     }
  return true;
}

unsigned char *cDvbSubtitleAssembler::Get(int &Length)
{
  if (length > pos + 5) {
     Length = (data[pos + 4] << 8) + data[pos + 5] + 6;
     if (length >= pos + Length) {
        unsigned char *result = data + pos;
        pos += Length;
        return result;
        }
     }
  return NULL;
}

void cDvbSubtitleAssembler::Put(const uchar *Data, int Length)
{
  if (Length && Realloc(length + Length)) {
     memcpy(data + length, Data, Length);
     length += Length;
     }
}

// --- cDvbSubtitleBitmaps ---------------------------------------------------

class cDvbSubtitleBitmaps : public cListObject {
private:
  int state;
  int64_t pts;
  int timeout;
  tArea *areas;
  int numAreas;
  tArea areaCombined;
  tArea areaOsd;
  double osdFactorX;
  double osdFactorY;
  cVector<cBitmap *> bitmaps;
public:
  cDvbSubtitleBitmaps(int State, int64_t Pts, int Timeout, tArea *Areas, int NumAreas, double OsdFactorX, double OsdFactorY, tArea &AreaCombined, tArea &AreaOsd);
  ~cDvbSubtitleBitmaps();
  int State(void) { return state; }
  int64_t Pts(void) { return pts; }
  int Timeout(void) { return timeout; }
  void AddBitmap(cBitmap *Bitmap);
  bool HasBitmaps(void) { return bitmaps.Size(); }
  void Draw(cOsd *Osd);
  void DbgDump(int WindowWidth, int WindowHeight);
  };

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int State, int64_t Pts, int Timeout, tArea *Areas, int NumAreas, double OsdFactorX, double OsdFactorY, tArea &AreaCombined, tArea &AreaOsd)
{
  state = State;
  pts = Pts;
  timeout = Timeout;
  areas = Areas;
  numAreas = NumAreas;
  areaCombined = AreaCombined;
  areaOsd = AreaOsd;
  osdFactorX = OsdFactorX;
  osdFactorY = OsdFactorY;
}

cDvbSubtitleBitmaps::~cDvbSubtitleBitmaps()
{
  delete[] areas;
  for (int i = 0; i < bitmaps.Size(); i++)
      delete bitmaps[i];
}

void cDvbSubtitleBitmaps::AddBitmap(cBitmap *Bitmap)
{
  bitmaps.Append(Bitmap);
}

void cDvbSubtitleBitmaps::Draw(cOsd *Osd)
{
  bool Scale = !(DoubleEqual(osdFactorX, 1.0) && DoubleEqual(osdFactorY, 1.0));
  bool AntiAlias = Setup.AntiAlias;
  if (Scale && osdFactorX > 1.0 || osdFactorY > 1.0) {
     // Upscaling requires 8bpp:
     int Bpp = areaOsd.bpp;
     areaOsd.bpp = 8;
     if (Osd->CanHandleAreas(&areaOsd, 1) != oeOk) {
        areaOsd.bpp = Bpp;
        AntiAlias = false;
        }
     }
  if (State() == 0 || Osd->SetAreas(&areaOsd, 1) == oeOk) {
     cBitmap combined(areaCombined.Width(), areaCombined.Height(), areaCombined.bpp);
     combined.SetOffset(areaCombined.x1, areaCombined.y1);
     for (int i = 0; i < bitmaps.Size(); i++) {
         // merge bitmaps into combined
         cBitmap *b = bitmaps[i];
         combined.DrawBitmap(b->X0(), b->Y0(), *b);
         }
     Osd->DrawScaledBitmap(int(round(combined.X0() * osdFactorX)), int(round(combined.Y0() * osdFactorY)), combined, osdFactorX, osdFactorY, AntiAlias);
     Osd->Flush();
     }
}

void cDvbSubtitleBitmaps::DbgDump(int WindowWidth, int WindowHeight)
{
  if (!SD.Active())
     return;
  SD.SetFirstPts(Pts());
  double STC = double(cDevice::PrimaryDevice()->GetSTC() - SD.FirstPts()) / 90000;
  double Start = double(Pts() - SD.FirstPts()) / 90000;
  double Duration = Timeout();
  double End = Start + Duration;
  cBitmap Bitmap(WindowWidth, WindowHeight, 8);
#define DBGBACKGROUND 0xA0A0A0
  Bitmap.DrawRectangle(0, 0, WindowWidth - 1, WindowHeight - 1, DBGBACKGROUND);
  for (int i = 0; i < bitmaps.Size(); i++) {
      cBitmap *b = bitmaps[i];
      Bitmap.DrawBitmap(b->X0(), b->Y0(), *b);
      }
  cString ImgName = SD.WriteJpeg(&Bitmap);
#define BORDER //" border=1"
  SD.WriteHtml("<p>%s<br>", State() == 0 ? "page update" : State() == 1 ? "page refresh" : State() == 2 ? "new page" : "???");
  SD.WriteHtml("<table" BORDER "><tr><td>");
  SD.WriteHtml("%.2f", STC);
  SD.WriteHtml("</td><td>");
  SD.WriteHtml("<img src=\"%s\">", *ImgName);
  SD.WriteHtml("</td><td style=\"height:100%%\"><table" BORDER " style=\"height:100%%\">");
  SD.WriteHtml("<tr><td valign=top><b>%.2f</b></td></tr>", Start);
  SD.WriteHtml("<tr><td valign=middle>%.2f</td></tr>", Duration);
  SD.WriteHtml("<tr><td valign=bottom>%.2f</td></tr>", End);
  SD.WriteHtml("</table></td>");
  SD.WriteHtml("</tr></table>\n");
}

// --- cDvbSubtitleConverter -------------------------------------------------

int cDvbSubtitleConverter::setupLevel = 0;

cDvbSubtitleConverter::cDvbSubtitleConverter(void)
:cThread("subtitle converter")
{
  dvbSubtitleAssembler = new cDvbSubtitleAssembler;
  osd = NULL;
  frozen = false;
  ddsVersionNumber = -1;
  displayWidth = windowWidth = 720;
  displayHeight = windowHeight = 576;
  windowHorizontalOffset = 0;
  windowVerticalOffset = 0;
  pages = new cList<cDvbSubtitlePage>;
  bitmaps = new cList<cDvbSubtitleBitmaps>;
  SD.Reset();
  Start();
}

cDvbSubtitleConverter::~cDvbSubtitleConverter()
{
  Cancel(3);
  delete dvbSubtitleAssembler;
  delete osd;
  delete bitmaps;
  delete pages;
}

void cDvbSubtitleConverter::SetupChanged(void)
{
  setupLevel++;
}

void cDvbSubtitleConverter::Reset(void)
{
  dbgconverter("converter reset -----------------------<br>\n");
  dvbSubtitleAssembler->Reset();
  Lock();
  pages->Clear();
  bitmaps->Clear();
  DELETENULL(osd);
  frozen = false;
  ddsVersionNumber = -1;
  displayWidth = windowWidth = 720;
  displayHeight = windowHeight = 576;
  windowHorizontalOffset = 0;
  windowVerticalOffset = 0;
  Unlock();
}

int cDvbSubtitleConverter::ConvertFragments(const uchar *Data, int Length)
{
  if (Data && Length > 8) {
     int PayloadOffset = PesPayloadOffset(Data);
     int SubstreamHeaderLength = 4;
     bool ResetSubtitleAssembler = Data[PayloadOffset + 3] == 0x00;

     // Compatibility mode for old subtitles plugin:
     if ((Data[7] & 0x01) && (Data[PayloadOffset - 3] & 0x81) == 0x01 && Data[PayloadOffset - 2] == 0x81) {
        PayloadOffset--;
        SubstreamHeaderLength = 1;
        ResetSubtitleAssembler = Data[8] >= 5;
        }

     if (Length > PayloadOffset + SubstreamHeaderLength) {
        int64_t pts = PesHasPts(Data) ? PesGetPts(Data) : -1;
        if (pts >= 0)
           dbgconverter("converter PTS: %" PRId64 "<br>\n", pts);
        const uchar *data = Data + PayloadOffset + SubstreamHeaderLength; // skip substream header
        int length = Length - PayloadOffset - SubstreamHeaderLength; // skip substream header
        if (ResetSubtitleAssembler)
           dvbSubtitleAssembler->Reset();

        if (length > 3) {
           if (data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F)
              dvbSubtitleAssembler->Put(data + 2, length - 2);
           else
              dvbSubtitleAssembler->Put(data, length);

           int Count;
           while (true) {
                 unsigned char *b = dvbSubtitleAssembler->Get(Count);
                 if (b && b[0] == 0x0F) {
                    if (ExtractSegment(b, Count, pts) == -1)
                       break;
                    }
                 else
                    break;
                 }
           }
        }
     return Length;
     }
  return 0;
}

int cDvbSubtitleConverter::Convert(const uchar *Data, int Length)
{
  if (Data && Length > 8) {
     int PayloadOffset = PesPayloadOffset(Data);
     if (Length > PayloadOffset) {
        int64_t pts = PesHasPts(Data) ? PesGetPts(Data) : -1;
        if (pts >= 0)
           dbgconverter("converter PTS: %" PRId64 "<br>\n", pts);
        const uchar *data = Data + PayloadOffset;
        int length = Length - PayloadOffset;
        if (length > 0) {
           if (length > 2 && data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F) {
              data += 2;
              length -= 2;
              }
           const uchar *b = data;
           while (length > 0) {
                 if (b[0] == STUFFING_SEGMENT)
                    break;
                 int n;
                 if (b[0] == 0x0F)
                    n = ExtractSegment(b, length, pts);
                 else
                    n = ExtractPgsSegment(b, length, pts);
                 if (n < 0)
                    break;
                 b += n;
                 length -= n;
                 }
           }
        }
     return Length;
     }
  return 0;
}

#define LimitTo32Bit(n) ((n) & 0x00000000FFFFFFFFL)

void cDvbSubtitleConverter::Action(void)
{
  int LastSetupLevel = setupLevel;
  cTimeMs Timeout;
  while (Running()) {
        int WaitMs = 100;
        if (!frozen) {
           LOCK_THREAD;
           if (osd) {
              int NewSetupLevel = setupLevel;
              if (Timeout.TimedOut() || LastSetupLevel != NewSetupLevel) {
                 dbgoutput("closing osd<br>\n");
                 DELETENULL(osd);
                 }
              LastSetupLevel = NewSetupLevel;
              }
           for (cDvbSubtitleBitmaps *sb = bitmaps->First(); sb; sb = bitmaps->Next(sb)) {
               // Calculate the Delta between the STC (the current timestamp of the video)
               // and the bitmap's PTS (the timestamp when the bitmap shall be presented).
               // A negative Delta means that the bitmap will be presented in the future:
               int64_t STC = cDevice::PrimaryDevice()->GetSTC();
               int64_t Delta = LimitTo32Bit(STC) - LimitTo32Bit(sb->Pts()); // some devices only deliver 32 bits
               if (Delta > (int64_t(1) << 31))
                  Delta -= (int64_t(1) << 32);
               else if (Delta < -((int64_t(1) << 31) - 1))
                  Delta += (int64_t(1) << 32);
               Delta /= 90; // STC and PTS are in 1/90000s
               if (Delta >= 0) { // found a bitmap that shall be displayed...
                  if (Delta < sb->Timeout() * 1000) { // ...and has not timed out yet
                     if (!sb->HasBitmaps()) {
                        Timeout.Set();
                        WaitMs = 0;
                        }
                     else if (AssertOsd()) {
                        dbgoutput("showing bitmap #%d of %d<br>\n", sb->Index() + 1, bitmaps->Count());
                        sb->Draw(osd);
                        Timeout.Set(sb->Timeout() * 1000);
                        dbgconverter("PTS: %" PRId64 "  STC: %" PRId64 " (%" PRId64 ") timeout: %d<br>\n", sb->Pts(), STC, Delta, sb->Timeout());
                        }
                     }
                  else
                     WaitMs = 0; // bitmap already timed out, so try next one immediately
                  dbgoutput("deleting bitmap #%d of %d<br>\n", sb->Index() + 1, bitmaps->Count());
                  bitmaps->Del(sb);
                  break;
                  }
               }
           }
        cCondWait::SleepMs(WaitMs);
        }
}

void cDvbSubtitleConverter::SetOsdData(void)
{
  int OsdWidth, OsdHeight;
  double OsdAspect;
  int VideoWidth, VideoHeight;
  double VideoAspect;
  cDevice::PrimaryDevice()->GetOsdSize(OsdWidth, OsdHeight, OsdAspect);
  cDevice::PrimaryDevice()->GetVideoSize(VideoWidth, VideoHeight, VideoAspect);
  if (OsdWidth == displayWidth && OsdHeight == displayHeight) {
     osdFactorX = osdFactorY = 1.0;
     osdDeltaX = osdDeltaY = 0;
     }
  else {
     osdFactorX = osdFactorY = min(double(OsdWidth) / displayWidth, double(OsdHeight) / displayHeight);
     osdDeltaX = (OsdWidth - displayWidth * osdFactorX) / 2;
     osdDeltaY = (OsdHeight - displayHeight * osdFactorY) / 2;
     }
}

bool cDvbSubtitleConverter::AssertOsd(void)
{
  LOCK_THREAD;
  if (!osd) {
     SetOsdData();
     osd = cOsdProvider::NewOsd(int(round(osdFactorX * windowHorizontalOffset + osdDeltaX)), int(round(osdFactorY * windowVerticalOffset + osdDeltaY)) + Setup.SubtitleOffset, OSD_LEVEL_SUBTITLES);
     }
  return osd != NULL;
}

cDvbSubtitlePage *cDvbSubtitleConverter::GetPageById(int PageId, bool New)
{
  for (cDvbSubtitlePage *sp = pages->First(); sp; sp = pages->Next(sp)) {
      if (sp->PageId() == PageId)
         return sp;
      }
  if (!New)
     return NULL;
  cDvbSubtitlePage *Page = new cDvbSubtitlePage(PageId);
  pages->Add(Page);
  return Page;
}

int cDvbSubtitleConverter::ExtractSegment(const uchar *Data, int Length, int64_t Pts)
{
  cBitStream bs(Data, Length * 8);
  if (Length > 5 && bs.GetBits(8) == 0x0F) { // sync byte
     int segmentType = bs.GetBits(8);
     if (segmentType == STUFFING_SEGMENT)
        return -1;
     LOCK_THREAD;
     cDvbSubtitlePage *page = GetPageById(bs.GetBits(16), true);
     int segmentLength = bs.GetBits(16);
     if (!bs.SetLength(bs.Index() + segmentLength * 8))
        return -1;
     switch (segmentType) {
       case PAGE_COMPOSITION_SEGMENT: {
            if (page->Pending()) {
               dbgsegments("END_OF_DISPLAY_SET_SEGMENT (simulated)<br>\n");
               FinishPage(page);
               }
            dbgsegments("PAGE_COMPOSITION_SEGMENT<br>\n");
            page->Parse(Pts, bs);
            SD.SetFactor(double(DBGBITMAPWIDTH) / windowWidth);
            break;
            }
       case REGION_COMPOSITION_SEGMENT: {
            dbgsegments("REGION_COMPOSITION_SEGMENT<br>\n");
            cSubtitleRegion *region = page->GetRegionById(bs.GetBits(8), true);
            region->Parse(bs);
            break;
            }
       case CLUT_DEFINITION_SEGMENT: {
            dbgsegments("CLUT_DEFINITION_SEGMENT<br>\n");
            cSubtitleClut *clut = page->GetClutById(bs.GetBits(8), true);
            clut->Parse(bs);
            break;
            }
       case OBJECT_DATA_SEGMENT: {
            dbgsegments("OBJECT_DATA_SEGMENT<br>\n");
            cSubtitleObject *object = page->GetObjectById(bs.GetBits(16), true);
            object->Parse(bs);
            break;
            }
       case DISPLAY_DEFINITION_SEGMENT: {
            dbgsegments("DISPLAY_DEFINITION_SEGMENT<br>\n");
            int version = bs.GetBits(4);
#ifndef FIX_SUBTITLE_VERSION_BROADCASTER_STUPIDITY
            if (version == ddsVersionNumber)
               break; // no update
#endif
            bool displayWindowFlag = bs.GetBit();
            windowHorizontalOffset = 0;
            windowVerticalOffset   = 0;
            bs.SkipBits(3); // reserved
            displayWidth  = windowWidth  = bs.GetBits(16) + 1;
            displayHeight = windowHeight = bs.GetBits(16) + 1;
            if (displayWindowFlag) {
               windowHorizontalOffset = bs.GetBits(16);                              // displayWindowHorizontalPositionMinimum
               windowWidth            = bs.GetBits(16) - windowHorizontalOffset + 1; // displayWindowHorizontalPositionMaximum
               windowVerticalOffset   = bs.GetBits(16);                              // displayWindowVerticalPositionMinimum
               windowHeight           = bs.GetBits(16) - windowVerticalOffset + 1;   // displayWindowVerticalPositionMaximum
               }
            SetOsdData();
            ddsVersionNumber = version;
            dbgdisplay("<b>display</b> version %d flag %d width %d height %d ofshor %d ofsver %d<br>\n", ddsVersionNumber, displayWindowFlag, windowWidth, windowHeight, windowHorizontalOffset, windowVerticalOffset);
            break;
            }
       case DISPARITY_SIGNALING_SEGMENT: {
            dbgsegments("DISPARITY_SIGNALING_SEGMENT<br>\n");
            bs.SkipBits(4); // dss_version_number
            bool disparity_shift_update_sequence_page_flag = bs.GetBit();
            bs.SkipBits(3); // reserved
            bs.SkipBits(8); // page_default_disparity_shift
            if (disparity_shift_update_sequence_page_flag) {
               bs.SkipBits(8); // disparity_shift_update_sequence_length
               bs.SkipBits(24); // interval_duration[23..0]
               int division_period_count = bs.GetBits(8);
               for (int i = 0; i < division_period_count; ++i) {
                   bs.SkipBits(8); // interval_count
                   bs.SkipBits(8); // disparity_shift_update_integer_part
                   }
               }
            while (!bs.IsEOF()) {
                  bs.SkipBits(8); // region_id
                  bool disparity_shift_update_sequence_region_flag = bs.GetBit();
                  bs.SkipBits(5); // reserved
                  int number_of_subregions_minus_1 = bs.GetBits(2);
                  for (int i = 0; i <= number_of_subregions_minus_1; ++i) {
                      if (number_of_subregions_minus_1 > 0) {
                         bs.SkipBits(16); // subregion_horizontal_position
                         bs.SkipBits(16); // subregion_width
                         }
                      bs.SkipBits(8); // subregion_disparity_shift_integer_part
                      bs.SkipBits(4); // subregion_disparity_shift_fractional_part
                      bs.SkipBits(4); // reserved
                      if (disparity_shift_update_sequence_region_flag) {
                         bs.SkipBits(8); // disparity_shift_update_sequence_length
                         bs.SkipBits(24); // interval_duration[23..0]
                         int division_period_count = bs.GetBits(8);
                         for (int i = 0; i < division_period_count; ++i) {
                             bs.SkipBits(8); // interval_count
                             bs.SkipBits(8); // disparity_shift_update_integer_part
                             }
                         }
                      }
                  }
            break;
            }
       case END_OF_DISPLAY_SET_SEGMENT: {
            dbgsegments("END_OF_DISPLAY_SET_SEGMENT<br>\n");
            FinishPage(page);
            page->SetPending(false);
            break;
            }
       default:
            dbgsegments("*** unknown segment type: %02X<br>\n", segmentType);
       }
     return bs.Length() / 8;
     }
  return -1;
}

int cDvbSubtitleConverter::ExtractPgsSegment(const uchar *Data, int Length, int64_t Pts)
{
  cBitStream bs(Data, Length * 8);
  if (Length >= 3) {
     int segmentType = bs.GetBits(8);
     int segmentLength = bs.GetBits(16);
     if (!bs.SetLength(bs.Index() + segmentLength * 8))
        return -1;
     LOCK_THREAD;
     cDvbSubtitlePage *page = GetPageById(0, true);
     switch (segmentType) {
       case PGS_PRESENTATION_SEGMENT: {
            if (page->Pending()) {
               dbgsegments("PGS_DISPLAY_SEGMENT (simulated)<br>\n");
               FinishPage(page);
               }
            dbgsegments("PGS_PRESENTATION_SEGMENT<br>\n");
            displayWidth  = windowWidth  = bs.GetBits(16);
            displayHeight = windowHeight = bs.GetBits(16);
            bs.SkipBits(8);
            page->ParsePgs(Pts, bs);
            SD.SetFactor(double(DBGBITMAPWIDTH) / windowWidth);
            cSubtitleRegion *region = page->GetRegionById(0, true);
            region->ParsePgs(bs);
            break;
            }
       case PGS_WINDOW_SEGMENT: {
            bs.SkipBits(16);
            int regionHorizontalAddress = bs.GetBits(16);
            int regionVerticalAddress   = bs.GetBits(16);
            int regionWidth  = bs.GetBits(16);
            int regionHeight = bs.GetBits(16);
            cSubtitleRegion *region = page->GetRegionById(0, true);
            region->SetDimensions(regionWidth, regionHeight);
            page->AddRegionRef(new cSubtitleRegionRef(0, regionHorizontalAddress, regionVerticalAddress));
            dbgsegments("PGS_WINDOW_SEGMENT<br>\n");
            break;
            }
       case PGS_PALETTE_SEGMENT: {
            dbgsegments("PGS_PALETTE_SEGMENT<br>\n");
            cSubtitleClut *clut = page->GetClutById(bs.GetBits(8), true);
            clut->ParsePgs(bs);
            break;
            }
       case PGS_OBJECT_SEGMENT: {
            dbgsegments("PGS_OBJECT_SEGMENT<br>\n");
            cSubtitleObject *object = page->GetObjectById(bs.GetBits(16), true);
            object->ParsePgs(bs);
            break;
            }
       case PGS_DISPLAY_SEGMENT: {
            dbgsegments("PGS_DISPLAY_SEGMENT<br>\n");
            FinishPage(page);
            page->SetPending(false);
            break;
            }
       default:
            dbgsegments("*** unknown segment type: %02X<br>\n", segmentType);
            return -1;
       }
     return bs.Length() / 8;
     }
  return -1;
}

void cDvbSubtitleConverter::FinishPage(cDvbSubtitlePage *Page)
{
  if (!AssertOsd())
     return;
  int NumAreas;
  tArea *Areas = Page->GetAreas(NumAreas);
  if (!Areas)
     return;
  tArea AreaCombined = Page->CombineAreas(NumAreas, Areas);
  tArea AreaOsd = Page->ScaleArea(AreaCombined, osdFactorX, osdFactorY);
  int Bpp = 8;
  bool Reduced = false;
  if (osd) {
     while (osd->CanHandleAreas(&AreaOsd, 1) != oeOk) {
           dbgoutput("CanHandleAreas: %d<br>\n", osd->CanHandleAreas(&AreaOsd, 1));
           int HalfBpp = Bpp / 2;
           if (HalfBpp >= 2) {
              if (AreaOsd.bpp >= Bpp) {
                  AreaOsd.bpp = HalfBpp;
                  Reduced = true;
                  }
              Bpp = HalfBpp;
              }
           else
              return; // unable to draw bitmaps
           }
     }
  cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(Page->PageState(), Page->Pts(), Page->PageTimeout(), Areas, NumAreas, osdFactorX, osdFactorY, AreaCombined, AreaOsd);
  bitmaps->Add(Bitmaps);
  for (int i = 0; i < NumAreas; i++) {
      if (cSubtitleRegionRef *srr = Page->GetRegionRefByIndex(i)) {
         if (cSubtitleRegion *sr = Page->GetRegionById(srr->RegionId())) {
            if (cSubtitleClut *clut = Page->GetClutById(sr->ClutId())) {
               cBitmap *bm = new cBitmap(sr->RegionWidth(), sr->RegionHeight(), sr->RegionDepth());
               bm->Replace(*clut->GetPalette(sr->RegionDepth()));
               sr->Render(bm, Page->Objects());
               if (Reduced) {
                  if (sr->RegionDepth() != Areas[i].bpp) {
                     if (sr->RegionLevelOfCompatibility() <= Areas[i].bpp) {
                        //TODO this is untested - didn't have any such subtitle stream
                        cSubtitleClut *Clut = Page->GetClutById(sr->ClutId());
                        dbgregions("reduce region %d bpp %d level %d area bpp %d<br>\n", sr->RegionId(), sr->RegionDepth(), sr->RegionLevelOfCompatibility(), Areas[i].bpp);
                        bm->ReduceBpp(*Clut->GetPalette(sr->RegionDepth()));
                        }
                     else {
                        dbgregions("condense region %d bpp %d level %d area bpp %d<br>\n", sr->RegionId(), sr->RegionDepth(), sr->RegionLevelOfCompatibility(), Areas[i].bpp);
                        bm->ShrinkBpp(Areas[i].bpp);
                        }
                     }
                  }
               bm->SetOffset(srr->RegionHorizontalAddress(), srr->RegionVerticalAddress());
               Bitmaps->AddBitmap(bm);
               }
            }
         }
      }
  if (DebugPages)
     Bitmaps->DbgDump(windowWidth, windowHeight);
}
