/*
 * font.c: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.c 1.5 2004/01/16 13:17:57 kls Exp $
 */

#include "font.h"
#include "tools.h"

#include "fontfix.c"
#include "fontosd.c"
#include "fontsml.c"

#include "fontfix-iso8859-5.c"
#include "fontosd-iso8859-5.c"
#include "fontsml-iso8859-5.c"

#include "fontfix-iso8859-7.c"
#include "fontosd-iso8859-7.c"
#include "fontsml-iso8859-7.c"

static void *FontData[eDvbCodeSize][eDvbFontSize] = {
  { FontOsd_iso8859_1, FontFix_iso8859_1, FontSml_iso8859_1 },
  { FontOsd_iso8859_5, FontFix_iso8859_5, FontSml_iso8859_5 },
  { FontOsd_iso8859_7, FontFix_iso8859_7, FontSml_iso8859_7 },
  };

static const char *FontCode[eDvbCodeSize] = {
  "iso8859-1",
  "iso8859-5",
  "iso8859-7",
  };

eDvbCode cFont::code = code_iso8859_1;
cFont *cFont::fonts[eDvbFontSize] = { NULL };

cFont::cFont(void *Data)
{
  SetData(Data);
}

void cFont::SetData(void *Data)
{
  int h = ((tCharData *)Data)->height;
  for (int i = 0; i < NUMCHARS; i++)
      data[i] = (tCharData *)&((tPixelData *)Data)[(i < 32 ? 0 : i - 32) * (h + 2)];
}

int cFont::Width(const char *s) const
{
  int w = 0;
  while (s && *s)
        w += Width(*s++);
  return w;
}

int cFont::Height(const char *s) const
{
  int h = 0;
  if (s && *s)
     h = Height(*s); // all characters have the same height!
  return h;
}

bool cFont::SetCode(const char *Code)
{
  for (int i = 0; i < eDvbCodeSize; i++) {
      if (strcmp(Code, FontCode[i]) == 0) {
         SetCode(eDvbCode(i));
         return true;
         }
      }
  return false;
}

void cFont::SetCode(eDvbCode Code)
{
  if (code != Code) {
     code = Code;
     for (int i = 0; i < eDvbFontSize; i++) {
         if (fonts[i])
            fonts[i]->SetData(FontData[code][i]);
         }
     }
}

void cFont::SetFont(eDvbFont Font, void *Data)
{
  delete fonts[Font];
  fonts[Font] = new cFont(Data ? Data : FontData[code][Font]);
}

const cFont *cFont::GetFont(eDvbFont Font)
{
  if (!fonts[Font])
     SetFont(Font);
  return fonts[Font];
}
