/*
 * font.c: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.c 1.2 2000/11/18 15:16:08 kls Exp $
 */

#include "font.h"
#include "tools.h"

#include "fontfix.c"
#include "fontosd.c"

cFont::cFont(eDvbFont Font)
{

#define FONTINDEX(Name)\
    case font##Name: for (int i = 0; i < NUMCHARS; i++)\
                         data[i] = (tCharData *)&Font##Name[i < 32 ? 0 : i - 32];\
                     break;

  switch (Font) {
    default:
    FONTINDEX(Osd);
    FONTINDEX(Fix);
    // TODO others...
    }
}

int cFont::Width(const char *s)
{
  int w = 0;
  while (s && *s)
        w += Width(*s++);
  return w;
}

int cFont::Height(const char *s)
{
  int h = 0;
  if (s && *s)
     h = Height(*s); // all characters have the same height!
  return h;
}

