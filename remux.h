/*
 * remux.h: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.h 5.2 2021/12/25 14:11:39 kls Exp $
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
#define CATPID 0x0001 // CAT PID (constant 1)
#define EITPID 0x0012 // EIT PID (constant 18)
#define MAXPID 0x2000 // for arrays that use a PID as the index

#define PTSTICKS  90000 // number of PTS ticks per second
#define PCRFACTOR 300 // conversion from 27MHz PCR extension to 90kHz PCR base
#define MAX33BIT  0x00000001FFFFFFFFLL // max. possible value with 33 bit
#define MAX27MHZ  ((MAX33BIT + 1) * PCRFACTOR - 1) // max. possible PCR value

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

inline void TsSetPid(uchar *p, int Pid)
{
  p[1] = (p[1] & ~TS_PID_MASK_HI) | ((Pid >> 8) & TS_PID_MASK_HI);
  p[2] = Pid & 0x00FF;
}

inline bool TsIsScrambled(const uchar *p)
{
  return p[3] & TS_SCRAMBLING_CONTROL;
}

inline uchar TsContinuityCounter(const uchar *p)
{
  return p[3] & TS_CONT_CNT_MASK;
}

inline void TsSetContinuityCounter(uchar *p, uchar Counter)
{
  p[3] = (p[3] & ~TS_CONT_CNT_MASK) | (Counter & TS_CONT_CNT_MASK);
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

inline int64_t TsGetPcr(const uchar *p)
{
  if (TsHasAdaptationField(p)) {
     if (p[4] >= 7 && (p[5] & TS_ADAPT_PCR)) {
        return ((((int64_t)p[ 6]) << 25) |
                (((int64_t)p[ 7]) << 17) |
                (((int64_t)p[ 8]) <<  9) |
                (((int64_t)p[ 9]) <<  1) |
                (((int64_t)p[10]) >>  7)) * PCRFACTOR +
               (((((int)p[10]) & 0x01) << 8) |
                ( ((int)p[11])));
        }
     }
  return -1;
}

void TsHidePayload(uchar *p);
void TsSetPcr(uchar *p, int64_t Pcr);

// Helper macro and function to quickly check whether Data points to the beginning
// of a TS packet. The return value is the number of bytes that need to be skipped
// to synchronize on the next TS packet (zero if already sync'd). TsSync() can be
// called directly, the macro just performs the initial check inline and adds some
// debug information for logging.

#define TS_SYNC(Data, Length) (*Data == TS_SYNC_BYTE ? 0 : TsSync(Data, Length, __FILE__, __FUNCTION__, __LINE__))
int TsSync(const uchar *Data, int Length, const char *File = NULL, const char *Function = NULL, int Line = 0);

// The following functions all take a pointer to a sequence of complete TS packets.

int64_t TsGetPts(const uchar *p, int l);
int64_t TsGetDts(const uchar *p, int l);
void TsSetPts(uchar *p, int l, int64_t Pts);
void TsSetDts(uchar *p, int l, int64_t Dts);

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

inline bool PesHasDts(const uchar *p)
{
  return (p[7] & 0x40) && p[8] >= 10;
}

inline int64_t PesGetPts(const uchar *p)
{
  return ((((int64_t)p[ 9]) & 0x0E) << 29) |
         (( (int64_t)p[10])         << 22) |
         ((((int64_t)p[11]) & 0xFE) << 14) |
         (( (int64_t)p[12])         <<  7) |
         ((((int64_t)p[13]) & 0xFE) >>  1);
}

inline int64_t PesGetDts(const uchar *p)
{
  return ((((int64_t)p[14]) & 0x0E) << 29) |
         (( (int64_t)p[15])         << 22) |
         ((((int64_t)p[16]) & 0xFE) << 14) |
         (( (int64_t)p[17])         <<  7) |
         ((((int64_t)p[18]) & 0xFE) >>  1);
}

void PesSetPts(uchar *p, int64_t Pts);
void PesSetDts(uchar *p, int64_t Dts);

// PTS handling:

inline int64_t PtsAdd(int64_t Pts1, int64_t Pts2) { return (Pts1 + Pts2) & MAX33BIT; }
       ///< Adds the given PTS values, taking into account the 33bit wrap around.
int64_t PtsDiff(int64_t Pts1, int64_t Pts2);
       ///< Returns the difference between two PTS values. The result of Pts2 - Pts1
       ///< is the actual number of 90kHz time ticks that pass from Pts1 to Pts2,
       ///< properly taking into account the 33bit wrap around. If Pts2 is "before"
       ///< Pts1, the result is negative.

// A transparent TS payload handler:

class cTsPayload {
private:
  uchar *data;
  int length;
  int pid;
  int index; // points to the next byte to process
  int numPacketsPid; // the number of TS packets with the given PID (for statistical purposes)
  int numPacketsOther; // the number of TS packets with other PIDs (for statistical purposes)
  uchar SetEof(void);
protected:
  void Reset(void);
public:
  cTsPayload(void);
  cTsPayload(uchar *Data, int Length, int Pid = -1);
       ///< Creates a new TS payload handler and calls Setup() with the given Data.
  void Setup(uchar *Data, int Length, int Pid = -1);
       ///< Sets up this TS payload handler with the given Data, which points to a
       ///< sequence of Length bytes of complete TS packets. Any incomplete TS
       ///< packet at the end will be ignored.
       ///< If Pid is given, only TS packets with data for that PID will be processed.
       ///< Otherwise the PID of the first TS packet defines which payload will be
       ///< delivered.
       ///< Any intermediate TS packets with different PIDs will be skipped.
  bool AtTsStart(void) { return index < length && (index % TS_SIZE) == 0; }
       ///< Returns true if this payload handler is currently pointing to first byte
       ///< of a TS packet.
  bool AtPayloadStart(void) { return AtTsStart() && TsPayloadStart(data + index) && TsPid(data + index) == pid; }
       ///< Returns true if this payload handler is currently pointing to the first byte
       ///< of a TS packet that starts a new payload.
  int Available(void) { return length - index; }
       ///< Returns the number of raw bytes (including any TS headers) still available
       ///< in the TS payload handler.
  int Used(void) { return (index + TS_SIZE - 1) / TS_SIZE * TS_SIZE; }
       ///< Returns the number of raw bytes that have already been used (e.g. by calling
       ///< GetByte()). Any TS packet of which at least a single byte has been delivered
       ///< is counted with its full size.
  bool Eof(void) const { return index >= length; }
       ///< Returns true if all available bytes of the TS payload have been processed.
  void Statistics(void) const;
       ///< May be called after a new frame has been detected, and will log a warning
       ///< if the number of TS packets required to determine the frame type exceeded
       ///< some safety limits.
  uchar GetByte(void);
       ///< Gets the next byte of the TS payload, skipping any intermediate TS header data.
  bool SkipBytes(int Bytes);
       ///< Skips the given number of bytes in the payload and returns true if there
       ///< is still data left to read.
  bool SkipPesHeader(void);
       ///< Skips all bytes belonging to the PES header of the payload.
  int GetLastIndex(void);
       ///< Returns the index into the TS data of the payload byte that has most recently
       ///< been read. If no byte has been read yet, -1 will be returned.
  void SetByte(uchar Byte, int Index);
       ///< Sets the TS data byte at the given Index to the value Byte.
       ///< Index should be one that has been retrieved by a previous call to GetIndex(),
       ///< otherwise the behaviour is undefined. The current read index will not be
       ///< altered by a call to this function.
  bool Find(uint32_t Code);
       ///< Searches for the four byte sequence given in Code and returns true if it
       ///< was found within the payload data. The next call to GetByte() will return the
       ///< value immediately following the Code. If the code was not found, the read
       ///< index will remain the same as before this call, so that several calls to
       ///< Find() can be performed starting at the same index..
       ///< The special code 0xFFFFFFFF can not be searched, because this value is used
       ///< to initialize the scanner.
  };

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

#define MAX_PMT_PIDS 32

class cPatPmtParser {
private:
  uchar pmt[MAX_SECTION_SIZE];
  int pmtSize;
  int patVersion;
  int pmtVersion;
  int pmtPids[MAX_PMT_PIDS + 1]; // list is zero-terminated
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
  bool completed;
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
  bool ParsePatPmt(const uchar *Data, int Length);
       ///< Parses the given Data (which may consist of several TS packets, typically
       ///< an entire frame) and extracts the PAT and PMT.
       ///< Returns true if a valid PAT/PMT has been detected.
  bool GetVersions(int &PatVersion, int &PmtVersion) const;
       ///< Returns true if a valid PAT/PMT has been parsed and stores
       ///< the current version numbers in the given variables.
  bool IsPmtPid(int Pid) const { for (int i = 0; pmtPids[i]; i++) if (pmtPids[i] == Pid) return true; return false; }
       ///< Returns true if Pid the one of the PMT pids as defined by the current PAT.
       ///< If no PAT has been received yet, false will be returned.
  int Vpid(void) const { return vpid; }
       ///< Returns the video pid as defined by the current PMT, or 0 if no video
       ///< pid has been detected, yet.
  int Ppid(void) const { return ppid; }
       ///< Returns the PCR pid as defined by the current PMT, or 0 if no PCR
       ///< pid has been detected, yet.
  int Vtype(void) const { return vtype; }
       ///< Returns the video stream type as defined by the current PMT, or 0 if no video
       ///< stream type has been detected, yet.
  bool Completed(void) { return completed; }
       ///< Returns true if the PMT has been completely parsed.
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

// EIT Generator:

class cEitGenerator {
private:
  uchar eit[TS_SIZE];
  int counter;
  int version;
  uint16_t YMDtoMJD(int Y, int M, int D);
  uchar *AddParentalRatingDescriptor(uchar *p, uchar ParentalRating = 0);
public:
  cEitGenerator(int Sid = 0);
  uchar *Generate(int Sid);
  uchar *Data(void) { return eit; }
  int Length(void) { return sizeof(eit); }
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

#define MIN_TS_PACKETS_FOR_FRAME_DETECTOR 100

class cFrameParser;

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
  double framesPerSecond;
  int framesInPayloadUnit;
  int framesPerPayloadUnit; // Some broadcasters send one frame per payload unit (== 1),
                            // while others put an entire GOP into one payload unit (> 1).
  bool scanning;
  cFrameParser *parser;
public:
  cFrameDetector(int Pid = 0, int Type = 0);
      ///< Sets up a frame detector for the given Pid and stream Type.
      ///< If no Pid and Type is given, they need to be set by a separate
      ///< call to SetPid().
  void SetPid(int Pid, int Type);
      ///< Sets the Pid and stream Type to detect frames for.
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
