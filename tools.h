/*
 * tools.h: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.20 2000/11/12 15:27:06 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

extern int SysLogLevel;

#define esyslog if (SysLogLevel > 0) syslog
#define isyslog if (SysLogLevel > 1) syslog
#define dsyslog if (SysLogLevel > 2) syslog

#define LOG_ERROR         esyslog(LOG_ERR, "ERROR (%s,%d): %s", __FILE__, __LINE__, strerror(errno))
#define LOG_ERROR_STR(s)  esyslog(LOG_ERR, "ERROR: %s: %s", s, strerror(errno));

#define SECSINDAY  86400
#define MAXPROCESSTIMEOUT   3 // seconds

#define DELETENULL(p) (delete (p), p = NULL)

void writechar(int filedes, char c);
void writeint(int filedes, int n);
char readchar(int filedes);
bool readint(int filedes, int &n);
void purge(int filedes);
char *readline(FILE *f);
char *strn0cpy(char *dest, const char *src, size_t n);
char *strreplace(char *s, char c1, char c2);
char *skipspace(const char *s);
char *stripspace(char *s);
bool isempty(const char *s);
int time_ms(void);
void delay_ms(int ms);
bool isnumber(const char *s);
const char *AddDirectory(const char *DirName, const char *FileName);
uint FreeDiskSpaceMB(const char *Directory);
bool DirectoryOk(const char *DirName, bool LogErrors = false);
bool MakeDirs(const char *FileName, bool IsDirectory = false);
bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks = false);
bool CheckProcess(pid_t pid);
void KillProcess(pid_t pid, int Timeout = MAXPROCESSTIMEOUT);

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
  int ReadString(char *Buffer, int Size);
  bool Ready(bool Wait = true);
  static bool AnyFileReady(int FileDes = -1, int TimeoutMs = 1000);
  static bool FileReady(int FileDes, int TimeoutMs = 1000);
  };

class cListObject {
private:
  cListObject *prev, *next;
public:
  cListObject(void);
  virtual ~cListObject();
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
  };

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) const { return (T *)cListBase::Get(Index); }
  T *First(void) const { return (T *)objects; }
  T *Next(const T *object) const { return (T *)object->Next(); }
  };

#endif //__TOOLS_H
