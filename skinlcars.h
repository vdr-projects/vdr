/*
 * skinlcars.h: A VDR skin with Star Trek's "LCARS" layout
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinlcars.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __SKINLCARS_H
#define __SKINLCARS_H

#include "skins.h"

class cSkinLCARS : public cSkin {
public:
  cSkinLCARS(void);
  virtual const char *Description(void) override;
  virtual cSkinDisplayChannel *DisplayChannel(bool WithInfo) override;
  virtual cSkinDisplayMenu *DisplayMenu(void) override;
  virtual cSkinDisplayReplay *DisplayReplay(bool ModeOnly) override;
  virtual cSkinDisplayVolume *DisplayVolume(void) override;
  virtual cSkinDisplayTracks *DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks) override;
  virtual cSkinDisplayMessage *DisplayMessage(void) override;
  };

#endif //__SKINLCARS_H
