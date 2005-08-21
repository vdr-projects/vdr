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
 * The cDolbyRepacker code was originally written by Reinhard Nissl <rnissl@gmx.de>,
 * and adapted to the VDR coding style by Klaus.Schmidinger@cadsoft.de.
 *
 * $Id: remux.c 1.37 2005/08/21 08:58:58 kls Exp $
 */

#include "remux.h"
#include <stdlib.h>
#include "channels.h"
#include "thread.h"
#include "tools.h"

// --- cRepacker -------------------------------------------------------------

class cRepacker {
protected:
  int maxPacketSize;
  uint8_t subStreamId;
  static void DroppedData(const char *Reason, int Count) { esyslog("%s (dropped %d bytes)", Reason, Count); }
  static int Put(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count)
  {
    int n = ResultBuffer->Put(Data, Count);
    if (n != Count)
       esyslog("ERROR: result buffer overflow, dropped %d out of %d byte", Count - n, Count);
    return n;
  }
  static int AnalyzePesHeader(const uchar *Data, int Count, int &PesPayloadOffset, bool *ContinuationHeader = 0);
public:
  cRepacker(void) { maxPacketSize = 6 + 65535; subStreamId = 0; }
  virtual ~cRepacker() {}
  virtual void Reset(void) {}
  virtual void Repack(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count) = 0;
  virtual int BreakAt(const uchar *Data, int Count) = 0;
  virtual int QuerySnoopSize(void) { return 0; }
  void SetMaxPacketSize(int MaxPacketSize) { maxPacketSize = MaxPacketSize; }
  void SetSubStreamId(uint8_t SubStreamId) { subStreamId = SubStreamId; }
  };

int cRepacker::AnalyzePesHeader(const uchar *Data, int Count, int &PesPayloadOffset, bool *ContinuationHeader)
{
  if (Count < 7)
     return -1; // too short

  if ((Data[6] & 0xC0) == 0x80) { // MPEG 2
     if (Count < 9)
        return -1; // too short

     PesPayloadOffset = 6 + 3 + Data[8];
     if (Count < PesPayloadOffset)
        return -1; // too short

     if (ContinuationHeader)
        *ContinuationHeader = ((Data[6] == 0x80) && !Data[7] && !Data[8]);

     return 2; // MPEG 2
     }

  // check for MPEG 1 ...
  PesPayloadOffset = 6;

  // skip up to 16 stuffing bytes
  for (int i = 0; i < 16; i++) {
      if (Data[PesPayloadOffset] != 0xFF)
         break;

      if (Count <= ++PesPayloadOffset)
         return -1; // too short
      }

  // skip STD_buffer_scale/size
  if ((Data[PesPayloadOffset] & 0xC0) == 0x40) {
     PesPayloadOffset += 2;

     if (Count <= PesPayloadOffset)
        return -1; // too short
     }

  if (ContinuationHeader)
     *ContinuationHeader = false;

  if ((Data[PesPayloadOffset] & 0xF0) == 0x20) {
     // skip PTS only
     PesPayloadOffset += 5;
     }
  else if ((Data[PesPayloadOffset] & 0xF0) == 0x30) {
     // skip PTS and DTS
     PesPayloadOffset += 10;
     }
  else if (Data[PesPayloadOffset] == 0x0F) {
     // continuation header
     PesPayloadOffset++;

     if (ContinuationHeader)
        *ContinuationHeader = true;
     }
  else
     return 0; // unknown

  if (Count < PesPayloadOffset)
     return -1; // too short

  return 1; // MPEG 1
}

// --- cVideoRepacker --------------------------------------------------------

class cVideoRepacker : public cRepacker {
private:
  int skippedBytes;
  int packetTodo;
  uchar fragmentData[6 + 65535 + 3];
  int fragmentLen;
  uchar pesHeader[6 + 3 + 255 + 3];
  int pesHeaderLen;
  uchar pesHeaderBackup[6 + 3 + 255];
  int pesHeaderBackupLen;
  uint32_t scanner;
  enum eState {
    syncing,
    findPicture,
    scanPicture
    } state;
  uint32_t localScanner;
  int localStart;
  bool PushOutPacket(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count);
public:
  cVideoRepacker(void);
  virtual void Reset(void);
  virtual void Repack(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count);
  virtual int BreakAt(const uchar *Data, int Count);
  virtual int QuerySnoopSize() { return 4; }
  };

cVideoRepacker::cVideoRepacker(void)
{
  Reset();
}

void cVideoRepacker::Reset(void)
{
  skippedBytes = 0;
  packetTodo = maxPacketSize - 6 - 3;
  fragmentLen = 0;
  pesHeaderLen = 0;
  pesHeaderBackupLen = 0;
  scanner = 0xFFFFFFFF;
  state = syncing;
  localStart = -1;
}

bool cVideoRepacker::PushOutPacket(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count)
{
  // enter packet length into PES header ...
  if (fragmentLen > 0) { // ... which is contained in the fragment buffer
     // determine PES packet payload
     int PacketLen = fragmentLen + Count - 6;
     fragmentData[ 4 ] = PacketLen >> 8;
     fragmentData[ 5 ] = PacketLen & 0xFF;
     // amount of data to put into result buffer: a negative Count value means
     // to strip off any partially contained start code.
     int Bite = fragmentLen + (Count >= 0 ? 0 : Count);
     // put data into result buffer
     int n = Put(ResultBuffer, fragmentData, Bite);
     fragmentLen = 0;
     if (n != Bite)
        return false;
     }
  else if (pesHeaderLen > 0) { // ... which is contained in the PES header buffer
     int PacketLen = pesHeaderLen + Count - 6;
     pesHeader[ 4 ] = PacketLen >> 8;
     pesHeader[ 5 ] = PacketLen & 0xFF;
     // amount of data to put into result buffer: a negative Count value means
     // to strip off any partially contained start code.
     int Bite = pesHeaderLen + (Count >= 0 ? 0 : Count);
     // put data into result buffer
     int n = Put(ResultBuffer, pesHeader, Bite);
     pesHeaderLen = 0;
     if (n != Bite)
        return false;
     }
  // append further payload
  if (Count > 0) {
     // amount of data to put into result buffer
     int Bite = Count;
     // put data into result buffer
     int n = Put(ResultBuffer, Data, Bite);
     if (n != Bite)
        return false;
     }
  // we did it ;-)
  return true;
}

void cVideoRepacker::Repack(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count)
{
  // synchronisation is detected some bytes after frame start.
  const int SkippedBytesLimit = 4;

  // reset local scanner
  localStart = -1;

  int pesPayloadOffset = 0;
  bool continuationHeader = false;
  int mpegLevel = AnalyzePesHeader(Data, Count, pesPayloadOffset, &continuationHeader);
  if (mpegLevel <= 0) {
     DroppedData("cVideoRepacker: no valid PES packet header found", Count);
     return;
     }
  if (!continuationHeader) {
    // backup PES header
    pesHeaderBackupLen = pesPayloadOffset;
    memcpy(pesHeaderBackup, Data, pesHeaderBackupLen);
    }

  // skip PES header
  int done = pesPayloadOffset;
  int todo = Count - done;
  const uchar *data = Data + done;
  // remember start of the data
  const uchar *payload = data;

  while (todo > 0) {
        // collect number of skipped bytes while syncing
        if (state <= syncing)
           skippedBytes++;
        // did we reach a start code?
        scanner <<= 8;
        if (scanner != 0x00000100)
           scanner |= *data;
        else {
           scanner |= *data;

           // which kind of start code have we got?
           switch (*data) {
             case 0xB9 ... 0xFF: // system start codes
                  esyslog("cVideoRepacker: found system start code: stream seems to be scrambled or not demultiplexed");
                  break;
             case 0xB0 ... 0xB1: // reserved start codes
             case 0xB6:
                  esyslog("cVideoRepacker: found reserved start code: stream seems to be scrambled");
                  break;
             case 0xB4: // sequence error code
                  isyslog("cVideoRepacker: found sequence error code: stream seems to be damaged");
             case 0xB2: // user data start code
             case 0xB5: // extension start code
                  break;
             case 0xB7: // sequence end code
             case 0xB3: // sequence header code
             case 0xB8: // group start code
             case 0x00: // picture start code
                  if (state == scanPicture) {
                     // the above start codes indicate that the current picture is done. So
                     // push out the packet to start a new packet for the next picuture. If
                     // the byte count get's negative then the current buffer ends in a
                     // partitial start code that must be stripped off, as it shall be put
                     // in the next packet.
                     if (!PushOutPacket(ResultBuffer, payload, data - 3 - payload)) {
                        DroppedData("cVideoRepacker: result buffer overflow", Count - (done - 3));
                        return;
                        }
                     // go on with syncing to the next picture
                     state = syncing;
                     }
                  if (state == syncing) {
                     // report that syncing dropped some bytes
                     if (skippedBytes > SkippedBytesLimit)
                        esyslog("cVideoRepacker: skipped %d bytes to sync on next picture", skippedBytes - SkippedBytesLimit);
                     skippedBytes = 0;
                     // if there is a PES header available, then use it ...
                     if (pesHeaderBackupLen > 0) {
                        // ISO 13818-1 says:
                        // In the case of video, if a PTS is present in a PES packet header
                        // it shall refer to the access unit containing the first picture start
                        // code that commences in this PES packet. A picture start code commences
                        // in PES packet if the first byte of the picture start code is present
                        // in the PES packet.
                        memcpy(pesHeader, pesHeaderBackup, pesHeaderBackupLen);
                        pesHeaderLen = pesHeaderBackupLen;
                        pesHeaderBackupLen = 0;
                        }
                     else {
                        // ... otherwise create a continuation PES header
                        pesHeaderLen = 0;
                        pesHeader[pesHeaderLen++] = 0x00;
                        pesHeader[pesHeaderLen++] = 0x00;
                        pesHeader[pesHeaderLen++] = 0x01;
                        pesHeader[pesHeaderLen++] = Data[3]; // video stream ID
                        pesHeader[pesHeaderLen++] = 0x00; // length still unknown
                        pesHeader[pesHeaderLen++] = 0x00; // length still unknown

                        if (mpegLevel == 2) {
                           pesHeader[pesHeaderLen++] = 0x80;
                           pesHeader[pesHeaderLen++] = 0x00;
                           pesHeader[pesHeaderLen++] = 0x00;
                           }
                        else
                           pesHeader[pesHeaderLen++] = 0x0F;
                        }
                     // append the first three bytes of the start code
                     pesHeader[pesHeaderLen++] = 0x00;
                     pesHeader[pesHeaderLen++] = 0x00;
                     pesHeader[pesHeaderLen++] = 0x01;
                     // the next packet's payload will begin with the fourth byte of
                     // the start code (= the actual code)
                     payload = data;
                     // as there is no length information available, assume the
                     // maximum we can hold in one PES packet
                     packetTodo = maxPacketSize - pesHeaderLen;
                     // go on with finding the picture data
                     ((int &)state)++;
                     }
                  break;
             case 0x01 ... 0xAF: // slice start codes
                  if (state == findPicture) {
                     // go on with scanning the picture data
                     ((int &)state)++;
                     }
                  break;
             }
           }
        data++;
        done++;
        todo--;
        // do we have to start a new packet as there is no more space left?
        if (--packetTodo <= 0) {
           // we connot start a new packet here if the current might end in a start
           // code and this start code shall possibly be put in the next packet. So
           // overfill the current packet until we can safely detect that we won't
           // break a start code into pieces:
           //
           // A) the last four bytes were a start code.
           // B) the current byte introduces a start code.
           // C) the last three bytes begin a start code.
           //
           // Todo : Data                          : Rule : Result
           // -----:-------------------------------:------:-------
           //      : XX 00 00 00 01 YY|YY YY YY YY :      :
           //    0 :                ^^|            : A    : push
           // -----:-------------------------------:------:-------
           //      : XX XX 00 00 00 01|YY YY YY YY :      :
           //    0 :                ^^|            : B    : wait
           //   -1 :                  |^^          : A    : push
           // -----:-------------------------------:------:-------
           //      : XX XX XX 00 00 00|01 YY YY YY :      :
           //    0 :                ^^|            : C    : wait
           //   -1 :                  |^^          : B    : wait
           //   -2 :                  |   ^^       : A    : push
           // -----:-------------------------------:------:-------
           //      : XX XX XX XX 00 00|00 01 YY YY :      :
           //    0 :                ^^|            : C    : wait
           //   -1 :                  |^^          : C    : wait
           //   -2 :                  |   ^^       : B    : wait
           //   -3 :                  |      ^^    : A    : push
           // -----:-------------------------------:------:-------
           //      : XX XX XX XX XX 00|00 00 01 YY :      :
           //    0 :                ^^|            : C    : wait
           //   -1 :                  |^^          : C    : wait
           //   -2 :                  |   ^^       :      : push
           // -----:-------------------------------:------:-------
           bool A = ((scanner & 0xFFFFFF00) == 0x00000100);
           bool B = ((scanner &   0xFFFFFF) ==   0x000001);
           bool C = ((scanner &       0xFF) ==       0x00) && (packetTodo >= -1);
           if (A || (!B && !C)) {
              // actually we cannot push out an overfull packet. So we'll have to
              // adjust the byte count and payload start as necessary. If the byte
              // count get's negative we'll have to append the excess from fragment's
              // tail to the next PES header.
              int bite = data + packetTodo - payload;
              const uchar *excessData = fragmentData + fragmentLen + bite;
              // a negative byte count means to drop some bytes from the current
              // fragment's tail, to not exceed the maximum packet size.
              if (!PushOutPacket(ResultBuffer, payload, bite)) {
                 DroppedData("cVideoRepacker: result buffer overflow", Count - done);
                 return;
                 }
              // create a continuation PES header
              pesHeaderLen = 0;
              pesHeader[pesHeaderLen++] = 0x00;
              pesHeader[pesHeaderLen++] = 0x00;
              pesHeader[pesHeaderLen++] = 0x01;
              pesHeader[pesHeaderLen++] = Data[3]; // video stream ID
              pesHeader[pesHeaderLen++] = 0x00; // length still unknown
              pesHeader[pesHeaderLen++] = 0x00; // length still unknown

              if (mpegLevel == 2) {
                 pesHeader[pesHeaderLen++] = 0x80;
                 pesHeader[pesHeaderLen++] = 0x00;
                 pesHeader[pesHeaderLen++] = 0x00;
                 }
              else
                 pesHeader[pesHeaderLen++] = 0x0F;

              // copy any excess data
              while (bite++ < 0) {
                    // append the excess data here
                    pesHeader[pesHeaderLen++] = *excessData++;
                    packetTodo++;
                    }
              // the next packet's payload will begin here
              payload = data + packetTodo;
              // as there is no length information available, assume the
              // maximum we can hold in one PES packet
              packetTodo += maxPacketSize - pesHeaderLen;
              }
           }
        }
  // the packet is done. Now store any remaining data into fragment buffer
  // if we are no longer syncing.
  if (state != syncing) {
     // append the PES header ...
     int bite = pesHeaderLen;
     pesHeaderLen = 0;
     if (bite > 0) {
        memcpy(fragmentData + fragmentLen, pesHeader, bite);
        fragmentLen += bite;
        }
     // append payload. It may contain part of a start code at it's end,
     // which will be removed when the next packet gets processed.
     bite = data - payload;
     if (bite > 0) {
        memcpy(fragmentData + fragmentLen, payload, bite);
        fragmentLen += bite;
        }
     }
  // report that syncing dropped some bytes
  if (skippedBytes > SkippedBytesLimit) {
     esyslog("cVideoRepacker: skipped %d bytes while syncing on next picture", skippedBytes - SkippedBytesLimit);
     skippedBytes = SkippedBytesLimit;
     }
}

int cVideoRepacker::BreakAt(const uchar *Data, int Count)
{
  int PesPayloadOffset = 0;

  if (AnalyzePesHeader(Data, Count, PesPayloadOffset) <= 0)
     return -1; // not enough data for test

  // just detect end of picture
  if (state == scanPicture) {
     // setup local scanner
     if (localStart < 0) {
        localScanner = scanner;
        localStart = 0;
        }
     // start where we've stopped at the last run
     const uchar *data = Data + PesPayloadOffset + localStart;
     const uchar *limit = Data + Count;
     // scan data
     while (data < limit) {
           localStart++;
           localScanner <<= 8;
           localScanner |= *data++;
           // check start codes which follow picture data
           switch (localScanner) {
             case 0x00000100: // picture start code
             case 0x000001B8: // group start code
             case 0x000001B3: // sequence header code
             case 0x000001B7: // sequence end code
                  return data - Data;
             }
           }
     }
  // just fill up packet and append next start code
  return PesPayloadOffset + packetTodo + 4;
}

// --- cDolbyRepacker --------------------------------------------------------

class cDolbyRepacker : public cRepacker {
private:
  static int frameSizes[];
  uchar fragmentData[6 + 65535];
  int fragmentLen;
  int fragmentTodo;
  uchar pesHeader[6 + 3 + 255 + 4 + 4];
  int pesHeaderLen;
  uchar pesHeaderBackup[6 + 3 + 255];
  int pesHeaderBackupLen;
  uchar chk1;
  uchar chk2;
  int ac3todo;
  enum {
    find_0b,
    find_77,
    store_chk1,
    store_chk2,
    get_length,
    output_packet
    } state;
  int skippedBytes;
  void ResetPesHeader(bool ContinuationFrame = false);
  void AppendSubStreamID(bool ContinuationFrame = false);
  bool FinishRemainder(cRingBufferLinear *ResultBuffer, const uchar *const Data, const int Todo, int &Done, int &Bite);
  bool StartNewPacket(cRingBufferLinear *ResultBuffer, const uchar *const Data, const int Todo, int &Done, int &Bite);
public:
  cDolbyRepacker(void);
  virtual void Reset(void);
  virtual void Repack(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count);
  virtual int BreakAt(const uchar *Data, int Count);
  };

// frameSizes are in words, i. e. multiply them by 2 to get bytes
int cDolbyRepacker::frameSizes[] = {
  // fs = 48 kHz
    64,   64,   80,   80,   96,   96,  112,  112,  128,  128,  160,  160,  192,  192,  224,  224,
   256,  256,  320,  320,  384,  384,  448,  448,  512,  512,  640,  640,  768,  768,  896,  896,
  1024, 1024, 1152, 1152, 1280, 1280,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  // fs = 44.1 kHz
    69,   70,   87,   88,  104,  105,  121,  122,  139,  140,  174,  175,  208,  209,  243,  244,
   278,  279,  348,  349,  417,  418,  487,  488,  557,  558,  696,  697,  835,  836,  975,  976,
  1114, 1115, 1253, 1254, 1393, 1394,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  // fs = 32 kHz
    96,   96,  120,  120,  144,  144,  168,  168,  192,  192,  240,  240,  288,  288,  336,  336,
   384,  384,  480,  480,  576,  576,  672,  672,  768,  768,  960,  960, 1152, 1152, 1344, 1344,
  1536, 1536, 1728, 1728, 1920, 1920,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  //
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  };

cDolbyRepacker::cDolbyRepacker(void)
{
  pesHeader[0] = 0x00;
  pesHeader[1] = 0x00;
  pesHeader[2] = 0x01;
  pesHeader[3] = 0xBD;
  pesHeader[4] = 0x00;
  pesHeader[5] = 0x00;
  Reset();
}

void cDolbyRepacker::AppendSubStreamID(bool ContinuationFrame)
{
  if (subStreamId) {
     pesHeader[pesHeaderLen++] = subStreamId;
     // number of ac3 frames "starting" in this packet (1 by design).
     pesHeader[pesHeaderLen++] = 0x01;
     // offset to start of first ac3 frame (0 means "no ac3 frame starting"
     // so 1 (by design) addresses the first byte after the next two bytes).
     pesHeader[pesHeaderLen++] = 0x00;
     pesHeader[pesHeaderLen++] = (ContinuationFrame ? 0x00 : 0x01);
     }
}

void cDolbyRepacker::ResetPesHeader(bool ContinuationFrame)
{
  pesHeader[6] = 0x80;
  pesHeader[7] = 0x00;
  pesHeader[8] = 0x00;
  pesHeaderLen = 9;
  AppendSubStreamID(ContinuationFrame);
}

void cDolbyRepacker::Reset(void)
{
  ResetPesHeader();
  state = find_0b;
  ac3todo = 0;
  chk1 = 0;
  chk2 = 0;
  fragmentLen = 0;
  fragmentTodo = 0;
  pesHeaderBackupLen = 0;
  skippedBytes = 0;
}

bool cDolbyRepacker::FinishRemainder(cRingBufferLinear *ResultBuffer, const uchar *const Data, const int Todo, int &Done, int &Bite)
{
  // enough data available to put PES packet into buffer?
  if (fragmentTodo <= Todo) {
     // output a previous fragment first
     if (fragmentLen > 0) {
        Bite = fragmentLen;
        int n = Put(ResultBuffer, fragmentData, Bite);
        if (Bite != n) {
           Reset();
           return false;
           }
        fragmentLen = 0;
        }
     Bite = fragmentTodo;
     int n = Put(ResultBuffer, Data, Bite);
     if (Bite != n) {
        Reset();
        Done += n;
        return false;
        }
     fragmentTodo = 0;
     // ac3 frame completely processed?
     if (Bite >= ac3todo)
        state = find_0b; // go on with finding start of next packet
     }
  else {
     // copy the fragment into separate buffer for later processing
     Bite = Todo;
     if (fragmentLen + Bite > (int)sizeof(fragmentData)) {
        Reset();
        return false;
        }
     memcpy(fragmentData + fragmentLen, Data, Bite);
     fragmentLen += Bite;
     fragmentTodo -= Bite;
     }
  return true;
}

bool cDolbyRepacker::StartNewPacket(cRingBufferLinear *ResultBuffer, const uchar *const Data, const int Todo, int &Done, int &Bite)
{
  int packetLen = pesHeaderLen + ac3todo;
  // limit packet to maximum size
  if (packetLen > maxPacketSize)
     packetLen = maxPacketSize;
  pesHeader[4] = (packetLen - 6) >> 8;
  pesHeader[5] = (packetLen - 6) & 0xFF;
  Bite = pesHeaderLen;
  // enough data available to put PES packet into buffer?
  if (packetLen - pesHeaderLen <= Todo) {
     int n = Put(ResultBuffer, pesHeader, Bite);
     if (Bite != n) {
        Reset();
        return false;
        }
     Bite = packetLen - pesHeaderLen;
     n = Put(ResultBuffer, Data, Bite);
     if (Bite != n) {
        Reset();
        Done += n;
        return false;
        }
     // ac3 frame completely processed?
     if (Bite >= ac3todo)
        state = find_0b; // go on with finding start of next packet
     }
  else {
     fragmentTodo = packetLen;
     // copy the pesheader into separate buffer for later processing
     if (fragmentLen + Bite > (int)sizeof(fragmentData)) {
        Reset();
        return false;
        }
     memcpy(fragmentData + fragmentLen, pesHeader, Bite);
     fragmentLen += Bite;
     fragmentTodo -= Bite;
     // copy the fragment into separate buffer for later processing
     Bite = Todo;
     if (fragmentLen + Bite > (int)sizeof(fragmentData)) {
        Reset();
        return false;
        }
     memcpy(fragmentData + fragmentLen, Data, Bite);
     fragmentLen += Bite;
     fragmentTodo -= Bite;
     }
  return true;
}

void cDolbyRepacker::Repack(cRingBufferLinear *ResultBuffer, const uchar *Data, int Count)
{
  // synchronisation is detected some bytes after frame start.
  const int SkippedBytesLimit = 4;

  // check for MPEG 2
  if ((Data[6] & 0xC0) != 0x80) {
     DroppedData("cDolbyRepacker: MPEG 2 PES header expected", Count);
     return;
     }

  // backup PES header
  if (Data[6] != 0x80 || Data[7] != 0x00 || Data[8] != 0x00) {
     pesHeaderBackupLen = 6 + 3 + Data[8];
     memcpy(pesHeaderBackup, Data, pesHeaderBackupLen);
     }

  // skip PES header
  int done = 6 + 3 + Data[8];
  int todo = Count - done;
  const uchar *data = Data + done;

  // look for 0x0B 0x77 <chk1> <chk2> <frameSize>
  while (todo > 0) {
        switch (state) {
          case find_0b:
               if (*data == 0x0B) {
                  ++(int &)state;
                  // copy header information once for later use
                  if (pesHeaderBackupLen > 0) {
                     pesHeaderLen = pesHeaderBackupLen;
                     pesHeaderBackupLen = 0;
                     memcpy(pesHeader, pesHeaderBackup, pesHeaderLen);
                     AppendSubStreamID();
                     }
                  }
               data++;
               done++;
               todo--;
               skippedBytes++; // collect number of skipped bytes while syncing
               continue;
          case find_77:
               if (*data != 0x77) {
                  state = find_0b;
                  continue;
                  }
               data++;
               done++;
               todo--;
               skippedBytes++; // collect number of skipped bytes while syncing
               ++(int &)state;
               continue;
          case store_chk1:
               chk1 = *data++;
               done++;
               todo--;
               skippedBytes++; // collect number of skipped bytes while syncing
               ++(int &)state;
               continue;
          case store_chk2:
               chk2 = *data++;
               done++;
               todo--;
               skippedBytes++; // collect number of skipped bytes while syncing
               ++(int &)state;
               continue;
          case get_length:
               ac3todo = 2 * frameSizes[*data];
               // frameSizeCode was invalid => restart searching
               if (ac3todo <= 0) {
                  // reset PES header instead of using a wrong one
                  ResetPesHeader();
                  if (chk1 == 0x0B) {
                     if (chk2 == 0x77) {
                        state = store_chk1;
                        continue;
                        }
                     if (chk2 == 0x0B) {
                        state = find_77;
                        continue;
                        }
                     state = find_0b;
                     continue;
                     }
                  if (chk2 == 0x0B) {
                     state = find_77;
                     continue;
                     }
                  state = find_0b;
                  continue;
                  }
               // report that syncing dropped some bytes
               if (skippedBytes > SkippedBytesLimit)
                  esyslog("cDolbyRepacker: skipped %d bytes to sync on next AC3 frame", skippedBytes - SkippedBytesLimit);
               skippedBytes = 0;
               // append read data to header for common output processing
               pesHeader[pesHeaderLen++] = 0x0B;
               pesHeader[pesHeaderLen++] = 0x77;
               pesHeader[pesHeaderLen++] = chk1;
               pesHeader[pesHeaderLen++] = chk2;
               ac3todo -= 4;
               ++(int &)state;
               // fall through to output
          case output_packet: {
               int bite = 0;
               // finish remainder of ac3 frame?
               if (fragmentTodo > 0) {
                  if (!FinishRemainder(ResultBuffer, data, todo, done, bite)) {
                     DroppedData("cDolbyRepacker: result buffer overflow", Count - done);
                     return;
                     }
                  }
               else {
                  // start a new packet
                  if (!StartNewPacket(ResultBuffer, data, todo, done, bite)) {
                     DroppedData("cDolbyRepacker: result buffer overflow", Count - done);
                     return;
                     }
                  // prepare for next (continuation) packet
                  ResetPesHeader(state == output_packet);
                  }
               data += bite;
               done += bite;
               todo -= bite;
               ac3todo -= bite;
               }
          }
        }
  // report that syncing dropped some bytes
  if (skippedBytes > SkippedBytesLimit) {
     esyslog("cDolbyRepacker: skipped %d bytes while syncing on next AC3 frame", skippedBytes - 4);
     skippedBytes = SkippedBytesLimit;
     }
}

int cDolbyRepacker::BreakAt(const uchar *Data, int Count)
{
  // enough data for test?
  if (Count < 6 + 3)
     return -1;
  // check for MPEG 2
  if ((Data[6] & 0xC0) != 0x80)
     return -1;
  int headerLen = Data[8] + 6 + 3;
  // break after fragment tail?
  if (ac3todo > 0)
     return headerLen + ac3todo;
  // enough data for test?
  if (Count < headerLen + 5)
     return -1;
  const uchar *data = Data + headerLen;
  // break after ac3 frame?
  if (data[0] == 0x0B && data[1] == 0x77 && frameSizes[data[4]] > 0)
     return headerLen + 2 * frameSizes[data[4]];
  return -1;
}

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
#define MMAX_PLENGTH (64*MAX_PLENGTH) // some stations send PES packets that are extremely large, e.g. DVB-T in Finland or HDTV 1920x1080

#define IPACKS 2048

// Start codes:
#define SC_PICTURE 0x00  // "picture header"

#define MAXNONUSEFULDATA (10*1024*1024)
#define MAXNUMUPTERRORS  10

class cTS2PES {
private:
  int pid;
  int size;
  int found;
  int count;
  uint8_t *buf;
  uint8_t cid;
  uint8_t audioCid;
  uint8_t subStreamId;
  int plength;
  uint8_t plen[2];
  uint8_t flag1;
  uint8_t flag2;
  uint8_t hlength;
  int mpeg;
  uint8_t check;
  int which;
  bool done;
  cRingBufferLinear *resultBuffer;
  int tsErrors;
  int ccErrors;
  int ccCounter;
  cRepacker *repacker;
  static uint8_t headr[];
  void store(uint8_t *Data, int Count);
  void reset_ipack(void);
  void send_ipack(void);
  void write_ipack(const uint8_t *Data, int Count);
  void instant_repack(const uint8_t *Buf, int Count);
public:
  cTS2PES(int Pid, cRingBufferLinear *ResultBuffer, int Size, uint8_t AudioCid = 0x00, uint8_t SubStreamId = 0x00, cRepacker *Repacker = NULL);
  ~cTS2PES();
  int Pid(void) { return pid; }
  void ts_to_pes(const uint8_t *Buf); // don't need count (=188)
  void Clear(void);
  };

uint8_t cTS2PES::headr[] = { 0x00, 0x00, 0x01 };

cTS2PES::cTS2PES(int Pid, cRingBufferLinear *ResultBuffer, int Size, uint8_t AudioCid, uint8_t SubStreamId, cRepacker *Repacker)
{
  pid = Pid;
  resultBuffer = ResultBuffer;
  size = Size;
  audioCid = AudioCid;
  subStreamId = SubStreamId;
  repacker = Repacker;
  if (repacker) {
     repacker->SetMaxPacketSize(size);
     repacker->SetSubStreamId(subStreamId);
     size += repacker->QuerySnoopSize();
     }

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
  delete repacker;
}

void cTS2PES::Clear(void)
{
  reset_ipack();
  if (repacker)
     repacker->Reset();
}

void cTS2PES::store(uint8_t *Data, int Count)
{
  if (repacker)
     repacker->Repack(resultBuffer, Data, Count);
  else {
     int n = resultBuffer->Put(Data, Count);
     if (n != Count)
        esyslog("ERROR: result buffer overflow, dropped %d out of %d byte", Count - n, Count);
     }
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
  if (count <= ((mpeg == 2) ? 9 : 7)) // skip empty packets
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

  // determine amount of data to process
  int bite = Count;
  if (count + bite > size)
     bite = size - count;
  if (repacker) {
     int breakAt = repacker->BreakAt(buf, count);
     // avoid memcpy of data after break location
     if (0 <= breakAt && breakAt < count + bite) {
        bite = breakAt - count;
        if (bite < 0) // should never happen
           bite = 0;
        }
     }

  memcpy(buf + count, Data, bite);
  count += bite;

  if (repacker) {
     // determine break location
     int breakAt = repacker->BreakAt(buf, count);
     if (breakAt > size) // won't fit into packet?
        breakAt = -1;
     if (breakAt > count) // not enough data?
        breakAt = -1;
     // push out data before break location
     if (breakAt > 0) {
        // adjust bite if above memcpy was to large
        bite -= count - breakAt;
        count = breakAt;
        send_ipack();
        // recurse for data after break location
        if (Count - bite > 0)
           write_ipack(Data + bite, Count - bite);
        }
     }

  // push out data when buffer is full
  if (count >= size) {
     send_ipack();
     // recurse for remaining data
     if (Count - bite > 0)
        write_ipack(Data + bite, Count - bite);
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

#define RESULTBUFFERSIZE KILOBYTE(256)

cRemux::cRemux(int VPid, const int *APids, const int *DPids, const int *SPids, bool ExitOnFailure)
{
  exitOnFailure = ExitOnFailure;
  isRadio = VPid == 0 || VPid == 1 || VPid == 0x1FFF;
  numUPTerrors = 0;
  synced = false;
  skipped = 0;
  numTracks = 0;
  resultSkipped = 0;
  resultBuffer = new cRingBufferLinear(RESULTBUFFERSIZE, IPACKS, false, "Result");
  resultBuffer->SetTimeouts(0, 100);
  if (VPid)
#define TEST_cVideoRepacker
#ifdef TEST_cVideoRepacker
     ts2pes[numTracks++] = new cTS2PES(VPid, resultBuffer, IPACKS, 0x00, 0x00, new cVideoRepacker);
#else
     ts2pes[numTracks++] = new cTS2PES(VPid, resultBuffer, IPACKS);
#endif
  if (APids) {
     int n = 0;
     while (*APids && numTracks < MAXTRACKS && n < MAXAPIDS)
           ts2pes[numTracks++] = new cTS2PES(*APids++, resultBuffer, IPACKS, 0xC0 + n++);
     }
  if (DPids) {
     int n = 0;
     while (*DPids && numTracks < MAXTRACKS && n < MAXDPIDS)
           ts2pes[numTracks++] = new cTS2PES(*DPids++, resultBuffer, IPACKS, 0x00, 0x80 + n++, new cDolbyRepacker);
     }
  /* future...
  if (SPids) {
     int n = 0;
     while (*SPids && numTracks < MAXTRACKS && n < MAXSPIDS)
           ts2pes[numTracks++] = new cTS2PES(*SPids++, resultBuffer, IPACKS, 0x00, 0x28 + n++);
     }
  */
}

cRemux::~cRemux()
{
  for (int t = 0; t < numTracks; t++)
      delete ts2pes[t];
  delete resultBuffer;
}

int cRemux::GetPid(const uchar *Data)
{
  return (((uint16_t)Data[0] & PID_MASK_HI) << 8) | (Data[1] & 0xFF);
}

int cRemux::GetPacketLength(const uchar *Data, int Count, int Offset)
{
  // Returns the length of the packet starting at Offset, or -1 if Count is
  // too small to contain the entire packet.
  int Length = (Offset + 5 < Count) ? (Data[Offset + 4] << 8) + Data[Offset + 5] + 6 : -1;
  if (Length > 0 && Offset + Length <= Count)
     return Length;
  return -1;
}

int cRemux::ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType)
{
  // Scans the video packet starting at Offset and returns its length.
  // If the return value is -1 the packet was not completely in the buffer.
  int Length = GetPacketLength(Data, Count, Offset);
  if (Length > 0) {
     if (Length >= 8) {
        int i = Offset + 8; // the minimum length of the video packet header
        i += Data[i] + 1;   // possible additional header bytes
        for (; i < Offset + Length - 5; i++) {
            if (Data[i] == 0 && Data[i + 1] == 0 && Data[i + 2] == 1) {
               switch (Data[i + 3]) {
                 case SC_PICTURE: PictureType = (Data[i + 5] >> 3) & 0x07;
                                  return Length;
                 }
               }
            }
        }
     PictureType = NO_PICTURE;
     return Length;
     }
  return -1;
}

#define TS_SYNC_BYTE 0x47

int cRemux::Put(const uchar *Data, int Count)
{
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
      if (resultBuffer->Free() < 2 * IPACKS)
         break; // A cTS2PES might write one full packet and also a small rest
      int pid = GetPid(Data + i + 1);
      if (Data[i + 3] & 0x10) { // got payload
         for (int t = 0; t < numTracks; t++) {
             if (ts2pes[t]->Pid() == pid) {
                ts2pes[t]->ts_to_pes(Data + i);
                break;
                }
             }
         }
      used += TS_SIZE;
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
        skipped += used;
     }

  return used;
}

uchar *cRemux::Get(int &Count, uchar *PictureType)
{
  // Remove any previously skipped data from the result buffer:

  if (resultSkipped > 0) {
     resultBuffer->Del(resultSkipped);
     resultSkipped = 0;
     }

#if 0
  // Test recording without determining the real frame borders:
  if (PictureType)
     *PictureType = I_FRAME;
  return resultBuffer->Get(Count);
#endif

  // Special VPID case to enable recording radio channels:

  if (isRadio) {
     // XXX actually '0' should be enough, but '1' must be used with encrypted channels (driver bug?)
     // XXX also allowing 0x1FFF to not break Michael Paar's original patch,
     // XXX but it would probably be best to only use '0'
     // Force syncing of radio channels to avoid "no useful data" error
     synced = true;
     if (PictureType)
        *PictureType = I_FRAME;
     return resultBuffer->Get(Count);
     }

  // Check for frame borders:

  if (PictureType)
     *PictureType = NO_PICTURE;

  Count = 0;
  uchar *resultData = NULL;
  int resultCount = 0;
  uchar *data = resultBuffer->Get(resultCount);
  if (data) {
     for (int i = 0; i < resultCount - 3; i++) {
         if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            int l = 0;
            uchar StreamType = data[i + 3];
            if (VIDEO_STREAM_S <= StreamType && StreamType <= VIDEO_STREAM_E) {
               uchar pt = NO_PICTURE;
               l = ScanVideoPacket(data, resultCount, i, pt);
               if (l < 0)
                  return resultData;
               if (pt != NO_PICTURE) {
                  if (pt < I_FRAME || B_FRAME < pt) {
                     esyslog("ERROR: unknown picture type '%d'", pt);
                     if (++numUPTerrors > MAXNUMUPTERRORS && exitOnFailure)
                        cThread::EmergencyExit(true);
                     }
                  else if (!synced) {
                     if (pt == I_FRAME) {
                        if (PictureType)
                           *PictureType = pt;
                        resultSkipped = i; // will drop everything before this position
                        SetBrokenLink(data + i, l);
                        synced = true;
                        }
                     }
                  else if (Count)
                     return resultData;
                  else if (PictureType)
                     *PictureType = pt;
                  }
               }
            else { //if (AUDIO_STREAM_S <= StreamType && StreamType <= AUDIO_STREAM_E || StreamType == PRIVATE_STREAM1) {
               l = GetPacketLength(data, resultCount, i);
               if (l < 0)
                  return resultData;
               }
            if (synced) {
               if (!Count)
                  resultData = data + i;
               Count += l;
               }
            else
               resultSkipped = i + l;
            if (l > 0)
               i += l - 1; // the loop increments, too
            }
         }
     }
  return resultData;
}

void cRemux::Del(int Count)
{
  resultBuffer->Del(Count);
}

void cRemux::Clear(void)
{
  for (int t = 0; t < numTracks; t++)
      ts2pes[t]->Clear();
  resultBuffer->Clear();
  synced = false;
  skipped = 0;
  resultSkipped = 0;
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
