/*
 * font.h: Font handling for the DVB On Screen Display
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: font.h 1.11 2005/03/19 15:51:19 kls Exp $
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
  code_iso8859_2,
  code_iso8859_5,
  code_iso8859_7,
  code_iso8859_13,
  code_iso8859_15,
#define eDvbCodeSize (code_iso8859_15 + 1)
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
  int height;
public:
  cFont(void *Data);
  virtual ~cFont() {}
  void SetData(void *Data);
  virtual int Width(unsigned char c) const { return data[c]->width; }
      ///< Returns the width of the given character.
  virtual int Width(const char *s) const;
      ///< Returns the width of the given string.
  virtual int Height(unsigned char c) const { return data[c]->height; }
      ///< Returns the height of the given character.
  virtual int Height(const char *s) const;
      ///< Returns the height of the given string.
  virtual int Height(void) const { return height; }
      ///< Returns the height of this font (all characters have the same height).
  const tCharData *CharData(unsigned char c) const { return data[c]; }
  static bool SetCode(const char *Code);
  static void SetCode(eDvbCode Code);
  static void SetFont(eDvbFont Font, void *Data = NULL);
  static const cFont *GetFont(eDvbFont Font);
  };

class cTextWrapper {
private:
  char *text;
  char *eol;
  int lines;
  int lastLine;
public:
  cTextWrapper(void);
  cTextWrapper(const char *Text, const cFont *Font, int Width);
  ~cTextWrapper();
  void Set(const char *Text, const cFont *Font, int Width);
      ///< Wraps the Text to make it fit into the area defined by the given Width
      ///< when displayed with the given Font.
      ///< Wrapping is done by inserting the necessary number of newline
      ///< characters into the string.
  const char *Text(void);
      ///< Returns the full wrapped text.
  int Lines(void) { return lines; }
      ///< Returns the actual number of lines needed to display the full wrapped text.
  const char *GetLine(int Line);
      ///< Returns the given Line. The first line is numbered 0.
  };

#endif //__FONT_H
