/*
 * lirc.h: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: lirc.h 1.3 2005/07/31 10:18:15 kls Exp $
 */

#ifndef __LIRC_H
#define __LIRC_H

#include "remote.h"
#include "thread.h"

class cLircRemote : public cRemote, private cThread {
private:
  enum { LIRC_KEY_BUF = 30, LIRC_BUFFER_SIZE = 128 };
  int f;
  virtual void Action(void);
public:
  cLircRemote(const char *DeviceName);
  virtual ~cLircRemote();
  virtual bool Ready(void);
  };

#endif //__LIRC_H
