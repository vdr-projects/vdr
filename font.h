/*
 * font.h: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.h 1.1 2000/10/01 15:00:35 kls Exp $
 */

#ifndef __FONT_H
#define __FONT_H

enum eDvbFont {
  fontOsd,
/* TODO as soon as we have the font files...
  fontTtxSmall,
  fontTtxLarge,
*/
  };

class cFont {
public:
  enum { NUMCHARS = 256 };
  typedef unsigned long tPixelData;
  struct tCharData {
    tPixelData width, height;
    tPixelData lines[1];
    };
private:
  const tCharData *data[NUMCHARS];
public:
  cFont(eDvbFont Font);
  int Width(unsigned char c) { return data[c]->width; }
  int Width(const char *s);
  int Height(unsigned char c) { return data[c]->height; }
  int Height(const char *s);
  const tCharData *CharData(unsigned char c) { return data[c]; }
  };

#endif //__FONT_H
