/*
 * tools.h: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.42 2002/02/17 12:57:44 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

//#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

extern int SysLogLevel;

#define esyslog(a...) void( (SysLogLevel > 0) ? syslog(a) : void() )
#define isyslog(a...) void( (SysLogLevel > 1) ? syslog(a) : void() )
#define dsyslog(a...) void( (SysLogLevel > 2) ? syslog(a) : void() )

#define LOG_ERROR         esyslog(LOG_ERR, "ERROR (%s,%d): %m", __FILE__, __LINE__)
#define LOG_ERROR_STR(s)  esyslog(LOG_ERR, "ERROR: %s: %m", s)

#define SECSINDAY  86400

#define KILOBYTE(n) ((n) * 1024)
#define MEGABYTE(n) ((n) * 1024 * 1024)

#define MAXPARSEBUFFER KILOBYTE(10)

#define DELETENULL(p) (delete (p), p = NULL)

template<class T> inline T min(T a, T b) { return a <= b ? a : b; }
template<class T> inline T max(T a, T b) { return a >= b ? a : b; }
template<class T> inline void swap(T &a, T &b) { T t = a; a = b; b = t; }

ssize_t safe_read(int filedes, void *buffer, size_t size);
ssize_t safe_write(int filedes, const void *buffer, size_t size);
void writechar(int filedes, char c);
char *readline(FILE *f);
char *strcpyrealloc(char *dest, const char *src);
char *strn0cpy(char *dest, const char *src, size_t n);
char *strreplace(char *s, char c1, char c2);
char *strreplace(char *s, const char *s1, const char *s2); // re-allocates 's' and deletes the original string if necessary!
char *skipspace(const char *s);
char *stripspace(char *s);
char *compactspace(char *s);
const char *strescape(const char *s, const char *chars); // returns a statically allocated string!
bool startswith(const char *s, const char *p);
bool endswith(const char *s, const char *p);
bool isempty(const char *s);
int time_ms(void);
void delay_ms(int ms);
bool isnumber(const char *s);
const char *AddDirectory(const char *DirName, const char *FileName); // returns a statically allocated string!
int FreeDiskSpaceMB(const char *Directory, int *UsedMB = NULL);
bool DirectoryOk(const char *DirName, bool LogErrors = false);
bool MakeDirs(const char *FileName, bool IsDirectory = false);
bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks = false);
bool RemoveEmptyDirectories(const char *DirName, bool RemoveThis = false);
char *ReadLink(const char *FileName);
bool SpinUpDisk(const char *FileName);
const char *WeekDayName(int WeekDay); // returns a statically allocated string!
const char *DayDateTime(time_t t = 0); // returns a statically allocated string!

class cFile {
private:
  static bool files[];
  static int maxFiles;
  int f;
public:
  cFile(void);
  ~cFile();
  operator int () { return f; }
  bool Open(const char *FileName, int Flags, mode_t Mode = S_IRUSR | S_IWUSR | S_IRGRP);
  bool Open(int FileDes);
  void Close(void);
  bool IsOpen(void) { return f >= 0; }
  bool Ready(bool Wait = true);
  static bool AnyFileReady(int FileDes = -1, int TimeoutMs = 1000);
  static bool FileReady(int FileDes, int TimeoutMs = 1000);
  static bool FileReadyForWriting(int FileDes, int TimeoutMs = 1000);
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
private:
  cListObject *prev, *next;
public:
  cListObject(void);
  virtual ~cListObject();
  virtual bool operator< (const cListObject &ListObject) { return false; }
  void Append(cListObject *Object);
  void Unlink(void);
  int Index(void);
  cListObject *Prev(void) const { return prev; }
  cListObject *Next(void) const { return next; }
  };

class cListBase {
protected:
  cListObject *objects, *lastObject;
  cListBase(void);
public:
  virtual ~cListBase();
  void Add(cListObject *Object);
  void Del(cListObject *Object);
  virtual void Move(int From, int To);
  void Move(cListObject *From, cListObject *To);
  virtual void Clear(void);
  cListObject *Get(int Index) const;
  int Count(void) const;
  void Sort(void);
  };

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) const { return (T *)cListBase::Get(Index); }
  T *First(void) const { return (T *)objects; }
  T *Last(void) const { return (T *)lastObject; }
  T *Prev(const T *object) const { return (T *)object->Prev(); }
  T *Next(const T *object) const { return (T *)object->Next(); }
  };

#endif //__TOOLS_H
