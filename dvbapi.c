/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.174.1.1 2002/05/18 14:15:36 kls Exp $
 */

#include "dvbapi.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
extern "C" {
#define HAVE_BOOLEAN
#include <jpeglib.h>
}
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#include "recording.h"
#include "remux.h"
#include "ringbuffer.h"
#include "tools.h"
#include "videodir.h"

#define DEV_VIDEO         "/dev/video"
#define DEV_OST_OSD       "/dev/ost/osd"
#define DEV_OST_FRONTEND  "/dev/ost/frontend"
#define DEV_OST_SEC       "/dev/ost/sec"
#define DEV_OST_DVR       "/dev/ost/dvr"
#define DEV_OST_DEMUX     "/dev/ost/demux"
#define DEV_OST_VIDEO     "/dev/ost/video"
#define DEV_OST_AUDIO     "/dev/ost/audio"

// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE  MEGABYTE(1)

// The maximum size of a single frame:
#define MAXFRAMESIZE  KILOBYTE(192)

#define MAXFILESPERRECORDING 255

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

#define INDEXFILESUFFIX     "/index.vdr"
#define RECORDFILESUFFIX    "/%03d.vdr"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...

// The number of frames to back up when resuming an interrupted replay session:
#define RESUMEBACKUP (10 * FRAMESPERSEC)

// The maximum time we wait before assuming that a recorded video data stream
// is broken:
#define MAXBROKENTIMEOUT 30 // seconds

// The maximum time to wait before giving up while catching up on an index file:
#define MAXINDEXCATCHUP   2 // seconds

// The default priority for non-primary DVB cards:
#define DEFAULTPRIORITY  -2

#define CHECK(s) { if ((s) < 0) LOG_ERROR; } // used for 'ioctl()' calls

#define FATALERRNO (errno != EAGAIN && errno != EINTR)

typedef unsigned char uchar;

const char *IndexToHMSF(int Index, bool WithFrame)
{
  static char buffer[16];
  int f = (Index % FRAMESPERSEC) + 1;
  int s = (Index / FRAMESPERSEC);
  int m = s / 60 % 60;
  int h = s / 3600;
  s %= 60;
  snprintf(buffer, sizeof(buffer), WithFrame ? "%d:%02d:%02d.%02d" : "%d:%02d:%02d", h, m, s, f);
  return buffer;
}

int HMSFToIndex(const char *HMSF)
{
  int h, m, s, f = 0;
  if (3 <= sscanf(HMSF, "%d:%d:%d.%d", &h, &m, &s, &f))
     return (h * 3600 + m * 60 + s) * FRAMESPERSEC + f - 1;
  return 0;
}

// --- cIndexFile ------------------------------------------------------------

class cIndexFile {
private:
  struct tIndex { int offset; uchar type; uchar number; short reserved; };
  int f;
  char *fileName;
  int size, last;
  tIndex *index;
  cResumeFile resumeFile;
  bool CatchUp(int Index = -1);
public:
  cIndexFile(const char *FileName, bool Record);
  ~cIndexFile();
  bool Ok(void) { return index != NULL; }
  bool Write(uchar PictureType, uchar FileNumber, int FileOffset);
  bool Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType = NULL, int *Length = NULL);
  int GetNextIFrame(int Index, bool Forward, uchar *FileNumber = NULL, int *FileOffset = NULL, int *Length = NULL, bool StayOffEnd = false);
  int Get(uchar FileNumber, int FileOffset);
  int Last(void) { CatchUp(); return last; }
  int GetResume(void) { return resumeFile.Read(); }
  bool StoreResume(int Index) { return resumeFile.Save(Index); }
  };

cIndexFile::cIndexFile(const char *FileName, bool Record)
:resumeFile(FileName)
{
  f = -1;
  fileName = NULL;
  size = 0;
  last = -1;
  index = NULL;
  if (FileName) {
     fileName = new char[strlen(FileName) + strlen(INDEXFILESUFFIX) + 1];
     if (fileName) {
        strcpy(fileName, FileName);
        char *pFileExt = fileName + strlen(fileName);
        strcpy(pFileExt, INDEXFILESUFFIX);
        int delta = 0;
        if (access(fileName, R_OK) == 0) {
           struct stat buf;
           if (stat(fileName, &buf) == 0) {
              delta = buf.st_size % sizeof(tIndex);
              if (delta) {
                 delta = sizeof(tIndex) - delta;
                 esyslog(LOG_ERR, "ERROR: invalid file size (%ld) in '%s'", buf.st_size, fileName);
                 }
              last = (buf.st_size + delta) / sizeof(tIndex) - 1;
              if (!Record && last >= 0) {
                 size = last + 1;
                 index = new tIndex[size];
                 if (index) {
                    f = open(fileName, O_RDONLY);
                    if (f >= 0) {
                       if ((int)safe_read(f, index, buf.st_size) != buf.st_size) {
                          esyslog(LOG_ERR, "ERROR: can't read from file '%s'", fileName);
                          delete index;
                          index = NULL;
                          close(f);
                          f = -1;
                          }
                       // we don't close f here, see CatchUp()!
                       }
                    else
                       LOG_ERROR_STR(fileName);
                    }
                 else
                    esyslog(LOG_ERR, "ERROR: can't allocate %d bytes for index '%s'", size * sizeof(tIndex), fileName);
                 }
              }
           else
              LOG_ERROR;
           }
        else if (!Record)
           isyslog(LOG_INFO, "missing index file %s", fileName);
        if (Record) {
           if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0) {
              if (delta) {
                 esyslog(LOG_ERR, "ERROR: padding index file with %d '0' bytes", delta);
                 while (delta--)
                       writechar(f, 0);
                 }
              }
           else
              LOG_ERROR_STR(fileName);
           }
        }
     else
        esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", FileName);
     }
}

cIndexFile::~cIndexFile()
{
  if (f >= 0)
     close(f);
  delete fileName;
  delete index;
}

bool cIndexFile::CatchUp(int Index)
{
  if (index && f >= 0) {
     for (int i = 0; i <= MAXINDEXCATCHUP && (Index < 0 || Index >= last); i++) {
         struct stat buf;
         if (fstat(f, &buf) == 0) {
            int newLast = buf.st_size / sizeof(tIndex) - 1;
            if (newLast > last) {
               if (size <= newLast) {
                  size *= 2;
                  if (size <= newLast)
                     size = newLast + 1;
                  }
               index = (tIndex *)realloc(index, size * sizeof(tIndex));
               if (index) {
                  int offset = (last + 1) * sizeof(tIndex);
                  int delta = (newLast - last) * sizeof(tIndex);
                  if (lseek(f, offset, SEEK_SET) == offset) {
                     if (safe_read(f, &index[last + 1], delta) != delta) {
                        esyslog(LOG_ERR, "ERROR: can't read from index");
                        delete index;
                        index = NULL;
                        close(f);
                        f = -1;
                        break;
                        }
                     last = newLast;
                     }
                  else
                     LOG_ERROR_STR(fileName);
                  }
               else
                  esyslog(LOG_ERR, "ERROR: can't realloc() index");
               }
            }
         else
            LOG_ERROR_STR(fileName);
         if (Index >= last)
            sleep(1);
         else
            return true;
         }
     }
  return false;
}

bool cIndexFile::Write(uchar PictureType, uchar FileNumber, int FileOffset)
{
  if (f >= 0) {
     tIndex i = { FileOffset, PictureType, FileNumber, 0 };
     if (safe_write(f, &i, sizeof(i)) < 0) {
        LOG_ERROR_STR(fileName);
        close(f);
        f = -1;
        return false;
        }
     last++;
     }
  return f >= 0;
}

bool cIndexFile::Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType, int *Length)
{
  if (index) {
     CatchUp(Index);
     if (Index >= 0 && Index <= last) {
        *FileNumber = index[Index].number;
        *FileOffset = index[Index].offset;
        if (PictureType)
           *PictureType = index[Index].type;
        if (Length) {
           int fn = index[Index + 1].number;
           int fo = index[Index + 1].offset;
           if (fn == *FileNumber)
              *Length = fo - *FileOffset;
           else
              *Length = -1; // this means "everything up to EOF" (the buffer's Read function will act accordingly)
           }
        return true;
        }
     }
  return false;
}

int cIndexFile::GetNextIFrame(int Index, bool Forward, uchar *FileNumber, int *FileOffset, int *Length, bool StayOffEnd)
{
  if (index) {
     CatchUp();
     int d = Forward ? 1 : -1;
     for (;;) {
         Index += d;
         if (Index >= 0 && Index < last - ((Forward && StayOffEnd) ? 100 : 0)) {
            if (index[Index].type == I_FRAME) {
               if (FileNumber)
                  *FileNumber = index[Index].number;
               else
                  FileNumber = &index[Index].number;
               if (FileOffset)
                  *FileOffset = index[Index].offset;
               else
                  FileOffset = &index[Index].offset;
               if (Length) {
                  // all recordings end with a non-I_FRAME, so the following should be safe:
                  int fn = index[Index + 1].number;
                  int fo = index[Index + 1].offset;
                  if (fn == *FileNumber)
                     *Length = fo - *FileOffset;
                  else {
                     esyslog(LOG_ERR, "ERROR: 'I' frame at end of file #%d", *FileNumber);
                     *Length = -1;
                     }
                  }
               return Index;
               }
            }
         else
            break;
         }
     }
  return -1;
}

int cIndexFile::Get(uchar FileNumber, int FileOffset)
{
  if (index) {
     CatchUp();
     //TODO implement binary search!
     int i;
     for (i = 0; i < last; i++) {
         if (index[i].number > FileNumber || (index[i].number == FileNumber) && index[i].offset >= FileOffset)
            break;
         }
     return i;
     }
  return -1;
}

// --- cFileName -------------------------------------------------------------

class cFileName {
private:
  int file;
  int fileNumber;
  char *fileName, *pFileNumber;
  bool record;
  bool blocking;
public:
  cFileName(const char *FileName, bool Record, bool Blocking = false);
  ~cFileName();
  const char *Name(void) { return fileName; }
  int Number(void) { return fileNumber; }
  int Open(void);
  void Close(void);
  int SetOffset(int Number, int Offset = 0);
  int NextFile(void);
  };

cFileName::cFileName(const char *FileName, bool Record, bool Blocking)
{
  file = -1;
  fileNumber = 0;
  record = Record;
  blocking = Blocking;
  // Prepare the file name:
  fileName = new char[strlen(FileName) + RECORDFILESUFFIXLEN];
  if (!fileName) {
     esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", fileName);
     return;
     }
  strcpy(fileName, FileName);
  pFileNumber = fileName + strlen(fileName);
  SetOffset(1);
}

cFileName::~cFileName()
{
  Close();
  delete fileName;
}

int cFileName::Open(void)
{
  if (file < 0) {
     int BlockingFlag = blocking ? 0 : O_NONBLOCK;
     if (record) {
        dsyslog(LOG_INFO, "recording to '%s'", fileName);
        file = OpenVideoFile(fileName, O_RDWR | O_CREAT | BlockingFlag);
        if (file < 0)
           LOG_ERROR_STR(fileName);
        }
     else {
        if (access(fileName, R_OK) == 0) {
           dsyslog(LOG_INFO, "playing '%s'", fileName);
           file = open(fileName, O_RDONLY | BlockingFlag);
           if (file < 0)
              LOG_ERROR_STR(fileName);
           }
        else if (errno != ENOENT)
           LOG_ERROR_STR(fileName);
        }
     }
  return file;
}

void cFileName::Close(void)
{
  if (file >= 0) {
     if ((record && CloseVideoFile(file) < 0) || (!record && close(file) < 0))
        LOG_ERROR_STR(fileName);
     file = -1;
     }
}

int cFileName::SetOffset(int Number, int Offset)
{
  if (fileNumber != Number)
     Close();
  if (0 < Number && Number <= MAXFILESPERRECORDING) {
     fileNumber = Number;
     sprintf(pFileNumber, RECORDFILESUFFIX, fileNumber);
     if (record) {
        if (access(fileName, F_OK) == 0) // file exists, let's try next suffix
           return SetOffset(Number + 1);
        else if (errno != ENOENT) { // something serious has happened
           LOG_ERROR_STR(fileName);
           return -1;
           }
        // found a non existing file suffix
        }
     if (Open() >= 0) {
        if (!record && Offset >= 0 && lseek(file, Offset, SEEK_SET) != Offset) {
           LOG_ERROR_STR(fileName);
           return -1;
           }
        }
     return file;
     }
  esyslog(LOG_ERR, "ERROR: max number of files (%d) exceeded", MAXFILESPERRECORDING);
  return -1;
}

int cFileName::NextFile(void)
{
  return SetOffset(fileNumber + 1);
}

// --- cRecordBuffer ---------------------------------------------------------

class cRecordBuffer : public cRingBufferLinear {
private:
  cDvbApi *dvbApi;
  cFileName fileName;
  cIndexFile *index;
  cRemux remux;
  uchar pictureType;
  int fileSize;
  int videoDev;
  int recordFile;
  bool recording;
  time_t lastDiskSpaceCheck;
  bool RunningLowOnDiskSpace(void);
  bool NextFile(void);
protected:
  virtual void Input(void);
  virtual void Output(void);
public:
  cRecordBuffer(cDvbApi *DvbApi, const char *FileName, int VPid, int APid1, int APid2, int DPid1, int DPid2);
  virtual ~cRecordBuffer();
  };

cRecordBuffer::cRecordBuffer(cDvbApi *DvbApi, const char *FileName, int VPid, int APid1, int APid2, int DPid1, int DPid2)
:cRingBufferLinear(VIDEOBUFSIZE, true)
,fileName(FileName, true)
,remux(VPid, APid1, APid2, DPid1, DPid2, true)
{
  dvbApi = DvbApi;
  index = NULL;
  pictureType = NO_PICTURE;
  fileSize = 0;
  recordFile = fileName.Open();
  recording = false;
  lastDiskSpaceCheck = time(NULL);
  if (!fileName.Name())
     return;
  // Create the index file:
  index = new cIndexFile(FileName, true);
  if (!index)
     esyslog(LOG_ERR, "ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
  videoDev = dvbApi->SetModeRecord();
  Start();
}

cRecordBuffer::~cRecordBuffer()
{
  Stop();
  dvbApi->SetModeNormal(true);
  delete index;
}

bool cRecordBuffer::RunningLowOnDiskSpace(void)
{
  if (time(NULL) > lastDiskSpaceCheck + DISKCHECKINTERVAL) {
     int Free = FreeDiskSpaceMB(fileName.Name());
     lastDiskSpaceCheck = time(NULL);
     if (Free < MINFREEDISKSPACE) {
        dsyslog(LOG_INFO, "low disk space (%d MB, limit is %d MB)", Free, MINFREEDISKSPACE);
        return true;
        }
     }
  return false;
}

bool cRecordBuffer::NextFile(void)
{
  if (recordFile >= 0 && pictureType == I_FRAME) { // every file shall start with an I_FRAME
     if (fileSize > MEGABYTE(Setup.MaxVideoFileSize) || RunningLowOnDiskSpace()) {
        recordFile = fileName.NextFile();
        fileSize = 0;
        }
     }
  return recordFile >= 0;
}

void cRecordBuffer::Input(void)
{
  dsyslog(LOG_INFO, "input thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  time_t t = time(NULL);
  recording = true;
  for (;;) {
      if (cFile::FileReady(videoDev, 100)) {
         int r = read(videoDev, b, sizeof(b));
         if (r > 0) {
            uchar *p = b;
            while (r > 0) {
                  int w = Put(p, r);
                  p += w;
                  r -= w;
                  if (r > 0)
                     usleep(1); // this keeps the CPU load low
                  }
            t = time(NULL);
            }
         else if (r < 0) {
            if (FATALERRNO) {
               if (errno == EBUFFEROVERFLOW) // this error code is not defined in the library
                  esyslog(LOG_ERR, "ERROR (%s,%d): DVB driver buffer overflow", __FILE__, __LINE__);
               else {
                  LOG_ERROR;
                  break;
                  }
               }
            }
         }
      if (time(NULL) - t > MAXBROKENTIMEOUT) {
         esyslog(LOG_ERR, "ERROR: video data stream broken");
         cThread::EmergencyExit(true);
         t = time(NULL);
         }
      if (!recording)
         break;
      }

  dsyslog(LOG_INFO, "input thread ended (pid=%d)", getpid());
}

void cRecordBuffer::Output(void)
{
  dsyslog(LOG_INFO, "output thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  int r = 0;
  for (;;) {
      int g = Get(b + r, sizeof(b) - r);
      if (g > 0)
         r += g;
      if (r > 0) {
         int Count = r, Result;
         const uchar *p = remux.Process(b, Count, Result, &pictureType);
         if (p) {
            if (!Busy() && pictureType == I_FRAME) // finish the recording before the next 'I' frame
               break;
            if (NextFile()) {
               if (index && pictureType != NO_PICTURE)
                  index->Write(pictureType, fileName.Number(), fileSize);
               if (safe_write(recordFile, p, Result) < 0) {
                  LOG_ERROR_STR(fileName.Name());
                  recording = false;
                  return;
                  }
               fileSize += Result;
               }
            else
               break;
            }
         if (Count > 0) {
            r -= Count;
            memmove(b, b + Count, r);
            }
         if (!recording)
            break;
         }
      else
         usleep(1); // this keeps the CPU load low
      }
  recording = false;

  dsyslog(LOG_INFO, "output thread ended (pid=%d)", getpid());
}

// --- ReadFrame -------------------------------------------------------------

int ReadFrame(int f, uchar *b, int Length, int Max)
{
  if (Length == -1)
     Length = Max; // this means we read up to EOF (see cIndex)
  else if (Length > Max) {
     esyslog(LOG_ERR, "ERROR: frame larger than buffer (%d > %d)", Length, Max);
     Length = Max;
     }
  int r = safe_read(f, b, Length);
  if (r < 0)
     LOG_ERROR;
  return r;
}

// --- cBackTrace ----------------------------------------------------------

#define AVG_FRAME_SIZE 15000         // an assumption about the average frame size
#define DVB_BUF_SIZE   (256 * 1024)  // an assumption about the dvb firmware buffer size
#define BACKTRACE_ENTRIES (DVB_BUF_SIZE / AVG_FRAME_SIZE + 20) // how many entries are needed to backtrace buffer contents

class cBackTrace {
private:
  int index[BACKTRACE_ENTRIES];
  int length[BACKTRACE_ENTRIES];
  int pos, num;
public:
  cBackTrace(void);
  void Clear(void);
  void Add(int Index, int Length);
  int Get(bool Forward);
  };

cBackTrace::cBackTrace(void)
{
  Clear();
}

void cBackTrace::Clear(void)
{
  pos = num = 0;
}

void cBackTrace::Add(int Index, int Length)
{
  index[pos] = Index;
  length[pos] = Length;
  if (++pos >= BACKTRACE_ENTRIES)
     pos = 0;
  if (num < BACKTRACE_ENTRIES)
     num++;
}

int cBackTrace::Get(bool Forward)
{
  int p = pos;
  int n = num;
  int l = DVB_BUF_SIZE + (Forward ? 0 : 256 * 1024); //XXX (256 * 1024) == DVB_BUF_SIZE ???
  int i = -1;

  while (n && l > 0) {
        if (--p < 0)
           p = BACKTRACE_ENTRIES - 1;
        i = index[p] - 1;
        l -= length[p];
        n--; 
        }
  return i;
}

// --- cPlayBuffer ---------------------------------------------------------

#define MAX_VIDEO_SLOWMOTION 63 // max. arg to pass to VIDEO_SLOWMOTION // TODO is this value correct?

class cPlayBuffer : public cRingBufferFrame {
private:
  cBackTrace backTrace;
protected:
  enum ePlayModes { pmPlay, pmPause, pmSlow, pmFast, pmStill };
  enum ePlayDirs { pdForward, pdBackward };
  static int Speeds[];
  cDvbApi *dvbApi;
  int videoDev, audioDev;
  cPipe dolbyDev;
  int blockInput, blockOutput;
  ePlayModes playMode;
  ePlayDirs playDir;
  int trickSpeed;
  int readIndex, writeIndex;
  bool canDoTrickMode;
  bool canToggleAudioTrack;
  bool skipAC3bytes;
  uchar audioTrack;
  void TrickSpeed(int Increment);
  virtual void Empty(bool Block = false);
  virtual void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00) {}
  virtual void PlayExternalDolby(const uchar *b, int MaxLength);
  virtual void Output(void);
  void putFrame(cFrame *Frame);
  void putFrame(unsigned char *Data, int Length, eFrameType Type = ftUnknown);
public:
  cPlayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev);
  virtual ~cPlayBuffer();
  virtual void Pause(void);
  virtual void Play(void);
  virtual void Forward(void);
  virtual void Backward(void);
  virtual int SkipFrames(int Frames) { return -1; }
  virtual void SkipSeconds(int Seconds) {}
  virtual void Goto(int Position, bool Still = false) {}
  virtual void GetIndex(int &Current, int &Total, bool SnapToIFrame = false) { Current = Total = -1; }
  bool GetReplayMode(bool &Play, bool &Forward, int &Speed);
  bool CanToggleAudioTrack(void) { return canToggleAudioTrack; };
  virtual void ToggleAudioTrack(void);
  };

#define NORMAL_SPEED  4 // the index of the '1' entry in the following array
#define MAX_SPEEDS    3 // the offset of the maximum speed from normal speed in either direction
#define SPEED_MULT   12 // the speed multiplier
int cPlayBuffer::Speeds[] = { 0, -2, -4, -8, 1, 2, 4, 12, 0 };

cPlayBuffer::cPlayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev)
:cRingBufferFrame(VIDEOBUFSIZE)
{
  dvbApi = DvbApi;
  videoDev = VideoDev;
  audioDev = AudioDev;
  blockInput = blockOutput = false;
  playMode = pmPlay;
  playDir = pdForward;
  trickSpeed = NORMAL_SPEED;
  readIndex = writeIndex = -1;
  canDoTrickMode = false;
  canToggleAudioTrack = false;
  skipAC3bytes = false;
  audioTrack = 0xC0;
}

cPlayBuffer::~cPlayBuffer()
{
}

void cPlayBuffer::PlayExternalDolby(const uchar *b, int MaxLength)
{
  if (cDvbApi::AudioCommand()) {
     if (!dolbyDev && !dolbyDev.Open(cDvbApi::AudioCommand(), "w")) {
        esyslog(LOG_ERR, "ERROR: can't open pipe to audio command '%s'", cDvbApi::AudioCommand());
        return;
        }
     if (b[0] == 0x00 && b[1] == 0x00 && b[2] == 0x01) {
        if (b[3] == 0xBD) { // dolby
           int l = b[4] * 256 + b[5] + 6;
           int written = b[8] + (skipAC3bytes ? 13 : 9); // skips the PES header
           int n = min(l - written, MaxLength);
           while (n > 0) {
                 int w = fwrite(&b[written], 1, n, dolbyDev);
                 if (w < 0) {
                    LOG_ERROR;
                    break;
                    }
                 n -= w;
                 written += w;
                 }
           }
        }
     }
}

void cPlayBuffer::Output(void)
{
  dsyslog(LOG_INFO, "output thread started (pid=%d)", getpid());

  while (Busy()) {
        if (blockOutput) {
           if (blockOutput > 1)
              blockOutput = 1;
           continue;
           }
        const cFrame *frame = Get();
        if (frame) {
           if (frame->Type() == ftDolby)
              PlayExternalDolby(frame->Data(), frame->Count());
           else {
              StripAudioPackets((uchar *)frame->Data(), frame->Count(), (playMode == pmFast || playMode == pmSlow) ? 0x00 : audioTrack);//XXX
              const uchar *p = frame->Data();
              int r = frame->Count();
              while (r > 0 && Busy() && !blockOutput) {
                    if (cFile::FileReadyForWriting(videoDev, 100)) {
                       int w = write(videoDev, p, r);
                       if (w > 0) {
                          p += w;
                          r -= w;
                          }
                       else if (w < 0 && FATALERRNO) {
                          LOG_ERROR;
                          Stop();
                          return;
                          }
                       }
                    }
              writeIndex = frame->Index();
              backTrace.Add(frame->Index(), frame->Count());
              }
           Drop(frame);
           }
        }

  dsyslog(LOG_INFO, "output thread ended (pid=%d)", getpid());
}

void cPlayBuffer::putFrame(cFrame *Frame)
{
  while (Busy() && !blockInput) {
        if (Put(Frame))
           return;
        }
  delete Frame; // caller relies on frame being put, otherwise this would be a memory leak!
}

void cPlayBuffer::putFrame(unsigned char *Data, int Length, eFrameType Type)
{
  putFrame(new cFrame(Data, Length, Type));
}

void cPlayBuffer::TrickSpeed(int Increment)
{
  int nts = trickSpeed + Increment;
  if (Speeds[nts] == 1) {
     trickSpeed = nts;
     if (playMode == pmFast)
        Play();
     else
        Pause();
     }
  else if (Speeds[nts]) {
     trickSpeed = nts;
     int Mult = (playMode == pmSlow && playDir == pdForward) ? 1 : SPEED_MULT;
     int sp = (Speeds[nts] > 0) ? Mult / Speeds[nts] : -Speeds[nts] * Mult;
     if (sp > MAX_VIDEO_SLOWMOTION)
        sp = MAX_VIDEO_SLOWMOTION;
     CHECK(ioctl(videoDev, VIDEO_SLOWMOTION, sp));
     }
}

void cPlayBuffer::Empty(bool Block)
{
  if (!(blockInput || blockOutput)) {
     blockInput = blockOutput = 2;
     EnablePut();
     EnableGet();
     time_t t0 = time(NULL);
     while ((blockInput > 1 || blockOutput > 1) && time(NULL) - t0 < 2)
           usleep(1);
     Lock();
     if ((readIndex = backTrace.Get(playDir == pdForward)) < 0)
        readIndex = writeIndex;
     cRingBufferFrame::Clear();
     CHECK(ioctl(videoDev, VIDEO_CLEAR_BUFFER));
     CHECK(ioctl(audioDev, AUDIO_CLEAR_BUFFER));
     }
  if (!Block) {
     blockInput = blockOutput = 0;
     backTrace.Clear();
     Unlock();
     }
}

void cPlayBuffer::Pause(void)
{
  if (playMode == pmPause || playMode == pmStill)
     Play();
  else {
     bool empty = (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward));
     if (empty)
        Empty(true);
     CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
     CHECK(ioctl(videoDev, VIDEO_FREEZE));
     playMode = pmPause;
     if (empty)
        Empty(false);
     }
}

void cPlayBuffer::Play(void)
{
  if (playMode != pmPlay) {
     bool empty = (playMode == pmStill || playMode == pmFast || (playMode == pmSlow && playDir == pdBackward));
     if (empty)
        Empty(true);
     CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, true));
     CHECK(ioctl(videoDev, VIDEO_CONTINUE));
     playMode = pmPlay;
     playDir = pdForward;
     if (empty)
        Empty(false);
    }
}

void cPlayBuffer::Forward(void)
{
  if (canDoTrickMode) {
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? 1 : -1);
               break;
               }
            else if (playDir == pdForward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay:
            Empty(true);
            CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
            playMode = pmFast;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            Empty(false);
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdForward ? -1 : 1);
               break;
               }
            else if (playDir == pdForward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause:
            CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
            playMode = pmSlow;
            playDir = pdForward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            break;
       }
     }
}

void cPlayBuffer::Backward(void)
{
  if (canDoTrickMode) {
     switch (playMode) {
       case pmFast:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? 1 : -1);
               break;
               }
            else if (playDir == pdBackward) {
               Play();
               break;
               }
            // run into pmPlay
       case pmPlay:
            Empty(true);
            CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
            playMode = pmFast;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? 1 : MAX_SPEEDS);
            Empty(false);
            break;
       case pmSlow:
            if (Setup.MultiSpeedMode) {
               TrickSpeed(playDir == pdBackward ? -1 : 1);
               break;
               }
            else if (playDir == pdBackward) {
               Pause();
               break;
               }
            // run into pmPause
       case pmStill:
       case pmPause:
            Empty(true);
            CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
            playMode = pmSlow;
            playDir = pdBackward;
            trickSpeed = NORMAL_SPEED;
            TrickSpeed(Setup.MultiSpeedMode ? -1 : -MAX_SPEEDS);
            Empty(false);
            break;
       }
     }
}

bool cPlayBuffer::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  Play = (playMode == pmPlay || playMode == pmFast);
  Forward = (playDir == pdForward);
  if (playMode == pmFast || playMode == pmSlow)
     Speed = Setup.MultiSpeedMode ? abs(trickSpeed - NORMAL_SPEED) : 0;
  else
     Speed = -1;
  return true;
}

void cPlayBuffer::ToggleAudioTrack(void)
{
  if (CanToggleAudioTrack()) {
     audioTrack = (audioTrack == 0xC0) ? 0xC1 : 0xC0;
     Empty();
     }
}

// --- cReplayBuffer ---------------------------------------------------------

class cReplayBuffer : public cPlayBuffer {
private:
  cIndexFile *index;
  cFileName fileName;
  int replayFile;
  bool eof;
  bool NextFile(uchar FileNumber = 0, int FileOffset = -1);
  void Close(void);
  virtual void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00);
  void DisplayFrame(uchar *b, int Length);
  int Resume(void);
  bool Save(void);
protected:
  virtual void Input(void);
public:
  cReplayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev, const char *FileName);
  virtual ~cReplayBuffer();
  virtual int SkipFrames(int Frames);
  virtual void SkipSeconds(int Seconds);
  virtual void Goto(int Position, bool Still = false);
  virtual void GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
  };

cReplayBuffer::cReplayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev, const char *FileName)
:cPlayBuffer(DvbApi, VideoDev, AudioDev)
,fileName(FileName, false)
{
  index = NULL;
  replayFile = fileName.Open();
  eof = false;
  if (!fileName.Name())
     return;
  // Create the index file:
  index = new cIndexFile(FileName, false);
  if (!index)
     esyslog(LOG_ERR, "ERROR: can't allocate index");
  else if (!index->Ok()) {
     delete index;
     index = NULL;
     }
  canDoTrickMode = index != NULL;
  dvbApi->SetModeReplay();
  Start();
}

cReplayBuffer::~cReplayBuffer()
{
  Stop();
  Save();
  Close();
  dvbApi->SetModeNormal(false);
  delete index;
}

void cReplayBuffer::Input(void)
{
  dsyslog(LOG_INFO, "input thread started (pid=%d)", getpid());

  readIndex = Resume();
  if (readIndex >= 0)
     isyslog(LOG_INFO, "resuming replay at index %d (%s)", readIndex, IndexToHMSF(readIndex, true));

  uchar b[MAXFRAMESIZE];
  while (Busy() && (blockInput || NextFile())) {
        if (blockInput) {
           if (blockInput > 1)
              blockInput = 1;
           continue;
           }
        if (playMode != pmStill) {
           int r = 0;
           if (playMode == pmFast || (playMode == pmSlow && playDir == pdBackward)) {
              uchar FileNumber;
              int FileOffset, Length;
              int Index = index->GetNextIFrame(readIndex, playDir == pdForward, &FileNumber, &FileOffset, &Length, true);
              if (Index >= 0) {
                 if (!NextFile(FileNumber, FileOffset))
                    break;
                 }
              else {
                 // can't call Play() here, because those functions may only be
                 // called from the foreground thread - and we also don't need
                 // to empty the buffer here
                 CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, true));
                 CHECK(ioctl(videoDev, VIDEO_CONTINUE));
                 playMode = pmPlay;
                 playDir = pdForward;
                 continue;
                 }
              readIndex = Index;
              r = ReadFrame(replayFile, b, Length, sizeof(b));
              // must call StripAudioPackets() here because the buffer is not emptied
              // when falling back from "fast forward" to "play" (see above)
              StripAudioPackets(b, r);
              }
           else if (index) {
              uchar FileNumber;
              int FileOffset, Length;
              readIndex++;
              if (!(index->Get(readIndex, &FileNumber, &FileOffset, NULL, &Length) && NextFile(FileNumber, FileOffset)))
                 break;
              r = ReadFrame(replayFile, b, Length, sizeof(b));
              }
           else // allows replay even if the index file is missing
              r = read(replayFile, b, sizeof(b));
           if (r > 0)
              putFrame(new cFrame(b, r, ftUnknown, readIndex));
           else if (r == 0)
              eof = true;
           else if (r < 0 && FATALERRNO) {
              LOG_ERROR;
              break;
              }
           }
        else//XXX
           usleep(1); // this keeps the CPU load low
        }

  dsyslog(LOG_INFO, "input thread ended (pid=%d)", getpid());
}

void cReplayBuffer::StripAudioPackets(uchar *b, int Length, uchar Except)
{
  if (canDoTrickMode) {
     for (int i = 0; i < Length - 6; i++) {
         if (b[i] == 0x00 && b[i + 1] == 0x00 && b[i + 2] == 0x01) {
            uchar c = b[i + 3];
            int l = b[i + 4] * 256 + b[i + 5] + 6;
            switch (c) {
              case 0xBD: // dolby
                   if (Except)
                      PlayExternalDolby(&b[i], Length - i);
                   // continue with deleting the data - otherwise it disturbs DVB replay
              case 0xC0 ... 0xC1: // audio
                   if (c == 0xC1)
                      canToggleAudioTrack = true;
                   if (!Except || c != Except) {
                      int n = l;
                      for (int j = i; j < Length && n--; j++)
                          b[j] = 0x00;
                      }
                   break;
              case 0xE0 ... 0xEF: // video
                   break;
              default:
                   //esyslog(LOG_ERR, "ERROR: unexpected packet id %02X", c);
                   l = 0;
              }
            if (l)
               i += l - 1; // the loop increments, too!
            }
         /*XXX
         else
            esyslog(LOG_ERR, "ERROR: broken packet header");
            XXX*/
         }
     }
}

void cReplayBuffer::DisplayFrame(uchar *b, int Length)
{
  StripAudioPackets(b, Length);
  CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
  CHECK(ioctl(audioDev, AUDIO_SET_MUTE, true));
/* Using the VIDEO_STILLPICTURE ioctl call would be the
   correct way to display a still frame, but unfortunately this
   doesn't work with frames from VDR. So let's do pretty much the
   same here as in DVB/driver/dvb.c's play_iframe() - I have absolutely
   no idea why it works this way, but doesn't work with VIDEO_STILLPICTURE.
   If anybody ever finds out what could be changed so that VIDEO_STILLPICTURE
   could be used, please let me know!
   kls 2002-03-23
*/
//#define VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
#ifdef VIDEO_STILLPICTURE_WORKS_WITH_VDR_FRAMES
  videoDisplayStillPicture sp = { (char *)b, Length };
  CHECK(ioctl(videoDev, VIDEO_STILLPICTURE, &sp));
#else
#define MIN_IFRAME 400000
  for (int i = MIN_IFRAME / Length + 1; i > 0; i--) {
      safe_write(videoDev, b, Length);
      usleep(1); // allows the buffer to be displayed in case the progress display is active
      }
#endif
}

void cReplayBuffer::Close(void)
{
  if (replayFile >= 0) {
     fileName.Close();
     replayFile = -1;
     }
}

int cReplayBuffer::Resume(void)
{
  if (index) {
     int Index = index->GetResume();
     if (Index >= 0) {
        uchar FileNumber;
        int FileOffset;
        if (index->Get(Index, &FileNumber, &FileOffset) && NextFile(FileNumber, FileOffset))
           return Index;
        }
     }
  return -1;
}

bool cReplayBuffer::Save(void)
{
  if (index) {
     int Index = writeIndex;
     if (Index >= 0) {
        Index -= RESUMEBACKUP;
        if (Index > 0)
           Index = index->GetNextIFrame(Index, false);
        else
           Index = 0;
        if (Index >= 0)
           return index->StoreResume(Index);
        }
     }
  return false;
}

int cReplayBuffer::SkipFrames(int Frames)
{
  if (index && Frames) {
     int Current, Total;
     GetIndex(Current, Total, true);
     int OldCurrent = Current;
     Current = index->GetNextIFrame(Current + Frames, Frames > 0);
     return Current >= 0 ? Current : OldCurrent;
     }
  return -1;
}

void cReplayBuffer::SkipSeconds(int Seconds)
{
  if (index && Seconds) {
     Empty(true);
     int Index = writeIndex;
     if (Index >= 0) {
        Index = max(Index + Seconds * FRAMESPERSEC, 0);
        if (Index > 0)
           Index = index->GetNextIFrame(Index, false, NULL, NULL, NULL, true);
        if (Index >= 0)
           readIndex = writeIndex = Index - 1; // Input() will first increment it!
        }
     Empty(false);
     Play();
     }
}

void cReplayBuffer::Goto(int Index, bool Still)
{
  if (index) {
     Empty(true);
     if (++Index <= 0)
        Index = 1; // not '0', to allow GetNextIFrame() below to work!
     uchar FileNumber;
     int FileOffset, Length;
     Index = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset, &Length);
     if (Index >= 0 && NextFile(FileNumber, FileOffset) && Still) {
        uchar b[MAXFRAMESIZE];
        int r = ReadFrame(replayFile, b, Length, sizeof(b));
        if (r > 0) {
           if (playMode == pmPause)
              CHECK(ioctl(videoDev, VIDEO_CONTINUE));
           DisplayFrame(b, r);
           }
        playMode = pmStill;
        }
     readIndex = writeIndex = Index;
     Empty(false);
     }
}

void cReplayBuffer::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (index) {
     if (playMode == pmStill)
        Current = max(readIndex, 0);
     else {
        Current = max(writeIndex, 0);
        if (SnapToIFrame) {
           int i1 = index->GetNextIFrame(Current + 1, false);
           int i2 = index->GetNextIFrame(Current, true);
           Current = (abs(Current - i1) <= abs(Current - i2)) ? i1 : i2;
           }
        }
     Total = index->Last();
     }
  else
     Current = Total = -1;
}

bool cReplayBuffer::NextFile(uchar FileNumber, int FileOffset)
{
  if (FileNumber > 0)
     replayFile = fileName.SetOffset(FileNumber, FileOffset);
  else if (replayFile >= 0 && eof) {
     Close();
     replayFile = fileName.NextFile();
     }
  eof = false;
  return replayFile >= 0;
}

// --- cTransferBuffer -------------------------------------------------------

class cTransferBuffer : public cRingBufferLinear {
private:
  cDvbApi *dvbApi;
  int fromDevice, toDevice;
  bool gotBufferReserve;
  cRemux remux;
protected:
  virtual void Input(void);
  virtual void Output(void);
public:
  cTransferBuffer(cDvbApi *DvbApi, int ToDevice, int VPid, int APid);
  virtual ~cTransferBuffer();
  void SetAudioPid(int APid);
  };

cTransferBuffer::cTransferBuffer(cDvbApi *DvbApi, int ToDevice, int VPid, int APid)
:cRingBufferLinear(VIDEOBUFSIZE, true)
,remux(VPid, APid, 0, 0, 0)
{
  dvbApi = DvbApi;
  fromDevice = dvbApi->SetModeRecord();
  toDevice = ToDevice;
  gotBufferReserve = false;
  Start();
}

cTransferBuffer::~cTransferBuffer()
{
  Stop();
  dvbApi->SetModeNormal(true);
}

void cTransferBuffer::SetAudioPid(int APid)
{
  Clear();
  //XXX we may need to have access to the audio device, too, in order to clear it
  CHECK(ioctl(toDevice, VIDEO_CLEAR_BUFFER));
  gotBufferReserve = false;
  remux.SetAudioPid(APid);
}

void cTransferBuffer::Input(void)
{
  dsyslog(LOG_INFO, "input thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  int n = 0;
  while (Busy()) {
        if (cFile::FileReady(fromDevice, 100)) {
           int r = read(fromDevice, b + n, sizeof(b) - n);
           if (r > 0) {
              n += r;
              int Count = n, Result;
              const uchar *p = remux.Process(b, Count, Result);
              if (p) {
                 while (Result > 0 && Busy()) {
                       int w = Put(p, Result);
                       p += w;
                       Result -= w;
                       }
                 }
              if (Count > 0) {
                 n -= Count;
                 memmove(b, b + Count, n);
                 }
              }
           else if (r < 0) {
              if (FATALERRNO) {
                 if (errno == EBUFFEROVERFLOW) // this error code is not defined in the library
                    esyslog(LOG_ERR, "ERROR (%s,%d): DVB driver buffer overflow", __FILE__, __LINE__);
                 else {
                    LOG_ERROR;
                    break;
                    }
                 }
              }
           }
        }

  dsyslog(LOG_INFO, "input thread ended (pid=%d)", getpid());
}

void cTransferBuffer::Output(void)
{
  dsyslog(LOG_INFO, "output thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  while (Busy()) {
        if (!gotBufferReserve) {
           if (Available() < MAXFRAMESIZE) {
              usleep(100000); // allow the buffer to collect some reserve
              continue;
              }
           else
              gotBufferReserve = true;
           }
        int r = Get(b, sizeof(b));
        if (r > 0) {
           uchar *p = b;
           while (r > 0 && Busy() && cFile::FileReadyForWriting(toDevice, 100)) {
                 int w = write(toDevice, p, r);
                 if (w > 0) {
                    p += w;
                    r -= w;
                    }
                 else if (w < 0 && FATALERRNO) {
                    LOG_ERROR;
                    Stop();
                    return;
                    }
                 }
           }
        else
           usleep(1); // this keeps the CPU load low
        }

  dsyslog(LOG_INFO, "output thread ended (pid=%d)", getpid());
}

// --- cCuttingBuffer --------------------------------------------------------

class cCuttingBuffer : public cThread {
private:
  const char *error;
  bool active;
  int fromFile, toFile;
  cFileName *fromFileName, *toFileName;
  cIndexFile *fromIndex, *toIndex;
  cMarks fromMarks, toMarks;
protected:
  virtual void Action(void);
public:
  cCuttingBuffer(const char *FromFileName, const char *ToFileName);
  virtual ~cCuttingBuffer();
  const char *Error(void) { return error; }
  };

cCuttingBuffer::cCuttingBuffer(const char *FromFileName, const char *ToFileName)
{
  error = NULL;
  active = false;
  fromFile = toFile = -1;
  fromFileName = toFileName = NULL;
  fromIndex = toIndex = NULL;
  if (fromMarks.Load(FromFileName) && fromMarks.Count()) {
     fromFileName = new cFileName(FromFileName, false, true);
     toFileName = new cFileName(ToFileName, true, true);
     fromIndex = new cIndexFile(FromFileName, false);
     toIndex = new cIndexFile(ToFileName, true);
     toMarks.Load(ToFileName); // doesn't actually load marks, just sets the file name
     Start();
     }
  else
     esyslog(LOG_ERR, "no editing marks found for %s", FromFileName);
}

cCuttingBuffer::~cCuttingBuffer()
{
  active = false;
  Cancel(3);
  delete fromFileName;
  delete toFileName;
  delete fromIndex;
  delete toIndex;
}

void cCuttingBuffer::Action(void)
{
  dsyslog(LOG_INFO, "video cutting thread started (pid=%d)", getpid());

  cMark *Mark = fromMarks.First();
  if (Mark) {
     fromFile = fromFileName->Open();
     toFile = toFileName->Open();
     active = fromFile >= 0 && toFile >= 0;
     int Index = Mark->position;
     Mark = fromMarks.Next(Mark);
     int FileSize = 0;
     int CurrentFileNumber = 0;
     int LastIFrame = 0;
     toMarks.Add(0);
     toMarks.Save();
     uchar buffer[MAXFRAMESIZE];
     while (active) {
           uchar FileNumber;
           int FileOffset, Length;
           uchar PictureType;

           // Make sure there is enough disk space:

           AssertFreeDiskSpace();

           // Read one frame:

           if (fromIndex->Get(Index++, &FileNumber, &FileOffset, &PictureType, &Length)) {
              if (FileNumber != CurrentFileNumber) {
                 fromFile = fromFileName->SetOffset(FileNumber, FileOffset);
                 CurrentFileNumber = FileNumber;
                 }
              if (fromFile >= 0) {
                 int len = ReadFrame(fromFile, buffer,  Length, sizeof(buffer));
                 if (len < 0) {
                    error = "ReadFrame";
                    break;
                    }
                 if (len != Length) {
                    CurrentFileNumber = 0; // this re-syncs in case the frame was larger than the buffer
                    Length = len;
                    }
                 }
              else {
                 error = "fromFile";
                 break;
                 }
              }
           else
              break;

           // Write one frame:

           if (PictureType == I_FRAME) { // every file shall start with an I_FRAME
              if (!Mark) // edited version shall end before next I-frame
                 break;
              if (FileSize > MEGABYTE(Setup.MaxVideoFileSize)) {
                 toFile = toFileName->NextFile();
                 if (toFile < 0) {
                    error = "toFile 1";
                    break;
                    }
                 FileSize = 0;
                 }
              LastIFrame = 0;
              }
           if (safe_write(toFile, buffer, Length) < 0) {
              error = "safe_write";
              break;
              }
           if (!toIndex->Write(PictureType, toFileName->Number(), FileSize)) {
              error = "toIndex";
              break;
              }
           FileSize += Length;
           if (!LastIFrame)
              LastIFrame = toIndex->Last();

           // Check editing marks:

           if (Mark && Index >= Mark->position) {
              Mark = fromMarks.Next(Mark);
              toMarks.Add(LastIFrame);
              if (Mark)
                 toMarks.Add(toIndex->Last() + 1);
              toMarks.Save();
              if (Mark) {
                 Index = Mark->position;
                 Mark = fromMarks.Next(Mark);
                 CurrentFileNumber = 0; // triggers SetOffset before reading next frame
                 if (Setup.SplitEditedFiles) {
                    toFile = toFileName->NextFile();
                    if (toFile < 0) {
                       error = "toFile 2";
                       break;
                       }
                    FileSize = 0;
                    }
                 }
              // the 'else' case (i.e. 'final end mark reached') is handled above
              // in 'Write one frame', so that the edited version will end right
              // before the next I-frame.
              }
           }
     }
  else
     esyslog(LOG_ERR, "no editing marks found!");
  dsyslog(LOG_INFO, "end video cutting thread");
}

// --- cVideoCutter ----------------------------------------------------------

char *cVideoCutter::editedVersionName = NULL;
cCuttingBuffer *cVideoCutter::cuttingBuffer = NULL;
bool cVideoCutter::error = false;
bool cVideoCutter::ended = false;

bool cVideoCutter::Start(const char *FileName)
{
  if (!cuttingBuffer) {
     error = false;
     ended = false;
     cRecording Recording(FileName);
     const char *evn = Recording.PrefixFileName('%');
     if (evn && RemoveVideoFile(evn) && MakeDirs(evn, true)) {
        // XXX this can be removed once RenameVideoFile() follows symlinks (see videodir.c)
        // remove a possible deleted recording with the same name to avoid symlink mixups:
        char *s = strdup(evn);
        char *e = strrchr(s, '.');
        if (e) {
           if (strcmp(e, ".rec") == 0) {
              strcpy(e, ".del");
              RemoveVideoFile(s);
              }
           }
        delete s;
        // XXX
        editedVersionName = strdup(evn);
        Recording.WriteSummary();
        cuttingBuffer = new cCuttingBuffer(FileName, editedVersionName);
        return true;
        }
     }
  return false;
}

void cVideoCutter::Stop(void)
{
  bool Interrupted = cuttingBuffer && cuttingBuffer->Active();
  const char *Error = cuttingBuffer ? cuttingBuffer->Error() : NULL;
  delete cuttingBuffer;
  cuttingBuffer = NULL;
  if ((Interrupted || Error) && editedVersionName) {
     if (Interrupted)
        isyslog(LOG_INFO, "editing process has been interrupted");
     if (Error)
        esyslog(LOG_ERR, "ERROR: '%s' during editing process", Error);
     RemoveVideoFile(editedVersionName); //XXX what if this file is currently being replayed?
     }
}

bool cVideoCutter::Active(void)
{
  if (cuttingBuffer) {
     if (cuttingBuffer->Active())
        return true;
     error = cuttingBuffer->Error();
     Stop();
     if (!error)
        cRecordingUserCommand::InvokeCommand(RUC_EDITEDRECORDING, editedVersionName);
     delete editedVersionName;
     editedVersionName = NULL;
     ended = true;
     }
  return false;
}

bool cVideoCutter::Error(void)
{
  bool result = error;
  error = false;
  return result;
}

bool cVideoCutter::Ended(void)
{
  bool result = ended;
  ended = false;
  return result;
}

// --- cDvbApi ---------------------------------------------------------------

static const char *OstName(const char *Name, int n)
{
  static char buffer[_POSIX_PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d", Name, n);
  return buffer;
}

static int OstOpen(const char *Name, int n, int Mode, bool ReportError = false)
{
  const char *FileName = OstName(Name, n);
  int fd = open(FileName, Mode);
  if (fd < 0 && ReportError)
     LOG_ERROR_STR(FileName);
  return fd;
}

int cDvbApi::NumDvbApis = 0;
int cDvbApi::useDvbApi = 0;
cDvbApi *cDvbApi::dvbApi[MAXDVBAPI] = { NULL };
cDvbApi *cDvbApi::PrimaryDvbApi = NULL;
char *cDvbApi::audioCommand = NULL;

cDvbApi::cDvbApi(int n)
{
  frontendType = FrontendType(-1); // don't know how else to initialize this - there is no FE_UNKNOWN
  vPid = aPid1 = aPid2 = dPid1 = dPid2 = 0;
  siProcessor = NULL;
  recordBuffer = NULL;
  replayBuffer = NULL;
  transferBuffer = NULL;
  transferringFromDvbApi = NULL;
  ca = -1;
  priority = DEFAULTPRIORITY;
  cardIndex = n;

  // Devices that are only present on DVB-C or DVB-S cards:

  fd_frontend = OstOpen(DEV_OST_FRONTEND, n, O_RDWR);
  fd_sec      = OstOpen(DEV_OST_SEC,      n, O_RDWR);

  // Devices that all DVB cards must have:

  fd_demuxv  = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);
  fd_demuxa1 = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);
  fd_demuxa2 = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);
  fd_demuxd1 = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);
  fd_demuxd2 = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);
  fd_demuxt  = OstOpen(DEV_OST_DEMUX,  n, O_RDWR | O_NONBLOCK, true);

  // Devices not present on "budget" cards:

  fd_osd     = OstOpen(DEV_OST_OSD,    n, O_RDWR);
  fd_video   = OstOpen(DEV_OST_VIDEO,  n, O_RDWR | O_NONBLOCK);
  fd_audio   = OstOpen(DEV_OST_AUDIO,  n, O_RDWR | O_NONBLOCK);

  // Devices that will be dynamically opened and closed when necessary:

  fd_dvr     = -1;

  // Video format:

  SetVideoFormat(Setup.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);

  // We only check the devices that must be present - the others will be checked before accessing them:

  if (fd_frontend >= 0 && fd_demuxv >= 0 && fd_demuxa1 >= 0 && fd_demuxa2 >= 0 && fd_demuxd1 >= 0 && fd_demuxd2 >= 0 && fd_demuxt >= 0) {
     siProcessor = new cSIProcessor(OstName(DEV_OST_DEMUX, n));
     FrontendInfo feinfo;
     CHECK(ioctl(fd_frontend, FE_GET_INFO, &feinfo));
     frontendType = feinfo.type;
     }
  else
     esyslog(LOG_ERR, "ERROR: can't open video device %d", n);
  cols = rows = 0;

#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
  initscr();
  keypad(stdscr, true);
  nonl();
  cbreak();
  noecho();
  timeout(10);
#endif
#if defined(DEBUG_OSD)
  memset(&colorPairs, 0, sizeof(colorPairs));
  start_color();
  leaveok(stdscr, true);
  window = NULL;
#else
  osd = NULL;
#endif
  currentChannel = 1;
  mute = false;
  volume = Setup.CurrentVolume;
}

cDvbApi::~cDvbApi()
{
  delete siProcessor;
  Close();
  StopReplay();
  StopRecord();
  StopTransfer();
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
  endwin();
#endif
}

void cDvbApi::SetUseDvbApi(int n)
{
  if (n < MAXDVBAPI)
     useDvbApi |= (1 << n);
}

bool cDvbApi::SetPrimaryDvbApi(int n)
{
  n--;
  if (0 <= n && n < NumDvbApis && dvbApi[n]) {
     isyslog(LOG_INFO, "setting primary DVB to %d", n + 1);
     PrimaryDvbApi = dvbApi[n];
     return true;
     }
  esyslog(LOG_ERR, "invalid DVB interface: %d", n + 1);
  return false;
}

int cDvbApi::CanShift(int Ca, int Priority, int UsedCards)
{
  // Test whether a recording on this DVB device can be shifted to another one
  // in order to perform a new recording with the given Ca and Priority on this device:
  int ShiftLevel = -1; // default means this device can't be shifted
  if (UsedCards & (1 << CardIndex()) != 0)
     return ShiftLevel; // otherwise we would get into a loop
  if (Recording()) {
     if (ProvidesCa(Ca) // this device provides the requested Ca
        && (Ca != this->Ca() // the requested Ca is different from the one currently used...
           || Priority > this->Priority())) { // ...or the request comes from a higher priority
        cDvbApi *d = NULL;
        int Provides[MAXDVBAPI];
        UsedCards |= (1 << CardIndex());
        for (int i = 0; i < NumDvbApis; i++) {
            if ((Provides[i] = dvbApi[i]->ProvidesCa(this->Ca())) != 0) { // this device is basicly able to do the job
               if (dvbApi[i] != this) { // it is not _this_ device
                  int sl = dvbApi[i]->CanShift(this->Ca(), Priority, UsedCards); // this is the original Priority!
                  if (sl >= 0 && (ShiftLevel < 0 || sl < ShiftLevel)) {
                     d = dvbApi[i];
                     ShiftLevel = sl;
                     }
                  }
               }
            }
        if (ShiftLevel >= 0)
           ShiftLevel++; // adds the device's own shift
        }
     }
  else if (Priority > this->Priority())
     ShiftLevel = 0; // no shifting necessary, this device can do the job
  return ShiftLevel;
}

cDvbApi *cDvbApi::GetDvbApi(int Ca, int Priority)
{
  cDvbApi *d = NULL;
  int Provides[MAXDVBAPI];
  // Check which devices provide Ca:
  for (int i = 0; i < NumDvbApis; i++) {
      if ((Provides[i] = dvbApi[i]->ProvidesCa(Ca)) != 0) { // this device is basicly able to do the job
         if (Priority > dvbApi[i]->Priority() // Priority is high enough to use this device
            && (!d // we don't have a device yet, or...
               || dvbApi[i]->Priority() < d->Priority() // ...this one has an even lower Priority
               || (dvbApi[i]->Priority() == d->Priority() // ...same Priority...
                  && Provides[i] < Provides[d->CardIndex()]))) // ...but this one provides fewer Ca values
            d = dvbApi[i];
         }
      }
  if (!d && Ca > MAXDVBAPI) {
     // We didn't find one the easy way, so now we have to try harder:
     int ShiftLevel = -1;
     for (int i = 0; i < NumDvbApis; i++) {
         if (Provides[i]) { // this device is basicly able to do the job, but for some reason we didn't get it above
            int sl = dvbApi[i]->CanShift(Ca, Priority); // asks this device to shift its job to another device
            if (sl >= 0 && (ShiftLevel < 0 || sl < ShiftLevel)) {
               d = dvbApi[i]; // found one that can be shifted with the fewest number of subsequent shifts
               ShiftLevel = sl;
               }
            }
         }
     }
  return d;
}

void cDvbApi::SetCaCaps(void)
{
  for (int d = 0; d < NumDvbApis; d++) {
      for (int i = 0; i < MAXCACAPS; i++)
          dvbApi[d]->caCaps[i] = Setup.CaCaps[dvbApi[d]->CardIndex()][i];
      }
}

int cDvbApi::ProvidesCa(int Ca)
{
  if (Ca == CardIndex() + 1)
     return 1; // exactly _this_ card was requested
  if (Ca && Ca <= MAXDVBAPI)
     return 0; // a specific card was requested, but not _this_ one
  int result = Ca ? 0 : 1; // by default every card can provide FTA
  int others = Ca ? 1 : 0;
  for (int i = 0; i < MAXCACAPS; i++) {
      if (caCaps[i]) {
         if (caCaps[i] == Ca)
            result = 1;
         else
            others++;
         }
      }
  return result ? result + others : 0;
}

bool cDvbApi::Probe(const char *FileName)
{
  if (access(FileName, F_OK) == 0) {
     dsyslog(LOG_INFO, "probing %s", FileName);
     int f = open(FileName, O_RDONLY);
     if (f >= 0) {
        close(f);
        return true;
        }
     else if (errno != ENODEV && errno != EINVAL)
        LOG_ERROR_STR(FileName);
     }
  else if (errno != ENOENT)
     LOG_ERROR_STR(FileName);
  return false;
}

bool cDvbApi::Init(void)
{
  NumDvbApis = 0;
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (useDvbApi == 0 || (useDvbApi & (1 << i)) != 0) {
         if (Probe(OstName(DEV_OST_FRONTEND, i)))
            dvbApi[NumDvbApis++] = new cDvbApi(i);
         else
            break;
         }
      }
  PrimaryDvbApi = dvbApi[0];
  if (NumDvbApis > 0)
     isyslog(LOG_INFO, "found %d video device%s", NumDvbApis, NumDvbApis > 1 ? "s" : "");
  else
     esyslog(LOG_ERR, "ERROR: no video device found, giving up!");
  SetCaCaps();
  return NumDvbApis > 0;
}

void cDvbApi::Cleanup(void)
{
  for (int i = 0; i < NumDvbApis; i++) {
      delete dvbApi[i];
      dvbApi[i] = NULL;
      }
  PrimaryDvbApi = NULL;
}

bool cDvbApi::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  int videoDev = OstOpen(DEV_VIDEO, CardIndex(), O_RDWR, true);
  if (videoDev >= 0) {
     int result = 0;
     struct video_mbuf mbuf;
     result |= ioctl(videoDev, VIDIOCGMBUF, &mbuf);
     if (result == 0) {
        int msize = mbuf.size;
        unsigned char *mem = (unsigned char *)mmap(0, msize, PROT_READ | PROT_WRITE, MAP_SHARED, videoDev, 0);
        if (mem && mem != (unsigned char *)-1) {
           // set up the size and RGB
           struct video_capability vc;
           result |= ioctl(videoDev, VIDIOCGCAP, &vc);
           struct video_mmap vm;
           vm.frame = 0;
           if ((SizeX > 0) && (SizeX <= vc.maxwidth) &&
               (SizeY > 0) && (SizeY <= vc.maxheight)) {
              vm.width = SizeX;
              vm.height = SizeY;
              }
           else {
              vm.width = vc.maxwidth;
              vm.height = vc.maxheight;
              }
           vm.format = VIDEO_PALETTE_RGB24;
           result |= ioctl(videoDev, VIDIOCMCAPTURE, &vm);
           result |= ioctl(videoDev, VIDIOCSYNC, &vm.frame);
           // make RGB out of BGR:
           int memsize = vm.width * vm.height;
           unsigned char *mem1 = mem;
           for (int i = 0; i < memsize; i++) {
               unsigned char tmp = mem1[2];
               mem1[2] = mem1[0];
               mem1[0] = tmp;
               mem1 += 3;
               }
         
           if (Quality < 0)
              Quality = 255; //XXX is this 'best'???
         
           isyslog(LOG_INFO, "grabbing to %s (%s %d %d %d)", FileName, Jpeg ? "JPEG" : "PNM", Quality, vm.width, vm.height);
           FILE *f = fopen(FileName, "wb");
           if (f) {
              if (Jpeg) {
                 // write JPEG file:
                 struct jpeg_compress_struct cinfo;
                 struct jpeg_error_mgr jerr;
                 cinfo.err = jpeg_std_error(&jerr);
                 jpeg_create_compress(&cinfo);
                 jpeg_stdio_dest(&cinfo, f);
                 cinfo.image_width = vm.width;
                 cinfo.image_height = vm.height;
                 cinfo.input_components = 3;
                 cinfo.in_color_space = JCS_RGB;
         
                 jpeg_set_defaults(&cinfo);
                 jpeg_set_quality(&cinfo, Quality, true);
                 jpeg_start_compress(&cinfo, true);
         
                 int rs = vm.width * 3;
                 JSAMPROW rp[vm.height];
                 for (int k = 0; k < vm.height; k++)
                     rp[k] = &mem[rs * k];
                 jpeg_write_scanlines(&cinfo, rp, vm.height);
                 jpeg_finish_compress(&cinfo);
                 jpeg_destroy_compress(&cinfo);
                 }
              else {
                 // write PNM file:
                 if (fprintf(f, "P6\n%d\n%d\n255\n", vm.width, vm.height) < 0 ||
                     fwrite(mem, vm.width * vm.height * 3, 1, f) < 0) {
                    LOG_ERROR_STR(FileName);
                    result |= 1;
                    }
                 }
              fclose(f);
              }
           else {
              LOG_ERROR_STR(FileName);
              result |= 1;
              }
           munmap(mem, msize);
           }
        else
           result |= 1;
        }
     close(videoDev);
     return result == 0;
     }
  return false;
}

#ifdef DEBUG_OSD
void cDvbApi::SetColor(eDvbColor colorFg, eDvbColor colorBg)
{
  int color = (colorBg << 16) | colorFg | 0x80000000;
  for (int i = 0; i < MaxColorPairs; i++) {
      if (!colorPairs[i]) {
         colorPairs[i] = color;
         init_pair(i + 1, colorFg, colorBg);
         wattrset(window, COLOR_PAIR(i + 1));
         break;
         }
      else if (color == colorPairs[i]) {
         wattrset(window, COLOR_PAIR(i + 1));
         break;
         }
      }
}
#endif

void cDvbApi::Open(int w, int h)
{
  int d = (h < 0) ? Setup.OSDheight + h : 0;
  h = abs(h);
  cols = w;
  rows = h;
#ifdef DEBUG_OSD
  window = subwin(stdscr, h, w, d, (Setup.OSDwidth - w) / 2);
  syncok(window, true);
  #define B2C(b) (((b) * 1000) / 255)
  #define SETCOLOR(n, r, g, b, o) init_color(n, B2C(r), B2C(g), B2C(b))
  //XXX
  SETCOLOR(clrBackground,  0x00, 0x00, 0x00, 127); // background 50% gray
  SETCOLOR(clrBlack,       0x00, 0x00, 0x00, 255);
  SETCOLOR(clrRed,         0xFC, 0x14, 0x14, 255);
  SETCOLOR(clrGreen,       0x24, 0xFC, 0x24, 255);
  SETCOLOR(clrYellow,      0xFC, 0xC0, 0x24, 255);
  SETCOLOR(clrBlue,        0x00, 0x00, 0xFC, 255);
  SETCOLOR(clrCyan,        0x00, 0xFC, 0xFC, 255);
  SETCOLOR(clrMagenta,     0xB0, 0x00, 0xFC, 255);
  SETCOLOR(clrWhite,       0xFC, 0xFC, 0xFC, 255);
#else
  w *= charWidth;
  h *= lineHeight;
  d *= lineHeight;
  int x = (720 - w + charWidth) / 2; //TODO PAL vs. NTSC???
  int y = (576 - Setup.OSDheight * lineHeight) / 2 + d;
  //XXX
  osd = new cDvbOsd(fd_osd, x, y);
  //XXX TODO this should be transferred to the places where the individual windows are requested (there's too much detailed knowledge here!)
  if (h / lineHeight == 5) { //XXX channel display
     osd->Create(0,              0, w, h, 4);
     }
  else if (h / lineHeight == 1) { //XXX info display
     osd->Create(0,              0, w, h, 4);
     }
  else if (d == 0) { //XXX full menu
     osd->Create(0,                            0, w,                         lineHeight, 2);
     osd->Create(0,                   lineHeight, w, (Setup.OSDheight - 3) * lineHeight, 2);
     osd->AddColor(clrBackground);
     osd->AddColor(clrCyan);
     osd->AddColor(clrWhite);
     osd->AddColor(clrBlack);
     osd->Create(0, (Setup.OSDheight - 2) * lineHeight, w,               2 * lineHeight, 4);
     }
  else { //XXX progress display
     /*XXX
     osd->Create(0,              0, w, lineHeight, 1);
     osd->Create(0,     lineHeight, w, lineHeight, 2, false);
     osd->Create(0, 2 * lineHeight, w, lineHeight, 1);
     XXX*///XXX some pixels are not drawn correctly with lower bpp values
     osd->Create(0,              0, w, 3*lineHeight, 4);
     }
#endif
}

void cDvbApi::Close(void)
{
#ifdef DEBUG_OSD
  if (window) {
     delwin(window);
     window = 0;
     }
#else
  delete osd;
  osd = NULL;
#endif
}

void cDvbApi::Clear(void)
{
#ifdef DEBUG_OSD
  SetColor(clrBackground, clrBackground);
  Fill(0, 0, cols, rows, clrBackground);
#else
  osd->Clear();
#endif
}

void cDvbApi::Fill(int x, int y, int w, int h, eDvbColor color)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(color, color);
  for (int r = 0; r < h; r++) {
      wmove(window, y + r, x); // ncurses wants 'y' before 'x'!
      whline(window, ' ', w);
      }
  wsyncup(window); // shouldn't be necessary because of 'syncok()', but w/o it doesn't work
#else
  osd->Fill(x * charWidth, y * lineHeight, (x + w) * charWidth - 1, (y + h) * lineHeight - 1, color);
#endif
}

void cDvbApi::SetBitmap(int x, int y, const cBitmap &Bitmap)
{
#ifndef DEBUG_OSD
  osd->SetBitmap(x, y, Bitmap);
#endif
}

void cDvbApi::ClrEol(int x, int y, eDvbColor color)
{
  Fill(x, y, cols - x, 1, color);
}

int cDvbApi::CellWidth(void)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return charWidth;
#endif
}

int cDvbApi::LineHeight(void)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return lineHeight;
#endif
}

int cDvbApi::Width(unsigned char c)
{
#ifdef DEBUG_OSD
  return 1;
#else
  return osd->Width(c);
#endif
}

int cDvbApi::WidthInCells(const char *s)
{
#ifdef DEBUG_OSD
  return strlen(s);
#else
  return (osd->Width(s) + charWidth - 1) / charWidth;
#endif
}

eDvbFont cDvbApi::SetFont(eDvbFont Font)
{
#ifdef DEBUG_OSD
  return Font;
#else
  return osd->SetFont(Font);
#endif
}

void cDvbApi::Text(int x, int y, const char *s, eDvbColor colorFg, eDvbColor colorBg)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(colorFg, colorBg);
  wmove(window, y, x); // ncurses wants 'y' before 'x'!
  waddnstr(window, s, cols - x);
#else
  osd->Text(x * charWidth, y * lineHeight, s, colorFg, colorBg);
#endif
}

void cDvbApi::Flush(void)
{
#ifdef DEBUG_OSD
  refresh();
#else
  if (osd)
     osd->Flush();
#endif
}

int cDvbApi::Priority(void)
{
  return (this == PrimaryDvbApi && !Recording()) ? Setup.PrimaryLimit - 1 : priority;
}

int cDvbApi::SetModeRecord(void)
{
  // Sets up the DVB device for recording

  SetPids(true);
  if (fd_dvr >= 0)
     close(fd_dvr);
  fd_dvr = OstOpen(DEV_OST_DVR, CardIndex(), O_RDONLY | O_NONBLOCK);
  if (fd_dvr < 0)
     LOG_ERROR;
  return fd_dvr;
}

void cDvbApi::SetModeReplay(void)
{
  // Sets up the DVB device for replay

  if (fd_video >= 0 && fd_audio >= 0) {
     if (siProcessor)
        siProcessor->SetStatus(false);
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     CHECK(ioctl(fd_audio, AUDIO_PLAY));
     CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
     CHECK(ioctl(fd_video, VIDEO_PLAY));
     }
}

void cDvbApi::SetModeNormal(bool FromRecording)
{
  // Puts the DVB device back into "normal" viewing mode (after replay or recording)

  if (FromRecording) {
     close(fd_dvr);
     fd_dvr = -1;
     SetPids(false);
     }
  else {
     if (fd_video >= 0 && fd_audio >= 0) {
        CHECK(ioctl(fd_video, VIDEO_STOP, true));
        CHECK(ioctl(fd_audio, AUDIO_STOP, true));
        CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
        CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
        CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX));
        CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX));
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
        CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
        if (siProcessor)
           siProcessor->SetStatus(true);
        }
     }
}

void cDvbApi::SetVideoFormat(videoFormat_t Format)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, Format));
}

bool cDvbApi::SetPid(int fd, dmxPesType_t PesType, int Pid, dmxOutput_t Output)
{
  if (Pid) {
     CHECK(ioctl(fd, DMX_STOP));
     if (Pid != 0x1FFF) {
        dmxPesFilterParams pesFilterParams;
        pesFilterParams.pid     = Pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = Output;
        pesFilterParams.pesType = PesType;
        pesFilterParams.flags   = DMX_IMMEDIATE_START;
        if (ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
           LOG_ERROR;
           return false;
           }
        }
     }
  return true;
}

bool cDvbApi::SetPids(bool ForRecording)
{
  return SetVpid(vPid,   ForRecording ? DMX_OUT_TS_TAP : DMX_OUT_DECODER) &&
         SetApid1(aPid1, ForRecording ? DMX_OUT_TS_TAP : DMX_OUT_DECODER) &&
         SetApid2(ForRecording ? aPid2 : 0, DMX_OUT_TS_TAP) && (!Setup.RecordDolbyDigital ||
         SetDpid1(ForRecording ? dPid1 : 0, DMX_OUT_TS_TAP) &&
         SetDpid2(ForRecording ? dPid2 : 0, DMX_OUT_TS_TAP));
}

eSetChannelResult cDvbApi::SetChannel(int ChannelNumber, int Frequency, char Polarization, int Diseqc, int Srate, int Vpid, int Apid1, int Apid2, int Dpid1, int Dpid2, int Tpid, int Ca, int Pnr)
{
  StopTransfer();
  StopReplay();

  // Must set this anyway to avoid getting stuck when switching through
  // channels with 'Up' and 'Down' keys:
  currentChannel = ChannelNumber;
  vPid = Vpid;
  aPid1 = Apid1;
  aPid2 = Apid2;
  dPid1 = Dpid1;
  dPid2 = Dpid2;

  // Avoid noise while switching:

  if (fd_video >= 0 && fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
     }

  // Stop setting system time:

  if (siProcessor)
     siProcessor->SetCurrentTransponder(0);

  // If this card can't receive this channel, we must not actually switch
  // the channel here, because that would irritate the driver when we
  // start replaying in Transfer Mode immediately after switching the channel:
  bool NeedsTransferMode = (this == PrimaryDvbApi && !ProvidesCa(Ca));

  if (!NeedsTransferMode) {

     // Turn off current PIDs:

     SetVpid( 0x1FFF, DMX_OUT_DECODER);
     SetApid1(0x1FFF, DMX_OUT_DECODER);
     SetApid2(0x1FFF, DMX_OUT_DECODER);
     SetDpid1(0x1FFF, DMX_OUT_DECODER);
     SetDpid2(0x1FFF, DMX_OUT_DECODER);
     SetTpid( 0x1FFF, DMX_OUT_DECODER);

     FrontendParameters Frontend;

     switch (frontendType) {
       case FE_QPSK: { // DVB-S

            // Frequency offsets:

            unsigned int freq = Frequency;
            int tone = SEC_TONE_OFF;

            if (freq < (unsigned int)Setup.LnbSLOF) {
               freq -= Setup.LnbFrequLo;
               tone = SEC_TONE_OFF;
               }
            else {
               freq -= Setup.LnbFrequHi;
               tone = SEC_TONE_ON;
               }

            Frontend.Frequency = freq * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qpsk.SymbolRate = Srate * 1000UL;
            Frontend.u.qpsk.FEC_inner = FEC_AUTO;

            int volt = (Polarization == 'v' || Polarization == 'V') ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;

            // DiseqC:

            secCommand scmd;
            scmd.type = 0;
            scmd.u.diseqc.addr = 0x10;
            scmd.u.diseqc.cmd = 0x38;
            scmd.u.diseqc.numParams = 1;
            scmd.u.diseqc.params[0] = 0xF0 | ((Diseqc * 4) & 0x0F) | (tone == SEC_TONE_ON ? 1 : 0) | (volt == SEC_VOLTAGE_18 ? 2 : 0);

            secCmdSequence scmds;
            scmds.voltage = volt;
            scmds.miniCommand = SEC_MINI_NONE;
            scmds.continuousTone = tone;
            scmds.numCommands = Setup.DiSEqC ? 1 : 0;
            scmds.commands = &scmd;

            CHECK(ioctl(fd_sec, SEC_SEND_SEQUENCE, &scmds));
            }
            break;
       case FE_QAM: { // DVB-C

            // Frequency and symbol rate:

            Frontend.Frequency = Frequency * 1000000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.qam.SymbolRate = Srate * 1000UL;
            Frontend.u.qam.FEC_inner = FEC_AUTO;
            Frontend.u.qam.QAM = QAM_64;
            }
            break;
       case FE_OFDM: { // DVB-T

            // Frequency and OFDM paramaters:

            Frontend.Frequency = Frequency * 1000UL;
            Frontend.Inversion = INVERSION_AUTO;
            Frontend.u.ofdm.bandWidth=BANDWIDTH_8_MHZ;
            Frontend.u.ofdm.HP_CodeRate=FEC_2_3;
            Frontend.u.ofdm.LP_CodeRate=FEC_1_2;
            Frontend.u.ofdm.Constellation=QAM_64;
            Frontend.u.ofdm.TransmissionMode=TRANSMISSION_MODE_2K;
            Frontend.u.ofdm.guardInterval=GUARD_INTERVAL_1_32;
            Frontend.u.ofdm.HierarchyInformation=HIERARCHY_NONE;
            }
            break;
       default:
            esyslog(LOG_ERR, "ERROR: attempt to set channel with unknown DVB frontend type");
            return scrFailed;
       }

     // Tuning:

     CHECK(ioctl(fd_frontend, FE_SET_FRONTEND, &Frontend));

     // Wait for channel sync:

     if (cFile::FileReady(fd_frontend, 5000)) {
        FrontendEvent event;
        int res = ioctl(fd_frontend, FE_GET_EVENT, &event);
        if (res >= 0) {
           if (event.type != FE_COMPLETION_EV) {
              esyslog(LOG_ERR, "ERROR: channel %d not sync'ed on DVB card %d!", ChannelNumber, CardIndex() + 1);
              if (this == PrimaryDvbApi)
                 cThread::RaisePanic();
              return scrFailed;
              }
           }
        else
           esyslog(LOG_ERR, "ERROR %d in frontend get event (channel %d, card %d)", res, ChannelNumber, CardIndex() + 1);
        }
     else
        esyslog(LOG_ERR, "ERROR: timeout while tuning");

     // PID settings:

     if (!SetPids(false)) {
        esyslog(LOG_ERR, "ERROR: failed to set PIDs for channel %d", ChannelNumber);
        return scrFailed;
        }
     SetTpid(Tpid, DMX_OUT_DECODER);
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     }

  if (this == PrimaryDvbApi && siProcessor)
     siProcessor->SetCurrentServiceID(Pnr);

  eSetChannelResult Result = scrOk;

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     cDvbApi *CaDvbApi = GetDvbApi(Ca, 0);
     if (CaDvbApi && !CaDvbApi->Recording()) {
        if ((Result = CaDvbApi->SetChannel(ChannelNumber, Frequency, Polarization, Diseqc, Srate, Vpid, Apid1, Apid2, Dpid1, Dpid2, Tpid, Ca, Pnr)) == scrOk) {
           SetModeReplay();
           transferringFromDvbApi = CaDvbApi->StartTransfer(fd_video);
           }
        }
     else
        Result = scrNoTransfer;
     }

  if (fd_video >= 0 && fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
     }

  // Start setting system time:

  if (Result == scrOk && siProcessor)
     siProcessor->SetCurrentTransponder(Frequency);

  return Result;
}

bool cDvbApi::Transferring(void)
{
  return transferBuffer;
}

cDvbApi *cDvbApi::StartTransfer(int TransferToVideoDev)
{
  StopTransfer();
  transferBuffer = new cTransferBuffer(this, TransferToVideoDev, vPid, aPid1);
  return this;
}

void cDvbApi::StopTransfer(void)
{
  if (transferBuffer) {
     delete transferBuffer;
     transferBuffer = NULL;
     }
  if (transferringFromDvbApi) {
     transferringFromDvbApi->StopTransfer();
     transferringFromDvbApi = NULL;
     }
}

int cDvbApi::SecondsToFrames(int Seconds)
{
  return Seconds * FRAMESPERSEC;
}

bool cDvbApi::Recording(void)
{
  if (recordBuffer && !recordBuffer->Active())
     StopRecord();
  return recordBuffer != NULL;
}

bool cDvbApi::Replaying(void)
{
  if (replayBuffer && !replayBuffer->Active())
     StopReplay();
  return replayBuffer != NULL;
}

bool cDvbApi::StartRecord(const char *FileName, int Ca, int Priority)
{
  if (Recording()) {
     esyslog(LOG_ERR, "ERROR: StartRecord() called while recording - ignored!");
     return false;
     }

  StopTransfer();

  StopReplay(); // TODO: remove this if the driver is able to do record and replay at the same time

  // Check FileName:

  if (!FileName) {
     esyslog(LOG_ERR, "ERROR: StartRecord: file name is (null)");
     return false;
     }
  isyslog(LOG_INFO, "record %s", FileName);

  // Create directories if necessary:

  if (!MakeDirs(FileName, true))
     return false;

  // Make sure the disk is up and running:

  SpinUpDisk(FileName);

  // Create recording buffer:

  recordBuffer = new cRecordBuffer(this, FileName, vPid, aPid1, aPid2, dPid1, dPid2);

  if (recordBuffer) {
     ca = Ca;
     priority = Priority;
     return true;
     }
  else
     esyslog(LOG_ERR, "ERROR: can't allocate recording buffer");

  return false;
}

void cDvbApi::StopRecord(void)
{
  if (recordBuffer) {
     delete recordBuffer;
     recordBuffer = NULL;
     ca = -1;
     priority = DEFAULTPRIORITY;
     }
}

bool cDvbApi::StartReplay(const char *FileName)
{
  if (Recording()) {
     esyslog(LOG_ERR, "ERROR: StartReplay() called while recording - ignored!");
     return false;
     }
  StopTransfer();
  StopReplay();
  if (fd_video >= 0 && fd_audio >= 0) {

     // Check FileName:

     if (!FileName) {
        esyslog(LOG_ERR, "ERROR: StartReplay: file name is (null)");
        return false;
        }
     isyslog(LOG_INFO, "replay %s", FileName);

     // Create replay buffer:

     replayBuffer = new cReplayBuffer(this, fd_video, fd_audio, FileName);
     if (replayBuffer)
        return true;
     else
        esyslog(LOG_ERR, "ERROR: can't allocate replaying buffer");
     }
  return false;
}

void cDvbApi::StopReplay(void)
{
  if (replayBuffer) {
     delete replayBuffer;
     replayBuffer = NULL;
     if (this == PrimaryDvbApi) {
        // let's explicitly switch the channel back in case it was in Transfer Mode:
        cChannel *Channel = Channels.GetByNumber(currentChannel);
        if (Channel) {
           Channel->Switch(this, false);
           usleep(100000); // allow driver to sync in case a new replay will start immediately
           }
        }
     }
}

void cDvbApi::Pause(void)
{
  if (replayBuffer)
     replayBuffer->Pause();
}

void cDvbApi::Play(void)
{
  if (replayBuffer)
     replayBuffer->Play();
}

void cDvbApi::Forward(void)
{
  if (replayBuffer)
     replayBuffer->Forward();
}

void cDvbApi::Backward(void)
{
  if (replayBuffer)
     replayBuffer->Backward();
}

void cDvbApi::SkipSeconds(int Seconds)
{
  if (replayBuffer)
     replayBuffer->SkipSeconds(Seconds);
}

int cDvbApi::SkipFrames(int Frames)
{
  if (replayBuffer)
     return replayBuffer->SkipFrames(Frames);
  return -1;
}

bool cDvbApi::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (replayBuffer) {
     replayBuffer->GetIndex(Current, Total, SnapToIFrame);
     return true;
     }
  return false;
}

bool cDvbApi::GetReplayMode(bool &Play, bool &Forward, int &Speed)
{
  return replayBuffer && replayBuffer->GetReplayMode(Play, Forward, Speed);
}

void cDvbApi::Goto(int Position, bool Still)
{
  if (replayBuffer)
     replayBuffer->Goto(Position, Still);
}

bool cDvbApi::CanToggleAudioTrack(void)
{
  return replayBuffer ? replayBuffer->CanToggleAudioTrack() : (aPid1 && aPid2 && aPid1 != aPid2);
}

bool cDvbApi::ToggleAudioTrack(void)
{
  if (replayBuffer) {
     replayBuffer->ToggleAudioTrack();
     return true;
     }
  else {
     int a = aPid2;
     aPid2 = aPid1;
     aPid1 = a;
     if (transferringFromDvbApi)
        return transferringFromDvbApi->ToggleAudioTrack();
     else {
        if (transferBuffer)
           transferBuffer->SetAudioPid(aPid1);
        return SetPids(transferBuffer != NULL);
        }
     }
  return false;
}

bool cDvbApi::ToggleMute(void)
{
  int OldVolume = volume;
  mute = !mute;
  SetVolume(0, mute);
  volume = OldVolume;
  return mute;
}

void cDvbApi::SetVolume(int Volume, bool Absolute)
{
  if (fd_audio >= 0) {
     volume = min(max(Absolute ? Volume : volume + Volume, 0), MAXVOLUME);
     audioMixer_t am;
     am.volume_left = am.volume_right = volume;
     CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
     if (volume > 0)
        mute = false;
     }
}

void cDvbApi::SetAudioCommand(const char *Command)
{
  delete audioCommand;
  audioCommand = strdup(Command);
}

// --- cEITScanner -----------------------------------------------------------

cEITScanner::cEITScanner(void)
{
  lastScan = lastActivity = time(NULL);
  currentChannel = 0;
  lastChannel = 0;
  numTransponders = 0;
  transponders = NULL;
}

cEITScanner::~cEITScanner()
{
  delete transponders;
}

bool cEITScanner::TransponderScanned(cChannel *Channel)
{
  for (int i = 0; i < numTransponders; i++) {
      if (transponders[i] == Channel->frequency)
         return true;
      }
  transponders = (int *)realloc(transponders, ++numTransponders * sizeof(int));
  transponders[numTransponders - 1] = Channel->frequency;
  return false;
}

void cEITScanner::Activity(void)
{
  if (currentChannel) {
     Channels.SwitchTo(currentChannel);
     currentChannel = 0;
     }
  lastActivity = time(NULL);
}

void cEITScanner::Process(void)
{
  if (Setup.EPGScanTimeout && Channels.MaxNumber() > 1) {
     time_t now = time(NULL);
     if (now - lastScan > ScanTimeout && now - lastActivity > ActivityTimeout) {
        for (int i = 0; i < MAXDVBAPI; i++) {
            cDvbApi *DvbApi = cDvbApi::GetDvbApi(i + 1, MAXPRIORITY + 1);
            if (DvbApi) {
               if (DvbApi != cDvbApi::PrimaryDvbApi || (cDvbApi::NumDvbApis == 1 && Setup.EPGScanTimeout && now - lastActivity > Setup.EPGScanTimeout * 3600)) {
                  if (!(DvbApi->Recording() || DvbApi->Replaying() || DvbApi->Transferring())) {
                     int oldCh = lastChannel;
                     int ch = oldCh + 1;
                     while (ch != oldCh) {
                           if (ch > Channels.MaxNumber()) {
                              ch = 1;
                              numTransponders = 0;
                              }
                           cChannel *Channel = Channels.GetByNumber(ch);
                           if (Channel) {
                              if (Channel->ca <= MAXDVBAPI && !DvbApi->ProvidesCa(Channel->ca))
                                 break; // the channel says it explicitly needs a different card
                              if (Channel->pnr && !TransponderScanned(Channel)) {
                                 if (DvbApi == cDvbApi::PrimaryDvbApi && !currentChannel)
                                    currentChannel = DvbApi->Channel();
                                 Channel->Switch(DvbApi, false);
                                 lastChannel = ch;
                                 break;
                                 }
                              }
                           ch++;
                           }
                     }
                  }
               }
            }
        lastScan = time(NULL);
        }
     }
}

