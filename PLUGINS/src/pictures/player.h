/*
 * player.h: A player for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: player.h 1.1 2008/01/12 16:21:57 kls Exp $
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
  virtual void Hide(void) {}
public:
  cPictureControl(cPictureEntry *Pictures, const cPictureEntry *PictureEntry, bool SlideShow = false);
  virtual ~cPictureControl();
  virtual eOSState ProcessKey(eKeys Key);
  static bool Active(void) { return active > 0; }
  static const char *LastDisplayed(void);
  };

#endif //_PLAYER_H
