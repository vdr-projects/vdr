/*
 * tools.c: Various tools
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: tools.c 1.28 2001/02/04 11:27:49 kls Exp $
 */

#define _GNU_SOURCE
#include "tools.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#if defined(DEBUG_OSD)
#include <ncurses.h>
#endif
#include <sys/time.h>
#include <unistd.h>

#define MaxBuffer 1000

int SysLogLevel = 3;

void writechar(int filedes, char c)
{
  write(filedes, &c, sizeof(c));
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

char *strn0cpy(char *dest, const char *src, size_t n)
{
  char *s = dest;
  for ( ; --n && (*dest = *src) != 0; dest++, src++) ;
  *dest = 0;
  return s;
}

char *strreplace(char *s, char c1, char c2)
{
  char *p = s;

  while (*p) {
        if (*p == c1)
           *p = c2;
        p++;
        }
  return s;
}

char *skipspace(const char *s)
{
  while (*s && isspace(*s))
        s++;
  return (char *)s;
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

bool isempty(const char *s)
{
  return !(s && *skipspace(s));
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

bool isnumber(const char *s)
{
  while (*s) {
        if (!isdigit(*s))
           return false;
        s++;
        }
  return true;
}

const char *AddDirectory(const char *DirName, const char *FileName)
{
  static char *buf = NULL;
  delete buf;
  asprintf(&buf, "%s/%s", DirName && *DirName ? DirName : ".", FileName);
  return buf;
}

#define DFCMD  "df -m '%s'"

uint FreeDiskSpaceMB(const char *Directory)
{
  //TODO Find a simpler way to determine the amount of free disk space!
  uint Free = 0;
  char *cmd = NULL;
  asprintf(&cmd, DFCMD, Directory);
  FILE *p = popen(cmd, "r");
  if (p) {
     char *s;
     while ((s = readline(p)) != NULL) {
           if (*s == '/') {
              uint available;
              sscanf(s, "%*s %*d %*d %u", &available);
              Free = available;
              break;
              }
           }
     pclose(p);
     }
  else
     esyslog(LOG_ERR, "ERROR: can't open pipe for cmd '%s'", cmd);
  delete cmd;
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
           esyslog(LOG_ERR, "ERROR: can't access %s", DirName);
        }
     else if (LogErrors)
        esyslog(LOG_ERR, "ERROR: %s is not a directory", DirName);
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
           dsyslog(LOG_INFO, "creating directory %s", s);
           if (mkdir(s, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
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
  delete s;
  return result;
}

bool RemoveFileOrDir(const char *FileName, bool FollowSymlinks)
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
                    if (FollowSymlinks) {
                       int size = strlen(buffer) * 2; // should be large enough
                       char *l = new char[size];
                       int n = readlink(buffer, l, size);
                       if (n < 0) {
                          if (errno != EINVAL)
                             LOG_ERROR_STR(buffer);
                          }
                       else if (n < size) {
                          l[n] = 0;
                          dsyslog(LOG_INFO, "removing %s", l);
                          if (remove(l) < 0)
                             LOG_ERROR_STR(l);
                          }
                       else
                          esyslog(LOG_ERR, "ERROR: symlink name length (%d) exceeded anticipated buffer size (%d)", n, size);
                       delete l;
                       }
                    dsyslog(LOG_INFO, "removing %s", buffer);
                    if (remove(buffer) < 0)
                       LOG_ERROR_STR(buffer);
                    delete buffer;
                    }
                 }
           closedir(d);
           }
        else {
           LOG_ERROR_STR(FileName);
           return false;
           }
        }
     dsyslog(LOG_INFO, "removing %s", FileName);
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

char *ReadLink(const char *FileName)
{
  char RealName[_POSIX_PATH_MAX];
  const char *TargetName = NULL;
  int n = readlink(FileName, RealName, sizeof(RealName) - 1);
  if (n < 0) {
     if (errno == ENOENT || errno == EINVAL) // file doesn't exist or is not a symlink
        TargetName = FileName;
     else { // some other error occurred
        LOG_ERROR_STR(FileName);
        }
     }
  else if (n < int(sizeof(RealName))) { // got it!
     RealName[n] = 0;
     TargetName = RealName;
     }
  else
     esyslog(LOG_ERR, "ERROR: symlink's target name too long: %s", FileName);
  return TargetName ? strdup(TargetName) : NULL;
}

// --- cFile -----------------------------------------------------------------

bool cFile::files[FD_SETSIZE] = { false };
int cFile::maxFiles = 0;

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
  esyslog(LOG_ERR, "ERROR: attempt to re-open %s", FileName);
  return false;
}

bool cFile::Open(int FileDes)
{
  if (FileDes >= 0) {
     if (!IsOpen()) {
        f = FileDes;
        if (f >= 0) {
           if (f < FD_SETSIZE) {
              if (f >= maxFiles)
                 maxFiles = f + 1;
              if (!files[f])
                 files[f] = true;
              else
                 esyslog(LOG_ERR, "ERROR: file descriptor %d already in files[]", f);
              return true;
              }
           else
              esyslog(LOG_ERR, "ERROR: file descriptor %d is larger than FD_SETSIZE (%d)", f, FD_SETSIZE);
           }
        }
     else
        esyslog(LOG_ERR, "ERROR: attempt to re-open file descriptor %d", FileDes);
     }
  return false;
}

void cFile::Close(void)
{
  if (f >= 0) {
     close(f);
     files[f] = false;
     f = -1;
     }
}

int cFile::ReadString(char *Buffer, int Size)
{
  int rbytes = 0;
  bool wait = true;

  while (Ready(wait)) {
        int n = read(f, Buffer + rbytes, 1);
        if (n == 0)
           break; // EOF
        if (n < 0) {
           LOG_ERROR;
           return -1;
           }
        rbytes += n;
        if (rbytes == Size || Buffer[rbytes - 1] == '\n')
           break;
        wait = false;
        }
  return rbytes;
}

bool cFile::Ready(bool Wait)
{
  return f >= 0 && AnyFileReady(f, Wait ? 1000 : 0);
}

bool cFile::AnyFileReady(int FileDes, int TimeoutMs)
{
#ifdef DEBUG_OSD
  refresh();
#endif
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

bool cFile::FileReady(int FileDes, int TimeoutMs)
{
#ifdef DEBUG_OSD
  refresh();
#endif
  fd_set set;
  struct timeval timeout;
  FD_ZERO(&set);
  FD_SET(FileDes, &set);
  if (TimeoutMs < 100)
     TimeoutMs = 100;
  timeout.tv_sec  = 0;
  timeout.tv_usec = TimeoutMs * 1000;
  return select(FD_SETSIZE, &set, NULL, NULL, &timeout) > 0 && FD_ISSET(FileDes, &set);
}

// --- cSafeFile -------------------------------------------------------------

cSafeFile::cSafeFile(const char *FileName)
{
  f = NULL;
  fileName = ReadLink(FileName);
  tempName = fileName ? new char[strlen(fileName) + 5] : NULL;
  if (tempName)
     strcat(strcpy(tempName, fileName), ".$$$");
}

cSafeFile::~cSafeFile()
{
  if (f)
     fclose(f);
  unlink(tempName);
  delete fileName;
  delete tempName;
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

void cSafeFile::Close(void)
{
  if (f) {
     fclose(f);
     f = NULL;
     rename(tempName, fileName);
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

cListObject *cListBase::Get(int Index) const
{
  if (Index < 0)
     return NULL;
  cListObject *object = objects;
  while (object && Index-- > 0)
        object = object->Next();
  return object;
}

int cListBase::Count(void) const
{
  int n = 0;
  cListObject *object = objects;

  while (object) {
        n++;
        object = object->Next();
        }
  return n;
}

