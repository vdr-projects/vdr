/*
 * remux.h: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 2.32 2011/09/04 12:48:26 kls Exp $
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

#define PATPID 0x0000 // PAT PID (constant 0)
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
  int o = TsHasAdaptationField(p) ? p[4] + 5 : 4;
  return o <= TS_SIZE ? o : TS_SIZE;
}

inline int TsGetPayload(const uchar **p)
{
  if (TsHasPayload(*p)) {
     int o = TsPayloadOffset(*p);
     *p += o;
     return TS_SIZE - o;
     }
  return 0;
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
  int MakeAC3Descriptor(uchar *Target, uchar Type);
  int MakeSubtitlingDescriptor(uchar *Target, const char *Language, uchar SubtitlingType, uint16_t CompositionPageId, uint16_t AncillaryPageId);
  int MakeLanguageDescriptor(uchar *Target, const char *Language);
  int MakeCRC(uchar *Target, const uchar *Data, int Length);
  void GeneratePmtPid(const cChannel *Channel);
       ///< Generates a PMT pid that doesn't collide with any of the actual
       ///< pids of the Channel.
  void GeneratePat(void);
       ///< Generates a PAT section for later use with GetPat().
  void GeneratePmt(const cChannel *Channel);
       ///< Generates a PMT section for the given Channel, for later use
       ///< with GetPmt().
public:
  cPatPmtGenerator(const cChannel *Channel = NULL);
  void SetVersions(int PatVersion, int PmtVersion);
       ///< Sets the version numbers for the generated PAT and PMT, in case
       ///< this generator is used to, e.g.,  continue a previously interrupted
       ///< recording (in which case the numbers given should be derived from
       ///< the PAT/PMT versions last used in the existing recording, incremented
       ///< by 1. If the given numbers exceed the allowed range of 0..31, the
       ///< higher bits will automatically be cleared.
       ///< SetVersions() needs to be called before SetChannel() in order to
       ///< have an effect from the very start.
  void SetChannel(const cChannel *Channel);
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
  int ppid;
  int vtype;
  int apids[MAXAPIDS + 1]; // list is zero-terminated
  int atypes[MAXAPIDS + 1]; // list is zero-terminated
  char alangs[MAXAPIDS][MAXLANGCODE2];
  int dpids[MAXDPIDS + 1]; // list is zero-terminated
  int dtypes[MAXDPIDS + 1]; // list is zero-terminated
  char dlangs[MAXDPIDS][MAXLANGCODE2];
  int spids[MAXSPIDS + 1]; // list is zero-terminated
  char slangs[MAXSPIDS][MAXLANGCODE2];
  uchar subtitlingTypes[MAXSPIDS];
  uint16_t compositionPageIds[MAXSPIDS];
  uint16_t ancillaryPageIds[MAXSPIDS];
  bool updatePrimaryDevice;
protected:
  int SectionLength(const uchar *Data, int Length) { return (Length >= 3) ? ((int(Data[1]) & 0x0F) << 8)| Data[2] : 0; }
public:
  cPatPmtParser(bool UpdatePrimaryDevice = false);
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
  bool GetVersions(int &PatVersion, int &PmtVersion) const;
       ///< Returns true if a valid PAT/PMT has been parsed and stores
       ///< the current version numbers in the given variables.
  int PmtPid(void) const { return pmtPid; }
       ///< Returns the PMT pid as defined by the current PAT.
       ///< If no PAT has been received yet, -1 will be returned.
  int Vpid(void) const { return vpid; }
       ///< Returns the video pid as defined by the current PMT, or 0 if no video
       ///< pid has been detected, yet.
  int Ppid(void) const { return ppid; }
       ///< Returns the PCR pid as defined by the current PMT, or 0 if no PCR
       ///< pid has been detected, yet.
  int Vtype(void) const { return vtype; }
       ///< Returns the video stream type as defined by the current PMT, or 0 if no video
       ///< stream type has been detected, yet.
  const int *Apids(void) const { return apids; }
  const int *Dpids(void) const { return dpids; }
  const int *Spids(void) const { return spids; }
  int Apid(int i) const { return (0 <= i && i < MAXAPIDS) ? apids[i] : 0; }
  int Dpid(int i) const { return (0 <= i && i < MAXDPIDS) ? dpids[i] : 0; }
  int Spid(int i) const { return (0 <= i && i < MAXSPIDS) ? spids[i] : 0; }
  int Atype(int i) const { return (0 <= i && i < MAXAPIDS) ? atypes[i] : 0; }
  int Dtype(int i) const { return (0 <= i && i < MAXDPIDS) ? dtypes[i] : 0; }
  const char *Alang(int i) const { return (0 <= i && i < MAXAPIDS) ? alangs[i] : ""; }
  const char *Dlang(int i) const { return (0 <= i && i < MAXDPIDS) ? dlangs[i] : ""; }
  const char *Slang(int i) const { return (0 <= i && i < MAXSPIDS) ? slangs[i] : ""; }
  uchar SubtitlingType(int i) const { return (0 <= i && i < MAXSPIDS) ? subtitlingTypes[i] : uchar(0); }
  uint16_t CompositionPageId(int i) const { return (0 <= i && i < MAXSPIDS) ? compositionPageIds[i] : uint16_t(0); }
  uint16_t AncillaryPageId(int i) const { return (0 <= i && i < MAXSPIDS) ? ancillaryPageIds[i] : uint16_t(0); }
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
  uchar *lastData;
  int lastLength;
  bool repeatLast;
public:
  cTsToPes(void);
  ~cTsToPes();
  void PutTs(const uchar *Data, int Length);
       ///< Puts the payload data of the single TS packet at Data into the converter.
       ///< Length is always TS_SIZE.
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
       ///< split into several PES packets with smaller sizes.
       ///< Note that for video data GetPes() may only be called if the next
       ///< TS packet that will be given to PutTs() has the "payload start" flag
       ///< set, because this is the only way to determine the end of a video PES
       ///< packet.
  void SetRepeatLast(void);
       ///< Makes the next call to GetPes() return exactly the same data as the
       ///< last one (provided there was no call to Reset() in the meantime).
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

#define MIN_TS_PACKETS_FOR_FRAME_DETECTOR 5

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
  int numFrames;
  int numIFrames;
  bool isVideo;
  double framesPerSecond;
  int framesInPayloadUnit;
  int framesPerPayloadUnit; // Some broadcasters send one frame per payload unit (== 1),
                            // some put an entire GOP into one payload unit (> 1), and
                            // some spread a single frame over several payload units (< 0).
  int payloadUnitOfFrame;
  bool scanning;
  uint32_t scanner;
  int SkipPackets(const uchar *&Data, int &Length, int &Processed, int &FrameTypeOffset);
public:
  cFrameDetector(int Pid = 0, int Type = 0);
      ///< Sets up a frame detector for the given Pid and stream Type.
      ///< If no Pid and Type is given, they need to be set by a separate
      ///< call to SetPid().
  void SetPid(int Pid, int Type);
      ///< Sets the Pid and stream Type to detect frames for.
  void Reset(void);
      ///< Resets any counters and flags used while syncing and prepares
      ///< the frame detector for actual work.
  int Analyze(const uchar *Data, int Length);
      ///< Analyzes the TS packets pointed to by Data. Length is the number of
      ///< bytes Data points to, and must be a multiple of TS_SIZE.
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
  double FramesPerSecond(void) { return framesPerSecond; }
      ///< Returns the number of frames per second, or 0 if this information is not
      ///< available.
  };

#endif // __REMUX_H
