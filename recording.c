/*
 * recording.c: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.c 1.22 2000/12/16 14:25:14 kls Exp $
 */

#define _GNU_SOURCE
#include "recording.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dvbapi.h"
#include "interface.h"
#include "tools.h"
#include "videodir.h"

#define RECEXT       ".rec"
#define DELEXT       ".del"
#define DATAFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#define NAMEFORMAT   "%s/%s/" DATAFORMAT

#define SUMMARYFILESUFFIX "/summary.vdr"
#define MARKSFILESUFFIX   "/marks.vdr"

#define FINDCMD      "find %s -follow -type d -name '%s' 2> /dev/null | sort -df"

#define MINDISKSPACE 1024 // MB

#define DISKCHECKDELTA 300 // seconds between checks for free disk space
#define REMOVELATENCY   10 // seconds to wait until next check after removing a file 

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
           if (r0 && r0->Remove()) {
              LastFreeDiskCheck += REMOVELATENCY;
              return;
              }
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
  // substitute characters that would cause problems in file names:
  for (char *p = name; *p; p++) {
      switch (*p) {
        case '\n': *p = ' '; break;
        case '/':  *p = '-'; break; 
        }
      }
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

const char *cRecording::Title(char Delimiter, bool NewIndicator)
{
  char New = ' ';
  if (NewIndicator) {
     cResumeFile ResumeFile(FileName());
     if (ResumeFile.Read() <= 0)
        New = '*';
     }
  delete titleBuffer;
  titleBuffer = NULL;
  struct tm *t = localtime(&start);
  asprintf(&titleBuffer, "%02d.%02d%c%02d:%02d%c%c%s",
                         t->tm_mday,
                         t->tm_mon + 1,
                         Delimiter,
                         t->tm_hour,
                         t->tm_min,
                         New,
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
     Interface->Error("Error while opening pipe!");
  delete cmd;
  return result;
}

// --- cMark -----------------------------------------------------------------

char *cMark::buffer = NULL;

cMark::cMark(int Position, const char *Comment)
{
  position = Position;
  comment = Comment ? strdup(Comment) : NULL;
}

cMark::~cMark()
{
  delete comment;
}

const char *cMark::ToText(void)
{
  delete buffer;
  asprintf(&buffer, "%s%s%s\n", IndexToHMSF(position, true), comment ? " " : "", comment ? comment : "");
  return buffer;
}

bool cMark::Parse(const char *s)
{
  delete comment;
  comment = NULL;
  position = HMSFToIndex(s);
  const char *p = strchr(s, ' ');
  if (p) {
     p = skipspace(p);
     if (*p) {
        comment = strdup(p);
        comment[strlen(comment) - 1] = 0; // strips trailing newline
        }
     }
  return true;
}

bool cMark::Save(FILE *f)
{
  return fprintf(f, ToText()) > 0;
}

// --- cMarks ----------------------------------------------------------------

bool cMarks::Load(const char *RecordingFileName)
{
  const char *MarksFile = AddDirectory(RecordingFileName, MARKSFILESUFFIX);
  if (cConfig::Load(MarksFile)) {
     Sort();
     return true;
     }
  return false;
}

void cMarks::Sort(void)
{
  for (cMark *m1 = First(); m1; m1 = Next(m1)) {
      for (cMark *m2 = Next(m1); m2; m2 = Next(m2)) {
          if (m2->position < m1->position) {
             swap(m1->position, m2->position);
             swap(m1->comment, m2->comment);
             }
          }
      }
}

cMark *cMarks::Add(int Position)
{
  cMark *m = Get(Position);
  if (!m) {
     cConfig::Add(m = new cMark(Position));
     Sort();
     }
  return m;
}

cMark *cMarks::Get(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi)) {
      if (mi->position == Position)
         return mi;
      }
  return NULL;
}

cMark *cMarks::GetPrev(int Position)
{
  for (cMark *mi = Last(); mi; mi = Prev(mi)) {
      if (mi->position < Position)
         return mi;
      }
  return NULL;
}

cMark *cMarks::GetNext(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi)) {
      if (mi->position > Position)
         return mi;
      }
  return NULL;
}

