/*
 * font.h: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.h 1.4 2003/10/24 12:59:45 kls Exp $
 */

#ifndef __FONT_H
#define __FONT_H

#include <stdlib.h>

enum eDvbFont {
  fontOsd,
  fontFix,
  fontSml
#define eDvbFontSize (fontSml + 1)
  };

enum eDvbCode {
  code_iso8859_1,
  code_iso8859_7
#define eDvbCodeSize (code_iso8859_7 + 1)
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
  static eDvbCode code;
  static cFont *fonts[];
  const tCharData *data[NUMCHARS];
public:
  cFont(void *Data);
  void SetData(void *Data);
  int Width(unsigned char c) const { return data[c]->width; }
  int Width(const char *s) const;
  int Height(unsigned char c) const { return data[c]->height; }
  int Height(const char *s) const;
  const tCharData *CharData(unsigned char c) const { return data[c]; }
  static bool SetCode(const char *Code);
  static void SetCode(eDvbCode Code);
  static void SetFont(eDvbFont Font, void *Data = NULL);
  static const cFont *GetFont(eDvbFont Font);
  };

#endif //__FONT_H
