/*
 * tools.h: Various tools
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.h 1.1 2000/02/19 13:36:48 kls Exp $
 */

#ifndef __TOOLS_H
#define __TOOLS_H

#include <syslog.h>

//TODO
#define dsyslog syslog
#define esyslog syslog
#define isyslog syslog

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
  void Clear(void);
  cListObject *Get(int Index);
  int Count(void);
  };

template<class T> class cList : public cListBase {
public:
  T *Get(int Index) { return (T *)cListBase::Get(Index); }
  T *First(void) { return (T *)objects; }
  };

int time_ms(void);

#endif //__TOOLS_H
