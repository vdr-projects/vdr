/*
 * font.c: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * BiDi support by Osama Alrawab <alrawab@hotmail.com> @2008 Tripoli-Libya.
 *
 * $Id: font.c 5.3 2025/02/17 11:13:13 kls Exp $
 */

#include "font.h"
#include <ctype.h>
#include <fontconfig/fontconfig.h>
#ifdef BIDI
#include <fribidi.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include "config.h"
#include "osd.h"
#include "tools.h"

const char *DefaultFontOsd = "Sans Serif:Bold";
const char *DefaultFontSml = "Sans Serif";
const char *DefaultFontFix = "Courier:Bold";

// --- cFreetypeFont ---------------------------------------------------------

#define KERNING_UNKNOWN  (-10000)

struct tKerning {
  uint prevSym;
  int kerning;
  tKerning(uint PrevSym, int Kerning = 0) { prevSym = PrevSym; kerning = Kerning; }
  };

class cGlyph : public cListObject {
private:
  uint charCode;
  uchar *bitmap;
  int advanceX;
  int advanceY;
  int left;  ///< The bitmap's left bearing expressed in integer pixels.
  int top;   ///< The bitmap's top bearing expressed in integer pixels.
  int width; ///< The number of pixels per bitmap row.
  int rows;  ///< The number of bitmap rows.
  int pitch; ///< The pitch's absolute value is the number of bytes taken by one bitmap row, including padding.
  cVector<tKerning> kerningCache;
public:
  cGlyph(uint CharCode, FT_GlyphSlotRec_ *GlyphData);
  virtual ~cGlyph();
  uint CharCode(void) const { return charCode; }
  uchar *Bitmap(void) const { return bitmap; }
  int AdvanceX(void) const { return advanceX; }
  int AdvanceY(void) const { return advanceY; }
  int Left(void) const { return left; }
  int Top(void) const { return top; }
  int Width(void) const { return width; }
  int Rows(void) const { return rows; }
  int Pitch(void) const { return pitch; }
  int GetKerningCache(uint PrevSym) const;
  void SetKerningCache(uint PrevSym, int Kerning);
  };

cGlyph::cGlyph(uint CharCode, FT_GlyphSlotRec_ *GlyphData)
{
  charCode = CharCode;
  advanceX = GlyphData->advance.x >> 6;
  advanceY = GlyphData->advance.y >> 6;
  left = GlyphData->bitmap_left;
  top = GlyphData->bitmap_top;
  width = GlyphData->bitmap.width;
  rows = GlyphData->bitmap.rows;
  pitch = GlyphData->bitmap.pitch;
  bitmap = MALLOC(uchar, rows * pitch);
  if (int bytes = rows * pitch)
     memcpy(bitmap, GlyphData->bitmap.buffer, bytes);
}

cGlyph::~cGlyph()
{
  free(bitmap);
}

int cGlyph::GetKerningCache(uint PrevSym) const
{
  for (int i = kerningCache.Size(); --i > 0; ) {
      if (kerningCache[i].prevSym == PrevSym)
         return kerningCache[i].kerning;
      }
  return KERNING_UNKNOWN;
}

void cGlyph::SetKerningCache(uint PrevSym, int Kerning)
{
  kerningCache.Append(tKerning(PrevSym, Kerning));
}

class cFreetypeFont : public cFont {
private:
  cString fontName;
  int size;
  int width;
  int height;
  int bottom;
  FT_Library library; ///< Handle to library
  FT_Face face; ///< Handle to face object
  mutable cList<cGlyph> glyphCacheMonochrome;
  mutable cList<cGlyph> glyphCacheAntiAliased;
  int Bottom(void) const { return bottom; }
  int Kerning(cGlyph *Glyph, uint PrevSym) const;
  cGlyph* Glyph(uint CharCode, bool AntiAliased = false) const;
public:
  cFreetypeFont(const char *Name, int CharHeight, int CharWidth = 0);
  virtual ~cFreetypeFont();
  virtual const char *FontName(void) const { return fontName; }
  virtual int Size(void) const { return size; }
  virtual int Width(void) const { return width; }
  virtual int Width(uint c) const;
  virtual int Width(const char *s) const;
  virtual int Height(void) const { return height; }
  virtual void DrawText(cBitmap *Bitmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const;
  virtual void DrawText(cPixmap *Pixmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const;
  };

cFreetypeFont::cFreetypeFont(const char *Name, int CharHeight, int CharWidth)
{
  fontName = Name;
  size = CharHeight;
  width = CharWidth;
  height = 0;
  bottom = 0;
  int error = FT_Init_FreeType(&library);
  if (!error) {
     error = FT_New_Face(library, Name, 0, &face);
     if (!error) {
        if (face->num_fixed_sizes && face->available_sizes) { // fixed font
           // TODO what exactly does all this mean?
           height = face->available_sizes->height;
           for (uint sym ='A'; sym < 'z'; sym++) { // search for descender for fixed font FIXME
               FT_UInt glyph_index = FT_Get_Char_Index(face, sym);
               error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
               if (!error) {
                  error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                  if (!error) {
                     if (int(face->glyph->bitmap.rows-face->glyph->bitmap_top) > bottom)
                        bottom = face->glyph->bitmap.rows-face->glyph->bitmap_top;
                     }
                  else
                     esyslog("ERROR: FreeType: error %d in FT_Render_Glyph", error);
                  }
               else
                  esyslog("ERROR: FreeType: error %d in FT_Load_Glyph", error);
               }
           }
        else {
           error = FT_Set_Char_Size(face, // handle to face object
                                    CharWidth * 64,  // CharWidth in 1/64th of points
                                    CharHeight * 64, // CharHeight in 1/64th of points
                                    0,    // horizontal device resolution
                                    0);   // vertical device resolution
           if (!error) {
              height = (face->size->metrics.ascender - face->size->metrics.descender + 63) / 64;
              bottom = abs((face->size->metrics.descender - 63) / 64);
              }
           else
              esyslog("ERROR: FreeType: error %d during FT_Set_Char_Size (font = %s)\n", error, Name);
           }
        }
     else
        esyslog("ERROR: FreeType: load error %d (font = %s)", error, Name);
     }
  else
     esyslog("ERROR: FreeType: initialization error %d (font = %s)", error, Name);
}

cFreetypeFont::~cFreetypeFont()
{
  FT_Done_Face(face);
  FT_Done_FreeType(library);
}

int cFreetypeFont::Kerning(cGlyph *Glyph, uint PrevSym) const
{
  int kerning = 0;
  if (Glyph && PrevSym) {
     kerning = Glyph->GetKerningCache(PrevSym);
     if (kerning == KERNING_UNKNOWN) {
        FT_Vector delta;
        FT_UInt glyph_index = FT_Get_Char_Index(face, Glyph->CharCode());
        FT_UInt glyph_index_prev = FT_Get_Char_Index(face, PrevSym);
        FT_Get_Kerning(face, glyph_index_prev, glyph_index, FT_KERNING_DEFAULT, &delta);
        kerning = delta.x / 64;
        Glyph->SetKerningCache(PrevSym, kerning);
        }
     }
  return kerning;
}

cGlyph* cFreetypeFont::Glyph(uint CharCode, bool AntiAliased) const
{
  // Non-breaking space:
  if (CharCode == 0xA0)
     CharCode = 0x20;

  // Lookup in cache:
  cList<cGlyph> *glyphCache = AntiAliased ? &glyphCacheAntiAliased : &glyphCacheMonochrome;
  for (cGlyph *g = glyphCache->First(); g; g = glyphCache->Next(g)) {
      if (g->CharCode() == CharCode)
         return g;
      }

  FT_UInt glyph_index = FT_Get_Char_Index(face, CharCode);

  // Load glyph image into the slot (erase previous one):
  int error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
  if (error)
     esyslog("ERROR: FreeType: error during FT_Load_Glyph");
  else {
#if ((FREETYPE_MAJOR == 2 && FREETYPE_MINOR == 1 && FREETYPE_PATCH >= 7) || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR == 2 && FREETYPE_PATCH <= 1))// TODO workaround for bug? which one?
     if (AntiAliased || CharCode == 32)
#else
     if (AntiAliased)
#endif
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
     else
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
     if (error)
        esyslog("ERROR: FreeType: error during FT_Render_Glyph %d, %d\n", CharCode, glyph_index);
     else { //new bitmap
        cGlyph *Glyph = new cGlyph(CharCode, face->glyph);
        glyphCache->Add(Glyph);
        return Glyph;
        }
     }
#define UNKNOWN_GLYPH_INDICATOR '?'
  if (CharCode != UNKNOWN_GLYPH_INDICATOR)
     return Glyph(UNKNOWN_GLYPH_INDICATOR, AntiAliased);
  return NULL;
}

int cFreetypeFont::Width(uint c) const
{
  cGlyph *g = Glyph(c, Setup.AntiAlias);
  return g ? g->AdvanceX() : 0;
}

int cFreetypeFont::Width(const char *s) const
{
  int w = 0;
  if (s) {
#ifdef BIDI
     cString bs = Bidi(s);
     s = bs;
#endif
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           cGlyph *g = Glyph(sym, Setup.AntiAlias);
           if (g)
              w += g->AdvanceX() + Kerning(g, prevSym);
           prevSym = sym;
           }
     }
  return w;
}

#define MAX_BLEND_LEVELS 256

void cFreetypeFont::DrawText(cBitmap *Bitmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const
{
  if (s && height) { // checking height to make sure we actually have a valid font
#ifdef BIDI
     cString bs = Bidi(s);
     s = bs;
#endif
     bool AntiAliased = Setup.AntiAlias && Bitmap->Bpp() >= 8;
     bool TransparentBackground = ColorBg == clrTransparent;
     int16_t BlendLevelIndex[MAX_BLEND_LEVELS]; // tIndex is 8 bit unsigned, so a negative value can be used to mark unused entries
     if (AntiAliased && !TransparentBackground)
        memset(BlendLevelIndex, 0xFF, sizeof(BlendLevelIndex)); // initializes the array with negative values
     tIndex fg = Bitmap->Index(ColorFg);
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           cGlyph *g = Glyph(sym, AntiAliased);
           if (!g)
              continue;
           int kerning = Kerning(g, prevSym);
           prevSym = sym;
           uchar *buffer = g->Bitmap();
           int symWidth = g->Width();
           if (Width && x + symWidth + g->Left() + kerning - 1 > Width)
              break; // we don't draw partial characters
           if (x + symWidth + g->Left() + kerning > 0) {
              for (int row = 0; row < g->Rows(); row++) {
                  for (int pitch = 0; pitch < g->Pitch(); pitch++) {
                      uchar bt = *(buffer + (row * g->Pitch() + pitch));
                      if (AntiAliased) {
                         if (bt > 0x00) {
                            int px = x + pitch + g->Left() + kerning;
                            int py = y + row + (height - Bottom() - g->Top());
                            tColor bg;
                            if (bt == 0xFF)
                               bg = fg;
                            else if (TransparentBackground)
                               bg = Bitmap->Index(Bitmap->Blend(ColorFg, Bitmap->GetColor(px, py), bt));
                            else if (BlendLevelIndex[bt] >= 0)
                               bg = BlendLevelIndex[bt];
                            else
                               bg = BlendLevelIndex[bt] = Bitmap->Index(Bitmap->Blend(ColorFg, ColorBg, bt));
                            Bitmap->SetIndex(px, py, bg);
                            }
                         }
                      else { //monochrome rendering
                         for (int col = 0; col < 8 && col + pitch * 8 <= symWidth; col++) {
                             if (bt & 0x80)
                                Bitmap->SetIndex(x + col + pitch * 8 + g->Left() + kerning, y + row + (height - Bottom() - g->Top()), fg);
                             bt <<= 1;
                             }
                         }
                      }
                  }
              }
           x += g->AdvanceX() + kerning;
           if (x > Bitmap->Width() - 1)
              break;
           }
     }
}

void cFreetypeFont::DrawText(cPixmap *Pixmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const
{
  if (s && height) { // checking height to make sure we actually have a valid font
#ifdef BIDI
     cString bs = Bidi(s);
     s = bs;
#endif
     bool AntiAliased = Setup.AntiAlias;
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           cGlyph *g = Glyph(sym, AntiAliased);
           if (!g)
              continue;
           int kerning = Kerning(g, prevSym);
           prevSym = sym;
           uchar *buffer = g->Bitmap();
           int symWidth = g->Width();
           if (Width && x + symWidth + g->Left() + kerning - 1 > Width)
              break; // we don't draw partial characters
           if (x + symWidth + g->Left() + kerning > 0) {
              for (int row = 0; row < g->Rows(); row++) {
                  for (int pitch = 0; pitch < g->Pitch(); pitch++) {
                      uchar bt = *(buffer + (row * g->Pitch() + pitch));
                      if (AntiAliased) {
                         if (bt > 0x00)
                            Pixmap->DrawPixel(cPoint(x + pitch + g->Left() + kerning, y + row + (height - Bottom() - g->Top())), AlphaBlend(ColorFg, ColorBg, bt));
                         }
                      else { //monochrome rendering
                         for (int col = 0; col < 8 && col + pitch * 8 <= symWidth; col++) {
                             if (bt & 0x80)
                                Pixmap->DrawPixel(cPoint(x + col + pitch * 8 + g->Left() + kerning, y + row + (height - Bottom() - g->Top())), ColorFg);
                             bt <<= 1;
                             }
                         }
                      }
                  }
              }
           x += g->AdvanceX() + kerning;
           if (x > Pixmap->DrawPort().Width() - 1)
              break;
           }
     }
}

// --- cDummyFont ------------------------------------------------------------

// A dummy font, in case there are no fonts installed:

class cDummyFont : public cFont {
private:
  int height;
  int width;
public:
  cDummyFont(int CharHeight, int CharWidth) { height = CharHeight; width = CharWidth; }
  virtual int Width(void) const { return width ? width : height; }
  virtual int Width(uint c) const { return width ? width : height; }
  virtual int Width(const char *s) const { return width ? width : height; }
  virtual int Height(void) const { return height; }
  virtual void DrawText(cBitmap *Bitmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const {}
  virtual void DrawText(cPixmap *Pixmap, int x, int y, const char *s, tColor ColorFg, tColor ColorBg, int Width) const {};
  };

// --- cFont -----------------------------------------------------------------

cFont *cFont::fonts[eDvbFontSize] = { NULL };

void cFont::SetFont(eDvbFont Font, const char *Name, int CharHeight)
{
  delete fonts[Font];
  fonts[Font] = CreateFont(Name, constrain(CharHeight, MINFONTSIZE, MAXFONTSIZE));
}

const cFont *cFont::GetFont(eDvbFont Font)
{
  if (Setup.UseSmallFont == 0 && Font == fontSml)
     Font = fontOsd;
  else if (Setup.UseSmallFont == 2)
     Font = fontSml;
  if (!fonts[Font]) {
     switch (Font) {
       case fontOsd: SetFont(Font, Setup.FontOsd, Setup.FontOsdSize); break;
       case fontSml: SetFont(Font, Setup.FontSml, min(Setup.FontSmlSize, Setup.FontOsdSize)); break;
       case fontFix: SetFont(Font, Setup.FontFix, Setup.FontFixSize); break;
       default: esyslog("ERROR: unknown Font %d (%s %d)", Font, __FUNCTION__, __LINE__);
       }
     }
  return fonts[Font];
}

cFont *cFont::CreateFont(const char *Name, int CharHeight, int CharWidth)
{
  cString fn = GetFontFileName(Name);
  cFont *f = *fn ? new cFreetypeFont(fn, CharHeight, CharWidth) : NULL;
  if (!f || !f->Height()) {
     delete f;
     f = new cDummyFont(CharHeight, CharWidth);
     }
  return f;
}

bool cFont::GetAvailableFontNames(cStringList *FontNames, bool Monospaced)
{
  if (!FontNames->Size()) {
     FcInit();
     FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, NULL);
     FcPattern *pat = FcPatternCreate();
     FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
     if (Monospaced)
        FcPatternAddInteger(pat, FC_SPACING, FC_MONO);
     FcFontSet* fontset = FcFontList(NULL, pat, os);
     for (int i = 0; i < fontset->nfont; i++) {
         char *s = (char *)FcNameUnparse(fontset->fonts[i]);
         if (s) {
            // Strip i18n stuff:
            char *c = strchr(s, ':');
            if (c) {
               char *p = strchr(c + 1, ',');
               if (p)
                  *p = 0;
               }
            char *p = strchr(s, ',');
            if (p) {
               if (c)
                  memmove(p, c, strlen(c) + 1);
               else
                  *p = 0;
               }
            // Make it user presentable:
            s = strreplace(s, "\\", ""); // '-' is escaped
            s = strreplace(s, "style=", "");
            FontNames->Append(s); // takes ownership of s
            }
         }
     FcFontSetDestroy(fontset);
     FcPatternDestroy(pat);
     FcObjectSetDestroy(os);
     //FcFini(); // older versions of fontconfig are broken - and FcInit() can be called more than once
     FontNames->Sort();
     }
  return FontNames->Size() > 0;
}

cString cFont::GetFontFileName(const char *FontName)
{
  cString FontFileName;
  if (FontName) {
     char *fn = strdup(FontName);
     fn = strreplace(fn, ":", ":style=");
     fn = strreplace(fn, "-", "\\-");
     FcInit();
     FcPattern *pat = FcNameParse((FcChar8 *)fn);
     FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
     FcConfigSubstitute(NULL, pat, FcMatchPattern);
     FcDefaultSubstitute(pat);
     FcResult fresult;
     FcFontSet *fontset = FcFontSort(NULL, pat, FcFalse, NULL, &fresult);
     if (fontset) {
        for (int i = 0; i < fontset->nfont; i++) {
            FcBool scalable;
            FcPatternGetBool(fontset->fonts[i], FC_SCALABLE, 0, &scalable);
            if (scalable) {
               FcChar8 *s = NULL;
               FcPatternGetString(fontset->fonts[i], FC_FILE, 0, &s);
               FontFileName = (char *)s;
               break;
               }
            }
        FcFontSetDestroy(fontset);
        }
     else
        esyslog("ERROR: no usable font found for '%s'", FontName);
     FcPatternDestroy(pat);
     free(fn);
     //FcFini(); // older versions of fontconfig are broken - and FcInit() can be called more than once
     }
  return FontFileName;
}

#ifdef BIDI
cString cFont::Bidi(const char *Ltr)
{
  if (!cCharSetConv::SystemCharacterTable()) { // bidi requires UTF-8
     fribidi_set_mirroring(true);
     fribidi_set_reorder_nsm(false);
     FriBidiCharSet fribidiCharset = FRIBIDI_CHAR_SET_UTF8;
     int LtrLen = strlen(Ltr);
     FriBidiCharType Base = FRIBIDI_TYPE_L;
     FriBidiChar *Logical = MALLOC(FriBidiChar, LtrLen + 1) ;
     int RtlLen = fribidi_charset_to_unicode(fribidiCharset, const_cast<char *>(Ltr), LtrLen, Logical);
     FriBidiChar *Visual = MALLOC(FriBidiChar, LtrLen + 1) ;
     char *Rtl = NULL;
     bool ok = fribidi_log2vis(Logical, RtlLen, &Base, Visual, NULL, NULL, NULL);
     if (ok) {
        fribidi_remove_bidi_marks(Visual, RtlLen, NULL, NULL, NULL);
        Rtl = MALLOC(char, RtlLen * 4 + 1);
        fribidi_unicode_to_charset(fribidiCharset, Visual, RtlLen, Rtl);
        }
     free(Logical);
     free(Visual);
     if (ok)
        return cString(Rtl, true);
     }
  return cString(Ltr);
}
#endif

// --- cTextWrapper ----------------------------------------------------------

cTextWrapper::cTextWrapper(void)
{
  text = eol = NULL;
  lines = 0;
  lastLine = -1;
}

cTextWrapper::cTextWrapper(const char *Text, const cFont *Font, int Width)
{
  text = NULL;
  Set(Text, Font, Width);
}

cTextWrapper::~cTextWrapper()
{
  free(text);
}

void cTextWrapper::Set(const char *Text, const cFont *Font, int Width)
{
  free(text);
  text = Text ? strdup(Text) : NULL;
  eol = NULL;
  lines = 0;
  lastLine = -1;
  if (!text)
     return;
  lines = 1;
  if (Width <= 0)
     return;

  char *Blank = NULL;
  char *Delim = NULL;
  int w = 0;

  stripspace(text); // strips trailing newlines

  for (char *p = text; *p; ) {
      int sl = Utf8CharLen(p);
      uint sym = Utf8CharGet(p, sl);
      if (sym == '\n') {
         lines++;
         w = 0;
         Blank = Delim = NULL;
         p++;
         continue;
         }
      else if (sl == 1 && isspace(sym))
         Blank = p;
      int cw = Font->Width(sym);
      if (w + cw > Width) {
         if (Blank) {
            *Blank = '\n';
            p = Blank;
            continue;
            }
         else if (w > 0) { // there has to be at least one character before the newline
            // Here's the ugly part, where we don't have any whitespace to
            // punch in a newline, so we need to make room for it:
            if (Delim)
               p = Delim + 1; // let's fall back to the most recent delimiter
            char *s = MALLOC(char, strlen(text) + 2); // The additional '\n' plus the terminating '\0'
            int l = p - text;
            strncpy(s, text, l);
            s[l] = '\n';
            strcpy(s + l + 1, p);
            free(text);
            text = s;
            p = text + l;
            continue;
            }
         }
      w += cw;
      if (strchr("-.,:;!?_~", *p)) {
         Delim = p;
         Blank = NULL;
         }
      p += sl;
      }
}

const char *cTextWrapper::Text(void)
{
  if (eol) {
     *eol = '\n';
     eol = NULL;
     }
  return text;
}

const char *cTextWrapper::GetLine(int Line)
{
  char *s = NULL;
  if (Line < lines) {
     if (eol) {
        *eol = '\n';
        if (Line == lastLine + 1)
           s = eol + 1;
        eol = NULL;
        }
     if (!s) {
        s = text;
        for (int i = 0; i < Line; i++) {
            s = strchr(s, '\n');
            if (s)
               s++;
            else
               break;
            }
        }
     if (s) {
        if ((eol = strchr(s, '\n')) != NULL)
           *eol = 0;
        }
     lastLine = Line;
     }
  return s;
}
