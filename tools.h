/*
 * tools.h: Various tools
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.2 2000/03/05 16:14:05 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

#include <stdio.h>
#include <syslog.h>

//TODO
#define dsyslog syslog
#define esyslog syslog
#define isyslog syslog

#define SECSINDAY  86400

#define DELETENULL(p) (delete (p), p = NULL)

char *readline(FILE *f);
int time_ms(void);
bool MakeDirs(const char *FileName);

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
  void Move(int From, int To);
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
