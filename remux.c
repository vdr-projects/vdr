/*
 * remux.c: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.c 5.1 2021/05/11 20:47:31 kls Exp $
 */

#include "remux.h"
#include "device.h"
#include "libsi/si.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"
#include "recording.h"
#include "shutdown.h"
#include "tools.h"

// Set these to 'true' for debug output:
static bool DebugPatPmt = false;
static bool DebugFrames = false;

#define dbgpatpmt(a...) if (DebugPatPmt) fprintf(stderr, a)
#define dbgframes(a...) if (DebugFrames) fprintf(stderr, a)

#define MAX_TS_PACKETS_FOR_VIDEO_FRAME_DETECTION 6
#define WRN_TS_PACKETS_FOR_VIDEO_FRAME_DETECTION (MAX_TS_PACKETS_FOR_VIDEO_FRAME_DETECTION / 2)
#define WRN_TS_PACKETS_FOR_FRAME_DETECTOR (MIN_TS_PACKETS_FOR_FRAME_DETECTOR / 2)

#define EMPTY_SCANNER (0xFFFFFFFF)

ePesHeader AnalyzePesHeader(const uchar *Data, int Count, int &PesPayloadOffset, bool *ContinuationHeader)
{
  if (Count < 7)
     return phNeedMoreData; // too short

  if ((Data[6] & 0xC0) == 0x80) { // MPEG 2
     if (Count < 9)
        return phNeedMoreData; // too short

     PesPayloadOffset = 6 + 3 + Data[8];
     if (Count < PesPayloadOffset)
        return phNeedMoreData; // too short

     if (ContinuationHeader)
        *ContinuationHeader = ((Data[6] == 0x80) && !Data[7] && !Data[8]);

     return phMPEG2; // MPEG 2
     }

  // check for MPEG 1 ...
  PesPayloadOffset = 6;

  // skip up to 16 stuffing bytes
  for (int i = 0; i < 16; i++) {
      if (Data[PesPayloadOffset] != 0xFF)
         break;

      if (Count <= ++PesPayloadOffset)
         return phNeedMoreData; // too short
      }

  // skip STD_buffer_scale/size
  if ((Data[PesPayloadOffset] & 0xC0) == 0x40) {
     PesPayloadOffset += 2;

     if (Count <= PesPayloadOffset)
        return phNeedMoreData; // too short
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
     return phInvalid; // unknown

  if (Count < PesPayloadOffset)
     return phNeedMoreData; // too short

  return phMPEG1; // MPEG 1
}

#define VIDEO_STREAM_S   0xE0

// --- cRemux ----------------------------------------------------------------

void cRemux::SetBrokenLink(uchar *Data, int Length)
{
  int PesPayloadOffset = 0;
  if (AnalyzePesHeader(Data, Length, PesPayloadOffset) >= phMPEG1 && (Data[3] & 0xF0) == VIDEO_STREAM_S) {
     for (int i = PesPayloadOffset; i < Length - 7; i++) {
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

// --- Some TS handling tools ------------------------------------------------

void TsHidePayload(uchar *p)
{
  p[1] &= ~TS_PAYLOAD_START;
  p[3] |=  TS_ADAPT_FIELD_EXISTS;
  p[3] &= ~TS_PAYLOAD_EXISTS;
  p[4] = TS_SIZE - 5;
  p[5] = 0x00;
  memset(p + 6, 0xFF, TS_SIZE - 6);
}

void TsSetPcr(uchar *p, int64_t Pcr)
{
  if (TsHasAdaptationField(p)) {
     if (p[4] >= 7 && (p[5] & TS_ADAPT_PCR)) {
        int64_t b = Pcr / PCRFACTOR;
        int e = Pcr % PCRFACTOR;
        p[ 6] =  b >> 25;
        p[ 7] =  b >> 17;
        p[ 8] =  b >>  9;
        p[ 9] =  b >>  1;
        p[10] = (b <<  7) | (p[10] & 0x7E) | ((e >> 8) & 0x01);
        p[11] =  e;
        }
     }
}

int TsSync(const uchar *Data, int Length, const char *File, const char *Function, int Line)
{
  int Skipped = 0;
  while (Length > 0 && (*Data != TS_SYNC_BYTE || Length > TS_SIZE && Data[TS_SIZE] != TS_SYNC_BYTE)) {
        Data++;
        Length--;
        Skipped++;
        }
  if (Skipped && File && Function && Line)
     esyslog("ERROR: skipped %d bytes to sync on start of TS packet at %s/%s(%d)", Skipped, File, Function, Line);
  return Skipped;
}

int64_t TsGetPts(const uchar *p, int l)
{
  // Find the first packet with a PTS and use it:
  while (l > 0) {
        const uchar *d = p;
        if (TsPayloadStart(d) && TsGetPayload(&d) && PesHasPts(d))
           return PesGetPts(d);
        p += TS_SIZE;
        l -= TS_SIZE;
        }
  return -1;
}

int64_t TsGetDts(const uchar *p, int l)
{
  // Find the first packet with a DTS and use it:
  while (l > 0) {
        const uchar *d = p;
        if (TsPayloadStart(d) && TsGetPayload(&d) && PesHasDts(d))
           return PesGetDts(d);
        p += TS_SIZE;
        l -= TS_SIZE;
        }
  return -1;
}

void TsSetPts(uchar *p, int l, int64_t Pts)
{
  // Find the first packet with a PTS and use it:
  while (l > 0) {
        const uchar *d = p;
        if (TsPayloadStart(d) && TsGetPayload(&d) && PesHasPts(d)) {
           PesSetPts(const_cast<uchar *>(d), Pts);
           return;
           }
        p += TS_SIZE;
        l -= TS_SIZE;
        }
}

void TsSetDts(uchar *p, int l, int64_t Dts)
{
  // Find the first packet with a DTS and use it:
  while (l > 0) {
        const uchar *d = p;
        if (TsPayloadStart(d) && TsGetPayload(&d) && PesHasDts(d)) {
           PesSetDts(const_cast<uchar *>(d), Dts);
           return;
           }
        p += TS_SIZE;
        l -= TS_SIZE;
        }
}

// --- Some PES handling tools -----------------------------------------------

void PesSetPts(uchar *p, int64_t Pts)
{
  p[ 9] = ((Pts >> 29) & 0x0E) | (p[9] & 0xF1);
  p[10] =   Pts >> 22;
  p[11] = ((Pts >> 14) & 0xFE) | 0x01;
  p[12] =   Pts >>  7;
  p[13] = ((Pts <<  1) & 0xFE) | 0x01;
}

void PesSetDts(uchar *p, int64_t Dts)
{
  p[14] = ((Dts >> 29) & 0x0E) | (p[14] & 0xF1);
  p[15] =   Dts >> 22;
  p[16] = ((Dts >> 14) & 0xFE) | 0x01;
  p[17] =   Dts >>  7;
  p[18] = ((Dts <<  1) & 0xFE) | 0x01;
}

int64_t PtsDiff(int64_t Pts1, int64_t Pts2)
{
  int64_t d = Pts2 - Pts1;
  if (d > MAX33BIT / 2)
     return d - (MAX33BIT + 1);
  if (d < -MAX33BIT / 2)
     return d + (MAX33BIT + 1);
  return d;
}

// --- cTsPayload ------------------------------------------------------------

cTsPayload::cTsPayload(void)
{
  data = NULL;
  length = 0;
  pid = -1;
  Reset();
}

cTsPayload::cTsPayload(uchar *Data, int Length, int Pid)
{
  Setup(Data, Length, Pid);
}

uchar cTsPayload::SetEof(void)
{
  length = index; // triggers EOF
  return 0x00;
}

void cTsPayload::Reset(void)
{
  index = 0;
  numPacketsPid = 0;
  numPacketsOther = 0;
}

void cTsPayload::Setup(uchar *Data, int Length, int Pid)
{
  data = Data;
  length = Length;
  pid = Pid >= 0 ? Pid : TsPid(Data);
  Reset();
}

uchar cTsPayload::GetByte(void)
{
  if (!Eof()) {
     if (index % TS_SIZE == 0) { // encountered the next TS header
        for (;; index += TS_SIZE) {
            if (data[index] == TS_SYNC_BYTE && index + TS_SIZE <= length) { // to make sure we are at a TS header start and drop incomplete TS packets at the end
               uchar *p = data + index;
               if (TsPid(p) == pid) { // only handle TS packets for the initial PID
                  if (++numPacketsPid > MAX_TS_PACKETS_FOR_VIDEO_FRAME_DETECTION)
                     return SetEof();
                  if (TsHasPayload(p)) {
                     if (index > 0 && TsPayloadStart(p)) // checking index to not skip the very first TS packet
                        return SetEof();
                     index += TsPayloadOffset(p);
                     break;
                     }
                  }
               else if (TsPid(p) == PATPID)
                  return SetEof(); // caller must see PAT packets in case of index regeneration
               else
                  numPacketsOther++;
               }
            else
               return SetEof();
           }
        }
     return data[index++];
     }
  return 0x00;
}

bool cTsPayload::SkipBytes(int Bytes)
{
  while (Bytes-- > 0)
        GetByte();
  return !Eof();
}

bool cTsPayload::SkipPesHeader(void)
{
  return SkipBytes(PesPayloadOffset(data + TsPayloadOffset(data)));
}

int cTsPayload::GetLastIndex(void)
{
  return index - 1;
}

void cTsPayload::SetByte(uchar Byte, int Index)
{
  if (Index >= 0 && Index < length)
     data[Index] = Byte;
}

bool cTsPayload::Find(uint32_t Code)
{
  int OldIndex = index;
  int OldNumPacketsPid = numPacketsPid;
  int OldNumPacketsOther = numPacketsOther;
  uint32_t Scanner = EMPTY_SCANNER;
  while (!Eof()) {
        Scanner = (Scanner << 8) | GetByte();
        if (Scanner == Code)
           return true;
        }
  index = OldIndex;
  numPacketsPid = OldNumPacketsPid;
  numPacketsOther = OldNumPacketsOther;
  return false;
}

void cTsPayload::Statistics(void) const
{
  if (numPacketsPid + numPacketsOther > WRN_TS_PACKETS_FOR_FRAME_DETECTOR)
     dsyslog("WARNING: required (%d+%d) TS packets to determine frame type", numPacketsOther, numPacketsPid);
  if (numPacketsPid > WRN_TS_PACKETS_FOR_VIDEO_FRAME_DETECTION)
     dsyslog("WARNING: required %d video TS packets to determine frame type", numPacketsPid);
}

// --- cPatPmtGenerator ------------------------------------------------------

cPatPmtGenerator::cPatPmtGenerator(const cChannel *Channel)
{
  numPmtPackets = 0;
  patCounter = pmtCounter = 0;
  patVersion = pmtVersion = 0;
  pmtPid = 0;
  esInfoLength = NULL;
  SetChannel(Channel);
}

void cPatPmtGenerator::IncCounter(int &Counter, uchar *TsPacket)
{
  TsPacket[3] = (TsPacket[3] & 0xF0) | Counter;
  if (++Counter > 0x0F)
     Counter = 0x00;
}

void cPatPmtGenerator::IncVersion(int &Version)
{
  if (++Version > 0x1F)
     Version = 0x00;
}

void cPatPmtGenerator::IncEsInfoLength(int Length)
{
  if (esInfoLength) {
     Length += ((*esInfoLength & 0x0F) << 8) | *(esInfoLength + 1);
     *esInfoLength = 0xF0 | (Length >> 8);
     *(esInfoLength + 1) = Length;
     }
}

int cPatPmtGenerator::MakeStream(uchar *Target, uchar Type, int Pid)
{
  int i = 0;
  Target[i++] = Type; // stream type
  Target[i++] = 0xE0 | (Pid >> 8); // dummy (3), pid hi (5)
  Target[i++] = Pid; // pid lo
  esInfoLength = &Target[i];
  Target[i++] = 0xF0; // dummy (4), ES info length hi
  Target[i++] = 0x00; // ES info length lo
  return i;
}

int cPatPmtGenerator::MakeAC3Descriptor(uchar *Target, uchar Type)
{
  int i = 0;
  Target[i++] = Type;
  Target[i++] = 0x01; // length
  Target[i++] = 0x00;
  IncEsInfoLength(i);
  return i;
}

int cPatPmtGenerator::MakeSubtitlingDescriptor(uchar *Target, const char *Language, uchar SubtitlingType, uint16_t CompositionPageId, uint16_t AncillaryPageId)
{
  int i = 0;
  Target[i++] = SI::SubtitlingDescriptorTag;
  Target[i++] = 0x08; // length
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = SubtitlingType;
  Target[i++] = CompositionPageId >> 8;
  Target[i++] = CompositionPageId & 0xFF;
  Target[i++] = AncillaryPageId >> 8;
  Target[i++] = AncillaryPageId & 0xFF;
  IncEsInfoLength(i);
  return i;
}

int cPatPmtGenerator::MakeLanguageDescriptor(uchar *Target, const char *Language)
{
  int i = 0;
  Target[i++] = SI::ISO639LanguageDescriptorTag;
  int Length = i++;
  Target[Length] = 0x00; // length
  for (const char *End = Language + strlen(Language); Language < End; ) {
      Target[i++] = *Language++;
      Target[i++] = *Language++;
      Target[i++] = *Language++;
      Target[i++] = 0x00;     // audio type
      Target[Length] += 0x04; // length
      if (*Language == '+')
         Language++;
      }
  IncEsInfoLength(i);
  return i;
}

int cPatPmtGenerator::MakeCRC(uchar *Target, const uchar *Data, int Length)
{
  int crc = SI::CRC32::crc32((const char *)Data, Length, 0xFFFFFFFF);
  int i = 0;
  Target[i++] = crc >> 24;
  Target[i++] = crc >> 16;
  Target[i++] = crc >> 8;
  Target[i++] = crc;
  return i;
}

#define P_TSID    0x8008 // pseudo TS ID
#define P_PMT_PID 0x0084 // pseudo PMT pid
#define MAXPID    0x2000 // the maximum possible number of pids

void cPatPmtGenerator::GeneratePmtPid(const cChannel *Channel)
{
  bool Used[MAXPID] = { false };
#define SETPID(p) { if ((p) >= 0 && (p) < MAXPID) Used[p] = true; }
#define SETPIDS(l) { const int *p = l; while (*p) { SETPID(*p); p++; } }
  SETPID(Channel->Vpid());
  SETPID(Channel->Ppid());
  SETPID(Channel->Tpid());
  SETPIDS(Channel->Apids());
  SETPIDS(Channel->Dpids());
  SETPIDS(Channel->Spids());
  for (pmtPid = P_PMT_PID; Used[pmtPid]; pmtPid++)
      ;
}

void cPatPmtGenerator::GeneratePat(void)
{
  memset(pat, 0xFF, sizeof(pat));
  uchar *p = pat;
  int i = 0;
  p[i++] = TS_SYNC_BYTE; // TS indicator
  p[i++] = TS_PAYLOAD_START | (PATPID >> 8); // flags (3), pid hi (5)
  p[i++] = PATPID & 0xFF; // pid lo
  p[i++] = 0x10; // flags (4), continuity counter (4)
  p[i++] = 0x00; // pointer field (payload unit start indicator is set)
  int PayloadStart = i;
  p[i++] = 0x00; // table id
  p[i++] = 0xB0; // section syntax indicator (1), dummy (3), section length hi (4)
  int SectionLength = i;
  p[i++] = 0x00; // section length lo (filled in later)
  p[i++] = P_TSID >> 8;   // TS id hi
  p[i++] = P_TSID & 0xFF; // TS id lo
  p[i++] = 0xC1 | (patVersion << 1); // dummy (2), version number (5), current/next indicator (1)
  p[i++] = 0x00; // section number
  p[i++] = 0x00; // last section number
  p[i++] = pmtPid >> 8;   // program number hi
  p[i++] = pmtPid & 0xFF; // program number lo
  p[i++] = 0xE0 | (pmtPid >> 8); // dummy (3), PMT pid hi (5)
  p[i++] = pmtPid & 0xFF; // PMT pid lo
  pat[SectionLength] = i - SectionLength - 1 + 4; // -1 = SectionLength storage, +4 = length of CRC
  MakeCRC(pat + i, pat + PayloadStart, i - PayloadStart);
  IncVersion(patVersion);
}

void cPatPmtGenerator::GeneratePmt(const cChannel *Channel)
{
  // generate the complete PMT section:
  uchar buf[MAX_SECTION_SIZE];
  memset(buf, 0xFF, sizeof(buf));
  numPmtPackets = 0;
  if (Channel) {
     int Vpid = Channel->Vpid();
     int Ppid = Channel->Ppid();
     uchar *p = buf;
     int i = 0;
     p[i++] = 0x02; // table id
     int SectionLength = i;
     p[i++] = 0xB0; // section syntax indicator (1), dummy (3), section length hi (4)
     p[i++] = 0x00; // section length lo (filled in later)
     p[i++] = pmtPid >> 8;   // program number hi
     p[i++] = pmtPid & 0xFF; // program number lo
     p[i++] = 0xC1 | (pmtVersion << 1); // dummy (2), version number (5), current/next indicator (1)
     p[i++] = 0x00; // section number
     p[i++] = 0x00; // last section number
     p[i++] = 0xE0 | (Ppid >> 8); // dummy (3), PCR pid hi (5)
     p[i++] = Ppid; // PCR pid lo
     p[i++] = 0xF0; // dummy (4), program info length hi (4)
     p[i++] = 0x00; // program info length lo

     if (Vpid)
        i += MakeStream(buf + i, Channel->Vtype(), Vpid);
     for (int n = 0; Channel->Apid(n); n++) {
         i += MakeStream(buf + i, Channel->Atype(n), Channel->Apid(n));
         const char *Alang = Channel->Alang(n);
         i += MakeLanguageDescriptor(buf + i, Alang);
         }
     for (int n = 0; Channel->Dpid(n); n++) {
         i += MakeStream(buf + i, 0x06, Channel->Dpid(n));
         i += MakeAC3Descriptor(buf + i, Channel->Dtype(n));
         i += MakeLanguageDescriptor(buf + i, Channel->Dlang(n));
         }
     for (int n = 0; Channel->Spid(n); n++) {
         i += MakeStream(buf + i, 0x06, Channel->Spid(n));
         i += MakeSubtitlingDescriptor(buf + i, Channel->Slang(n), Channel->SubtitlingType(n), Channel->CompositionPageId(n), Channel->AncillaryPageId(n));
         }

     int sl = i - SectionLength - 2 + 4; // -2 = SectionLength storage, +4 = length of CRC
     buf[SectionLength] |= (sl >> 8) & 0x0F;
     buf[SectionLength + 1] = sl;
     MakeCRC(buf + i, buf, i);
     // split the PMT section into several TS packets:
     uchar *q = buf;
     bool pusi = true;
     while (i > 0) {
           uchar *p = pmt[numPmtPackets++];
           int j = 0;
           p[j++] = TS_SYNC_BYTE; // TS indicator
           p[j++] = (pusi ? TS_PAYLOAD_START : 0x00) | (pmtPid >> 8); // flags (3), pid hi (5)
           p[j++] = pmtPid & 0xFF; // pid lo
           p[j++] = 0x10; // flags (4), continuity counter (4)
           if (pusi) {
              p[j++] = 0x00; // pointer field (payload unit start indicator is set)
              pusi = false;
              }
           int l = TS_SIZE - j;
           memcpy(p + j, q, l);
           q += l;
           i -= l;
           }
     IncVersion(pmtVersion);
     }
}

void cPatPmtGenerator::SetVersions(int PatVersion, int PmtVersion)
{
  patVersion = PatVersion & 0x1F;
  pmtVersion = PmtVersion & 0x1F;
}

void cPatPmtGenerator::SetChannel(const cChannel *Channel)
{
  if (Channel) {
     GeneratePmtPid(Channel);
     GeneratePat();
     GeneratePmt(Channel);
     }
}

uchar *cPatPmtGenerator::GetPat(void)
{
  IncCounter(patCounter, pat);
  return pat;
}

uchar *cPatPmtGenerator::GetPmt(int &Index)
{
  if (Index < numPmtPackets) {
     IncCounter(pmtCounter, pmt[Index]);
     return pmt[Index++];
     }
  return NULL;
}

// --- cPatPmtParser ---------------------------------------------------------

cPatPmtParser::cPatPmtParser(bool UpdatePrimaryDevice)
{
  updatePrimaryDevice = UpdatePrimaryDevice;
  Reset();
}

void cPatPmtParser::Reset(void)
{
  completed = false;
  pmtSize = 0;
  patVersion = pmtVersion = -1;
  pmtPids[0] = 0;
  vpid = vtype = 0;
  ppid = 0;
}

void cPatPmtParser::ParsePat(const uchar *Data, int Length)
{
  // Unpack the TS packet:
  int PayloadOffset = TsPayloadOffset(Data);
  Data += PayloadOffset;
  Length -= PayloadOffset;
  // The PAT is always assumed to fit into a single TS packet
  if ((Length -= Data[0] + 1) <= 0)
     return;
  Data += Data[0] + 1; // process pointer_field
  SI::PAT Pat(Data, false);
  if (Pat.CheckCRCAndParse()) {
     dbgpatpmt("PAT: TSid = %d, c/n = %d, v = %d, s = %d, ls = %d\n", Pat.getTransportStreamId(), Pat.getCurrentNextIndicator(), Pat.getVersionNumber(), Pat.getSectionNumber(), Pat.getLastSectionNumber());
     if (patVersion == Pat.getVersionNumber())
        return;
     int NumPmtPids = 0;
     SI::PAT::Association assoc;
     for (SI::Loop::Iterator it; Pat.associationLoop.getNext(assoc, it); ) {
         dbgpatpmt("     isNITPid = %d\n", assoc.isNITPid());
         if (!assoc.isNITPid()) {
            if (NumPmtPids <= MAX_PMT_PIDS)
               pmtPids[NumPmtPids++] = assoc.getPid();
            dbgpatpmt("     service id = %d, pid = %d\n", assoc.getServiceId(), assoc.getPid());
            }
         }
     pmtPids[NumPmtPids] = 0;
     patVersion = Pat.getVersionNumber();
     }
  else
     esyslog("ERROR: can't parse PAT");
}

void cPatPmtParser::ParsePmt(const uchar *Data, int Length)
{
  // Unpack the TS packet:
  bool PayloadStart = TsPayloadStart(Data);
  int PayloadOffset = TsPayloadOffset(Data);
  Data += PayloadOffset;
  Length -= PayloadOffset;
  // The PMT may extend over several TS packets, so we need to assemble them
  if (PayloadStart) {
     pmtSize = 0;
     if ((Length -= Data[0] + 1) <= 0)
        return;
     Data += Data[0] + 1; // this is the first packet
     if (SectionLength(Data, Length) > Length) {
        if (Length <= int(sizeof(pmt))) {
           memcpy(pmt, Data, Length);
           pmtSize = Length;
           }
        else
           esyslog("ERROR: PMT packet length too big (%d byte)!", Length);
        return;
        }
     // the packet contains the entire PMT section, so we run into the actual parsing
     }
  else if (pmtSize > 0) {
     // this is a following packet, so we add it to the pmt storage
     if (Length <= int(sizeof(pmt)) - pmtSize) {
        memcpy(pmt + pmtSize, Data, Length);
        pmtSize += Length;
        }
     else {
        esyslog("ERROR: PMT section length too big (%d byte)!", pmtSize + Length);
        pmtSize = 0;
        }
     if (SectionLength(pmt, pmtSize) > pmtSize)
        return; // more packets to come
     // the PMT section is now complete, so we run into the actual parsing
     Data = pmt;
     }
  else
     return; // fragment of broken packet - ignore
  SI::PMT Pmt(Data, false);
  if (Pmt.CheckCRCAndParse()) {
     dbgpatpmt("PMT: sid = %d, c/n = %d, v = %d, s = %d, ls = %d\n", Pmt.getServiceId(), Pmt.getCurrentNextIndicator(), Pmt.getVersionNumber(), Pmt.getSectionNumber(), Pmt.getLastSectionNumber());
     dbgpatpmt("     pcr = %d\n", Pmt.getPCRPid());
     if (pmtVersion == Pmt.getVersionNumber())
        return;
     if (updatePrimaryDevice)
        cDevice::PrimaryDevice()->ClrAvailableTracks(false, true);
     int NumApids = 0;
     int NumDpids = 0;
     int NumSpids = 0;
     vpid = vtype = 0;
     ppid = 0;
     apids[0] = 0;
     dpids[0] = 0;
     spids[0] = 0;
     atypes[0] = 0;
     dtypes[0] = 0;
     SI::PMT::Stream stream;
     for (SI::Loop::Iterator it; Pmt.streamLoop.getNext(stream, it); ) {
         dbgpatpmt("     stream type = %02X, pid = %d", stream.getStreamType(), stream.getPid());
         switch (stream.getStreamType()) {
           case 0x01: // STREAMTYPE_11172_VIDEO
           case 0x02: // STREAMTYPE_13818_VIDEO
           case 0x1B: // H.264
           case 0x24: // H.265
                      vpid = stream.getPid();
                      vtype = stream.getStreamType();
                      ppid = Pmt.getPCRPid();
                      break;
           case 0x03: // STREAMTYPE_11172_AUDIO
           case 0x04: // STREAMTYPE_13818_AUDIO
           case 0x0F: // ISO/IEC 13818-7 Audio with ADTS transport syntax
           case 0x11: // ISO/IEC 14496-3 Audio with LATM transport syntax
                      {
                      if (NumApids < MAXAPIDS) {
                         apids[NumApids] = stream.getPid();
                         atypes[NumApids] = stream.getStreamType();
                         *alangs[NumApids] = 0;
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    SI::ISO639LanguageDescriptor::Language l;
                                    char *s = alangs[NumApids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; ld->languageLoop.getNext(l, it); ) {
                                        if (*ld->languageCode != '-') { // some use "---" to indicate "none"
                                           dbgpatpmt(" '%s'", l.languageCode);
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(l.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    }
                                    break;
                               default: ;
                               }
                             delete d;
                             }
                         if (updatePrimaryDevice)
                            cDevice::PrimaryDevice()->SetAvailableTrack(ttAudio, NumApids, apids[NumApids], alangs[NumApids]);
                         NumApids++;
                         apids[NumApids] = 0;
                         }
                      }
                      break;
           case 0x06: // STREAMTYPE_13818_PES_PRIVATE
                      {
                      int dpid = 0;
                      int dtype = 0;
                      char lang[MAXLANGCODE1] = "";
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::AC3DescriptorTag:
                            case SI::EnhancedAC3DescriptorTag:
                                 dbgpatpmt(" AC3");
                                 dpid = stream.getPid();
                                 dtype = d->getDescriptorTag();
                                 break;
                            case SI::SubtitlingDescriptorTag:
                                 dbgpatpmt(" subtitling");
                                 if (NumSpids < MAXSPIDS) {
                                    spids[NumSpids] = stream.getPid();
                                    *slangs[NumSpids] = 0;
                                    subtitlingTypes[NumSpids] = 0;
                                    compositionPageIds[NumSpids] = 0;
                                    ancillaryPageIds[NumSpids] = 0;
                                    SI::SubtitlingDescriptor *sd = (SI::SubtitlingDescriptor *)d;
                                    SI::SubtitlingDescriptor::Subtitling sub;
                                    char *s = slangs[NumSpids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; sd->subtitlingLoop.getNext(sub, it); ) {
                                        if (sub.languageCode[0]) {
                                           dbgpatpmt(" '%s'", sub.languageCode);
                                           subtitlingTypes[NumSpids] = sub.getSubtitlingType();
                                           compositionPageIds[NumSpids] = sub.getCompositionPageId();
                                           ancillaryPageIds[NumSpids] = sub.getAncillaryPageId();
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(sub.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    if (updatePrimaryDevice)
                                       cDevice::PrimaryDevice()->SetAvailableTrack(ttSubtitle, NumSpids, spids[NumSpids], slangs[NumSpids]);
                                    NumSpids++;
                                    spids[NumSpids] = 0;
                                    }
                                 break;
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 dbgpatpmt(" '%s'", ld->languageCode);
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 }
                                 break;
                            default: ;
                            }
                          delete d;
                          }
                      if (dpid) {
                         if (NumDpids < MAXDPIDS) {
                            dpids[NumDpids] = dpid;
                            dtypes[NumDpids] = dtype;
                            strn0cpy(dlangs[NumDpids], lang, sizeof(dlangs[NumDpids]));
                            if (updatePrimaryDevice && Setup.UseDolbyDigital)
                               cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, NumDpids, dpid, lang);
                            NumDpids++;
                            dpids[NumDpids] = 0;
                            }
                         }
                      }
                      break;
           case 0x81: // STREAMTYPE_USER_PRIVATE - AC3 audio for ATSC and BD
           case 0x82: // STREAMTYPE_USER_PRIVATE - DTS audio for BD
           case 0x87: // eac3
                      {
                      dbgpatpmt(" %s",
                          stream.getStreamType() == 0x81 ? "AC3" :
                          stream.getStreamType() == 0x87 ? "AC3" :
                          stream.getStreamType() == 0x82 ? "DTS" : "");
                      char lang[MAXLANGCODE1] = { 0 };
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 dbgpatpmt(" '%s'", ld->languageCode);
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 }
                                 break;
                            default: ;
                            }
                         delete d;
                         }
                      if (NumDpids < MAXDPIDS) {
                         dpids[NumDpids] = stream.getPid();
                         dtypes[NumDpids] = SI::AC3DescriptorTag;
                         strn0cpy(dlangs[NumDpids], lang, sizeof(dlangs[NumDpids]));
                         if (updatePrimaryDevice && Setup.UseDolbyDigital)
                            cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, NumDpids, stream.getPid(), lang);
                         NumDpids++;
                         dpids[NumDpids] = 0;
                         }
                      }
                      break;
           case 0x90: // PGS subtitles for BD
                      {
                      dbgpatpmt(" subtitling");
                      char lang[MAXLANGCODE1] = { 0 };
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 dbgpatpmt(" '%s'", ld->languageCode);
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 if (NumSpids < MAXSPIDS) {
                                    spids[NumSpids] = stream.getPid();
                                    *slangs[NumSpids] = 0;
                                    subtitlingTypes[NumSpids] = 0;
                                    compositionPageIds[NumSpids] = 0;
                                    ancillaryPageIds[NumSpids] = 0;
                                    if (updatePrimaryDevice)
                                       cDevice::PrimaryDevice()->SetAvailableTrack(ttSubtitle, NumSpids, stream.getPid(), lang);
                                    NumSpids++;
                                    spids[NumSpids] = 0;
                                    }
                                 }
                                 break;
                            default: ;
                            }
                          delete d;
                          }
                      }
                      break;
           default: ;
           }
         dbgpatpmt("\n");
         if (updatePrimaryDevice) {
            cDevice::PrimaryDevice()->EnsureAudioTrack(true);
            cDevice::PrimaryDevice()->EnsureSubtitleTrack();
            }
         }
     pmtVersion = Pmt.getVersionNumber();
     completed = true;
     }
  else
     esyslog("ERROR: can't parse PMT");
  pmtSize = 0;
}

bool cPatPmtParser::ParsePatPmt(const uchar *Data, int Length)
{
  while (Length >= TS_SIZE) {
        if (*Data != TS_SYNC_BYTE)
           break; // just for safety
        int Pid = TsPid(Data);
        if (Pid == PATPID)
           ParsePat(Data, TS_SIZE);
        else if (IsPmtPid(Pid)) {
           ParsePmt(Data, TS_SIZE);
           if (patVersion >= 0 && pmtVersion >= 0)
              return true;
           }
        Data += TS_SIZE;
        Length -= TS_SIZE;
        }
  return false;
}

bool cPatPmtParser::GetVersions(int &PatVersion, int &PmtVersion) const
{
  PatVersion = patVersion;
  PmtVersion = pmtVersion;
  return patVersion >= 0 && pmtVersion >= 0;
}

// --- cEitGenerator ---------------------------------------------------------

cEitGenerator::cEitGenerator(int Sid)
{
  counter = 0;
  version = 0;
  if (Sid)
     Generate(Sid);
}

uint16_t cEitGenerator::YMDtoMJD(int Y, int M, int D)
{
  int L = (M < 3) ? 1 : 0;
  return 14956 + D + int((Y - L) * 365.25) + int((M + 1 + L * 12) * 30.6001);
}

uchar *cEitGenerator::AddParentalRatingDescriptor(uchar *p, uchar ParentalRating)
{
  *p++ = SI::ParentalRatingDescriptorTag;
  *p++ = 0x04; // descriptor length
  *p++ = '9';  // country code "902" ("All countries") -> EN 300 468 / 6.2.28; www.dvbservices.com/country_codes/index.php
  *p++ = '0';
  *p++ = '2';
  *p++ = ParentalRating;
  return p;
}

uchar *cEitGenerator::Generate(int Sid)
{
  uchar *PayloadStart;
  uchar *SectionStart;
  uchar *DescriptorsStart;
  memset(eit, 0xFF, sizeof(eit));
  struct tm tm_r;
  time_t t = time(NULL) - 3600; // let's have the event start one hour in the past
  tm *tm = localtime_r(&t, &tm_r);
  uint16_t MJD = YMDtoMJD(tm->tm_year, tm->tm_mon + 1, tm->tm_mday);
  uchar *p = eit;
  // TS header:
  *p++ = TS_SYNC_BYTE;
  *p++ = TS_PAYLOAD_START;
  *p++ = EITPID;
  *p++ = 0x10 | (counter++ & 0x0F); // continuity counter
  *p++ = 0x00; // pointer field (payload unit start indicator is set)
  // payload:
  PayloadStart = p;
  *p++ = 0x4E; // TID present/following event on this transponder
  *p++ = 0xF0;
  *p++ = 0x00; // section length
  SectionStart = p;
  *p++ = Sid >> 8;
  *p++ = Sid & 0xFF;
  *p++ = 0xC1 | (version << 1);
  *p++ = 0x00; // section number
  *p++ = 0x00; // last section number
  *p++ = 0x00; // transport stream id
  *p++ = 0x00; // ...
  *p++ = 0x00; // original network id
  *p++ = 0x00; // ...
  *p++ = 0x00; // segment last section number
  *p++ = 0x4E; // last table id
  *p++ = 0x00; // event id
  *p++ = 0x01; // ...
  *p++ = MJD >> 8; // start time
  *p++ = MJD & 0xFF;  // ...
  *p++ = tm->tm_hour; // ...
  *p++ = tm->tm_min;  // ...
  *p++ = tm->tm_sec;  // ...
  *p++ = 0x24; // duration (one day, should cover everything)
  *p++ = 0x00; // ...
  *p++ = 0x00; // ...
  *p++ = 0x90; // running status, free/CA mode
  *p++ = 0x00; // descriptors loop length
  DescriptorsStart = p;
  p = AddParentalRatingDescriptor(p);
  // fill in lengths:
  *(SectionStart - 1) = p - SectionStart + 4; // +4 = length of CRC
  *(DescriptorsStart - 1) = p - DescriptorsStart;
  // checksum
  int crc = SI::CRC32::crc32((char *)PayloadStart, p - PayloadStart, 0xFFFFFFFF);
  *p++ = crc >> 24;
  *p++ = crc >> 16;
  *p++ = crc >> 8;
  *p++ = crc;
  return eit;
}

// --- cTsToPes --------------------------------------------------------------

cTsToPes::cTsToPes(void)
{
  data = NULL;
  size = 0;
  Reset();
}

cTsToPes::~cTsToPes()
{
  free(data);
}

void cTsToPes::PutTs(const uchar *Data, int Length)
{
  if (TsError(Data)) {
     Reset();
     return; // ignore packets with TEI set, and drop any PES data collected so far
     }
  if (TsPayloadStart(Data))
     Reset();
  else if (!size)
     return; // skip everything before the first payload start
  Length = TsGetPayload(&Data);
  if (length + Length > size) {
     int NewSize = max(KILOBYTE(2), length + Length);
     if (uchar *NewData = (uchar *)realloc(data, NewSize)) {
        data = NewData;
        size = NewSize;
        }
     else {
        esyslog("ERROR: out of memory");
        Reset();
        return;
        }
     }
  memcpy(data + length, Data, Length);
  length += Length;
}

#define MAXPESLENGTH 0xFFF0

const uchar *cTsToPes::GetPes(int &Length)
{
  if (repeatLast) {
     repeatLast = false;
     Length = lastLength;
     return lastData;
     }
  if (offset < length && PesLongEnough(length)) {
     if (!PesHasLength(data)) // this is a video PES packet with undefined length
        offset = 6; // trigger setting PES length for initial slice
     if (offset) {
        uchar *p = data + offset - 6;
        if (p != data) {
           p -= 3;
           if (p < data) {
              Reset();
              return NULL;
              }
           memmove(p, data, 4);
           }
        int l = min(length - offset, MAXPESLENGTH);
        offset += l;
        if (p != data) {
           l += 3;
           p[6]  = 0x80;
           p[7]  = 0x00;
           p[8]  = 0x00;
           }
        p[4] = l / 256;
        p[5] = l & 0xFF;
        Length = l + 6;
        lastLength = Length;
        lastData = p;
        return p;
        }
     else {
        Length = PesLength(data);
        if (Length <= length) {
           offset = Length; // to make sure we break out in case of garbage data
           lastLength = Length;
           lastData = data;
           return data;
           }
        }
     }
  return NULL;
}

void cTsToPes::SetRepeatLast(void)
{
  repeatLast = true;
}

void cTsToPes::Reset(void)
{
  length = offset = 0;
  lastData = NULL;
  lastLength = 0;
  repeatLast = false;
}

// --- Some helper functions for debugging -----------------------------------

void BlockDump(const char *Name, const u_char *Data, int Length)
{
  printf("--- %s\n", Name);
  for (int i = 0; i < Length; i++) {
      if (i && (i % 16) == 0)
         printf("\n");
      printf(" %02X", Data[i]);
      }
  printf("\n");
}

void TsDump(const char *Name, const u_char *Data, int Length)
{
  printf("%s: %04X", Name, Length);
  int n = min(Length, 20);
  for (int i = 0; i < n; i++)
      printf(" %02X", Data[i]);
  if (n < Length) {
     printf(" ...");
     n = max(n, Length - 10);
     for (n = max(n, Length - 10); n < Length; n++)
         printf(" %02X", Data[n]);
     }
  printf("\n");
}

void PesDump(const char *Name, const u_char *Data, int Length)
{
  TsDump(Name, Data, Length);
}

// --- cFrameParser ----------------------------------------------------------

class cFrameParser {
protected:
  bool debug;
  bool newFrame;
  bool independentFrame;
  int iFrameTemporalReferenceOffset;
public:
  cFrameParser(void);
  virtual ~cFrameParser() {};
  virtual int Parse(const uchar *Data, int Length, int Pid) = 0;
       ///< Parses the given Data, which is a sequence of Length bytes of TS packets.
       ///< The payload in the TS packets with the given Pid is searched for just
       ///< enough information to determine the beginning and type of the next video
       ///< frame.
       ///< Returns the number of bytes parsed. Upon return, the functions NewFrame()
       ///< and IndependentFrame() can be called to retrieve the required information.
  void SetDebug(bool Debug) { debug = Debug; }
  bool NewFrame(void) { return newFrame; }
  bool IndependentFrame(void) { return independentFrame; }
  int IFrameTemporalReferenceOffset(void) { return iFrameTemporalReferenceOffset; }
  };

cFrameParser::cFrameParser(void)
{
  debug = true;
  newFrame = false;
  independentFrame = false;
  iFrameTemporalReferenceOffset = 0;
}

// --- cAudioParser ----------------------------------------------------------

class cAudioParser : public cFrameParser {
public:
  cAudioParser(void);
  virtual int Parse(const uchar *Data, int Length, int Pid);
  };

cAudioParser::cAudioParser(void)
{
}

int cAudioParser::Parse(const uchar *Data, int Length, int Pid)
{
  if (TsPayloadStart(Data)) {
     newFrame = independentFrame = true;
     if (debug)
        dbgframes("/");
     }
  else
     newFrame = independentFrame = false;
  return TS_SIZE;
}

// --- cMpeg2Parser ----------------------------------------------------------

class cMpeg2Parser : public cFrameParser {
private:
  uint32_t scanner;
  bool seenIndependentFrame;
  int lastIFrameTemporalReference;
public:
  cMpeg2Parser(void);
  virtual int Parse(const uchar *Data, int Length, int Pid);
  };

cMpeg2Parser::cMpeg2Parser(void)
{
  scanner = EMPTY_SCANNER;
  seenIndependentFrame = false;
  lastIFrameTemporalReference = -1; // invalid
}

int cMpeg2Parser::Parse(const uchar *Data, int Length, int Pid)
{
  newFrame = independentFrame = false;
  bool SeenPayloadStart = false;
  cTsPayload tsPayload(const_cast<uchar *>(Data), Length, Pid);
  if (TsPayloadStart(Data)) {
     SeenPayloadStart = true;
     tsPayload.SkipPesHeader();
     scanner = EMPTY_SCANNER;
     if (debug && seenIndependentFrame)
        dbgframes("/");
     }
  uint32_t OldScanner = scanner; // need to remember it in case of multiple frames per payload
  for (;;) {
      if (!SeenPayloadStart && tsPayload.AtTsStart())
         OldScanner = scanner;
      scanner = (scanner << 8) | tsPayload.GetByte();
      if (scanner == 0x00000100) { // Picture Start Code
         if (!SeenPayloadStart && tsPayload.GetLastIndex() > TS_SIZE) {
            scanner = OldScanner;
            return tsPayload.Used() - TS_SIZE;
            }
         uchar b1 = tsPayload.GetByte();
         uchar b2 = tsPayload.GetByte();
         int TemporalReference = (b1 << 2 ) + ((b2 & 0xC0) >> 6);
         uchar FrameType = (b2 >> 3) & 0x07;
         if (tsPayload.Find(0x000001B5)) { // Extension start code
            if (((tsPayload.GetByte() & 0xF0) >> 4) == 0x08) { // Picture coding extension
               tsPayload.GetByte();
               uchar PictureStructure = tsPayload.GetByte() & 0x03;
               if (PictureStructure == 0x02) // bottom field
                  break;
               }
            }
         newFrame = true;
         independentFrame = FrameType == 1; // I-Frame
         if (independentFrame) {
            if (lastIFrameTemporalReference >= 0)
               iFrameTemporalReferenceOffset = TemporalReference - lastIFrameTemporalReference;
            lastIFrameTemporalReference = TemporalReference;
            }
         if (debug) {
            seenIndependentFrame |= independentFrame;
            if (seenIndependentFrame) {
               static const char FrameTypes[] = "?IPBD???";
               dbgframes("%c", FrameTypes[FrameType]);
               }
            }
         tsPayload.Statistics();
         break;
         }
      if (tsPayload.AtPayloadStart() // stop at a new payload start to have the buffer refilled if necessary
         || tsPayload.Eof()) // or if we're out of data
         break;
      }
  return tsPayload.Used();
}

// --- cH264Parser -----------------------------------------------------------

class cH264Parser : public cFrameParser {
private:
  enum eNalUnitType {
    nutCodedSliceNonIdr     = 1,
    nutCodedSliceIdr        = 5,
    nutSequenceParameterSet = 7,
    nutAccessUnitDelimiter  = 9,
    };
  uchar byte; // holds the current byte value in case of bitwise access
  int bit; // the bit index into the current byte (-1 if we're not in bit reading mode)
  int zeroBytes; // the number of consecutive zero bytes (to detect 0x000003)
  // Identifiers written in '_' notation as in "ITU-T H.264":
  bool separate_colour_plane_flag;
  int log2_max_frame_num;
  bool frame_mbs_only_flag;
protected:
  cTsPayload tsPayload;
  uint32_t scanner;
  bool gotAccessUnitDelimiter;
  bool gotSequenceParameterSet;
  uchar GetByte(bool Raw = false);
       ///< Gets the next data byte. If Raw is true, no filtering will be done.
       ///< With Raw set to false, if the byte sequence 0x000003 is encountered,
       ///< the byte with 0x03 will be skipped.
  uchar GetBit(void);
  uint32_t GetBits(int Bits);
  uint32_t GetGolombUe(void);
  int32_t GetGolombSe(void);
  void ParseAccessUnitDelimiter(void);
  void ParseSequenceParameterSet(void);
  void ParseSliceHeader(void);
public:
  cH264Parser(void);
       ///< Sets up a new H.264 parser.
       ///< This class parses only the data absolutely necessary to determine the
       ///< frame borders and field count of the given H264 material.
  virtual int Parse(const uchar *Data, int Length, int Pid);
  };

cH264Parser::cH264Parser(void)
{
  byte = 0;
  bit = -1;
  zeroBytes = 0;
  scanner = EMPTY_SCANNER;
  separate_colour_plane_flag = false;
  log2_max_frame_num = 0;
  frame_mbs_only_flag = false;
  gotAccessUnitDelimiter = false;
  gotSequenceParameterSet = false;
}

uchar cH264Parser::GetByte(bool Raw)
{
  uchar b = tsPayload.GetByte();
  if (!Raw) {
     // If we encounter the byte sequence 0x000003, we need to skip the 0x03:
     if (b == 0x00)
        zeroBytes++;
     else {
        if (b == 0x03 && zeroBytes >= 2)
           b = tsPayload.GetByte();
        zeroBytes = 0;
        }
     }
  else
     zeroBytes = 0;
  bit = -1;
  return b;
}

uchar cH264Parser::GetBit(void)
{
  if (bit < 0) {
     byte = GetByte();
     bit = 7;
     }
  return (byte & (1 << bit--)) ? 1 : 0;
}

uint32_t cH264Parser::GetBits(int Bits)
{
  uint32_t b = 0;
  while (Bits--)
        b |= GetBit() << Bits;
  return b;
}

uint32_t cH264Parser::GetGolombUe(void)
{
  int z = -1;
  for (int b = 0; !b && z < 32; z++) // limiting z to no get stuck if GetBit() always returns 0
      b = GetBit();
  return (1 << z) - 1 + GetBits(z);
}

int32_t cH264Parser::GetGolombSe(void)
{
  uint32_t v = GetGolombUe();
  if (v) {
     if ((v & 0x01) != 0)
        return (v + 1) / 2; // fails for v == 0xFFFFFFFF, but that will probably never happen
     else
        return -int32_t(v / 2);
     }
  return v;
}

int cH264Parser::Parse(const uchar *Data, int Length, int Pid)
{
  newFrame = independentFrame = false;
  tsPayload.Setup(const_cast<uchar *>(Data), Length, Pid);
  if (TsPayloadStart(Data)) {
     tsPayload.SkipPesHeader();
     scanner = EMPTY_SCANNER;
     if (debug && gotSequenceParameterSet) {
        dbgframes("/");
        }
     }
  for (;;) {
      scanner = (scanner << 8) | GetByte(true);
      if ((scanner & 0xFFFFFF00) == 0x00000100) { // NAL unit start
         uchar NalUnitType = scanner & 0x1F;
         switch (NalUnitType) {
           case nutAccessUnitDelimiter:  ParseAccessUnitDelimiter();
                                         gotAccessUnitDelimiter = true;
                                         break;
           case nutSequenceParameterSet: if (gotAccessUnitDelimiter) {
                                            ParseSequenceParameterSet();
                                            gotSequenceParameterSet = true;
                                            }
                                         break;
           case nutCodedSliceNonIdr:
           case nutCodedSliceIdr:        if (gotAccessUnitDelimiter && gotSequenceParameterSet) {
                                            ParseSliceHeader();
                                            gotAccessUnitDelimiter = false;
                                            if (newFrame)
                                               tsPayload.Statistics();
                                            return tsPayload.Used();
                                            }
                                         break;
           default: ;
           }
         }
      if (tsPayload.AtPayloadStart() // stop at a new payload start to have the buffer refilled if necessary
         || tsPayload.Eof()) // or if we're out of data
         break;
      }
  return tsPayload.Used();
}

void cH264Parser::ParseAccessUnitDelimiter(void)
{
  if (debug && gotSequenceParameterSet)
     dbgframes("A");
  GetByte(); // primary_pic_type
}

void cH264Parser::ParseSequenceParameterSet(void)
{
  uchar profile_idc = GetByte(); // profile_idc
  GetByte(); // constraint_set[0-5]_flags, reserved_zero_2bits
  GetByte(); // level_idc
  GetGolombUe(); // seq_parameter_set_id
  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 244 || profile_idc == 44 || profile_idc == 83 || profile_idc == 86 || profile_idc ==118 || profile_idc == 128) {
     int chroma_format_idc = GetGolombUe(); // chroma_format_idc
     if (chroma_format_idc == 3)
        separate_colour_plane_flag = GetBit();
     GetGolombUe(); // bit_depth_luma_minus8
     GetGolombUe(); // bit_depth_chroma_minus8
     GetBit(); // qpprime_y_zero_transform_bypass_flag
     if (GetBit()) { // seq_scaling_matrix_present_flag
        for (int i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); i++) {
            if (GetBit()) { // seq_scaling_list_present_flag
               int SizeOfScalingList = (i < 6) ? 16 : 64;
               int LastScale = 8;
               int NextScale = 8;
               for (int j = 0; j < SizeOfScalingList; j++) {
                   if (NextScale)
                      NextScale = (LastScale + GetGolombSe() + 256) % 256; // delta_scale
                   if (NextScale)
                      LastScale = NextScale;
                   }
               }
            }
        }
     }
  log2_max_frame_num = GetGolombUe() + 4; // log2_max_frame_num_minus4
  int pic_order_cnt_type = GetGolombUe(); // pic_order_cnt_type
  if (pic_order_cnt_type == 0)
     GetGolombUe(); // log2_max_pic_order_cnt_lsb_minus4
  else if (pic_order_cnt_type == 1) {
     GetBit(); // delta_pic_order_always_zero_flag
     GetGolombSe(); // offset_for_non_ref_pic
     GetGolombSe(); // offset_for_top_to_bottom_field
     for (int i = GetGolombUe(); i--; ) // num_ref_frames_in_pic_order_cnt_cycle
         GetGolombSe(); // offset_for_ref_frame
     }
  GetGolombUe(); // max_num_ref_frames
  GetBit(); // gaps_in_frame_num_value_allowed_flag
  GetGolombUe(); // pic_width_in_mbs_minus1
  GetGolombUe(); // pic_height_in_map_units_minus1
  frame_mbs_only_flag = GetBit(); // frame_mbs_only_flag
  if (debug) {
     if (gotAccessUnitDelimiter && !gotSequenceParameterSet)
        dbgframes("A"); // just for completeness
     dbgframes(frame_mbs_only_flag ? "S" : "s");
     }
}

void cH264Parser::ParseSliceHeader(void)
{
  newFrame = true;
  GetGolombUe(); // first_mb_in_slice
  int slice_type = GetGolombUe(); // slice_type, 0 = P, 1 = B, 2 = I, 3 = SP, 4 = SI
  independentFrame = (slice_type % 5) == 2;
  if (debug) {
     static const char SliceTypes[] = "PBIpi";
     dbgframes("%c", SliceTypes[slice_type % 5]);
     }
  if (frame_mbs_only_flag)
     return; // don't need the rest - a frame is complete
  GetGolombUe(); // pic_parameter_set_id
  if (separate_colour_plane_flag)
     GetBits(2); // colour_plane_id
  GetBits(log2_max_frame_num); // frame_num
  if (!frame_mbs_only_flag) {
     if (GetBit()) // field_pic_flag
        newFrame = !GetBit(); // bottom_field_flag
     if (debug)
        dbgframes(newFrame ? "t" : "b");
     }
}

// --- cH265Parser -----------------------------------------------------------

class cH265Parser : public cH264Parser {
private:
  enum eNalUnitType {
    nutSliceSegmentTrailingN =  0,
    nutSliceSegmentTrailingR =  1,
    nutSliceSegmentTSAN      =  2,
    nutSliceSegmentTSAR      =  3,
    nutSliceSegmentSTSAN     =  4,
    nutSliceSegmentSTSAR     =  5,
    nutSliceSegmentRADLN     =  6,
    nutSliceSegmentRADLR     =  7,
    nutSliceSegmentRASLN     =  8,
    nutSliceSegmentRASLR     =  9,
    nutSliceSegmentBLAWLP    = 16,
    nutSliceSegmentBLAWRADL  = 17,
    nutSliceSegmentBLANLP    = 18,
    nutSliceSegmentIDRWRADL  = 19,
    nutSliceSegmentIDRNLP    = 20,
    nutSliceSegmentCRANUT    = 21,
    nutVideoParameterSet     = 32,
    nutSequenceParameterSet  = 33,
    nutPictureParameterSet   = 34,
    nutAccessUnitDelimiter   = 35,
    nutEndOfSequence         = 36,
    nutEndOfBitstream        = 37,
    nutFillerData            = 38,
    nutPrefixSEI             = 39,
    nutSuffixSEI             = 40,
    nutNonVCLRes0            = 41,
    nutNonVCLRes3            = 44,
    nutUnspecified0          = 48,
    nutUnspecified7          = 55,
    };
public:
  cH265Parser(void);
  virtual int Parse(const uchar *Data, int Length, int Pid);
  };

cH265Parser::cH265Parser(void)
:cH264Parser()
{
}

int cH265Parser::Parse(const uchar *Data, int Length, int Pid)
{
  newFrame = independentFrame = false;
  tsPayload.Setup(const_cast<uchar *>(Data), Length, Pid);
  if (TsPayloadStart(Data)) {
     tsPayload.SkipPesHeader();
     scanner = EMPTY_SCANNER;
     }
  for (;;) {
      scanner = (scanner << 8) | GetByte(true);
      if ((scanner & 0xFFFFFF00) == 0x00000100) { // NAL unit start
         uchar NalUnitType = (scanner >> 1) & 0x3F;
         GetByte(); // nuh_layer_id + nuh_temporal_id_plus1
         if (NalUnitType <= nutSliceSegmentRASLR || (NalUnitType >= nutSliceSegmentBLAWLP && NalUnitType <= nutSliceSegmentCRANUT)) {
            if (NalUnitType == nutSliceSegmentIDRWRADL || NalUnitType == nutSliceSegmentIDRNLP || NalUnitType == nutSliceSegmentCRANUT)
               independentFrame = true;
            if (GetBit()) { // first_slice_segment_in_pic_flag
               newFrame = true;
               tsPayload.Statistics();
               }
            break;
            }
         }
      if (tsPayload.AtPayloadStart() // stop at a new payload start to have the buffer refilled if necessary
         || tsPayload.Eof()) // or if we're out of data
         break;
      }
  return tsPayload.Used();
}

// --- cFrameDetector --------------------------------------------------------

cFrameDetector::cFrameDetector(int Pid, int Type)
{
  parser = NULL;
  SetPid(Pid, Type);
  synced = false;
  newFrame = independentFrame = false;
  numPtsValues = 0;
  numIFrames = 0;
  framesPerSecond = 0;
  framesInPayloadUnit = framesPerPayloadUnit = 0;
  scanning = false;
}

static int CmpUint32(const void *p1, const void *p2)
{
  if (*(uint32_t *)p1 < *(uint32_t *)p2) return -1;
  if (*(uint32_t *)p1 > *(uint32_t *)p2) return  1;
  return 0;
}

void cFrameDetector::SetPid(int Pid, int Type)
{
  pid = Pid;
  type = Type;
  isVideo = type == 0x01 || type == 0x02 || type == 0x1B || type == 0x24; // MPEG 1, 2, H.264 or H.265
  delete parser;
  parser = NULL;
  if (type == 0x01 || type == 0x02)
     parser = new cMpeg2Parser;
  else if (type == 0x1B)
     parser = new cH264Parser;
  else if (type == 0x24)
     parser = new cH265Parser;
  else if (type == 0x03 || type == 0x04 || type == 0x06) // MPEG audio or AC3 audio
     parser = new cAudioParser;
  else if (type != 0)
     esyslog("ERROR: unknown stream type %d (PID %d) in frame detector", type, pid);
}

int cFrameDetector::Analyze(const uchar *Data, int Length)
{
  if (!parser)
     return 0;
  int Processed = 0;
  newFrame = independentFrame = false;
  while (Length >= MIN_TS_PACKETS_FOR_FRAME_DETECTOR * TS_SIZE) { // makes sure we are looking at enough data, in case the frame type is not stored in the first TS packet
        // Sync on TS packet borders:
        if (int Skipped = TS_SYNC(Data, Length))
           return Processed + Skipped;
        // Handle one TS packet:
        int Handled = TS_SIZE;
        if (TsHasPayload(Data) && !TsIsScrambled(Data)) {
           int Pid = TsPid(Data);
           if (Pid == pid) {
              if (Processed)
                 return Processed;
              if (TsPayloadStart(Data))
                 scanning = true;
              if (scanning) {
                 // Detect the beginning of a new frame:
                 if (TsPayloadStart(Data)) {
                    if (!framesPerPayloadUnit)
                       framesPerPayloadUnit = framesInPayloadUnit;
                    }
                 int n = parser->Parse(Data, Length, pid);
                 if (n > 0) {
                    if (parser->NewFrame()) {
                       newFrame = true;
                       independentFrame = parser->IndependentFrame();
                       if (synced) {
                          if (framesPerPayloadUnit <= 1)
                             scanning = false;
                          }
                       else {
                          framesInPayloadUnit++;
                          if (independentFrame)
                             numIFrames++;
                          }
                       }
                    Handled = n;
                    }
                 }
              if (TsPayloadStart(Data)) {
                 // Determine the frame rate from the PTS values in the PES headers:
                 if (framesPerSecond <= 0.0) {
                    // frame rate unknown, so collect a sequence of PTS values:
                    if (numPtsValues < 2 || numPtsValues < MaxPtsValues && numIFrames < 2) { // collect a sequence containing at least two I-frames
                       if (newFrame) { // only take PTS values at the beginning of a frame (in case if fields!)
                          const uchar *Pes = Data + TsPayloadOffset(Data);
                          if (numIFrames && PesHasPts(Pes)) {
                             ptsValues[numPtsValues] = PesGetPts(Pes);
                             // check for rollover:
                             if (numPtsValues && ptsValues[numPtsValues - 1] > 0xF0000000 && ptsValues[numPtsValues] < 0x10000000) {
                                dbgframes("#");
                                numPtsValues = 0;
                                numIFrames = 0;
                                }
                             else
                                numPtsValues++;
                             }
                          }
                       }
                    if (numPtsValues >= 2 && numIFrames >= 2) {
                       // find the smallest PTS delta:
                       qsort(ptsValues, numPtsValues, sizeof(uint32_t), CmpUint32);
                       numPtsValues--;
                       for (int i = 0; i < numPtsValues; i++)
                           ptsValues[i] = ptsValues[i + 1] - ptsValues[i];
                       qsort(ptsValues, numPtsValues, sizeof(uint32_t), CmpUint32);
                       int Div = framesPerPayloadUnit;
                       if (framesPerPayloadUnit > 1)
                          Div += parser->IFrameTemporalReferenceOffset();
                       if (Div <= 0)
                          Div = 1;
                       int Delta = ptsValues[0] / Div;
                       // determine frame info:
                       if (isVideo) {
                          if (Delta == 3753)
                             framesPerSecond = 24.0 / 1.001;
                          else if (abs(Delta - 3600) <= 1)
                             framesPerSecond = 25.0;
                          else if (Delta % 3003 == 0)
                             framesPerSecond = 30.0 / 1.001;
                          else if (abs(Delta - 1800) <= 1)
                             framesPerSecond = 50.0;
                          else if (Delta == 1501)
                             framesPerSecond = 60.0 / 1.001;
                          else {
                             framesPerSecond = DEFAULTFRAMESPERSECOND;
                             dsyslog("unknown frame delta (%d), assuming %5.2f fps", Delta, DEFAULTFRAMESPERSECOND);
                             }
                          }
                       else // audio
                          framesPerSecond = double(PTSTICKS) / Delta; // PTS of audio frames is always increasing
                       dbgframes("\nDelta = %d  FPS = %5.2f  FPPU = %d NF = %d TRO = %d\n", Delta, framesPerSecond, framesPerPayloadUnit, numPtsValues + 1, parser->IFrameTemporalReferenceOffset());
                       synced = true;
                       parser->SetDebug(false);
                       }
                    }
                 }
              }
           else if (Pid == PATPID && synced && Processed)
              return Processed; // allow the caller to see any PAT packets
           }
        Data += Handled;
        Length -= Handled;
        Processed += Handled;
        if (newFrame)
           break;
        }
  return Processed;
}
