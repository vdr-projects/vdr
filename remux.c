/*
 * remux.c: Tools for detecting frames and handling PAT/PMT
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remux.c 2.64 2012/03/02 10:56:49 kls Exp $
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

void TsSetTeiOnBrokenPackets(uchar *p, int l)
{
  bool Processed[MAXPID] = { false };
  while (l >= TS_SIZE) {
        if (*p != TS_SYNC_BYTE)
           break;
        int Pid = TsPid(p);
        if (!Processed[Pid]) {
           if (!TsPayloadStart(p))
              p[1] |= TS_ERROR;
           else {
              Processed[Pid] = true;
              int offs = TsPayloadOffset(p);
              cRemux::SetBrokenLink(p + offs, TS_SIZE - offs);
              }
           }
        l -= TS_SIZE;
        p += TS_SIZE;
        }
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
  pat[SectionLength] = i - SectionLength - 1 + 4; // -2 = SectionLength storage, +4 = length of CRC
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
  pmtSize = 0;
  patVersion = pmtVersion = -1;
  pmtPid = -1;
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
           case 0x1B: // MPEG4
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
                         apids[NumApids]= 0;
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
                                    spids[NumSpids]= 0;
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
                            dpids[NumDpids]= 0;
                            }
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
     }
  else
     esyslog("ERROR: can't parse PMT");
  pmtSize = 0;
}

bool cPatPmtParser::GetVersions(int &PatVersion, int &PmtVersion) const
{
  PatVersion = patVersion;
  PmtVersion = pmtVersion;
  return patVersion >= 0 && pmtVersion >= 0;
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

// --- cFrameDetector --------------------------------------------------------

#define EMPTY_SCANNER (0xFFFFFFFF)

cFrameDetector::cFrameDetector(int Pid, int Type)
{
  SetPid(Pid, Type);
  synced = false;
  newFrame = independentFrame = false;
  numPtsValues = 0;
  numFrames = 0;
  numIFrames = 0;
  framesPerSecond = 0;
  framesInPayloadUnit = framesPerPayloadUnit = 0;
  payloadUnitOfFrame = 0;
  scanning = false;
  scanner = EMPTY_SCANNER;
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
  isVideo = type == 0x01 || type == 0x02 || type == 0x1B; // MPEG 1, 2 or 4
}

void cFrameDetector::Reset(void)
{
  newFrame = independentFrame = false;
  payloadUnitOfFrame = 0;
  scanning = false;
  scanner = EMPTY_SCANNER;
}

int cFrameDetector::SkipPackets(const uchar *&Data, int &Length, int &Processed, int &FrameTypeOffset)
{
  if (!synced)
     dbgframes("%d>", FrameTypeOffset);
  while (Length >= TS_SIZE) {
        // switch to the next TS packet, but skip those that have a different PID:
        Data += TS_SIZE;
        Length -= TS_SIZE;
        Processed += TS_SIZE;
        if (TsPid(Data) == pid)
           break;
        else if (Length < TS_SIZE)
           esyslog("ERROR: out of data while skipping TS packets in cFrameDetector");
        }
  FrameTypeOffset -= TS_SIZE;
  FrameTypeOffset += TsPayloadOffset(Data);
  return FrameTypeOffset;
}

int cFrameDetector::Analyze(const uchar *Data, int Length)
{
  int SeenPayloadStart = false;
  int Processed = 0;
  newFrame = independentFrame = false;
  while (Length >= TS_SIZE) {
        if (Data[0] != TS_SYNC_BYTE) {
           int Skipped = 1;
           while (Skipped < Length && (Data[Skipped] != TS_SYNC_BYTE || Length - Skipped > TS_SIZE && Data[Skipped + TS_SIZE] != TS_SYNC_BYTE))
                 Skipped++;
           esyslog("ERROR: skipped %d bytes to sync on start of TS packet", Skipped);
           return Processed + Skipped;
           }
        if (TsHasPayload(Data) && !TsIsScrambled(Data)) {
           int Pid = TsPid(Data);
           if (Pid == pid) {
              if (TsPayloadStart(Data)) {
                 SeenPayloadStart = true;
                 if (synced && Processed)
                    return Processed;
                 if (Length < MIN_TS_PACKETS_FOR_FRAME_DETECTOR * TS_SIZE)
                    return Processed; // need more data, in case the frame type is not stored in the first TS packet
                 if (framesPerSecond <= 0.0) {
                    // frame rate unknown, so collect a sequence of PTS values:
                    if (numPtsValues < 2 || numPtsValues < MaxPtsValues && numIFrames < 2) { // collect a sequence containing at least two I-frames
                       const uchar *Pes = Data + TsPayloadOffset(Data);
                       if (numIFrames && PesHasPts(Pes)) {
                          ptsValues[numPtsValues] = PesGetPts(Pes);
                          // check for rollover:
                          if (numPtsValues && ptsValues[numPtsValues - 1] > 0xF0000000 && ptsValues[numPtsValues] < 0x10000000) {
                             dbgframes("#");
                             numPtsValues = 0;
                             numIFrames = 0;
                             numFrames = 0;
                             }
                          else
                             numPtsValues++;
                          }
                       }
                    else {
                       // find the smallest PTS delta:
                       qsort(ptsValues, numPtsValues, sizeof(uint32_t), CmpUint32);
                       numPtsValues--;
                       for (int i = 0; i < numPtsValues; i++)
                           ptsValues[i] = ptsValues[i + 1] - ptsValues[i];
                       qsort(ptsValues, numPtsValues, sizeof(uint32_t), CmpUint32);
                       uint32_t Delta = ptsValues[0];
                       // determine frame info:
                       if (isVideo) {
                          if (abs(Delta - 3600) <= 1)
                             framesPerSecond = 25.0;
                          else if (Delta % 3003 == 0)
                             framesPerSecond = 30.0 / 1.001;
                          else if (abs(Delta - 1800) <= 1) {
                             if (numFrames > 50) {
                                // this is a "best guess": if there are more than 50 frames between two I-frames, we assume each "frame" actually contains a "field", so two "fields" make one "frame"
                                framesPerSecond = 25.0;
                                framesPerPayloadUnit = -2;
                                }
                             else
                                framesPerSecond = 50.0;
                             }
                          else if (Delta == 1501)
                             if (numFrames > 50) {
                                // this is a "best guess": if there are more than 50 frames between two I-frames, we assume each "frame" actually contains a "field", so two "fields" make one "frame"
                                framesPerSecond = 30.0 / 1.001;
                                framesPerPayloadUnit = -2;
                                }
                             else
                                framesPerSecond = 60.0 / 1.001;
                          else {
                             framesPerSecond = DEFAULTFRAMESPERSECOND;
                             dsyslog("unknown frame delta (%d), assuming %5.2f fps", Delta, DEFAULTFRAMESPERSECOND);
                             }
                          }
                       else // audio
                          framesPerSecond = 90000.0 / Delta; // PTS of audio frames is always increasing
                       dbgframes("\nDelta = %d  FPS = %5.2f  FPPU = %d NF = %d\n", Delta, framesPerSecond, framesPerPayloadUnit, numFrames);
                       }
                    }
                 scanner = EMPTY_SCANNER;
                 scanning = true;
                 }
              if (scanning) {
                 int PayloadOffset = TsPayloadOffset(Data);
                 if (TsPayloadStart(Data)) {
                    PayloadOffset += PesPayloadOffset(Data + PayloadOffset);
                    if (!framesPerPayloadUnit)
                       framesPerPayloadUnit = framesInPayloadUnit;
                    if (DebugFrames && !synced)
                       dbgframes("/");
                    }
                 for (int i = PayloadOffset; scanning && i < TS_SIZE; i++) {
                     scanner <<= 8;
                     scanner |= Data[i];
                     switch (type) {
                       case 0x01: // MPEG 1 video
                       case 0x02: // MPEG 2 video
                            if (scanner == 0x00000100) { // Picture Start Code
                               scanner = EMPTY_SCANNER;
                               if (synced && !SeenPayloadStart && Processed)
                                  return Processed; // flush everything before this new frame
                               int FrameTypeOffset = i + 2;
                               if (FrameTypeOffset >= TS_SIZE) // the byte to check is in the next TS packet
                                  i = SkipPackets(Data, Length, Processed, FrameTypeOffset);
                               newFrame = true;
                               uchar FrameType = (Data[FrameTypeOffset] >> 3) & 0x07;
                               independentFrame = FrameType == 1; // I-Frame
                               if (synced) {
                                  if (framesPerPayloadUnit <= 1)
                                     scanning = false;
                                  }
                               else {
                                  framesInPayloadUnit++;
                                  if (independentFrame)
                                     numIFrames++;
                                  if (numIFrames == 1)
                                     numFrames++;
                                  dbgframes("%u ", FrameType);
                                  }
                               if (synced)
                                  return Processed + TS_SIZE; // flag this new frame
                               }
                            break;
                       case 0x1B: // MPEG 4 video
                            if (scanner == 0x00000109) { // Access Unit Delimiter
                               scanner = EMPTY_SCANNER;
                               if (synced && !SeenPayloadStart && Processed)
                                  return Processed; // flush everything before this new frame
                               int FrameTypeOffset = i + 1;
                               if (FrameTypeOffset >= TS_SIZE) // the byte to check is in the next TS packet
                                  i = SkipPackets(Data, Length, Processed, FrameTypeOffset);
                               newFrame = true;
                               uchar FrameType = Data[FrameTypeOffset];
                               independentFrame = FrameType == 0x10;
                               if (synced) {
                                  if (framesPerPayloadUnit < 0) {
                                     payloadUnitOfFrame = (payloadUnitOfFrame + 1) % -framesPerPayloadUnit;
                                     if (payloadUnitOfFrame != 0 && independentFrame)
                                        payloadUnitOfFrame = 0;
                                     if (payloadUnitOfFrame)
                                        newFrame = false;
                                     }
                                  if (framesPerPayloadUnit <= 1)
                                     scanning = false;
                                  }
                               else {
                                  framesInPayloadUnit++;
                                  if (independentFrame)
                                     numIFrames++;
                                  if (numIFrames == 1)
                                     numFrames++;
                                  dbgframes("%02X ", FrameType);
                                  }
                               if (synced)
                                  return Processed + TS_SIZE; // flag this new frame
                               }
                            break;
                       case 0x04: // MPEG audio
                       case 0x06: // AC3 audio
                            if (synced && Processed)
                               return Processed;
                            newFrame = true;
                            independentFrame = true;
                            if (!synced) {
                               framesInPayloadUnit = 1;
                               if (TsPayloadStart(Data))
                                  numIFrames++;
                               }
                            scanning = false;
                            break;
                       default: esyslog("ERROR: unknown stream type %d (PID %d) in frame detector", type, pid);
                                pid = 0; // let's just ignore any further data
                       }
                     }
                 if (!synced && framesPerSecond > 0.0 && independentFrame) {
                    synced = true;
                    dbgframes("*\n");
                    Reset();
                    return Processed + TS_SIZE;
                    }
                 }
              }
           else if (Pid == PATPID && synced && Processed)
              return Processed; // allow the caller to see any PAT packets
           }
        Data += TS_SIZE;
        Length -= TS_SIZE;
        Processed += TS_SIZE;
        }
  return Processed;
}
