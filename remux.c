/*
 * remux.c: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * The parts of this code that implement cTS2PES have been taken from
 * the Linux DVB driver's 'tuxplayer' example and were rewritten to suit
 * VDR's needs.
 *
 * $Id: remux.c 1.17 2003/09/14 10:34:39 kls Exp $
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
#include <stdlib.h>
#include "thread.h"
#include "tools.h"

// --- cTS2PES ---------------------------------------------------------------

#include <netinet/in.h>

//XXX TODO: these should really be available in some driver header file!
#define PROG_STREAM_MAP  0xBC
#ifndef PRIVATE_STREAM1
#define PRIVATE_STREAM1  0xBD
#endif
#define PADDING_STREAM   0xBE
#ifndef PRIVATE_STREAM2
#define PRIVATE_STREAM2  0xBF
#endif
#define AUDIO_STREAM_S   0xC0
#define AUDIO_STREAM_E   0xDF
#define VIDEO_STREAM_S   0xE0
#define VIDEO_STREAM_E   0xEF
#define ECM_STREAM       0xF0
#define EMM_STREAM       0xF1
#define DSM_CC_STREAM    0xF2
#define ISO13522_STREAM  0xF3
#define PROG_STREAM_DIR  0xFF

//pts_dts flags
#define PTS_ONLY         0x80

#define TS_SIZE        188
#define PID_MASK_HI    0x1F
#define CONT_CNT_MASK  0x0F

// Flags:
#define PAY_START      0x40
#define TS_ERROR       0x80
#define ADAPT_FIELD    0x20

#define MAX_PLENGTH  0xFFFF          // the maximum PES packet length (theoretically)
#define MMAX_PLENGTH (8*MAX_PLENGTH) // some stations send PES packets that are extremely large, e.g. DVB-T in Finland

#define IPACKS 2048

// Start codes:
#define SC_PICTURE 0x00  // "picture header"

#define MAXNONUSEFULDATA (10*1024*1024)

class cTS2PES {
private:
  int size;
  int found;
  int count;
  uint8_t *buf;
  uint8_t cid;
  uint8_t audioCid;
  int plength;
  uint8_t plen[2];
  uint8_t flag1;
  uint8_t flag2;
  uint8_t hlength;
  int mpeg;
  uint8_t check;
  int which;
  bool done;
  uint8_t *resultBuffer;
  int *resultCount;
  int tsErrors;
  int ccErrors;
  int ccCounter;
  static uint8_t headr[];
  void store(uint8_t *Data, int Count);
  void reset_ipack(void);
  void send_ipack(void);
  void write_ipack(const uint8_t *Data, int Count);
  void instant_repack(const uint8_t *Buf, int Count);
public:
  cTS2PES(uint8_t *ResultBuffer, int *ResultCount, int Size, uint8_t AudioCid = 0x00);
  ~cTS2PES();
  void ts_to_pes(const uint8_t *Buf); // don't need count (=188)
  void Clear(void);
  };

uint8_t cTS2PES::headr[] = { 0x00, 0x00, 0x01 };

cTS2PES::cTS2PES(uint8_t *ResultBuffer, int *ResultCount, int Size, uint8_t AudioCid)
{
  resultBuffer = ResultBuffer;
  resultCount = ResultCount;
  size = Size;
  audioCid = AudioCid;

  tsErrors = 0;
  ccErrors = 0;
  ccCounter = -1;

  if (!(buf = MALLOC(uint8_t, size)))
     esyslog("Not enough memory for ts_transform");

  reset_ipack();
}

cTS2PES::~cTS2PES()
{
  if (tsErrors || ccErrors)
     dsyslog("cTS2PES got %d TS errors, %d TS continuity errors", tsErrors, ccErrors);
  free(buf);
}

void cTS2PES::Clear(void)
{
  reset_ipack();
}

void cTS2PES::store(uint8_t *Data, int Count)
{
  if (*resultCount + Count > RESULTBUFFERSIZE) {
     esyslog("ERROR: result buffer overflow (%d + %d > %d)", *resultCount, Count, RESULTBUFFERSIZE);
     Count = RESULTBUFFERSIZE - *resultCount;
     }
  memcpy(resultBuffer + *resultCount, Data, Count);
  *resultCount += Count;
}

void cTS2PES::reset_ipack(void)
{
  found = 0;
  cid = 0;
  plength = 0;
  flag1 = 0;
  flag2 = 0;
  hlength = 0;
  mpeg = 0;
  check = 0;
  which = 0;
  done = false;
  count = 0;
}

void cTS2PES::send_ipack(void)
{
  if (count < 10)
     return;
  buf[3] = (AUDIO_STREAM_S <= cid && cid <= AUDIO_STREAM_E && audioCid) ? audioCid : cid;
  buf[4] = (uint8_t)(((count - 6) & 0xFF00) >> 8);
  buf[5] = (uint8_t)((count - 6) & 0x00FF);
  store(buf, count);

  switch (mpeg) {
    case 2:
            buf[6] = 0x80;
            buf[7] = 0x00;
            buf[8] = 0x00;
            count = 9;
            break;
    case 1:
            buf[6] = 0x0F;
            count = 7;
            break;
    }
}

void cTS2PES::write_ipack(const uint8_t *Data, int Count)
{
  if (count < 6) {
     memcpy(buf, headr, 3);
     count = 6;
     }

  if (count + Count < size) {
     memcpy(buf + count, Data, Count);
     count += Count;
     }
  else {
     int rest = size - count;
     memcpy(buf + count, Data, rest);
     count += rest;
     send_ipack();
     if (Count - rest > 0)
        write_ipack(Data + rest, Count - rest);
     }
}

void cTS2PES::instant_repack(const uint8_t *Buf, int Count)
{
  int c = 0;

  while (c < Count && (mpeg == 0 || (mpeg == 1 && found < 7) || (mpeg == 2 && found < 9)) && (found < 5 || !done)) {
        switch (found ) {
          case 0:
          case 1:
                  if (Buf[c] == 0x00)
                     found++;
                  else
                     found = 0;
                  c++;
                  break;
          case 2:
                  if (Buf[c] == 0x01)
                     found++;
                  else if (Buf[c] != 0)
                     found = 0;
                  c++;
                  break;
          case 3:
                  cid = 0;
                  switch (Buf[c]) {
                    case PROG_STREAM_MAP:
                    case PRIVATE_STREAM2:
                    case PROG_STREAM_DIR:
                    case ECM_STREAM     :
                    case EMM_STREAM     :
                    case PADDING_STREAM :
                    case DSM_CC_STREAM  :
                    case ISO13522_STREAM:
                         done = true;
                    case PRIVATE_STREAM1:
                    case VIDEO_STREAM_S ... VIDEO_STREAM_E:
                    case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                         found++;
                         cid = Buf[c++];
                         break;
                    default:
                         found = 0;
                         break;
                    }
                  break;
          case 4:
                  if (Count - c > 1) {
                     unsigned short *pl = (unsigned short *)(Buf + c);
                     plength = ntohs(*pl);
                     c += 2;
                     found += 2;
                     }
                  else {
                     plen[0] = Buf[c];
                     found++;
                     return;
                     }
                  break;
          case 5: {
                    plen[1] = Buf[c++];
                    unsigned short *pl = (unsigned short *)plen;
                    plength = ntohs(*pl);
                    found++;
                  }
                  break;
          case 6:
                  if (!done) {
                     flag1 = Buf[c++];
                     found++;
                     if ((flag1 & 0xC0) == 0x80 )
                        mpeg = 2;
                     else {
                        hlength = 0;
                        which = 0;
                        mpeg = 1;
                        flag2 = 0;
                        }
                     }
                  break;
          case 7:
                  if (!done && mpeg == 2) {
                     flag2 = Buf[c++];
                     found++;
                     }
                  break;
          case 8:
                  if (!done && mpeg == 2) {
                     hlength = Buf[c++];
                     found++;
                     }
                  break;
          default:
                  break;
          }
        }

  if (!plength)
     plength = MMAX_PLENGTH - 6;

  if (done || ((mpeg == 2 && found >= 9) || (mpeg == 1 && found >= 7))) {
     switch (cid) {
       case AUDIO_STREAM_S ... AUDIO_STREAM_E:
       case VIDEO_STREAM_S ... VIDEO_STREAM_E:
       case PRIVATE_STREAM1:

            if (mpeg == 2 && found == 9) {
               write_ipack(&flag1, 1);
               write_ipack(&flag2, 1);
               write_ipack(&hlength, 1);
               }

            if (mpeg == 1 && found == 7)
               write_ipack(&flag1, 1);

            if (mpeg == 2 && (flag2 & PTS_ONLY) && found < 14) {
               while (c < Count && found < 14) {
                     write_ipack(Buf + c, 1);
                     c++;
                     found++;
                     }
               if (c == Count)
                  return;
               }

            while (c < Count && found < plength + 6) {
                  int l = Count - c;
                  if (l + found > plength + 6)
                     l = plength + 6 - found;
                  write_ipack(Buf + c, l);
                  found += l;
                  c += l;
                  }

            break;
       }

     if (done) {
        if (found + Count - c < plength + 6) {
           found += Count - c;
           c = Count;
           }
        else {
           c += plength + 6 - found;
           found = plength + 6;
           }
        }

     if (plength && found == plength + 6) {
        if (plength == MMAX_PLENGTH - 6)
           esyslog("ERROR: PES packet length overflow in remuxer (stream corruption)");
        send_ipack();
        reset_ipack();
        if (c < Count)
           instant_repack(Buf + c, Count - c);
        }
     }
  return;
}

void cTS2PES::ts_to_pes(const uint8_t *Buf) // don't need count (=188)
{
  if (!Buf)
     return;

  if (Buf[1] & TS_ERROR)
     tsErrors++;
  if ((Buf[3] ^ ccCounter) & CONT_CNT_MASK) {
     // This should check duplicates and packets which do not increase the counter.
     // But as the errors usually come in bursts this should be enough to
     // show you there is something wrong with signal quality.
     if (ccCounter != -1 && ((Buf[3] ^ (ccCounter + 1)) & CONT_CNT_MASK)) {
        ccErrors++;
        // Enable this if you are having problems with signal quality.
        // These are the errors I used to get with Nova-T when antenna
        // was not positioned correcly (not transport errors). //tvr
        //dsyslog("TS continuity error (%d)", ccCounter);
        }
     ccCounter = Buf[3] & CONT_CNT_MASK;
     }

  if (Buf[1] & PAY_START) {
     if (plength == MMAX_PLENGTH - 6 && found > 6) {
        plength = found - 6;
        found = 0;
        send_ipack();
        reset_ipack();
        }
     }

  uint8_t off = 0;

  if (Buf[3] & ADAPT_FIELD) {  // adaptation field?
     off = Buf[4] + 1;
     if (off + 4 > 187)
        return;
     }

  instant_repack(Buf + 4 + off, TS_SIZE - 4 - off);
}

// --- cRemux ----------------------------------------------------------------

cRemux::cRemux(int VPid, int APid1, int APid2, int DPid1, int DPid2, bool ExitOnFailure)
{
  vPid = VPid;
  aPid1 = APid1;
  aPid2 = APid2;
  dPid1 = DPid1;
  dPid2 = DPid2;
  exitOnFailure = ExitOnFailure;
  synced = false;
  skipped = 0;
  resultCount = resultDelivered = 0;
  vTS2PES  =         new cTS2PES(resultBuffer, &resultCount, IPACKS);
  aTS2PES1 =         new cTS2PES(resultBuffer, &resultCount, IPACKS, 0xC0);
  aTS2PES2 = aPid2 ? new cTS2PES(resultBuffer, &resultCount, IPACKS, 0xC1) : NULL;
  dTS2PES1 = dPid1 ? new cTS2PES(resultBuffer, &resultCount, IPACKS)       : NULL;
  //XXX don't yet know how to tell apart primary and secondary DD data...
  dTS2PES2 = /*XXX dPid2 ? new cTS2PES(resultBuffer, &resultCount, IPACKS) : XXX*/ NULL;
}

cRemux::~cRemux()
{
  delete vTS2PES;
  delete aTS2PES1;
  delete aTS2PES2;
  delete dTS2PES1;
  delete dTS2PES2;
}

int cRemux::GetPid(const uchar *Data)
{
  return (((uint16_t)Data[0] & PID_MASK_HI) << 8) | (Data[1] & 0xFF);
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

#define TS_SYNC_BYTE 0x47

uchar *cRemux::Process(const uchar *Data, int &Count, int &Result, uchar *PictureType)
{
  uchar dummyPictureType;
  if (!PictureType)
     PictureType = &dummyPictureType;

/*XXX
  // test recording the raw TS:
  Result = Count;
  *PictureType = I_FRAME;
  return Data;
XXX*/

  // Remove any previously delivered data from the result buffer:

  if (resultDelivered) {
     if (resultDelivered < resultCount)
        memmove(resultBuffer, resultBuffer + resultDelivered, resultCount - resultDelivered);
     resultCount -= resultDelivered;
     resultDelivered = 0;
     }

  int used = 0;

  // Make sure we are looking at a TS packet:

  while (Count > TS_SIZE) {
        if (Data[0] == TS_SYNC_BYTE && Data[TS_SIZE] == TS_SYNC_BYTE)
           break;
        Data++;
        Count--;
        used++;
        }
  if (used)
     esyslog("ERROR: skipped %d byte to sync on TS packet", used);

  // Convert incoming TS data into multiplexed PES:

  for (int i = 0; i < Count; i += TS_SIZE) {
      if (Count - i < TS_SIZE)
         break;
      if (Data[i] != TS_SYNC_BYTE)
         break;
      int pid = GetPid(Data + i + 1);
      if (Data[i + 3] & 0x10) { // got payload
         if      (pid == vPid)              vTS2PES->ts_to_pes(Data + i);
         else if (pid == aPid1)             aTS2PES1->ts_to_pes(Data + i);
         else if (pid == aPid2 && aTS2PES2) aTS2PES2->ts_to_pes(Data + i);
         else if (pid == dPid1 && dTS2PES1) dTS2PES1->ts_to_pes(Data + i);
         else if (pid == dPid2 && dTS2PES2) dTS2PES2->ts_to_pes(Data + i);
         }
      used += TS_SIZE;
      if (resultCount > (int)sizeof(resultBuffer) / 2)
         break;
      }
  Count = used;

/*XXX
  // test recording without determining the real frame borders:
  *PictureType = I_FRAME;
  Result = resultDelivered = resultCount;
  return Result ? resultBuffer : NULL;
XXX*/

  // Special VPID case to enable recording radio channels:

  if (vPid == 0 || vPid == 1 || vPid == 0x1FFF) {
     // XXX actually '0' should be enough, but '1' must be used with encrypted channels (driver bug?)
     // XXX also allowing 0x1FFF to not break Michael Paar's original patch,
     // XXX but it would probably be best to only use '0'
     *PictureType = I_FRAME;
     Result = resultDelivered = resultCount;
     return Result ? resultBuffer : NULL;
     }

  // Check if we're getting anywhere here:

  if (!synced && skipped >= 0) {
     if (skipped > MAXNONUSEFULDATA) {
        esyslog("ERROR: no useful data seen within %d byte of video stream", skipped);
        skipped = -1;
        if (exitOnFailure)
           cThread::EmergencyExit(true);
        }
     else
        skipped += Count;
     }

  // Check for frame borders:

  *PictureType = NO_PICTURE;

  if (resultCount >= MINVIDEODATA) {
     for (int i = 0; i < resultCount; i++) {
         if (resultBuffer[i] == 0 && resultBuffer[i + 1] == 0 && resultBuffer[i + 2] == 1) {
            switch (resultBuffer[i + 3]) {
              case VIDEO_STREAM_S ... VIDEO_STREAM_E:
                   {
                     uchar pt = NO_PICTURE;
                     int l = ScanVideoPacket(resultBuffer, resultCount, i, pt);
                     if (l < 0)
                        return NULL; // no useful data found, wait for more
                     if (pt != NO_PICTURE) {
                        if (pt < I_FRAME || B_FRAME < pt)
                           esyslog("ERROR: unknown picture type '%d'", pt);
                        else if (!synced) {
                           if (pt == I_FRAME) {
                              resultDelivered = i; // will drop everything before this position
                              SetBrokenLink(resultBuffer + i, l);
                              synced = true;
                              }
                           else {
                              resultDelivered = i + l; // will drop everything before and including this packet
                              return NULL;
                              }
                           }
                        }
                     if (synced) {
                        *PictureType = pt;
                        Result = l;
                        uchar *p = resultBuffer + resultDelivered;
                        resultDelivered += l;
                        return p;
                        }
                     else {
                        resultDelivered = i + l; // will drop everything before and including this packet
                        return NULL;
                        }
                   }
                   break;
              case PRIVATE_STREAM1:
              case AUDIO_STREAM_S ... AUDIO_STREAM_E:
                   {
                     int l = GetPacketLength(resultBuffer, resultCount, i);
                     if (l < 0)
                        return NULL; // no useful data found, wait for more
                     if (synced) {
                        Result = l;
                        uchar *p = resultBuffer + resultDelivered;
                        resultDelivered += l;
                        return p;
                        }
                     else {
                        resultDelivered = i + l; // will drop everything before and including this packet
                        return NULL;
                        }
                   }
                   break;
              }
            }
         }
     }
  return NULL; // no useful data found, wait for more
}

void cRemux::SetBrokenLink(uchar *Data, int Length)
{
  if (Length > 9 && Data[0] == 0 && Data[1] == 0 && Data[2] == 1 && (Data[3] & 0xF0) == VIDEO_STREAM_S) {
     for (int i = Data[8] + 9; i < Length - 7; i++) { // +9 to skip video packet header
         if (Data[i] == 0 && Data[i + 1] == 0 && Data[i + 2] == 1 && Data[i + 3] == 0xB8) {
            if (!(Data[i + 7] & 0x40)) // set flag only if GOP is not closed
               Data[i + 7] |= 0x20;
            return;
            }
         }
     dsyslog("SetBrokenLink: no GOP header found in video packet");
     }
  else
     dsyslog("SetBrokenLink: no video packet in frame");
}
