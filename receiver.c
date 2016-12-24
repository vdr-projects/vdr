/*
 * receiver.c: The basic receiver interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: receiver.c 4.1 2015/09/16 11:19:47 kls Exp $
 */

#include "receiver.h"
#include <stdio.h>
#include "tools.h"

cReceiver::cReceiver(const cChannel *Channel, int Priority)
{
  device = NULL;
  SetPriority(Priority);
  numPids = 0;
  SetPids(Channel);
}

cReceiver::~cReceiver()
{
  if (device) {
     const char *msg = "ERROR: cReceiver has not been detached yet! This is a design fault and VDR will abort now!";
     esyslog("%s", msg);
     fprintf(stderr, "%s\n", msg);
     abort();
     }
}

void cReceiver::SetPriority(int Priority)
{
  priority = constrain(Priority, MINPRIORITY, MAXPRIORITY);
}

bool cReceiver::AddPid(int Pid)
{
  if (Pid) {
     if (numPids < MAXRECEIVEPIDS) {
        if (!WantsPid(Pid))
           pids[numPids++] = Pid;
        }
     else {
        dsyslog("too many PIDs in cReceiver (Pid = %d)", Pid);
        return false;
        }
     }
  return true;
}

bool cReceiver::AddPids(const int *Pids)
{
  if (Pids) {
     while (*Pids) {
           if (!AddPid(*Pids++))
              return false;
           }
     }
  return true;
}

bool cReceiver::AddPids(int Pid1, int Pid2, int Pid3, int Pid4, int Pid5, int Pid6, int Pid7, int Pid8, int Pid9)
{
  return AddPid(Pid1) && AddPid(Pid2) && AddPid(Pid3) && AddPid(Pid4) && AddPid(Pid5) && AddPid(Pid6) && AddPid(Pid7) && AddPid(Pid8) && AddPid(Pid9);
}

bool cReceiver::SetPids(const cChannel *Channel)
{
  numPids = 0;
  if (Channel) {
     channelID = Channel->GetChannelID();
     return AddPid(Channel->Vpid()) &&
            (Channel->Ppid() == Channel->Vpid() || AddPid(Channel->Ppid())) &&
            AddPids(Channel->Apids()) &&
            AddPids(Channel->Dpids()) &&
            AddPids(Channel->Spids());
     }
  return true;
}

void cReceiver::DelPid(int Pid)
{
  if (Pid) {
     for (int i = 0; i < numPids; i++) {
         if (pids[i] == Pid) {
            for ( ; i < numPids; i++) // we also copy the terminating 0!
                pids[i] = pids[i + 1];
            numPids--;
            return;
            }
         }
     }
}

void cReceiver::DelPids(const int *Pids)
{
  if (Pids) {
     while (*Pids)
           DelPid(*Pids++);
     }
}

bool cReceiver::WantsPid(int Pid)
{
  if (Pid) {
     for (int i = 0; i < numPids; i++) {
         if (pids[i] == Pid)
            return true;
         }
     }
  return false;
}

void cReceiver::Detach(void)
{
  if (device)
     device->Detach(this);
}
