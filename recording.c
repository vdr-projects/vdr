/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.c 1.9 2000/07/15 16:55:08 kls Exp $
 */

#define _GNU_SOURCE
#include "recording.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "interface.h"
#include "tools.h"

#define RECEXT       ".rec"
#define DELEXT       ".del"
#define DATAFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#define NAMEFORMAT   "%s/%s/" DATAFORMAT

#define FINDCMD      "find %s -type d -name '%s' | sort -df"

#define DFCMD        "df -m %s"
#define MINDISKSPACE 1024 // MB

#define DISKCHECKDELTA 300 // seconds between checks for free disk space

const char *BaseDir = "/video";

static bool LowDiskSpace(void)
{
  //TODO Find a simpler way to determine the amount of free disk space!
  bool result = true;
  char *cmd = NULL;
  asprintf(&cmd, DFCMD, BaseDir);
  FILE *p = popen(cmd, "r");
  if (p) {
     char *s;
     while ((s = readline(p)) != NULL) {
           if (*s == '/') {
              int available;
              sscanf(s, "%*s %*d %*d %d", &available);
              result = available < MINDISKSPACE;
              break;
              }
           }
     pclose(p);
     }
  else
     esyslog(LOG_ERR, "ERROR: can't open pipe for cmd '%s'", cmd);
  delete cmd;
  return result;
}

void AssertFreeDiskSpace(void)
{
  // With every call to this function we try to actually remove
  // a file, or mark a file for removal ("delete" it), so that
  // it will get removed during the next call.
  static time_t LastFreeDiskCheck = 0;
  if (time(NULL) - LastFreeDiskCheck > DISKCHECKDELTA) {
     if (LowDiskSpace()) {
        // Remove the oldest file that has been "deleted":
        cRecordings Recordings;
        if (Recordings.Load(true)) {
           cRecording *r = Recordings.First();
           cRecording *r0 = r;
           while (r) {
                 if (r->start < r0->start)
                    r0 = r;
                 r = Recordings.Next(r);
                 }
           if (r0 && r0->Remove())
              return;
           }
        // No "deleted" files to remove, so let's see if we can delete a recording:
        if (Recordings.Load(false)) {
           cRecording *r = Recordings.First();
           cRecording *r0 = NULL;
           while (r) {
                 if ((time(NULL) - r->start) / SECSINDAY > r->lifetime) {
                    if (r0) {
                       if (r->priority < r0->priority)
                          r0 = r;
                       }
                    else
                       r0 = r;
                    }
                 r = Recordings.Next(r);
                 }
           if (r0 && r0->Delete())
              return;
           }
        // Unable to free disk space, but there's nothing we can do about that...
        esyslog(LOG_ERR, "low disk space, but no recordings to delete");
        }
     LastFreeDiskCheck = time(NULL);
     }
}

// --- cRecording ------------------------------------------------------------

cRecording::cRecording(const char *Name, time_t Start, int Priority, int LifeTime)
{
  titleBuffer = NULL;
  fileName = NULL;
  name = strdup(Name);
  start = Start;
  priority = Priority;
  lifetime = LifeTime;
}

cRecording::cRecording(cTimer *Timer)
{
  titleBuffer = NULL;
  fileName = NULL;
  name = strdup(Timer->file);
  start = Timer->StartTime();
  priority = Timer->priority;
  lifetime = Timer->lifetime;
}

cRecording::cRecording(const char *FileName)
{
  titleBuffer = NULL;
  fileName = strdup(FileName);
  FileName += strlen(BaseDir) + 1;
  char *p = strrchr(FileName, '/');

  name = NULL;
  if (p) {
     time_t now = time(NULL);
     struct tm t = *localtime(&now); // this initializes the time zone in 't'
     if (7 == sscanf(p + 1, DATAFORMAT, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &priority, &lifetime)) {
        t.tm_year -= 1900;
        t.tm_mon--;
        t.tm_sec = 0;
        start = mktime(&t);
        name = new char[p - FileName + 1];
        strncpy(name, FileName, p - FileName);
        name[p - FileName] = 0;
        }
     }
}

cRecording::~cRecording()
{
  delete titleBuffer;
  delete fileName;
  delete name;
}

const char *cRecording::FileName(void)
{
  if (!fileName) {
     struct tm *t = localtime(&start);
     asprintf(&fileName, NAMEFORMAT, BaseDir, name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, priority, lifetime);
     }
  return fileName;
}

const char *cRecording::Title(char Delimiter)
{
  delete titleBuffer;
  titleBuffer = NULL;
  struct tm *t = localtime(&start);
  asprintf(&titleBuffer, "%02d.%02d.%04d%c%02d:%02d%c%s",
                         t->tm_mday,
                         t->tm_mon + 1,
                         t->tm_year + 1900,
                         Delimiter,
                         t->tm_hour,
                         t->tm_min,
                         Delimiter,
                         name);
  return titleBuffer;
}

bool cRecording::Delete(void)
{
  bool result = true;
  char *NewName = strdup(FileName());
  char *ext = strrchr(NewName, '.');
  if (strcmp(ext, RECEXT) == 0) {
     strncpy(ext, DELEXT, strlen(ext));
     isyslog(LOG_INFO, "deleting recording %s", FileName());
     if (rename(FileName(), NewName) == -1) {
        esyslog(LOG_ERR, "ERROR: %s: %s", FileName(), strerror(errno));
        result = false;
        }
     }
  delete NewName;
  return result;
}

bool cRecording::Remove(void)
{
  isyslog(LOG_INFO, "removing recording %s", FileName());
  return RemoveFileOrDir(FileName());
}

// --- cRecordings -----------------------------------------------------------

bool cRecordings::Load(bool Deleted)
{
  Clear();
  bool result = false;
  char *cmd = NULL;
  asprintf(&cmd, FINDCMD, BaseDir, Deleted ? "*" DELEXT : "*" RECEXT);
  FILE *p = popen(cmd, "r");
  if (p) {
     char *s;
     while ((s = readline(p)) != NULL) {
           cRecording *r = new cRecording(s);
           if (r->name)
              Add(r);
           else
              delete r;
           }
     pclose(p);
     result = Count() > 0;
     }
  else
     Interface.Error("Error while opening pipe!");
  delete cmd;
  return result;
}

