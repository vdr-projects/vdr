/*
 * remux.c: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.c 1.1 2001/03/31 08:42:17 kls Exp $
 */

/* The calling interface of the 'cRemux::Process()' function is defined
   as follows:

   'Data' points to a chunk of data that consists of 'Count' bytes.
   The 'Process' function shall try to remultiplex as much of the
   data as possible and return a pointer to the resulting buffer.
   That buffer typically is different from the incoming 'Data',
   but in the simplest case (when 'Process' does nothing) might
   as well point to the original 'Data'. When returning, 'Count'
   shall be set to the number of bytes that have been processed
   (i.e. have been taken from 'Data'), while 'Result' indicates
   how many bytes the returned buffer contains. 'PictureType' shall
   be set to NO_PICTURE if the returned data does not start a new
   picture, or one of I_FRAME, P_FRAME or B_FRAME if a new picture
   starting point has been found. This also means that the returned
   data buffer may contain at most one entire video frame, because
   the next frame must be returned with its own value for 'PictureType'.

   'Process' shall do it's best to keep the latency time as short
   as possible in order to allow a quick start of VDR's "Transfer
   mode" (displaying the signal of one DVB card on another card).
   In order to do that, this function may decide to first pass
   through the incoming data (almost) unprocessed, and make
   actual processing kick in after a few seconds (if that is at
   all possible for the algorithm). This may result in a non-
   optimal stream at the beginning, which won't matter for normal
   recordings but may make switching through encrypted channels
   in "Transfer mode" faster.

   In the resulting data stream, a new packet shall always be started
   when a frame border is encountered. VDR needs this in order to
   be able to detect and store the frame indexes, and to easily
   display single frames in fast forward/back mode. The very first
   data block returned shall be the starting point of an I_FRAME.
   Everything before that shall be silently dropped.

   If the incoming data is not enough to do remultiplexing, a value
   of NULL shall be returned ('Result' has no meaning then). This
   will tell the caller to wait for more data to be presented in
   the next call. If NULL is returned and 'Count' is not 0, the
   caller shall remove 'Count' bytes from the beginning of 'Data'
   before the next call. This is the way 'Process' indicates that
   it must skip that data.

   Any data that is not used during this call will appear at the
   beginning of the incoming 'Data' buffer at the next call, plus
   any new data that has become available.

   It is guaranteed that the caller will completely process any
   returned data before the next call to 'Process'. That way, 'Process'
   can dynamically allocate its return buffer and be sure the caller
   doesn't keep any pointers into that buffer.
*/

#include "remux.h"
#include "tools.h"

#if defined(REMUX_NONE)

cRemux::cRemux(void)
{
  synced = false;
}

cRemux::~cRemux()
{
}

int cRemux::GetPacketLength(const uchar *Data, int Count, int Offset)
{
  // Returns the entire length of the packet starting at offset, or -1 in case of error.
  return (Offset + 5 < Count) ? (Data[Offset + 4] << 8) + Data[Offset + 5] + 6 : -1;
}

int cRemux::ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType)
{
  // Scans the video packet starting at Offset and returns its length.
  // If the return value is -1 the packet was not completely in the buffer.

  int Length = GetPacketLength(Data, Count, Offset);
  if (Length > 0 && Offset + Length <= Count) {
     int i = Offset + 8; // the minimum length of the video packet header
     i += Data[i] + 1;   // possible additional header bytes
     for (; i < Offset + Length; i++) {
         if (Data[i] == 0 && Data[i + 1] == 0 && Data[i + 2] == 1) {
            switch (Data[i + 3]) {
              case SC_PICTURE: PictureType = (Data[i + 5] >> 3) & 0x07;
                               return Length;
              }
            }
         }
     PictureType = NO_PICTURE;
     return Length;
     }
  return -1;
}

const uchar *cRemux::Process(const uchar *Data, int &Count, int &Result, uchar &PictureType)
{
  int Skip = 0;

  PictureType = NO_PICTURE;

  if (Count >= MINVIDEODATA) {
     for (int i = 0; i < Count; i++) {
         if (Data[i] == 0 && Data[i + 1] == 0 && Data[i + 2] == 1) {
            switch (Data[i + 3]) {
              case SC_VIDEO:
                   {
                     uchar pt = NO_PICTURE;
                     int l = ScanVideoPacket(Data, Count, i, pt);
                     if (l < 0) {
                        if (Skip < Count)
                           Count = Skip;
                        return NULL; // no useful data found, wait for more
                        }
                     if (pt != NO_PICTURE) {
                        if (pt < I_FRAME || B_FRAME < pt) {
                           esyslog(LOG_ERR, "ERROR: unknown picture type '%d'", pt);
                           }
                        else if (PictureType == NO_PICTURE) {
                           if (!synced) {
                              if (pt == I_FRAME) {
                                 Skip = i;
                                 synced = true;
                                 }
                              else {
                                 i += l;
                                 Skip = i;
                                 break;
                                 }
                              }
                           if (synced)
                              PictureType = pt;
                           }
                        else {
                           Count = i;
                           Result = i - Skip;
                           return Data + Skip;
                           }
                        }
                     else if (!synced) {
                        i += l;
                        Skip = i;
                        break;
                        }
                     i += l - 1; // -1 to compensate for i++ in the loop!
                   }
                   break;
              case SC_AUDIO:
                   i += GetPacketLength(Data, Count, i) - 1; // -1 to compensate for i++ in the loop!
                   break;
              }
            }
         }
     }
  if (Skip < Count)
     Count = Skip;
  return NULL; // no useful data found, wait for more
}

#elif defined(REMUX_TEST)
#endif

