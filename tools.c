/*
 * tools.c: Various tools
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 1.2 2000/03/05 14:33:58 kls Exp $
 */

#include "tools.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MaxBuffer 1000

char *readline(FILE *f)
{
  static char buffer[MaxBuffer];
  if (fgets(buffer, sizeof(buffer), f) > 0) {
     int l = strlen(buffer) - 1;
     if (l >= 0 && buffer[l] == '\n')
        buffer[l] = 0;
     return buffer;
     }
  return NULL;
}

int time_ms(void)
{
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0)
     return t.tv_sec * 1000 + t.tv_usec / 1000;
  return 0;
}

bool MakeDirs(const char *FileName)
{
  bool result = true;
  char *s = strdup(FileName);
  char *p = s;
  if (*p == '/')
     p++;
  while ((p = strchr(p, '/')) != NULL) {
        *p = 0;
        struct stat fs;
        if (stat(s, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
           isyslog(LOG_INFO, "creating directory %s", s);
           if (mkdir(s, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
              esyslog(LOG_ERR, "ERROR while creating directory %s: %s", s, strerror(errno));
              result = false;
              break;
              }
           }
        *p++ = '/';
        }
  delete s;
  return result;
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
  next = prev = NULL;
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

void cListBase::Move(int From, int To)
{
  Move(Get(From), Get(To));
}

void cListBase::Move(cListObject *From, cListObject *To)
{
  if (From && To) {
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
     else
        lastObject->Append(From);
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

