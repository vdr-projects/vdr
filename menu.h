/*
 * menu.h: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.h 1.5 2000/04/24 15:31:53 kls Exp $
 */

#ifndef _MENU_H
#define _MENU_H

#include "osd.h"

class cMenuMain : public cOsdMenu {
public:
  cMenuMain(void);
  virtual eOSState ProcessKey(eKeys Key);
  };
  
class cReplayDisplay {
private:
  bool shown;
public:
  cReplayDisplay(void);
  ~cReplayDisplay();
  eKeys ProcessKey(eKeys Key);
  };

#endif //_MENU_H
