/*
 * player.h: A player for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: player.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef _PLAYER_H
#define _PLAYER_H

#include <vdr/osd.h>
#include <vdr/player.h>
#include <vdr/tools.h>
#include "entry.h"

extern int SlideShowDelay;

cString HandleUnderscores(const char *s);

class cPicturePlayer;

class cPictureControl : public cControl {
private:
  static int active;
  static cString lastDisplayed;
  cPictureEntry *pictures;
  const cPictureEntry *pictureEntry;
  cPicturePlayer *player;
  cOsd *osd;
  cString lastPath;
  cTimeMs slideShowDelay;
  bool slideShow;
  bool alwaysDisplayCaption;
  void NextPicture(int Direction);
  void NextDirectory(int Direction);
  void DisplayCaption(void);
  virtual void Hide(void) override {}
public:
  cPictureControl(cPictureEntry *Pictures, const cPictureEntry *PictureEntry, bool SlideShow = false);
  virtual ~cPictureControl() override;
  virtual cString GetHeader(void) override;
  virtual eOSState ProcessKey(eKeys Key) override;
  static bool Active(void) { return active > 0; }
  static const char *LastDisplayed(void);
  };

#endif //_PLAYER_H
