/*
 * recording.c: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.c 1.61 2002/04/21 14:02:55 kls Exp $
 */

#include "recording.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "i18n.h"
#include "interface.h"
#include "tools.h"
#include "videodir.h"

#define RECEXT       ".rec"
#define DELEXT       ".del"
#ifdef VFAT
#define DATAFORMAT   "%4d-%02d-%02d.%02d.%02d.%02d.%02d" RECEXT
#else
#define DATAFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#endif
#define NAMEFORMAT   "%s/%s/" DATAFORMAT

#define RESUMEFILESUFFIX  "/resume.vdr"
#define SUMMARYFILESUFFIX "/summary.vdr"
#define MARKSFILESUFFIX   "/marks.vdr"

#define FINDCMD      "find %s -follow -type d -name '%s' 2> /dev/null"

#define MINDISKSPACE 1024 // MB

#define DELETEDLIFETIME     1 // hours after which a deleted recording will be actually removed
#define REMOVECHECKDELTA 3600 // seconds between checks for removing deleted files
#define DISKCHECKDELTA    100 // seconds between checks for free disk space
#define REMOVELATENCY      10 // seconds to wait until next check after removing a file

#define TIMERMACRO_TITLE    "TITLE"
#define TIMERMACRO_EPISODE  "EPISODE"

void RemoveDeletedRecordings(void)
{
  static time_t LastRemoveCheck = 0;
  if (time(NULL) - LastRemoveCheck > REMOVECHECKDELTA) {
     // Make sure only one instance of VDR does this:
     cLockFile LockFile(VideoDirectory);
     if (!LockFile.Lock())
        return;
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
        if (r0 && time(NULL) - r0->start > DELETEDLIFETIME * 60) {
           r0->Remove();
           RemoveEmptyVideoDirectories();
           LastRemoveCheck += REMOVELATENCY;
           return;
           }
        }
     LastRemoveCheck = time(NULL);
     }
}

void AssertFreeDiskSpace(int Priority)
{
  // With every call to this function we try to actually remove
  // a file, or mark a file for removal ("delete" it), so that
  // it will get removed during the next call.
  static time_t LastFreeDiskCheck = 0;
  if (time(NULL) - LastFreeDiskCheck > DISKCHECKDELTA) {
     if (!VideoFileSpaceAvailable(MINDISKSPACE)) {
        // Make sure only one instance of VDR does this:
        cLockFile LockFile(VideoDirectory);
        if (!LockFile.Lock())
           return;
        // Remove the oldest file that has been "deleted":
        isyslog(LOG_INFO, "low disk space while recording, trying to remove a deleted recording...");
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
        isyslog(LOG_INFO, "...no deleted recording found, trying to delete an old recording...");
        if (Recordings.Load(false)) {
           cRecording *r = Recordings.First();
           cRecording *r0 = NULL;
           while (r) {
                 if (r->lifetime < MAXLIFETIME) { // recordings with MAXLIFETIME live forever
                    if ((r->lifetime == 0 && Priority > r->priority) || // the recording has no guaranteed lifetime and the new recording has higher priority
                        (time(NULL) - r->start) / SECSINDAY > r->lifetime) { // the recording's guaranteed lifetime has expired
                       if (r0) {
                          if (r->priority < r0->priority || (r->priority == r0->priority && r->start < r0->start))
                             r0 = r; // in any case we delete the one with the lowest priority (or the older one in case of equal priorities)
                          }
                       else
                          r0 = r;
                       }
                    }
                 r = Recordings.Next(r);
                 }
           if (r0 && r0->Delete())
              return;
           }
        // Unable to free disk space, but there's nothing we can do about that...
        isyslog(LOG_INFO, "...no old recording found, giving up");
        Interface->Confirm(tr("Low disk space!"), 30);
        }
     LastFreeDiskCheck = time(NULL);
     }
}

// --- cResumeFile ------------------------------------------------------------

cResumeFile::cResumeFile(const char *FileName)
{
  fileName = new char[strlen(FileName) + strlen(RESUMEFILESUFFIX) + 1];
  if (fileName) {
     strcpy(fileName, FileName);
     strcat(fileName, RESUMEFILESUFFIX);
     }
  else
     esyslog(LOG_ERR, "ERROR: can't allocate memory for resume file name");
}

cResumeFile::~cResumeFile()
{
  delete fileName;
}

int cResumeFile::Read(void)
{
  int resume = -1;
  if (fileName) {
     int f = open(fileName, O_RDONLY);
     if (f >= 0) {
        if (safe_read(f, &resume, sizeof(resume)) != sizeof(resume)) {
           resume = -1;
           LOG_ERROR_STR(fileName);
           }
        close(f);
        }
     else if (errno != ENOENT)
        LOG_ERROR_STR(fileName);
     }
  return resume;
}

bool cResumeFile::Save(int Index)
{
  if (fileName) {
     int f = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
     if (f >= 0) {
        if (safe_write(f, &Index, sizeof(Index)) < 0)
           LOG_ERROR_STR(fileName);
        close(f);
        return true;
        }
     }
  return false;
}

void cResumeFile::Delete(void)
{
  if (fileName) {
     if (remove(fileName) < 0 && errno != ENOENT)
        LOG_ERROR_STR(fileName);
     }
}

// --- cRecording ------------------------------------------------------------

#define RESUME_NOT_INITIALIZED (-2)

struct tCharExchange { char a; char b; };
tCharExchange CharExchange[] = {
  { '~',  '/'    },
  { ' ',  '_'    },
  { '\'', '\x01' },
  { '/',  '\x02' },
  { 0, 0 }
  };

static char *ExchangeChars(char *s, bool ToFileSystem)
{
  char *p = s;
  while (*p) {
#ifdef VFAT
        // The VFAT file system can't handle all characters, so we
        // have to take extra efforts to encode/decode them:
        if (ToFileSystem) {
           switch (*p) {
                  // characters that can be used "as is":
                  case '!':
                  case '@':
                  case '$':
                  case '%':
                  case '&':
                  case '(':
                  case ')':
                  case '+':
                  case ',':
                  case '-':
                  case ';':
                  case '=':
                  case '0' ... '9':
                  case 'a' ... 'z':
                  case 'A' ... 'Z':
                  case 'ä': case 'Ä':
                  case 'ö': case 'Ö':
                  case 'ü': case 'Ü':
                  case 'ß':
                       break;
                  // characters that can be mapped to other characters:
                  case ' ': *p = '_'; break;
                  case '~': *p = '/'; break;
                  // characters that have to be encoded:
                  default:
                    if (*p != '.' || !*(p + 1) || *(p + 1) == '~') { // Windows can't handle '.' at the end of directory names
                       int l = p - s;
                       s = (char *)realloc(s, strlen(s) + 10);
                       p = s + l;
                       char buf[4];
                       sprintf(buf, "#%02X", (unsigned char)*p);
                       memmove(p + 2, p, strlen(p) + 1);
                       strncpy(p, buf, 3);
                       p += 2;
                       }
                  }
           }
        else {
           switch (*p) {
             // mapped characters:
             case '_': *p = ' '; break;
             case '/': *p = '~'; break;
             // encodes characters:
             case '#': {
                  if (strlen(p) > 2) {
                     char buf[3];
                     sprintf(buf, "%c%c", *(p + 1), *(p + 2));
                     unsigned char c = strtol(buf, NULL, 16);
                     *p = c;
                     memmove(p + 1, p + 3, strlen(p) - 2);
                     }
                  }
                  break;
             // backwards compatibility:
             case '\x01': *p = '\''; break;
             case '\x02': *p = '/';  break;
             case '\x03': *p = ':';  break;
             }
           }
#else
        for (struct tCharExchange *ce = CharExchange; ce->a && ce->b; ce++) {
            if (*p == (ToFileSystem ? ce->a : ce->b)) {
               *p = ToFileSystem ? ce->b : ce->a;
               break;
               }
            }
#endif
        p++;
        }
  return s;
}

cRecording::cRecording(cTimer *Timer, const char *Title, const char *Subtitle, const char *Summary)
{
  resume = RESUME_NOT_INITIALIZED;
  titleBuffer = NULL;
  sortBuffer = NULL;
  fileName = NULL;
  name = NULL;
  // set up the actual name:
  if (isempty(Title))
     Title = Channels.GetChannelNameByNumber(Timer->channel);
  if (isempty(Subtitle))
     Subtitle = " ";
  char *macroTITLE   = strstr(Timer->file, TIMERMACRO_TITLE);
  char *macroEPISODE = strstr(Timer->file, TIMERMACRO_EPISODE);
  if (macroTITLE || macroEPISODE) {
     name = strdup(Timer->file);
     name = strreplace(name, TIMERMACRO_TITLE, Title);
     name = strreplace(name, TIMERMACRO_EPISODE, Subtitle);
     if (Timer->IsSingleEvent()) {
        Timer->SetFile(name); // this was an instant recording, so let's set the actual data
        Timers.Save();
        }
     }
  else if (Timer->IsSingleEvent() || !Setup.UseSubtitle)
     name = strdup(Timer->file);
  else
     asprintf(&name, "%s~%s", Timer->file, Subtitle);
  // substitute characters that would cause problems in file names:
  strreplace(name, '\n', ' ');
  start = Timer->StartTime();
  priority = Timer->priority;
  lifetime = Timer->lifetime;
  // handle summary:
  summary = !isempty(Timer->summary) ? strdup(Timer->summary) : NULL;
  if (!summary) {
     if (isempty(Subtitle))
        Subtitle = "";
     if (isempty(Summary))
        Summary = "";
     if (*Subtitle || *Summary)
        asprintf(&summary, "%s\n\n%s%s%s", Title, Subtitle, (*Subtitle && *Summary) ? "\n\n" : "", Summary);
     }
}

cRecording::cRecording(const char *FileName)
{
  resume = RESUME_NOT_INITIALIZED;
  titleBuffer = NULL;
  sortBuffer = NULL;
  fileName = strdup(FileName);
  FileName += strlen(VideoDirectory) + 1;
  char *p = strrchr(FileName, '/');

  name = NULL;
  summary = NULL;
  if (p) {
     time_t now = time(NULL);
     struct tm tm_r;
     struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
     t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
     if (7 == sscanf(p + 1, DATAFORMAT, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &priority, &lifetime)) {
        t.tm_year -= 1900;
        t.tm_mon--;
        t.tm_sec = 0;
        start = mktime(&t);
        name = new char[p - FileName + 1];
        strncpy(name, FileName, p - FileName);
        name[p - FileName] = 0;
        name = ExchangeChars(name, false);
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
              int rbytes = safe_read(f, summary, size);
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
  delete sortBuffer;
  delete fileName;
  delete name;
  delete summary;
}

char *cRecording::StripEpisodeName(char *s)
{
  char *t = s, *s1 = NULL, *s2 = NULL;
  while (*t) {
        if (*t == '/') {
           if (s1) {
              if (s2)
                 s1 = s2;
              s2 = t;
              }
           else
              s1 = t;
           }
        t++;
        }
  if (s1 && s2)
     memmove(s1 + 1, s2, t - s2 + 1);
  return s;
}

char *cRecording::SortName(void)
{
  if (!sortBuffer) {
     char *s = StripEpisodeName(strdup(FileName() + strlen(VideoDirectory) + 1));
     int l = strxfrm(NULL, s, 0);
     sortBuffer = new char[l];
     strxfrm(sortBuffer, s, l);
     delete s;
     }
  return sortBuffer;
}

int cRecording::GetResume(void)
{
  if (resume == RESUME_NOT_INITIALIZED) {
     cResumeFile ResumeFile(FileName());
     resume = ResumeFile.Read();
     }
  return resume;
}

bool cRecording::operator< (const cListObject &ListObject)
{
  cRecording *r = (cRecording *)&ListObject;
  return strcasecmp(SortName(), r->SortName()) < 0;
}

const char *cRecording::FileName(void)
{
  if (!fileName) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     name = ExchangeChars(name, true);
     asprintf(&fileName, NAMEFORMAT, VideoDirectory, name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, priority, lifetime);
     name = ExchangeChars(name, false);
     }
  return fileName;
}

const char *cRecording::Title(char Delimiter, bool NewIndicator, int Level)
{
  char New = NewIndicator && IsNew() ? '*' : ' ';
  delete titleBuffer;
  titleBuffer = NULL;
  if (Level < 0 || Level == HierarchyLevels()) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     char *s;
     if (Level > 0 && (s = strrchr(name, '~')) != NULL)
        s++;
     else
        s = name;
     asprintf(&titleBuffer, "%02d.%02d%c%02d:%02d%c%c%s",
                            t->tm_mday,
                            t->tm_mon + 1,
                            Delimiter,
                            t->tm_hour,
                            t->tm_min,
                            New,
                            Delimiter,
                            s);
     // let's not display a trailing '~':
     stripspace(titleBuffer);
     s = &titleBuffer[strlen(titleBuffer) - 1];
     if (*s == '~')
        *s = 0;
     }
  else if (Level < HierarchyLevels()) {
     const char *s = name;
     const char *p = s;
     while (*++s) {
           if (*s == '~') {
              if (Level--)
                 p = s + 1;
              else
                 break;
              }
           }
     titleBuffer = new char[s - p + 3];
     *titleBuffer = Delimiter;
     *(titleBuffer + 1) = Delimiter;
     strn0cpy(titleBuffer + 2, p, s - p + 1);
     }
  else
     return "";
  return titleBuffer;
}

const char *cRecording::PrefixFileName(char Prefix)
{
  const char *p = PrefixVideoFileName(FileName(), Prefix);
  if (p) {
     delete fileName;
     fileName = strdup(p);
     return fileName;
     }
  return NULL;
}

int cRecording::HierarchyLevels(void)
{
  const char *s = name;
  int level = 0;
  while (*++s) {
        if (*s == '~')
           level++;
        }
  return level;
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
     if (access(NewName, F_OK) == 0) {
        // the new name already exists, so let's remove that one first:
        isyslog(LOG_INFO, "removing recording %s", NewName);
        RemoveVideoFile(NewName);
        }
     isyslog(LOG_INFO, "deleting recording %s", FileName());
     result = RenameVideoFile(FileName(), NewName);
     }
  delete NewName;
  return result;
}

bool cRecording::Remove(void)
{
  // let's do a final safety check here:
  if (!endswith(FileName(), DELEXT)) {
     esyslog(LOG_ERR, "attempt to remove recording %s", FileName());
     return false;
     }
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
           if (r->Name())
              Add(r);
           else
              delete r;
           }
     pclose(p);
     Sort();
     result = Count() > 0;
     }
  else
     Interface->Error("Error while opening pipe!");
  delete cmd;
  return result;
}

cRecording *cRecordings::GetByName(const char *FileName)
{
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (strcmp(recording->FileName(), FileName) == 0)
         return recording;
      }
  return NULL;
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
  if (cConfig<cMark>::Load(MarksFile)) {
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
     cConfig<cMark>::Add(m = new cMark(Position));
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

// --- cRecordingUserCommand -------------------------------------------------

const char *cRecordingUserCommand::command = NULL;

void cRecordingUserCommand::InvokeCommand(const char *State, const char *RecordingFileName)
{
  if (command) {
     char *cmd;
     asprintf(&cmd, "%s %s \"%s\"", command, State, strescape(RecordingFileName, "\"$"));
     isyslog(LOG_INFO, "executing '%s'", cmd);
     SystemExec(cmd);
     delete cmd;
     }
}
