/*
 * font.c: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.c 1.1 2000/10/01 15:01:49 kls Exp $
 */

#include "font.h"
#include "tools.h"

#include "fontosd.c"

cFont::cFont(eDvbFont Font)//XXX
{
  switch (Font) {
    default:
    case fontOsd: for (int i = 0; i < NUMCHARS; i++)
                      data[i] = (tCharData *)&FontOsd[i < 32 ? 0 : i - 32];
                  break;
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

