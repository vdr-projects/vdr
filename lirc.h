/*
 * lirc.h: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: lirc.h 5.1 2022/11/26 13:37:06 kls Exp $
 */

#ifndef __LIRC_H
#define __LIRC_H

#include "remote.h"
#include "thread.h"

class cLircRemote : public cRemote, protected cThread {
protected:
  int f;
  cLircRemote(const char *Name);
public:
  virtual ~cLircRemote();
  virtual bool Ready(void);
  static void NewLircRemote(const char *Name);
  };

#endif //__LIRC_H
