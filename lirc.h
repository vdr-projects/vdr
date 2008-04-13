/*
 * lirc.h: LIRC remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: lirc.h 2.0 2006/01/27 16:00:19 kls Exp $
 */

#ifndef __LIRC_H
#define __LIRC_H

#include <sys/un.h>
#include "remote.h"
#include "thread.h"

class cLircRemote : public cRemote, private cThread {
private:
  enum { LIRC_KEY_BUF = 30, LIRC_BUFFER_SIZE = 128 };
  int f;
  struct sockaddr_un addr;
  virtual void Action(void);
  bool Connect(void);
public:
  cLircRemote(const char *DeviceName);
  virtual ~cLircRemote();
  virtual bool Ready(void);
  };

#endif //__LIRC_H
