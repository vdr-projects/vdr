/*
 * receiver.c: The basic receiver interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: receiver.c 1.3 2002/07/28 15:14:49 kls Exp $
 */

#include <stdarg.h>
#include <stdio.h>
#include "receiver.h"
#include "tools.h"

cReceiver::cReceiver(int Ca, int Priority, int NumPids, ...)
{
  device = NULL;
  ca = Ca;
  priority = Priority;
  for (int i = 0; i < MAXRECEIVEPIDS; i++)
      pids[i] = 0;
  if (NumPids) {
     va_list ap;
     va_start(ap, NumPids);
     int n = 0;
     while (n < MAXRECEIVEPIDS && NumPids--) {
           if ((pids[n] = va_arg(ap, int)) != 0)
              n++;
           }
     va_end(ap);
     }
  else
     esyslog("ERROR: cReceiver called without a PID!");
}

cReceiver::~cReceiver()
{
  Detach();
}

bool cReceiver::WantsPid(int Pid)
{
  if (Pid) {
     for (int i = 0; i < MAXRECEIVEPIDS; i++) {
         if (pids[i] == Pid)
            return true;
         if (!pids[i])
            break;
         }
     }
  return false;
}

void cReceiver::Detach(void)
{
  if (device)
     device->Detach(this);
}
