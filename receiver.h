/*
 * receiver.h: The basic receiver interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: receiver.h 1.5 2007/01/05 11:00:36 kls Exp $
 */

#ifndef __RECEIVER_H
#define __RECEIVER_H

#include "device.h"

#define MAXRECEIVEPIDS  64 // the maximum number of PIDs per receiver

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
  cReceiver(tChannelID ChannelID, int Priority, int Pid, const int *Pids1 = NULL, const int *Pids2 = NULL, const int *Pids3 = NULL);
               ///< Creates a new receiver for the channel with the given ChannelID with
               ///< the given Priority. Pid is a single PID (typically the video PID), while
               ///< Pids1...Pids3 are pointers to zero terminated lists of PIDs.
               ///< If any of these PIDs are 0, they will be silently ignored.
               ///< The total number of non-zero PIDs must not exceed MAXRECEIVEPIDS.
               ///< Priority may be any value in the range -99..99. Negative values indicate
               ///< that this cReceiver may be detached at any time (without blocking the
               ///< cDevice it is attached to).
               ///< The ChannelID is necessary to allow the device that will be used for this
               ///< receiver to detect and store whether the channel can be decrypted in case
               ///< this is an encrypted channel. If the channel is not encrypted or this
               ///< detection is not wanted, an invalid tChannelID may be given.
  virtual ~cReceiver();
  tChannelID ChannelID(void) { return channelID; }
  bool IsAttached(void) { return device != NULL; }
               ///< Returns true if this receiver is (still) attached to a device.
               ///< A receiver may be automatically detached from its device in
               ///< case the device is needed otherwise, so code that uses a cReceiver
               ///< should repeatedly check whether it is still attached, and if
               ///< it isn't, delete it (or take any other appropriate measures).
  };

#endif //__RECEIVER_H
