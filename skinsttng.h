/*
 * skinsttng.h: A VDR skin with ST:TNG Panels
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: skinsttng.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __SKINSTTNG_H
#define __SKINSTTNG_H

#include "skins.h"

class cSkinSTTNG : public cSkin {
public:
  cSkinSTTNG(void);
  virtual const char *Description(void) override;
  virtual cSkinDisplayChannel *DisplayChannel(bool WithInfo) override;
  virtual cSkinDisplayMenu *DisplayMenu(void) override;
  virtual cSkinDisplayReplay *DisplayReplay(bool ModeOnly) override;
  virtual cSkinDisplayVolume *DisplayVolume(void) override;
  virtual cSkinDisplayTracks *DisplayTracks(const char *Title, int NumTracks, const char * const *Tracks) override;
  virtual cSkinDisplayMessage *DisplayMessage(void) override;
  };

#endif //__SKINSTTNG_H
