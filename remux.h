/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 1.12 2004/10/15 12:31:16 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

#include <time.h> //XXX FIXME: DVB/linux/dvb/dmx.h should include <time.h> itself!!!
#include <linux/dvb/dmx.h>
#include "ringbuffer.h"
#include "tools.h"

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

class cTS2PES;

class cRemux {
private:
  bool exitOnFailure;
  int numUPTerrors;
  bool synced;
  int skipped;
  int vPid, aPid1, aPid2, dPid1, dPid2;
  cTS2PES *vTS2PES, *aTS2PES1, *aTS2PES2, *dTS2PES1, *dTS2PES2;
  cRingBufferLinear *resultBuffer;
  int resultSkipped;
  int GetPid(const uchar *Data);
  int GetPacketLength(const uchar *Data, int Count, int Offset);
  int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
public:
  cRemux(int VPid, int APid1, int APid2, int DPid1, int DPid2, bool ExitOnFailure = false);
  ~cRemux();
  int Put(const uchar *Data, int Count);
       ///< Puts at most Count bytes of Data into the remuxer.
       ///< \return Returns the number of bytes actually consumed from Data.
  uchar *Get(int &Count, uchar *PictureType = NULL);
       ///< Gets all currently available data from the remuxer.
       ///< \return Count contains the number of bytes the result points to, and
       ///< PictureType (if not NULL) will contain one of NO_PICTURE, I_FRAME, P_FRAME
       ///< or B_FRAME.
  void Del(int Count);
       ///< Deletes Count bytes from the remuxer. Count must be the number returned
       ///< from a previous call to Get(). Several calls to Del() with fractions of
       ///< a previously returned Count may be made, but the total sum of all Count
       ///< values must be exactly what the previous Get() has returned.
  void Clear(void);
       ///< Clears the remuxer of all data it might still contain, keeping the PID
       ///< settings as they are.
  static void SetBrokenLink(uchar *Data, int Length);
  };

#endif // __REMUX_H
