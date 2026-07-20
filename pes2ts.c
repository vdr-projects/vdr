/*
 * pes2ts.c: Convert legacy PES recordings to TS
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: pes2ts.c 1.1 2026/07/20 08:25:33 kls Exp $
 */

#include "pes2ts.h"
#include "config.h"
#include "recording.h"
#include "tools.h"

// --- Helpers ---------------------------------------------------------------

#define USE __attribute__((warn_unused_result)) // marks functions where the retrun value must be checked

static cString LogFileName;

#define LOGFILESUFFIX  "/pes2ts.log"

static void SetLogFileName(const char *FileName)
{
  LogFileName = AddDirectory(FileName, LOGFILESUFFIX);
  unlink(LogFileName);
}

static void Log(const char *s)
{
  if (FILE *f = fopen(LogFileName, "a")) {
     fputs(s, f);
     fputc('\n', f);
     fclose(f);
     }
}

static void Info(int Level, const char *Fmt, ...) __attribute__ ((format (printf, 2, 3)));
static void Info(int Level, const char *Fmt, ...)
{
  va_list ap;
  va_start(ap, Fmt);
  cString s = cString::vsprintf(Fmt, ap);
  if (Level > 1)
     s = cString::sprintf("%*s%s", (Level - 1) * 3, "", *s);
  va_end(ap);
  if (Level <= SysLogLevel) {
     fputs(s, stdout);
     fputc('\n', stdout);
     }
  Log(s);
}

static void Warning(const char *Fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void Warning(const char *Fmt, ...)
{
  va_list ap;
  va_start(ap, Fmt);
  cString s = cString::vsprintf(Fmt, ap);
  s = cString::sprintf("WARNING: %s", *s);
  va_end(ap);
  fputs(s, stdout);
  fputc('\n', stdout);
  Log(s);
}

static bool Error(const char *Fmt, ...) __attribute__ ((format (printf, 1, 2)));
static bool Error(const char *Fmt, ...)
{
  va_list ap;
  va_start(ap, Fmt);
  cString s = cString::vsprintf(Fmt, ap);
  s = cString::sprintf("ERROR: %s", *s);
  va_end(ap);
  fputs(s, stderr);
  fputc('\n', stderr);
  Log(s);
  return false; // convenience return value
}

// --- CopyMarks -------------------------------------------------------------

#define MARKSFILESUFFIX   "/marks"

static USE bool CopyMarks(const char *PesFileName, const char *TsFileName)
{
  cString PesMarksFileName = AddDirectory(PesFileName, MARKSFILESUFFIX ".vdr");
  cString TsMarksFileName = AddDirectory(TsFileName, MARKSFILESUFFIX);
  if (FILE *fp = fopen(PesMarksFileName, "r")) {
     Info(2, "copying PES marks file %s to %s", *PesMarksFileName, *TsMarksFileName);
     if (FILE *ft = fopen(TsMarksFileName, "w")) {
        cReadLine ReadLine;
        while (char *s = ReadLine.Read(fp)) {
              fprintf(ft, "%s\n", s);
              Info(3, "%s", s);
              }
        fclose(ft);
        }
     else if (errno != ENOENT) {
        fclose(fp);
        return Error("%s: %m", *TsMarksFileName);
        }
     fclose(fp);
     }
  else if (errno != ENOENT)
     return Error("%s: %m", *PesMarksFileName);
  return true; // a missing marks file is not an error
}

// --- cRecordingTimerId -----------------------------------------------------

// Creates a recording timer id to make sure replay of the resulting TS recording can
// begin immediately, and makes sure the id is removed when finished.

class cRecordingTimerId {
private:
  cString fileName;
public:
  cRecordingTimerId(const char *FileName);
  ~cRecordingTimerId();
  };

cRecordingTimerId::cRecordingTimerId(const char *FileName)
{
  fileName = FileName;
  SetRecordingTimerId(fileName, cString::sprintf("%d@%s", 0, Setup.SVDRPHostName));
}

cRecordingTimerId::~cRecordingTimerId()
{
  SetRecordingTimerId(fileName, NULL);
}

// --- cPesIndex -------------------------------------------------------------

// Stripped down version of cIndexFile, just enough to read a PES index file.

struct __attribute__((packed)) tIndexPes {
  uint32_t offset;
  uchar type;
  uchar number;
  uint16_t reserved;
  };

class cPesIndex {
private:
  cString fileName;
  tIndexPes *index;
  int last;
public:
  cPesIndex(void);
  ~cPesIndex();
  USE bool Load(const char *FileName);
  USE bool IsValid(int Index) { return Index >= 0 && Index <= last; }
  USE bool Get(int Index, uchar &FileNumber, uint32_t &FileOffset, bool &Independent, int &Length);
  };

#define INDEXFILESUFFIX  "/index.vdr"

cPesIndex::cPesIndex(void)
{
  index = NULL;
  last = -1;
}

bool cPesIndex::Load(const char *FileName)
{
  fileName = AddDirectory(FileName, INDEXFILESUFFIX);
  if (access(fileName, R_OK) == 0) {
     struct stat buf;
     if (stat(fileName, &buf) == 0) {
        int delta = int(buf.st_size % sizeof(tIndexPes));
        if (delta) {
           delta = sizeof(tIndexPes) - delta;
           Warning("invalid file size (%u) in '%s', continuing anyway", uint32_t(buf.st_size), *fileName);
           // no 'return false' here
           }
        last = int((buf.st_size + delta) / sizeof(tIndexPes) - 1);
        if (last >= 0) {
           int size = last + 1;
           index = MALLOC(tIndexPes, size);
           if (index) {
              int f = open(fileName, O_RDONLY);
              if (f >= 0) {
                 if (safe_read(f, index, size_t(buf.st_size)) != buf.st_size) {
                    Error("can't read from file '%s'", *fileName);
                    free(index);
                    last = -1;
                    index = NULL;
                    }
                 close(f);
                 }
              else
                 Error("%s: %m", *fileName);
              }
           else
              Error("can't allocate %zd bytes for index '%s'", size * sizeof(tIndexPes), *fileName);
           }
        else
           Error("%s: improper file size", *fileName);
        }
     else
        Error("%s: %m", *fileName);
     }
  else
     Error("missing index file %s", *fileName);
  return index != NULL;
}

cPesIndex::~cPesIndex()
{
  free(index);
}

bool cPesIndex::Get(int Index, uchar &FileNumber, uint32_t &FileOffset, bool &Independent, int &Length)
{
  if (IsValid(Index)) {
     FileNumber = index[Index].number;
     FileOffset = index[Index].offset;
     Independent = index[Index].type == 1;
     if (Index < last) {
        uchar fn = index[Index + 1].number;
        uint32_t fo = index[Index + 1].offset;
        if (fn == FileNumber)
           Length = int(fo - FileOffset);
        else
           Length = -1; // this means "everything up to EOF"
        }
     else
        Length = -1;
     return true;
     }
  return false;
}

// --- cPesResume ------------------------------------------------------------

// Stripped down version of cResumeFile, just enough to read a PES resume file.

class cPesResume {
private:
  int index;
public:
  cPesResume(void);
  USE bool Read(const char *FileName);
  USE bool Write(const char *TsFileName);
  };

#define RESUMEFILESUFFIX  "/resume"

cPesResume::cPesResume(void)
{
  index = -1;
}

bool cPesResume::Read(const char *FileName)
{
  cString PesResumeFileName = AddDirectory(FileName, RESUMEFILESUFFIX ".vdr");
  int f = open(PesResumeFileName, O_RDONLY);
  if (f >= 0) {
     Info(2, "reading PES resume file %s", *PesResumeFileName);
     int Read = safe_read(f, &index, sizeof(index));
     close(f);
     if (Read != sizeof(index)) {
        index = -1;
        return Error("%s: %m", *PesResumeFileName);
        }
     Info(3, "index = %d", index);
     }
  else if (errno != ENOENT)
     return Error("%s: %m", *PesResumeFileName);
  return true; // a missing resume file is not an error
}

bool cPesResume::Write(const char *TsFileName)
{
  if (index >= 0) {
     cString TsResumeFileName = AddDirectory(TsFileName, RESUMEFILESUFFIX);
     Info(2, "writing TS resume file %s", *TsResumeFileName);
     FILE *f = fopen(TsResumeFileName, "w");
     if (f) {
        fprintf(f, "I %d\n", index);
        Info(3, "I %d", index);
        fclose(f);
        }
     else
        return Error("%s: %m", *TsResumeFileName);
     }
  return true;
}

// --- cPesSummary -----------------------------------------------------------

#define SUMMARYFILESUFFIX "/summary.vdr"

class cPesSummary {
private:
  cString data[3];
public:
  USE bool Read(const char *FileName);
  const char *Title(void) { return data[0]; }
  const char *ShortText(void) { return data[1]; }
  const char *Description(void) { return data[2]; }
  };

bool cPesSummary::Read(const char *FileName)
{
  cString SummaryFileName = AddDirectory(FileName, SUMMARYFILESUFFIX);
  if (FILE *f = fopen(SummaryFileName, "r")) {
     Info(2, "reading PES summary file %s", *SummaryFileName);
     int line = 0;
     cReadLine ReadLine;
     while (char *s = ReadLine.Read(f)) {
           if (*s) {
              if (*data[line]) {
                 data[line].Append("|");
                 data[line].Append(s);
                 }
              else
                 data[line] = s;
              if (line < 2)
                 line++;
              }
           }
     fclose(f);
     if (*data[1] && strlen(data[1]) > 80) {
        // If line 1 is too long, it can't be the short text,
        // so assume the short text is missing and concatenate
        // line 1 and line 2 to be the description:
        if (*data[2]) {
           data[1].Append("|");
           data[1].Append(data[2]);
           }
        data[2] = data[1];
        data[1] = NULL;
        }
     Info(3, "Title       = '%s'", *data[0]);
     Info(3, "Short text  = '%s'", *data[1]);
     Info(3, "Description = '%s'", *data[2]);
     }
  else if (errno != ENOENT)
     return Error("%s: %m", *SummaryFileName);
  return true; // a missing summary file is not an error
}

// --- cPesInfo --------------------------------------------------------------

class cPesInfo {
private:
  cStringList info;
  cComponents components;
public:
  USE bool Read(const char *FileName);
  USE bool Write(const char *TsFileName);
  cComponents *Components(void) { return &components;}
  };

#define INFOFILESUFFIX  "/info"

bool cPesInfo::Read(const char *FileName)
{
  cString InfoFileName = AddDirectory(FileName, INFOFILESUFFIX ".vdr");
  if (access(InfoFileName, R_OK) == 0) {
     Info(2, "reading PES info file %s", *InfoFileName);
     if (FILE *f = fopen(InfoFileName, "r")) {
        cReadLine ReadLine;
        while (char *s = ReadLine.Read(f)) {
              if (*s == 'X') {
                 int i = components.NumComponents();
                 components.SetComponent(i, skipspace(s + 1));
                 if (!*components.Component(i)->language) {
                    // This is a dummy component, inserted to handle an offset in the PES file.
                    // We need it during the conversion from PES to TS, but not in the final TS info file.
                    Info(3, "%s - skipped!", s);
                    continue;
                    }
                 }
              info.Append(strdup(s));
              Info(3, "%s", s);
              }
        fclose(f);
        }
     else
        return Error("%s: %m", *InfoFileName);
     }
  else {
     cPesSummary PesSummary;
     if (PesSummary.Read(FileName)) {
        if (PesSummary.Title())
           info.Append(strdup(cString::sprintf("T %s", PesSummary.Title())));
        if (PesSummary.ShortText())
           info.Append(strdup(cString::sprintf("S %s", PesSummary.ShortText())));
        if (PesSummary.Description())
           info.Append(strdup(cString::sprintf("D %s", PesSummary.Description())));
        }
     else
        return false;
     }
  Info(2, "taking priority and lifetime from PES info file name %s", *InfoFileName);
  int Lifetime = MAXLIFETIME;
  int Priority = MAXPRIORITY;
  int n = 0;
  int len = strlen(FileName);
  const char *p = FileName + len - 1;
  while (len-- > 0) {
        if (*p == '.') {
           n++;
           if (n == 2)
              Lifetime = atoi(p + 1);
           else if (n == 3) {
              Priority = atoi(p + 1);
              break;
              }
           }
        p--;
        }
  info.Append(strdup(cString::sprintf("L %d", Lifetime)));
  info.Append(strdup(cString::sprintf("P %d", Priority)));
  Info(3, "priority = %d", Priority);
  Info(3, "lifetime = %d", Lifetime);
  return true;
}

bool cPesInfo::Write(const char *TsFileName)
{
  cString InfoFileName = AddDirectory(TsFileName, INFOFILESUFFIX);
  Info(2, "writing TS info file %s", *InfoFileName);
  if (FILE *f = fopen(InfoFileName, "w")) {
     for (int i = 0; i < info.Size(); i++) {
         fprintf(f, "%s\n", info[i]);
         Info(3, "%s", info[i]);
         }
     fclose(f);
     return true;
     }
  return Error("%s: %m", *InfoFileName);
}

// --- cPesFile --------------------------------------------------------------

class cPesFile {
private:
  cString fileName;
  int number;
  int file;
  uint32_t size;
  uchar frame[MAXFRAMESIZE];
public:
  cPesFile(void);
  ~cPesFile();
  USE bool Init(const char *FileName);
      // Initializes the PES file handler.
      // Returns false in case of an error.
  USE bool Open(int Number);
      // Opens the PES recording file with the given number.
      // Returns false in case of an error.
  void Close(void);
      // Closes the currently open PES file.
  USE const uchar *Read(int Number, uint32_t Offset, int Length, int &Bytes);
      // Reads Length bytes at Offset from the PES file with the given Number.
      // Returns a pointer to the read bytes. Bytes returns the number of
      // bytes actually read, which must normally be equal to Length, but
      // is the remaining number of bytes in the PES file if Length is -1.
      // Returns NULL in case of an error.
  };

#define MAXFILESPERRECORDINGPES 255
#define RECORDFILESUFFIXPES     "/%03d.vdr"

cPesFile::cPesFile(void)
{
  number = 0;
  file = -1;
  size = 0;
}

cPesFile::~cPesFile()
{
  Close();
}

bool cPesFile::Init(const char *FileName)
{
  fileName = FileName;
  return true;
}

bool cPesFile::Open(int Number)
{
  if (number == Number)
     return true;
  Close();
  if (0 < Number && Number <= MAXFILESPERRECORDINGPES) {
     cString Name = cString::sprintf("%s" RECORDFILESUFFIXPES, *fileName, Number);
     if (access(Name, R_OK) == 0) {
        file = open(Name, O_RDONLY | O_LARGEFILE);
        if (file >= 0) {
           size = FileSize(Name);
           return true;
           }
        }
     Error("%s: %m", *Name);
     }
  else
     Error("invalid file number '%d'", Number);
  return false;
}

void cPesFile::Close(void)
{
  if (file >= 0) {
     close(file);
     file = -1;
     }
}

const uchar *cPesFile::Read(int Number, uint32_t Offset, int Length, int &Bytes)
{
  if (Length < 0)
     Length = size - Offset;
  if (Length <= MAXFRAMESIZE) {
     if (Open(Number)) {
        if (Offset >= 0 && lseek(file, Offset, SEEK_SET) == Offset) {
           Bytes = safe_read(file, frame, Length);
           if (Bytes == Length)
              return frame;
           Error("invalid length '%d', only read '%d' bytes", Length, Bytes);
           }
        else
           Error("invalid offset '%u'", Offset);
        }
     else
        Error("invalid number '%d'", Number);
     }
  else
     Error("invalid length '%d'", Length);
  return NULL;
}

// --- cPesReader ------------------------------------------------------------

class cPesReader {
private:
  cPesIndex index;
  cPesFile file;
public:
  USE bool Init(const char *FileName);
      // Initializes this PES reader.
  USE bool HasData(int Index) { return index.IsValid(Index); }
      // Returns true if data exists for the given Index.
  const uchar *ReadPesFrame(int Index, bool &Independent, int &Bytes);
      // Reads one complete PES frame at the given Index.
      // Returns a pointer to the read bytes. Bytes returns the number of
      // bytes actually read. Independent is true if this in an I-frame.
      // Returns NULL in case of an error.
  };

bool cPesReader::Init(const char *FileName)
{
  return file.Init(FileName) && index.Load(FileName);
}

const uchar *cPesReader::ReadPesFrame(int Index, bool &Independent, int &Bytes)
{
  uchar FileNumber;
  uint32_t FileOffset;
  int Length;
  if (index.Get(Index, FileNumber, FileOffset, Independent, Length))
     return file.Read(FileNumber, FileOffset, Length, Bytes);
  return NULL;
}

// --- cPesSubtitleAssembler -------------------------------------------------

#define MAX_PES_PACKET_SIZE   (0xFFFF + 6) // 6 = PES header size

class cPesSubtitleAssembler {
private:
  uchar *data;
  int size;
  int length;
  uint16_t compositionPageId;
  bool complete;
public:
  cPesSubtitleAssembler(void);
  ~cPesSubtitleAssembler();
  USE bool Put(const uchar *Data, int Length);
  const uchar *Get(int &Length);
  uint16_t CompositionPageId(void) { return compositionPageId; }
  };

cPesSubtitleAssembler::cPesSubtitleAssembler(void)
{
  size = MAX_PES_PACKET_SIZE;
  data = MALLOC(uchar, size);
  length = 0;
  compositionPageId = 0;
  complete = false;
}

cPesSubtitleAssembler::~cPesSubtitleAssembler()
{
  free(data);
}

bool cPesSubtitleAssembler::Put(const uchar *Data, int Length)
{
  if (Data && PesLongEnough(Length)) {
     int PayloadOffset = PesPayloadOffset(Data);
     int SubstreamHeaderLength = 4;
     bool ResetSubtitleAssembler = Data[PayloadOffset + 3] == 0x00;

     // Compatibility mode for old subtitles plugin:
     if ((Data[7] & 0x01) && (Data[PayloadOffset - 3] & 0x81) == 0x01 && Data[PayloadOffset - 2] == 0x81) {
        SubstreamHeaderLength = 0;
        ResetSubtitleAssembler = Data[8] >= 5; // has PTS
        }

     if (ResetSubtitleAssembler)
        length = 0;

     if (length == 0) {
        // First packet, take header:
        memcpy(data, Data, PayloadOffset);
        length += PayloadOffset;
        }
     Data += PayloadOffset + SubstreamHeaderLength;
     Length -= (PayloadOffset + SubstreamHeaderLength);
     if (length + Length > size)
        return Error("packet too large in subtitle assembler");
     if (compositionPageId == 0) {
        for (int i = 0; i < Length; i++) {
            if (Data[i] == 0x0F && i + 3 < Length) {
               compositionPageId = (Data[i + 2] << 8) | Data[i + 3];
               break;
               }
            }
        }
     memcpy(data + length, Data, Length);
     length += Length;
     uint16_t NewLength = length - 6;
     data[4] = NewLength >> 8;
     data[5] = NewLength & 0xFF;
     // Check for end of packet
     uchar *p = data + length - 1;
     if (Length < 2048 && *p == 0xFF)
        complete = true;
     if (*p-- == 0xFF && *p-- == 0x00 && *p-- == 0x00 && *p-- == (compositionPageId & 0xFF) && *p-- == (compositionPageId >> 8) && *p-- == 0x80 && *p-- == 0x0F)
        complete = true;
     return true;
     }
  return Error("no data for subtitle assembler");
}

const uchar *cPesSubtitleAssembler::Get(int &Length)
{
  if (complete && length > 0) {
     Length = length;
     length = 0;
     complete = false;
     return data;
     }
  return NULL;
}

// --- cTsGenerator ----------------------------------------------------------

class cTsGenerator {
private:
  int pid;
  uchar counter;
public:
  cTsGenerator(int Pid);
      // Sets up a TS generator for the given Pid.
  int GenerateTs(const uchar *Data, int Length, bool Pusi, uchar *Dest);
      // Generates one TS packet from the first bytes in Data, and writes the result
      // (which is always exactly TS_SIZE bytes in size) to Dest. If Pusi is true, the
      // "payload unit start indicator" flag will be set.
      // Returns the number of bytes used from Data.
  };

cTsGenerator::cTsGenerator(int Pid)
{
  pid = Pid;
  counter = 0;
}

int cTsGenerator::GenerateTs(const uchar *Data, int Length, bool Pusi, uchar *Dest)
{
  uchar *p = Dest;
  *p++ = TS_SYNC_BYTE;                               // TS indicator
  *p++ = (Pusi ? TS_PAYLOAD_START : 0) | (pid >> 8); // flags (3), pid hi (5)
  *p++ = pid & 0xFF;                                 // pid low
  int Payload = TS_SIZE - 4;                         // maximum payload after TS header
  int Fill = Payload - Length;
  if (Fill > 0) {
     // Last TS packet, use adaptation field for stuffing:
     *p++ = 0x30 | counter; // adaptation field + payload
     if (Fill == 1) // minimal adaptation field, only length byte
        *p++ = 0x00;     // adaptation_field_length = 0
     else { // normal adaptation field with stuffing
        *p++ = Fill - 1; // adaptation_field_length
        *p++ = 0x00;     // adaptation flags
        if (Fill > 2)
           memset(p, 0xFF, Fill - 2); // stuffing bytes
        p += Fill - 2;
        }
     // Copy remaining PES data into payload
     memcpy(p, Data, Length);
     Payload = Length;
     }
  else {
     *p++ = 0x10 | counter; // payload only
     memcpy(p, Data, Payload);
     }
  counter = (counter + 1) & 0x0F;
  return Payload; // number of PES bytes written into this TS packet
}

// --- cPatPmtMaker ----------------------------------------------------------

#define PID_BASE_VIDEO  101
#define PID_BASE_AUDIO  201
#define PID_BASE_DOLBY  301
#define PID_BASE_SUBT   401
#define PID_BASE_LCPM   501

class cPatPmtMaker : public cPatPmtGenerator {
private:
  int vpid;
  int vtype;
  int ppid;
  cVector<int> apids;
  cVector<int> atypes;
  char alangs[MAXAPIDS][MAXLANGCODE2];
  cVector<int> dpids;
  cVector<int> dtypes;
  char dlangs[MAXDPIDS][MAXLANGCODE2];
  cVector<int> spids;
  cVector<uchar> stypes;
  char slangs[MAXSPIDS][MAXLANGCODE2];
  cVector<uint16_t> compositionPageIds; // also used for ancillaryPageIds
  cVector<bool> usedPids;
  cComponents *components;
public:
  cPatPmtMaker(cComponents *Components);
  void ResetPids(void);
  USE bool ListPids(void);
  void MakePatPmt(void);
  void SetVpid(int Vpid, int Vtype);
  void SetApid(int Apid, int Atype, int Index);
  void SetDpid(int Dpid, int Dtype, int Index);
  void SetSpid(int Spid, uchar Stype, uint16_t CompositionPageId, int Index);
  };

cPatPmtMaker::cPatPmtMaker(cComponents *Components)
{
  components = Components;
  ResetPids();
}

void cPatPmtMaker::ResetPids(void)
{
  vpid  = 0;
  vtype = 0;
  ppid  = 0;
  apids.Clear();
  atypes.Clear();
  memset(alangs, 0, sizeof(alangs));
  dpids.Clear();
  dtypes.Clear();
  memset(dlangs, 0, sizeof(dlangs));
  spids.Clear();
  stypes.Clear();
  memset(slangs, 0, sizeof(slangs));
  compositionPageIds.Clear();
  usedPids.Clear();
}

bool cPatPmtMaker::ListPids(void)
{
  bool HasPids = false;
  if (vpid) {
     Info(3, "vpid = %d %02X", vpid, vtype);
     HasPids = true;
     }
  for (int i = 0; apids[i]; i++) {
      Info(3, "apid[%d] = %d %02X '%s'", i, apids[i], atypes[i], alangs[i]);
      HasPids = true;
      }
  for (int i = 0; dpids[i]; i++) {
      Info(3, "dpid[%d] = %d %02X '%s'", i, dpids[i], dtypes[i], dlangs[i]);
      HasPids = true;
      }
  for (int i = 0; spids[i]; i++) {
      Info(3, "spid[%d] = %d %02X '%s' %d", i, spids[i], stypes[i], slangs[i], compositionPageIds[i]);
      HasPids = true;
      }
  if (!HasPids)
     return Error("no PIDs found");
  return true;
}

void cPatPmtMaker::MakePatPmt(void)
{
  SetPids(vpid, vtype, ppid, 0, apids.Data(), atypes.Data(), alangs, dpids.Data(), dtypes.Data(), dlangs, spids.Data(), stypes.Data(), slangs, compositionPageIds.Data(), compositionPageIds.Data());
}

void cPatPmtMaker::SetVpid(int Vpid, int Vtype)
{
  if (vpid != Vpid) {
     vpid  = Vpid;
     vtype = Vtype;
     ppid  = vpid;
     MakePatPmt();
     }
}

template<class T> int AppendZeroTerminated(cVector<T> &Pids, T Pid)
{
  int i = Pids.Size();
  if (i > 0)
     i--;
  Pids[i] = Pid;
  Pids[i + 1] = 0;
  return i;
}

void cPatPmtMaker::SetApid(int Apid, int Atype, int Index)
{
  if (!usedPids[Apid]) {
     int i = AppendZeroTerminated(apids, Apid);
     atypes.Append(Atype);
     if (tComponent *Component = components->GetComponent(Index, 2, 0))
        strn0cpy(alangs[i], Component->language, MAXLANGCODE2);
     MakePatPmt();
     usedPids[Apid] = true;
     }
}

void cPatPmtMaker::SetDpid(int Dpid, int Dtype, int Index)
{
  if (!usedPids[Dpid]) {
     int i = AppendZeroTerminated(dpids, Dpid);
     dtypes.Append(Dtype);
     if (tComponent *Component = components->GetComponent(Index, 2, 0))
        strn0cpy(dlangs[i], Component->language, MAXLANGCODE2);
     MakePatPmt();
     usedPids[Dpid] = true;
     }
}

void cPatPmtMaker::SetSpid(int Spid, uchar Stype, uint16_t CompositionPageId, int Index)
{
  if (!usedPids[Spid]) {
     int i = AppendZeroTerminated(spids, Spid);
     stypes.Append(Stype);
     if (tComponent *Component = components->GetComponent(Index, 3, 0))
        strn0cpy(slangs[i], Component->language, MAXLANGCODE2);
     compositionPageIds.Append(CompositionPageId);
     MakePatPmt();
     usedPids[Spid] = true;
     }
}

// --- cPes2TsConverter ------------------------------------------------------

#define MIN_PRE_1_3_19_PRIVATESTREAM 10 // the minimum number of unknown PS1 packets to consider this a "pre 1.3.19 private stream"

#define MAX_SUBSTREAMS  32
#define MAX_PIDS        (PID_BASE_LCPM + MAX_SUBSTREAMS)

class cPes2TsConverter {
private:
  int pre_1_3_19_PrivateStream;
  cPatPmtMaker patPmtMaker;
  cTsGenerator *tsGenerators[MAX_PIDS];
  cPesSubtitleAssembler *pesSubtitleAssemblers[MAX_SUBSTREAMS];
  uchar frame[MAXFRAMESIZE];
  int length;
  bool analyzing;
  USE bool MakeTs(const uchar *Data, int Length, int Pid);
      // Converts the PES packet in Data to a sequence of TS packets for Pid and stores them
      // in the member variable frame.
      // Returns false in case of an error.
  USE bool ProcessPesPacket(const uchar *Data, int Length);
      // Processes the PES packet in Data.
      // Returns false in case of an error.
public:
  cPes2TsConverter(cComponents *Components);
      // Components contains the data of the stream components, if available.
  ~cPes2TsConverter();
  bool IsPre_1_3_19_PrivateStream(void) { return pre_1_3_19_PrivateStream > MIN_PRE_1_3_19_PRIVATESTREAM; }
  void SetAnalyzing(bool Analyzing) { analyzing = Analyzing; }
      // Sets this converter to analyzing mode.
  void ResetPids(void);
      // Resets any PIDs detected so far.
  USE bool ListPids(void);
      // Lists all PIDs detected so far.
      // Returns true is any PIDs have been found,
  USE bool ConvertPesFrame(const uchar *Data, int Length, bool Independent);
      // Converts the complete PES frame in Data into a TS frame.
      // If Independent is true, this is an I-frame.
      // Returns false in case of an error.
  const uchar *TsFrame(int &Length) { Length = length; return frame; }
      // Returns the resulting TS frame and its Length.
      // The result is only valid if the previous call to ConvertPesFrame() has returned true.
  };

cPes2TsConverter::cPes2TsConverter(cComponents *Components)
:patPmtMaker(Components)
{
  pre_1_3_19_PrivateStream = 0;
  memset(tsGenerators, 0, sizeof(tsGenerators));
  memset(pesSubtitleAssemblers, 0, sizeof(pesSubtitleAssemblers));
  memset(frame, 0, sizeof(frame));
  length = 0;
  analyzing = false;
}

cPes2TsConverter::~cPes2TsConverter()
{
  for (int i = 0; i < MAX_PIDS; i++)
      delete tsGenerators[i];
  for (int i = 0; i < MAX_SUBSTREAMS; i++)
      delete pesSubtitleAssemblers[i];
}

void cPes2TsConverter::ResetPids(void)
{
  patPmtMaker.ResetPids();
}

bool cPes2TsConverter::ListPids(void)
{
  return patPmtMaker.ListPids();
}

bool cPes2TsConverter::MakeTs(const uchar *Data, int Length, int Pid)
{
  if (0 <= Pid && Pid < MAX_PIDS) {
     cTsGenerator *TsGenerator = tsGenerators[Pid];
     if (!TsGenerator)
        TsGenerator = tsGenerators[Pid] = new cTsGenerator(Pid);
     uchar *p = frame + length;
     bool First = true;
     while (Length > 0) {
           if (length + TS_SIZE > MAXFRAMESIZE)
              return Error("frame size exceeded");
           int w = TsGenerator->GenerateTs(Data, Length, First, p);
           if (w > 0) {
              Data += w;
              Length -= w;
              length += TS_SIZE;
              p += TS_SIZE;
              }
           else
              return false;
           First = false;
           }
     }
  else
     return Error("invalid PID '%d'", Pid);
  return true;
}

bool cPes2TsConverter::ProcessPesPacket(const uchar *Data, int Length)
{
  uchar c = Data[3];
  switch (c) {
    case 0xBE:            // padding stream, needed for MPEG1
         break;
    case 0xE0 ... 0xEF: { // video
         int Vpid = PID_BASE_VIDEO + c - 0xE0;
         patPmtMaker.SetVpid(Vpid, 0x02); // assuming STREAMTYPE_13818_VIDEO
         if (!MakeTs(Data, Length, Vpid))
            return false;
         break;
         }
    case 0xC0 ... 0xDF: { // audio
         int Index =  c - 0xC0;
         int Apid = PID_BASE_AUDIO + Index;
         patPmtMaker.SetApid(Apid, 0x04, Index); // assuming STREAMTYPE_13818_AUDIO
         if (!MakeTs(Data, Length, Apid))
            return false;
         break;
         }
    case 0xBD: { // private stream 1
         int PayloadOffset = Data[8] + 9;

         // Compatibility mode for old subtitles plugin:
         if ((Data[7] & 0x01) && (Data[PayloadOffset - 3] & 0x81) == 0x01 && Data[PayloadOffset - 2] == 0x81)
            PayloadOffset--;

         uchar SubStreamId = Data[PayloadOffset];
         uchar SubStreamType  = SubStreamId & 0xF0; // deliberately overlapping masks:
         uchar SubStreamIndex = SubStreamId & 0x1F; // 0x20..0x3F become subtitle indices 0..31.

         // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
pre_1_3_19_PrivateStreamDetected:
         if (pre_1_3_19_PrivateStream > MIN_PRE_1_3_19_PRIVATESTREAM) {
            SubStreamId = c;
            SubStreamType = 0x80;
            SubStreamIndex = 0;
            }
         else if (pre_1_3_19_PrivateStream)
            pre_1_3_19_PrivateStream--; // every known PS1 packet counts down towards 0 to recover from glitches...

         switch (SubStreamType) {
           case 0x20:   // SPU
           case 0x30: { // SPU
                int Spid = PID_BASE_SUBT + SubStreamIndex;
                cPesSubtitleAssembler *PesSubtitleAssembler = pesSubtitleAssemblers[SubStreamIndex];
                if (!PesSubtitleAssembler)
                   PesSubtitleAssembler = pesSubtitleAssemblers[SubStreamIndex] = new cPesSubtitleAssembler;
                if (!PesSubtitleAssembler->Put(Data, Length))
                   return false;
                Data = PesSubtitleAssembler->Get(Length);
                if (Data && !MakeTs(Data, Length, Spid))
                   return false;
                uint16_t CompositionPageId = PesSubtitleAssembler->CompositionPageId();
                patPmtMaker.SetSpid(Spid, SubStreamType, CompositionPageId, SubStreamIndex);
                break;
                }
           case 0x80: { // AC3 & DTS
                uchar buf[Length];
                const uchar *p = Data + PayloadOffset;
                if (*p++ == 0x80 && *p++ == 0x01 && *p++ == 0x00 && *p++ == 0x01) {
                   // remove VDR's AC-3 header:
                   memcpy(buf, Data, PayloadOffset);
                   memcpy(buf + PayloadOffset, Data + PayloadOffset + 4, Length - (PayloadOffset + 4));
                   Length -= 4;
                   int PacketLength = Length - 6;
                   buf[4] = PacketLength >> 8;
                   buf[5] = PacketLength & 0xFF;
                   Data = buf;
                   }
                int Dpid = PID_BASE_DOLBY + SubStreamIndex;
                patPmtMaker.SetDpid(Dpid, 0x6A, SubStreamIndex);
                if (!MakeTs(Data, Length, Dpid))
                   return false;
                break;
                }
           case 0xA0: { // LPCM
                if (!analyzing) {
                   Warning("don't know what to do with LPCM");
                   analyzing = true; // quick way of making sure this message is issued only once
                   }
                /*
                int Apid = PID_BASE_LCPM + SubStreamIndex;
                patPmtMaker.SetApid(Apid, 0x04, SubStreamIndex);
                if (!MakeTs(Data, Length, Apid))
                   return false;
                */
                break;
                }
           default:
                // Compatibility mode for old VDR recordings, where 0xBD was only AC3:
                if (pre_1_3_19_PrivateStream <= MIN_PRE_1_3_19_PRIVATESTREAM) {
                   pre_1_3_19_PrivateStream += 2; // ...and every unknown PS1 packet counts up (the very first one counts twice, but that's ok)
                   if (pre_1_3_19_PrivateStream > MIN_PRE_1_3_19_PRIVATESTREAM) {
                      pre_1_3_19_PrivateStream = MIN_PRE_1_3_19_PRIVATESTREAM + 1;
                      goto pre_1_3_19_PrivateStreamDetected;
                      }
                   }
           }
         }
         break;
    default:
         return Error("unexpected packet id %02X", c);
    }
  return true;
}

bool cPes2TsConverter::ConvertPesFrame(const uchar *Data, int Length, bool Independent)
{
  length = 0;
  if (Independent)
     length += patPmtMaker.GetPatPmt(frame, sizeof(frame));
  int i = 0;
  while (i <= Length - 6) {
        if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01) {
           int l = PesLength(Data + i);
           if (i + l > Length)
              return Error("incomplete PES packet");
           if (!ProcessPesPacket(Data + i, l))
              return false;
           i += l;
           }
        else {
           Warning("syncing on PES data!");
           i++;
           }
        }
  if (i < Length)
     return Error("leftover PES data");
  return true;
}

// --- cTsWriter -------------------------------------------------------------

class cTsWriter {
private:
  cString tsRecordingFileName;
  int f;
  off_t offset;
  cIndexFile *index;
public:
  cTsWriter(void);
  ~cTsWriter();
  USE bool Open(const char *FileName);
  USE bool Write(const uchar *Data, int Length, bool Independent);
  };

cTsWriter::cTsWriter(void)
{
  f = -1;
  offset = 0;
  index = NULL;
}

bool cTsWriter::Open(const char *FileName)
{
  cString TsIndexFileName = cIndexFile::IndexFileName(FileName, false);
  if (access(TsIndexFileName, R_OK) == 0) {
     Info(2, "deleting existing TS index file %s", *TsIndexFileName);
     if (unlink(TsIndexFileName) < 0)
        return Error("%s: %m", *TsIndexFileName);
     }
  tsRecordingFileName = AddDirectory(FileName, "00001.ts");
  Info(2, "generating TS recording file %s", *tsRecordingFileName);
  f = open(tsRecordingFileName, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE);
  if (f > 0) {
     Info(2, "generating TS index file %s", *TsIndexFileName);
     index = new cIndexFile(FileName, true);
     }
  else
     return Error("%s: %m", *tsRecordingFileName);
  return true;
}

cTsWriter::~cTsWriter()
{
  close(f);
  delete index;
}

bool cTsWriter::Write(const uchar *Data, int Length, bool Independent)
{
  int Written = write(f, Data, Length);
  if (Written < 0)
     return Error("%s: %m", *tsRecordingFileName);
  if (index->Write(Independent, 1, offset)) {
     offset += Length;
     return true;
     }
  return false;
}

// --- Pes2Ts ----------------------------------------------------------------

#define NUM_FRAMES_ANALYZE 100

bool Pes2Ts(const char *PesFileName, const char *TsFileName)
{
  if (!DirectoryOk(PesFileName))
     return Error("'%s' is not a directory", PesFileName);
  if (!DirectoryOk(TsFileName))
     return Error("'%s' is not a directory", TsFileName);
  char p1[PATH_MAX];
  char p2[PATH_MAX];
  if (realpath(PesFileName, p1) && realpath(TsFileName, p2)) {
     if (strcmp(p1, p2) == 0)
        return Error("PES and TS file names must be different");
     }
  else
     return Error("%m");
  SetLogFileName(TsFileName);
  Info(1, "processing PES recording %s", PesFileName);
  cPesInfo PesInfo;
  if (!PesInfo.Read(PesFileName))
     return false;
  if (!PesInfo.Write(TsFileName))
     return false;
  if (!CopyMarks(PesFileName, TsFileName))
     return false;
  cPesResume PesResume;
  if (!PesResume.Read(PesFileName))
     return false;
  if (!PesResume.Write(TsFileName))
     return false;
  cPesReader PesReader;
  if (!PesReader.Init(PesFileName))
     return false;
  cPes2TsConverter Pes2TsConverter(PesInfo.Components());

  Info(2, "analyzing PES recording");
  Pes2TsConverter.SetAnalyzing(true);
  for (int Index = 0; Index < NUM_FRAMES_ANALYZE; Index++) {
      if (PesReader.HasData(Index)) {
         bool Independent = false;
         int Bytes = 0;
         if (const uchar *PesFrame = PesReader.ReadPesFrame(Index, Independent, Bytes)) {
            if (Pes2TsConverter.ConvertPesFrame(PesFrame, Bytes, Independent)) {
               if (Pes2TsConverter.IsPre_1_3_19_PrivateStream()) {
                  Info(3, "recording %s is pre 1.3.19", PesFileName);
                  break;
                  }
               }
            else
               return false;
            }
         }
      else
         break;
      }
  if (Pes2TsConverter.IsPre_1_3_19_PrivateStream()) {
     Info(2, "analyzing pre 1.3.19 PES recording");
     Pes2TsConverter.ResetPids();
     for (int Index = 0; Index < NUM_FRAMES_ANALYZE; Index++) {
         if (PesReader.HasData(Index)) {
            bool Independent = false;
            int Bytes = 0;
            if (const uchar *PesFrame = PesReader.ReadPesFrame(Index, Independent, Bytes)) {
               if (!Pes2TsConverter.ConvertPesFrame(PesFrame, Bytes, Independent))
                  return false;
               }
            }
         else
            break;
         }
     }
  Pes2TsConverter.SetAnalyzing(false);
  if (!Pes2TsConverter.ListPids())
     return false;

  cTsWriter TsWriter;
  if (!TsWriter.Open(TsFileName))
     return false;
  cRecordingTimerId RecordingTimerId(TsFileName);
  int Index = 0;
  while (PesReader.HasData(Index)) {
        bool Independent = false;
        int Bytes = 0;
        if (const uchar *PesFrame = PesReader.ReadPesFrame(Index, Independent, Bytes)) {
           if (Pes2TsConverter.ConvertPesFrame(PesFrame, Bytes, Independent)) {
              int Length;
              const uchar *TsFrame = Pes2TsConverter.TsFrame(Length);
              if (!TsWriter.Write(TsFrame, Length, Independent))
                 return false;
              }
           else
              return false;
           }
        else
           return Error("no data for index '%d'", Index);
        Index++;
        }
  Info(1, "SUCCESS!");
  return true;
}
