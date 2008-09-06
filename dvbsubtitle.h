/*
 * dvbsubtitle.h: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schlüßler <marco@lordzodiac.de>
 *
 * $Id: dvbsubtitle.h 2.1 2008/05/25 14:36:52 kls Exp $
 */

#ifndef __DVBSUBTITLE_H
#define __DVBSUBTITLE_H

#include "osd.h"
#include "thread.h"
#include "tools.h"

class cDvbSubtitlePage;
class cDvbSubtitleAssembler; // for legacy PES recordings
class cDvbSubtitleBitmaps;

class cDvbSubtitleConverter : public cThread {
private:
  static int setupLevel;
  cDvbSubtitleAssembler *dvbSubtitleAssembler;
  cOsd *osd;
  cList<cDvbSubtitlePage> *pages;
  cList<cDvbSubtitleBitmaps> *bitmaps;
  tColor yuv2rgb(int Y, int Cb, int Cr);
  bool AssertOsd(void);
  int ExtractSegment(const uchar *Data, int Length, int64_t Pts);
  void FinishPage(cDvbSubtitlePage *Page);
public:
  cDvbSubtitleConverter(void);
  virtual ~cDvbSubtitleConverter();
  void Action(void);
  void Reset(void);
  int ConvertFragments(const uchar *Data, int Length); // for legacy PES recordings
  int Convert(const uchar *Data, int Length);
  static void SetupChanged(void);
  };

#endif //__DVBSUBTITLE_H
