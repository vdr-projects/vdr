/*
 * tools.c: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 1.7 2000/04/24 15:01:35 kls Exp $
 */

#define _GNU_SOURCE
#include "tools.h"
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MaxBuffer 1000

int SysLogLevel = 3;

bool DataAvailable(int filedes)
{
  fd_set set;
  FD_ZERO(&set);
  FD_SET(filedes, &set);
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && FD_ISSET(filedes, &set);
}

void writechar(int filedes, char c)
{
  write(filedes, &c, sizeof(c));
}

void writeint(int filedes, int n)
{
  write(filedes, &n, sizeof(n));
}

char readchar(int filedes)
{
  char c;
  read(filedes, &c, 1);
  return c;
}

bool readint(int filedes, int &n)
{
  return DataAvailable(filedes) && read(filedes, &n, sizeof(n)) == sizeof(n);
}

void purge(int filedes)
{
  while (DataAvailable(filedes))
        readchar(filedes);
}

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
  static time_t t0 = 0;
  struct timeval t;
  if (gettimeofday(&t, NULL) == 0) {
     if (t0 == 0)
        t0 = t.tv_sec; // this avoids an overflow (we only work with deltas)
     return (t.tv_sec - t0) * 1000 + t.tv_usec / 1000;
     }
  return 0;
}

void delay_ms(int ms)
{
  int t0 = time_ms();
  while (time_ms() - t0 < ms)
        ;
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
           isyslog(LOG_INFO, "creating directory %s", s);
           if (mkdir(s, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
              esyslog(LOG_ERR, "ERROR: %s: %s", s, strerror(errno));
              result = false;
              break;
              }
           }
        if (p)
           *p++ = '/';
        else
           break;
        }
  delete s;
  return result;
}

bool RemoveFileOrDir(const char *FileName)
{
  struct stat st;
  if (stat(FileName, &st) == 0) {
     if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(FileName);
        if (d) {
           struct dirent *e;
           while ((e = readdir(d)) != NULL) {
                 if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                    char *buffer;
                    asprintf(&buffer, "%s/%s", FileName, e->d_name);
                    if (remove(buffer) < 0)
                       esyslog(LOG_ERR, "ERROR: %s: %s", buffer, strerror(errno));
                    delete buffer;
                    }
                 }
           closedir(d);
           }
        else {
           esyslog(LOG_ERR, "ERROR: %s: %s", FileName, strerror(errno));
           return false;
           }
        }
     if (remove(FileName) == 0)
        return true;
     }
  else
     esyslog(LOG_ERR, "ERROR: %s: %s", FileName, strerror(errno));
  return false;
}

bool CheckProcess(pid_t pid)
{
  pid_t Pid2Check = pid;
  int status;
  pid = waitpid(Pid2Check, &status, WNOHANG);
  if (pid < 0) {
     if (errno != ECHILD)
        LOG_ERROR;
     return false;
     }
  return true;
}

void KillProcess(pid_t pid, int Timeout)
{
  pid_t Pid2Wait4 = pid;
  for (time_t t0 = time(NULL); time(NULL) - t0 < Timeout; ) {
      int status;
      pid_t pid = waitpid(Pid2Wait4, &status, WNOHANG);
      if (pid < 0) {
         if (errno != ECHILD)
            LOG_ERROR;
         return;
         }
      if (pid == Pid2Wait4)
         return;
      }
  esyslog(LOG_ERR, "ERROR: process %d won't end (waited %d seconds) - terminating it...", Pid2Wait4, Timeout);
  if (kill(Pid2Wait4, SIGTERM) < 0) {
     esyslog(LOG_ERR, "ERROR: process %d won't terminate (%s) - killing it...", Pid2Wait4, strerror(errno));
     if (kill(Pid2Wait4, SIGKILL) < 0)
        esyslog(LOG_ERR, "ERROR: process %d won't die (%s) - giving up", Pid2Wait4, strerror(errno));
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

