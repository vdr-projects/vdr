/*
 * remux.h: A streaming MPEG2 remultiplexer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 2.2 2008/09/06 14:48:28 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

#include "channels.h"
#include "ringbuffer.h"
#include "tools.h"

enum ePesHeader {
  phNeedMoreData = -1,
  phInvalid = 0,
  phMPEG1 = 1,
  phMPEG2 = 2
  };

ePesHeader AnalyzePesHeader(const uchar *Data, int Count, int &PesPayloadOffset, bool *ContinuationHeader = NULL);

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

#define MAXTRACKS 64

class cTS2PES;

class cRemux {
private:
  bool exitOnFailure;
  bool noVideo;
  int numUPTerrors;
  bool synced;
  int skipped;
  cTS2PES *ts2pes[MAXTRACKS];
  int numTracks;
  cRingBufferLinear *resultBuffer;
  int resultSkipped;
  int GetPid(const uchar *Data);
public:
  cRemux(int VPid, const int *APids, const int *DPids, const int *SPids, bool ExitOnFailure = false);
       ///< Creates a new remuxer for the given PIDs. VPid is the video PID, while
       ///< APids, DPids and SPids are pointers to zero terminated lists of audio,
       ///< dolby and subtitle PIDs (the pointers may be NULL if there is no such
       ///< PID). If ExitOnFailure is true, the remuxer will initiate an "emergency
       ///< exit" in case of problems with the data stream.
  ~cRemux();
  void SetTimeouts(int PutTimeout, int GetTimeout) { resultBuffer->SetTimeouts(PutTimeout, GetTimeout); }
       ///< By default cRemux assumes that Put() and Get() are called from different
       ///< threads, and uses a timeout in the Get() function in case there is no
       ///< data available. SetTimeouts() can be used to modify these timeouts.
       ///< Especially if Put() and Get() are called from the same thread, setting
       ///< both timeouts to 0 is recommended.
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
  static int GetPacketLength(const uchar *Data, int Count, int Offset);
  static int ScanVideoPacket(const uchar *Data, int Count, int Offset, uchar &PictureType);
  };

// Some TS handling tools.
// The following functions all take a pointer to one complete TS packet.

#define TS_SYNC_BYTE          0x47
#define TS_SIZE               188
#define TS_ADAPT_FIELD_EXISTS 0x20
#define TS_PAYLOAD_EXISTS     0x10
#define TS_CONT_CNT_MASK      0x0F
#define TS_PAYLOAD_START      0x40
#define TS_ERROR              0x80
#define TS_PID_MASK_HI        0x1F

inline int TsHasPayload(const uchar *p)
{
  return p[3] & TS_PAYLOAD_EXISTS;
}

inline int TsPayloadStart(const uchar *p)
{
  return p[1] & TS_PAYLOAD_START;
}

inline int TsError(const uchar *p)
{
  return p[1] & TS_ERROR;
}

inline int TsPid(const uchar *p)
{
  return (p[1] & TS_PID_MASK_HI) * 256 + p[2];
}

inline int TsPayloadOffset(const uchar *p)
{
  return (p[3] & TS_ADAPT_FIELD_EXISTS) ? p[4] + 5 : 4;
}

inline int TsGetPayload(const uchar **p)
{
  int o = TsPayloadOffset(*p);
  *p += o;
  return TS_SIZE - o;
}

inline int TsContinuityCounter(const uchar *p)
{
  return p[3] & TS_CONT_CNT_MASK;
}

// Some PES handling tools:
// The following functions that take a pointer to PES data all assume that
// there is enough data so that PesLongEnough() returns true.

inline bool PesLongEnough(int Length)
{
  return Length >= 6;
}

inline int PesLength(const uchar *p)
{
  return 6 + p[4] * 256 + p[5];
}

inline int PesPayloadOffset(const uchar *p)
{
  return 9 + p[8];
}

inline int64_t PesGetPts(const uchar *p)
{
  if ((p[7] & 0x80) && p[8] >= 5) {
     return ((((int64_t)p[ 9]) & 0x0E) << 29) |
            (( (int64_t)p[10])         << 22) |
            ((((int64_t)p[11]) & 0xFE) << 14) |
            (( (int64_t)p[12])         <<  7) |
            ((((int64_t)p[13]) & 0xFE) >>  1);
     }
  return 0;
}

// PAT/PMT Generator:

#define MAX_SECTION_SIZE 4096 // maximum size of an SI section
#define MAX_PMT_TS  (MAX_SECTION_SIZE / TS_SIZE + 1)

class cPatPmtGenerator {
private:
  uchar pat[TS_SIZE]; // the PAT always fits into a single TS packet
  uchar pmt[MAX_PMT_TS][TS_SIZE]; // the PMT may well extend over several TS packets
  int numPmtPackets;
  int patCounter;
  int pmtCounter;
  int patVersion;
  int pmtVersion;
  uchar *esInfoLength;
  void IncCounter(int &Counter, uchar *TsPacket);
  void IncVersion(int &Version);
  void IncEsInfoLength(int Length);
protected:
  int MakeStream(uchar *Target, uchar Type, int Pid);
  int MakeAC3Descriptor(uchar *Target);
  int MakeSubtitlingDescriptor(uchar *Target, const char *Language);
  int MakeLanguageDescriptor(uchar *Target, const char *Language);
  int MakeCRC(uchar *Target, const uchar *Data, int Length);
public:
  cPatPmtGenerator(void);
  void GeneratePat(void);
       ///< Generates a PAT section for later use with GetPat().
       ///< This function is called by default from the constructor.
  void GeneratePmt(tChannelID ChannelID);
       ///< Generates a PMT section for the given ChannelId, for later use
       ///< with GetPmt().
  uchar *GetPat(void);
       ///< Returns a pointer to the PAT section, which consist of exactly
       ///< one TS packet.
  uchar *GetPmt(int &Index);
       ///< Returns a pointer to the Index'th TS packet of the PMT section.
       ///< Index must be initialized to 0 and will be incremented by each
       ///< call to GetPmt(). Returns NULL is all packets of the PMT section
       ///< have been fetched..
  };

// PAT/PMT Parser:

class cPatPmtParser {
private:
  uchar pmt[MAX_SECTION_SIZE];
  int pmtSize;
  int pmtPid;
  int vpid;
  int vtype;
protected:
  int SectionLength(const uchar *Data, int Length) { return (Length >= 3) ? ((int(Data[1]) & 0x0F) << 8)| Data[2] : 0; }
public:
  cPatPmtParser(void);
  void ParsePat(const uchar *Data, int Length);
       ///< Parses the given PAT Data, which is the payload of a single TS packet
       ///< from the PAT stream. The PAT may consist only of a single TS packet.
  void ParsePmt(const uchar *Data, int Length);
       ///< Parses the given PMT Data, which is the payload of a single TS packet
       ///< from the PMT stream. The PMT may consist of several TS packets, which
       ///< are delivered to the parser through several subsequent calls to
       ///< ParsePmt(). The whole PMT data will be processed once the last packet
       ///< has been received.
  int PmtPid(void) { return pmtPid; }
       ///< Returns the PMT pid as defined by the current PAT.
       ///< If no PAT has been received yet, -1 will be returned.
  int Vpid(void) { return vpid; }
       ///< Returns the video pid as defined by the current PMT.
  int Vtype(void) { return vtype; }
  };

// TS to PES converter:
// Puts together the payload of several TS packets that form one PES
// packet.

class cTsToPes {
private:
  uchar *data;
  int size;
  int length;
  bool synced;
public:
  cTsToPes(void);
  ~cTsToPes();
  void PutTs(const uchar *Data, int Length);
       ///< Puts the payload data of the single TS packet at Data into the converter.
       ///< Length is always 188.
       ///< If the given TS packet starts a new PES payload packet, the converter
       ///< will be automatically reset. Any packets before the first one that starts
       ///< a new PES payload packet will be ignored.
  const uchar *GetPes(int &Length);
       ///< Gets a pointer to the complete PES packet, or NULL if the packet
       ///< is not complete yet. If the packet is complete, Length will contain
       ///< the total packet length. The returned pointer is only valid until
       ///< the next call to PutTs() or Reset(), or until this object is destroyed.
  void Reset(void);
       ///< Resets the converter. This needs to be called after a PES packet has
       ///< been fetched by a call to GetPes(), and before the next call to
       ///< PutTs().
  };

// Some helper functions for debugging:

void BlockDump(const char *Name, const u_char *Data, int Length);
void TsDump(const char *Name, const u_char *Data, int Length);
void PesDump(const char *Name, const u_char *Data, int Length);

#endif // __REMUX_H
