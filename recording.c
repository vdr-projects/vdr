/*
 * recording.h: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.c 1.15 2000/07/29 14:08:17 kls Exp $
 */

#define _GNU_SOURCE
#include "recording.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "interface.h"
#include "tools.h"
#include "videodir.h"

#define RECEXT       ".rec"
#define DELEXT       ".del"
#define DATAFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#define NAMEFORMAT   "%s/%s/" DATAFORMAT

#define SUMMARYFILESUFFIX "/summary.vdr"

#define FINDCMD      "find %s -follow -type d -name '%s' 2> /dev/null | sort -df"

#define MINDISKSPACE 1024 // MB

#define DISKCHECKDELTA 300 // seconds between checks for free disk space

void AssertFreeDiskSpace(void)
{
  // With every call to this function we try to actually remove
  // a file, or mark a file for removal ("delete" it), so that
  // it will get removed during the next call.
  static time_t LastFreeDiskCheck = 0;
  if (time(NULL) - LastFreeDiskCheck > DISKCHECKDELTA) {
     if (!VideoFileSpaceAvailable(MINDISKSPACE)) {
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

cRecording::cRecording(cTimer *Timer)
{
  titleBuffer = NULL;
  fileName = NULL;
  name = strdup(Timer->file);
  summary = Timer->summary ? strdup(Timer->summary) : NULL;
  if (summary)
     strreplace(summary, '|', '\n');
  start = Timer->StartTime();
  priority = Timer->priority;
  lifetime = Timer->lifetime;
}

cRecording::cRecording(const char *FileName)
{
  titleBuffer = NULL;
  fileName = strdup(FileName);
  FileName += strlen(VideoDirectory) + 1;
  char *p = strrchr(FileName, '/');

  name = NULL;
  summary = NULL;
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
        strreplace(name, '_', ' ');
        }
     // read an optional summary file:
     char *SummaryFileName = NULL;
     asprintf(&SummaryFileName, "%s%s", fileName, SUMMARYFILESUFFIX);
     int f = open(SummaryFileName, O_RDONLY);
     if (f >= 0) {
        struct stat buf;
        if (fstat(f, &buf) == 0) {
           int size = buf.st_size;
           summary = new char[size + 1]; // +1 for terminating 0
           if (summary) {
              int rbytes = read(f, summary, size);
              if (rbytes >= 0) {
                 summary[rbytes] = 0;
                 if (rbytes != size)
                    esyslog(LOG_ERR, "%s: expected %d bytes but read %d", SummaryFileName, size, rbytes);
                 }
              else {
                 LOG_ERROR_STR(SummaryFileName);
                 delete summary;
                 summary = NULL;
                 }
              
              }
           else
              esyslog(LOG_ERR, "can't allocate %d byte of memory for summary file '%s'", size + 1, SummaryFileName);
           close(f);
           }
        else
           LOG_ERROR_STR(SummaryFileName);
        }
     else if (errno != ENOENT)
        LOG_ERROR_STR(SummaryFileName);
     delete SummaryFileName;
     }
}

cRecording::~cRecording()
{
  delete titleBuffer;
  delete fileName;
  delete name;
  delete summary;
}

const char *cRecording::FileName(void)
{
  if (!fileName) {
     struct tm *t = localtime(&start);
     asprintf(&fileName, NAMEFORMAT, VideoDirectory, name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, priority, lifetime);
     if (fileName)
        strreplace(fileName, ' ', '_');
     }
  return fileName;
}

const char *cRecording::Title(char Delimiter)
{
  delete titleBuffer;
  titleBuffer = NULL;
  struct tm *t = localtime(&start);
  asprintf(&titleBuffer, "%02d.%02d.%02d%c%02d:%02d%c%s",
                         t->tm_mday,
                         t->tm_mon + 1,
                         t->tm_year % 100,
                         Delimiter,
                         t->tm_hour,
                         t->tm_min,
                         Delimiter,
                         name);
  return titleBuffer;
}

bool cRecording::WriteSummary(void)
{
  if (summary) {
     char *SummaryFileName = NULL;
     asprintf(&SummaryFileName, "%s%s", fileName, SUMMARYFILESUFFIX);
     FILE *f = fopen(SummaryFileName, "w");
     if (f) {
        if (fputs(summary, f) < 0)
           LOG_ERROR_STR(SummaryFileName);
        fclose(f);
        }
     else
        LOG_ERROR_STR(SummaryFileName);
     delete SummaryFileName;
     }
  return true;
}

bool cRecording::Delete(void)
{
  bool result = true;
  char *NewName = strdup(FileName());
  char *ext = strrchr(NewName, '.');
  if (strcmp(ext, RECEXT) == 0) {
     strncpy(ext, DELEXT, strlen(ext));
     isyslog(LOG_INFO, "deleting recording %s", FileName());
     result = RenameVideoFile(FileName(), NewName);
     }
  delete NewName;
  return result;
}

bool cRecording::Remove(void)
{
  isyslog(LOG_INFO, "removing recording %s", FileName());
  return RemoveVideoFile(FileName());
}

// --- cRecordings -----------------------------------------------------------

bool cRecordings::Load(bool Deleted)
{
  Clear();
  bool result = false;
  char *cmd = NULL;
  asprintf(&cmd, FINDCMD, VideoDirectory, Deleted ? "*" DELEXT : "*" RECEXT);
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

