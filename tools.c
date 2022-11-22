/*
 * tools.c: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 5.7 2022/11/22 14:33:48 kls Exp $
 */

#include "tools.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
extern "C" {
#ifdef boolean
#define HAVE_BOOLEAN
#endif
#include <jpeglib.h>
#undef boolean
}
#include <locale.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "i18n.h"
#include "thread.h"

int SysLogLevel = 3;

#define MAXSYSLOGBUF 256

void syslog_with_tid(int priority, const char *format, ...)
{
  va_list ap;
  char fmt[MAXSYSLOGBUF];
  snprintf(fmt, sizeof(fmt), "[%d] %s", cThread::ThreadId(), format);
  va_start(ap, format);
  vsyslog(priority, fmt, ap);
  va_end(ap);
}

int BCD2INT(int x)
{
  return ((1000000 * BCDCHARTOINT((x >> 24) & 0xFF)) +
            (10000 * BCDCHARTOINT((x >> 16) & 0xFF)) +
              (100 * BCDCHARTOINT((x >>  8) & 0xFF)) +
                     BCDCHARTOINT( x        & 0xFF));
}

ssize_t safe_read(int filedes, void *buffer, size_t size)
{
  for (;;) {
      ssize_t p = read(filedes, buffer, size);
      if (p < 0 && errno == EINTR) {
         dsyslog("EINTR while reading from file handle %d - retrying", filedes);
         continue;
         }
      return p;
      }
}

ssize_t safe_write(int filedes, const void *buffer, size_t size)
{
  ssize_t p = 0;
  ssize_t written = size;
  const unsigned char *ptr = (const unsigned char *)buffer;
  while (size > 0) {
        p = write(filedes, ptr, size);
        if (p < 0) {
           if (errno == EINTR) {
              dsyslog("EINTR while writing to file handle %d - retrying", filedes);
              continue;
              }
           break;
           }
        ptr  += p;
        size -= p;
        }
  return p < 0 ? p : written;
}

void writechar(int filedes, char c)
{
  safe_write(filedes, &c, sizeof(c));
}

int WriteAllOrNothing(int fd, const uchar *Data, int Length, int TimeoutMs, int RetryMs)
{
  int written = 0;
  while (Length > 0) {
        int w = write(fd, Data + written, Length);
        if (w > 0) {
           Length -= w;
           written += w;
           }
        else if (written > 0 && !FATALERRNO) {
           // we've started writing, so we must finish it!
           cTimeMs t;
           cPoller Poller(fd, true);
           Poller.Poll(RetryMs);
           if (TimeoutMs > 0 && (TimeoutMs -= t.Elapsed()) <= 0)
              break;
           }
        else
           // nothing written yet (or fatal error), so we can just return the error code:
           return w;
        }
  return written;
}

char *strcpyrealloc(char *dest, const char *src)
{
  if (src) {
     int l = max(dest ? strlen(dest) : 0, strlen(src)) + 1; // don't let the block get smaller!
     dest = (char *)realloc(dest, l);
     if (dest)
        strcpy(dest, src);
     else
        esyslog("ERROR: out of memory");
     }
  else {
     free(dest);
     dest = NULL;
     }
  return dest;
}

char *strn0cpy(char *dest, const char *src, size_t n)
{
  char *s = dest;
  for ( ; --n && (*dest = *src) != 0; dest++, src++) ;
  *dest = 0;
  return s;
}

char *strreplace(char *s, char c1, char c2)
{
  if (s) {
     char *p = s;
     while (*p) {
           if (*p == c1)
              *p = c2;
           p++;
           }
     }
  return s;
}

char *strreplace(char *s, const char *s1, const char *s2)
{
  if (!s || !s1 || !s2)
     return s;
  char *p = strstr(s, s1);
  if (p) {
     int of = p - s;
     int l  = strlen(s);
     int l1 = strlen(s1);
     int l2 = strlen(s2);
     if (l2 > l1) {
        if (char *NewBuffer = (char *)realloc(s, l + l2 - l1 + 1))
           s = NewBuffer;
        else {
           esyslog("ERROR: out of memory");
           return s;
           }
        }
     char *sof = s + of;
     if (l2 != l1)
        memmove(sof + l2, sof + l1, l - of - l1 + 1);
     memcpy(sof, s2, l2);
     }
  return s;
}

const char *strchrn(const char *s, char c, size_t n)
{
  if (n == 0)
     return s;
  if (s) {
     for ( ; *s; s++) {
         if (*s == c && --n == 0)
            return s;
         }
     }
  return NULL;
}

int strcountchr(const char *s, char c)
{
  int n = 0;
  if (s && c) {
     for ( ; *s; s++) {
         if (*s == c)
            n++;
         }
     }
  return n;
}

cString strgetbefore(const char *s, char c, int n)
{
  const char *p = strrchr(s, 0); // points to the terminating 0 of s
  while (--p >= s) {
        if (*p == c && --n == 0)
           break;
        }
  return cString(s, p);
}

const char *strgetlast(const char *s, char c)
{
  const char *p = strrchr(s, c);
  return p ? p + 1 : s;
}

char *stripspace(char *s)
{
  if (s && *s) {
     for (char *p = s + strlen(s) - 1; p >= s; p--) {
         if (!isspace(*p))
            break;
         *p = 0;
         }
     }
  return s;
}

char *compactspace(char *s)
{
  if (s && *s) {
     char *t = stripspace(skipspace(s));
     char *p = t;
     while (p && *p) {
           char *q = skipspace(p);
           if (q - p > 1)
              memmove(p + 1, q, strlen(q) + 1);
           p++;
           }
     if (t != s)
        memmove(s, t, strlen(t) + 1);
     }
  return s;
}

char *compactchars(char *s, char c)
{
  if (s && *s && c) {
     char *t = s;
     char *p = s;
     int n = 0;
     while (*p) {
           if (*p != c) {
              *t++ = *p;
              n = 0;
              }
           else if (t != s && n == 0) {
              *t++ = *p;
              n++;
              }
           p++;
           }
     if (n)
        t--; // the last character was c
     *t = 0;
     }
  return s;
}

cString strescape(const char *s, const char *chars)
{
  char *buffer;
  const char *p = s;
  char *t = NULL;
  while (*p) {
        if (strchr(chars, *p)) {
           if (!t) {
              buffer = MALLOC(char, 2 * strlen(s) + 1);
              t = buffer + (p - s);
              s = strcpy(buffer, s);
              }
           *t++ = '\\';
           }
        if (t)
           *t++ = *p;
        p++;
        }
  if (t)
     *t = 0;
  return cString(s, t != NULL);
}

cString strgetval(const char *s, const char *name, char d)
{
  if (s && name) {
     int l = strlen(name);
     const char *t = s;
     while (const char *p = strstr(t, name)) {
           t = skipspace(p + l);
           if (p == s || *(p - 1) <= ' ') {
              if (*t == d) {
                 t = skipspace(t + 1);
                 const char *v = t;
                 while (*t > ' ')
                       t++;
                 return cString(v, t);
                 break;
                 }
              }
           }
     }
  return NULL;
}

char *strshift(char *s, int n)
{
  if (s && n > 0) {
     int l = strlen(s);
     if (n < l)
        memmove(s, s + n, l - n + 1); // we also copy the terminating 0!
     else
        *s = 0;
     }
  return s;
}

bool startswith(const char *s, const char *p)
{
  while (*p) {
        if (*p++ != *s++)
           return false;
        }
  return true;
}

bool endswith(const char *s, const char *p)
{
  const char *se = s + strlen(s) - 1;
  const char *pe = p + strlen(p) - 1;
  while (pe >= p) {
        if (*pe-- != *se-- || (se < s && pe >= p))
           return false;
        }
  return true;
}

bool isempty(const char *s)
{
  return !(s && *skipspace(s));
}

int numdigits(int n)
{
  int res = 1;
  while (n >= 10) {
        n /= 10;
        res++;
        }
  return res;
}

bool isnumber(const char *s)
{
  if (!s || !*s)
     return false;
  do {
     if (!isdigit(*s))
        return false;
     } while (*++s);
  return true;
}

int64_t StrToNum(const char *s)
{
  char *t = NULL;
  int64_t n = strtoll(s, &t, 10);
  if (t) {
     switch (*t) {
       case 'T': n *= 1024;
       case 'G': n *= 1024;
       case 'M': n *= 1024;
       case 'K': n *= 1024;
       }
     }
  return n;
}

bool StrInArray(const char *a[], const char *s)
{
  if (a) {
     while (*a) {
           if (strcmp(*a, s) == 0)
              return true;
           a++;
           }
     }
  return false;
}

cString AddDirectory(const char *DirName, const char *FileName)
{
  if (*FileName == '/')
     FileName++;
  return cString::sprintf("%s/%s", DirName && *DirName ? DirName : ".", FileName);
}

#define DECIMAL_POINT_C '.'

double atod(const char *s)
{
  static lconv *loc = localeconv();
  if (*loc->decimal_point != DECIMAL_POINT_C) {
     char buf[strlen(s) + 1];
     char *p = buf;
     while (*s) {
           if (*s == DECIMAL_POINT_C)
              *p = *loc->decimal_point;
           else
              *p = *s;
           p++;
           s++;
           }
     *p = 0;
     return atof(buf);
     }
  else
     return atof(s);
}

cString dtoa(double d, const char *Format)
{
  static lconv *loc = localeconv();
  char buf[16];
  snprintf(buf, sizeof(buf), Format, d);
  if (*loc->decimal_point != DECIMAL_POINT_C)
     strreplace(buf, *loc->decimal_point, DECIMAL_POINT_C);
  return buf;
}

cString itoa(int n)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", n);
  return buf;
}

bool EntriesOnSameFileSystem(const char *File1, const char *File2)
{
  struct stat st;
  if (stat(File1, &st) == 0) {
     dev_t dev1 = st.st_dev;
     if (stat(File2, &st) == 0)
        return st.st_dev == dev1;
     else
        LOG_ERROR_STR(File2);
     }
  else
     LOG_ERROR_STR(File1);
  return true; // we only return false if both files actually exist and are in different file systems!
}

int FreeDiskSpaceMB(const char *Directory, int *UsedMB)
{
  if (UsedMB)
     *UsedMB = 0;
  int Free = 0;
  struct statfs statFs;
  if (statfs(Directory, &statFs) == 0) {
     double blocksPerMeg = 1024.0 * 1024.0 / statFs.f_bsize;
     if (UsedMB)
        *UsedMB = int((statFs.f_blocks - statFs.f_bfree) / blocksPerMeg);
     Free = int(statFs.f_bavail / blocksPerMeg);
     }
  else
     LOG_ERROR_STR(Directory);
  return Free;
}

bool DirectoryOk(const char *DirName, bool LogErrors)
{
  struct stat ds;
  if (stat(DirName, &ds) == 0) {
     if (S_ISDIR(ds.st_mode)) {
        if (access(DirName, R_OK | W_OK | X_OK) == 0)
           return true;
        else if (LogErrors)
           esyslog("ERROR: can't access %s", DirName);
        }
     else if (LogErrors)
        esyslog("ERROR: %s is not a directory", DirName);
     }
  else if (LogErrors)
     LOG_ERROR_STR(DirName);
  return false;
}

bool MakeDirs(const char *FileName, bool IsDirectory)
{
  bool result = true;
  char *s = strdup(FileName);
  char *p = s;
  if (*p == '/')
     p++;
  while ((p = strchr(p, '/')) != NULL || IsDirectory) {
        if (p)
           *p = 0;
        struct stat fs;
        if (stat(s, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
           dsyslog("creating directory %s", s);
           if (mkdir(s, ACCESSPERMS) == -1) {
              LOG_ERROR_STR(s);
              result = false;
              break;
              }
           }
        if (p)
           *p++ = '/';
        else
           break;
        }
  free(s);
  return result;
}

bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks)
{
  struct stat st;
  if (stat(FileName, &st) == 0) {
     if (S_ISDIR(st.st_mode)) {
        cReadDir d(FileName);
        if (d.Ok()) {
           struct dirent *e;
           while ((e = d.Next()) != NULL) {
                 cString buffer = AddDirectory(FileName, e->d_name);
                 if (FollowSymlinks) {
                    struct stat st2;
                    if (lstat(buffer, &st2) == 0) {
                       if (S_ISLNK(st2.st_mode)) {
                          int size = st2.st_size + 1;
                          char *l = MALLOC(char, size);
                          int n = readlink(buffer, l, size - 1);
                          if (n < 0) {
                             if (errno != EINVAL)
                                LOG_ERROR_STR(*buffer);
                             }
                          else {
                             l[n] = 0;
                             dsyslog("removing %s", l);
                             if (remove(l) < 0)
                                LOG_ERROR_STR(l);
                             }
                          free(l);
                          }
                       }
                    else if (errno != ENOENT) {
                       LOG_ERROR_STR(FileName);
                       return false;
                       }
                    }
                 dsyslog("removing %s", *buffer);
                 if (remove(buffer) < 0)
                    LOG_ERROR_STR(*buffer);
                 }
           }
        else {
           LOG_ERROR_STR(FileName);
           return false;
           }
        }
     dsyslog("removing %s", FileName);
     if (remove(FileName) < 0) {
        LOG_ERROR_STR(FileName);
        return false;
        }
     }
  else if (errno != ENOENT) {
     LOG_ERROR_STR(FileName);
     return false;
     }
  return true;
}

bool RemoveEmptyDirectories(const char *DirName, bool RemoveThis, const char *IgnoreFiles[])
{
  bool HasIgnoredFiles = false;
  cReadDir d(DirName);
  if (d.Ok()) {
     bool empty = true;
     struct dirent *e;
     while ((e = d.Next()) != NULL) {
           if (strcmp(e->d_name, "lost+found")) {
              cString buffer = AddDirectory(DirName, e->d_name);
              struct stat st;
              if (stat(buffer, &st) == 0) {
                 if (S_ISDIR(st.st_mode)) {
                    if (!RemoveEmptyDirectories(buffer, true, IgnoreFiles))
                       empty = false;
                    }
                 else if (RemoveThis && IgnoreFiles && StrInArray(IgnoreFiles, e->d_name))
                    HasIgnoredFiles = true;
                 else
                    empty = false;
                 }
              else {
                 LOG_ERROR_STR(*buffer);
                 empty = false;
                 }
              }
           }
     if (RemoveThis && empty) {
        if (HasIgnoredFiles) {
           while (*IgnoreFiles) {
                 cString buffer = AddDirectory(DirName, *IgnoreFiles);
                 if (access(buffer, F_OK) == 0) {
                    dsyslog("removing %s", *buffer);
                    if (remove(buffer) < 0) {
                       LOG_ERROR_STR(*buffer);
                       return false;
                       }
                    }
                 IgnoreFiles++;
                 }
           }
        dsyslog("removing %s", DirName);
        if (remove(DirName) < 0) {
           LOG_ERROR_STR(DirName);
           return false;
           }
        }
     return empty;
     }
  else
     LOG_ERROR_STR(DirName);
  return false;
}

int DirSizeMB(const char *DirName)
{
  cReadDir d(DirName);
  if (d.Ok()) {
     int size = 0;
     struct dirent *e;
     while (size >= 0 && (e = d.Next()) != NULL) {
           cString buffer = AddDirectory(DirName, e->d_name);
           struct stat st;
           if (stat(buffer, &st) == 0) {
              if (S_ISDIR(st.st_mode)) {
                 int n = DirSizeMB(buffer);
                 if (n >= 0)
                    size += n;
                 else
                    size = -1;
                 }
              else
                 size += st.st_size / MEGABYTE(1);
              }
           else {
              LOG_ERROR_STR(*buffer);
              size = -1;
              }
           }
     return size;
     }
  else if (errno != ENOENT)
     LOG_ERROR_STR(DirName);
  return -1;
}

char *ReadLink(const char *FileName)
{
  if (!FileName)
     return NULL;
  char *TargetName = canonicalize_file_name(FileName);
  if (!TargetName) {
     if (errno == ENOENT) // file doesn't exist
        TargetName = strdup(FileName);
     else // some other error occurred
        LOG_ERROR_STR(FileName);
     }
  return TargetName;
}

bool SpinUpDisk(const char *FileName)
{
  for (int n = 0; n < 10; n++) {
      cString buf;
      if (DirectoryOk(FileName))
         buf = cString::sprintf("%s/vdr-%06d", *FileName ? FileName : ".", n);
      else
         buf = cString::sprintf("%s.vdr-%06d", FileName, n);
      if (access(buf, F_OK) != 0) { // the file does not exist
         timeval tp1, tp2;
         gettimeofday(&tp1, NULL);
         int f = open(buf, O_WRONLY | O_CREAT, DEFFILEMODE);
         // O_SYNC doesn't work on all file systems
         if (f >= 0) {
            if (fdatasync(f) < 0)
               LOG_ERROR_STR(*buf);
            close(f);
            remove(buf);
            gettimeofday(&tp2, NULL);
            double seconds = (((long long)tp2.tv_sec * 1000000 + tp2.tv_usec) - ((long long)tp1.tv_sec * 1000000 + tp1.tv_usec)) / 1000000.0;
            if (seconds > 0.5)
               dsyslog("SpinUpDisk took %.2f seconds", seconds);
            return true;
            }
         else
            LOG_ERROR_STR(*buf);
         }
      }
  esyslog("ERROR: SpinUpDisk failed");
  return false;
}

void TouchFile(const char *FileName)
{
  if (utime(FileName, NULL) == -1 && errno != ENOENT)
     LOG_ERROR_STR(FileName);
}

time_t LastModifiedTime(const char *FileName)
{
  struct stat fs;
  if (stat(FileName, &fs) == 0)
     return fs.st_mtime;
  return 0;
}

off_t FileSize(const char *FileName)
{
  struct stat fs;
  if (stat(FileName, &fs) == 0)
     return fs.st_size;
  return -1;
}

// --- cTimeMs ---------------------------------------------------------------

cTimeMs::cTimeMs(int Ms)
{
  if (Ms >= 0)
     Set(Ms);
  else
     begin = 0;
}

uint64_t cTimeMs::Now(void)
{
#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
#define MIN_RESOLUTION 5 // ms
  static bool initialized = false;
  static bool monotonic = false;
  struct timespec tp;
  if (!initialized) {
     // check if monotonic timer is available and provides enough accurate resolution:
     if (clock_getres(CLOCK_MONOTONIC, &tp) == 0) {
        long Resolution = tp.tv_nsec;
        // require a minimum resolution:
        if (tp.tv_sec == 0 && tp.tv_nsec <= MIN_RESOLUTION * 1000000) {
           if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0) {
              dsyslog("cTimeMs: using monotonic clock (resolution is %ld ns)", Resolution);
              monotonic = true;
              }
           else
              esyslog("cTimeMs: clock_gettime(CLOCK_MONOTONIC) failed");
           }
        else
           dsyslog("cTimeMs: not using monotonic clock - resolution is too bad (%jd s %ld ns)", intmax_t(tp.tv_sec), tp.tv_nsec);
        }
     else
        esyslog("cTimeMs: clock_getres(CLOCK_MONOTONIC) failed");
     initialized = true;
     }
  if (monotonic) {
     if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
        return (uint64_t(tp.tv_sec)) * 1000 + tp.tv_nsec / 1000000;
     esyslog("cTimeMs: clock_gettime(CLOCK_MONOTONIC) failed");
     monotonic = false;
     // fall back to gettimeofday()
     }
#else
#  warning Posix monotonic clock not available
#endif
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return (uint64_t(t.tv_sec)) * 1000 + t.tv_usec / 1000;
  return 0;
}

void cTimeMs::Set(int Ms)
{
  begin = Now() + Ms;
}

bool cTimeMs::TimedOut(void) const
{
  return Now() >= begin;
}

uint64_t cTimeMs::Elapsed(void) const
{
  return Now() - begin;
}

// --- UTF-8 support ---------------------------------------------------------

static uint SystemToUtf8[128] = { 0 };

int Utf8CharLen(const char *s)
{
  if (cCharSetConv::SystemCharacterTable())
     return 1;
#define MT(s, m, v) ((*(s) & (m)) == (v)) // Mask Test
  if (MT(s, 0xE0, 0xC0) && MT(s + 1, 0xC0, 0x80))
     return 2;
  if (MT(s, 0xF0, 0xE0) && MT(s + 1, 0xC0, 0x80) && MT(s + 2, 0xC0, 0x80))
     return 3;
  if (MT(s, 0xF8, 0xF0) && MT(s + 1, 0xC0, 0x80) && MT(s + 2, 0xC0, 0x80) && MT(s + 3, 0xC0, 0x80))
     return 4;
  return 1;
}

uint Utf8CharGet(const char *s, int Length)
{
  if (cCharSetConv::SystemCharacterTable())
     return (uchar)*s < 128 ? *s : SystemToUtf8[(uchar)*s - 128];
  if (!Length)
     Length = Utf8CharLen(s);
  switch (Length) {
    case 2: return ((*s & 0x1F) <<  6) |  (*(s + 1) & 0x3F);
    case 3: return ((*s & 0x0F) << 12) | ((*(s + 1) & 0x3F) <<  6) |  (*(s + 2) & 0x3F);
    case 4: return ((*s & 0x07) << 18) | ((*(s + 1) & 0x3F) << 12) | ((*(s + 2) & 0x3F) << 6) | (*(s + 3) & 0x3F);
    default: ;
    }
  return *s;
}

int Utf8CharSet(uint c, char *s)
{
  if (c < 0x80 || cCharSetConv::SystemCharacterTable()) {
     if (s)
        *s = c;
     return 1;
     }
  if (c < 0x800) {
     if (s) {
        *s++ = ((c >> 6) & 0x1F) | 0xC0;
        *s   = (c & 0x3F) | 0x80;
        }
     return 2;
     }
  if (c < 0x10000) {
     if (s) {
        *s++ = ((c >> 12) & 0x0F) | 0xE0;
        *s++ = ((c >>  6) & 0x3F) | 0x80;
        *s   = (c & 0x3F) | 0x80;
        }
     return 3;
     }
  if (c < 0x110000) {
     if (s) {
        *s++ = ((c >> 18) & 0x07) | 0xF0;
        *s++ = ((c >> 12) & 0x3F) | 0x80;
        *s++ = ((c >>  6) & 0x3F) | 0x80;
        *s   = (c & 0x3F) | 0x80;
        }
     return 4;
     }
  return 0; // can't convert to UTF-8
}

int Utf8SymChars(const char *s, int Symbols)
{
  if (cCharSetConv::SystemCharacterTable())
     return Symbols;
  int n = 0;
  while (*s && Symbols--) {
        int sl = Utf8CharLen(s);
        s += sl;
        n += sl;
        }
  return n;
}

int Utf8StrLen(const char *s)
{
  if (cCharSetConv::SystemCharacterTable())
     return strlen(s);
  int n = 0;
  while (*s) {
        s += Utf8CharLen(s);
        n++;
        }
  return n;
}

char *Utf8Strn0Cpy(char *Dest, const char *Src, int n)
{
  if (cCharSetConv::SystemCharacterTable())
     return strn0cpy(Dest, Src, n);
  char *d = Dest;
  while (*Src) {
        int sl = Utf8CharLen(Src);
        n -= sl;
        if (n > 0) {
           while (sl--)
                 *d++ = *Src++;
           }
        else
           break;
        }
  *d = 0;
  return Dest;
}

int Utf8ToArray(const char *s, uint *a, int Size)
{
  int n = 0;
  while (*s && --Size > 0) {
        if (cCharSetConv::SystemCharacterTable())
           *a++ = (uchar)(*s++);
        else {
           int sl = Utf8CharLen(s);
           *a++ = Utf8CharGet(s, sl);
           s += sl;
           }
        n++;
        }
  if (Size > 0)
     *a = 0;
  return n;
}

int Utf8FromArray(const uint *a, char *s, int Size, int Max)
{
  int NumChars = 0;
  int NumSyms = 0;
  while (*a && NumChars < Size) {
        if (Max >= 0 && NumSyms++ >= Max)
           break;
        if (cCharSetConv::SystemCharacterTable()) {
           *s++ = *a++;
           NumChars++;
           }
        else {
           int sl = Utf8CharSet(*a);
           if (NumChars + sl <= Size) {
              Utf8CharSet(*a, s);
              a++;
              s += sl;
              NumChars += sl;
              }
           else
              break;
           }
        }
  if (NumChars < Size)
     *s = 0;
  return NumChars;
}

// --- cCharSetConv ----------------------------------------------------------

char *cCharSetConv::systemCharacterTable = NULL;

cCharSetConv::cCharSetConv(const char *FromCode, const char *ToCode)
{
  if (!FromCode)
     FromCode = systemCharacterTable ? systemCharacterTable : "UTF-8";
  if (!ToCode)
     ToCode = "UTF-8";
  cd = iconv_open(ToCode, FromCode);
  result = NULL;
  length = 0;
}

cCharSetConv::~cCharSetConv()
{
  free(result);
  if (cd != (iconv_t)-1)
     iconv_close(cd);
}

void cCharSetConv::SetSystemCharacterTable(const char *CharacterTable)
{
  free(systemCharacterTable);
  systemCharacterTable = NULL;
  if (!strcasestr(CharacterTable, "UTF-8")) {
     // Set up a map for the character values 128...255:
     char buf[129];
     for (int i = 0; i < 128; i++)
         buf[i] = i + 128;
     buf[128] = 0;
     cCharSetConv csc(CharacterTable);
     const char *s = csc.Convert(buf);
     int i = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           SystemToUtf8[i] = Utf8CharGet(s, sl);
           s += sl;
           i++;
           }
     systemCharacterTable = strdup(CharacterTable);
     }
}

const char *cCharSetConv::Convert(const char *From, char *To, size_t ToLength)
{
  if (cd != (iconv_t)-1 && From && *From) {
     char *FromPtr = (char *)From;
     size_t FromLength = strlen(From);
     char *ToPtr = To;
     if (!ToPtr) {
        int NewLength = max(length, FromLength * 2); // some reserve to avoid later reallocations
        if (char *NewBuffer = (char *)realloc(result, NewLength)) {
           length = NewLength;
           result = NewBuffer;
           }
        else {
           esyslog("ERROR: out of memory");
           return From;
           }
        ToPtr = result;
        ToLength = length;
        }
     else if (!ToLength)
        return From; // can't convert into a zero sized buffer
     ToLength--; // save space for terminating 0
     char *Converted = ToPtr;
     while (FromLength > 0) {
           if (iconv(cd, &FromPtr, &FromLength, &ToPtr, &ToLength) == size_t(-1)) {
              if (errno == E2BIG || errno == EILSEQ && ToLength < 1) {
                 if (To)
                    break; // caller provided a fixed size buffer, but it was too small
                 // The result buffer is too small, so increase it:
                 size_t d = ToPtr - result;
                 size_t r = length / 2;
                 int NewLength = length + r;
                 if (char *NewBuffer = (char *)realloc(result, NewLength)) {
                    length = NewLength;
                    Converted = result = NewBuffer;
                    }
                 else {
                    esyslog("ERROR: out of memory");
                    return From;
                    }
                 ToLength += r;
                 ToPtr = result + d;
                 }
              if (errno == EILSEQ) {
                 // A character can't be converted, so mark it with '?' and proceed:
                 FromPtr++;
                 FromLength--;
                 *ToPtr++ = '?';
                 ToLength--;
                 }
              else if (errno != E2BIG)
                 return From; // unknown error, return original string
              }
           }
     *ToPtr = 0;
     return Converted;
     }
  return From;
}

// --- cString ---------------------------------------------------------------

cString::cString(const char *S, bool TakePointer)
{
  s = TakePointer ? (char *)S : S ? strdup(S) : NULL;
}

cString::cString(const char *S, const char *To)
{
  if (!S)
     s = NULL;
  else if (!To)
     s = strdup(S);
  else {
     int l = To - S;
     s = MALLOC(char, l + 1);
     strncpy(s, S, l);
     s[l] = 0;
     }
}

cString::cString(const cString &String)
{
  s = String.s ? strdup(String.s) : NULL;
}

cString::~cString()
{
  free(s);
}

cString &cString::operator=(const cString &String)
{
  if (this == &String)
     return *this;
  free(s);
  s = String.s ? strdup(String.s) : NULL;
  return *this;
}

cString &cString::operator=(const char *String)
{
  if (s == String)
    return *this;
  free(s);
  s = String ? strdup(String) : NULL;
  return *this;
}

cString &cString::Append(const char *String)
{
  if (String) {
     int l1 = s ? strlen(s) : 0;
     int l2 = strlen(String);
     if (char *p = (char *)realloc(s, l1 + l2 + 1)) {
        s = p;
        strcpy(s + l1, String);
        }
     else
        esyslog("ERROR: out of memory");
     }
  return *this;
}

cString &cString::Truncate(int Index)
{
  int l = strlen(s);
  if (Index < 0)
     Index = l + Index;
  if (Index >= 0 && Index < l)
     s[Index] = 0;
  return *this;
}

cString &cString::CompactChars(char c)
{
  compactchars(s, c);
  return *this;
}

cString cString::sprintf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char *buffer;
  if (!fmt || vasprintf(&buffer, fmt, ap) < 0) {
     esyslog("error in vasprintf('%s', ...)", fmt);
     buffer = strdup("???");
     }
  va_end(ap);
  return cString(buffer, true);
}

cString cString::vsprintf(const char *fmt, va_list &ap)
{
  char *buffer;
  if (!fmt || vasprintf(&buffer, fmt, ap) < 0) {
     esyslog("error in vasprintf('%s', ...)", fmt);
     buffer = strdup("???");
     }
  return cString(buffer, true);
}

cString WeekDayName(int WeekDay)
{
  char buffer[16];
  WeekDay = WeekDay == 0 ? 6 : WeekDay - 1; // we start with Monday==0!
  if (0 <= WeekDay && WeekDay <= 6) {
     // TRANSLATORS: abbreviated weekdays, beginning with monday (must all be 3 letters!)
     const char *day = tr("MonTueWedThuFriSatSun");
     day += Utf8SymChars(day, WeekDay * 3);
     strn0cpy(buffer, day, min(Utf8SymChars(day, 3) + 1, int(sizeof(buffer))));
     return buffer;
     }
  else
     return "???";
}

cString WeekDayName(time_t t)
{
  struct tm tm_r;
  return WeekDayName(localtime_r(&t, &tm_r)->tm_wday);
}

cString WeekDayNameFull(int WeekDay)
{
  WeekDay = WeekDay == 0 ? 6 : WeekDay - 1; // we start with Monday==0!
  switch (WeekDay) {
    case 0: return tr("Monday");
    case 1: return tr("Tuesday");
    case 2: return tr("Wednesday");
    case 3: return tr("Thursday");
    case 4: return tr("Friday");
    case 5: return tr("Saturday");
    case 6: return tr("Sunday");
    default: return "???";
    }
}

cString WeekDayNameFull(time_t t)
{
  struct tm tm_r;
  return WeekDayNameFull(localtime_r(&t, &tm_r)->tm_wday);
}

cString DayDateTime(time_t t)
{
  char buffer[32];
  if (t == 0)
     time(&t);
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  snprintf(buffer, sizeof(buffer), "%s %02d.%02d. %02d:%02d", *WeekDayName(tm->tm_wday), tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min);
  return buffer;
}

cString TimeToString(time_t t)
{
  char buffer[32];
  if (ctime_r(&t, buffer)) {
     buffer[strlen(buffer) - 1] = 0; // strip trailing newline
     return buffer;
     }
  return "???";
}

cString DateString(time_t t)
{
  char buf[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  char *p = stpcpy(buf, WeekDayName(tm->tm_wday));
  *p++ = ' ';
  strftime(p, sizeof(buf) - (p - buf), "%d.%m.%Y", tm);
  return buf;
}

cString ShortDateString(time_t t)
{
  char buf[32];
  struct tm tm_r;
  tm *tm = localtime_r(&t, &tm_r);
  strftime(buf, sizeof(buf), "%d.%m.%y", tm);
  return buf;
}

cString TimeString(time_t t)
{
  char buf[25];
  struct tm tm_r;
  strftime(buf, sizeof(buf), "%R", localtime_r(&t, &tm_r));
  return buf;
}

// --- RgbToJpeg -------------------------------------------------------------

#define JPEGCOMPRESSMEM 500000

struct tJpegCompressData {
  int size;
  uchar *mem;
  };

static void JpegCompressInitDestination(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     cinfo->dest->free_in_buffer = jcd->size = JPEGCOMPRESSMEM;
     cinfo->dest->next_output_byte = jcd->mem = MALLOC(uchar, jcd->size);
     }
}

static boolean JpegCompressEmptyOutputBuffer(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     int Used = jcd->size;
     int NewSize = jcd->size + JPEGCOMPRESSMEM;
     if (uchar *NewBuffer = (uchar *)realloc(jcd->mem, NewSize)) {
        jcd->size = NewSize;
        jcd->mem = NewBuffer;
        }
     else {
        esyslog("ERROR: out of memory");
        return FALSE;
        }
     if (jcd->mem) {
        cinfo->dest->next_output_byte = jcd->mem + Used;
        cinfo->dest->free_in_buffer = jcd->size - Used;
        return TRUE;
        }
     }
  return FALSE;
}

static void JpegCompressTermDestination(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     int Used = cinfo->dest->next_output_byte - jcd->mem;
     if (Used < jcd->size) {
        if (uchar *NewBuffer = (uchar *)realloc(jcd->mem, Used)) {
           jcd->size = Used;
           jcd->mem = NewBuffer;
           }
        else
           esyslog("ERROR: out of memory");
        }
     }
}

uchar *RgbToJpeg(uchar *Mem, int Width, int Height, int &Size, int Quality)
{
  if (Quality < 0)
     Quality = 0;
  else if (Quality > 100)
     Quality = 100;

  jpeg_destination_mgr jdm;

  jdm.init_destination = JpegCompressInitDestination;
  jdm.empty_output_buffer = JpegCompressEmptyOutputBuffer;
  jdm.term_destination = JpegCompressTermDestination;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  cinfo.dest = &jdm;
  tJpegCompressData jcd;
  cinfo.client_data = &jcd;
  cinfo.image_width = Width;
  cinfo.image_height = Height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, Quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  int rs = Width * 3;
  JSAMPROW rp[Height];
  for (int k = 0; k < Height; k++)
      rp[k] = &Mem[rs * k];
  jpeg_write_scanlines(&cinfo, rp, Height);
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  Size = jcd.size;
  return jcd.mem;
}

// --- GetHostName -----------------------------------------------------------

const char *GetHostName(void)
{
  static char buffer[HOST_NAME_MAX] = "";
  if (!*buffer) {
     if (gethostname(buffer, sizeof(buffer)) < 0) {
        LOG_ERROR;
        strcpy(buffer, "vdr");
        }
     }
  return buffer;
}

// --- cBase64Encoder --------------------------------------------------------

const char *cBase64Encoder::b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

cBase64Encoder::cBase64Encoder(const uchar *Data, int Length, int MaxResult)
{
  data = Data;
  length = Length;
  maxResult = MaxResult;
  i = 0;
  result = MALLOC(char, maxResult + 1);
}

cBase64Encoder::~cBase64Encoder()
{
  free(result);
}

const char *cBase64Encoder::NextLine(void)
{
  int r = 0;
  while (i < length && r < maxResult - 3) {
        result[r++] = b64[(data[i] >> 2) & 0x3F];
        uchar c = (data[i] << 4) & 0x3F;
        if (++i < length)
           c |= (data[i] >> 4) & 0x0F;
        result[r++] = b64[c];
        if (i < length) {
           c = (data[i] << 2) & 0x3F;
           if (++i < length)
              c |= (data[i] >> 6) & 0x03;
           result[r++] = b64[c];
           }
        else {
           i++;
           result[r++] = '=';
           }
        if (i < length) {
           c = data[i] & 0x3F;
           result[r++] = b64[c];
           }
        else
           result[r++] = '=';
        i++;
        }
  if (r > 0) {
     result[r] = 0;
     return result;
     }
  return NULL;
}

// --- cBitStream ------------------------------------------------------------

int cBitStream::GetBit(void)
{
  if (index >= length)
     return 1;
  int r = (data[index >> 3] >> (7 - (index & 7))) & 1;
  ++index;
  return r;
}

uint32_t cBitStream::GetBits(int n)
{
  uint32_t r = 0;
  while (n--)
        r |= GetBit() << n;
  return r;
}

void cBitStream::ByteAlign(void)
{
  int n = index % 8;
  if (n > 0)
     SkipBits(8 - n);
}

void cBitStream::WordAlign(void)
{
  int n = index % 16;
  if (n > 0)
     SkipBits(16 - n);
}

bool cBitStream::SetLength(int Length)
{
  if (Length > length)
     return false;
  length = Length;
  return true;
}

// --- cReadLine -------------------------------------------------------------

cReadLine::cReadLine(void)
{
  size = 0;
  buffer = NULL;
}

cReadLine::~cReadLine()
{
  free(buffer);
}

char *cReadLine::Read(FILE *f)
{
  int n = getline(&buffer, &size, f);
  if (n > 0) {
     n--;
     if (buffer[n] == '\n') {
        buffer[n] = 0;
        if (n > 0) {
           n--;
           if (buffer[n] == '\r')
              buffer[n] = 0;
           }
        }
     return buffer;
     }
  return NULL;
}

// --- cPoller ---------------------------------------------------------------

cPoller::cPoller(int FileHandle, bool Out)
{
  numFileHandles = 0;
  Add(FileHandle, Out);
}

bool cPoller::Add(int FileHandle, bool Out)
{
  if (FileHandle >= 0) {
     for (int i = 0; i < numFileHandles; i++) {
         if (pfd[i].fd == FileHandle && pfd[i].events == (Out ? POLLOUT : POLLIN))
            return true;
         }
     if (numFileHandles < MaxPollFiles) {
        pfd[numFileHandles].fd = FileHandle;
        pfd[numFileHandles].events = Out ? POLLOUT : POLLIN;
        pfd[numFileHandles].revents = 0;
        numFileHandles++;
        return true;
        }
     esyslog("ERROR: too many file handles in cPoller");
     }
  return false;
}

void cPoller::Del(int FileHandle, bool Out)
{
  if (FileHandle >= 0) {
     for (int i = 0; i < numFileHandles; i++) {
         if (pfd[i].fd == FileHandle && pfd[i].events == (Out ? POLLOUT : POLLIN)) {
            if (i < numFileHandles - 1)
               memmove(&pfd[i], &pfd[i + 1], (numFileHandles - i - 1) * sizeof(pollfd));
            numFileHandles--;
            }
         }
     }
}

bool cPoller::Poll(int TimeoutMs)
{
  if (numFileHandles) {
     if (poll(pfd, numFileHandles, TimeoutMs) != 0)
        return true; // returns true even in case of an error, to let the caller
                     // access the file and thus see the error code
     }
  return false;
}

// --- cReadDir --------------------------------------------------------------

cReadDir::cReadDir(const char *Directory)
{
  directory = opendir(Directory);
}

cReadDir::~cReadDir()
{
  if (directory)
     closedir(directory);
}

struct dirent *cReadDir::Next(void)
{
  if (directory) {
#if !__GLIBC_PREREQ(2, 24) // readdir_r() is deprecated as of GLIBC 2.24
     while (readdir_r(directory, &u.d, &result) == 0 && result) {
#else
     while ((result = readdir(directory)) != NULL) {
#endif
           if (strcmp(result->d_name, ".") && strcmp(result->d_name, ".."))
              return result;
           }
     }
  return NULL;
}

// --- cStringList -----------------------------------------------------------

cStringList::~cStringList()
{
  Clear();
}

int cStringList::Find(const char *s) const
{
  for (int i = 0; i < Size(); i++) {
      if (!strcmp(s, At(i)))
         return i;
      }
  return -1;
}

void cStringList::Clear(void)
{
  for (int i = 0; i < Size(); i++)
      free(At(i));
  cVector<char *>::Clear();
}

// --- cFileNameList ---------------------------------------------------------

// TODO better GetFileNames(const char *Directory, cStringList *List)?
cFileNameList::cFileNameList(const char *Directory, bool DirsOnly)
{
  Load(Directory, DirsOnly);
}

bool cFileNameList::Load(const char *Directory, bool DirsOnly)
{
  Clear();
  if (Directory) {
     cReadDir d(Directory);
     struct dirent *e;
     if (d.Ok()) {
        while ((e = d.Next()) != NULL) {
              if (DirsOnly) {
                 struct stat ds;
                 if (stat(AddDirectory(Directory, e->d_name), &ds) == 0) {
                    if (!S_ISDIR(ds.st_mode))
                       continue;
                    }
                 }
              Append(strdup(e->d_name));
              }
        Sort();
        return true;
        }
     else
        LOG_ERROR_STR(Directory);
     }
  return false;
}

// --- cFile -----------------------------------------------------------------

#if DEPRECATED_CFILE
bool cFile::files[FD_SETSIZE] = { false };
int cFile::maxFiles = 0;
#endif

cFile::cFile(void)
{
  f = -1;
}

cFile::~cFile()
{
  Close();
}

bool cFile::Open(const char *FileName, int Flags, mode_t Mode)
{
  if (!IsOpen())
     return Open(open(FileName, Flags, Mode));
  esyslog("ERROR: attempt to re-open %s", FileName);
  return false;
}

bool cFile::Open(int FileDes)
{
  if (FileDes >= 0) {
     if (!IsOpen()) {
        f = FileDes;
#if DEPRECATED_CFILE
        if (f >= 0) {
           if (f < FD_SETSIZE) {
              if (f >= maxFiles)
                 maxFiles = f + 1;
              if (!files[f])
                 files[f] = true;
              else
                 esyslog("ERROR: file descriptor %d already in files[]", f);
              return true;
              }
           else
              esyslog("ERROR: file descriptor %d is larger than FD_SETSIZE (%d)", f, FD_SETSIZE);
           }
#endif
        }
     else
        esyslog("ERROR: attempt to re-open file descriptor %d", FileDes);
     }
  return IsOpen();
}

void cFile::Close(void)
{
  if (f >= 0) {
     close(f);
#if DEPRECATED_CFILE
     files[f] = false;
#endif
     f = -1;
     }
}

bool cFile::Ready(bool Wait)
{
  return f >= 0 && FileReady(f, Wait ? 1000 : 0);
}

#if DEPRECATED_CFILE
bool cFile::AnyFileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  FD_ZERO(&set);
  for (int i = 0; i < maxFiles; i++) {
      if (files[i])
         FD_SET(i, &set);
      }
  if (0 <= FileDes && FileDes < FD_SETSIZE && !files[FileDes])
     FD_SET(FileDes, &set); // in case we come in with an arbitrary descriptor
  if (TimeoutMs == 0)
     TimeoutMs = 10; // load gets too heavy with 0
  struct timeval timeout;
  timeout.tv_sec  = TimeoutMs / 1000;
  timeout.tv_usec = (TimeoutMs % 1000) * 1000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && (FileDes < 0 || FD_ISSET(FileDes, &set));
}
#endif

bool cFile::FileReady(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs >= 0) {
     if (TimeoutMs < 100)
        TimeoutMs = 100;
     timeout.tv_sec  = TimeoutMs / 1000;
     timeout.tv_usec = (TimeoutMs % 1000) * 1000;
     }
  return select(FD_SETSIZE, &set, NULL, NULL, (TimeoutMs >= 0) ? &timeout : NULL) > 0 && FD_ISSET(FileDes, &set);
}

#if DEPRECATED_CFILE
bool cFile::FileReadyForWriting(int FileDes, int TimeoutMs)
{
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs < 100)
     TimeoutMs = 100;
  timeout.tv_sec  = 0;
  timeout.tv_usec = TimeoutMs * 1000;
  return select(FD_SETSIZE, NULL, &set, NULL, &timeout) > 0 && FD_ISSET(FileDes, &set);
}
#endif

// --- cSafeFile -------------------------------------------------------------

cSafeFile::cSafeFile(const char *FileName)
{
  f = NULL;
  fileName = ReadLink(FileName);
  tempName = fileName ? MALLOC(char, strlen(fileName) + 5) : NULL;
  if (tempName)
     strcat(strcpy(tempName, fileName), ".$$$");
}

cSafeFile::~cSafeFile()
{
  if (f)
     fclose(f);
  unlink(tempName);
  free(fileName);
  free(tempName);
}

bool cSafeFile::Open(void)
{
  if (!f && fileName && tempName) {
     f = fopen(tempName, "w");
     if (!f)
        LOG_ERROR_STR(tempName);
     }
  return f != NULL;
}

bool cSafeFile::Close(void)
{
  bool result = true;
  if (f) {
     if (ferror(f) != 0) {
        LOG_ERROR_STR(tempName);
        result = false;
        }
     fflush(f);
     fsync(fileno(f));
     if (fclose(f) < 0) {
        LOG_ERROR_STR(tempName);
        result = false;
        }
     f = NULL;
     if (result && rename(tempName, fileName) < 0) {
        LOG_ERROR_STR(fileName);
        result = false;
        }
     }
  else
     result = false;
  return result;
}

// --- cUnbufferedFile -------------------------------------------------------

#ifndef USE_FADVISE_READ
#define USE_FADVISE_READ 0
#endif
#ifndef USE_FADVISE_WRITE
#define USE_FADVISE_WRITE 1
#endif

#define WRITE_BUFFER KILOBYTE(800)

cUnbufferedFile::cUnbufferedFile(void)
{
  fd = -1;
}

cUnbufferedFile::~cUnbufferedFile()
{
  Close();
}

int cUnbufferedFile::Open(const char *FileName, int Flags, mode_t Mode)
{
  Close();
  fd = open(FileName, Flags, Mode);
  curpos = 0;
#if USE_FADVISE_READ || USE_FADVISE_WRITE
  begin = lastpos = ahead = 0;
  cachedstart = 0;
  cachedend = 0;
  readahead = KILOBYTE(128);
  written = 0;
  totwritten = 0;
  if (fd >= 0)
     posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM); // we could use POSIX_FADV_SEQUENTIAL, but we do our own readahead, disabling the kernel one.
#endif
  return fd;
}

int cUnbufferedFile::Close(void)
{
  if (fd >= 0) {
#if USE_FADVISE_READ || USE_FADVISE_WRITE
     if (totwritten)    // if we wrote anything make sure the data has hit the disk before
        fdatasync(fd);  // calling fadvise, as this is our last chance to un-cache it.
     posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
#endif
     int OldFd = fd;
     fd = -1;
     return close(OldFd);
     }
  errno = EBADF;
  return -1;
}

// When replaying and going e.g. FF->PLAY the position jumps back 2..8M
// hence we do not want to drop recently accessed data at once.
// We try to handle the common cases such as PLAY->FF->PLAY, small
// jumps, moving editing marks etc.

#define FADVGRAN   KILOBYTE(4) // AKA fadvise-chunk-size; PAGE_SIZE or getpagesize(2) would also work.
#define READCHUNK  MEGABYTE(8)

void cUnbufferedFile::SetReadAhead(size_t ra)
{
  readahead = ra;
}

int cUnbufferedFile::FadviseDrop(off_t Offset, off_t Len)
{
  // rounding up the window to make sure that not PAGE_SIZE-aligned data gets freed.
  return posix_fadvise(fd, Offset - (FADVGRAN - 1), Len + (FADVGRAN - 1) * 2, POSIX_FADV_DONTNEED);
}

off_t cUnbufferedFile::Seek(off_t Offset, int Whence)
{
  if (Whence == SEEK_SET && Offset == curpos)
     return curpos;
  curpos = lseek(fd, Offset, Whence);
  return curpos;
}

ssize_t cUnbufferedFile::Read(void *Data, size_t Size)
{
  if (fd >= 0) {
#if USE_FADVISE_READ
     off_t jumped = curpos-lastpos; // nonzero means we're not at the last offset
     if ((cachedstart < cachedend) && (curpos < cachedstart || curpos > cachedend)) {
        // current position is outside the cached window -- invalidate it.
        FadviseDrop(cachedstart, cachedend-cachedstart);
        cachedstart = curpos;
        cachedend = curpos;
        }
     cachedstart = min(cachedstart, curpos);
#endif
     ssize_t bytesRead = safe_read(fd, Data, Size);
     if (bytesRead > 0) {
        curpos += bytesRead;
#if USE_FADVISE_READ
        cachedend = max(cachedend, curpos);

        // Read ahead:
        // no jump? (allow small forward jump still inside readahead window).
        if (jumped >= 0 && jumped <= (off_t)readahead) {
           // Trigger the readahead IO, but only if we've used at least
           // 1/2 of the previously requested area. This avoids calling
           // fadvise() after every read() call.
           if (ahead - curpos < (off_t)(readahead / 2)) {
              posix_fadvise(fd, curpos, readahead, POSIX_FADV_WILLNEED);
              ahead = curpos + readahead;
              cachedend = max(cachedend, ahead);
              }
           if (readahead < Size * 32) { // automagically tune readahead size.
              readahead = Size * 32;
              }
           }
        else
           ahead = curpos; // jumped -> we really don't want any readahead, otherwise e.g. fast-rewind gets in trouble.
#endif
        }
#if USE_FADVISE_READ
     if (cachedstart < cachedend) {
        if (curpos - cachedstart > READCHUNK * 2) {
           // current position has moved forward enough, shrink tail window.
           FadviseDrop(cachedstart, curpos - READCHUNK - cachedstart);
           cachedstart = curpos - READCHUNK;
           }
        else if (cachedend > ahead && cachedend - curpos > READCHUNK * 2) {
           // current position has moved back enough, shrink head window.
           FadviseDrop(curpos + READCHUNK, cachedend - (curpos + READCHUNK));
           cachedend = curpos + READCHUNK;
           }
        }
     lastpos = curpos;
#endif
     return bytesRead;
     }
  return -1;
}

ssize_t cUnbufferedFile::Write(const void *Data, size_t Size)
{
  if (fd >=0) {
     ssize_t bytesWritten = safe_write(fd, Data, Size);
#if USE_FADVISE_WRITE
     if (bytesWritten > 0) {
        begin = min(begin, curpos);
        curpos += bytesWritten;
        written += bytesWritten;
        lastpos = max(lastpos, curpos);
        if (written > WRITE_BUFFER) {
           if (lastpos > begin) {
              // Now do three things:
              // 1) Start writeback of begin..lastpos range
              // 2) Drop the already written range (by the previous fadvise call)
              // 3) Handle nonpagealigned data.
              //    This is why we double the WRITE_BUFFER; the first time around the
              //    last (partial) page might be skipped, writeback will start only after
              //    second call; the third call will still include this page and finally
              //    drop it from cache.
              off_t headdrop = min(begin, off_t(WRITE_BUFFER * 2));
              posix_fadvise(fd, begin - headdrop, lastpos - begin + headdrop, POSIX_FADV_DONTNEED);
              }
           begin = lastpos = curpos;
           totwritten += written;
           written = 0;
           // The above fadvise() works when writing slowly (recording), but could
           // leave cached data around when writing at a high rate, e.g. when cutting,
           // because by the time we try to flush the cached pages (above) the data
           // can still be dirty - we are faster than the disk I/O.
           // So we do another round of flushing, just like above, but at larger
           // intervals -- this should catch any pages that couldn't be released
           // earlier.
           if (totwritten > MEGABYTE(32)) {
              // It seems in some setups, fadvise() does not trigger any I/O and
              // a fdatasync() call would be required do all the work (reiserfs with some
              // kind of write gathering enabled), but the syncs cause (io) load..
              // Uncomment the next line if you think you need them.
              //fdatasync(fd);
              off_t headdrop = min(off_t(curpos - totwritten), off_t(totwritten * 2));
              posix_fadvise(fd, curpos - totwritten - headdrop, totwritten + headdrop, POSIX_FADV_DONTNEED);
              totwritten = 0;
              }
           }
        }
#endif
     return bytesWritten;
     }
  return -1;
}

cUnbufferedFile *cUnbufferedFile::Create(const char *FileName, int Flags, mode_t Mode)
{
  cUnbufferedFile *File = new cUnbufferedFile;
  if (File->Open(FileName, Flags, Mode) < 0) {
     delete File;
     File = NULL;
     }
  return File;
}

// --- cLockFile -------------------------------------------------------------

#define LOCKFILENAME      ".lock-vdr"
#define LOCKFILESTALETIME 600 // seconds before considering a lock file "stale"

cLockFile::cLockFile(const char *Directory)
{
  fileName = NULL;
  f = -1;
  if (DirectoryOk(Directory))
     fileName = strdup(AddDirectory(Directory, LOCKFILENAME));
}

cLockFile::~cLockFile()
{
  Unlock();
  free(fileName);
}

bool cLockFile::Lock(int WaitSeconds)
{
  if (f < 0 && fileName) {
     time_t Timeout = time(NULL) + WaitSeconds;
     do {
        f = open(fileName, O_WRONLY | O_CREAT | O_EXCL, DEFFILEMODE);
        if (f < 0) {
           if (errno == EEXIST) {
              struct stat fs;
              if (stat(fileName, &fs) == 0) {
                 if (abs(time(NULL) - fs.st_mtime) > LOCKFILESTALETIME) {
                    esyslog("ERROR: removing stale lock file '%s'", fileName);
                    if (remove(fileName) < 0) {
                       LOG_ERROR_STR(fileName);
                       break;
                       }
                    continue;
                    }
                 }
              else if (errno != ENOENT) {
                 LOG_ERROR_STR(fileName);
                 break;
                 }
              }
           else {
              LOG_ERROR_STR(fileName);
              if (errno == ENOSPC) {
                 esyslog("ERROR: can't create lock file '%s' - assuming lock anyway!", fileName);
                 return true;
                 }
              break;
              }
           if (WaitSeconds)
              cCondWait::SleepMs(1000);
           }
        } while (f < 0 && time(NULL) < Timeout);
     }
  return f >= 0;
}

void cLockFile::Unlock(void)
{
  if (f >= 0) {
     close(f);
     remove(fileName);
     f = -1;
     }
}

// --- cListObject -----------------------------------------------------------

cListObject::cListObject(void)
{
  prev = next = NULL;
}

cListObject::~cListObject()
{
}

void cListObject::Append(cListObject *Object)
{
  next = Object;
  Object->prev = this;
}

void cListObject::Insert(cListObject *Object)
{
  prev = Object;
  Object->next = this;
}

void cListObject::Unlink(void)
{
  if (next)
     next->prev = prev;
  if (prev)
     prev->next = next;
  next = prev = NULL;
}

int cListObject::Index(void) const
{
  cListObject *p = prev;
  int i = 0;

  while (p) {
        i++;
        p = p->prev;
        }
  return i;
}

// --- cListGarbageCollector -------------------------------------------------

#define LIST_GARBAGE_COLLECTOR_TIMEOUT 5 // seconds

cListGarbageCollector ListGarbageCollector;

cListGarbageCollector::cListGarbageCollector(void)
{
  objects = NULL;
  lastPut = 0;
}

cListGarbageCollector::~cListGarbageCollector()
{
  if (objects)
     esyslog("ERROR: ListGarbageCollector destroyed without prior Purge()!");
}

void cListGarbageCollector::Put(cListObject *Object)
{
  mutex.Lock();
  Object->next = objects;
  objects = Object;
  lastPut = time(NULL);
  mutex.Unlock();
}

void cListGarbageCollector::Purge(bool Force)
{
  mutex.Lock();
  if (objects && (time(NULL) - lastPut > LIST_GARBAGE_COLLECTOR_TIMEOUT || Force)) {
     // We make sure that any object stays in the garbage collector for at least
     // LIST_GARBAGE_COLLECTOR_TIMEOUT seconds, to give objects that have pointers
     // to them a chance to drop these references before the object is finally
     // deleted.
     while (cListObject *Object = objects) {
           objects = Object->next;
           delete Object;
           }
     }
  mutex.Unlock();
}

// --- cListBase -------------------------------------------------------------

cListBase::cListBase(const char *NeedsLocking)
:stateLock(NeedsLocking)
{
  objects = lastObject = NULL;
  count = 0;
  needsLocking = NeedsLocking;
  useGarbageCollector = needsLocking;
}

cListBase::~cListBase()
{
  Clear();
}

bool cListBase::Lock(cStateKey &StateKey, bool Write, int TimeoutMs) const
{
  if (needsLocking)
     return stateLock.Lock(StateKey, Write, TimeoutMs);
  else
     esyslog("ERROR: cListBase::Lock() called for a list that doesn't require locking");
  return false;
}

void cListBase::Add(cListObject *Object, cListObject *After)
{
  if (After && After != lastObject) {
     After->Next()->Insert(Object);
     After->Append(Object);
     }
  else {
     if (lastObject)
        lastObject->Append(Object);
     else
        objects = Object;
     lastObject = Object;
     }
  count++;
}

void cListBase::Ins(cListObject *Object, cListObject *Before)
{
  if (Before && Before != objects) {
     Before->Prev()->Append(Object);
     Before->Insert(Object);
     }
  else {
     if (objects)
        objects->Insert(Object);
     else
        lastObject = Object;
     objects = Object;
     }
  count++;
}

void cListBase::Del(cListObject *Object, bool DeleteObject)
{
  if (Object == objects)
     objects = Object->Next();
  if (Object == lastObject)
     lastObject = Object->Prev();
  Object->Unlink();
  if (DeleteObject) {
     if (useGarbageCollector)
        ListGarbageCollector.Put(Object);
     else
        delete Object;
     }
  count--;
}

void cListBase::Move(int From, int To)
{
  Move(Get(From), Get(To));
}

void cListBase::Move(cListObject *From, cListObject *To)
{
  if (From && To && From != To) {
     if (From->Index() < To->Index())
        To = To->Next();
     if (From == objects)
        objects = From->Next();
     if (From == lastObject)
        lastObject = From->Prev();
     From->Unlink();
     if (To) {
        if (To->Prev())
           To->Prev()->Append(From);
        From->Append(To);
        }
     else {
        lastObject->Append(From);
        lastObject = From;
        }
     if (!From->Prev())
        objects = From;
     }
}

void cListBase::Clear(void)
{
  while (objects) {
        cListObject *object = objects->Next();
        delete objects;
        objects = object;
        }
  objects = lastObject = NULL;
  count = 0;
}

bool cListBase::Contains(const cListObject *Object) const
{
  for (const cListObject *o = objects; o; o = o->Next()) {
      if (o == Object)
         return true;
      }
  return false;
}

void cListBase::SetExplicitModify(void)
{
  stateLock.SetExplicitModify();
}

void cListBase::SetModified(void)
{
  stateLock.SetModified();
}

const cListObject *cListBase::Get(int Index) const
{
  if (Index < 0)
     return NULL;
  const cListObject *object = objects;
  while (object && Index-- > 0)
        object = object->Next();
  return object;
}

static int CompareListObjects(const void *a, const void *b)
{
  const cListObject *la = *(const cListObject **)a;
  const cListObject *lb = *(const cListObject **)b;
  return la->Compare(*lb);
}

void cListBase::Sort(void)
{
  int n = Count();
  cListObject **a = MALLOC(cListObject *, n);
  if (a == NULL)
     return;
  cListObject *object = objects;
  int i = 0;
  while (object && i < n) {
        a[i++] = object;
        object = object->Next();
        }
  qsort(a, n, sizeof(cListObject *), CompareListObjects);
  objects = lastObject = NULL;
  for (i = 0; i < n; i++) {
      a[i]->Unlink();
      count--;
      Add(a[i]);
      }
  free(a);
}

// --- cDynamicBuffer --------------------------------------------------------

cDynamicBuffer::cDynamicBuffer(int InitialSize)
{
  initialSize = InitialSize;
  buffer = NULL;
  size = used = 0;
}

cDynamicBuffer::~cDynamicBuffer()
{
  free(buffer);
}

bool cDynamicBuffer::Realloc(int NewSize)
{
  if (size < NewSize) {
     NewSize = max(NewSize, size ? size * 3 / 2 : initialSize); // increase size by at least 50%
     if (uchar *NewBuffer = (uchar *)realloc(buffer, NewSize)) {
        buffer = NewBuffer;
        size = NewSize;
        }
     else {
        esyslog("ERROR: out of memory");
        return false;
        }
     }
  return true;
}

void cDynamicBuffer::Append(const uchar *Data, int Length)
{
  if (Assert(used + Length)) {
     memcpy(buffer + used, Data, Length);
     used += Length;
     }
}

// --- cHashBase -------------------------------------------------------------

cHashBase::cHashBase(int Size, bool OwnObjects)
{
  size = Size;
  ownObjects = OwnObjects;
  hashTable = (cList<cHashObject>**)calloc(size, sizeof(cList<cHashObject>*));
}

cHashBase::~cHashBase(void)
{
  Clear();
  free(hashTable);
}

void cHashBase::Add(cListObject *Object, unsigned int Id)
{
  unsigned int hash = hashfn(Id);
  if (!hashTable[hash])
     hashTable[hash] = new cList<cHashObject>;
  hashTable[hash]->Add(new cHashObject(Object, Id));
}

void cHashBase::Del(cListObject *Object, unsigned int Id)
{
  cList<cHashObject> *list = hashTable[hashfn(Id)];
  if (list) {
     for (cHashObject *hob = list->First(); hob; hob = list->Next(hob)) {
         if (hob->object == Object) {
            list->Del(hob);
            break;
            }
         }
     }
}

void cHashBase::Clear(void)
{
  for (int i = 0; i < size; i++) {
      if (ownObjects) {
         cList<cHashObject> *list = hashTable[i];
         if (list) {
            for (cHashObject *hob = list->First(); hob; hob = list->Next(hob))
                delete hob->object;
            }
         }
      delete hashTable[i];
      hashTable[i] = NULL;
      }
}

cListObject *cHashBase::Get(unsigned int Id) const
{
  cList<cHashObject> *list = hashTable[hashfn(Id)];
  if (list) {
     for (cHashObject *hob = list->First(); hob; hob = list->Next(hob)) {
         if (hob->id == Id)
            return hob->object;
         }
     }
  return NULL;
}

cList<cHashObject> *cHashBase::GetList(unsigned int Id) const
{
  return hashTable[hashfn(Id)];
}
