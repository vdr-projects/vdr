/*
 * skinclassic.h: The 'classic' VDR skin
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinclassic.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __SKINCLASSIC_H
#define __SKINCLASSIC_H

#include "skins.h"

class cSkinClassic : public cSkin {
public:
  cSkinClassic(void);
  virtual const char *Description(void) override;
  virtual cSkinDisplayChannel *DisplayChannel(bool WithInfo) override;
  virtual cSkinDisplayMenu *DisplayMenu(void) override;
  virtual cSkinDisplayReplay *DisplayReplay(bool ModeOnly) override;
  virtual cSkinDisplayVolume *DisplayVolume(void) override;
  virtual cSkinDisplayTracks *DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks) override;
  virtual cSkinDisplayMessage *DisplayMessage(void) override;
  };

#endif //__SKINCLASSIC_H
