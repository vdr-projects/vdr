/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.64 2001/03/18 16:47:16 kls Exp $
 */

#include "dvbapi.h"
#include <errno.h>
#include <fcntl.h>
extern "C" {
#include <jpeglib.h>
}
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "config.h"
#include "interface.h"
#include "recording.h"
#include "remux.h"
#include "ringbuffer.h"
#include "tools.h"
#include "videodir.h"

#define VIDEODEVICE "/dev/video"
#define VBIDEVICE   "/dev/vbi"

// The size of the array used to buffer video data:
// (must be larger than MINVIDEODATA - see remux.h)
#define VIDEOBUFSIZE (1024*1024)

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

typedef unsigned char uchar;

static void SetPlayMode(int VideoDev, int Mode)
{
  if (VideoDev >= 0) {
     struct video_play_mode pmode;
     pmode.mode = Mode;
     ioctl(VideoDev, VIDIOCSPLAYMODE, &pmode);
     }
}

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
        if (Record) {
           if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP)) >= 0) {
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

// --- cRingBuffer_ -----------------------------------------------------------

/* cRingBuffer reads data from an input file, stores it in a buffer and writes
   it to an output file upon request. The Read() and Write() functions should
   be called only when the associated file is ready to provide or receive data
   (use the 'select()' function to determine that), and the files should be
   opened in non-blocking mode.
   The '...Limit' parameters define safety limits. If they are exceeded a log entry
   will be made.
*/

class cRingBuffer_ {
private:
  uchar *buffer;
  int size, head, tail, freeLimit, availLimit;
  int countLimit, countOverflow;
  int minFree;
  bool eof;
  int *inFile, *outFile;
protected:
  int Free(void) { return ((tail >= head) ? size + head - tail : head - tail) - 1; }
public:
  int Available(void) { return (tail >= head) ? tail - head : size - head + tail; }
protected:
  int Readable(void) { return (tail >= head) ? size - tail - (head ? 0 : 1) : head - tail - 1; } // keep a 1 byte gap!
  int Writeable(void) { return (tail >= head) ? tail - head : size - head; }
  int Byte(int Offset);
  bool Set(int Offset, int Length, int Value);
protected:
  int GetStartCode(int Offset) { return (Byte(Offset) == 0x00 && Byte(Offset + 1) == 0x00 && Byte(Offset + 2) == 0x01) ? Byte(Offset + 3) : -1; }
  int GetPictureType(int Offset) { return (Byte(Offset + 5) >> 3) & 0x07; }
  int FindStartCode(uchar Code, int Offset = 0);
  int GetPacketLength(int Offset = 0);
public:
  cRingBuffer_(int *InFile, int *OutFile, int Size, int FreeLimit = 0, int AvailLimit = 0);
  virtual ~cRingBuffer_();
  virtual int Read(int Max = -1);
  virtual int Write(int Max = -1);
  bool EndOfFile(void) { return eof; }
  bool Empty(void) { return Available() == 0; }
  void Clear(void) { head = tail = 0; }
  void Skip(int n);
  };

cRingBuffer_::cRingBuffer_(int *InFile, int *OutFile, int Size, int FreeLimit, int AvailLimit)
{
  inFile = InFile;
  outFile = OutFile;
  size = Size;
  Clear();
  freeLimit = FreeLimit;
  availLimit = AvailLimit;
  eof = false;
  countLimit = countOverflow = 0;
  minFree = size - 1;
  buffer = new uchar[size];
  if (!buffer)
     esyslog(LOG_ERR, "ERROR: can't allocate ring buffer (size=%d)", size);
}

cRingBuffer_::~cRingBuffer_()
{
  dsyslog(LOG_INFO, "buffer stats: %d free, %d overflows, limit exceeded %d times", minFree, countOverflow, countLimit);
  delete buffer;
}

int cRingBuffer_::Byte(int Offset)
{
  if (buffer && Offset < Available()) {
     Offset += head;
     if (Offset >= size)
        Offset -= size;
     return buffer[Offset];
     }
  return -1;
}

bool cRingBuffer_::Set(int Offset, int Length, int Value)
{
  if (buffer && Offset + Length <= Available() ) {
     Offset += head;
     while (Length--) {
           if (Offset >= size)
              Offset -= size;
           buffer[Offset] = Value;
           Offset++;
           }
     return true;
     }
  return false;
}

void cRingBuffer_::Skip(int n)
{
  if (n > 0) {
     if (head < tail) {
        head += n;
        if (head > tail)
           head = tail;
        }
     else if (head > tail) {
        head += n;
        if (head >= size)
           head -= size;
        if (head > tail)
           head = tail;
        }
     }
}

int cRingBuffer_::Read(int Max)
{
  if (buffer) {
     eof = false;
     int free = Free();
     if (free < minFree)
        minFree = free;
     if (freeLimit) {
        if (free == 0) {
           esyslog(LOG_ERR, "ERROR: buffer overflow (size=%d)", size);
           countOverflow++;
           }
        else if (free < freeLimit) {
           dsyslog(LOG_INFO, "free buffer space dipped into limit (%d < %d)", free, freeLimit);
           countLimit++;
           }
        }
     if (free == 0)
        return 0; // the buffer is full
     int readin = 0;
     for (int i = 0; i < 2; i++) {
         // If we read in exactly as many bytes as are immediately
         // "readable" we have to do it again, because that means we
         // were at the very end of the physical buffer and possibly only
         // read in very few bytes.
         int immediate = Readable();
         int n = immediate;
         if (Max > 0 && n > Max)
            n = Max;
         if (n > 0) {
            int r = read(*inFile, buffer + tail, n);
            if (r > 0) {
               readin += r;
               tail += r;
               if (tail > size)
                  esyslog(LOG_ERR, "ERROR: ooops: buffer tail (%d) exceeds size (%d)", tail, size);
               if (tail >= size)
                  tail = 0;
               }
            else if (r < 0) {
               if (errno != EAGAIN) {
                  LOG_ERROR;
                  return -1;
                  }
               }
            else
               eof = true;
            if (r == immediate && Max != immediate && tail == 0)
               Max -= immediate;
            else
               break;
            }
         }
     return readin;
     }
  return -1;
}

int cRingBuffer_::Write(int Max)
{
  if (buffer) {
     int avail = Available();
     if (availLimit) {
        //XXX stats???
        if (avail == 0)
           //XXX esyslog(LOG_ERR, "ERROR: buffer empty!");
        {//XXX
           esyslog(LOG_ERR, "ERROR: buffer empty! %d", Max);
           return Max > 0 ? Max : 0;
           }//XXX
        else if (avail < availLimit)
;//XXX           dsyslog(LOG_INFO, "available buffer data dipped into limit (%d < %d)", avail, availLimit);
        }
     if (avail == 0)
        return 0; // the buffer is empty
     int n = Writeable();
     if (Max > 0 && n > Max)
        n = Max;
     int w = write(*outFile, buffer + head, n);
     if (w > 0) {
        head += w;
        if (head > size)
           esyslog(LOG_ERR, "ERROR: ooops: buffer head (%d) exceeds size (%d)", head, size);
        if (head >= size)
           head = 0;
        }
     else if (w < 0) {
        if (errno != EAGAIN)
           LOG_ERROR;
        else
           w = 0;
        }
     return w;
     }
  return -1;
}

int cRingBuffer_::FindStartCode(uchar Code, int Offset)
{
  // Searches for a start code (beginning at Offset) and returns the number
  // of bytes from Offset to the start code.

  int n = Available() - Offset;

  for (int i = 0; i < n; i++) {
      int c = GetStartCode(Offset + i);
      if (c == Code) 
         return i;
      if (i > 0 && c == SC_PHEAD)
         break; // found another block start while looking for a different code
      }
  return -1;
}

int cRingBuffer_::GetPacketLength(int Offset)
{
  // Returns the entire length of the packet starting at offset.
  return (Byte(Offset + 4) << 8) + Byte(Offset + 5) + 6;
}

// --- cFileName -------------------------------------------------------------

class cFileName {
private:
  int file;
  int fileNumber;
  char *fileName, *pFileNumber;
  bool record;
public:
  cFileName(const char *FileName, bool Record);
  ~cFileName();
  const char *Name(void) { return fileName; }
  int Number(void) { return fileNumber; }
  int Open(void);
  void Close(void);
  int SetOffset(int Number, int Offset = 0);
  int NextFile(void);
  };

cFileName::cFileName(const char *FileName, bool Record)
{
  file = -1;
  fileNumber = 0;
  record = Record;
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
     if (record) {
        dsyslog(LOG_INFO, "recording to '%s'", fileName);
        file = OpenVideoFile(fileName, O_RDWR | O_CREAT | O_NONBLOCK);
        if (file < 0)
           LOG_ERROR_STR(fileName);
        }
     else {
        if (access(fileName, R_OK) == 0) {
           dsyslog(LOG_INFO, "playing '%s'", fileName);
           file = open(fileName, O_RDONLY | O_NONBLOCK);
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
  cRecordBuffer(int *InFile, const char *FileName);
  virtual ~cRecordBuffer();
  };

cRecordBuffer::cRecordBuffer(int *InFile, const char *FileName)
:cRingBuffer(VIDEOBUFSIZE)
,fileName(FileName, true)
{
  index = NULL;
  pictureType = NO_PICTURE;
  fileSize = 0;
  videoDev = *InFile;
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
  Start();
}

cRecordBuffer::~cRecordBuffer()
{
  Stop();
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
            break;
            }
         }
      else if (time(NULL) - t > 5) {
         esyslog(LOG_ERR, "ERROR: video data stream broken");
         t = time(NULL);
         }
      cFile::FileReady(videoDev, 100);
      if (!recording)
         break;
      }
  SetPlayMode(videoDev, VID_PLAY_RESET);

  dsyslog(LOG_INFO, "input thread ended (pid=%d)", getpid());
}

void cRecordBuffer::Output(void)
{
  dsyslog(LOG_INFO, "output thread started (pid=%d)", getpid());

  uchar b[MINVIDEODATA * 2];
  int r = 0;
  for (;;) {
      usleep(1); // this keeps the CPU load low
      r += Get(b + r, sizeof(b) - r);
      if (r > 0) {
         //XXX buffer full???
         int Count = r, Result;
         const uchar *p = remux.Process(b, Count, Result, pictureType);
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
      }
  recording = false;

  dsyslog(LOG_INFO, "output thread ended (pid=%d)", getpid());
}

// --- cReplayBuffer ---------------------------------------------------------

class cReplayBuffer : public cRingBuffer_, public cThread {
private:
  enum eReplayCmd { rcNone, rcStill, rcPause, rcPlay, rcForward, rcBackward };
  enum eReplayMode { rmStill, rmPlay, rmFastForward, rmFastRewind, rmSlowRewind };
  cIndexFile *index;
  cFileName fileName;
  int fileOffset;
  int videoDev;
  int replayFile;
  eReplayMode mode;
  int lastIndex, stillIndex;
  int brakeCounter;
  eReplayCmd command;
  bool active;
  bool NextFile(uchar FileNumber = 0, int FileOffset = -1);
  void Close(void);
  void SetCmd(eReplayCmd Cmd) { LOCK_THREAD; command = Cmd; }
  void SetTemporalReference(void);
protected:
  virtual void Action(void);
public:
  cReplayBuffer(int *OutFile, const char *FileName);
  virtual ~cReplayBuffer();
  virtual int Read(int Max = -1);
  virtual int Write(int Max = -1);
  void SetMode(eReplayMode Mode);
  int Resume(void);
  bool Save(void);
  void Pause(void)    { SetCmd(rcPause); }
  void Play(void)     { SetCmd(rcPlay); }
  void Forward(void)  { SetCmd(rcForward); }
  void Backward(void) { SetCmd(rcBackward); }
  int SkipFrames(int Frames);
  void SkipSeconds(int Seconds);
  void Goto(int Position, bool Still = false);
  void GetIndex(int &Current, int &Total, bool SnapToIFrame = false);
  };

cReplayBuffer::cReplayBuffer(int *OutFile, const char *FileName)
:cRingBuffer_(&replayFile, OutFile, VIDEOBUFSIZE, 0, VIDEOBUFSIZE / 10)
,fileName(FileName, false)
{
  index = NULL;
  fileOffset = 0;
  videoDev = *OutFile;
  replayFile = fileName.Open();
  mode = rmPlay;
  brakeCounter = 0;
  command = rcNone;
  lastIndex = stillIndex = -1;
  active = false;
  if (!fileName.Name())
     return;
  // Create the index file:
  index = new cIndexFile(FileName, false);
  if (!index)
     esyslog(LOG_ERR, "ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
  Start();
}

cReplayBuffer::~cReplayBuffer()
{
  active = false;
  Cancel(3);
  Close();
  SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
  SetPlayMode(videoDev, VID_PLAY_RESET);
  delete index;
}

void cReplayBuffer::Action(void)
{
  dsyslog(LOG_INFO, "replay thread started (pid=%d)", getpid());

  bool Paused = false;
  bool FastForward = false;
  bool FastRewind = false;

  int ResumeIndex = Resume();
  if (ResumeIndex >= 0)
     isyslog(LOG_INFO, "resuming replay at index %d (%s)", ResumeIndex, IndexToHMSF(ResumeIndex, true));
  active = true;
  while (active) {
        usleep(1); // this keeps the CPU load low

        LOCK_THREAD;

        if (command != rcNone) {
           switch (command) {
             case rcStill:    SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
                              SetPlayMode(videoDev, VID_PLAY_NORMAL);
                              SetMode(rmStill);
                              Paused = FastForward = FastRewind = false;
                              break;
             case rcPause:    SetPlayMode(videoDev, Paused ? VID_PLAY_NORMAL : VID_PLAY_PAUSE);
                              Paused = !Paused;
                              if (FastForward || FastRewind) {
                                 SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
                                 Clear();
                                 }
                              FastForward = FastRewind = false;
                              SetMode(rmPlay);
                              if (!Paused)
                                 stillIndex = -1;
                              break;
             case rcPlay:     if (FastForward || FastRewind || Paused) {
                                 SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
                                 SetPlayMode(videoDev, VID_PLAY_NORMAL);
                                 FastForward = FastRewind = Paused = false;
                                 SetMode(rmPlay);
                                 }
                              stillIndex = -1;
                              break;
             case rcForward:  SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
                              Clear();
                              FastForward = !FastForward;
                              FastRewind = false;
                              if (Paused) {
                                 SetMode(rmPlay);
                                 SetPlayMode(videoDev, FastForward ? VID_PLAY_SLOW_MOTION : VID_PLAY_PAUSE);
                                 }
                              else {
                                 SetPlayMode(videoDev, VID_PLAY_NORMAL);
                                 SetMode(FastForward ? rmFastForward : rmPlay);
                                 }
                              stillIndex = -1;
                              break;
             case rcBackward: SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
                              Clear();
                              FastRewind = !FastRewind;
                              FastForward = false;
                              if (Paused) {
                                 SetMode(FastRewind ? rmSlowRewind : rmPlay);
                                 SetPlayMode(videoDev, FastRewind ? VID_PLAY_NORMAL : VID_PLAY_PAUSE);
                                 }
                              else {
                                 SetPlayMode(videoDev, VID_PLAY_NORMAL);
                                 SetMode(FastRewind ? rmFastRewind : rmPlay);
                                 }
                              stillIndex = -1;
                              break;
             default:         break;
             }
           command = rcNone;
           }
        if (Read() < 0 || Write() < 0)
           break;
        }
  Save();

  dsyslog(LOG_INFO, "end replaying thread");
}

void cReplayBuffer::Close(void)
{
  if (replayFile >= 0) {
     fileName.Close();
     replayFile = -1;
     fileOffset = 0;
     }
}

void cReplayBuffer::SetMode(eReplayMode Mode)
{
  mode = Mode;
  brakeCounter = 0;
  if (mode != rmPlay)
     Clear();
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

     LOCK_THREAD;

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
  LOCK_THREAD;

  SetPlayMode(videoDev, VID_PLAY_PAUSE);
  SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
  SetPlayMode(videoDev, VID_PLAY_NORMAL);
  command = rcPlay;
  SetMode(rmPlay);
  Clear();

  if (index && Seconds) {
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
     }
}

void cReplayBuffer::Goto(int Index, bool Still)
{
  LOCK_THREAD;

  if (Still)
     command = rcStill;
  if (++Index <= 0)
     Index = 1; // not '0', to allow GetNextIFrame() below to work!
  uchar FileNumber;
  int FileOffset;
  if ((stillIndex = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset)) >= 0)
     NextFile(FileNumber, FileOffset);
  SetPlayMode(videoDev, VID_PLAY_CLEAR_BUFFER);
  Clear();
}

void cReplayBuffer::GetIndex(int &Current, int &Total, bool SnapToIFrame)
{
  if (index) {

     LOCK_THREAD;

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
     Clear();
     fileOffset = FileOffset;
     replayFile = fileName.SetOffset(FileNumber, FileOffset);
     }
  else if (replayFile >= 0 && EndOfFile()) {
     Close();
     replayFile = fileName.NextFile();
     }
  return replayFile >= 0;
}

void cReplayBuffer::SetTemporalReference(void)
{
  for (int i = 0; i < Available(); i++) {
      if (Byte(i) == 0 && Byte(i + 1) == 0 && Byte(i + 2) == 1) {
         switch (Byte(i + 3)) {
           case SC_PICTURE: {
                              unsigned short m = (Byte(i + 4) << 8) | Byte(i + 5);
                              m &= 0x003F;
                              Set(i + 4, 1, m >> 8);
                              Set(i + 5, 1, m & 0xFF);
                            }
                            return;
           }
         }
      }
}

int cReplayBuffer::Read(int Max = -1)
{
  if (mode != rmPlay) {
     if (index) {
        if (Available())
           return 0; // write out the entire block
        if (mode == rmStill) {
           uchar FileNumber;
           int FileOffset, Length;
           if (index->GetNextIFrame(stillIndex + 1, false, &FileNumber, &FileOffset, &Length) >= 0) {
              if (!NextFile(FileNumber, FileOffset))
                 return -1;
              Max = Length;
              }
           command = rcPause;
           }
        else {
           int Index = (lastIndex >= 0) ? lastIndex : index->Get(fileName.Number(), fileOffset);
           if (Index >= 0) {
              if (mode == rmSlowRewind && (brakeCounter++ % 24) != 0) {
                 // show every I_FRAME 24 times in rmSlowRewind mode to achieve roughly the same speed as in slow forward mode
                 Index = index->GetNextIFrame(Index, true); // jump ahead one frame
                 }
              uchar FileNumber;
              int FileOffset, Length;
              Index = index->GetNextIFrame(Index, mode == rmFastForward, &FileNumber, &FileOffset, &Length);
              if (Index >= 0) {
                 if (!NextFile(FileNumber, FileOffset))
                    return -1;
                 Max = Length;
                 }
              lastIndex = Index;
              }
           if (Index < 0) {
              // This results in normal replay after a fast rewind.
              // After a fast forward it will stop.
              // TODO Could we cause it to pause at the last frame?
              SetMode(rmPlay);
              return 0;
              }
           }
        }
     }
  else
     lastIndex = -1;
  //XXX timeout as in recording???
  if (NextFile()) {
     int readin = 0;
     do {
        // If Max is > 0 here we need to make sure we read in the entire block!
        int r = cRingBuffer_::Read(Max);
        if (r >= 0)
           readin += r;
        else
           return -1;
        } while (readin < Max && Free() > 0);
     if (mode != rmPlay) {
        // delete the audio data in modes other than rmPlay:
        int AudioOffset, StartOffset = 0;
        while ((AudioOffset = FindStartCode(SC_AUDIO, StartOffset)) >= 0) {
              if (!Set(StartOffset + AudioOffset, GetPacketLength(StartOffset + AudioOffset), 0))
                 break; // to be able to replay old AV_PES recordings!
              StartOffset += AudioOffset;
              }
        SetTemporalReference();
        }
     return readin;
     }
  if (Available() > 0)
     return 0;
  return -1;
}

int cReplayBuffer::Write(int Max)
{
  int Written = 0;
  if (Max) {
     int w;
     do {
        w = cRingBuffer_::Write(Max);
        if (w >= 0) {
           fileOffset += w;
           Written += w;
           if (Max < 0)
              break;
           Max -= w;
           }
        else
           return w;
        } while (Max > 0); // we MUST write this entire frame block
     }
  return Written;
}

// --- cTransferBuffer -------------------------------------------------------

class cTransferBuffer : public cThread {
private:
  bool active;
  int fromDevice, toDevice;
protected:
  virtual void Action(void);
public:
  cTransferBuffer(int FromDevice, int ToDevice);
  virtual ~cTransferBuffer();
  };

cTransferBuffer::cTransferBuffer(int FromDevice, int ToDevice)
{
  fromDevice = FromDevice;
  toDevice = ToDevice;
  active = false;
  Start();
}

cTransferBuffer::~cTransferBuffer()
{
  active = false;
  Cancel(3);
  SetPlayMode(fromDevice, VID_PLAY_RESET);
  SetPlayMode(toDevice, VID_PLAY_RESET);
}

void cTransferBuffer::Action(void)
{
  dsyslog(LOG_INFO, "data transfer thread started (pid=%d)", getpid());

  cRingBuffer_ Buffer(&fromDevice, &toDevice, VIDEOBUFSIZE, 0, 0);
  active = true;
  while (active && Buffer.Available() < 100000) { // need to give the read buffer a head start
        Buffer.Read(); // initializes fromDevice for reading
        usleep(1); // this keeps the CPU load low
        }
  while (active) {
        if (Buffer.Read() < 0 || Buffer.Write() < 0)
           break;
        usleep(1); // this keeps the CPU load low
        }
  dsyslog(LOG_INFO, "data transfer thread stopped (pid=%d)", getpid());
}

// --- cCuttingBuffer --------------------------------------------------------

class cCuttingBuffer : public cRingBuffer_, public cThread {
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
:cRingBuffer_(&fromFile, &toFile, VIDEOBUFSIZE, 0, VIDEOBUFSIZE / 10)
{
  active = false;
  fromFile = toFile = -1;
  fromFileName = toFileName = NULL;
  fromIndex = toIndex = NULL;
  if (fromMarks.Load(FromFileName) && fromMarks.Count()) {
     fromFileName = new cFileName(FromFileName, false);
     toFileName = new cFileName(ToFileName, true);
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
     while (active) {
           uchar FileNumber;
           int FileOffset, Length;
           uchar PictureType;
   
           Clear(); // makes sure one frame is completely read and written
   
           // Read one frame:
   
           if (fromIndex->Get(Index++, &FileNumber, &FileOffset, &PictureType, &Length)) {
              if (FileNumber != CurrentFileNumber) {
                 fromFile = fromFileName->SetOffset(FileNumber, FileOffset);
                 CurrentFileNumber = FileNumber;
                 }
              if (fromFile >= 0)
                 Length = cRingBuffer_::Read(Length);
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
           cRingBuffer_::Write(Length);
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

int cDvbApi::NumDvbApis = 0;
int cDvbApi::useDvbApi = 0;
cDvbApi *cDvbApi::dvbApi[MAXDVBAPI] = { NULL };
cDvbApi *cDvbApi::PrimaryDvbApi = NULL;

cDvbApi::cDvbApi(const char *VideoFileName, const char *VbiFileName)
{
  siProcessor = NULL;
  recordBuffer = NULL;
  replayBuffer = NULL;
  transferBuffer = NULL;
  transferringFromDvbApi = NULL;
  ca = 0;
  priority = -1;
  videoDev = open(VideoFileName, O_RDWR | O_NONBLOCK);
  if (videoDev >= 0) {
     siProcessor = new cSIProcessor(VbiFileName);
     if (!dvbApi[0]) // only the first one shall set the system time
        siProcessor->SetUseTSTime(Setup.SetSystemTime);
     siProcessor->AddFilter(0x14, 0x70);  // TDT
     siProcessor->AddFilter(0x14, 0x73);  // TOT
     siProcessor->AddFilter(0x12, 0x4e);  // event info, actual TS, present/following
     siProcessor->AddFilter(0x12, 0x4f);  // event info, other TS, present/following
     siProcessor->AddFilter(0x12, 0x50);  // event info, actual TS, schedule
     siProcessor->AddFilter(0x12, 0x60);  // event info, other TS, schedule
     siProcessor->Start();
     }
  else
     LOG_ERROR_STR(VideoFileName);
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
  if (videoDev >= 0) {
     delete siProcessor;
     Close();
     StopReplay();
     StopRecord();
     StopTransfer();
     OvlO(false); //Overlay off!
     //XXX the following call sometimes causes a segfault - driver problem?
     close(videoDev);
     }
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
  for (int i = MAXDVBAPI; --i >= 0; ) {
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

bool cDvbApi::Init(void)
{
  NumDvbApis = 0;
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (useDvbApi == 0 || (useDvbApi & (1 << i)) != 0) {
         char fileName[strlen(VIDEODEVICE) + 10];
         sprintf(fileName, "%s%d", VIDEODEVICE, i);
         if (access(fileName, F_OK | R_OK | W_OK) == 0) {
            dsyslog(LOG_INFO, "probing %s", fileName);
            int f = open(fileName, O_RDWR);
            if (f >= 0) {
               struct video_capability cap;
               int r = ioctl(f, VIDIOCGCAP, &cap);
               close(f);
               if (r == 0 && (cap.type & VID_TYPE_DVB)) {
                  char vbiFileName[strlen(VBIDEVICE) + 10];
                  sprintf(vbiFileName, "%s%d", VBIDEVICE, i);
                  dvbApi[NumDvbApis++] = new cDvbApi(fileName, vbiFileName);
                  }
               }
            else {
               if (errno != ENODEV)
                  LOG_ERROR_STR(fileName);
               break;
               }
            }
         else {
            if (errno != ENOENT)
               LOG_ERROR_STR(fileName);
            break;
            }
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
#else
void cDvbApi::Cmd(OSD_Command cmd, int color, int x0, int y0, int x1, int y1, const void *data)
{
  if (videoDev >= 0) {
     struct drawcmd dc;
     dc.cmd   = cmd;
     dc.color = color;
     dc.x0    = x0;
     dc.y0    = y0;
     dc.x1    = x1;
     dc.y1    = y1;
     dc.data  = (void *)data;
     ioctl(videoDev, VIDIOCSOSDCOMMAND, &dc);
     usleep(10); // XXX Workaround for a driver bug (cInterface::DisplayChannel() displayed texts at wrong places
                 // XXX and sometimes the OSD was no longer displayed).
                 // XXX Increase the value if the problem still persists on your particular system.
                 // TODO Check if this is still necessary with driver versions after 0.6.
     }
}
#endif

void cDvbApi::Open(int w, int h)
{
  int d = (h < 0) ? MenuLines + h : 0;
  h = abs(h);
  cols = w;
  rows = h;
#ifdef DEBUG_OSD
  window = subwin(stdscr, h, w, d, 0);
  syncok(window, true);
  #define B2C(b) (((b) * 1000) / 255)
  #define SETCOLOR(n, r, g, b, o) init_color(n, B2C(r), B2C(g), B2C(b))
#else
  w *= charWidth;
  h *= lineHeight;
  d *= lineHeight;
  int x = (720 - MenuColumns * charWidth) / 2; //TODO PAL vs. NTSC???
  int y = (576 - MenuLines * lineHeight) / 2 + d;
  osd = new cDvbOsd(videoDev, x, y, x + w - 1, y + h - 1, 4);
  #define SETCOLOR(n, r, g, b, o) Cmd(OSD_SetColor, n, r, g, b, o)
  SETCOLOR(clrTransparent, 0x00, 0x00, 0x00,   0);
#endif
  SETCOLOR(clrBackground,  0x00, 0x00, 0x00, 127); // background 50% gray
  SETCOLOR(clrBlack,       0x00, 0x00, 0x00, 255);
  SETCOLOR(clrRed,         0xFC, 0x14, 0x14, 255);
  SETCOLOR(clrGreen,       0x24, 0xFC, 0x24, 255);
  SETCOLOR(clrYellow,      0xFC, 0xC0, 0x24, 255);
  SETCOLOR(clrBlue,        0x00, 0x00, 0xFC, 255);
  SETCOLOR(clrCyan,        0x00, 0xFC, 0xFC, 255);
  SETCOLOR(clrMagenta,     0xB0, 0x00, 0xFC, 255);
  SETCOLOR(clrWhite,       0xFC, 0xFC, 0xFC, 255);
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

bool cDvbApi::SetChannel(int ChannelNumber, int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid, int Tpid, int Ca, int Pnr)
{
  if (videoDev >= 0) {
     cThreadLock ThreadLock(siProcessor); // makes sure the siProcessor won't access the vbi-device while switching
     StopTransfer();
     StopReplay();
     SetPlayMode(videoDev, VID_PLAY_RESET);
     struct frontend front;
     ioctl(videoDev, VIDIOCGFRONTEND, &front);
     unsigned int freq = FrequencyMHz;
     if (front.type == FRONT_DVBS) {
        front.ttk = (freq < 11700UL) ? 0 : 1;
        if (freq < 11700UL) {
           freq -= Setup.LnbFrequLo;
           front.ttk = 0;
           }
        else {
           freq -= Setup.LnbFrequHi;
           front.ttk = 1;
           }
        }
     front.channel_flags = Ca ? DVB_CHANNEL_CA : DVB_CHANNEL_FTA;
     front.pnr       = Pnr;
     front.freq      = freq * 1000000UL;
     front.diseqc    = Diseqc;
     front.srate     = Srate * 1000;
     front.volt      = (Polarization == 'v' || Polarization == 'V') ? 0 : 1;
     front.video_pid = Vpid;
     front.audio_pid = Apid;
     front.tt_pid    = Tpid;
     front.fec       = 8;
     front.AFC       = 1;
     front.qam       = 2;
     ioctl(videoDev, VIDIOCSFRONTEND, &front);
     if (front.sync & 0x1F == 0x1F) {
        if (this == PrimaryDvbApi && siProcessor)
           siProcessor->SetCurrentServiceID(Pnr);
        currentChannel = ChannelNumber;
        // If this DVB card can't receive this channel, let's see if we can
        // use the card that actually can receive it and transfer data from there:
        if (this == PrimaryDvbApi && Ca && Ca != Index() + 1) {
           cDvbApi *CaDvbApi = GetDvbApi(Ca, 0);
           if (CaDvbApi) {
              if (!CaDvbApi->Recording()) {
                 if (CaDvbApi->SetChannel(ChannelNumber, FrequencyMHz, Polarization, Diseqc, Srate, Vpid, Apid, Tpid, Ca, Pnr))
                    transferringFromDvbApi = CaDvbApi->StartTransfer(videoDev);
                 }
              }
           }
        return true;
        }
     esyslog(LOG_ERR, "ERROR: channel %d not sync'ed (front.sync=%X)!", ChannelNumber, front.sync);
     }
  return false;
}

bool cDvbApi::Transferring(void)
{
  return transferBuffer;
}

cDvbApi *cDvbApi::StartTransfer(int TransferToVideoDev)
{
  StopTransfer();
  transferBuffer = new cTransferBuffer(videoDev, TransferToVideoDev);
  return this;
}

void cDvbApi::StopTransfer(void)
{
  if (transferBuffer) {
     delete transferBuffer;
     transferBuffer = NULL;
     SetPlayMode(videoDev, VID_PLAY_RESET);
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
  if (videoDev >= 0) {

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

     recordBuffer = new cRecordBuffer(&videoDev, FileName);

     if (recordBuffer) {
        ca = Ca;
        priority = Priority;
        return true;
        }
     else
        esyslog(LOG_ERR, "ERROR: can't allocate recording buffer");
     }
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
  if (videoDev >= 0) {

     // Check FileName:

     if (!FileName) {
        esyslog(LOG_ERR, "ERROR: StartReplay: file name is (null)");
        return false;
        }
     isyslog(LOG_INFO, "replay %s", FileName);

     // Create replay buffer:

     replayBuffer = new cReplayBuffer(&videoDev, FileName);
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
            cDvbApi *DvbApi = cDvbApi::GetDvbApi(i, 0);
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

