/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 1.4 2001/06/14 15:27:07 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

#include <time.h> //XXX FIXME: DVB/ost/include/ost/dmx.h should include <time.h> itself!!!
#include <ost/dmx.h>

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

// The minimum amount of video data necessary to identify frames:
#define MINVIDEODATA (16*1024) // just a safe guess (max. size of any frame block, plus some safety)

#define RESULTBUFFERSIZE (MINVIDEODATA * 4)

typedef unsigned char uchar;
class cTS2PES;

class cRemux {
private:
  bool exitOnFailure;
  bool synced;
  int skipped;
  dvb_pid_t vPid, aPid1, aPid2;
  cTS2PES *vTS2PES, *aTS2PES1, *aTS2PES2;
  uchar resultBuffer[RESULTBUFFERSIZE];
  int resultCount;
  int resultDelivered;
  int GetPid(const uchar *Data);
  int GetPacketLength(const uchar *Data, int Count, int Offset);
  int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
public:
  cRemux(dvb_pid_t VPid, dvb_pid_t APid1, dvb_pid_t APid2 = 0, bool ExitOnFailure = false);
  ~cRemux();
  void SetAudioPid(int APid);
  const uchar *Process(const uchar *Data, int &Count, int &Result, uchar *PictureType = NULL);
  };

#endif // __REMUX_H
