/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 1.10 2003/04/26 14:13:11 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

#include <time.h> //XXX FIXME: DVB/linux/dvb/dmx.h should include <time.h> itself!!!
#include <linux/dvb/dmx.h>
#include "tools.h"

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

// The minimum amount of video data necessary to identify frames:
#define MINVIDEODATA (16*1024) // just a safe guess (max. size of any frame block, plus some safety)

#define RESULTBUFFERSIZE (MINVIDEODATA * 4)

class cTS2PES;

class cRemux {
private:
  bool exitOnFailure;
  bool synced;
  int skipped;
  int vPid, aPid1, aPid2, dPid1, dPid2;
  cTS2PES *vTS2PES, *aTS2PES1, *aTS2PES2, *dTS2PES1, *dTS2PES2;
  uchar resultBuffer[RESULTBUFFERSIZE];
  int resultCount;
  int resultDelivered;
  int GetPid(const uchar *Data);
  int GetPacketLength(const uchar *Data, int Count, int Offset);
  int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
public:
  cRemux(int VPid, int APid1, int APid2, int DPid1, int DPid2, bool ExitOnFailure = false);
  ~cRemux();
  uchar *Process(const uchar *Data, int &Count, int &Result, uchar *PictureType = NULL);
  static void SetBrokenLink(uchar *Data, int Length);
  };

#endif // __REMUX_H
