/*
 * menu.h: A menu for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: menu.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include <vdr/osdbase.h>
#include <vdr/tools.h>
#include "entry.h"

extern char PictureDirectory[PATH_MAX];

class cPictureMenu : public cOsdMenu {
private:
  static cPictureEntry *pictures;
  const cPictureEntry *pictureEntry;
  void Set(const char *Path);
  eOSState SelectItem(const char *Path = NULL, bool SlideShow = false);
public:
  cPictureMenu(const cPictureEntry *PictureEntry, const char *Path = NULL);
  virtual ~cPictureMenu() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  static cPictureMenu *CreatePictureMenu(void);
  };

#endif //_MENU_H
