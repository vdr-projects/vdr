/*
 * tools.c: Various tools
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "tools.h"
#include <stdlib.h>
#include <sys/time.h>

int time_ms(void)
{
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return t.tv_sec * 1000 + t.tv_usec / 1000;
  return 0;
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

void cListObject::Unlink(void)
{
  if (next)
     next->prev = prev;
  if (prev)
     prev->next = next;
}

int cListObject::Index(void)
{
  cListObject *p = prev;
  int i = 0;

  while (p) {
        i++;
        p = p->prev;
        }
  return i;
}

// --- cListBase -------------------------------------------------------------

cListBase::cListBase(void)
{ 
  objects = lastObject = NULL;
}

cListBase::~cListBase()
{
  Clear();
  while (objects) {
        cListObject *object = objects->Next();
        delete objects;
        objects = object;
        }
}

void cListBase::Add(cListObject *Object) 
{ 
  if (lastObject)
     lastObject->Append(Object);
  else
     objects = Object;
  lastObject = Object;
}

void cListBase::Del(cListObject *Object)
{
  if (Object == objects)
     objects = Object->Next();
  if (Object == lastObject)
     lastObject = Object->Prev();
  Object->Unlink();
  delete Object;
}

void cListBase::Clear(void)
{
  while (objects) {
        cListObject *object = objects->Next();
        delete objects;
        objects = object;
        }
  objects = lastObject = NULL;
}

cListObject *cListBase::Get(int Index)
{
  cListObject *object = objects;
  while (object && Index-- > 0)
        object = object->Next();
  return object;
}

int cListBase::Count(void)
{
  int n = 0;
  cListObject *object = objects;

  while (object) {
        n++;
        object = object->Next();
        }
  return n;
}

