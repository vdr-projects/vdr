/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.94 2001/07/29 09:00:19 kls Exp $
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

#define DEV_VIDEO      "/dev/video"
#define DEV_OST_OSD    "/dev/ost/osd"
#define DEV_OST_QAMFE  "/dev/ost/qamfe"
#define DEV_OST_QPSKFE "/dev/ost/qpskfe"
#define DEV_OST_SEC    "/dev/ost/sec"
#define DEV_OST_DVR    "/dev/ost/dvr"
#define DEV_OST_DEMUX  "/dev/ost/demux"
#define DEV_OST_VIDEO  "/dev/ost/video"
#define DEV_OST_AUDIO  "/dev/ost/audio"

// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE (1024*1024)

// The maximum size of a single frame:
#define MAXFRAMESIZE (192*1024)

#define FRAMESPERSEC 25

// The maximum file size is limited by the range that can be covered
// with 'int'. 4GB might be possible (if the range is considered
// 'unsigned'), 2GB should be possible (even if the range is considered
// 'signed'), so let's use 1GB for absolute safety (the actual file size
// may be slightly higher because we stop recording only before the next
// 'I' frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZE (1024*1024*1024) // Byte
#define MAXFILESPERRECORDING 255

#define MINFREEDISKSPACE    (512) // MB
#define DISKCHECKINTERVAL   100 // seconds

#define INDEXFILESUFFIX     "/index.vdr"
#define RECORDFILESUFFIX    "/%03d.vdr"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...

// The number of frames to back up when resuming an interrupted replay session:
#define RESUMEBACKUP (10 * FRAMESPERSEC)

#define CHECK(s) { if ((s) < 0) LOG_ERROR; } // used for 'ioctl()' calls

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
  char *fileName, *pFileExt;
  int size, last;
  tIndex *index;
  cResumeFile resumeFile;
  bool CatchUp(void);
public:
  cIndexFile(const char *FileName, bool Record);
  ~cIndexFile();
  bool Ok(void) { return index != NULL; }
  void Write(uchar PictureType, uchar FileNumber, int FileOffset);
  bool Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType = NULL, int *Length = NULL);
  int GetNextIFrame(int Index, bool Forward, uchar *FileNumber = NULL, int *FileOffset = NULL, int *Length = NULL);
  int Get(uchar FileNumber, int FileOffset);
  int Last(void) { CatchUp(); return last; }
  int GetResume(void) { return resumeFile.Read(); }
  bool StoreResume(int Index) { return resumeFile.Save(Index); }
  };

cIndexFile::cIndexFile(const char *FileName, bool Record)
:resumeFile(FileName)
{
  f = -1;
  fileName = pFileExt = NULL;
  size = 0;
  last = -1;
  index = NULL;
  if (FileName) {
     fileName = new char[strlen(FileName) + strlen(INDEXFILESUFFIX) + 1];
     if (fileName) {
        strcpy(fileName, FileName);
        pFileExt = fileName + strlen(fileName);
        strcpy(pFileExt, INDEXFILESUFFIX);
        int delta = 0;
        if (access(fileName, R_OK) == 0) {
           struct stat buf;
           if (stat(fileName, &buf) == 0) {
              delta = buf.st_size % sizeof(tIndex);
              if (delta) {
                 delta = sizeof(tIndex) - delta;
                 esyslog(LOG_ERR, "ERROR: invalid file size (%d) in '%s'", buf.st_size, fileName);
                 }
              last = (buf.st_size + delta) / sizeof(tIndex) - 1;
              if (!Record && last >= 0) {
                 size = last + 1;
                 index = new tIndex[size];
                 if (index) {
                    f = open(fileName, O_RDONLY);
                    if (f >= 0) {
                       if ((int)read(f, index, buf.st_size) != buf.st_size) {
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
           delete fileName;
           fileName = pFileExt = NULL;
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
}

bool cIndexFile::CatchUp(void)
{
  if (index && f >= 0) {
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
                 if (read(f, &index[last + 1], delta) != delta) {
                    esyslog(LOG_ERR, "ERROR: can't read from index");
                    delete index;
                    index = NULL;
                    close(f);
                    f = -1;
                    }
                 last = newLast;
                 return true;
                 }
              else
                 LOG_ERROR;
              }
           else
              esyslog(LOG_ERR, "ERROR: can't realloc() index");
           }
        }
     else
        LOG_ERROR;
     }
  return false;
}

void cIndexFile::Write(uchar PictureType, uchar FileNumber, int FileOffset)
{
  if (f >= 0) {
     tIndex i = { FileOffset, PictureType, FileNumber, 0 };
     if (write(f, &i, sizeof(i)) != sizeof(i)) {
        esyslog(LOG_ERR, "ERROR: can't write to index file");
        close(f);
        f = -1;
        return;
        }
     last++;
     }
}

bool cIndexFile::Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType, int *Length)
{
  if (index) {
     CatchUp();
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

int cIndexFile::GetNextIFrame(int Index, bool Forward, uchar *FileNumber, int *FileOffset, int *Length)
{
  if (index) {
     if (Forward)
        CatchUp();
     int d = Forward ? 1 : -1;
     for (;;) {
         Index += d;
         if (Index >= 0 && Index <= last - 100) { // '- 100': need to stay off the end!
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

class cRecordBuffer : public cRingBuffer {
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
:cRingBuffer(VIDEOBUFSIZE, true)
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
     uint Free = FreeDiskSpaceMB(fileName.Name());
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
     if (fileSize > MAXVIDEOFILESIZE || RunningLowOnDiskSpace()) {
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
      int r = read(videoDev, b, sizeof(b));
      if (r > 0) {
         uchar *p = b;
         while (r > 0) {
               int w = Put(p, r);
               p += w;
               r -= w;
               }
         t = time(NULL);
         }
      else if (r < 0) {
         if (errno != EAGAIN) {
            LOG_ERROR;
            if (errno != EBUFFEROVERFLOW)
               break;
            }
         }
      if (time(NULL) - t > 10) {
         esyslog(LOG_ERR, "ERROR: video data stream broken");
         cThread::EmergencyExit(true);
         t = time(NULL);
         }
      cFile::FileReady(videoDev, 100);
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
      if (g > 0) {
         r += g;
         int Count = r, Result;
         const uchar *p = remux.Process(b, Count, Result, &pictureType);
         if (p) {
            if (!Busy() && pictureType == I_FRAME) // finish the recording before the next 'I' frame
               break;
            if (NextFile()) {
               if (index && pictureType != NO_PICTURE)
                  index->Write(pictureType, fileName.Number(), fileSize);
               while (Result > 0) {
                     int w = write(recordFile, p, Result);
                     if (w < 0) {
                        LOG_ERROR_STR(fileName.Name());
                        recording = false;
                        return;
                        }
                     p += w;
                     Result -= w;
                     fileSize += w;
                     }
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
  int r = read(f, b, Length);
  if (r < 0)
     LOG_ERROR;
  return r;
}

// --- cReplayBuffer ---------------------------------------------------------

class cReplayBuffer : public cRingBuffer {
private:
  cDvbApi *dvbApi;
  cIndexFile *index;
  cFileName fileName;
  int fileOffset;
  int videoDev, audioDev;
  FILE *dolbyDev;
  int replayFile;
  bool eof;
  int blockInput, blockOutput;
  bool paused, fastForward, fastRewind;
  int lastIndex, stillIndex, playIndex;
  bool canToggleAudioTrack;
  uchar audioTrack;
  bool NextFile(uchar FileNumber = 0, int FileOffset = -1);
  void Clear(bool Block = false);
  void Close(void);
  void StripAudioPackets(uchar *b, int Length, uchar Except = 0x00);
  void DisplayFrame(uchar *b, int Length);
  int Resume(void);
  bool Save(void);
protected:
  virtual void Input(void);
  virtual void Output(void);
public:
  cReplayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev, const char *FileName);
  virtual ~cReplayBuffer();
  void Pause(void);
  void Play(void);
  void Forward(void);
  void Backward(void);
  int SkipFrames(int Frames);
  void SkipSeconds(int Seconds);
  void Goto(int Position, bool Still = false);
  void GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
  bool CanToggleAudioTrack(void) { return canToggleAudioTrack; }
  void ToggleAudioTrack(void);
  };

cReplayBuffer::cReplayBuffer(cDvbApi *DvbApi, int VideoDev, int AudioDev, const char *FileName)
:cRingBuffer(VIDEOBUFSIZE)
,fileName(FileName, false)
{
  dvbApi = DvbApi;
  index = NULL;
  fileOffset = 0;
  videoDev = VideoDev;
  audioDev = AudioDev;
  dolbyDev = NULL;
  if (cDvbApi::AudioCommand()) {
     dolbyDev = popen(cDvbApi::AudioCommand(), "w");
     if (!dolbyDev)
        esyslog(LOG_ERR, "ERROR: can't open pipe to audio command '%s'", cDvbApi::AudioCommand());
     }
  replayFile = fileName.Open();
  eof = false;
  blockInput = blockOutput = false;
  paused = fastForward = fastRewind = false;
  lastIndex = stillIndex = playIndex = -1;
  canToggleAudioTrack = false;
  audioTrack = 0xC0;
  if (!fileName.Name())
     return;
  // Create the index file:
  index = new cIndexFile(FileName, false);
  if (!index) {
     esyslog(LOG_ERR, "ERROR: can't allocate index");
     }
  else if (!index->Ok()) {
     delete index;
     index = NULL;
     }
  dvbApi->SetModeReplay();
  Start();
}

cReplayBuffer::~cReplayBuffer()
{
  Stop();
  Save();
  Close();
  if (dolbyDev)
     pclose(dolbyDev);
  dvbApi->SetModeNormal(false);
  delete index;
}

void cReplayBuffer::Input(void)
{
  dsyslog(LOG_INFO, "input thread started (pid=%d)", getpid());

  int ResumeIndex = Resume();
  if (ResumeIndex >= 0)
     isyslog(LOG_INFO, "resuming replay at index %d (%s)", ResumeIndex, IndexToHMSF(ResumeIndex, true));

  int lastIndex = -1;
  int brakeCounter = 0;
  uchar b[MAXFRAMESIZE];
  while (Busy() && (blockInput || NextFile())) {
        if (!blockInput && stillIndex < 0) {
           int r = 0;
           if (fastForward && !paused || fastRewind) {
              int Index = (lastIndex >= 0) ? lastIndex : index->Get(fileName.Number(), fileOffset);
              uchar FileNumber;
              int FileOffset, Length;
              if (!paused || (brakeCounter++ % 24) == 0) // show every I_FRAME 24 times in rmSlowRewind mode to achieve roughly the same speed as in slow forward mode
                 Index = index->GetNextIFrame(Index, fastForward, &FileNumber, &FileOffset, &Length);
              if (Index >= 0) {
                 if (!NextFile(FileNumber, FileOffset))
                    break;
                 }
              else {
                 paused = fastForward = fastRewind = false;
                 Play();
                 continue;
                 }
              lastIndex = Index;
              playIndex = -1;
              r = ReadFrame(replayFile, b, Length, sizeof(b));
              StripAudioPackets(b, r);
              }
           else if (index) {
              lastIndex = -1;
              playIndex = (playIndex >= 0) ? playIndex + 1 : index->Get(fileName.Number(), fileOffset);
              uchar FileNumber;
              int FileOffset, Length;
              if (!(index->Get(playIndex, &FileNumber, &FileOffset, NULL, &Length) && NextFile(FileNumber, FileOffset)))
                 break;
              r = ReadFrame(replayFile, b, Length, sizeof(b));
              StripAudioPackets(b, r, audioTrack);
              }
           else // allows replay even if the index file is missing
              r = read(replayFile, b, sizeof(b));
           if (r > 0) {
              uchar *p = b;
              while (r > 0 && Busy() && !blockInput) {
                    int w = Put(p, r);
                    p += w;
                    r -= w;
                    usleep(1); // this keeps the CPU load low
                    }
              }
           else if (r ==0)
              eof = true;
           else if (r < 0 && errno != EAGAIN) {
              LOG_ERROR;
              break;
              }
           }
        else
           usleep(1); // this keeps the CPU load low
        if (blockInput > 1)
           blockInput = 1;
        }

  dsyslog(LOG_INFO, "input thread ended (pid=%d)", getpid());
}

void cReplayBuffer::Output(void)
{
  dsyslog(LOG_INFO, "output thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA];
  while (Busy()) {
        int r = blockOutput ? 0 : Get(b, sizeof(b));
        if (r > 0) {
           uchar *p = b;
           while (r > 0 && Busy() && !blockOutput) {
                 cFile::FileReadyForWriting(videoDev, 100);
                 int w = write(videoDev, p, r);
                 if (w > 0) {
                    p += w;
                    r -= w;
                    fileOffset += w;
                    }
                 else if (w < 0 && errno != EAGAIN) {
                    LOG_ERROR;
                    Stop();
                    return;
                    }
                 }
           }
        else
           usleep(1); // this keeps the CPU load low
        if (blockOutput > 1)
           blockOutput = 1;
        }

  dsyslog(LOG_INFO, "output thread ended (pid=%d)", getpid());
}

void cReplayBuffer::StripAudioPackets(uchar *b, int Length, uchar Except)
{
  for (int i = 0; i < Length - 6; i++) {
      if (b[i] == 0x00 && b[i + 1] == 0x00 && b[i + 2] == 0x01) {
         uchar c = b[i + 3];
         int l = b[i + 4] * 256 + b[i + 5] + 6;
         switch (c) {
           case 0xBD: // dolby
                if (Except && dolbyDev) {
                   int written = b[i + 8] + 9; // skips the PES header
                   int n = l - written;
                   while (n > 0) {
                         int w = fwrite(&b[i + written], 1, n, dolbyDev);
                         if (w < 0) {
                            LOG_ERROR;
                            break;
                            }
                         n -= w;
                         written += w;
                         }
                   }
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

void cReplayBuffer::DisplayFrame(uchar *b, int Length)
{
  StripAudioPackets(b, Length);
  videoDisplayStillPicture sp = { (char *)b, Length };
  CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, false));
  CHECK(ioctl(audioDev, AUDIO_SET_MUTE, true));
  CHECK(ioctl(videoDev, VIDEO_STILLPICTURE, &sp));
}

void cReplayBuffer::Clear(bool Block)
{
  if (!(blockInput || blockOutput)) {
     blockInput = blockOutput = 2;
     time_t t0 = time(NULL);
     while ((blockInput > 1 || blockOutput > 1) && time(NULL) - t0 < 2)
           usleep(1);
     Lock();
     cRingBuffer::Clear();
     playIndex = -1;
     CHECK(ioctl(videoDev, VIDEO_CLEAR_BUFFER));
     CHECK(ioctl(audioDev, AUDIO_CLEAR_BUFFER));
     }
  if (!Block) {
     blockInput = blockOutput = 0;
     Unlock();
     }
}

void cReplayBuffer::Pause(void)
{
  paused = !paused;
  CHECK(ioctl(videoDev, paused ? VIDEO_FREEZE : VIDEO_CONTINUE));
  if (fastForward || fastRewind) {
     if (paused)
        Clear();
     fastForward = fastRewind = false;
     }
  CHECK(ioctl(audioDev, AUDIO_SET_MUTE, paused));
  stillIndex = -1;
}

void cReplayBuffer::Play(void)
{
  if (fastForward || fastRewind || paused) {
     if (!paused)
        Clear();
     stillIndex = -1;
     CHECK(ioctl(videoDev, paused ? VIDEO_CONTINUE : VIDEO_PLAY));
     CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, true));
     CHECK(ioctl(audioDev, AUDIO_SET_MUTE, false));
     fastForward = fastRewind = paused = false;
     }
}

void cReplayBuffer::Forward(void)
{
  if (index || paused) {
     if (!paused)
        Clear(true);
     stillIndex = -1;
     fastForward = !fastForward;
     fastRewind = false;
     if (paused)
        CHECK(ioctl(videoDev, fastForward ? VIDEO_SLOWMOTION : VIDEO_FREEZE, 2));
     CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, !fastForward));
     CHECK(ioctl(audioDev, AUDIO_SET_MUTE, fastForward || paused));
     if (!paused)
        Clear(false);
     }
}

void cReplayBuffer::Backward(void)
{
  if (index) {
     Clear(true);
     stillIndex = -1;
     fastRewind = !fastRewind;
     fastForward = false;
     CHECK(ioctl(audioDev, AUDIO_SET_AV_SYNC, !fastRewind));
     CHECK(ioctl(audioDev, AUDIO_SET_MUTE, fastRewind || paused));
     Clear(false);
     }
}

void cReplayBuffer::Close(void)
{
  if (replayFile >= 0) {
     fileName.Close();
     replayFile = -1;
     fileOffset = 0;
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
     int Index = index->Get(fileName.Number(), fileOffset);
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
     Clear(true);
     int Index = index->Get(fileName.Number(), fileOffset);
     if (Index >= 0) {
        if (Seconds < 0) {
           int sec = index->Last() / FRAMESPERSEC;
           if (Seconds < -sec)
              Seconds = -sec;
           }
        Index += Seconds * FRAMESPERSEC;
        if (Index < 0)
           Index = 1; // not '0', to allow GetNextIFrame() below to work!
        uchar FileNumber;
        int FileOffset;
        if (index->GetNextIFrame(Index, false, &FileNumber, &FileOffset) >= 0)
           NextFile(FileNumber, FileOffset);
        }
     Clear(false);
     Play();
     }
}

void cReplayBuffer::Goto(int Index, bool Still)
{
  if (index) {
     Clear(true);
     if (paused)
        CHECK(ioctl(videoDev, VIDEO_CONTINUE));
     if (++Index <= 0)
        Index = 1; // not '0', to allow GetNextIFrame() below to work!
     uchar FileNumber;
     int FileOffset, Length;
     Index = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset, &Length);
     if (Index >= 0 && NextFile(FileNumber, FileOffset) && Still) {
        stillIndex = Index;
        playIndex = -1;
        uchar b[MAXFRAMESIZE];
        int r = ReadFrame(replayFile, b, Length, sizeof(b));
        if (r > 0)
           DisplayFrame(b, r);
        fileOffset += Length;
        paused = true;
        }
     else
        stillIndex = playIndex = -1;
     Clear(false);
     }
}

void cReplayBuffer::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (index) {
     if (stillIndex >= 0)
        Current = stillIndex;
     else {
        Current = index->Get(fileName.Number(), fileOffset);
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
  if (FileNumber > 0) {
     fileOffset = FileOffset;
     replayFile = fileName.SetOffset(FileNumber, FileOffset);
     }
  else if (replayFile >= 0 && eof) {
     Close();
     replayFile = fileName.NextFile();
     }
  eof = false;
  return replayFile >= 0;
}

void cReplayBuffer::ToggleAudioTrack(void)
{
  if (CanToggleAudioTrack()) {
     audioTrack = (audioTrack == 0xC0) ? 0xC1 : 0xC0;
     Clear();
     }
}

// --- cTransferBuffer -------------------------------------------------------

class cTransferBuffer : public cRingBuffer {
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
:cRingBuffer(VIDEOBUFSIZE, true)
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
        cFile::FileReady(fromDevice, 100);
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
           if (errno != EAGAIN) {
              LOG_ERROR;
              if (errno != EBUFFEROVERFLOW)
                 break;
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
           while (r > 0 && Busy()) {
                 int w = write(toDevice, p, r);
                 if (w > 0) {
                    p += w;
                    r -= w;
                    }
                 else if (w < 0 && errno != EAGAIN) {
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
  };

cCuttingBuffer::cCuttingBuffer(const char *FromFileName, const char *ToFileName)
{
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

           // Read one frame:

           if (fromIndex->Get(Index++, &FileNumber, &FileOffset, &PictureType, &Length)) {
              if (FileNumber != CurrentFileNumber) {
                 fromFile = fromFileName->SetOffset(FileNumber, FileOffset);
                 CurrentFileNumber = FileNumber;
                 }
              if (fromFile >= 0) {
                 Length = ReadFrame(fromFile, buffer,  Length, sizeof(buffer));
                 if (Length < 0)
                    break;
                 }
              else
                 break;
              }
           else
              break;

           // Write one frame:

           if (PictureType == I_FRAME) { // every file shall start with an I_FRAME
              if (FileSize > MAXVIDEOFILESIZE) {
                 toFile = toFileName->NextFile();
                 if (toFile < 0)
                    break;
                 FileSize = 0;
                 }
              LastIFrame = 0;
              }
           write(toFile, buffer, Length);
           toIndex->Write(PictureType, toFileName->Number(), FileSize);
           FileSize += Length;
           if (!LastIFrame)
              LastIFrame = toIndex->Last();

           // Check editing marks:

           if (Mark && Index >= Mark->position) {
              Mark = fromMarks.Next(Mark);
              if (Mark) {
                 Index = Mark->position;
                 Mark = fromMarks.Next(Mark);
                 CurrentFileNumber = 0; // triggers SetOffset before reading next frame
                 toMarks.Add(LastIFrame);
                 toMarks.Add(toIndex->Last() + 1);
                 toMarks.Save();
                 }
              else
                 break; // final end mark reached
              }
           }
     }
  else
     esyslog(LOG_ERR, "no editing marks found!");
  dsyslog(LOG_INFO, "end video cutting thread");
}

// --- cVideoCutter ----------------------------------------------------------

cCuttingBuffer *cVideoCutter::cuttingBuffer = NULL;

bool cVideoCutter::Start(const char *FileName)
{
  if (!cuttingBuffer) {
     const char *EditedVersionName = PrefixVideoFileName(FileName, '%');
     if (EditedVersionName && RemoveVideoFile(EditedVersionName) && MakeDirs(EditedVersionName, true)) {
        cuttingBuffer = new cCuttingBuffer(FileName, EditedVersionName);
        return true;
        }
     }
  return false;
}

void cVideoCutter::Stop(void)
{
  delete cuttingBuffer;
  cuttingBuffer = NULL;
}

bool cVideoCutter::Active(void)
{
  if (cuttingBuffer) {
     if (cuttingBuffer->Active())
        return true;
     Stop();
     }
  return false;
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
  vPid = aPid1 = aPid2 = dPid1 = dPid2 = 0;
  siProcessor = NULL;
  recordBuffer = NULL;
  replayBuffer = NULL;
  transferBuffer = NULL;
  transferringFromDvbApi = NULL;
  ca = 0;
  priority = -1;

  // Devices that are only present on DVB-C or DVB-S cards:

  fd_qamfe   = OstOpen(DEV_OST_QAMFE,  n, O_RDWR);
  fd_qpskfe  = OstOpen(DEV_OST_QPSKFE, n, O_RDWR);
  fd_sec     = OstOpen(DEV_OST_SEC,    n, O_RDWR);

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

  // Devices that may not be available, and are not necessary for normal operation:

  videoDev   = OstOpen(DEV_VIDEO,      n, O_RDWR);

  // Devices that will be dynamically opened and closed when necessary:

  fd_dvr     = -1;

  // Video format:

  SetVideoFormat(Setup.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);

  // We only check the devices that must be present - the others will be checked before accessing them:

  if (((fd_qpskfe >= 0 && fd_sec >= 0) || fd_qamfe >= 0) && fd_demuxv >= 0 && fd_demuxa1 >= 0 && fd_demuxa2 >= 0 && fd_demuxd1 >= 0 && fd_demuxd2 >= 0 && fd_demuxt >= 0) {
     siProcessor = new cSIProcessor(OstName(DEV_OST_DEMUX, n));
     if (!dvbApi[0]) // only the first one shall set the system time
        siProcessor->SetUseTSTime(Setup.SetSystemTime);
     }
  else
     esyslog(LOG_ERR, "ERROR: can't open video device %d", n);
  cols = rows = 0;

  ovlGeoSet = ovlStat = ovlFbSet = false;
  ovlBrightness = ovlColour = ovlHue = ovlContrast = 32768;
  ovlClipCount = 0;

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
}

cDvbApi::~cDvbApi()
{
  delete siProcessor;
  Close();
  StopReplay();
  StopRecord();
  StopTransfer();
  OvlO(false); //Overlay off!
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

cDvbApi *cDvbApi::GetDvbApi(int Ca, int Priority)
{
  cDvbApi *d = NULL, *dMinPriority = NULL;
  int index = Ca - 1;
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (dvbApi[i]) {
         if (i == index) { // means we need exactly _this_ device
            d = dvbApi[i];
            break;
            }
         else if (Ca == 0) { // means any device would be acceptable
            if (!d || !dvbApi[i]->Recording() || (d->Recording() && d->Priority() > dvbApi[i]->Priority()))
               d = dvbApi[i]; // this is one that is either not currently recording or has the lowest priority
            if (d && d != PrimaryDvbApi && !d->Recording()) // avoids the PrimaryDvbApi if possible
               break;
            if (d && d->Recording() && d->Priority() < Setup.PrimaryLimit && (!dMinPriority || d->Priority() < dMinPriority->Priority()))
               dMinPriority = d; // this is the one with the lowest priority below Setup.PrimaryLimit
            }
         }
      }
  if (d == PrimaryDvbApi) { // the PrimaryDvbApi was the only one that was free
     if (Priority < Setup.PrimaryLimit)
        return NULL;        // not enough priority to use the PrimaryDvbApi
     if (dMinPriority)      // there's one that must not use the PrimaryDvbApi...
        d = dMinPriority;   // ...so let's kick out that one
     }
  return (d                           // we found one...
      && (!d->Recording()             // ...that's either not currently recording...
          || d->Priority() < Priority // ...or has a lower priority...
          || (!d->Ca() && Ca)))       // ...or doesn't need this card
          ? d : NULL;
}

int cDvbApi::Index(void)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (dvbApi[i] == this)
         return i;
      }
  return -1;
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
         if (Probe(OstName(DEV_OST_QPSKFE, i)) || Probe(OstName(DEV_OST_QAMFE, i)))
            dvbApi[NumDvbApis++] = new cDvbApi(i);
         else
            break;
         }
      }
  PrimaryDvbApi = dvbApi[0];
  if (NumDvbApis > 0) {
     isyslog(LOG_INFO, "found %d video device%s", NumDvbApis, NumDvbApis > 1 ? "s" : "");
     } // need braces because of isyslog-macro
  else {
     esyslog(LOG_ERR, "ERROR: no video device found, giving up!");
     }
  return NumDvbApis > 0;
}

void cDvbApi::Cleanup(void)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      delete dvbApi[i];
      dvbApi[i] = NULL;
      }
  PrimaryDvbApi = NULL;
}

const cSchedules *cDvbApi::Schedules(cThreadLock *ThreadLock) const
{
  if (siProcessor && ThreadLock->Lock(siProcessor))
     return siProcessor->Schedules();
  return NULL;
}

bool cDvbApi::GrabImage(const char *FileName, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  if (videoDev < 0)
     return false;
  int result = 0;
  // just do this once?
  struct video_mbuf mbuf;
  result |= ioctl(videoDev, VIDIOCGMBUF, &mbuf);
  int msize = mbuf.size;
  // gf: this needs to be a protected member of cDvbApi! //XXX kls: WHY???
  unsigned char *mem = (unsigned char *)mmap(0, msize, PROT_READ | PROT_WRITE, MAP_SHARED, videoDev, 0);
  if (!mem || mem == (unsigned char *)-1)
     return false;
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
  // this needs to be done every time:
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

  if (ovlStat && ovlGeoSet) {
     // switch the Overlay on again (gf: why have i to do anything again?)
     OvlG(ovlSizeX, ovlSizeY, ovlPosX, ovlPosY);
     }
  if (ovlFbSet)
     OvlP(ovlBrightness, ovlColour, ovlHue, ovlContrast);

  munmap(mem, msize);
  return result == 0;
}

bool cDvbApi::OvlF(int SizeX, int SizeY, int FbAddr, int Bpp, int Palette)
{
  if (videoDev < 0)
     return false;
  int result = 0;
  // get the actual X-Server settings???
  // plausibility-check problem: can't be verified w/o X-server!!!
  if (SizeX <= 0 || SizeY <= 0 || FbAddr == 0 || Bpp / 8 > 4 ||
      Bpp / 8 <= 0 || Palette <= 0 || Palette > 13 || ovlClipCount < 0 ||
      SizeX > 4096 || SizeY > 4096) {
     ovlFbSet = ovlGeoSet = false;
     OvlO(false);
     return false;
     }
  else {
    dsyslog(LOG_INFO, "OvlF: %d %d %x %d %d", SizeX, SizeY, FbAddr, Bpp, Palette);
    // this is the problematic part!
    struct video_buffer vb;
    result |= ioctl(videoDev, VIDIOCGFBUF, &vb);
    vb.base = (void*)FbAddr;
    vb.depth = Bpp;
    vb.height = SizeY;
    vb.width = SizeX;
    vb.bytesperline = ((vb.depth + 1) / 8) * vb.width;
    //now the real thing: setting the framebuffer
    result |= ioctl(videoDev, VIDIOCSFBUF, &vb);
    if (result) {
       ovlFbSet = ovlGeoSet = false;
       ovlClipCount = 0;
       OvlO(false);
       return false;
       }
    else {
       ovlFbSizeX = SizeX;
       ovlFbSizeY = SizeY;
       ovlBpp = Bpp;
       ovlPalette = Palette;
       ovlFbSet = true;
       return true;
      }
    }
}

bool cDvbApi::OvlG(int SizeX, int SizeY, int PosX, int PosY)
{
  if (videoDev < 0)
     return false;
  int result = 0;
  // get the actual X-Server settings???
  struct video_capability vc;
  result |= ioctl(videoDev, VIDIOCGCAP, &vc);
  if (!ovlFbSet)
     return false;
  if (SizeX < vc.minwidth || SizeY < vc.minheight ||
      SizeX > vc.maxwidth || SizeY>vc.maxheight
//      || PosX > FbSizeX || PosY > FbSizeY
//         PosX < -SizeX || PosY < -SizeY ||
     ) {
     ovlGeoSet = false;
     OvlO(false);
     return false;
     }
  else {
     struct video_window vw;
     result |= ioctl(videoDev, VIDIOCGWIN,  &vw);
     vw.x = PosX;
     vw.y = PosY;
     vw.width = SizeX;
     vw.height = SizeY;
     vw.chromakey = ovlPalette;
     vw.flags = VIDEO_WINDOW_CHROMAKEY; // VIDEO_WINDOW_INTERLACE; //VIDEO_CLIP_BITMAP;
     vw.clips = ovlClipRects;
     vw.clipcount = ovlClipCount;
     result |= ioctl(videoDev, VIDIOCSWIN, &vw);
     if (result) {
        ovlGeoSet = false;
        ovlClipCount = 0;
        return false;
        }
     else {
        ovlSizeX = SizeX;
        ovlSizeY = SizeY;
        ovlPosX = PosX;
        ovlPosY = PosY;
        ovlGeoSet = true;
        ovlStat = true;
        return true;
        }
     }
}

bool cDvbApi::OvlC(int ClipCount, CRect *cr)
{
  if (videoDev < 0)
     return false;
  if (ovlGeoSet && ovlFbSet) {
     for (int i = 0; i < ClipCount; i++) {
         ovlClipRects[i].x = cr[i].x;
         ovlClipRects[i].y = cr[i].y;
         ovlClipRects[i].width = cr[i].width;
         ovlClipRects[i].height = cr[i].height;
         ovlClipRects[i].next = &(ovlClipRects[i + 1]);
         }
     ovlClipCount = ClipCount;
     //use it:
     return OvlG(ovlSizeX, ovlSizeY, ovlPosX, ovlPosY);
     }
  return false;
}

bool cDvbApi::OvlP(__u16 Brightness, __u16 Colour, __u16 Hue, __u16 Contrast)
{
  if (videoDev < 0)
     return false;
  int result = 0;
  ovlBrightness = Brightness;
  ovlColour = Colour;
  ovlHue = Hue;
  ovlContrast = Contrast;
  struct video_picture vp;
  if (!ovlFbSet)
     return false;
  result |= ioctl(videoDev, VIDIOCGPICT, &vp);
  vp.brightness = Brightness;
  vp.colour = Colour;
  vp.hue = Hue;
  vp.contrast = Contrast;
  vp.depth = ovlBpp;
  vp.palette = ovlPalette; // gf: is this always ok? VIDEO_PALETTE_RGB565;
  result |= ioctl(videoDev, VIDIOCSPICT, &vp);
  return result == 0;
}

bool cDvbApi::OvlO(bool Value)
{
  if (videoDev < 0)
     return false;
  int result = 0;
  if (!ovlGeoSet && Value)
     return false;
  int one = 1;
  int zero = 0;
  result |= ioctl(videoDev, VIDIOCCAPTURE, Value ? &one : &zero);
  ovlStat = Value;
  if (result) {
     ovlStat = false;
     return false;
     }
  return true;
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
  window = subwin(stdscr, h, w, d, 0);
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
  int x = (720 - (Setup.OSDwidth - 1) * charWidth) / 2; //TODO PAL vs. NTSC???
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
     osd->Create(0,                   lineHeight, w, (Setup.OSDheight - 3) * lineHeight, 2, true, clrBackground, clrCyan, clrWhite, clrBlack);
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
#ifndef DEBUG_OSD
  if (osd)
     osd->Flush();
#endif
}

int cDvbApi::SetModeRecord(void)
{
  // Sets up the DVB device for recording

  SetPids(true);
  if (fd_dvr >= 0)
     close(fd_dvr);
  fd_dvr = OstOpen(DEV_OST_DVR, Index(), O_RDONLY | O_NONBLOCK);
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
  if (fd_video)
     CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, Format));
}

bool cDvbApi::SetPid(int fd, dmxPesType_t PesType, int Pid, dmxOutput_t Output)
{
  if (Pid) {
     CHECK(ioctl(fd, DMX_STOP));
     dmxPesFilterParams pesFilterParams;
     pesFilterParams.pid     = Pid;
     pesFilterParams.input   = DMX_IN_FRONTEND;
     pesFilterParams.output  = Output;
     pesFilterParams.pesType = PesType;
     pesFilterParams.flags   = DMX_IMMEDIATE_START;
     if (ioctl(fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
        if (Pid != 0x1FFF)
           LOG_ERROR;
        return false;
        }
     }
  return true;
}

bool cDvbApi::SetPids(bool ForRecording)
{
  return SetVpid(vPid,   ForRecording ? DMX_OUT_TS_TAP : DMX_OUT_DECODER) &&
         SetApid1(aPid1, ForRecording ? DMX_OUT_TS_TAP : DMX_OUT_DECODER) &&
         SetApid2(ForRecording ? aPid2 : 0, DMX_OUT_TS_TAP) &&
         SetDpid1(ForRecording ? dPid1 : 0, DMX_OUT_TS_TAP) &&
         SetDpid2(ForRecording ? dPid2 : 0, DMX_OUT_TS_TAP);
}

bool cDvbApi::SetChannel(int ChannelNumber, int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid1, int Apid2, int Dpid1, int Dpid2, int Tpid, int Ca, int Pnr)
{
  // Make sure the siProcessor won't access the device while switching
  cThreadLock ThreadLock(siProcessor);

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

  // If this card can't receive this channel, we must not actually switch
  // the channel here, because that would irritate the driver when we
  // start replaying in Transfer Mode immediately after switching the channel:
  bool NeedsTransferMode = (this == PrimaryDvbApi && Ca && Ca != Index() + 1);

  if (!NeedsTransferMode) {

     // Turn off current PIDs:

     SetVpid( 0x1FFF, DMX_OUT_DECODER);
     SetApid1(0x1FFF, DMX_OUT_DECODER);
     SetApid2(0x1FFF, DMX_OUT_DECODER);
     SetDpid1(0x1FFF, DMX_OUT_DECODER);
     SetDpid2(0x1FFF, DMX_OUT_DECODER);
     SetTpid( 0x1FFF, DMX_OUT_DECODER);

     bool ChannelSynced = false;

     if (fd_qpskfe >= 0 && fd_sec >= 0) { // DVB-S

        // Frequency offsets:

        unsigned int freq = FrequencyMHz;
        int tone = SEC_TONE_OFF;

        if (freq < (unsigned int)Setup.LnbSLOF) {
           freq -= Setup.LnbFrequLo;
           tone = SEC_TONE_OFF;
           }
        else {
           freq -= Setup.LnbFrequHi;
           tone = SEC_TONE_ON;
           }

        qpskParameters qpsk;
        qpsk.iFrequency = freq * 1000UL;
        qpsk.SymbolRate = Srate * 1000UL;
        qpsk.FEC_inner = FEC_AUTO;

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

        // Tuning:

        CHECK(ioctl(fd_qpskfe, QPSK_TUNE, &qpsk));

        // Wait for channel sync:

        if (cFile::FileReady(fd_qpskfe, 5000)) {
           qpskEvent event;
           int res = ioctl(fd_qpskfe, QPSK_GET_EVENT, &event);
           if (res >= 0)
              ChannelSynced = event.type == FE_COMPLETION_EV;
           else
              esyslog(LOG_ERR, "ERROR %d in qpsk get event", res);
           }
        else
           esyslog(LOG_ERR, "ERROR: timeout while tuning\n");
        }
     else if (fd_qamfe >= 0) { // DVB-C

        // Frequency and symbol rate:

        qamParameters qam;
        qam.Frequency = FrequencyMHz * 1000000UL;
        qam.SymbolRate = Srate * 1000UL;
        qam.FEC_inner = FEC_AUTO;
        qam.QAM = QAM_64;

        // Tuning:

        CHECK(ioctl(fd_qamfe, QAM_TUNE, &qam));

        // Wait for channel sync:

        if (cFile::FileReady(fd_qamfe, 5000)) {
           qamEvent event;
           int res = ioctl(fd_qamfe, QAM_GET_EVENT, &event);
           if (res >= 0)
              ChannelSynced = event.type == FE_COMPLETION_EV;
           else
              esyslog(LOG_ERR, "ERROR %d in qam get event", res);
           }
        else
           esyslog(LOG_ERR, "ERROR: timeout while tuning\n");
        }
     else {
        esyslog(LOG_ERR, "ERROR: attempt to set channel without DVB-S or DVB-C device");
        return false;
        }

     if (!ChannelSynced) {
        esyslog(LOG_ERR, "ERROR: channel %d not sync'ed!", ChannelNumber);
        if (this == PrimaryDvbApi)
           cThread::RaisePanic();
        return false;
        }

     // PID settings:

     if (!SetPids(false)) {
        esyslog(LOG_ERR, "ERROR: failed to set PIDs for channel %d", ChannelNumber);
        return false;
        }
     SetTpid(Tpid, DMX_OUT_DECODER);
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     }

  if (this == PrimaryDvbApi && siProcessor)
     siProcessor->SetCurrentServiceID(Pnr);

  // If this DVB card can't receive this channel, let's see if we can
  // use the card that actually can receive it and transfer data from there:

  if (NeedsTransferMode) {
     cDvbApi *CaDvbApi = GetDvbApi(Ca, 0);
     if (CaDvbApi) {
        if (!CaDvbApi->Recording()) {
           if (CaDvbApi->SetChannel(ChannelNumber, FrequencyMHz, Polarization, Diseqc, Srate, Vpid, Apid1, Apid2, Dpid1, Dpid2, Tpid, Ca, Pnr)) {
              SetModeReplay();
              transferringFromDvbApi = CaDvbApi->StartTransfer(fd_video);
              }
           }
        }
     }

  if (fd_video >= 0 && fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
     }

  return true;
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
     ca = 0;
     priority = -1;
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
        for (int i = 0; i < cDvbApi::NumDvbApis; i++) {
            cDvbApi *DvbApi = cDvbApi::GetDvbApi(i + 1, MAXPRIORITY);
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
                           if (Channel && Channel->pnr && !TransponderScanned(Channel)) {
                              if (DvbApi == cDvbApi::PrimaryDvbApi && !currentChannel)
                                 currentChannel = DvbApi->Channel();
                              Channel->Switch(DvbApi, false);
                              lastChannel = ch;
                              break;
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

