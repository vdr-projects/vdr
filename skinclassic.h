/*
 * skinclassic.h: The 'classic' VDR skin
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinclassic.h 2.0 2005/01/02 14:38:56 kls Exp $
 */

#ifndef __SKINCLASSIC_H
#define __SKINCLASSIC_H

#include "skins.h"

class cSkinClassic : public cSkin {
public:
  cSkinClassic(void);
  virtual const char *Description(void);
  virtual cSkinDisplayChannel *DisplayChannel(bool WithInfo);
  virtual cSkinDisplayMenu *DisplayMenu(void);
  virtual cSkinDisplayReplay *DisplayReplay(bool ModeOnly);
  virtual cSkinDisplayVolume *DisplayVolume(void);
  virtual cSkinDisplayTracks *DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks);
  virtual cSkinDisplayMessage *DisplayMessage(void);
  };

#endif //__SKINCLASSIC_H
