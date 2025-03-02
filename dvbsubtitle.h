/*
 * dvbsubtitle.h: DVB subtitles
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * Original author: Marco Schluessler <marco@lordzodiac.de>
 *
 * $Id: dvbsubtitle.h 5.1 2025/03/02 11:03:35 kls Exp $
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
  bool frozen;
  int ddsVersionNumber;
  int displayWidth;
  int displayHeight;
  int windowHorizontalOffset;
  int windowVerticalOffset;
  int windowWidth;
  int windowHeight;
  int osdDeltaX;
  int osdDeltaY;
  double osdFactorX;
  double osdFactorY;
  cList<cDvbSubtitlePage> *pages;
  cList<cDvbSubtitleBitmaps> *bitmaps;
  cDvbSubtitlePage *GetPageById(int PageId, bool New = false);
  void SetOsdData(void);
  bool AssertOsd(void);
  int ExtractSegment(const uchar *Data, int Length, int64_t Pts);
  int ExtractPgsSegment(const uchar *Data, int Length, int64_t Pts);
  void FinishPage(cDvbSubtitlePage *Page);
public:
  cDvbSubtitleConverter(void);
  virtual ~cDvbSubtitleConverter() override;
  virtual void Action(void) override;
  void Reset(void);
  void Freeze(bool Status) { frozen = Status; }
  int ConvertFragments(const uchar *Data, int Length); // for legacy PES recordings
  int Convert(const uchar *Data, int Length);
  static void SetupChanged(void);
  };

#endif //__DVBSUBTITLE_H
