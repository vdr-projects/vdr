/*
 * tools.h: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.5 2000/04/24 09:46:05 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

#include <errno.h>
#include <stdio.h>
#include <syslog.h>

extern int SysLogLevel;

#define esyslog if (SysLogLevel > 0) syslog
#define isyslog if (SysLogLevel > 1) syslog
#define dsyslog if (SysLogLevel > 2) syslog

#define LOG_ERROR         esyslog(LOG_ERR, "ERROR (%s,%d): %s", __FILE__, __LINE__, strerror(errno))
#define LOG_ERROR_STR(s)  esyslog(LOG_ERR, "ERROR: %s: %s", s, strerror(errno));

#define SECSINDAY  86400

#define DELETENULL(p) (delete (p), p = NULL)

void writechar(int filedes, char c);
void writeint(int filedes, int n);
char readchar(int filedes);
bool readint(int filedes, int &n);
char *readline(FILE *f);
int time_ms(void);
void delay_ms(int ms);
bool MakeDirs(const char *FileName, bool IsDirectory = false);
bool RemoveFileOrDir(const char *FileName);

class cListObject {
private:
  cListObject *prev, *next;
public:
  cListObject(void);
  virtual ~cListObject();
  void Append(cListObject *Object);
  void Unlink(void);
  int Index(void);
  cListObject *Prev(void) { return prev; }
  cListObject *Next(void) { return next; }
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
  void Clear(void);
  cListObject *Get(int Index);
  int Count(void);
  };

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) { return (T *)cListBase::Get(Index); }
  T *First(void) { return (T *)objects; }
  T *Next(T *object) { return (T *)object->Next(); }
  };

#endif //__TOOLS_H
