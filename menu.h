/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.1 2000/02/19 13:36:48 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include "osd.h"

class cMenuMain : public cOsdMenu {
public:
  cMenuMain(void);
  virtual eOSStatus ProcessKey(eKeys Key);
  };
  
#endif //_MENU_H
