/*
 * receiver.h: The basic receiver interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: receiver.h 2.2 2011/12/04 13:38:17 kls Exp $
 */

#ifndef __RECEIVER_H
#define __RECEIVER_H

#include "device.h"

#define MAXRECEIVEPIDS  64 // the maximum number of PIDs per receiver

#define LEGACY_CRECEIVER // Code enclosed with this macro is deprecated and may be removed in a future version

class cReceiver {
  friend class cDevice;
private:
  cDevice *device;
  tChannelID channelID;
  int priority;
  int pids[MAXRECEIVEPIDS];
  int numPids;
  bool WantsPid(int Pid);
protected:
  void Detach(void);
  virtual void Activate(bool On) {}
               ///< This function is called just before the cReceiver gets attached to
               ///< (On == true) or detached from (On == false) a cDevice. It can be used
               ///< to do things like starting/stopping a thread.
               ///< It is guaranteed that Receive() will not be called before Activate(true).
  virtual void Receive(uchar *Data, int Length) = 0;
               ///< This function is called from the cDevice we are attached to, and
               ///< delivers one TS packet from the set of PIDs the cReceiver has requested.
               ///< The data packet must be accepted immediately, and the call must return
               ///< as soon as possible, without any unnecessary delay. Each TS packet
               ///< will be delivered only ONCE, so the cReceiver must make sure that
               ///< it will be able to buffer the data if necessary.
public:
#ifdef LEGACY_CRECEIVER
  cReceiver(tChannelID ChannelID, int Priority, int Pid, const int *Pids1 = NULL, const int *Pids2 = NULL, const int *Pids3 = NULL);
#endif
  cReceiver(const cChannel *Channel = NULL, int Priority = -1);
               ///< Creates a new receiver for the given Channel with the given Priority.
               ///< If Channel is not NULL, its pids set by a call to SetPids().
               ///< Otherwise pids can be added to the receiver by separate calls to the AddPid[s]
               ///< functions.
               ///< The total number of PIDs added to a receiver must not exceed MAXRECEIVEPIDS.
               ///< Priority may be any value in the range -99..99. Negative values indicate
               ///< that this cReceiver may be detached at any time (without blocking the
               ///< cDevice it is attached to).
  virtual ~cReceiver();
  bool AddPid(int Pid);
               ///< Adds the given Pid to the list of PIDs of this receiver.
  bool AddPids(const int *Pids);
               ///< Adds the given zero terminated list of Pids to the list of PIDs of this
               ///< receiver.
  bool AddPids(int Pid1, int Pid2, int Pid3 = 0, int Pid4 = 0, int Pid5 = 0, int Pid6 = 0, int Pid7 = 0, int Pid8 = 0, int Pid9 = 0);
               ///< Adds the given Pids to the list of PIDs of this receiver.
  bool SetPids(const cChannel *Channel);
               ///< Sets the PIDs of this receiver to those of the given Channel,
               ///< replacing and previously stored PIDs. If Channel is NULL, all
               ///< PIDs will be cleared. Parameters in the Setup may control whether
               ///< certain types of PIDs (like Dolby Digital, for instance) are
               ///< actually set. The Channel's ID is stored and can later be retrieved
               ///< through ChannelID(). The ChannelID is necessary to allow the device
               ///< that will be used for this receiver to detect and store whether the
               ///< channel can be decrypted in case this is an encrypted channel.
  tChannelID ChannelID(void) { return channelID; }
  bool IsAttached(void) { return device != NULL; }
               ///< Returns true if this receiver is (still) attached to a device.
               ///< A receiver may be automatically detached from its device in
               ///< case the device is needed otherwise, so code that uses a cReceiver
               ///< should repeatedly check whether it is still attached, and if
               ///< it isn't, delete it (or take any other appropriate measures).
  };

#endif //__RECEIVER_H
