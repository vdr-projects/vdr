/* Copyright (c) Mark J. Kilgard, 1997. */

/* This program is freely distributable without licensing fees  and is
   provided without guarantee or warrantee expressed or  implied. This
   program is -not- in the public domain. */

/* X compile line: cc -o gentexfont gentexfont.c -lX11 */

/* 2000-10-01: Stripped down the original code to get a simple bitmap C-code generator  */
/*             for use with the VDR project (see http://www.cadsoft.de/vdr)             */
/*             Renamed the file 'genfontfile.c' since it no longer generates 'tex' data */
/*             Klaus Schmidinger (kls@cadsoft.de)                                       */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>

typedef struct {
  unsigned short c;       /* Potentially support 16-bit glyphs. */
  unsigned char width;
  unsigned char height;
  signed char xoffset;
  signed char yoffset;
  signed char advance;
  char dummy;           /* Space holder for alignment reasons. */
  short x;
  short y;
} TexGlyphInfo;

typedef struct {
  short width;
  short height;
  short xoffset;
  short yoffset;
  short advance;
  unsigned char *bitmap;
} PerGlyphInfo, *PerGlyphInfoPtr;

typedef struct {
  int min_char;
  int max_char;
  int max_ascent;
  int max_descent;
  PerGlyphInfo glyph[1];
} FontInfo, *FontInfoPtr;

Display *dpy;
FontInfoPtr fontinfo;

/* #define REPORT_GLYPHS */
#ifdef REPORT_GLYPHS
#define DEBUG_GLYPH4(msg,a,b,c,d) printf(msg,a,b,c,d)
#define DEBUG_GLYPH(msg) printf(msg)
#else
#define DEBUG_GLYPH4(msg,a,b,c,d) { /* nothing */ }
#define DEBUG_GLYPH(msg) { /* nothing */ }
#endif

#define MAX_GLYPHS_PER_GRAB 512  /* this is big enough for 2^9 glyph
                                    character sets */

FontInfoPtr
SuckGlyphsFromServer(Display * dpy, Font font)
{
  Pixmap offscreen;
  XFontStruct *fontinfo;
  XImage *image;
  GC xgc;
  XGCValues values;
  int numchars;
  int width, height, pixwidth;
  int i, j;
  XCharStruct *charinfo;
  XChar2b character;
  unsigned char *bitmapData;
  int x, y;
  int spanLength;
  int charWidth, charHeight, maxSpanLength;
  int grabList[MAX_GLYPHS_PER_GRAB];
  int glyphsPerGrab = MAX_GLYPHS_PER_GRAB;
  int numToGrab, thisglyph;
  FontInfoPtr myfontinfo;

  fontinfo = XQueryFont(dpy, font);
  if (!fontinfo)
    return NULL;

  numchars = fontinfo->max_char_or_byte2 - fontinfo->min_char_or_byte2 + 1;
  if (numchars < 1)
    return NULL;

  myfontinfo = (FontInfoPtr) malloc(sizeof(FontInfo) + (numchars - 1) * sizeof(PerGlyphInfo));
  if (!myfontinfo)
    return NULL;

  myfontinfo->min_char = fontinfo->min_char_or_byte2;
  myfontinfo->max_char = fontinfo->max_char_or_byte2;
  myfontinfo->max_ascent = fontinfo->max_bounds.ascent;
  myfontinfo->max_descent = fontinfo->max_bounds.descent;

  width = fontinfo->max_bounds.rbearing - fontinfo->min_bounds.lbearing;
  height = fontinfo->max_bounds.ascent + fontinfo->max_bounds.descent;

  maxSpanLength = (width + 7) / 8;
  /* Be careful determining the width of the pixmap; the X protocol allows
     pixmaps of width 2^16-1 (unsigned short size) but drawing coordinates
     max out at 2^15-1 (signed short size).  If the width is too large, we
     need to limit the glyphs per grab.  */
  if ((glyphsPerGrab * 8 * maxSpanLength) >= (1 << 15)) {
    glyphsPerGrab = (1 << 15) / (8 * maxSpanLength);
  }
  pixwidth = glyphsPerGrab * 8 * maxSpanLength;
  offscreen = XCreatePixmap(dpy, RootWindow(dpy, DefaultScreen(dpy)),
    pixwidth, height, 1);

  values.font = font;
  values.background = 0;
  values.foreground = 0;
  xgc = XCreateGC(dpy, offscreen, GCFont | GCBackground | GCForeground, &values);

  XFillRectangle(dpy, offscreen, xgc, 0, 0, 8 * maxSpanLength * glyphsPerGrab, height);
  XSetForeground(dpy, xgc, 1);

  numToGrab = 0;
  if (fontinfo->per_char == NULL) {
    charinfo = &(fontinfo->min_bounds);
    charWidth = charinfo->rbearing - charinfo->lbearing;
    charHeight = charinfo->ascent + charinfo->descent;
    spanLength = (charWidth + 7) / 8;
  }
  for (i = 0; i < numchars; i++) {
    if (fontinfo->per_char != NULL) {
      charinfo = &(fontinfo->per_char[i]);
      charWidth = charinfo->rbearing - charinfo->lbearing;
      charHeight = charinfo->ascent + charinfo->descent;
      if (charWidth == 0 || charHeight == 0) {
        /* Still must move raster pos even if empty character */
        myfontinfo->glyph[i].width = 0;
        myfontinfo->glyph[i].height = 0;
        myfontinfo->glyph[i].xoffset = 0;
        myfontinfo->glyph[i].yoffset = 0;
        myfontinfo->glyph[i].advance = charinfo->width;
        myfontinfo->glyph[i].bitmap = NULL;
        goto PossiblyDoGrab;
      }
    }
    grabList[numToGrab] = i;

    /* XXX is this right for large fonts? */
    character.byte2 = (i + fontinfo->min_char_or_byte2) & 255;
    character.byte1 = (i + fontinfo->min_char_or_byte2) >> 8;

    /* XXX we could use XDrawImageString16 which would also paint the backing 

       rectangle but X server bugs in some scalable font rasterizers makes it 

       more effective to do XFillRectangles to clear the pixmap and
       XDrawImage16 for the text.  */
    XDrawString16(dpy, offscreen, xgc,
      -charinfo->lbearing + 8 * maxSpanLength * numToGrab,
      charinfo->ascent, &character, 1);

    numToGrab++;

  PossiblyDoGrab:

    if (numToGrab >= glyphsPerGrab || i == numchars - 1) {
      image = XGetImage(dpy, offscreen,
        0, 0, pixwidth, height, 1, XYPixmap);
      for (j = 0; j < numToGrab; j++) {
        thisglyph = grabList[j];
        if (fontinfo->per_char != NULL) {
          charinfo = &(fontinfo->per_char[thisglyph]);
          charWidth = charinfo->rbearing - charinfo->lbearing;
          charHeight = charinfo->ascent + charinfo->descent;
          spanLength = (charWidth + 7) / 8;
        }
        bitmapData = (unsigned char *)calloc(height * spanLength, sizeof(char));
        if (!bitmapData)
          goto FreeFontAndReturn;
        DEBUG_GLYPH4("index %d, glyph %d (%d by %d)\n",
          j, thisglyph + fontinfo->min_char_or_byte2, charWidth, charHeight);
        for (y = 0; y < charHeight; y++) {
          for (x = 0; x < charWidth; x++) {
            /* XXX The algorithm used to suck across the font ensures that
               each glyph begins on a byte boundary.  In theory this would
               make it convienent to copy the glyph into a byte oriented
               bitmap.  We actually use the XGetPixel function to extract
               each pixel from the image which is not that efficient.  We
               could either do tighter packing in the pixmap or more
               efficient extraction from the image.  Oh well.  */
            if (XGetPixel(image, j * maxSpanLength * 8 + x, charHeight - 1 - y)) {
              DEBUG_GLYPH("x");
              bitmapData[y * spanLength + x / 8] |= (1 << (x & 7));
            } else {
              DEBUG_GLYPH(" ");
            }
          }
          DEBUG_GLYPH("\n");
        }
        myfontinfo->glyph[thisglyph].width = charWidth;
        myfontinfo->glyph[thisglyph].height = charHeight;
        myfontinfo->glyph[thisglyph].xoffset = charinfo->lbearing;
        myfontinfo->glyph[thisglyph].yoffset = -charinfo->descent;
        myfontinfo->glyph[thisglyph].advance = charinfo->width;
        myfontinfo->glyph[thisglyph].bitmap = bitmapData;
      }
      XDestroyImage(image);
      numToGrab = 0;
      /* do we need to clear the offscreen pixmap to get more? */
      if (i < numchars - 1) {
        XSetForeground(dpy, xgc, 0);
        XFillRectangle(dpy, offscreen, xgc, 0, 0, 8 * maxSpanLength * glyphsPerGrab, height);
        XSetForeground(dpy, xgc, 1);
      }
    }
  }
  XFreeGC(dpy, xgc);
  XFreePixmap(dpy, offscreen);
  return myfontinfo;

FreeFontAndReturn:
  XDestroyImage(image);
  XFreeGC(dpy, xgc);
  XFreePixmap(dpy, offscreen);
  for (j = i - 1; j >= 0; j--) {
    if (myfontinfo->glyph[j].bitmap)
      free(myfontinfo->glyph[j].bitmap);
  }
  free(myfontinfo);
  return NULL;
}

void
printGlyph(FontInfoPtr font, int c)
{
  PerGlyphInfoPtr glyph;
  unsigned char *bitmapData;
  int width, height, spanLength, charWidth;
  int x, y, l;
  char buf[1000], *b;

  if (c < font->min_char || c > font->max_char) {
    fprintf(stderr, "out of range glyph\n");
    exit(1);
  }
  glyph = &font->glyph[c - font->min_char];
  bitmapData = glyph->bitmap;
    width = glyph->width;
    spanLength = (width + 7) / 8;
    height = glyph->height;
    charWidth = glyph->xoffset + width;
    if (charWidth < glyph->advance)
       charWidth = glyph->advance;

    printf("  {             // %d\n", c);
    printf("     %d, %d,\n", charWidth, font->max_ascent + font->max_descent);
    for (y = 0; y < font->max_ascent - glyph->yoffset - height; y++) {
        printf("     0x%08X,  // ", 0);
        for (x = 0; x < charWidth; x++)
            putchar('.');
        putchar('\n');
        }
    for (y = height; y-- > 0;) {
        l = 0;
        b = buf;
        for (x = 0; x < glyph->xoffset; x++)
            *b++ = '.';
        if (bitmapData) {
           for (x = 0; x < width; x++) {
               l <<= 1;
               if (bitmapData[y * spanLength + x / 8] & (1 << (x & 7))) {
                  *b++ = '*';
                  l |= 1;
                  }
               else
                  *b++ = '.';
               }
           for (x = 0; x < glyph->advance - width - glyph->xoffset; x++) {
               *b++ = '.';
               l <<= 1;
               }
           }
        *b = 0;
        printf("     0x%08X,  // %s\n", l, buf);
        }
    for (y = 0; y < font->max_descent + glyph->yoffset; y++) {
        printf("     0x%08X,  // ", 0);
        for (x = 0; x < glyph->xoffset + width || x < glyph->advance; x++)
            putchar('.');
        putchar('\n');
        }
    printf("  },\n");
}

void
getMetric(FontInfoPtr font, int c, TexGlyphInfo * tgi)
{
  PerGlyphInfoPtr glyph;
  unsigned char *bitmapData;

  tgi->c = c;
  if (c < font->min_char || c > font->max_char) {
    tgi->width = 0;
    tgi->height = 0;
    tgi->xoffset = 0;
    tgi->yoffset = 0;
    tgi->dummy = 0;
    tgi->advance = 0;
    return;
  }
  glyph = &font->glyph[c - font->min_char];
  bitmapData = glyph->bitmap;
  if (bitmapData) {
    tgi->width = glyph->width;
    tgi->height = glyph->height;
    tgi->xoffset = glyph->xoffset;
    tgi->yoffset = glyph->yoffset;
  } else {
    tgi->width = 0;
    tgi->height = 0;
    tgi->xoffset = 0;
    tgi->yoffset = 0;
  }
  tgi->dummy = 0;
  tgi->advance = glyph->advance;
}

int
main(int argc, char *argv[])
{
  int c;
  TexGlyphInfo tgi;
  int usageError = 0;
  char *varname, *fontname;
  XFontStruct *xfont;
  int i;

  if (argc == 3) {
     varname  = argv[1];
     fontname = argv[2];
     }
  else
     usageError = 1;

  if (usageError) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: genfontfile variable_name X_font_name\n");
    fprintf(stderr, "\n");
    exit(1);
  }

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "could not open display\n");
    exit(1);
  }
  /* find an OpenGL-capable RGB visual with depth buffer */
  xfont = XLoadQueryFont(dpy, fontname);
  if (!xfont) {
    fprintf(stderr, "could not get load X font: %s\n", fontname);
    exit(1);
  }
  fontinfo = SuckGlyphsFromServer(dpy, xfont->fid);
  if (!fontinfo) {
    fprintf(stderr, "could not get font glyphs\n");
    exit(1);
  }

  printf("%s[][%d] = {\n", varname, fontinfo->max_ascent + fontinfo->max_descent + 2);
  for (c = 32; c < 256; c++) {
      getMetric(fontinfo, c, &tgi);
      printGlyph(fontinfo, c);
      }
  printf("  };\n");
  return 0;
}
