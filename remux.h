/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 1.3 2001/06/02 15:15:43 kls Exp $
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

//XXX -> remux.c???
// Start codes:
#define SC_PICTURE 0x00  // "picture header"
#define SC_SEQU    0xB3  // "sequence header"
#define SC_PHEAD   0xBA  // "pack header"
#define SC_SHEAD   0xBB  // "system header"
#define SC_AUDIO   0xC0
#define SC_VIDEO   0xE0

// The minimum amount of video data necessary to identify frames:
#define MINVIDEODATA (16*1024) // just a safe guess (max. size of any frame block, plus some safety)

typedef unsigned char uchar;
class cTS2PES;

class cRemux {
private:
  bool exitOnFailure;
  bool synced;
  int skipped;
  dvb_pid_t vPid, aPid;
  cTS2PES *vTS2PES, *aTS2PES;
  uchar resultBuffer[MINVIDEODATA * 4];//XXX
  int resultCount;
  int resultDelivered;
  int GetPid(const uchar *Data);
  int GetPacketLength(const uchar *Data, int Count, int Offset);
  int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
public:
  cRemux(dvb_pid_t VPid, dvb_pid_t APid, bool ExitOnFailure = false);
  ~cRemux();
  void SetAudioPid(int APid);
  const uchar *Process(const uchar *Data, int &Count, int &Result, uchar *PictureType = NULL);
  };

#endif // __REMUX_H
