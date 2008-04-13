/*
 * menu.h: A menu for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: menu.h 2.0 2008/01/12 11:22:52 kls Exp $
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
  ~cPictureMenu();
  virtual eOSState ProcessKey(eKeys Key);
  static cPictureMenu *CreatePictureMenu(void);
  };

#endif //_MENU_H
