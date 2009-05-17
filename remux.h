/*
 * remux.h: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 2.13 2009/05/17 09:52:56 kls Exp $
 */

#ifndef __REMUX_H
#define __REMUX_H

#include "channels.h"
#include "tools.h"

enum ePesHeader {
  phNeedMoreData = -1,
  phInvalid = 0,
  phMPEG1 = 1,
  phMPEG2 = 2
  };

ePesHeader AnalyzePesHeader(const uchar *Data, int Count, int &PesPayloadOffset, bool *ContinuationHeader = NULL);

class cRemux {
public:
  static void SetBrokenLink(uchar *Data, int Length);
  };

// Some TS handling tools.
// The following functions all take a pointer to one complete TS packet.

#define TS_SYNC_BYTE          0x47
#define TS_SIZE               188
#define TS_ERROR              0x80
#define TS_PAYLOAD_START      0x40
#define TS_TRANSPORT_PRIORITY 0x20
#define TS_PID_MASK_HI        0x1F
#define TS_SCRAMBLING_CONTROL 0xC0
#define TS_ADAPT_FIELD_EXISTS 0x20
#define TS_PAYLOAD_EXISTS     0x10
#define TS_CONT_CNT_MASK      0x0F
#define TS_ADAPT_DISCONT      0x80
#define TS_ADAPT_RANDOM_ACC   0x40 // would be perfect for detecting independent frames, but unfortunately not used by all broadcasters
#define TS_ADAPT_ELEM_PRIO    0x20
#define TS_ADAPT_PCR          0x10
#define TS_ADAPT_OPCR         0x08
#define TS_ADAPT_SPLICING     0x04
#define TS_ADAPT_TP_PRIVATE   0x02
#define TS_ADAPT_EXTENSION    0x01

#define MAXPID 0x2000 // for arrays that use a PID as the index

inline bool TsHasPayload(const uchar *p)
{
  return p[3] & TS_PAYLOAD_EXISTS;
}

inline bool TsHasAdaptationField(const uchar *p)
{
  return p[3] & TS_ADAPT_FIELD_EXISTS;
}

inline bool TsPayloadStart(const uchar *p)
{
  return p[1] & TS_PAYLOAD_START;
}

inline bool TsError(const uchar *p)
{
  return p[1] & TS_ERROR;
}

inline int TsPid(const uchar *p)
{
  return (p[1] & TS_PID_MASK_HI) * 256 + p[2];
}

inline bool TsIsScrambled(const uchar *p)
{
  return p[3] & TS_SCRAMBLING_CONTROL;
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

inline int TsGetAdaptationField(const uchar *p)
{
  return TsHasAdaptationField(p) ? p[5] : 0x00;
}

// The following functions all take a pointer to a sequence of complete TS packets.

int64_t TsGetPts(const uchar *p, int l);
void TsSetTeiOnBrokenPackets(uchar *p, int l);

// Some PES handling tools:
// The following functions that take a pointer to PES data all assume that
// there is enough data so that PesLongEnough() returns true.

inline bool PesLongEnough(int Length)
{
  return Length >= 6;
}

inline bool PesHasLength(const uchar *p)
{
  return p[4] | p[5];
}

inline int PesLength(const uchar *p)
{
  return 6 + p[4] * 256 + p[5];
}

inline int PesPayloadOffset(const uchar *p)
{
  return 9 + p[8];
}

inline bool PesHasPts(const uchar *p)
{
  return (p[7] & 0x80) && p[8] >= 5;
}

inline int64_t PesGetPts(const uchar *p)
{
  return ((((int64_t)p[ 9]) & 0x0E) << 29) |
         (( (int64_t)p[10])         << 22) |
         ((((int64_t)p[11]) & 0xFE) << 14) |
         (( (int64_t)p[12])         <<  7) |
         ((((int64_t)p[13]) & 0xFE) >>  1);
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
  int pmtPid;
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
  void GeneratePmtPid(cChannel *Channel);
       ///< Generates a PMT pid that doesn't collide with any of the actual
       ///< pids of the Channel.
  void GeneratePat(void);
       ///< Generates a PAT section for later use with GetPat().
  void GeneratePmt(cChannel *Channel);
       ///< Generates a PMT section for the given Channel, for later use
       ///< with GetPmt().
public:
  cPatPmtGenerator(cChannel *Channel = NULL);
  void SetChannel(cChannel *Channel);
       ///< Sets the Channel for which the PAT/PMT shall be generated.
  uchar *GetPat(void);
       ///< Returns a pointer to the PAT section, which consists of exactly
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
  int patVersion;
  int pmtVersion;
  int pmtPid;
  int vpid;
  int vtype;
protected:
  int SectionLength(const uchar *Data, int Length) { return (Length >= 3) ? ((int(Data[1]) & 0x0F) << 8)| Data[2] : 0; }
public:
  cPatPmtParser(void);
  void Reset(void);
       ///< Resets the parser. This function must be called whenever a new
       ///< stream is parsed.
  void ParsePat(const uchar *Data, int Length);
       ///< Parses the PAT data from the single TS packet in Data.
       ///< Length is always TS_SIZE.
  void ParsePmt(const uchar *Data, int Length);
       ///< Parses the PMT data from the single TS packet in Data.
       ///< Length is always TS_SIZE.
       ///< The PMT may consist of several TS packets, which
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
  int offset;
public:
  cTsToPes(void);
  ~cTsToPes();
  void PutTs(const uchar *Data, int Length);
       ///< Puts the payload data of the single TS packet at Data into the converter.
       ///< Length is always 188.
       ///< If the given TS packet starts a new PES payload packet, the converter
       ///< will be automatically reset. Any packets before the first one that starts
       ///< a new PES payload packet will be ignored.
       ///< Once a TS packet has been put into a cTsToPes converter, all subsequent
       ///< packets until the next call to Reset() must belong to the same PID as
       ///< the first packet. There is no check whether this actually is the case, so
       ///< the caller is responsible for making sure this condition is met.
  const uchar *GetPes(int &Length);
       ///< Gets a pointer to the complete PES packet, or NULL if the packet
       ///< is not complete yet. If the packet is complete, Length will contain
       ///< the total packet length. The returned pointer is only valid until
       ///< the next call to PutTs() or Reset(), or until this object is destroyed.
       ///< Once GetPes() has returned a non-NULL value, it must be called
       ///< repeatedly, and the data processed, until it returns NULL. This
       ///< is because video packets may be larger than the data a single
       ///< PES packet with an actual length field can hold, and are therefore
       ///< split into several PES packates with smaller sizes.
  void Reset(void);
       ///< Resets the converter. This needs to be called after a PES packet has
       ///< been fetched by a call to GetPes(), and before the next call to
       ///< PutTs().
  };

// Some helper functions for debugging:

void BlockDump(const char *Name, const u_char *Data, int Length);
void TsDump(const char *Name, const u_char *Data, int Length);
void PesDump(const char *Name, const u_char *Data, int Length);

// Frame detector:

class cFrameDetector {
private:
  enum { MaxPtsValues = 150 };
  int pid;
  int type;
  bool synced;
  bool newFrame;
  bool independentFrame;
  uint32_t ptsValues[MaxPtsValues]; // 32 bit is enough - we only need the delta
  int numPtsValues;
  int numIFrames;
  bool isVideo;
  int frameDuration;
  int framesInPayloadUnit;
  int framesPerPayloadUnit; // Some broadcasters send one frame per payload unit (== 1),
                            // some put an entire GOP into one payload unit (> 1), and
                            // some spread a single frame over several payload units (< 0).
  int payloadUnitOfFrame;
  bool scanning;
  uint32_t scanner;
public:
  cFrameDetector(int Pid, int Type);
  int Analyze(const uchar *Data, int Length);
      ///< Analyzes the TS packets pointed to by Data. Length is the number of
      ///< bytes Data points to, and must be a multiple of 188.
      ///< Returns the number of bytes that have been analyzed.
      ///< If the return value is 0, the data was not sufficient for analyzing and
      ///< Analyze() needs to be called again with more actual data.
  bool Synced(void) { return synced; }
      ///< Returns true if the frame detector has synced on the data stream.
  bool NewFrame(void) { return newFrame; }
      ///< Returns true if the data given to the last call to Analyze() started a
      ///< new frame.
  bool IndependentFrame(void) { return independentFrame; }
      ///< Returns true if a new frame was detected and this is an independent frame
      ///< (i.e. one that can be displayed by itself, without using data from any
      ///< other frames).
  double FramesPerSecond(void) { return frameDuration ? 90000.0 / frameDuration : 0; }
      ///< Returns the number of frames per second, or 0 if this information is not
      ///< available.
  };

#endif // __REMUX_H
