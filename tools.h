/*
 * tools.h: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 5.13 2025/11/21 09:52:51 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <iconv.h>
#include <math.h>
#include <poll.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "thread.h"

typedef unsigned char uchar;

extern int SysLogLevel;

#define esyslog(a...) void( (SysLogLevel > 0) ? syslog_with_tid(LOG_ERR,   a) : void() )
#define isyslog(a...) void( (SysLogLevel > 1) ? syslog_with_tid(LOG_INFO,  a) : void() )
#define dsyslog(a...) void( (SysLogLevel > 2) ? syslog_with_tid(LOG_DEBUG, a) : void() )

#define LOG_ERROR         esyslog("ERROR (%s,%d): %m", __FILE__, __LINE__)
#define LOG_ERROR_STR(s)  esyslog("ERROR (%s,%d): %s: %m", __FILE__, __LINE__, s)

#define SECSINDAY  86400

#define KILOBYTE(n) ((n) * 1024)
#define MEGABYTE(n) ((n) * 1024LL * 1024LL)

#define MALLOC(type, size)  (type *)malloc(sizeof(type) * (size))

template<class T> inline void DELETENULL(T *&p) { T *q = p; p = NULL; delete q; }

#define CHECK(s) { if ((s) < 0) LOG_ERROR; } // used for 'ioctl()' calls
#define FATALERRNO (errno && errno != EAGAIN && errno != EINTR)

#if __cplusplus >= 201103L
// any gcc >= 4.8.1; we have swap, min, max
#include <algorithm> // std::min, std::max, (c++98: also swap)
#include <utility> // std::swap (since c++11)
using std::min;
using std::max;
using std::swap;
#else
// no c++11 and old compiler, let's include our own templates
template<class T> inline T min(T a, T b) { return a <= b ? a : b; }
template<class T> inline T max(T a, T b) { return a >= b ? a : b; }
template<class T> inline void swap(T &a, T &b) { T t = a; a = b; b = t; }
#endif

template<class T> inline int sgn(T a) { return a < 0 ? -1 : a > 0 ? 1 : 0; }

template<class T> inline T constrain(T v, T l, T h) { return v < l ? l : v > h ? h : v; }

void syslog_with_tid(int priority, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

#define BCDCHARTOINT(x) (10 * ((x & 0xF0) >> 4) + (x & 0xF))
int BCD2INT(int x);

#define IsBitSet(v, b) ((v) & (1 << (b))) // checks if the bit at index b is set in v, where the least significant bit has index 0

// Unfortunately there are no platform independent macros for unaligned
// access, so we do it this way:

template<class T> inline T get_unaligned(T *p)
{
  struct s { T v; } __attribute__((packed));
  return ((s *)p)->v;
}

template<class T> inline void put_unaligned(unsigned int v, T* p)
{
  struct s { T v; } __attribute__((packed));
  ((s *)p)->v = v;
}

// Comparing doubles for equality is unsafe, but unfortunately we can't
// overwrite operator==(double, double), so this will have to do:

inline bool DoubleEqual(double a, double b)
{
  return fabs(a - b) <= DBL_EPSILON;
}

// When handling strings that might contain UTF-8 characters, it may be necessary
// to process a "symbol" that consists of several actual character bytes. The
// following functions allow transparently accessing a "char *" string without
// having to worry about what character set is actually used.

int Utf8CharLen(const char *s);
    ///< Returns the number of character bytes at the beginning of the given
    ///< string that form a UTF-8 symbol.
uint Utf8CharGet(const char *s, int Length = 0);
    ///< Returns the UTF-8 symbol at the beginning of the given string.
    ///< Length can be given from a previous call to Utf8CharLen() to avoid calculating
    ///< it again. If no Length is given, Utf8CharLen() will be called.
int Utf8CharSet(uint c, char *s = NULL);
    ///< Converts the given UTF-8 symbol to a sequence of character bytes and copies
    ///< them to the given string. Returns the number of bytes written. If no string
    ///< is given, only the number of bytes is returned and nothing is copied.
int Utf8SymChars(const char *s, int Symbols);
    ///< Returns the number of character bytes at the beginning of the given
    ///< string that form at most the given number of UTF-8 symbols.
int Utf8StrLen(const char *s);
    ///< Returns the number of UTF-8 symbols formed by the given string of
    ///< character bytes.
char *Utf8Strn0Cpy(char *Dest, const char *Src, int n);
    ///< Copies at most n character bytes from Src to Dest, making sure that the
    ///< resulting copy ends with a complete UTF-8 symbol. The copy is guaranteed
    ///< to be zero terminated.
    ///< Returns a pointer to Dest.
int Utf8ToArray(const char *s, uint *a, int Size);
    ///< Converts the given character bytes (including the terminating 0) into an
    ///< array of UTF-8 symbols of the given Size. Returns the number of symbols
    ///< in the array (without the terminating 0).
int Utf8FromArray(const uint *a, char *s, int Size, int Max = -1);
    ///< Converts the given array of UTF-8 symbols (including the terminating 0)
    ///< into a sequence of character bytes of at most Size length. Returns the
    ///< number of character bytes written (without the terminating 0).
    ///< If Max is given, only that many symbols will be converted.
    ///< The resulting string is always zero-terminated if Size is big enough.

// When allocating buffer space, make sure we reserve enough space to hold
// a string in UTF-8 representation:

#define Utf8BufSize(s) ((s) * 4 + 1)

// The following macros automatically use the correct versions of the character
// class functions:

#define Utf8to(conv, c) (cCharSetConv::SystemCharacterTable() ? to##conv(c) : tow##conv(c))
#define Utf8is(ccls, c) (cCharSetConv::SystemCharacterTable() ? is##ccls(c) : isw##ccls(c))

class cCharSetConv {
private:
  iconv_t cd;
  char *result;
  size_t length;
  static char *systemCharacterTable;
public:
  cCharSetConv(const char *FromCode = NULL, const char *ToCode = NULL);
     ///< Sets up a character set converter to convert from FromCode to ToCode.
     ///< If FromCode is NULL, the previously set systemCharacterTable is used
     ///< (or "UTF-8" if no systemCharacterTable has been set).
     ///< If ToCode is NULL, "UTF-8" is used.
  ~cCharSetConv();
  const char *Convert(const char *From, char *To = NULL, size_t ToLength = 0);
     ///< Converts the given Text from FromCode to ToCode (as set in the constructor).
     ///< If To is given, it is used to copy at most ToLength bytes of the result
     ///< (including the terminating 0) into that buffer. If To is not given,
     ///< the result is copied into a dynamically allocated buffer and is valid as
     ///< long as this object lives, or until the next call to Convert(). The
     ///< return value always points to the result if the conversion was successful
     ///< (even if a fixed size To buffer was given and the result didn't fit into
     ///< it). If the string could not be converted, the result points to the
     ///< original From string.
  static const char *SystemCharacterTable(void) { return systemCharacterTable; }
  static void SetSystemCharacterTable(const char *CharacterTable);
  };

class cString {
private:
  char *s;
public:
  cString(const char *S = NULL, bool TakePointer = false);
  cString(const char *S, const char *To); ///< Copies S up to To (exclusive). To must be a valid pointer into S. If To is NULL, everything is copied.
  cString(const cString &String);
  cString(cString &&String): s(String.s) { String.s = NULL; }
  virtual ~cString();
  operator const void * () const { return s; } // to catch cases where operator*() should be used
  operator const char * () const { return s; } // for use in (const char *) context
  const char * operator*() const { return s; } // for use in (const void *) context (printf() etc.)
  cString &operator=(const cString &String);
  cString &operator=(cString &&String);
  cString &operator=(const char *String);
  cString &Append(const char *String);
  cString &Append(char c);
  cString &Truncate(int Index); ///< Truncate the string at the given Index (if Index is < 0 it is counted from the end of the string).
  cString &CompactChars(char c); ///< Compact any sequence of characters 'c' to a single character, and strip all of them from the beginning and end of this string.
  static cString sprintf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
  static cString vsprintf(const char *fmt, va_list &ap);
  };

class cNullTerminate {
private:
  char *p;
  char c;
public:
  cNullTerminate(void) {
    p = NULL;
    c = 0;
    }
  cNullTerminate(char *s) {
    Set(s);
    }
  ~cNullTerminate() {
    if (p)
       *p = c;
    }
  void Set(char *s) {
    if (s) {
       p = s;
       c = *s;
       *s = 0;
       }
    else
       p = NULL;
    }
  };

ssize_t safe_read(int filedes, void *buffer, size_t size);
ssize_t safe_write(int filedes, const void *buffer, size_t size);
void writechar(int filedes, char c);
int WriteAllOrNothing(int fd, const uchar *Data, int Length, int TimeoutMs = 0, int RetryMs = 0);
    ///< Writes either all Data to the given file descriptor, or nothing at all.
    ///< If TimeoutMs is greater than 0, it will only retry for that long, otherwise
    ///< it will retry forever. RetryMs defines the time between two retries.
char *strcpyrealloc(char *dest, const char *src);
char *strn0cpy(char *dest, const char *src, size_t n);
char *strreplace(char *s, char c1, char c2);
char *strreplace(char *s, const char *s1, const char *s2); ///< re-allocates 's' and deletes the original string if necessary!
const char *strchrn(const char *s, char c, size_t n); ///< returns a pointer to the n'th occurrence (counting from 1) of c in s, or NULL if no such character was found. If n is 0, s is returned.
int strcountchr(const char *s, char c); ///< returns the number of occurrences of 'c' in 's'.
cString strgetbefore(const char *s, char c, int n = 1); // returns the part of 's' before (and excluding) the n'th occurrence of 'c' from the right, or an empty string if there is no such 'c'.
const char *strgetlast(const char *s, char c); // returns the part of 's' after the last occurrence of 'c', or 's' if there is no 'c'.
inline char *strgetlast(char *s, char c) { return const_cast<char *>(strgetlast(static_cast<const char *>(s), c)); } // returns the part of 's' after the last occurrence of 'c', or 's' if there is no 'c'.
inline char *skipspace(const char *s)
{
  if ((uchar)*s > ' ') // most strings don't have any leading space, so handle this case as fast as possible
     return (char *)s;
  while (*s && (uchar)*s <= ' ') // avoiding isspace() here, because it is much slower
        s++;
  return (char *)s;
}
char *stripspace(char *s);
char *compactspace(char *s);
char *compactchars(char *s, char c); ///< removes all occurrences of 'c' from the beginning an end of 's' and replaces sequences of multiple 'c's with a single 'c'.
cString strescape(const char *s, const char *chars);
cString strgetval(const char *s, const char *name, char d = '=');
    ///< Returns the value part of a 'name=value' pair in s.
    ///< name must either be at the beginning of s, or has to be preceded by white space.
    ///< There may be any number of white space around the '=' sign. The value is
    ///< everything up to (and excluding) the next white space, or the end of s.
    ///< If an other delimiter shall be used (like, e.g., ':'), it can be given
    ///< as the third parameter.
    ///< If name occurs more than once in s, only the first occurrence is taken.
char *strshift(char *s, int n);
    ///< Shifts the given string to the left by the given number of bytes, thus
    ///< removing the first n bytes from s.
    ///< If n is greater than the length of s, the resulting string will be empty.
    ///< If n is <= 0 s will be unchanged.
    ///< Returns s.
bool startswith(const char *s, const char *p);
bool endswith(const char *s, const char *p);
bool isempty(const char *s);
int numdigits(int n);
bool isnumber(const char *s);
int64_t StrToNum(const char *s);
    ///< Converts the given string to a number.
    ///< The numerical part of the string may be followed by one of the letters
    ///< K, M, G or T to abbreviate Kilo-, Mega-, Giga- or Terabyte, respectively
    ///< (based on 1024). Everything after the first non-numeric character is
    ///< silently ignored, as are any characters other than the ones mentioned here.
bool StrInArray(const char *a[], const char *s);
    ///< Returns true if the string s is equal to one of the strings pointed
    ///< to by the (NULL terminated) array a.
double atod(const char *s);
    ///< Converts the given string, which is a floating point number using a '.' as
    ///< the decimal point, to a double value, independent of the currently selected
    ///< locale.
cString dtoa(double d, const char *Format = "%f");
    ///< Converts the given double value to a string, making sure it uses a '.' as
    ///< the decimal point, independent of the currently selected locale.
    ///< If Format is given, it will be used instead of the default.
cString itoa(int n);
inline uint16_t Peek13(const uchar *p)
{
  uint16_t v = uint16_t(*p++ & 0x1F) << 8;
  return v + (*p & 0xFF);
}
inline void Poke13(uchar *p, uint16_t v)
{
  v |= uint16_t(*p & ~0x1F) << 8;
  *p++ = v >> 8;
  *p = v & 0xFF;
}
cString AddDirectory(const char *DirName, const char *FileName);
bool EntriesOnSameFileSystem(const char *File1, const char *File2);
    ///< Checks whether the given files are on the same file system. If either of the
    ///< files doesn't exist, this function returns *true* to avoid any actions that might be
    ///< triggered if files are on different file system.
int FreeDiskSpaceMB(const char *Directory, int *UsedMB = NULL);
bool DirectoryOk(const char *DirName, bool LogErrors = false);
bool MakeDirs(const char *FileName, bool IsDirectory = false);
bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks = false);
bool RemoveEmptyDirectories(const char *DirName, bool RemoveThis = false, const char *IgnoreFiles[] = NULL);
     ///< Removes all empty directories under the given directory DirName.
     ///< If RemoveThis is true, DirName will also be removed if it is empty.
     ///< IgnoreFiles can be set to an array of file names that will be ignored when
     ///< considering whether a directory is empty. If IgnoreFiles is given, the array
     ///< must end with a NULL pointer.
int DirSizeMB(const char *DirName); ///< returns the total size of the files in the given directory, or -1 in case of an error
char *ReadLink(const char *FileName); ///< returns a new string allocated on the heap, which the caller must delete (or NULL in case of an error)
bool SpinUpDisk(const char *FileName);
void TouchFile(const char *FileName, bool Create = false);
time_t LastModifiedTime(const char *FileName);
off_t FileSize(const char *FileName); ///< returns the size of the given file, or -1 in case of an error (e.g. if the file doesn't exist)
cString WeekDayName(int WeekDay);
    ///< Converts the given WeekDay (0=Sunday, 1=Monday, ...) to a three letter
    ///< day name.
cString WeekDayName(time_t t);
    ///< Converts the week day of the given time to a three letter day name.
cString WeekDayNameFull(int WeekDay);
    ///< Converts the given WeekDay (0=Sunday, 1=Monday, ...) to a full
    ///< day name.
cString WeekDayNameFull(time_t t);
    ///< Converts the week day of the given time to a full day name.
cString DayDateTime(time_t t = 0);
    ///< Converts the given time to a string of the form "www dd.mm. hh:mm".
    ///< If no time is given, the current time is taken.
cString TimeToString(time_t t);
    ///< Converts the given time to a string of the form "www mmm dd hh:mm:ss yyyy".
cString DateString(time_t t);
    ///< Converts the given time to a string of the form "www dd.mm.yyyy".
cString ShortDateString(time_t t);
    ///< Converts the given time to a string of the form "dd.mm.yy".
cString TimeString(time_t t);
    ///< Converts the given time to a string of the form "hh:mm".
uchar *RgbToJpeg(uchar *Mem, int Width, int Height, int &Size, int Quality = 100);
    ///< Converts the given Memory to a JPEG image and returns a pointer
    ///< to the resulting image. Mem must point to a data block of exactly
    ///< (Width * Height) triplets of RGB image data bytes. Upon return, Size
    ///< will hold the number of bytes of the resulting JPEG data.
    ///< Quality can be in the range 0..100 and controls the quality of the
    ///< resulting image, where 100 is "best". The caller takes ownership of
    ///< the result and has to delete it once it is no longer needed.
    ///< The result may be NULL in case of an error.
const char *GetHostName(void);
    ///< Gets the host name of this machine.

class cBase64Encoder {
private:
  const uchar *data;
  int length;
  int maxResult;
  int i;
  char *result;
  static const char *b64;
public:
  cBase64Encoder(const uchar *Data, int Length, int MaxResult = 64);
      ///< Sets up a new base 64 encoder for the given Data, with the given Length.
      ///< Data will not be copied and must be valid as long as NextLine() will be
      ///< called. MaxResult defines the maximum number of characters in any
      ///< result line. The resulting lines may be shorter than MaxResult in case
      ///< its value is not a multiple of 4.
  ~cBase64Encoder();
  const char *NextLine(void);
      ///< Returns the next line of encoded data (terminated by '\0'), or NULL if
      ///< there is no more encoded data. The caller must call NextLine() and process
      ///< each returned line until NULL is returned, in order to get the entire
      ///< data encoded. The returned data is only valid until the next time NextLine()
      ///< is called, or until the object is destroyed.
  };

class cBitStream {
private:
  const uint8_t *data;
  int length; // in bits
  int index; // in bits
public:
  cBitStream(const uint8_t *Data, int Length) : data(Data), length(Length), index(0) {}
  ~cBitStream() {}
  int GetBit(void);
  uint32_t GetBits(int n);
  void ByteAlign(void);
  void WordAlign(void);
  bool SetLength(int Length);
  void SkipBits(int n) { index += n; }
  void SkipBit(void) { SkipBits(1); }
  bool IsEOF(void) const { return index >= length; }
  void Reset(void) { index = 0; }
  int Length(void) const { return length; }
  int Index(void) const { return (IsEOF() ? length : index); }
  const uint8_t *GetData(void) const { return (IsEOF() ? NULL : data + (index / 8)); }
  };

class cTimeMs {
private:
  uint64_t begin;
public:
  cTimeMs(int Ms = 0);
      ///< Creates a timer with ms resolution and an initial timeout of Ms.
      ///< If Ms is negative the timer is not initialized with the current
      ///< time.
  static uint64_t Now(void);
  void Set(int Ms = 0);
      ///< Sets the timer. If Ms is 0, call Elapsed() to get the number of milliseconds
      ///< since the timer has been set. If Ms is greater than 0, TimedOut() returns
      ///< true as soon as Ms milliseconds have passed since calling Set(). If Ms is
      ///< negative, results are undefined.
      ///< Depending on the value of Ms, an object of cTimeMs can handle either
      ///< timeouts or elapsed times, not both at the same time.
  bool TimedOut(void) const;
  uint64_t Elapsed(void) const;
  };

class cReadLine {
private:
  size_t size;
  char *buffer;
public:
  cReadLine(void);
  ~cReadLine();
  char *Read(FILE *f);
  };

class cPoller {
private:
  enum { MaxPollFiles = 64 };
  pollfd pfd[MaxPollFiles];
  int numFileHandles;
public:
  cPoller(int FileHandle = -1, bool Out = false);
  bool Add(int FileHandle, bool Out);
  void Del(int FileHandle, bool Out);
  bool Poll(int TimeoutMs = 0);
  };

class cReadDir {
private:
  DIR *directory;
  struct dirent *result;
#if !__GLIBC_PREREQ(2, 24) // readdir_r() is deprecated as of GLIBC 2.24
  union { // according to "The GNU C Library Reference Manual"
    struct dirent d;
    char b[offsetof(struct dirent, d_name) + NAME_MAX + 1];
    } u;
#endif
public:
  cReadDir(const char *Directory);
  ~cReadDir();
  bool Ok(void) { return directory != NULL; }
  struct dirent *Next(void);
  };

class cFile {
private:
  int f;
public:
  cFile(void);
  ~cFile();
  operator int () { return f; }
  bool Open(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  bool Open(int FileDes);
  void Close(void);
  bool IsOpen(void) { return f >= 0; }
  bool Ready(bool Wait = true);
  static bool FileReady(int FileDes, int TimeoutMs = 1000);
  };

class cSafeFile {
private:
  FILE *f;
  char *fileName;
  char *tempName;
public:
  cSafeFile(const char *FileName);
  ~cSafeFile();
  operator FILE* () { return f; }
  bool Open(void);
  bool Close(void);
  };

/// cUnbufferedFile is used for large files that are mainly written or read
/// in a streaming manner, and thus should not be cached.

class cUnbufferedFile {
private:
  int fd;
  off_t curpos;
  off_t cachedstart;
  off_t cachedend;
  off_t begin;
  off_t lastpos;
  off_t ahead;
  size_t readahead;
  size_t written;
  size_t totwritten;
  int FadviseDrop(off_t Offset, off_t Len);
public:
  cUnbufferedFile(void);
  ~cUnbufferedFile();
  int Open(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  int Close(void);
  void SetReadAhead(size_t ra);
  off_t Seek(off_t Offset, int Whence);
  ssize_t Read(void *Data, size_t Size);
  ssize_t Write(const void *Data, size_t Size);
  static cUnbufferedFile *Create(const char *FileName, int Flags, mode_t Mode = DEFFILEMODE);
  };

class cLockFile {
private:
  char *fileName;
  int f;
public:
  cLockFile(const char *Directory);
  ~cLockFile();
  bool Lock(int WaitSeconds = 0);
  void Unlock(void);
  };

class cListObject {
  friend class cListGarbageCollector;
private:
  cListObject *prev, *next;
  cListObject(const cListObject &ListObject) { abort(); } // no copy constructor!
  cListObject& operator= (const cListObject &ListObject) { abort(); return *this; } // no assignment operator!
public:
  cListObject(void);
  virtual ~cListObject();
  virtual int Compare(const cListObject &ListObject) const { return 0; }
      ///< Must return 0 if this object is equal to ListObject, a positive value
      ///< if it is "greater", and a negative value if it is "smaller".
  void Append(cListObject *Object);
  void Insert(cListObject *Object);
  void Unlink(void);
  int Index(void) const;
  cListObject *Prev(void) const { return prev; }
  cListObject *Next(void) const { return next; }
  };

class cListGarbageCollector {
private:
  cMutex mutex;
  cListObject *objects;
  time_t lastPut;
public:
  cListGarbageCollector(void);
  ~cListGarbageCollector();
  void Put(cListObject *Object);
  void Purge(bool Force = false);
  };

extern cListGarbageCollector ListGarbageCollector;

class cListBase {
protected:
  cListObject *objects, *lastObject;
  int count;
  mutable cStateLock stateLock;
  const char *needsLocking;
  bool useGarbageCollector;
  cListBase(const char *NeedsLocking = NULL);
public:
  virtual ~cListBase();
  bool Lock(cStateKey &StateKey, bool Write = false, int TimeoutMs = 0) const;
       ///< Tries to get a lock on this list and returns true if successful.
       ///< By default a read lock is requested. Set Write to true to obtain
       ///< a write lock. If TimeoutMs is not zero, it waits for the given
       ///< number of milliseconds before giving up.
       ///< If you need to lock more than one list at the same time, make sure
       ///< you set TimeoutMs to a suitable value in all of the calls to
       ///< Lock(), and be prepared to handle situations where you do not get all
       ///< of the requested locks. In such cases you should release all the locks
       ///< you have obtained so far and try again. StateKey.TimedOut() tells you
       ///< whether the lock attempt failed due to a timeout or because the state
       ///< of the lock hasn't changed since the previous locking attempt.
       ///< To implicitly avoid deadlocks when locking more than one of the global
       ///< lists of VDR at the same time, make sure you always lock Timers, Channels,
       ///< Recordings and Schedules in this sequence.
       ///< You may keep pointers to objects in this list, even after releasing
       ///< the lock. However, you may only access such objects if you are
       ///< holding a proper lock again. If an object has been deleted from the list
       ///< while you did not hold a lock (for instance by an other thread), the
       ///< object will still be there, but no longer within this list (it is then
       ///< stored in the ListGarbageCollector). That way even if you access the object
       ///< after it has been deleted, you won't cause a segfault. You can call the
       ///< Contains() function to check whether an object you are holding a pointer
       ///< to is still in the list. Note that the garbage collector is purged when
       ///< the usual housekeeping is done.
  void SetSyncStateKey(cStateKey &StateKey) { stateLock.SetSyncStateKey(StateKey); }
       ///< When making changes to this list (while holding a write lock) that shall
       ///< not affect some other code that reacts to such changes, this function can
       ///< be called with the StateKey used by that other code.
       ///< See cStateLock::SetSyncStateKey() for details.
  void SetUseGarbageCollector(void) { useGarbageCollector = true; }
  void SetExplicitModify(void);
       ///< If you have obtained a write lock on this list, and you don't want it to
       ///< be automatically marked as modified when the lock is released, a call to
       ///< this function will disable this, and you can explicitly call SetModified()
       ///< to have the list marked as modified.
  void SetModified(void);
       ///< Unconditionally marks this list as modified.
  void Add(cListObject *Object, cListObject *After = NULL);
  void Ins(cListObject *Object, cListObject *Before = NULL);
  void Del(cListObject *Object, bool DeleteObject = true);
  virtual void Move(int From, int To);
  void Move(cListObject *From, cListObject *To);
  virtual void Clear(void);
  bool Contains(const cListObject *Object) const;
       ///< If a pointer to an object contained in this list has been obtained while
       ///< holding a lock, and that lock has been released, but the pointer is kept for
       ///< later use (after obtaining a new lock), Contains() can be called with that
       ///< pointer to make sure the object it points to is still part of this list
       ///< (it may have been deleted or otherwise removed from the list after the lock
       ///< during which the pointer was initially retrieved has been released).
  const cListObject *Get(int Index) const;
  cListObject *Get(int Index) { return const_cast<cListObject *>(static_cast<const cListBase *>(this)->Get(Index)); }
  int Count(void) const { return count; }
  void Sort(void);
  };

template<class T> class cList : public cListBase {
public:
  cList(const char *NeedsLocking = NULL): cListBase(NeedsLocking) {}
      ///< Sets up a new cList of the given type T. If NeedsLocking is given, the list
      ///< and any of its elements may only be accessed if the caller holds a lock
      ///< obtained by a call to Lock() (see cListBase::Lock() for details).
      ///< NeedsLocking is used as both a boolean flag to enable locking, and as
      ///< a name to identify this list in debug output. It must be a static string
      ///< and should be no longer than 10 characters. The string will not be copied!
  const T *Get(int Index) const { return (T *)cListBase::Get(Index); }
      ///< Returns the list element at the given Index, or NULL if no such element
      ///< exists.
  const T *First(void) const { return (T *)objects; }
      ///< Returns the first element in this list, or NULL if the list is empty.
  const T *Last(void) const { return (T *)lastObject; }
      ///< Returns the last element in this list, or NULL if the list is empty.
  const T *Prev(const T *Object) const { return (T *)Object->cListObject::Prev(); } // need to call cListObject's members to
      ///< Returns the element immediately before Object in this list, or NULL
      ///< if Object is the first element in the list. Object must not be NULL!
  const T *Next(const T *Object) const { return (T *)Object->cListObject::Next(); } // avoid ambiguities in case of a "list of lists"
      ///< Returns the element immediately following Object in this list, or NULL
      ///< if Object is the last element in the list. Object must not be NULL!
  T *Get(int Index) { return const_cast<T *>(static_cast<const cList<T> *>(this)->Get(Index)); }
      ///< Non-const version of Get().
  T *First(void) { return const_cast<T *>(static_cast<const cList<T> *>(this)->First()); }
      ///< Non-const version of First().
  T *Last(void) { return const_cast<T *>(static_cast<const cList<T> *>(this)->Last()); }
      ///< Non-const version of Last().
  T *Prev(const T *Object) { return const_cast<T *>(static_cast<const cList<T> *>(this)->Prev(Object)); }
      ///< Non-const version of Prev().
  T *Next(const T *Object) { return const_cast<T *>(static_cast<const cList<T> *>(this)->Next(Object)); }
      ///< Non-const version of Next().
  };

// The DEF_LIST_LOCK macro defines a convenience class that can be used to obtain
// a lock on a cList and make sure the lock is released when the current scope
// is left:

#define DEF_LIST_LOCK2(Class, Name) \
class c##Name##_Lock { \
private: \
  cStateKey stateKey; \
  const c##Class *list; \
public: \
  c##Name##_Lock(bool Write = false) \
  { \
    if (Write) \
       list = c##Class::Get##Name##Write(stateKey); \
    else \
       list = c##Class::Get##Name##Read(stateKey); \
  } \
  ~c##Name##_Lock() { if (list) stateKey.Remove(); } \
  const c##Class *Name(void) const { return list; } \
  c##Class *Name(void) { return const_cast<c##Class *>(list); } \
  }
#define DEF_LIST_LOCK(Class) DEF_LIST_LOCK2(Class, Class)

// The USE_LIST_LOCK macro sets up a local variable of a class defined by
// a suitable DEF_LIST_LOCK, and also a pointer to the provided list:

#define USE_LIST_LOCK_READ2(Class, Name) \
c##Name##_Lock Name##_Lock(false); \
const c##Class *Name __attribute__((unused)) = Name##_Lock.Name();
#define USE_LIST_LOCK_READ(Class) USE_LIST_LOCK_READ2(Class, Class)

#define USE_LIST_LOCK_WRITE2(Class, Name) \
c##Name##_Lock Name##_Lock(true); \
c##Class *Name __attribute__((unused)) = Name##_Lock.Name();
#define USE_LIST_LOCK_WRITE(Class) USE_LIST_LOCK_WRITE2(Class, Class)

template<class T> class cVector {
  ///< cVector may only be used for *simple* types, like int or pointers - not for class objects that allocate additional memory!
private:
  mutable int allocated;
  mutable int size;
  mutable T *data;
  cVector(const cVector &Vector) {} // don't copy...
  cVector &operator=(const cVector &Vector) { return *this; } // ...or assign this!
  void Realloc(int Index) const
  {
    if (++Index > allocated) {
       data = (T *)realloc(data, Index * sizeof(T));
       if (!data) {
          esyslog("ERROR: out of memory - abort!");
          abort();
          }
       for (int i = allocated; i < Index; i++)
           data[i] = T(0);
       allocated = Index;
       }
  }
public:
  cVector(int Allocated = 10)
  {
    allocated = 0;
    size = 0;
    data = NULL;
    Realloc(Allocated);
  }
  virtual ~cVector() { free(data); }
  T& At(int Index) const
  {
    Realloc(Index);
    if (Index >= size)
       size = Index + 1;
    return data[Index];
  }
  const T& operator[](int Index) const
  {
    return At(Index);
  }
  T& operator[](int Index)
  {
    return At(Index);
  }
  int IndexOf(const T &Data) // returns the index of Data, or -1 if not found
  {
    for (int i = 0; i < size; i++) {
        if (data[i] == Data)
           return i;
        }
    return -1;
  }
  int Size(void) const { return size; }
  virtual void Insert(T Data, int Before = 0)
  {
    if (Before < size) {
       Realloc(size);
       memmove(&data[Before + 1], &data[Before], (size - Before) * sizeof(T));
       size++;
       data[Before] = Data;
       }
    else
       Append(Data);
  }
  bool InsertUnique(T Data, int Before = 0)
  {
    if (IndexOf(Data) < 0) {
       Insert(Data, Before);
       return true;
       }
    return false;
  }
  virtual void Append(T Data)
  {
    if (size >= allocated)
       Realloc(allocated * 3 / 2); // increase size by 50%
    data[size++] = Data;
  }
  bool AppendUnique(T Data)
  {
    if (IndexOf(Data) < 0) {
       Append(Data);
       return true;
       }
    return false;
  }
  virtual void Remove(int Index, int Count = 1)
  {
    if (Index < 0 || Index >= size || Count < 1)
       return; // prevents out-of-bounds access
    if (Index + Count > size)
       Count = size - Index;
    int Tail = size - (Index + Count);
    if (Tail > 0)
       memmove(&data[Index], &data[Index + Count], Tail * sizeof(T));
    size -= Count;
  }
  bool RemoveElement(const T &Data)
  {
    int i = IndexOf(Data);
    if (i >= 0) {
       Remove(i);
       return true;
       }
    return false;
  }
  virtual void Clear(void)
  {
    for (int i = 0; i < size; i++)
        data[i] = T(0);
    size = 0;
  }
  void Sort(__compar_fn_t Compare)
  {
    qsort(data, size, sizeof(T), Compare);
  }
  };

inline int CompareInts(const void *a, const void *b)
{
  return *(const int *)a - *(const int *)b;
}

inline int CompareStrings(const void *a, const void *b)
{
  return strcmp(*(const char **)a, *(const char **)b);
}

inline int CompareStringsIgnoreCase(const void *a, const void *b)
{
  return strcasecmp(*(const char **)a, *(const char **)b);
}

inline int CompareStringsNumerically(const void *a, const void *b)
{
  int d = atoi(*(const char **)a) - atoi(*(const char **)b);
  return d ? d : CompareStrings(a, b);
}

class cStringList : public cVector<char *> {
public:
  cStringList(int Allocated = 10): cVector<char *>(Allocated) {}
  virtual ~cStringList() override;
  int Find(const char *s) const;
  void Sort(bool IgnoreCase = false)
  {
    if (IgnoreCase)
       cVector<char *>::Sort(CompareStringsIgnoreCase);
    else
       cVector<char *>::Sort(CompareStrings);
  }
  void SortNumerically(void)
  {
    cVector<char *>::Sort(CompareStringsNumerically);
  }
  virtual void Clear(void) override;
  };

class cFileNameList : public cStringList {
public:
  cFileNameList(const char *Directory = NULL, bool DirsOnly = false);
  bool Load(const char *Directory, bool DirsOnly = false);
  };

class cDynamicBuffer {
private:
  uchar *buffer;
  int initialSize;
  int size; // the total size of the buffer (bytes in memory)
  int used; // the number of used bytes, starting at the beginning of the buffer
  bool Realloc(int NewSize);
  bool Assert(int NewSize) { return size < NewSize ? Realloc(NewSize) : true; } // inline for performance!
public:
  cDynamicBuffer(int InitialSize = 1024);
  ~cDynamicBuffer();
  void Append(const uchar *Data, int Length);
  void Append(uchar Data) { if (Assert(used + 1)) buffer[used++] = Data; }
  void Set(int Index, uchar Data) { if (Assert(Index + 1)) buffer[Index] = Data; }
  uchar Get(int Index) { return Index < used ? buffer[Index] : 0; }
  void Clear(void) { used = 0; }
  uchar *Data(void) { return buffer; }
  int Length(void) { return used; }
  };

class cHashObject : public cListObject {
  friend class cHashBase;
private:
  unsigned int id;
  cListObject *object;
public:
  cHashObject(cListObject *Object, unsigned int Id) { object = Object; id = Id; }
  cListObject *Object(void) { return object; }
  };

class cHashBase {
private:
  cList<cHashObject> **hashTable;
  int size;
  bool ownObjects;
  unsigned int hashfn(unsigned int Id) const { return Id % size; }
protected:
  cHashBase(int Size, bool OwnObjects);
       ///< Creates a new hash of the given Size. If OwnObjects is true, the
       ///< hash takes ownership of the objects given in the calls to Add(),
       ///< and deletes them when Clear() is called or the hash is destroyed
       ///< (unless the object has been removed from the hash by calling Del()).
public:
  virtual ~cHashBase();
  void Add(cListObject *Object, unsigned int Id);
  void Del(cListObject *Object, unsigned int Id);
  void Clear(void);
  cListObject *Get(unsigned int Id) const;
  cList<cHashObject> *GetList(unsigned int Id) const;
  };

#define HASHSIZE 512

template<class T> class cHash : public cHashBase {
public:
  cHash(int Size = HASHSIZE, bool OwnObjects = false) : cHashBase(Size, OwnObjects) {}
  T *Get(unsigned int Id) const { return (T *)cHashBase::Get(Id); }
};

#endif //__TOOLS_H
