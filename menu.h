/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.2 2000/03/05 10:57:27 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include "osd.h"

class cMenuMain : public cOsdMenu {
public:
  cMenuMain(void);
  virtual eOSState ProcessKey(eKeys Key);
  };
  
#endif //_MENU_H
