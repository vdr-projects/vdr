/*
 * dvbapi.c: Interface to the DVB driver
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbapi.c 1.12 2000/07/15 13:33:04 kls Exp $
 */

#include "dvbapi.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "interface.h"
#include "tools.h"

#define VIDEODEVICE "/dev/video"

// The size of the array used to buffer video data:
#define VIDEOBUFSIZE (1024*1024)

// The minimum amount of video data necessary to identify frames
// (must be smaller than VIDEOBUFSIZE!):
#define MINVIDEODATA (20*1024) // just a safe guess (max. size of any AV_PES block, plus some safety)

// The maximum time the buffer is allowed to write data to disk when recording:
#define MAXRECORDWRITETIME 50 // ms

#define AV_PES_HEADER_LEN 8

// AV_PES block types:
#define AV_PES_VIDEO 1
#define AV_PES_AUDIO 2

// Picture types:
#define NO_PICTURE 0
#define I_FRAME    1
#define P_FRAME    2
#define B_FRAME    3

#define FRAMESPERSEC 25

// The maximum file size is limited by the range that can be covered
// with 'int'. 4GB might be possible (if the range is considered
// 'unsigned'), 2GB should be possible (even if the range is considered
// 'signed'), so let's use 1GB for absolute safety (the actual file size
// may be slightly higher because we stop recording only before the next
// 'I' frame, to have a complete Group Of Pictures):
#define MAXVIDEOFILESIZE (1024*1024*1024)
#define MAXFILESPERRECORDING 255

#define INDEXFILESUFFIX     "/index.vdr"
#define RESUMEFILESUFFIX    "/resume.vdr"
#define RECORDFILESUFFIX    "/%03d.vdr"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...

// The number of frames to back up when resuming an interrupted replay session:
#define RESUMEBACKUP (10 * FRAMESPERSEC)

typedef unsigned char uchar;

// --- cIndexFile ------------------------------------------------------------

class cIndexFile {
private:
  struct tIndex { int offset; uchar type; uchar number; short reserved; };
  int f;
  char *fileName, *pFileExt;
  int size, last, resume;
  tIndex *index;
  bool CatchUp(void);
public:
  cIndexFile(const char *FileName, bool Record = false);
  ~cIndexFile();
  void Write(uchar PictureType, uchar FileNumber, int FileOffset);
  bool Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType = NULL);
  int GetNextIFrame(int Index, bool Forward, uchar *FileNumber, int *FileOffset, int *Length = NULL);
  int Get(uchar FileNumber, int FileOffset);
  int Last(void) { return last; }
  int GetResume(void) { return resume; }
  bool StoreResume(int Index);
  static char *Str(int Index, bool WithFrame = false);
  };

cIndexFile::cIndexFile(const char *FileName, bool Record)
{
  f = -1;
  fileName = pFileExt = NULL;
  size = 0;
  last = resume = -1;
  index = NULL;
  if (FileName) {
     fileName = new char[strlen(FileName) + strlen(INDEXFILESUFFIX) + strlen(RESUMEFILESUFFIX) + 1];
     if (fileName) {                        // no max() function at hand...
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
           if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) >= 0) {
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
        else {
           strcpy(pFileExt, RESUMEFILESUFFIX);
           int resumeFile = open(fileName, O_RDONLY);
           if (resumeFile >= 0) {
              if (read(resumeFile, &resume, sizeof(resume)) != sizeof(resume)) {
                 resume = -1;
                 LOG_ERROR_STR(fileName);
                 }
              close(resumeFile);
              }
           else if (errno != ENOENT)
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

bool cIndexFile::Get(int Index, uchar *FileNumber, int *FileOffset, uchar *PictureType)
{
  if (index) {
     CatchUp();
     if (Index >= 0 && Index <= last) {
        *FileNumber = index[Index].number;
        *FileOffset = index[Index].offset;
        if (PictureType)
           *PictureType = index[Index].type;
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
               *FileNumber = index[Index].number;
               *FileOffset = index[Index].offset;
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

bool cIndexFile::StoreResume(int Index)
{
  if (fileName) {
     int resumeFile = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
     if (resumeFile >= 0) {
        if (write(resumeFile, &Index, sizeof(Index)) != sizeof(Index))
           LOG_ERROR_STR(fileName);
        close(resumeFile);
        return true;
        }
     else if (errno != ENOENT)
        LOG_ERROR_STR(fileName);
     }
  return false;
}

char *cIndexFile::Str(int Index, bool WithFrame)
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

// --- cRingBuffer -----------------------------------------------------------

/* cRingBuffer reads data from an input file, stores it in a buffer and writes
   it to an output file upon request. The Read() and Write() functions should
   be called only when the associated file is ready to provide or receive data
   (use the 'select()' function to determine that), and the files should be
   opened in non-blocking mode.
   The '...Limit' parameters define safety limits. If they are exceeded a log entry
   will be made.
*/

class cRingBuffer {
private:
  uchar *buffer;
  int size, head, tail, freeLimit, availLimit;
  int countLimit, countOverflow;
  int minFree;
  bool eof;
  int *inFile, *outFile;
protected:
  int Free(void) { return ((tail >= head) ? size + head - tail : head - tail) - 1; }
  int Available(void) { return (tail >= head) ? tail - head : size - head + tail; }
  int Readable(void) { return (tail >= head) ? size - tail - (head ? 0 : 1) : head - tail - 1; } // keep a 1 byte gap!
  int Writeable(void) { return (tail > head) ? tail - head : size - head; }
  int Byte(int Offset);
  bool WaitForOutFile(int Timeout);
public:
  cRingBuffer(int *InFile, int *OutFile, int Size, int FreeLimit = 0, int AvailLimit = 0);
  virtual ~cRingBuffer();
  virtual int Read(int Max = -1);
  virtual int Write(int Max = -1);
  bool EndOfFile(void) { return eof; }
  bool Empty(void) { return Available() == 0; }
  void Clear(void) { head = tail = 0; }
  void Skip(int n);
  };

cRingBuffer::cRingBuffer(int *InFile, int *OutFile, int Size, int FreeLimit, int AvailLimit)
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

cRingBuffer::~cRingBuffer()
{
  dsyslog(LOG_INFO, "buffer stats: %d free, %d overflows, limit exceeded %d times", minFree, countOverflow, countLimit);
  delete buffer;
}

int cRingBuffer::Byte(int Offset)
{
  if (buffer && Offset < Available()) {
     Offset += head;
     if (Offset >= size)
        Offset -= size;
     return buffer[Offset];
     }
  return -1;
}

void cRingBuffer::Skip(int n)
{
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

bool cRingBuffer::WaitForOutFile(int Timeout)
{
  fd_set set;
  FD_ZERO(&set);
  FD_SET(*outFile, &set);
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = Timeout;
  if (select(FD_SETSIZE, NULL, &set, NULL, &timeout) > 0) {
     if (FD_ISSET(*outFile, &set))
        return true;
     }
  esyslog(LOG_ERR, "ERROR: timeout in WaitForOutFile(%d)", Timeout);  
  return false;
}

int cRingBuffer::Read(int Max)
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
            else if (r < 0 && errno != EAGAIN) {
               LOG_ERROR;
               return -1;
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

int cRingBuffer::Write(int Max)
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

// --- cFileBuffer -----------------------------------------------------------

class cFileBuffer : public cRingBuffer {
protected:
  cIndexFile *index;
  uchar fileNumber;
  char *fileName, *pFileNumber;
  bool stop;
  int GetAvPesLength(void)
  { 
    if (Byte(0) == 'A' && Byte(1) == 'V' && Byte(4) == 'U')
       return (Byte(6) << 8) + Byte(7) + AV_PES_HEADER_LEN;
    return 0;
  }
  int GetAvPesType(void) { return Byte(2); }
  int GetAvPesTag(void) { return Byte(3); }
  int FindAvPesBlock(void);
  int GetPictureType(int Offset) { return (Byte(Offset + 5) >> 3) & 0x07; }
  int FindPictureStartCode(int Length);
public:
  cFileBuffer(int *InFile, int *OutFile, const char *FileName, bool Recording, int Size, int FreeLimit = 0, int AvailLimit = 0);
  virtual ~cFileBuffer();
  void Stop(void) { stop = true; }
  };

cFileBuffer::cFileBuffer(int *InFile, int *OutFile, const char *FileName, bool Recording, int Size, int FreeLimit = 0, int AvailLimit = 0)
:cRingBuffer(InFile, OutFile, Size, FreeLimit, AvailLimit)
{
  index = NULL;
  fileNumber = 0;
  stop = false;
  // Prepare the file name:
  fileName = new char[strlen(FileName) + RECORDFILESUFFIXLEN];
  if (!fileName) {
     esyslog(LOG_ERR, "ERROR: can't copy file name '%s'", fileName);
     return;
     }
  strcpy(fileName, FileName);
  pFileNumber = fileName + strlen(fileName);
  // Create the index file:
  index = new cIndexFile(FileName, Recording);
  if (!index)
     esyslog(LOG_ERR, "ERROR: can't allocate index");
     // let's continue without index, so we'll at least have the recording
}

cFileBuffer::~cFileBuffer()
{
  delete index;
  delete fileName;
}

int cFileBuffer::FindAvPesBlock(void)
{
  int Skipped = 0;

  while (Available() > MINVIDEODATA) {
        if (GetAvPesLength())
           return Skipped;
        Skip(1);
        Skipped++;
        }
  return -1;
}

int cFileBuffer::FindPictureStartCode(int Length)
{
  for (int i = AV_PES_HEADER_LEN; i < Length; i++) {
      if (Byte(i) == 0 && Byte(i + 1) == 0 && Byte(i + 2) == 1 && Byte(i + 3) == 0)
         return i;
      }
  return -1;
}

// --- cRecordBuffer ---------------------------------------------------------

class cRecordBuffer : public cFileBuffer {
private:
  uchar pictureType;
  int fileSize;
  int recordFile;
  uchar tagAudio, tagVideo;
  bool ok, synced;
  int Synchronize(void);
  bool NextFile(void);
  virtual int Write(int Max = -1);
public:
  cRecordBuffer(int *InFile, const char *FileName);
  virtual ~cRecordBuffer();
  int WriteWithTimeout(bool EndIfEmpty = false);
  };

cRecordBuffer::cRecordBuffer(int *InFile, const char *FileName)
:cFileBuffer(InFile, &recordFile, FileName, true, VIDEOBUFSIZE, VIDEOBUFSIZE / 10, 0)
{
  pictureType = NO_PICTURE;
  fileSize = 0;
  recordFile = -1;
  tagAudio = tagVideo = 0;
  ok = synced = false;
  if (!fileName)
     return;//XXX find a better way???
  // Find the highest existing file suffix:
  for (;;) {
      sprintf(pFileNumber, RECORDFILESUFFIX, ++fileNumber);
      if (access(fileName, F_OK) < 0) {
         if (errno == ENOENT)
            break; // found a non existing file suffix
         LOG_ERROR;
         return;
         }
      }
  ok = true;
  //XXX hack to make the video device go into 'recording' mode:
  char dummy;
  read(*InFile, &dummy, sizeof(dummy));
}

cRecordBuffer::~cRecordBuffer()
{
  if (recordFile >= 0)
     close(recordFile);
}

int cRecordBuffer::Synchronize(void)
{
  int Length = 0;
  int Skipped = 0;
  int s;

  while ((s = FindAvPesBlock()) >= 0) {
        pictureType = NO_PICTURE;
        Skipped += s;
        Length = GetAvPesLength();
        if (Length <= MINVIDEODATA) {
           switch (GetAvPesType()) {
             case AV_PES_VIDEO:
                     {
                       int PictureOffset = FindPictureStartCode(Length);
                       if (PictureOffset >= 0) {
                          pictureType = GetPictureType(PictureOffset);
                          if (pictureType < I_FRAME || pictureType > B_FRAME)
                             esyslog(LOG_ERR, "ERROR: unknown picture type '%d'", pictureType);
                          }
                     }
                     if (!synced) {
                        tagVideo = GetAvPesTag();
                        if (pictureType == I_FRAME) {
                           synced = true;
                           Skipped = 0;
                           }
                        else {
                           Skip(Length - 1);
                           Length = 0;
                           }
                        }
                     else {
                        if (++tagVideo != GetAvPesTag()) {
                           esyslog(LOG_ERR, "ERROR: missed video data block #%d (next block is #%d)", tagVideo, GetAvPesTag());
                           tagVideo = GetAvPesTag();
                           }
                        }
                     break;
             case AV_PES_AUDIO:
                     if (!synced) {
                        tagAudio = GetAvPesTag();
                        Skip(Length - 1);
                        Length = 0;
                        }
                     else {
                        //XXX might get fooled the first time!!!
                        if (++tagAudio != GetAvPesTag()) {
                           esyslog(LOG_ERR, "ERROR: missed audio data block #%d (next block is #%d)", tagAudio, GetAvPesTag());
                           tagAudio = GetAvPesTag();
                           }
                        }
                     break;
             default: // unknown data
                      Length = 0; // don't skip entire block - maybe we're just not in sync with AV_PES yet
                      if (synced)
                         esyslog(LOG_ERR, "ERROR: unknown data type '%d'", GetAvPesType());
             }
           if (Length > 0)
              break;
           }
        else if (synced) {
           esyslog(LOG_ERR, "ERROR: block length too big (%d)", Length);
           Length = 0;
           }
        Skip(1);
        Skipped++;
        }
  if (synced && Skipped)
     esyslog(LOG_ERR, "ERROR: skipped %d bytes", Skipped);
  return Length;
}

bool cRecordBuffer::NextFile(void)
{
  if (recordFile >= 0 && fileSize > MAXVIDEOFILESIZE && pictureType == I_FRAME) {
     if (close(recordFile) < 0)
        LOG_ERROR;
        // don't return 'false', maybe we can still record into the next file
     recordFile = -1;
     fileNumber++;
     if (fileNumber == 0)
        esyslog(LOG_ERR, "ERROR: max number of files (%d) exceeded", MAXFILESPERRECORDING);
     fileSize = 0;
     }
  if (recordFile < 0) {
     sprintf(pFileNumber, RECORDFILESUFFIX, fileNumber);
     dsyslog(LOG_INFO, "recording to '%s'", fileName);
     recordFile = open(fileName, O_RDWR | O_CREAT | O_NONBLOCK, S_IRUSR | S_IWUSR);
     if (recordFile < 0) {
        LOG_ERROR;
        return false;
        }
     }
  return true;
}

int cRecordBuffer::Write(int Max)
{
  // This function ignores the incoming 'Max'!
  // It tries to write out exactly *one* block of AV_PES data.
  if (!ok)
     return -1;
  int n = Synchronize();
  if (n) {
     if (stop && pictureType == I_FRAME) {
        ok = false;
        return -1; // finish the recording before the next 'I' frame       
        }
     if (NextFile()) {
        if (index && pictureType != NO_PICTURE)
           index->Write(pictureType, fileNumber, fileSize);
        int written = 0;
        for (;;) {
            int w = cFileBuffer::Write(n);
            if (w >= 0) {
               fileSize += w;
               written += w;
               n -= w;
               if (n == 0)
                  return written;
               }
            else
               return w;
            }
        }
     return -1;
     }
  return 0;
}

int cRecordBuffer::WriteWithTimeout(bool EndIfEmpty)
{
  int w, written = 0;
  int t0 = time_ms();
  while ((w = Write()) > 0 && time_ms() - t0 < MAXRECORDWRITETIME)
        written += w;
  return w < 0 ? w : (written == 0 && EndIfEmpty ? -1 : written);
}

// --- cReplayBuffer ---------------------------------------------------------

enum eReplayMode { rmPlay, rmFastForward, rmFastRewind };

class cReplayBuffer : public cFileBuffer {
private:
  int fileOffset;
  int replayFile;
  eReplayMode mode;
  bool skipAudio;
  int lastIndex;
  void SkipAudioBlocks(void);
  bool NextFile(uchar FileNumber = 0, int FileOffset = -1);
  void Close(void);
public:
  cReplayBuffer(int *OutFile, const char *FileName);
  virtual ~cReplayBuffer();  
  virtual int Read(int Max = -1);
  virtual int Write(int Max = -1);
  void SetMode(eReplayMode Mode);
  int Resume(void);
  bool Save(void);
  void SkipSeconds(int Seconds);
  void GetIndex(int &Current, int &Total);
  };

cReplayBuffer::cReplayBuffer(int *OutFile, const char *FileName)
:cFileBuffer(&replayFile, OutFile, FileName, false, VIDEOBUFSIZE, 0, VIDEOBUFSIZE / 10)
{
  fileOffset = 0;
  replayFile = -1;
  mode = rmPlay;
  skipAudio = false;
  lastIndex = -1;
  if (!fileName)
     return;//XXX find a better way???
  // All recordings start with '1':
  fileNumber = 1; //TODO what if it doesn't start with '1'???
  //XXX hack to make the video device go into 'replaying' mode:
  char dummy;
  write(*OutFile, &dummy, sizeof(dummy));
}

cReplayBuffer::~cReplayBuffer()
{
  Close();
}

void cReplayBuffer::Close(void)
{
  if (replayFile >= 0) {
     if (close(replayFile) < 0)
        LOG_ERROR;
     replayFile = -1;
     fileOffset = 0;
     }
}

void cReplayBuffer::SetMode(eReplayMode Mode)
{
  mode = Mode;
  skipAudio = Mode != rmPlay;
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
     int Index = index->Get(fileNumber, fileOffset);
     if (Index >= 0) {
        Index -= RESUMEBACKUP;
        if (Index > 0) {
           uchar FileNumber;
           int FileOffset;
           Index = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset);
           }
        else
           Index = 0;
        if (Index >= 0)
           return index->StoreResume(Index);
        }
     }
  return false;
}

void cReplayBuffer::SkipSeconds(int Seconds)
{
  if (index && Seconds) {
     int Index = index->Get(fileNumber, fileOffset);
     if (Index >= 0) {
        if (Seconds < 0) {
           int sec = index->Last() / FRAMESPERSEC;
           if (Seconds < -sec)
              Seconds = - sec;
           }
        Index += Seconds * FRAMESPERSEC;
        if (Index < 0)
           Index = 1; // not '0', to allow GetNextIFrame() below to work!
        uchar FileNumber;
        int FileOffset;
        if (index->GetNextIFrame(Index, false, &FileNumber, &FileOffset) >= 0)
        if ((Index = index->GetNextIFrame(Index, false, &FileNumber, &FileOffset)) >= 0)
           NextFile(FileNumber, FileOffset);
        }
     }
}

void cReplayBuffer::GetIndex(int &Current, int &Total)
{
  if (index) {
     Current = index->Get(fileNumber, fileOffset);
     Total = index->Last();
     }
  else
     Current = Total = -1;
}

void cReplayBuffer::SkipAudioBlocks(void)
{
  int Length;

  while ((Length = GetAvPesLength()) > 0) {
        if (GetAvPesType() == AV_PES_AUDIO)
           Skip(Length);
        else
           break;
        }
}

bool cReplayBuffer::NextFile(uchar FileNumber, int FileOffset)
{
  if (FileNumber > 0) {
     Clear();
     if (FileNumber != fileNumber) {
        Close();
        fileNumber = FileNumber;
        }
     }
  if (replayFile >= 0 && EndOfFile()) {
     Close();
     fileNumber++;
     if (fileNumber == 0)
        esyslog(LOG_ERR, "ERROR: max number of files (%d) exceeded", MAXFILESPERRECORDING);
     }
  if (replayFile < 0) {
     sprintf(pFileNumber, RECORDFILESUFFIX, fileNumber);
     if (access(fileName, R_OK) == 0) {
        dsyslog(LOG_INFO, "playing '%s'", fileName);
        replayFile = open(fileName, O_RDONLY | O_NONBLOCK);
        if (replayFile < 0) {
           LOG_ERROR;
           return false;
           }
        }
     else if (errno != ENOENT)
        LOG_ERROR;
     }
  if (replayFile >= 0) {
     if (FileOffset >= 0) {
        if ((fileOffset = lseek(replayFile, FileOffset, SEEK_SET)) != FileOffset)
           LOG_ERROR;
           // don't return 'false', maybe we can still replay the file
        }
     return true;
     }
  return false;
}

int cReplayBuffer::Read(int Max = -1)
{
  if (stop)
     return -1;
  if (mode != rmPlay) {
     if (index) {
        if (Available())
           return 0; // write out the entire block
        int Index = (lastIndex >= 0) ? lastIndex : index->Get(fileNumber, fileOffset);
        if (Index >= 0) {
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
  else
     lastIndex = -1;
  //XXX timeout as in recording???
  if (NextFile()) {
     int readin = 0;
     do {
        // If Max is > 0 here we need to make sure we read in the entire block!
        int r = cFileBuffer::Read(Max);
        if (r >= 0)
           readin += r;
        else
           return -1;
        } while (readin < Max && Free() > 0);
     return readin;
     }
  if (Available() > 0)
     return 0;
  return -1;
}

int cReplayBuffer::Write(int Max)
{
  int Written = 0;

  do {
     if (skipAudio) {
        SkipAudioBlocks();
        Max = GetAvPesLength();
        }
     while (Max) {
           int w = cFileBuffer::Write(Max);
           if (w >= 0) {
              fileOffset += w;
              Written += w;
              if (Max < 0)
                 break;
              Max -= w;
              }
           else
              return w;
               //XXX??? Why does the buffer get empty here???
           if (Empty() || !WaitForOutFile(1000000))
              return Written;
           }
     } while (skipAudio && Available());
  return Written;
}

// --- cDvbApi ---------------------------------------------------------------

int cDvbApi::NumDvbApis = 0;
cDvbApi *cDvbApi::dvbApi[MAXDVBAPI] = { NULL };
cDvbApi *cDvbApi::PrimaryDvbApi = NULL;

cDvbApi::cDvbApi(const char *FileName)
{
  pidRecord = pidReplay = 0;
  fromRecord = toRecord = -1;
  fromReplay = toReplay = -1;
  videoDev = open(FileName, O_RDWR | O_NONBLOCK);
  if (videoDev < 0)
     LOG_ERROR;
  cols = rows = 0;
#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
  initscr();
  keypad(stdscr, TRUE);
  nonl();
  cbreak();
  noecho();
  timeout(1000);
#endif
#if defined(DEBUG_OSD)
  memset(&colorPairs, 0, sizeof(colorPairs));
  start_color();
  leaveok(stdscr, TRUE);
  window = NULL;
#endif
  lastProgress = lastTotal = -1;
  replayTitle = NULL;
}

cDvbApi::~cDvbApi()
{
  if (videoDev >= 0) {
     Close();
     StopReplay();
     StopRecord();
     close(videoDev);
     }
#if defined(DEBUG_OSD) || defined(REMOTE_KBD)
  endwin();
#endif
  delete replayTitle;
}

cDvbApi *cDvbApi::GetDvbApi(int Ca)
{
  Ca--;
  for (int i = MAXDVBAPI; --i >= 0; ) {
      if (dvbApi[i]) {
         if ((i == Ca || Ca < 0) && !dvbApi[i]->Recording())
            return dvbApi[i];
         }
      }
  return NULL;
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
  char fileName[strlen(VIDEODEVICE) + 10];
  int i;

  NumDvbApis = 0;
  for (i = 0; i < MAXDVBAPI; i++) {
      sprintf(fileName, "%s%d", VIDEODEVICE, i);
      if (access(fileName, F_OK | R_OK | W_OK) == 0) {
         dsyslog(LOG_INFO, "probing %s", fileName);
         int f = open(fileName, O_RDWR);
         if (f >= 0) {
            close(f);
            dvbApi[i] = new cDvbApi(fileName);
            NumDvbApis++;
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
  PrimaryDvbApi = dvbApi[0];
  if (NumDvbApis > 0)
     isyslog(LOG_INFO, "found %d video device%s", NumDvbApis, NumDvbApis > 1 ? "s" : "");
  else
     esyslog(LOG_ERR, "ERROR: no video device found, giving up!");
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
  syncok(window, TRUE);
  #define B2C(b) (((b) * 1000) / 255)
  #define SETCOLOR(n, r, g, b, o) init_color(n, B2C(r), B2C(g), B2C(b))
#else
  w *= charWidth;
  h *= lineHeight;
  d *= lineHeight;
  int x = (720 - MenuColumns * charWidth) / 2; //TODO PAL vs. NTSC???
  int y = (576 - MenuLines * lineHeight) / 2 + d;
  Cmd(OSD_Open, 4, x, y, x + w - 1, y + h - 1);
  #define SETCOLOR(n, r, g, b, o) Cmd(OSD_SetColor, n, r, g, b, o)
#endif
  SETCOLOR(clrBackground, 0x00, 0x00, 0x00, 127); // background 50% gray
  SETCOLOR(clrBlack,      0x00, 0x00, 0x00, 255);
  SETCOLOR(clrRed,        0xFC, 0x14, 0x14, 255);
  SETCOLOR(clrGreen,      0x24, 0xFC, 0x24, 255);
  SETCOLOR(clrYellow,     0xFC, 0xC0, 0x24, 255);
  SETCOLOR(clrBlue,       0x00, 0x00, 0xFC, 255);
  SETCOLOR(clrCyan,       0x00, 0xFC, 0xFC, 255);
  SETCOLOR(clrMagenta,    0xB0, 0x00, 0xFC, 255);
  SETCOLOR(clrWhite,      0xFC, 0xFC, 0xFC, 255);

  lastProgress = lastTotal = -1;
}

void cDvbApi::Close(void)
{
#ifdef DEBUG_OSD
  delwin(window);
#else
  Cmd(OSD_Close);
#endif
  lastProgress = lastTotal = -1;
}

void cDvbApi::Clear(void)
{
#ifdef DEBUG_OSD
  SetColor(clrBackground, clrBackground);
  Fill(0, 0, cols, rows, clrBackground);
#else
  Cmd(OSD_Clear);
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
  Cmd(OSD_FillBlock, color, x * charWidth, y * lineHeight, (x + w) * charWidth - 1, (y + h) * lineHeight - 1);
#endif
}

void cDvbApi::ClrEol(int x, int y, eDvbColor color)
{
  Fill(x, y, cols - x, 1, color);
}

void cDvbApi::Text(int x, int y, const char *s, eDvbColor colorFg, eDvbColor colorBg)
{
  if (x < 0) x = cols + x;
  if (y < 0) y = rows + y;
#ifdef DEBUG_OSD
  SetColor(colorFg, colorBg);
  wmove(window, y, x); // ncurses wants 'y' before 'x'!
  waddstr(window, s);
#else
  Cmd(OSD_Text, (int(colorBg) << 16) | colorFg, x * charWidth, y * lineHeight, 1, 0, s);
#endif
}

bool cDvbApi::ShowProgress(bool Initial)
{
  int Current, Total;

  if (GetIndex(&Current, &Total)) {
     if (Initial) {
        if (replayTitle)
           Text(0, 0, replayTitle);
        }
     if (Total != lastTotal)
        Text(-7, 2, cIndexFile::Str(Total));
#ifdef DEBUG_OSD
     int p = cols * Current / Total;
     Fill(0, 1, p, 1, clrGreen);
     Fill(p, 1, cols - p, 1, clrWhite);
#else
     int w = cols * charWidth;
     int p = w * Current / Total;
     if (p != lastProgress) {
        int y1 = 1 * lineHeight;
        int y2 = 2 * lineHeight - 1;
        int x1, x2;
        eDvbColor color;
        if (lastProgress < p) {
           x1 = lastProgress + 1;
           x2 = p;
           if (p >= w)
              p = w - 1;
           color = clrGreen;
           }
        else {
           x1 = p + 1;
           x2 = lastProgress;
           color = clrWhite;
           }
        if (lastProgress < 0)
           Cmd(OSD_FillBlock, clrWhite, 0, y1, w - 1, y2);
        Cmd(OSD_FillBlock, color, x1, y1, x2, y2);
        lastProgress = p;
        }
#endif
     Text(0, 2, cIndexFile::Str(Current));
     lastTotal = Total;
     return true;
     }
  return false;
}

bool cDvbApi::SetChannel(int FrequencyMHz, char Polarization, int Diseqc, int Srate, int Vpid, int Apid, int Ca, int Pnr)
{
  if (videoDev >= 0) {
     struct frontend front;
     ioctl(videoDev, VIDIOCGFRONTEND, &front);
     unsigned int freq = FrequencyMHz;
     front.ttk = (freq < 11700UL) ? 0 : 1;
     if (freq < 11700UL)
        freq -=  9750UL;
     else
        freq -= 10600UL;
     front.channel_flags = Ca ? DVB_CHANNEL_CA : DVB_CHANNEL_FTA;
     front.pnr       = Pnr;
     front.freq      = freq * 1000000UL;
     front.diseqc    = Diseqc;
     front.srate     = Srate * 1000;
     front.volt      = (Polarization == 'v') ? 0 : 1;
     front.video_pid = Vpid;
     front.audio_pid = Apid;
     front.fec       = 8;
     front.AFC       = 1;
     ioctl(videoDev, VIDIOCSFRONTEND, &front);
     if (front.sync & 0x1F == 0x1F)
        return true;
     esyslog(LOG_ERR, "ERROR: channel not sync'ed (front.sync=%X)!", front.sync);
     }
  return false;
}

bool cDvbApi::Recording(void)
{
  if (pidRecord && !CheckProcess(pidRecord))
     pidRecord = 0;
  return pidRecord;
}

bool cDvbApi::Replaying(void)
{
  if (pidReplay && !CheckProcess(pidReplay))
     pidReplay = 0;
  return pidReplay;
}

bool cDvbApi::StartRecord(const char *FileName)
{
  if (Recording()) {
     esyslog(LOG_ERR, "ERROR: StartRecord() called while recording - ignored!");
     return false;
     }
  if (videoDev >= 0) {

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

     // Open pipes for recording process:

     int fromRecordPipe[2], toRecordPipe[2];

     if (pipe(fromRecordPipe) != 0) {
        LOG_ERROR;
        return false;
        }
     if (pipe(toRecordPipe) != 0) {
        LOG_ERROR;
        return false;
        }

     // Create recording process:

     pidRecord = fork();
     if (pidRecord < 0) {
        LOG_ERROR;
        return false;
        }
     if (pidRecord == 0) {

        // This is the actual recording process

        dsyslog(LOG_INFO, "start recording process (pid=%d)", getpid());
        bool DataStreamBroken = false;
        int fromMain = toRecordPipe[0];
        int toMain = fromRecordPipe[1];
        cRecordBuffer *Buffer = new cRecordBuffer(&videoDev, FileName);
        if (Buffer) {
           for (;;) {
               fd_set set;
               FD_ZERO(&set);
               FD_SET(videoDev, &set);
               FD_SET(fromMain, &set);
               struct timeval timeout;
               timeout.tv_sec = 1;
               timeout.tv_usec = 0;
               bool ForceEnd = false;
               if (select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0) {
                  if (FD_ISSET(videoDev, &set)) {
                     if (Buffer->Read() < 0)
                        break;
                     DataStreamBroken = false;
                     }
                  if (FD_ISSET(fromMain, &set)) {
                     switch (readchar(fromMain)) {
                       case dvbStop: Buffer->Stop();
                                     ForceEnd = DataStreamBroken;
                                     break;
                       }
                     }
                  }
               else {
                  DataStreamBroken = true;
                  esyslog(LOG_ERR, "ERROR: video data stream broken");
                  }
               if (Buffer->WriteWithTimeout(ForceEnd) < 0)
                  break;
               }
           delete Buffer;
           }
        else
           esyslog(LOG_ERR, "ERROR: can't allocate recording buffer");
        close(fromMain);
        close(toMain);
        dsyslog(LOG_INFO, "end recording process");
        exit(0);
        }

     // Establish communication with the recording process:

     fromRecord = fromRecordPipe[0];
     toRecord = toRecordPipe[1];
     return true;
     }
  return false;
}

void cDvbApi::StopRecord(void)
{
  if (pidRecord) {
     writechar(toRecord, dvbStop);
     close(toRecord);
     close(fromRecord);
     toRecord = fromRecord = -1;
     KillProcess(pidRecord);
     pidRecord = 0;
     SetReplayMode(VID_PLAY_RESET); //XXX
     }
}

void cDvbApi::SetReplayMode(int Mode)
{
  if (videoDev >= 0) {
     struct video_play_mode pmode;
     pmode.mode = Mode;
     ioctl(videoDev, VIDIOCSPLAYMODE, &pmode);
     }
}

bool cDvbApi::StartReplay(const char *FileName, const char *Title)
{
  if (Recording()) {
     esyslog(LOG_ERR, "ERROR: StartReplay() called while recording - ignored!");
     return false;
     }
  StopReplay();
  if (videoDev >= 0) {

     lastProgress = lastTotal = -1;
     delete replayTitle;
     if (Title) {
        if ((replayTitle = strdup(Title)) == NULL)
           esyslog(LOG_ERR, "ERROR: StartReplay: can't copy title '%s'", Title);
        }

     // Check FileName:

     if (!FileName) {
        esyslog(LOG_ERR, "ERROR: StartReplay: file name is (null)");
        return false;
        }
     isyslog(LOG_INFO, "replay %s", FileName);

     // Open pipes for replay process:

     int fromReplayPipe[2], toReplayPipe[2];

     if (pipe(fromReplayPipe) != 0) {
        LOG_ERROR;
        return false;
        }
     if (pipe(toReplayPipe) != 0) {
        LOG_ERROR;
        return false;
        }

     // Create replay process:

     pidReplay = fork();
     if (pidReplay < 0) {
        LOG_ERROR;
        return false;
        }
     if (pidReplay == 0) {

        // This is the actual replaying process

        dsyslog(LOG_INFO, "start replaying process (pid=%d)", getpid());
        int fromMain = toReplayPipe[0];
        int toMain = fromReplayPipe[1];
        cReplayBuffer *Buffer = new cReplayBuffer(&videoDev, FileName);
        if (Buffer) {
           bool Paused = false;
           bool FastForward = false;
           bool FastRewind = false;
           int ResumeIndex = Buffer->Resume();
           if (ResumeIndex >= 0)
              isyslog(LOG_INFO, "resuming replay at index %d (%s)", ResumeIndex, cIndexFile::Str(ResumeIndex, true));
           for (;;) {
               if (Buffer->Read() < 0)
                  break;
               fd_set setIn, setOut;
               FD_ZERO(&setIn);
               FD_ZERO(&setOut);
               FD_SET(fromMain, &setIn);
               FD_SET(videoDev, &setOut);
               struct timeval timeout;
               timeout.tv_sec = 1;
               timeout.tv_usec = 0;
               if (select(FD_SETSIZE, &setIn, &setOut, NULL, &timeout) > 0) {
                  if (FD_ISSET(videoDev, &setOut)) {
                     if (Buffer->Write() < 0)
                        break;
                     }
                  if (FD_ISSET(fromMain, &setIn)) {
                     switch (readchar(fromMain)) {
                       case dvbStop:        SetReplayMode(VID_PLAY_CLEAR_BUFFER);
                                            Buffer->Stop(); break;
                       case dvbPauseReplay: SetReplayMode(Paused ? VID_PLAY_NORMAL : VID_PLAY_PAUSE);
                                            Paused = !Paused;
                                            FastForward = FastRewind = false;
                                            Buffer->SetMode(rmPlay);
                                            break;
                       case dvbFastForward: SetReplayMode(VID_PLAY_NORMAL);
                                            FastForward = !FastForward;
                                            FastRewind = Paused = false;
                                            Buffer->SetMode(FastForward ? rmFastForward : rmPlay);
                                            break;
                       case dvbFastRewind:  SetReplayMode(VID_PLAY_NORMAL);
                                            FastRewind = !FastRewind;
                                            FastForward = Paused = false;
                                            Buffer->SetMode(FastRewind ? rmFastRewind : rmPlay);
                                            break;
                       case dvbSkip:        {
                                              int Seconds;
                                              if (readint(fromMain, Seconds)) {
                                                 SetReplayMode(VID_PLAY_CLEAR_BUFFER);
                                                 SetReplayMode(VID_PLAY_NORMAL);
                                                 FastForward = FastRewind = Paused = false;
                                                 Buffer->SetMode(rmPlay);
                                                 Buffer->SkipSeconds(Seconds);
                                                 }
                                            }
                       case dvbGetIndex:    {
                                              int Current, Total;
                                              Buffer->GetIndex(Current, Total);
                                              writeint(toMain, Current);
                                              writeint(toMain, Total);
                                            }
                                            break;
                       }
                     }
                  }
               }
           Buffer->Save();
           delete Buffer;
           }
        else
           esyslog(LOG_ERR, "ERROR: can't allocate replaying buffer");
        close(fromMain);
        close(toMain);
        SetReplayMode(VID_PLAY_RESET); //XXX
        dsyslog(LOG_INFO, "end replaying process");
        exit(0);
        }

     // Establish communication with the replay process:

     fromReplay = fromReplayPipe[0];
     toReplay = toReplayPipe[1];
     return true;
     }
  return false;
}

void cDvbApi::StopReplay(void)
{
  if (pidReplay) {
     writechar(toReplay, dvbStop);
     close(toReplay);
     close(fromReplay);
     toReplay = fromReplay = -1;
     KillProcess(pidReplay);
     pidReplay = 0;
     SetReplayMode(VID_PLAY_RESET); //XXX
     }
}

void cDvbApi::PauseReplay(void)
{
  if (pidReplay)
     writechar(toReplay, dvbPauseReplay);
}

void cDvbApi::FastForward(void)
{
  if (pidReplay)
     writechar(toReplay, dvbFastForward);
}

void cDvbApi::FastRewind(void)
{
  if (pidReplay)
     writechar(toReplay, dvbFastRewind);
}

void cDvbApi::Skip(int Seconds)
{
  if (pidReplay) {
     writechar(toReplay, dvbSkip);
     writeint(toReplay, Seconds);
     }
}

bool cDvbApi::GetIndex(int *Current, int *Total)
{
  if (pidReplay) {
     int total;
     purge(fromReplay);
     writechar(toReplay, dvbGetIndex);
     if (readint(fromReplay, *Current) && readint(fromReplay, total)) {
        if (Total)
           *Total = total;
        }
     else
        *Current = -1;
     return *Current >= 0;
     }
  return false;
}

