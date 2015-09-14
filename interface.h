/*
 * interface.h: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.h 4.1 2015/04/28 11:15:11 kls Exp $
 */

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "config.h"
#include "remote.h"
#include "skins.h"

class cInterface {
private:
  bool interrupted;
  bool QueryKeys(cRemote *Remote, cSkinDisplayMenu *DisplayMenu);
public:
  cInterface(void);
  ~cInterface();
  void Interrupt(void) { interrupted = true; }
  eKeys GetKey(bool Wait = true);
  eKeys Wait(int Seconds = 0, bool KeepChar = false);
  bool Confirm(const char *s, int Seconds = 10, bool WaitForTimeout = false);
  void LearnKeys(void);
  };

extern cInterface *Interface;

#endif //__INTERFACE_H
