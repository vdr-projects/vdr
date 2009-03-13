/*
 * remux.h: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.c 2.14 2009/03/08 12:12:17 kls Exp $
 */

#include "remux.h"
#include "device.h"
#include "libsi/si.h"
#include "libsi/section.h"
#include "libsi/descriptor.h"
#include "shutdown.h"
#include "tools.h"

// Set these to 'true' for debug output:
static bool DebugPatPmt = false;
static bool DebugFrames = false;

#define dbgpatpmt(a...) if (DebugPatPmt) fprintf(stderr, a)
#define dbgframes(a...) if (DebugFrames) fprintf(stderr, a)

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

// --- cPatPmtGenerator ------------------------------------------------------

cPatPmtGenerator::cPatPmtGenerator(cChannel *Channel)
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

int cPatPmtGenerator::MakeAC3Descriptor(uchar *Target)
{
  int i = 0;
  Target[i++] = SI::AC3DescriptorTag;
  Target[i++] = 0x01; // length
  Target[i++] = 0x00;
  IncEsInfoLength(i);
  return i;
}

int cPatPmtGenerator::MakeSubtitlingDescriptor(uchar *Target, const char *Language)
{
  int i = 0;
  Target[i++] = SI::SubtitlingDescriptorTag;
  Target[i++] = 0x08; // length
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = 0x00; // subtitling type
  Target[i++] = 0x00; // composition page id hi
  Target[i++] = 0x01; // composition page id lo
  Target[i++] = 0x00; // ancillary page id hi
  Target[i++] = 0x01; // ancillary page id lo
  IncEsInfoLength(i);
  return i;
}

int cPatPmtGenerator::MakeLanguageDescriptor(uchar *Target, const char *Language)
{
  int i = 0;
  Target[i++] = SI::ISO639LanguageDescriptorTag;
  Target[i++] = 0x04; // length
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = *Language++;
  Target[i++] = 0x01; // audio type
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

void cPatPmtGenerator::GeneratePmtPid(cChannel *Channel)
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
  p[i++] = TS_PAYLOAD_START; // flags (3), pid hi (5)
  p[i++] = 0x00; // pid lo
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
  pat[SectionLength] = i - SectionLength - 1 + 4; // -2 = SectionLength storage, +4 = length of CRC
  MakeCRC(pat + i, pat + PayloadStart, i - PayloadStart);
  IncVersion(patVersion);
}

void cPatPmtGenerator::GeneratePmt(cChannel *Channel)
{
  // generate the complete PMT section:
  uchar buf[MAX_SECTION_SIZE];
  memset(buf, 0xFF, sizeof(buf));
  numPmtPackets = 0;
  if (Channel) {
     int Vpid = Channel->Vpid();
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
     p[i++] = 0xE0 | (Vpid >> 8); // dummy (3), PCR pid hi (5)
     p[i++] = Vpid; // PCR pid lo
     p[i++] = 0xF0; // dummy (4), program info length hi (4)
     p[i++] = 0x00; // program info length lo

     if (Vpid)
        i += MakeStream(buf + i, Channel->Vtype(), Vpid);
     for (int n = 0; Channel->Apid(n); n++) {
         i += MakeStream(buf + i, 0x04, Channel->Apid(n));
         const char *Alang = Channel->Alang(n);
         i += MakeLanguageDescriptor(buf + i, Alang);
         if (Alang[3] == '+')
            i += MakeLanguageDescriptor(buf + i, Alang + 3);
         }
     for (int n = 0; Channel->Dpid(n); n++) {
         i += MakeStream(buf + i, 0x06, Channel->Dpid(n));
         i += MakeAC3Descriptor(buf + i);
         i += MakeLanguageDescriptor(buf + i, Channel->Dlang(n));
         }
     for (int n = 0; Channel->Spid(n); n++) {
         i += MakeStream(buf + i, 0x06, Channel->Spid(n));
         i += MakeSubtitlingDescriptor(buf + i, Channel->Slang(n));
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

void cPatPmtGenerator::SetChannel(cChannel *Channel)
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

cPatPmtParser::cPatPmtParser(void)
{
  Reset();
}

void cPatPmtParser::Reset(void)
{
  pmtSize = 0;
  patVersion = pmtVersion = -1;
  pmtPid = -1;
  vpid = vtype = 0;
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
     SI::PAT::Association assoc;
     for (SI::Loop::Iterator it; Pat.associationLoop.getNext(assoc, it); ) {
         dbgpatpmt("     isNITPid = %d\n", assoc.isNITPid());
         if (!assoc.isNITPid()) {
            pmtPid = assoc.getPid();
            dbgpatpmt("     service id = %d, pid = %d\n", assoc.getServiceId(), assoc.getPid());
            }
         }
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
     cDevice::PrimaryDevice()->ClrAvailableTracks(false, true);
     int NumApids = 0;
     int NumDpids = 0;
     int NumSpids = 0;
     vpid = vtype = 0;
     SI::PMT::Stream stream;
     for (SI::Loop::Iterator it; Pmt.streamLoop.getNext(stream, it); ) {
         dbgpatpmt("     stream type = %02X, pid = %d", stream.getStreamType(), stream.getPid());
         switch (stream.getStreamType()) {
           case 0x02: // STREAMTYPE_13818_VIDEO
           case 0x1B: // MPEG4
                      vpid = stream.getPid();
                      vtype = stream.getStreamType();
                      break;
           case 0x04: // STREAMTYPE_13818_AUDIO
                      {
                      if (NumApids < MAXAPIDS) {
                         char ALangs[MAXLANGCODE2] = "";
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    SI::ISO639LanguageDescriptor::Language l;
                                    char *s = ALangs;
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
                         cDevice::PrimaryDevice()->SetAvailableTrack(ttAudio, NumApids, stream.getPid(), ALangs);
                         NumApids++;
                         }
                      }
                      break;
           case 0x06: // STREAMTYPE_13818_PES_PRIVATE
                      {
                      int dpid = 0;
                      char lang[MAXLANGCODE1] = "";
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::AC3DescriptorTag:
                                 dbgpatpmt(" AC3");
                                 dpid = stream.getPid();
                                 break;
                            case SI::SubtitlingDescriptorTag:
                                 dbgpatpmt(" subtitling");
                                 if (NumSpids < MAXSPIDS) {
                                    SI::SubtitlingDescriptor *sd = (SI::SubtitlingDescriptor *)d;
                                    SI::SubtitlingDescriptor::Subtitling sub;
                                    char SLangs[MAXLANGCODE2] = "";
                                    char *s = SLangs;
                                    int n = 0;
                                    for (SI::Loop::Iterator it; sd->subtitlingLoop.getNext(sub, it); ) {
                                        if (sub.languageCode[0]) {
                                           dbgpatpmt(" '%s'", sub.languageCode);
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(sub.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    cDevice::PrimaryDevice()->SetAvailableTrack(ttSubtitle, NumSpids, stream.getPid(), SLangs);
                                    NumSpids++;
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
                            cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, NumDpids, dpid, lang);
                            NumDpids++;
                            }
                         }
                      }
                      break;
           }
         dbgpatpmt("\n");
         cDevice::PrimaryDevice()->EnsureAudioTrack(true);
         cDevice::PrimaryDevice()->EnsureSubtitleTrack();
         }
     pmtVersion = Pmt.getVersionNumber();
     }
  else
     esyslog("ERROR: can't parse PMT");
  pmtSize = 0;
}

// --- cTsToPes --------------------------------------------------------------

cTsToPes::cTsToPes(void)
{
  data = NULL;
  size = length = offset = 0;
  synced = false;
}

cTsToPes::~cTsToPes()
{
  free(data);
}

void cTsToPes::PutTs(const uchar *Data, int Length)
{
  if (TsPayloadStart(Data))
     Reset();
  else if (!size)
     return; // skip everything before the first payload start
  Length = TsGetPayload(&Data);
  if (length + Length > size) {
     size = max(KILOBYTE(2), length + Length);
     data = (uchar *)realloc(data, size);
     }
  memcpy(data + length, Data, Length);
  length += Length;
}

#define MAXPESLENGTH 0xFFF0

const uchar *cTsToPes::GetPes(int &Length)
{
  if (offset < length && PesLongEnough(length)) {
     if (!PesHasLength(data)) // this is a video PES packet with undefined length
        offset = 6; // trigger setting PES length for initial slice
     if (offset) {
        uchar *p = data + offset - 6;
        if (p != data) {
           p -= 3;
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
        return p;
        }
     else {
        Length = PesLength(data);
        if (Length <= length) {
           offset = Length; // to make sure we break out in case of garbage data
           return data;
           }
        }
     }
  return NULL;
}

void cTsToPes::Reset(void)
{
  length = offset = 0;
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

// --- cFrameDetector --------------------------------------------------------

cFrameDetector::cFrameDetector(int Pid, int Type)
{
  pid = Pid;
  type = Type;
  newFrame = independentFrame = false;
  lastPts = 0;
  isVideo = type == 0x02 || type == 0x1B; // MPEG 2 or MPEG 4
  frameDuration = 0;
  framesPerPayloadUnit = 0;
  scanning = false;
  scanner = 0;
}

int cFrameDetector::Analyze(const uchar *Data, int Length)
{
  newFrame = independentFrame = false;
  if (Length >= TS_SIZE) {
     if (TsHasPayload(Data) && !TsIsScrambled(Data) && TsPid(Data) == pid) {
        if (TsPayloadStart(Data)) {
           if (!frameDuration) {
              const uchar *Pes = Data + TsPayloadOffset(Data);
              if (PesHasPts(Pes)) {
                 int64_t Pts = PesGetPts(Pes);
                 if (Pts < lastPts) { // avoid wrapping
                    lastPts = 0;
                    framesPerPayloadUnit = 0;
                    }
                 if ((!lastPts || !framesPerPayloadUnit) && Pts != lastPts)
                    lastPts = Pts;
                 else {
                    int64_t Delta = Pts - lastPts;
                    if (isVideo) {
                       if (Delta % 3600 == 0)
                          frameDuration = 3600; // PAL, 25 fps
                       else if (Delta % 3003 == 0)
                          frameDuration = 3003; // NTSC, 29.97 fps
                       else {
                          frameDuration = 3600; // unknown, assuming 25 fps
                          dsyslog("unknown frame duration, assuming 25 fps (PTS: %lld - %lld = %lld  FPPU = %d)\n", Pts, lastPts, Delta, framesPerPayloadUnit);
                          }
                       }
                    else // audio
                       frameDuration = Delta; // PTS of audio frames is always increasing
                    dbgframes("PTS: %lld - %lld = %lld -> FD = %d  FPS = %5.2f  FPPU = %d\n", Pts, lastPts, Delta, frameDuration, 90000.0 / frameDuration, framesPerPayloadUnit);
                    }
                 }
              }
           scanner = 0;
           scanning = true;
           }
        if (scanning) {
           int PayloadOffset = TsPayloadOffset(Data);
           if (TsPayloadStart(Data))
              PayloadOffset += PesPayloadOffset(Data + PayloadOffset);
           for (int i = PayloadOffset; i < TS_SIZE; i++) {
               scanner <<= 8;
               scanner |= Data[i];
               switch (type) {
                 case 0x02: // MPEG 2 video
                      if (scanner == 0x00000100) { // Picture Start Code
                         if (frameDuration) {
                            newFrame = true;
                            independentFrame = ((Data[i + 2] >> 3) & 0x07) == 1; // I-Frame
                            if (framesPerPayloadUnit == 1) {
                               scanning = false;
                               return TS_SIZE;
                               }
                            }
                         else {
                            framesPerPayloadUnit++;
                            dbgframes("%d ", (Data[i + 2] >> 3) & 0x07);
                            }
                         scanner = 0;
                         }
                      break;
                 case 0x1B: // MPEG 4 video
                      if (scanner == 0x00000109) { // Access Unit Delimiter
                         if (frameDuration) {
                            newFrame = true;
                            independentFrame = Data[i + 1] == 0x10;
                            if (framesPerPayloadUnit == 1) {
                               scanning = false;
                               return TS_SIZE;
                               }
                            }
                         else {
                            framesPerPayloadUnit++;
                            dbgframes("%02X ", Data[i + 1]);
                            }
                         scanner = 0;
                         }
                      break;
                 case 0x04: // MPEG audio
                 case 0x06: // AC3 audio
                      if (frameDuration) {
                         newFrame = true;
                         independentFrame = true;
                         scanning = false;
                         }
                      else
                         framesPerPayloadUnit = 1;
                      break;
                 default: esyslog("ERROR: unknown stream type %d (PID %d) in frame detector", type, pid);
                          pid = 0; // let's just ignore any further data
                 }
               }
           }
        }
     return TS_SIZE;
     }
  return 0;
}
