/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 1.1 2001/03/31 08:42:27 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

// There are various experiments with different types of remultiplexers
// going on at the moment. Select the remultiplexer here:
#define REMUX_NONE 1
//#define REMUX_TEST 1

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

// Start codes:
#define SC_PICTURE 0x00  // "picture header"
#define SC_SEQU    0xB3  // "sequence header"
#define SC_PHEAD   0xBA  // "pack header"
#define SC_SHEAD   0xBB  // "system header"
#define SC_AUDIO   0xC0
#define SC_VIDEO   0xE0

// The minimum amount of video data necessary to identify frames:
#define MINVIDEODATA (256*1024) // just a safe guess (max. size of any frame block, plus some safety)

typedef unsigned char uchar;

class cRemux {
private:
#if defined(REMUX_NONE)
  bool synced;
  int GetPacketLength(const uchar *Data, int Count, int Offset);
  int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
#elif defined(REMUX_TEST)
#endif
public:
  cRemux(void);
  ~cRemux();
  const uchar *Process(const uchar *Data, int &Count, int &Result, uchar &PictureType);
  };

#endif // __REMUX_H
