/*
 * dvbsubtitle.c: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schluessler <marco@lordzodiac.de>
 * With some input from the "subtitle plugin" by Pekka Virtanen <pekka.virtanen@sci.fi>
 *
 * $Id: dvbsubtitle.c 2.31 2012/03/16 11:56:56 kls Exp $
 */


#include "dvbsubtitle.h"
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include "device.h"
#include "libsi/si.h"

//#define FINISHPAGE_HACK

#define PAGE_COMPOSITION_SEGMENT    0x10
#define REGION_COMPOSITION_SEGMENT  0x11
#define CLUT_DEFINITION_SEGMENT     0x12
#define OBJECT_DATA_SEGMENT         0x13
#define DISPLAY_DEFINITION_SEGMENT  0x14
#define DISPARITY_SIGNALING_SEGMENT 0x15 // DVB BlueBook A156
#define END_OF_DISPLAY_SET_SEGMENT  0x80
#define STUFFING_SEGMENT            0xFF

// Set these to 'true' for debug output:
static bool DebugConverter = false;
static bool DebugSegments = false;
static bool DebugPages = false;
static bool DebugRegions = false;
static bool DebugObjects = false;
static bool DebugCluts = false;

#define dbgconverter(a...) if (DebugConverter) fprintf(stderr, a)
#define dbgsegments(a...) if (DebugSegments) fprintf(stderr, a)
#define dbgpages(a...) if (DebugPages) fprintf(stderr, a)
#define dbgregions(a...) if (DebugRegions) fprintf(stderr, a)
#define dbgobjects(a...) if (DebugObjects) fprintf(stderr, a)
#define dbgcluts(a...) if (DebugCluts) fprintf(stderr, a)

// --- cSubtitleClut ---------------------------------------------------------

class cSubtitleClut : public cListObject {
private:
  int clutId;
  int version;
  cPalette palette2;
  cPalette palette4;
  cPalette palette8;
public:
  cSubtitleClut(int ClutId);
  int ClutId(void) { return clutId; }
  int Version(void) { return version; }
  void SetVersion(int Version) { version = Version; }
  void SetColor(int Bpp, int Index, tColor Color);
  const cPalette *GetPalette(int Bpp);
  };

cSubtitleClut::cSubtitleClut(int ClutId)
:palette2(2)
,palette4(4)
,palette8(8)
{
  int a = 0, r = 0, g = 0, b = 0;
  clutId = ClutId;
  version = -1;
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
  int version;
  int codingMethod;
  bool nonModifyingColorFlag;
  uchar backgroundPixelCode;
  uchar foregroundPixelCode;
  int providerFlag;
  int px;
  int py;
  cBitmap *bitmap;
  char textData[Utf8BufSize(256)]; // number of character codes is an 8-bit field
  void DrawLine(int x, int y, tIndex Index, int Length);
  bool Decode2BppCodeString(cBitStream *bs, int&x, int y, const uint8_t *MapTable);
  bool Decode4BppCodeString(cBitStream *bs, int&x, int y, const uint8_t *MapTable);
  bool Decode8BppCodeString(cBitStream *bs, int&x, int y);
public:
  cSubtitleObject(int ObjectId, cBitmap *Bitmap);
  int ObjectId(void) { return objectId; }
  int Version(void) { return version; }
  int CodingMethod(void) { return codingMethod; }
  uchar BackgroundPixelCode(void) { return backgroundPixelCode; }
  uchar ForegroundPixelCode(void) { return foregroundPixelCode; }
  const char *TextData(void) { return &textData[0]; }
  int X(void) { return px; }
  int Y(void) { return py; }
  bool NonModifyingColorFlag(void) { return nonModifyingColorFlag; }
  void DecodeCharacterString(const uchar *Data, int NumberOfCodes);
  void DecodeSubBlock(const uchar *Data, int Length, bool Even);
  void SetVersion(int Version) { version = Version; }
  void SetBackgroundPixelCode(uchar BackgroundPixelCode) { backgroundPixelCode = BackgroundPixelCode; }
  void SetForegroundPixelCode(uchar ForegroundPixelCode) { foregroundPixelCode = ForegroundPixelCode; }
  void SetNonModifyingColorFlag(bool NonModifyingColorFlag) { nonModifyingColorFlag = NonModifyingColorFlag; }
  void SetCodingMethod(int CodingMethod) { codingMethod = CodingMethod; }
  void SetPosition(int x, int y) { px = x; py = y; }
  void SetProviderFlag(int ProviderFlag) { providerFlag = ProviderFlag; }
  };

cSubtitleObject::cSubtitleObject(int ObjectId, cBitmap *Bitmap)
{
  objectId = ObjectId;
  version = -1;
  codingMethod = -1;
  nonModifyingColorFlag = false;
  backgroundPixelCode = 0;
  foregroundPixelCode = 0;
  providerFlag = -1;
  px = py = 0;
  bitmap = Bitmap;
  memset(textData, 0, sizeof(textData));
}

void cSubtitleObject::DecodeCharacterString(const uchar *Data, int NumberOfCodes)
{
  if (NumberOfCodes > 0) {
     bool singleByte;
     const uchar *from = &Data[1];
     int len = NumberOfCodes * 2 - 1;
     cCharSetConv conv(SI::getCharacterTable(from, len, &singleByte));
     if (singleByte) {
        char txt[NumberOfCodes + 1];
        char *p = txt;
        for (int i = 2; i < NumberOfCodes; ++i) {
            char c = Data[i * 2 + 1] & 0xFF;
            if (c == 0)
               break;
            if (' ' <= c && c <= '~' || c == '\n' || 0xA0 <= c)
               *(p++) = c;
            else if (c == 0x8A)
               *(p++) = '\n';
            }
        *p = 0;
        const char *s = conv.Convert(txt);
        Utf8Strn0Cpy(textData, s, Utf8StrLen(s));
        }
     else {
        // TODO: add proper multibyte support for "UTF-16", "EUC-KR", "GB2312", "GBK", "UTF-8"
        }
     }
}

void cSubtitleObject::DecodeSubBlock(const uchar *Data, int Length, bool Even)
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
               dbgobjects("2-bit / pixel code string\n");
               switch (bitmap->Bpp()) {
                 case 8:  mapTable = map2to8; break;
                 case 4:  mapTable = map2to4; break;
                 default: mapTable = NULL;    break;
                 }
               while (Decode2BppCodeString(&bs, x, y, mapTable) && !bs.IsEOF())
                     ;
               bs.ByteAlign();
               break;
          case 0x11:
               dbgobjects("4-bit / pixel code string\n");
               switch (bitmap->Bpp()) {
                 case 8:  mapTable = map4to8; break;
                 default: mapTable = NULL;    break;
                 }
               while (Decode4BppCodeString(&bs, x, y, mapTable) && !bs.IsEOF())
                     ;
               bs.ByteAlign();
               break;
          case 0x12:
               dbgobjects("8-bit / pixel code string\n");
               while (Decode8BppCodeString(&bs, x, y) && !bs.IsEOF())
                     ;
               break;
          case 0x20:
               dbgobjects("sub block 2 to 4 map\n");
               map2to4[0] = bs.GetBits(4);
               map2to4[1] = bs.GetBits(4);
               map2to4[2] = bs.GetBits(4);
               map2to4[3] = bs.GetBits(4);
               break;
          case 0x21:
               dbgobjects("sub block 2 to 8 map\n");
               for (int i = 0; i < 4; ++i)
                   map2to8[i] = bs.GetBits(8);
               break;
          case 0x22:
               dbgobjects("sub block 4 to 8 map\n");
               for (int i = 0; i < 16; ++i)
                   map4to8[i] = bs.GetBits(8);
               break;
          case 0xF0:
               dbgobjects("end of object line\n");
               x = 0;
               y += 2;
               break;
          default: dbgobjects("unknown sub block %s %d\n", __FUNCTION__, __LINE__);
          }
        }
}

void cSubtitleObject::DrawLine(int x, int y, tIndex Index, int Length)
{
  if (nonModifyingColorFlag && Index == 1)
     return;
  x += px;
  y += py;
  for (int pos = x; pos < x + Length; pos++)
      bitmap->SetIndex(pos, y, Index);
}

bool cSubtitleObject::Decode2BppCodeString(cBitStream *bs, int &x, int y, const uint8_t *MapTable)
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
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode4BppCodeString(cBitStream *bs, int &x, int y, const uint8_t *MapTable)
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
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

bool cSubtitleObject::Decode8BppCodeString(cBitStream *bs, int &x, int y)
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
  DrawLine(x, y, color, rl);
  x += rl;
  return true;
}

// --- cSubtitleRegion -------------------------------------------------------

class cSubtitleRegion : public cListObject, public cBitmap {
private:
  int regionId;
  int version;
  int clutId;
  int horizontalAddress;
  int verticalAddress;
  int level;
  int lineHeight;
  cList<cSubtitleObject> objects;
public:
  cSubtitleRegion(int RegionId);
  int RegionId(void) { return regionId; }
  int Version(void) { return version; }
  int ClutId(void) { return clutId; }
  int Level(void) { return level; }
  int Depth(void) { return Bpp(); }
  void FillRegion(tIndex Index);
  cSubtitleObject *GetObjectById(int ObjectId, bool New = false);
  int HorizontalAddress(void) { return horizontalAddress; }
  int VerticalAddress(void) { return verticalAddress; }
  void SetVersion(int Version) { version = Version; }
  void SetClutId(int ClutId) { clutId = ClutId; }
  void SetLevel(int Level);
  void SetDepth(int Depth);
  void SetHorizontalAddress(int HorizontalAddress) { horizontalAddress = HorizontalAddress; }
  void SetVerticalAddress(int VerticalAddress) { verticalAddress = VerticalAddress; }
  void UpdateTextData(cSubtitleClut *Clut);
  };

cSubtitleRegion::cSubtitleRegion(int RegionId)
:cBitmap(1, 1, 4)
{
  regionId = RegionId;
  version = -1;
  clutId = -1;
  horizontalAddress = 0;
  verticalAddress = 0;
  level = 0;
  lineHeight = 26; // configurable subtitling font size
}

void cSubtitleRegion::FillRegion(tIndex Index)
{
  dbgregions("FillRegion %d\n", Index);
  for (int y = 0; y < Height(); y++) {
      for (int x = 0; x < Width(); x++)
          SetIndex(x, y, Index);
      }
}

cSubtitleObject *cSubtitleRegion::GetObjectById(int ObjectId, bool New)
{
  cSubtitleObject *result = NULL;
  for (cSubtitleObject *so = objects.First(); so; so = objects.Next(so)) {
      if (so->ObjectId() == ObjectId)
         result = so;
      }
  if (!result && New) {
     result = new cSubtitleObject(ObjectId, this);
     objects.Add(result);
     }
  return result;
}

void cSubtitleRegion::UpdateTextData(cSubtitleClut *Clut)
{
  const cPalette *palette = Clut ? Clut->GetPalette(Depth()) : NULL;
  for (cSubtitleObject *so = objects.First(); so && palette; so = objects.Next(so)) {
      if (Utf8StrLen(so->TextData()) > 0) {
         cFont *font = cFont::CreateFont(Setup.FontOsd, Setup.FontOsdSize);
         cBitmap tmp(font->Width(so->TextData()), font->Height(), Depth());
         double factor = (double)lineHeight / font->Height();
         tmp.DrawText(0, 0, so->TextData(), palette->Color(so->ForegroundPixelCode()), palette->Color(so->BackgroundPixelCode()), font);
         cBitmap *scaled = tmp.Scaled(factor, factor, true);
         DrawBitmap(so->X(), so->Y(), *scaled);
         delete scaled;
         delete font;
         }
      }
}

void cSubtitleRegion::SetLevel(int Level)
{
  if (Level > 0 && Level < 4)
     level = 1 << Level;
}

void cSubtitleRegion::SetDepth(int Depth)
{
  if (Depth > 0 && Depth < 4)
     SetBpp(1 << Depth);
}

// --- cDvbSubtitlePage ------------------------------------------------------

class cDvbSubtitlePage : public cListObject {
private:
  int pageId;
  int version;
  int state;
  int64_t pts;
  int timeout;
  cList<cSubtitleClut> cluts;
public:
  cList<cSubtitleRegion> regions;
  cDvbSubtitlePage(int PageId);
  virtual ~cDvbSubtitlePage();
  int PageId(void) { return pageId; }
  int Version(void) { return version; }
  int State(void) { return state; }
  tArea *GetAreas(double FactorX, double FactorY);
  cSubtitleClut *GetClutById(int ClutId, bool New = false);
  cSubtitleObject *GetObjectById(int ObjectId);
  cSubtitleRegion *GetRegionById(int RegionId, bool New = false);
  int64_t Pts(void) const { return pts; }
  int Timeout(void) { return timeout; }
  void SetVersion(int Version) { version = Version; }
  void SetPts(int64_t Pts) { pts = Pts; }
  void SetState(int State);
  void SetTimeout(int Timeout) { timeout = Timeout; }
  };

cDvbSubtitlePage::cDvbSubtitlePage(int PageId)
{
  pageId = PageId;
  version = -1;
  state = -1;
  pts = 0;
  timeout = 0;
}

cDvbSubtitlePage::~cDvbSubtitlePage()
{
}

tArea *cDvbSubtitlePage::GetAreas(double FactorX, double FactorY)
{
  if (regions.Count() > 0) {
     tArea *Areas = new tArea[regions.Count()];
     tArea *a = Areas;
     for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
         a->x1 = int(round(FactorX * sr->HorizontalAddress()));
         a->y1 = int(round(FactorY * sr->VerticalAddress()));
         a->x2 = int(round(FactorX * (sr->HorizontalAddress() + sr->Width() - 1)));
         a->y2 = int(round(FactorY * (sr->VerticalAddress() + sr->Height() - 1)));
         a->bpp = sr->Bpp();
         while ((a->Width() & 3) != 0)
               a->x2++; // aligns width to a multiple of 4, so 2, 4 and 8 bpp will work
         a++;
         }
     return Areas;
     }
  return NULL;
}

cSubtitleClut *cDvbSubtitlePage::GetClutById(int ClutId, bool New)
{
  cSubtitleClut *result = NULL;
  for (cSubtitleClut *sc = cluts.First(); sc; sc = cluts.Next(sc)) {
      if (sc->ClutId() == ClutId)
         result = sc;
      }
  if (!result && New) {
     result = new cSubtitleClut(ClutId);
     cluts.Add(result);
     }
  return result;
}

cSubtitleRegion *cDvbSubtitlePage::GetRegionById(int RegionId, bool New)
{
  cSubtitleRegion *result = NULL;
  for (cSubtitleRegion *sr = regions.First(); sr; sr = regions.Next(sr)) {
      if (sr->RegionId() == RegionId)
         result = sr;
      }
  if (!result && New) {
     result = new cSubtitleRegion(RegionId);
     regions.Add(result);
     }
  return result;
}

cSubtitleObject *cDvbSubtitlePage::GetObjectById(int ObjectId)
{
  cSubtitleObject *result = NULL;
  for (cSubtitleRegion *sr = regions.First(); sr && !result; sr = regions.Next(sr))
      result = sr->GetObjectById(ObjectId);
  return result;
}

void cDvbSubtitlePage::SetState(int State)
{
  state = State;
  switch (state) {
    case 0: // normal case - page update
         dbgpages("page update\n");
         break;
    case 1: // acquisition point - page refresh
         dbgpages("page refresh\n");
         regions.Clear();
         break;
    case 2: // mode change - new page
         dbgpages("new Page\n");
         regions.Clear();
         cluts.Clear();
         break;
    case 3: // reserved
         break;
    default: dbgpages("unknown page state (%s %d)\n", __FUNCTION__, __LINE__);
    }
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
  int64_t pts;
  int timeout;
  tArea *areas;
  int numAreas;
  double osdFactorX;
  double osdFactorY;
  cVector<cBitmap *> bitmaps;
public:
  cDvbSubtitleBitmaps(int64_t Pts, int Timeout, tArea *Areas, int NumAreas, double OsdFactorX, double OsdFactorY);
  ~cDvbSubtitleBitmaps();
  int64_t Pts(void) { return pts; }
  int Timeout(void) { return timeout; }
  void AddBitmap(cBitmap *Bitmap);
  void Draw(cOsd *Osd);
  };

cDvbSubtitleBitmaps::cDvbSubtitleBitmaps(int64_t Pts, int Timeout, tArea *Areas, int NumAreas, double OsdFactorX, double OsdFactorY)
{
  pts = Pts;
  timeout = Timeout;
  areas = Areas;
  numAreas = NumAreas;
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
  bool AntiAlias = true;
  if (Scale && osdFactorX > 1.0 || osdFactorY > 1.0) {
     // Upscaling requires 8bpp:
     int Bpp[MAXOSDAREAS];
     for (int i = 0; i < numAreas; i++) {
         Bpp[i] = areas[i].bpp;
         areas[i].bpp = 8;
         }
     if (Osd->CanHandleAreas(areas, numAreas) != oeOk) {
        for (int i = 0; i < numAreas; i++)
            Bpp[i] = areas[i].bpp = Bpp[i];
        AntiAlias = false;
        }
     }
  if (Osd->SetAreas(areas, numAreas) == oeOk) {
     for (int i = 0; i < bitmaps.Size(); i++) {
         cBitmap *b = bitmaps[i];
         if (Scale)
            b = b->Scaled(osdFactorX, osdFactorY, AntiAlias);
         Osd->DrawBitmap(int(round(b->X0() * osdFactorX)), int(round(b->Y0() * osdFactorY)), *b);
         if (b != bitmaps[i])
            delete b;
         }
     Osd->Flush();
     }
}

// --- cDvbSubtitleConverter -------------------------------------------------

int cDvbSubtitleConverter::setupLevel = 0;

cDvbSubtitleConverter::cDvbSubtitleConverter(void)
:cThread("subtitleConverter")
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
  dbgconverter("Converter reset -----------------------\n");
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
        int64_t pts = PesHasPts(Data) ? PesGetPts(Data) : 0;
        if (pts)
           dbgconverter("Converter PTS: %"PRId64"\n", pts);
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
        int64_t pts = PesGetPts(Data);
        if (pts)
           dbgconverter("Converter PTS: %"PRId64"\n", pts);
        const uchar *data = Data + PayloadOffset;
        int length = Length - PayloadOffset;
        if (length > 3) {
           if (data[0] == 0x20 && data[1] == 0x00 && data[2] == 0x0F) {
              data += 2;
              length -= 2;
              }
           const uchar *b = data;
           while (length > 0) {
                 if (b[0] == 0x0F) {
                    int n = ExtractSegment(b, length, pts);
                    if (n < 0)
                       break;
                    b += n;
                    length -= n;
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

#define LimitTo32Bit(n) ((n) & 0x00000000FFFFFFFFL)
#define MAXDELTA 40000 // max. reasonable PTS/STC delta in ms

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
                 DELETENULL(osd);
                 }
              LastSetupLevel = NewSetupLevel;
              }
           if (cDvbSubtitleBitmaps *sb = bitmaps->First()) {
              int64_t STC = cDevice::PrimaryDevice()->GetSTC();
              int64_t Delta = LimitTo32Bit(sb->Pts()) - LimitTo32Bit(STC); // some devices only deliver 32 bits
              if (Delta > (int64_t(1) << 31))
                 Delta -= (int64_t(1) << 32);
              else if (Delta < -((int64_t(1) << 31) - 1))
                 Delta += (int64_t(1) << 32);
              Delta /= 90; // STC and PTS are in 1/90000s
              if (Delta <= MAXDELTA) {
                 if (Delta <= 0) {
                    dbgconverter("Got %d bitmaps, showing #%d\n", bitmaps->Count(), sb->Index() + 1);
                    if (AssertOsd()) {
                       sb->Draw(osd);
                       Timeout.Set(sb->Timeout() * 1000);
                       dbgconverter("PTS: %"PRId64"  STC: %"PRId64" (%"PRId64") timeout: %d\n", sb->Pts(), cDevice::PrimaryDevice()->GetSTC(), Delta, sb->Timeout());
                       }
                    bitmaps->Del(sb);
                    }
                 else if (Delta < WaitMs)
                    WaitMs = Delta;
                 }
              else
                 bitmaps->Del(sb);
              }
           }
        cCondWait::SleepMs(WaitMs);
        }
}

tColor cDvbSubtitleConverter::yuv2rgb(int Y, int Cb, int Cr)
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
     osdFactorX = VideoAspect * OsdHeight / displayWidth;
     osdFactorY = double(OsdHeight) / displayHeight;
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

int cDvbSubtitleConverter::ExtractSegment(const uchar *Data, int Length, int64_t Pts)
{
  cBitStream bs(Data, Length * 8);
  if (Length > 5 && bs.GetBits(8) == 0x0F) { // sync byte
     int segmentType = bs.GetBits(8);
     if (segmentType == STUFFING_SEGMENT)
        return -1;
     int pageId = bs.GetBits(16);
     int segmentLength = bs.GetBits(16);
     if (!bs.SetLength(bs.Index() + segmentLength * 8))
        return -1;
     cDvbSubtitlePage *page = NULL;
     LOCK_THREAD;
     for (cDvbSubtitlePage *sp = pages->First(); sp; sp = pages->Next(sp)) {
         if (sp->PageId() == pageId) {
            page = sp;
            break;
            }
         }
     if (!page) {
        page = new cDvbSubtitlePage(pageId);
        pages->Add(page);
        dbgpages("Create SubtitlePage %d (total pages = %d)\n", pageId, pages->Count());
        }
     if (Pts)
        page->SetPts(Pts);
     switch (segmentType) {
       case PAGE_COMPOSITION_SEGMENT: {
            dbgsegments("PAGE_COMPOSITION_SEGMENT\n");
            int pageTimeout = bs.GetBits(8);
            int pageVersion = bs.GetBits(4);
            if (pageVersion == page->Version())
               break; // no update
            page->SetVersion(pageVersion);
            page->SetTimeout(pageTimeout);
            page->SetState(bs.GetBits(2));
            bs.SkipBits(2); // reserved
            dbgpages("Update page id %d version %d pts %"PRId64" timeout %d state %d\n", pageId, page->Version(), page->Pts(), page->Timeout(), page->State());
            while (!bs.IsEOF()) {
                  cSubtitleRegion *region = page->GetRegionById(bs.GetBits(8), true);
                  bs.SkipBits(8); // reserved
                  region->SetHorizontalAddress(bs.GetBits(16));
                  region->SetVerticalAddress(bs.GetBits(16));
                  }
            break;
            }
       case REGION_COMPOSITION_SEGMENT: {
            dbgsegments("REGION_COMPOSITION_SEGMENT\n");
            cSubtitleRegion *region = page->GetRegionById(bs.GetBits(8));
            if (!region)
               break;
            int regionVersion = bs.GetBits(4);
            if (regionVersion == region->Version())
               break; // no update
            region->SetVersion(regionVersion);
            bool regionFillFlag = bs.GetBit();
            bs.SkipBits(3); // reserved
            int regionWidth = bs.GetBits(16);
            if (regionWidth < 1)
               regionWidth = 1;
            int regionHeight = bs.GetBits(16);
            if (regionHeight < 1)
               regionHeight = 1;
            region->SetSize(regionWidth, regionHeight);
            region->SetLevel(bs.GetBits(3));
            region->SetDepth(bs.GetBits(3));
            bs.SkipBits(2); // reserved
            region->SetClutId(bs.GetBits(8));
            dbgregions("Region pageId %d id %d version %d fill %d width %d height %d level %d depth %d clutId %d\n", pageId, region->RegionId(), region->Version(), regionFillFlag, regionWidth, regionHeight, region->Level(), region->Depth(), region->ClutId());
            int region8bitPixelCode = bs.GetBits(8);
            int region4bitPixelCode = bs.GetBits(4);
            int region2bitPixelCode = bs.GetBits(2);
            bs.SkipBits(2); // reserved
            if (regionFillFlag) {
               switch (region->Bpp()) {
                 case 2: region->FillRegion(region8bitPixelCode); break;
                 case 4: region->FillRegion(region4bitPixelCode); break;
                 case 8: region->FillRegion(region2bitPixelCode); break;
                 default: dbgregions("unknown bpp %d (%s %d)\n", region->Bpp(), __FUNCTION__, __LINE__);
                 }
               }
            while (!bs.IsEOF()) {
                  cSubtitleObject *object = region->GetObjectById(bs.GetBits(16), true);
                  int objectType = bs.GetBits(2);
                  object->SetCodingMethod(objectType);
                  object->SetProviderFlag(bs.GetBits(2));
                  int objectHorizontalPosition = bs.GetBits(12);
                  bs.SkipBits(4); // reserved
                  int objectVerticalPosition = bs.GetBits(12);
                  object->SetPosition(objectHorizontalPosition, objectVerticalPosition);
                  if (objectType == 0x01 || objectType == 0x02) {
                     object->SetForegroundPixelCode(bs.GetBits(8));
                     object->SetBackgroundPixelCode(bs.GetBits(8));
                     }
                  }
            break;
            }
       case CLUT_DEFINITION_SEGMENT: {
            dbgsegments("CLUT_DEFINITION_SEGMENT\n");
            cSubtitleClut *clut = page->GetClutById(bs.GetBits(8), true);
            int clutVersion = bs.GetBits(4);
            if (clutVersion == clut->Version())
               break; // no update
            clut->SetVersion(clutVersion);
            bs.SkipBits(4); // reserved
            dbgcluts("Clut pageId %d id %d version %d\n", pageId, clut->ClutId(), clut->Version());
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
                  dbgcluts("%2d %d %d %d %08X\n", clutEntryId, entryClut2Flag ? 2 : 0, entryClut4Flag ? 4 : 0, entryClut8Flag ? 8 : 0, value);
                  if (entryClut2Flag)
                     clut->SetColor(2, clutEntryId, value);
                  if (entryClut4Flag)
                     clut->SetColor(4, clutEntryId, value);
                  if (entryClut8Flag)
                     clut->SetColor(8, clutEntryId, value);
                  }
            dbgcluts("\n");
            break;
            }
       case OBJECT_DATA_SEGMENT: {
            dbgsegments("OBJECT_DATA_SEGMENT\n");
            cSubtitleObject *object = page->GetObjectById(bs.GetBits(16));
            if (!object)
               break;
            int objectVersion = bs.GetBits(4);
            if (objectVersion == object->Version())
               break; // no update
            object->SetVersion(objectVersion);
            int codingMethod = bs.GetBits(2);
            object->SetNonModifyingColorFlag(bs.GetBit());
            bs.SkipBit(); // reserved
            dbgobjects("Object pageId %d id %d version %d method %d modify %d\n", pageId, object->ObjectId(), object->Version(), object->CodingMethod(), object->NonModifyingColorFlag());
            if (codingMethod == 0) { // coding of pixels
               int topFieldLength = bs.GetBits(16);
               int bottomFieldLength = bs.GetBits(16);
               object->DecodeSubBlock(bs.GetData(), topFieldLength, true);
               if (bottomFieldLength)
                  object->DecodeSubBlock(bs.GetData() + topFieldLength, bottomFieldLength, false);
               else
                  object->DecodeSubBlock(bs.GetData(), topFieldLength, false);
               bs.WordAlign();
               }
            else if (codingMethod == 1) { // coded as a string of characters
               int numberOfCodes = bs.GetBits(8);
               object->DecodeCharacterString(bs.GetData(), numberOfCodes);
               }
#ifdef FINISHPAGE_HACK
            FinishPage(page); // flush to OSD right away
#endif
            break;
            }
       case DISPLAY_DEFINITION_SEGMENT: {
            dbgsegments("DISPLAY_DEFINITION_SEGMENT\n");
            int version = bs.GetBits(4);
            if (version != ddsVersionNumber) {
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
               SetupChanged();
               ddsVersionNumber = version;
               }
            break;
            }
       case DISPARITY_SIGNALING_SEGMENT: {
            dbgsegments("DISPARITY_SIGNALING_SEGMENT\n");
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
            dbgsegments("END_OF_DISPLAY_SET_SEGMENT\n");
            FinishPage(page);
            break;
            }
       default:
            dbgsegments("*** unknown segment type: %02X\n", segmentType);
       }
     return bs.Length() / 8;
     }
  return -1;
}

void cDvbSubtitleConverter::FinishPage(cDvbSubtitlePage *Page)
{
  if (!AssertOsd())
     return;
  tArea *Areas = Page->GetAreas(osdFactorX, osdFactorY);
  int NumAreas = Page->regions.Count();
  int Bpp = 8;
  bool Reduced = false;
  while (osd && osd->CanHandleAreas(Areas, NumAreas) != oeOk) {
        int HalfBpp = Bpp / 2;
        if (HalfBpp >= 2) {
           for (int i = 0; i < NumAreas; i++) {
               if (Areas[i].bpp >= Bpp) {
                  Areas[i].bpp = HalfBpp;
                  Reduced = true;
                  }
               }
           Bpp = HalfBpp;
           }
        else
           return; // unable to draw bitmaps
        }
  cDvbSubtitleBitmaps *Bitmaps = new cDvbSubtitleBitmaps(Page->Pts(), Page->Timeout(), Areas, NumAreas, osdFactorX, osdFactorY);
  bitmaps->Add(Bitmaps);
  for (int i = 0; i < NumAreas; i++) { 
      cSubtitleRegion *sr = Page->regions.Get(i);
      cSubtitleClut *clut = Page->GetClutById(sr->ClutId());
      if (!clut)
         continue;
      sr->Replace(*clut->GetPalette(sr->Bpp()));
      sr->UpdateTextData(clut);
      if (Reduced) {
         if (sr->Bpp() != Areas[i].bpp) {
            if (sr->Level() <= Areas[i].bpp) {
               //TODO this is untested - didn't have any such subtitle stream
               cSubtitleClut *Clut = Page->GetClutById(sr->ClutId());
               if (Clut) {
                  dbgregions("reduce region %d bpp %d level %d area bpp %d\n", sr->RegionId(), sr->Bpp(), sr->Level(), Areas[i].bpp);
                  sr->ReduceBpp(*Clut->GetPalette(sr->Bpp()));
                  }
               }
            else {
               dbgregions("condense region %d bpp %d level %d area bpp %d\n", sr->RegionId(), sr->Bpp(), sr->Level(), Areas[i].bpp);
               sr->ShrinkBpp(Areas[i].bpp);
               }
            }
         }
      int posX = sr->HorizontalAddress();
      int posY = sr->VerticalAddress();
      if (sr->Width() > 0 && sr->Height() > 0) {
         cBitmap *bm = new cBitmap(sr->Width(), sr->Height(), sr->Bpp(), posX, posY);
         bm->DrawBitmap(posX, posY, *sr);
         Bitmaps->AddBitmap(bm);
         }
      }
}
