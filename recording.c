/*
 * recording.c: Recording file handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: recording.c 3.28 2015/02/16 07:49:14 kls Exp $
 */

#include "recording.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#define __STDC_FORMAT_MACROS // Required for format specifiers
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "channels.h"
#include "cutter.h"
#include "i18n.h"
#include "interface.h"
#include "menu.h"
#include "remux.h"
#include "ringbuffer.h"
#include "skins.h"
#include "tools.h"
#include "videodir.h"

#define SUMMARYFALLBACK

#define RECEXT       ".rec"
#define DELEXT       ".del"
/* This was the original code, which works fine in a Linux only environment.
   Unfortunately, because of Windows and its brain dead file system, we have
   to use a more complicated approach, in order to allow users who have enabled
   the --vfat command line option to see their recordings even if they forget to
   enable --vfat when restarting VDR... Gee, do I hate Windows.
   (kls 2002-07-27)
#define DATAFORMAT   "%4d-%02d-%02d.%02d:%02d.%02d.%02d" RECEXT
#define NAMEFORMAT   "%s/%s/" DATAFORMAT
*/
#define DATAFORMATPES   "%4d-%02d-%02d.%02d%*c%02d.%02d.%02d" RECEXT
#define NAMEFORMATPES   "%s/%s/" "%4d-%02d-%02d.%02d.%02d.%02d.%02d" RECEXT
#define DATAFORMATTS    "%4d-%02d-%02d.%02d.%02d.%d-%d" RECEXT
#define NAMEFORMATTS    "%s/%s/" DATAFORMATTS

#define RESUMEFILESUFFIX  "/resume%s%s"
#ifdef SUMMARYFALLBACK
#define SUMMARYFILESUFFIX "/summary.vdr"
#endif
#define INFOFILESUFFIX    "/info"
#define MARKSFILESUFFIX   "/marks"

#define SORTMODEFILE      ".sort"

#define MINDISKSPACE 1024 // MB

#define REMOVECHECKDELTA   60 // seconds between checks for removing deleted files
#define DELETEDLIFETIME   300 // seconds after which a deleted recording will be actually removed
#define DISKCHECKDELTA    100 // seconds between checks for free disk space
#define REMOVELATENCY      10 // seconds to wait until next check after removing a file
#define MARKSUPDATEDELTA   10 // seconds between checks for updating editing marks
#define MININDEXAGE      3600 // seconds before an index file is considered no longer to be written
#define MAXREMOVETIME      10 // seconds after which to return from removing deleted recordings

#define MAX_LINK_LEVEL  6

#define LIMIT_SECS_PER_MB_RADIO 5 // radio recordings typically have more than this

int DirectoryPathMax = PATH_MAX - 1;
int DirectoryNameMax = NAME_MAX;
bool DirectoryEncoding = false;
int InstanceId = 0;

cRecordings DeletedRecordings(true);
static cRecordings VanishedRecordings;

// --- cRemoveDeletedRecordingsThread ----------------------------------------

class cRemoveDeletedRecordingsThread : public cThread {
protected:
  virtual void Action(void);
public:
  cRemoveDeletedRecordingsThread(void);
  };

cRemoveDeletedRecordingsThread::cRemoveDeletedRecordingsThread(void)
:cThread("remove deleted recordings", true)
{
}

void cRemoveDeletedRecordingsThread::Action(void)
{
  // Make sure only one instance of VDR does this:
  cLockFile LockFile(cVideoDirectory::Name());
  if (LockFile.Lock()) {
     time_t StartTime = time(NULL);
     bool deleted = false;
     cThreadLock DeletedRecordingsLock(&DeletedRecordings);
     for (cRecording *r = DeletedRecordings.First(); r; ) {
         if (cIoThrottle::Engaged())
            return;
         if (time(NULL) - StartTime > MAXREMOVETIME)
            return; // don't stay here too long
         if (cRemote::HasKeys())
            return; // react immediately on user input
         if (r->Deleted() && time(NULL) - r->Deleted() > DELETEDLIFETIME) {
            cRecording *next = DeletedRecordings.Next(r);
            r->Remove();
            DeletedRecordings.Del(r);
            r = next;
            deleted = true;
            continue;
            }
         r = DeletedRecordings.Next(r);
         }
     if (deleted) {
        const char *IgnoreFiles[] = { SORTMODEFILE, NULL };
        cVideoDirectory::RemoveEmptyVideoDirectories(IgnoreFiles);
        }
     }
}

static cRemoveDeletedRecordingsThread RemoveDeletedRecordingsThread;

// ---

void RemoveDeletedRecordings(void)
{
  static time_t LastRemoveCheck = 0;
  if (time(NULL) - LastRemoveCheck > REMOVECHECKDELTA) {
     if (!RemoveDeletedRecordingsThread.Active()) {
        cThreadLock DeletedRecordingsLock(&DeletedRecordings);
        for (cRecording *r = DeletedRecordings.First(); r; r = DeletedRecordings.Next(r)) {
            if (r->Deleted() && time(NULL) - r->Deleted() > DELETEDLIFETIME) {
               RemoveDeletedRecordingsThread.Start();
               break;
               }
            }
        }
     LastRemoveCheck = time(NULL);
     }
}

void AssertFreeDiskSpace(int Priority, bool Force)
{
  static cMutex Mutex;
  cMutexLock MutexLock(&Mutex);
  // With every call to this function we try to actually remove
  // a file, or mark a file for removal ("delete" it), so that
  // it will get removed during the next call.
  static time_t LastFreeDiskCheck = 0;
  int Factor = (Priority == -1) ? 10 : 1;
  if (Force || time(NULL) - LastFreeDiskCheck > DISKCHECKDELTA / Factor) {
     if (!cVideoDirectory::VideoFileSpaceAvailable(MINDISKSPACE)) {
        // Make sure only one instance of VDR does this:
        cLockFile LockFile(cVideoDirectory::Name());
        if (!LockFile.Lock())
           return;
        // Remove the oldest file that has been "deleted":
        isyslog("low disk space while recording, trying to remove a deleted recording...");
        cThreadLock DeletedRecordingsLock(&DeletedRecordings);
        if (DeletedRecordings.Count()) {
           cRecording *r = DeletedRecordings.First();
           cRecording *r0 = NULL;
           while (r) {
                 if (r->IsOnVideoDirectoryFileSystem()) { // only remove recordings that will actually increase the free video disk space
                    if (!r0 || r->Start() < r0->Start())
                       r0 = r;
                    }
                 r = DeletedRecordings.Next(r);
                 }
           if (r0) {
              if (r0->Remove())
                 LastFreeDiskCheck += REMOVELATENCY / Factor;
              DeletedRecordings.Del(r0);
              return;
              }
           }
        else {
           // DeletedRecordings was empty, so to be absolutely sure there are no
           // deleted recordings we need to double check:
           DeletedRecordings.Update(true);
           if (DeletedRecordings.Count())
              return; // the next call will actually remove it
           }
        // No "deleted" files to remove, so let's see if we can delete a recording:
        if (Priority > 0) {
           isyslog("...no deleted recording found, trying to delete an old recording...");
           cThreadLock RecordingsLock(&Recordings);
           if (Recordings.Count()) {
              cRecording *r = Recordings.First();
              cRecording *r0 = NULL;
              while (r) {
                    if (r->IsOnVideoDirectoryFileSystem()) { // only delete recordings that will actually increase the free video disk space
                       if (!r->IsEdited() && r->Lifetime() < MAXLIFETIME) { // edited recordings and recordings with MAXLIFETIME live forever
                          if ((r->Lifetime() == 0 && Priority > r->Priority()) || // the recording has no guaranteed lifetime and the new recording has higher priority
                              (r->Lifetime() > 0 && (time(NULL) - r->Start()) / SECSINDAY >= r->Lifetime())) { // the recording's guaranteed lifetime has expired
                             if (r0) {
                                if (r->Priority() < r0->Priority() || (r->Priority() == r0->Priority() && r->Start() < r0->Start()))
                                   r0 = r; // in any case we delete the one with the lowest priority (or the older one in case of equal priorities)
                                }
                             else
                                r0 = r;
                             }
                          }
                       }
                    r = Recordings.Next(r);
                    }
              if (r0 && r0->Delete()) {
                 Recordings.Del(r0);
                 return;
                 }
              }
           // Unable to free disk space, but there's nothing we can do about that...
           isyslog("...no old recording found, giving up");
           }
        else
           isyslog("...no deleted recording found, priority %d too low to trigger deleting an old recording", Priority);
        Skins.QueueMessage(mtWarning, tr("Low disk space!"), 5, -1);
        }
     LastFreeDiskCheck = time(NULL);
     }
}

// --- Clear vanished recordings ---------------------------------------------

void ClearVanishedRecordings(void)
{
  cThreadLock RecordingsLock(&Recordings); // yes, it *is* Recordings!
  VanishedRecordings.Clear();
}

// --- cResumeFile -----------------------------------------------------------

cResumeFile::cResumeFile(const char *FileName, bool IsPesRecording)
{
  isPesRecording = IsPesRecording;
  const char *Suffix = isPesRecording ? RESUMEFILESUFFIX ".vdr" : RESUMEFILESUFFIX;
  fileName = MALLOC(char, strlen(FileName) + strlen(Suffix) + 1);
  if (fileName) {
     strcpy(fileName, FileName);
     sprintf(fileName + strlen(fileName), Suffix, Setup.ResumeID ? "." : "", Setup.ResumeID ? *itoa(Setup.ResumeID) : "");
     }
  else
     esyslog("ERROR: can't allocate memory for resume file name");
}

cResumeFile::~cResumeFile()
{
  free(fileName);
}

int cResumeFile::Read(void)
{
  int resume = -1;
  if (fileName) {
     struct stat st;
     if (stat(fileName, &st) == 0) {
        if ((st.st_mode & S_IWUSR) == 0) // no write access, assume no resume
           return -1;
        }
     if (isPesRecording) {
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
     else {
        FILE *f = fopen(fileName, "r");
        if (f) {
           cReadLine ReadLine;
           char *s;
           int line = 0;
           while ((s = ReadLine.Read(f)) != NULL) {
                 ++line;
                 char *t = skipspace(s + 1);
                 switch (*s) {
                   case 'I': resume = atoi(t);
                             break;
                   default: ;
                   }
                 }
           fclose(f);
           }
        else if (errno != ENOENT)
           LOG_ERROR_STR(fileName);
        }
     }
  return resume;
}

bool cResumeFile::Save(int Index)
{
  if (fileName) {
     if (isPesRecording) {
        int f = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE);
        if (f >= 0) {
           if (safe_write(f, &Index, sizeof(Index)) < 0)
              LOG_ERROR_STR(fileName);
           close(f);
           Recordings.ResetResume(fileName);
           return true;
           }
        }
     else {
        FILE *f = fopen(fileName, "w");
        if (f) {
           fprintf(f, "I %d\n", Index);
           fclose(f);
           Recordings.ResetResume(fileName);
           }
        else
           LOG_ERROR_STR(fileName);
        return true;
        }
     }
  return false;
}

void cResumeFile::Delete(void)
{
  if (fileName) {
     if (remove(fileName) == 0)
        Recordings.ResetResume(fileName);
     else if (errno != ENOENT)
        LOG_ERROR_STR(fileName);
     }
}

// --- cRecordingInfo --------------------------------------------------------

cRecordingInfo::cRecordingInfo(const cChannel *Channel, const cEvent *Event)
{
  channelID = Channel ? Channel->GetChannelID() : tChannelID::InvalidID;
  channelName = Channel ? strdup(Channel->Name()) : NULL;
  ownEvent = Event ? NULL : new cEvent(0);
  event = ownEvent ? ownEvent : Event;
  aux = NULL;
  framesPerSecond = DEFAULTFRAMESPERSECOND;
  priority = MAXPRIORITY;
  lifetime = MAXLIFETIME;
  fileName = NULL;
  if (Channel) {
     // Since the EPG data's component records can carry only a single
     // language code, let's see whether the channel's PID data has
     // more information:
     cComponents *Components = (cComponents *)event->Components();
     if (!Components)
        Components = new cComponents;
     for (int i = 0; i < MAXAPIDS; i++) {
         const char *s = Channel->Alang(i);
         if (*s) {
            tComponent *Component = Components->GetComponent(i, 2, 3);
            if (!Component)
               Components->SetComponent(Components->NumComponents(), 2, 3, s, NULL);
            else if (strlen(s) > strlen(Component->language))
               strn0cpy(Component->language, s, sizeof(Component->language));
            }
         }
     // There's no "multiple languages" for Dolby Digital tracks, but
     // we do the same procedure here, too, in case there is no component
     // information at all:
     for (int i = 0; i < MAXDPIDS; i++) {
         const char *s = Channel->Dlang(i);
         if (*s) {
            tComponent *Component = Components->GetComponent(i, 4, 0); // AC3 component according to the DVB standard
            if (!Component)
               Component = Components->GetComponent(i, 2, 5); // fallback "Dolby" component according to the "Premiere pseudo standard"
            if (!Component)
               Components->SetComponent(Components->NumComponents(), 2, 5, s, NULL);
            else if (strlen(s) > strlen(Component->language))
               strn0cpy(Component->language, s, sizeof(Component->language));
            }
         }
     // The same applies to subtitles:
     for (int i = 0; i < MAXSPIDS; i++) {
         const char *s = Channel->Slang(i);
         if (*s) {
            tComponent *Component = Components->GetComponent(i, 3, 3);
            if (!Component)
               Components->SetComponent(Components->NumComponents(), 3, 3, s, NULL);
            else if (strlen(s) > strlen(Component->language))
               strn0cpy(Component->language, s, sizeof(Component->language));
            }
         }
     if (Components != event->Components())
        ((cEvent *)event)->SetComponents(Components);
     }
}

cRecordingInfo::cRecordingInfo(const char *FileName)
{
  channelID = tChannelID::InvalidID;
  channelName = NULL;
  ownEvent = new cEvent(0);
  event = ownEvent;
  aux = NULL;
  framesPerSecond = DEFAULTFRAMESPERSECOND;
  priority = MAXPRIORITY;
  lifetime = MAXLIFETIME;
  fileName = strdup(cString::sprintf("%s%s", FileName, INFOFILESUFFIX));
}

cRecordingInfo::~cRecordingInfo()
{
  delete ownEvent;
  free(aux);
  free(channelName);
  free(fileName);
}

void cRecordingInfo::SetData(const char *Title, const char *ShortText, const char *Description)
{
  if (!isempty(Title))
     ((cEvent *)event)->SetTitle(Title);
  if (!isempty(ShortText))
     ((cEvent *)event)->SetShortText(ShortText);
  if (!isempty(Description))
     ((cEvent *)event)->SetDescription(Description);
}

void cRecordingInfo::SetAux(const char *Aux)
{
  free(aux);
  aux = Aux ? strdup(Aux) : NULL;
}

void cRecordingInfo::SetFramesPerSecond(double FramesPerSecond)
{
  framesPerSecond = FramesPerSecond;
}

void cRecordingInfo::SetFileName(const char *FileName)
{
  bool IsPesRecording = fileName && endswith(fileName, ".vdr");
  free(fileName);
  fileName = strdup(cString::sprintf("%s%s", FileName, IsPesRecording ? INFOFILESUFFIX ".vdr" : INFOFILESUFFIX));
}

bool cRecordingInfo::Read(FILE *f)
{
  if (ownEvent) {
     cReadLine ReadLine;
     char *s;
     int line = 0;
     while ((s = ReadLine.Read(f)) != NULL) {
           ++line;
           char *t = skipspace(s + 1);
           switch (*s) {
             case 'C': {
                         char *p = strchr(t, ' ');
                         if (p) {
                            free(channelName);
                            channelName = strdup(compactspace(p));
                            *p = 0; // strips optional channel name
                            }
                         if (*t)
                            channelID = tChannelID::FromString(t);
                       }
                       break;
             case 'E': {
                         unsigned int EventID;
                         time_t StartTime;
                         int Duration;
                         unsigned int TableID = 0;
                         unsigned int Version = 0xFF;
                         int n = sscanf(t, "%u %ld %d %X %X", &EventID, &StartTime, &Duration, &TableID, &Version);
                         if (n >= 3 && n <= 5) {
                            ownEvent->SetEventID(EventID);
                            ownEvent->SetStartTime(StartTime);
                            ownEvent->SetDuration(Duration);
                            ownEvent->SetTableID(uchar(TableID));
                            ownEvent->SetVersion(uchar(Version));
                            }
                       }
                       break;
             case 'F': framesPerSecond = atod(t);
                       break;
             case 'L': lifetime = atoi(t);
                       break;
             case 'P': priority = atoi(t);
                       break;
             case '@': free(aux);
                       aux = strdup(t);
                       break;
             case '#': break; // comments are ignored
             default: if (!ownEvent->Parse(s)) {
                         esyslog("ERROR: EPG data problem in line %d", line);
                         return false;
                         }
                      break;
             }
           }
     return true;
     }
  return false;
}

bool cRecordingInfo::Write(FILE *f, const char *Prefix) const
{
  if (channelID.Valid())
     fprintf(f, "%sC %s%s%s\n", Prefix, *channelID.ToString(), channelName ? " " : "", channelName ? channelName : "");
  event->Dump(f, Prefix, true);
  fprintf(f, "%sF %s\n", Prefix, *dtoa(framesPerSecond, "%.10g"));
  fprintf(f, "%sP %d\n", Prefix, priority);
  fprintf(f, "%sL %d\n", Prefix, lifetime);
  if (aux)
     fprintf(f, "%s@ %s\n", Prefix, aux);
  return true;
}

bool cRecordingInfo::Read(void)
{
  bool Result = false;
  if (fileName) {
     FILE *f = fopen(fileName, "r");
     if (f) {
        if (Read(f))
           Result = true;
        else
           esyslog("ERROR: EPG data problem in file %s", fileName);
        fclose(f);
        }
     else if (errno != ENOENT)
        LOG_ERROR_STR(fileName);
     }
  return Result;
}

bool cRecordingInfo::Write(void) const
{
  bool Result = false;
  if (fileName) {
     cSafeFile f(fileName);
     if (f.Open()) {
        if (Write(f))
           Result = true;
        f.Close();
        }
     else
        LOG_ERROR_STR(fileName);
     }
  return Result;
}

// --- cRecording ------------------------------------------------------------

#define RESUME_NOT_INITIALIZED (-2)

struct tCharExchange { char a; char b; };
tCharExchange CharExchange[] = {
  { FOLDERDELIMCHAR,  '/' },
  { '/',  FOLDERDELIMCHAR },
  { ' ',  '_'    },
  // backwards compatibility:
  { '\'', '\''   },
  { '\'', '\x01' },
  { '/',  '\x02' },
  { 0, 0 }
  };

const char *InvalidChars = "\"\\/:*?|<>#";

bool NeedsConversion(const char *p)
{
  return DirectoryEncoding &&
         (strchr(InvalidChars, *p) // characters that can't be part of a Windows file/directory name
          || *p == '.' && (!*(p + 1) || *(p + 1) == FOLDERDELIMCHAR)); // Windows can't handle '.' at the end of file/directory names
}

char *ExchangeChars(char *s, bool ToFileSystem)
{
  char *p = s;
  while (*p) {
        if (DirectoryEncoding) {
           // Some file systems can't handle all characters, so we
           // have to take extra efforts to encode/decode them:
           if (ToFileSystem) {
              switch (*p) {
                     // characters that can be mapped to other characters:
                     case ' ': *p = '_'; break;
                     case FOLDERDELIMCHAR: *p = '/'; break;
                     case '/': *p = FOLDERDELIMCHAR; break;
                     // characters that have to be encoded:
                     default:
                       if (NeedsConversion(p)) {
                          int l = p - s;
                          if (char *NewBuffer = (char *)realloc(s, strlen(s) + 10)) {
                             s = NewBuffer;
                             p = s + l;
                             char buf[4];
                             sprintf(buf, "#%02X", (unsigned char)*p);
                             memmove(p + 2, p, strlen(p) + 1);
                             strncpy(p, buf, 3);
                             p += 2;
                             }
                          else
                             esyslog("ERROR: out of memory");
                          }
                     }
              }
           else {
              switch (*p) {
                // mapped characters:
                case '_': *p = ' '; break;
                case FOLDERDELIMCHAR: *p = '/'; break;
                case '/': *p = FOLDERDELIMCHAR; break;
                // encoded characters:
                case '#': {
                     if (strlen(p) > 2 && isxdigit(*(p + 1)) && isxdigit(*(p + 2))) {
                        char buf[3];
                        sprintf(buf, "%c%c", *(p + 1), *(p + 2));
                        uchar c = uchar(strtol(buf, NULL, 16));
                        if (c) {
                           *p = c;
                           memmove(p + 1, p + 3, strlen(p) - 2);
                           }
                        }
                     }
                     break;
                // backwards compatibility:
                case '\x01': *p = '\''; break;
                case '\x02': *p = '/';  break;
                case '\x03': *p = ':';  break;
                default: ;
                }
              }
           }
        else {
           for (struct tCharExchange *ce = CharExchange; ce->a && ce->b; ce++) {
               if (*p == (ToFileSystem ? ce->a : ce->b)) {
                  *p = ToFileSystem ? ce->b : ce->a;
                  break;
                  }
               }
           }
        p++;
        }
  return s;
}

char *LimitNameLengths(char *s, int PathMax, int NameMax)
{
  // Limits the total length of the directory path in 's' to PathMax, and each
  // individual directory name to NameMax. The lengths of characters that need
  // conversion when using 's' as a file name are taken into account accordingly.
  // If a directory name exceeds NameMax, it will be truncated. If the whole
  // directory path exceeds PathMax, individual directory names will be shortened
  // (from right to left) until the limit is met, or until the currently handled
  // directory name consists of only a single character. All operations are performed
  // directly on the given 's', which may become shorter (but never longer) than
  // the original value.
  // Returns a pointer to 's'.
  int Length = strlen(s);
  int PathLength = 0;
  // Collect the resulting lengths of each character:
  bool NameTooLong = false;
  int8_t a[Length];
  int n = 0;
  int NameLength = 0;
  for (char *p = s; *p; p++) {
      if (*p == FOLDERDELIMCHAR) {
         a[n] = -1; // FOLDERDELIMCHAR is a single character, neg. sign marks it
         NameTooLong |= NameLength > NameMax;
         NameLength = 0;
         PathLength += 1;
         }
      else if (NeedsConversion(p)) {
         a[n] = 3; // "#xx"
         NameLength += 3;
         PathLength += 3;
         }
      else {
         int8_t l = Utf8CharLen(p);
         a[n] = l;
         NameLength += l;
         PathLength += l;
         while (l-- > 1) {
               a[++n] = 0;
               p++;
               }
         }
      n++;
      }
  NameTooLong |= NameLength > NameMax;
  // Limit names to NameMax:
  if (NameTooLong) {
     while (n > 0) {
           // Calculate the length of the current name:
           int NameLength = 0;
           int i = n;
           int b = i;
           while (i-- > 0 && a[i] >= 0) {
                 NameLength += a[i];
                 b = i;
                 }
           // Shorten the name if necessary:
           if (NameLength > NameMax) {
              int l = 0;
              i = n;
              while (i-- > 0 && a[i] >= 0) {
                    l += a[i];
                    if (NameLength - l <= NameMax) {
                       memmove(s + i, s + n, Length - n + 1);
                       memmove(a + i, a + n, Length - n + 1);
                       Length -= n - i;
                       PathLength -= l;
                       break;
                       }
                    }
              }
           // Switch to the next name:
           n = b - 1;
           }
     }
  // Limit path to PathMax:
  n = Length;
  while (PathLength > PathMax && n > 0) {
        // Calculate how much to cut off the current name:
        int i = n;
        int b = i;
        int l = 0;
        while (--i > 0 && a[i - 1] >= 0) {
              if (a[i] > 0) {
                 l += a[i];
                 b = i;
                 if (PathLength - l <= PathMax)
                    break;
                 }
              }
        // Shorten the name if necessary:
        if (l > 0) {
           memmove(s + b, s + n, Length - n + 1);
           Length -= n - b;
           PathLength -= l;
           }
        // Switch to the next name:
        n = i - 1;
        }
  return s;
}

cRecording::cRecording(cTimer *Timer, const cEvent *Event)
{
  resume = RESUME_NOT_INITIALIZED;
  titleBuffer = NULL;
  sortBufferName = sortBufferTime = NULL;
  fileName = NULL;
  name = NULL;
  fileSizeMB = -1; // unknown
  channel = Timer->Channel()->Number();
  instanceId = InstanceId;
  isPesRecording = false;
  isOnVideoDirectoryFileSystem = -1; // unknown
  framesPerSecond = DEFAULTFRAMESPERSECOND;
  numFrames = -1;
  deleted = 0;
  // set up the actual name:
  const char *Title = Event ? Event->Title() : NULL;
  const char *Subtitle = Event ? Event->ShortText() : NULL;
  if (isempty(Title))
     Title = Timer->Channel()->Name();
  if (isempty(Subtitle))
     Subtitle = " ";
  const char *macroTITLE   = strstr(Timer->File(), TIMERMACRO_TITLE);
  const char *macroEPISODE = strstr(Timer->File(), TIMERMACRO_EPISODE);
  if (macroTITLE || macroEPISODE) {
     name = strdup(Timer->File());
     name = strreplace(name, TIMERMACRO_TITLE, Title);
     name = strreplace(name, TIMERMACRO_EPISODE, Subtitle);
     // avoid blanks at the end:
     int l = strlen(name);
     while (l-- > 2) {
           if (name[l] == ' ' && name[l - 1] != FOLDERDELIMCHAR)
              name[l] = 0;
           else
              break;
           }
     if (Timer->IsSingleEvent()) {
        Timer->SetFile(name); // this was an instant recording, so let's set the actual data
        Timers.SetModified();
        }
     }
  else if (Timer->IsSingleEvent() || !Setup.UseSubtitle)
     name = strdup(Timer->File());
  else
     name = strdup(cString::sprintf("%s%c%s", Timer->File(), FOLDERDELIMCHAR, Subtitle));
  // substitute characters that would cause problems in file names:
  strreplace(name, '\n', ' ');
  start = Timer->StartTime();
  priority = Timer->Priority();
  lifetime = Timer->Lifetime();
  // handle info:
  info = new cRecordingInfo(Timer->Channel(), Event);
  info->SetAux(Timer->Aux());
  info->priority = priority;
  info->lifetime = lifetime;
}

cRecording::cRecording(const char *FileName)
{
  resume = RESUME_NOT_INITIALIZED;
  fileSizeMB = -1; // unknown
  channel = -1;
  instanceId = -1;
  priority = MAXPRIORITY; // assume maximum in case there is no info file
  lifetime = MAXLIFETIME;
  isPesRecording = false;
  isOnVideoDirectoryFileSystem = -1; // unknown
  framesPerSecond = DEFAULTFRAMESPERSECOND;
  numFrames = -1;
  deleted = 0;
  titleBuffer = NULL;
  sortBufferName = sortBufferTime = NULL;
  FileName = fileName = strdup(FileName);
  if (*(fileName + strlen(fileName) - 1) == '/')
     *(fileName + strlen(fileName) - 1) = 0;
  if (strstr(FileName, cVideoDirectory::Name()) == FileName)
     FileName += strlen(cVideoDirectory::Name()) + 1;
  const char *p = strrchr(FileName, '/');

  name = NULL;
  info = new cRecordingInfo(fileName);
  if (p) {
     time_t now = time(NULL);
     struct tm tm_r;
     struct tm t = *localtime_r(&now, &tm_r); // this initializes the time zone in 't'
     t.tm_isdst = -1; // makes sure mktime() will determine the correct DST setting
     if (7 == sscanf(p + 1, DATAFORMATTS, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &channel, &instanceId)
      || 7 == sscanf(p + 1, DATAFORMATPES, &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &priority, &lifetime)) {
        t.tm_year -= 1900;
        t.tm_mon--;
        t.tm_sec = 0;
        start = mktime(&t);
        name = MALLOC(char, p - FileName + 1);
        strncpy(name, FileName, p - FileName);
        name[p - FileName] = 0;
        name = ExchangeChars(name, false);
        isPesRecording = instanceId < 0;
        }
     else
        return;
     GetResume();
     // read an optional info file:
     cString InfoFileName = cString::sprintf("%s%s", fileName, isPesRecording ? INFOFILESUFFIX ".vdr" : INFOFILESUFFIX);
     FILE *f = fopen(InfoFileName, "r");
     if (f) {
        if (!info->Read(f))
           esyslog("ERROR: EPG data problem in file %s", *InfoFileName);
        else if (!isPesRecording) {
           priority = info->priority;
           lifetime = info->lifetime;
           framesPerSecond = info->framesPerSecond;
           }
        fclose(f);
        }
     else if (errno == ENOENT)
        info->ownEvent->SetTitle(name);
     else
        LOG_ERROR_STR(*InfoFileName);
#ifdef SUMMARYFALLBACK
     // fall back to the old 'summary.vdr' if there was no 'info.vdr':
     if (isempty(info->Title())) {
        cString SummaryFileName = cString::sprintf("%s%s", fileName, SUMMARYFILESUFFIX);
        FILE *f = fopen(SummaryFileName, "r");
        if (f) {
           int line = 0;
           char *data[3] = { NULL };
           cReadLine ReadLine;
           char *s;
           while ((s = ReadLine.Read(f)) != NULL) {
                 if (*s || line > 1) {
                    if (data[line]) {
                       int len = strlen(s);
                       len += strlen(data[line]) + 1;
                       if (char *NewBuffer = (char *)realloc(data[line], len + 1)) {
                          data[line] = NewBuffer;
                          strcat(data[line], "\n");
                          strcat(data[line], s);
                          }
                       else
                          esyslog("ERROR: out of memory");
                       }
                    else
                       data[line] = strdup(s);
                    }
                 else
                    line++;
                 }
           fclose(f);
           if (!data[2]) {
              data[2] = data[1];
              data[1] = NULL;
              }
           else if (data[1] && data[2]) {
              // if line 1 is too long, it can't be the short text,
              // so assume the short text is missing and concatenate
              // line 1 and line 2 to be the long text:
              int len = strlen(data[1]);
              if (len > 80) {
                 if (char *NewBuffer = (char *)realloc(data[1], len + 1 + strlen(data[2]) + 1)) {
                    data[1] = NewBuffer;
                    strcat(data[1], "\n");
                    strcat(data[1], data[2]);
                    free(data[2]);
                    data[2] = data[1];
                    data[1] = NULL;
                    }
                 else
                    esyslog("ERROR: out of memory");
                 }
              }
           info->SetData(data[0], data[1], data[2]);
           for (int i = 0; i < 3; i ++)
               free(data[i]);
           }
        else if (errno != ENOENT)
           LOG_ERROR_STR(*SummaryFileName);
        }
#endif
     }
}

cRecording::~cRecording()
{
  free(titleBuffer);
  free(sortBufferName);
  free(sortBufferTime);
  free(fileName);
  free(name);
  delete info;
}

char *cRecording::StripEpisodeName(char *s, bool Strip)
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
  if (s1 && s2) {
     // To have folders sorted before plain recordings, the '/' s1 points to
     // is replaced by the character '1'. All other slashes will be replaced
     // by '0' in SortName() (see below), which will result in the desired
     // sequence:
     *s1 = '1';
     if (Strip) {
        s1++;
        memmove(s1, s2, t - s2 + 1);
        }
     }
  return s;
}

char *cRecording::SortName(void) const
{
  char **sb = (RecordingsSortMode == rsmName) ? &sortBufferName : &sortBufferTime;
  if (!*sb) {
     if (RecordingsSortMode == rsmTime && !Setup.RecordingDirs) {
        char buf[32];
        struct tm tm_r;
        strftime(buf, sizeof(buf), "%Y%m%d%H%I", localtime_r(&start, &tm_r));
        *sb = strdup(buf);
        }
     else {
        char *s = strdup(FileName() + strlen(cVideoDirectory::Name()));
        if (RecordingsSortMode != rsmName || Setup.AlwaysSortFoldersFirst)
           s = StripEpisodeName(s, RecordingsSortMode != rsmName);
        strreplace(s, '/', '0'); // some locales ignore '/' when sorting
        int l = strxfrm(NULL, s, 0) + 1;
        *sb = MALLOC(char, l);
        strxfrm(*sb, s, l);
        free(s);
        }
     }
  return *sb;
}

void cRecording::ClearSortName(void)
{
  free(sortBufferName);
  free(sortBufferTime);
  sortBufferName = sortBufferTime = NULL;
}

int cRecording::GetResume(void) const
{
  if (resume == RESUME_NOT_INITIALIZED) {
     cResumeFile ResumeFile(FileName(), isPesRecording);
     resume = ResumeFile.Read();
     }
  return resume;
}

int cRecording::Compare(const cListObject &ListObject) const
{
  cRecording *r = (cRecording *)&ListObject;
  return strcasecmp(SortName(), r->SortName());
}

bool cRecording::IsInPath(const char *Path)
{
  if (isempty(Path))
     return true;
  int l = strlen(Path);
  return strncmp(Path, name, l) == 0 && (name[l] == FOLDERDELIMCHAR);
}

cString cRecording::Folder(void) const
{
  if (char *s = strrchr(name, FOLDERDELIMCHAR))
     return cString(name, s);
  return "";
}

cString cRecording::BaseName(void) const
{
  if (char *s = strrchr(name, FOLDERDELIMCHAR))
     return cString(s + 1);
  return name;
}

const char *cRecording::FileName(void) const
{
  if (!fileName) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     const char *fmt = isPesRecording ? NAMEFORMATPES : NAMEFORMATTS;
     int ch = isPesRecording ? priority : channel;
     int ri = isPesRecording ? lifetime : instanceId;
     char *Name = LimitNameLengths(strdup(name), DirectoryPathMax - strlen(cVideoDirectory::Name()) - 1 - 42, DirectoryNameMax); // 42 = length of an actual recording directory name (generated with DATAFORMATTS) plus some reserve
     if (strcmp(Name, name) != 0)
        dsyslog("recording file name '%s' truncated to '%s'", name, Name);
     Name = ExchangeChars(Name, true);
     fileName = strdup(cString::sprintf(fmt, cVideoDirectory::Name(), Name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, ch, ri));
     free(Name);
     }
  return fileName;
}

const char *cRecording::Title(char Delimiter, bool NewIndicator, int Level) const
{
  char New = NewIndicator && IsNew() ? '*' : ' ';
  free(titleBuffer);
  titleBuffer = NULL;
  if (Level < 0 || Level == HierarchyLevels()) {
     struct tm tm_r;
     struct tm *t = localtime_r(&start, &tm_r);
     char *s;
     if (Level > 0 && (s = strrchr(name, FOLDERDELIMCHAR)) != NULL)
        s++;
     else
        s = name;
     cString Length("");
     if (NewIndicator) {
        int Minutes = max(0, (LengthInSeconds() + 30) / 60);
        Length = cString::sprintf("%c%d:%02d",
                   Delimiter,
                   Minutes / 60,
                   Minutes % 60
                   );
        }
     titleBuffer = strdup(cString::sprintf("%02d.%02d.%02d%c%02d:%02d%s%c%c%s",
                            t->tm_mday,
                            t->tm_mon + 1,
                            t->tm_year % 100,
                            Delimiter,
                            t->tm_hour,
                            t->tm_min,
                            *Length,
                            New,
                            Delimiter,
                            s));
     // let's not display a trailing FOLDERDELIMCHAR:
     if (!NewIndicator)
        stripspace(titleBuffer);
     s = &titleBuffer[strlen(titleBuffer) - 1];
     if (*s == FOLDERDELIMCHAR)
        *s = 0;
     }
  else if (Level < HierarchyLevels()) {
     const char *s = name;
     const char *p = s;
     while (*++s) {
           if (*s == FOLDERDELIMCHAR) {
              if (Level--)
                 p = s + 1;
              else
                 break;
              }
           }
     titleBuffer = MALLOC(char, s - p + 3);
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
  cString p = cVideoDirectory::PrefixVideoFileName(FileName(), Prefix);
  if (*p) {
     free(fileName);
     fileName = strdup(p);
     return fileName;
     }
  return NULL;
}

int cRecording::HierarchyLevels(void) const
{
  const char *s = name;
  int level = 0;
  while (*++s) {
        if (*s == FOLDERDELIMCHAR)
           level++;
        }
  return level;
}

bool cRecording::IsEdited(void) const
{
  const char *s = strrchr(name, FOLDERDELIMCHAR);
  s = !s ? name : s + 1;
  return *s == '%';
}

bool cRecording::IsOnVideoDirectoryFileSystem(void) const
{
  if (isOnVideoDirectoryFileSystem < 0)
     isOnVideoDirectoryFileSystem = cVideoDirectory::IsOnVideoDirectoryFileSystem(FileName());
  return isOnVideoDirectoryFileSystem;
}

bool cRecording::HasMarks(void)
{
  return access(cMarks::MarksFileName(this), F_OK) == 0;
}

bool cRecording::DeleteMarks(void)
{
  if (remove(cMarks::MarksFileName(this)) < 0) {
     if (errno != ENOENT) {
        LOG_ERROR_STR(fileName);
        return false;
        }
     }
  return true;
}

void cRecording::ReadInfo(void)
{
  info->Read();
  priority = info->priority;
  lifetime = info->lifetime;
  framesPerSecond = info->framesPerSecond;
}

bool cRecording::WriteInfo(const char *OtherFileName)
{
  cString InfoFileName = cString::sprintf("%s%s", OtherFileName ? OtherFileName : FileName(), isPesRecording ? INFOFILESUFFIX ".vdr" : INFOFILESUFFIX);
  cSafeFile f(InfoFileName);
  if (f.Open()) {
     info->Write(f);
     f.Close();
     }
  else
     LOG_ERROR_STR(*InfoFileName);
  return true;
}

void cRecording::SetStartTime(time_t Start)
{
  start = Start;
  free(fileName);
  fileName = NULL;
}

bool cRecording::ChangePriorityLifetime(int NewPriority, int NewLifetime)
{
  if (NewPriority != Priority() || NewLifetime != Lifetime()) {
     dsyslog("changing priority/lifetime of '%s' to %d/%d", Name(), NewPriority, NewLifetime);
     if (IsPesRecording()) {
        cString OldFileName = FileName();
        priority = NewPriority;
        lifetime = NewLifetime;
        free(fileName);
        fileName = NULL;
        cString NewFileName = FileName();
        if (!cVideoDirectory::RenameVideoFile(OldFileName, NewFileName))
           return false;
        info->SetFileName(NewFileName);
        }
     else {
        priority = info->priority = NewPriority;
        lifetime = info->lifetime = NewLifetime;
        if (!WriteInfo())
           return false;
        }
     Recordings.ChangeState();
     Recordings.TouchUpdate();
     }
  return true;
}

bool cRecording::ChangeName(const char *NewName)
{
  if (strcmp(NewName, Name())) {
     dsyslog("changing name of '%s' to '%s'", Name(), NewName);
     cString OldName = Name();
     cString OldFileName = FileName();
     free(fileName);
     fileName = NULL;
     free(name);
     name = strdup(NewName);
     cString NewFileName = FileName();
     if (!(MakeDirs(NewFileName, true) && cVideoDirectory::MoveVideoFile(OldFileName, NewFileName))) {
        free(name);
        name = strdup(OldName);
        free(fileName);
        fileName = strdup(OldFileName);
        return false;
        }
     isOnVideoDirectoryFileSystem = -1; // it might have been moved to a different file system
     ClearSortName();
     Recordings.ChangeState();
     Recordings.TouchUpdate();
     }
  return true;
}

bool cRecording::Delete(void)
{
  bool result = true;
  char *NewName = strdup(FileName());
  char *ext = strrchr(NewName, '.');
  if (ext && strcmp(ext, RECEXT) == 0) {
     strncpy(ext, DELEXT, strlen(ext));
     if (access(NewName, F_OK) == 0) {
        // the new name already exists, so let's remove that one first:
        isyslog("removing recording '%s'", NewName);
        cVideoDirectory::RemoveVideoFile(NewName);
        }
     isyslog("deleting recording '%s'", FileName());
     if (access(FileName(), F_OK) == 0) {
        result = cVideoDirectory::RenameVideoFile(FileName(), NewName);
        cRecordingUserCommand::InvokeCommand(RUC_DELETERECORDING, NewName);
        }
     else {
        isyslog("recording '%s' vanished", FileName());
        result = true; // well, we were going to delete it, anyway
        }
     }
  free(NewName);
  return result;
}

bool cRecording::Remove(void)
{
  // let's do a final safety check here:
  if (!endswith(FileName(), DELEXT)) {
     esyslog("attempt to remove recording %s", FileName());
     return false;
     }
  isyslog("removing recording %s", FileName());
  return cVideoDirectory::RemoveVideoFile(FileName());
}

bool cRecording::Undelete(void)
{
  bool result = true;
  char *NewName = strdup(FileName());
  char *ext = strrchr(NewName, '.');
  if (ext && strcmp(ext, DELEXT) == 0) {
     strncpy(ext, RECEXT, strlen(ext));
     if (access(NewName, F_OK) == 0) {
        // the new name already exists, so let's not remove that one:
        esyslog("ERROR: attempt to undelete '%s', while recording '%s' exists", FileName(), NewName);
        result = false;
        }
     else {
        isyslog("undeleting recording '%s'", FileName());
        if (access(FileName(), F_OK) == 0)
           result = cVideoDirectory::RenameVideoFile(FileName(), NewName);
        else {
           isyslog("deleted recording '%s' vanished", FileName());
           result = false;
           }
        }
     }
  free(NewName);
  return result;
}

int cRecording::IsInUse(void) const
{
  int Use = ruNone;
  if (cRecordControls::GetRecordControl(FileName()))
     Use |= ruTimer;
  if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), FileName()) == 0)
     Use |= ruReplay;
  Use |= RecordingsHandler.GetUsage(FileName());
  return Use;
}

void cRecording::ResetResume(void) const
{
  resume = RESUME_NOT_INITIALIZED;
}

int cRecording::NumFrames(void) const
{
  if (numFrames < 0) {
     int nf = cIndexFile::GetLength(FileName(), IsPesRecording());
     if (time(NULL) - LastModifiedTime(cIndexFile::IndexFileName(FileName(), IsPesRecording())) < MININDEXAGE)
        return nf; // check again later for ongoing recordings
     numFrames = nf;
     }
  return numFrames;
}

int cRecording::LengthInSeconds(void) const
{
  int nf = NumFrames();
  if (nf >= 0)
     return int(nf / FramesPerSecond());
  return -1;
}

int cRecording::FileSizeMB(void) const
{
  if (fileSizeMB < 0) {
     int fs = DirSizeMB(FileName());
     if (time(NULL) - LastModifiedTime(cIndexFile::IndexFileName(FileName(), IsPesRecording())) < MININDEXAGE)
        return fs; // check again later for ongoing recordings
     fileSizeMB = fs;
     }
  return fileSizeMB;
}

// --- cRecordings -----------------------------------------------------------

cRecordings Recordings;

char *cRecordings::updateFileName = NULL;

cRecordings::cRecordings(bool Deleted)
:cThread("video directory scanner")
{
  deleted = Deleted;
  initial = true;
  lastUpdate = 0;
  state = 0;
}

cRecordings::~cRecordings()
{
  Cancel(3);
}

void cRecordings::Action(void)
{
  Refresh();
}

const char *cRecordings::UpdateFileName(void)
{
  if (!updateFileName)
     updateFileName = strdup(AddDirectory(cVideoDirectory::Name(), ".update"));
  return updateFileName;
}

void cRecordings::Refresh(bool Foreground)
{
  lastUpdate = time(NULL); // doing this first to make sure we don't miss anything
  initial = Count() == 0; // no name checking if the list is initially empty
  if (deleted) {
     Lock();
     Clear();
     ChangeState();
     Unlock();
     }
  ScanVideoDir(cVideoDirectory::Name(), Foreground);
}

bool cRecordings::ScanVideoDir(const char *DirName, bool Foreground, int LinkLevel, int DirLevel)
{
  bool DoChangeState = false;
  // Find any new recordings:
  cReadDir d(DirName);
  struct dirent *e;
  while ((Foreground || Running()) && (e = d.Next()) != NULL) {
        cString buffer = AddDirectory(DirName, e->d_name);
        struct stat st;
        if (lstat(buffer, &st) == 0) {
           int Link = 0;
           if (S_ISLNK(st.st_mode)) {
              if (LinkLevel > MAX_LINK_LEVEL) {
                 isyslog("max link level exceeded - not scanning %s", *buffer);
                 continue;
                 }
              Link = 1;
              if (stat(buffer, &st) != 0)
                 continue;
              }
           if (S_ISDIR(st.st_mode)) {
              if (endswith(buffer, deleted ? DELEXT : RECEXT)) {
                 if (deleted || initial || !GetByName(buffer)) {
                    cRecording *r = new cRecording(buffer);
                    if (r->Name()) {
                       r->NumFrames(); // initializes the numFrames member
                       r->FileSizeMB(); // initializes the fileSizeMB member
                       r->IsOnVideoDirectoryFileSystem(); // initializes the isOnVideoDirectoryFileSystem member
                       if (deleted)
                          r->deleted = time(NULL);
                       Lock();
                       Add(r);
                       if (initial)
                          ChangeState();
                       else
                          DoChangeState = true;
                       Unlock();
                       }
                    else
                       delete r;
                    }
                 }
              else
                 DoChangeState |= ScanVideoDir(buffer, Foreground, LinkLevel + Link, DirLevel + 1);
              }
           }
        }
  // Handle any vanished recordings:
  if (!deleted && !initial && DirLevel == 0) {
     for (cRecording *recording = First(); recording; ) {
         cRecording *r = recording;
         recording = Next(recording);
         if (access(r->FileName(), F_OK) != 0) {
            Lock();
            Del(r, false);
            VanishedRecordings.Add(r);
            DoChangeState = true;
            Unlock();
            }
         }
     }
  if (DoChangeState && DirLevel == 0)
     ChangeState();
  return DoChangeState;
}

bool cRecordings::StateChanged(int &State)
{
  int NewState = state;
  bool Result = State != NewState;
  State = state;
  return Result;
}

void cRecordings::TouchUpdate(void)
{
  bool needsUpdate = NeedsUpdate();
  TouchFile(UpdateFileName());
  if (!needsUpdate)
     lastUpdate = time(NULL); // make sure we don't trigger ourselves
}

bool cRecordings::NeedsUpdate(void)
{
  time_t lastModified = LastModifiedTime(UpdateFileName());
  if (lastModified > time(NULL))
     return false; // somebody's clock isn't running correctly
  return lastUpdate < lastModified;
}

bool cRecordings::Update(bool Wait)
{
  if (Wait) {
     Refresh(true);
     return Count() > 0;
     }
  else
     Start();
  return false;
}

cRecording *cRecordings::GetByName(const char *FileName)
{
  if (FileName) {
     LOCK_THREAD;
     for (cRecording *recording = First(); recording; recording = Next(recording)) {
         if (strcmp(recording->FileName(), FileName) == 0)
            return recording;
         }
     }
  return NULL;
}

void cRecordings::AddByName(const char *FileName, bool TriggerUpdate)
{
  LOCK_THREAD;
  cRecording *recording = GetByName(FileName);
  if (!recording) {
     recording = new cRecording(FileName);
     Add(recording);
     ChangeState();
     if (TriggerUpdate)
        TouchUpdate();
     }
}

void cRecordings::DelByName(const char *FileName)
{
  LOCK_THREAD;
  cRecording *recording = GetByName(FileName);
  cRecording *dummy = NULL;
  if (!recording)
     recording = dummy = new cRecording(FileName); // allows us to use a FileName that is not in the Recordings list
  cThreadLock DeletedRecordingsLock(&DeletedRecordings);
  if (!dummy)
     Del(recording, false);
  char *ext = strrchr(recording->fileName, '.');
  if (ext) {
     strncpy(ext, DELEXT, strlen(ext));
     if (access(recording->FileName(), F_OK) == 0) {
        recording->deleted = time(NULL);
        DeletedRecordings.Add(recording);
        recording = NULL; // to prevent it from being deleted below
        }
     }
  delete recording;
  ChangeState();
  TouchUpdate();
}

void cRecordings::UpdateByName(const char *FileName)
{
  LOCK_THREAD;
  cRecording *recording = GetByName(FileName);
  if (recording)
     recording->ReadInfo();
}

int cRecordings::TotalFileSizeMB(void)
{
  int size = 0;
  LOCK_THREAD;
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      int FileSizeMB = recording->FileSizeMB();
      if (FileSizeMB > 0 && recording->IsOnVideoDirectoryFileSystem())
         size += FileSizeMB;
      }
  return size;
}

double cRecordings::MBperMinute(void)
{
  int size = 0;
  int length = 0;
  LOCK_THREAD;
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (recording->IsOnVideoDirectoryFileSystem()) {
         int FileSizeMB = recording->FileSizeMB();
         if (FileSizeMB > 0) {
            int LengthInSeconds = recording->LengthInSeconds();
            if (LengthInSeconds > 0) {
               if (LengthInSeconds / FileSizeMB < LIMIT_SECS_PER_MB_RADIO) { // don't count radio recordings
                  size += FileSizeMB;
                  length += LengthInSeconds;
                  }
               }
            }
         }
      }
  return (size && length) ? double(size) * 60 / length : -1;
}

int cRecordings::PathIsInUse(const char *Path)
{
  LOCK_THREAD;
  int Use = ruNone;
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (recording->IsInPath(Path))
         Use |= recording->IsInUse();
      }
  return Use;
}

int cRecordings::GetNumRecordingsInPath(const char *Path)
{
  LOCK_THREAD;
  int n = 0;
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (recording->IsInPath(Path))
         n++;
      }
  return n;
}

bool cRecordings::MoveRecordings(const char *OldPath, const char *NewPath)
{
  if (OldPath && NewPath && strcmp(OldPath, NewPath)) {
     LOCK_THREAD;
     dsyslog("moving '%s' to '%s'", OldPath, NewPath);
     for (cRecording *recording = First(); recording; recording = Next(recording)) {
         if (recording->IsInPath(OldPath)) {
            const char *p = recording->Name() + strlen(OldPath);
            cString NewName = cString::sprintf("%s%s", NewPath, p);
            if (!recording->ChangeName(NewName))
               return false;
            ChangeState();
            }
         }
     }
  return true;
}

void cRecordings::ResetResume(const char *ResumeFileName)
{
  LOCK_THREAD;
  for (cRecording *recording = First(); recording; recording = Next(recording)) {
      if (!ResumeFileName || strncmp(ResumeFileName, recording->FileName(), strlen(recording->FileName())) == 0)
         recording->ResetResume();
      }
  ChangeState();
}

void cRecordings::ClearSortNames(void)
{
  LOCK_THREAD;
  for (cRecording *recording = First(); recording; recording = Next(recording))
      recording->ClearSortName();
}

// --- cDirCopier ------------------------------------------------------------

class cDirCopier : public cThread {
private:
  cString dirNameSrc;
  cString dirNameDst;
  bool error;
  bool suspensionLogged;
  bool Throttled(void);
  virtual void Action(void);
public:
  cDirCopier(const char *DirNameSrc, const char *DirNameDst);
  virtual ~cDirCopier();
  void Stop(void);
  bool Error(void) { return error; }
  };

cDirCopier::cDirCopier(const char *DirNameSrc, const char *DirNameDst)
:cThread("file copier", true)
{
  dirNameSrc = DirNameSrc;
  dirNameDst = DirNameDst;
  error = true; // prepare for the worst!
  suspensionLogged = false;
}

cDirCopier::~cDirCopier()
{
  Stop();
}

bool cDirCopier::Throttled(void)
{
  if (cIoThrottle::Engaged()) {
     if (!suspensionLogged) {
        dsyslog("suspending copy thread");
        suspensionLogged = true;
        }
     return true;
     }
  else if (suspensionLogged) {
     dsyslog("resuming copy thread");
     suspensionLogged = false;
     }
  return false;
}

void cDirCopier::Action(void)
{
  if (DirectoryOk(dirNameDst, true)) {
     cReadDir d(dirNameSrc);
     if (d.Ok()) {
        dsyslog("copying directory '%s' to '%s'", *dirNameSrc, *dirNameDst);
        dirent *e = NULL;
        cString FileNameSrc;
        cString FileNameDst;
        int From = -1;
        int To = -1;
        size_t BufferSize = BUFSIZ;
        while (Running()) {
              // Suspend cutting if we have severe throughput problems:
              if (Throttled()) {
                 cCondWait::SleepMs(100);
                 continue;
                 }
              // Copy all files in the source directory to the destination directory:
              if (e) {
                 // We're currently copying a file:
                 uchar Buffer[BufferSize];
                 size_t Read = safe_read(From, Buffer, sizeof(Buffer));
                 if (Read > 0) {
                    size_t Written = safe_write(To, Buffer, Read);
                    if (Written != Read) {
                       esyslog("ERROR: can't write to destination file '%s': %m", *FileNameDst);
                       break;
                       }
                    }
                 else if (Read == 0) { // EOF on From
                    e = NULL; // triggers switch to next entry
                    if (fsync(To) < 0) {
                       esyslog("ERROR: can't sync destination file '%s': %m", *FileNameDst);
                       break;
                       }
                    if (close(From) < 0) {
                       esyslog("ERROR: can't close source file '%s': %m", *FileNameSrc);
                       break;
                       }
                    if (close(To) < 0) {
                       esyslog("ERROR: can't close destination file '%s': %m", *FileNameDst);
                       break;
                       }
                    // Plausibility check:
                    off_t FileSizeSrc = FileSize(FileNameSrc);
                    off_t FileSizeDst = FileSize(FileNameDst);
                    if (FileSizeSrc != FileSizeDst) {
                       esyslog("ERROR: file size discrepancy: %" PRId64 " != %" PRId64, FileSizeSrc, FileSizeDst);
                       break;
                       }
                    }
                 else {
                    esyslog("ERROR: can't read from source file '%s': %m", *FileNameSrc);
                    break;
                    }
                 }
              else if ((e = d.Next()) != NULL) {
                 // We're switching to the next directory entry:
                 FileNameSrc = AddDirectory(dirNameSrc, e->d_name);
                 FileNameDst = AddDirectory(dirNameDst, e->d_name);
                 struct stat st;
                 if (stat(FileNameSrc, &st) < 0) {
                    esyslog("ERROR: can't access source file '%s': %m", *FileNameSrc);
                    break;
                    }
                 if (!(S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))) {
                    esyslog("ERROR: source file '%s' is neither a regular file nor a symbolic link", *FileNameSrc);
                    break;
                    }
                 dsyslog("copying file '%s' to '%s'", *FileNameSrc, *FileNameDst);
                 BufferSize = max(size_t(st.st_blksize * 10), size_t(BUFSIZ));
                 if (access(FileNameDst, F_OK) == 0) {
                    esyslog("ERROR: destination file '%s' already exists", *FileNameDst);
                    break;
                    }
                 if ((From = open(FileNameSrc, O_RDONLY)) < 0) {
                    esyslog("ERROR: can't open source file '%s': %m", *FileNameSrc);
                    break;
                    }
                 if ((To = open(FileNameDst, O_WRONLY | O_CREAT | O_EXCL, DEFFILEMODE)) < 0) {
                    esyslog("ERROR: can't open destination file '%s': %m", *FileNameDst);
                    close(From);
                    break;
                    }
                 }
              else {
                 // We're done:
                 dsyslog("done copying directory '%s' to '%s'", *dirNameSrc, *dirNameDst);
                 error = false;
                 return;
                 }
              }
        close(From); // just to be absolutely sure
        close(To);
        esyslog("ERROR: copying directory '%s' to '%s' ended prematurely", *dirNameSrc, *dirNameDst);
        }
     else
        esyslog("ERROR: can't open '%s'", *dirNameSrc);
     }
  else
     esyslog("ERROR: can't access '%s'", *dirNameDst);
}

void cDirCopier::Stop(void)
{
  Cancel(3);
  if (error) {
     cVideoDirectory::RemoveVideoFile(dirNameDst);
     Recordings.AddByName(dirNameSrc);
     Recordings.DelByName(dirNameDst);
     }
}

// --- cRecordingsHandlerEntry -----------------------------------------------

class cRecordingsHandlerEntry : public cListObject {
private:
  int usage;
  cString fileNameSrc;
  cString fileNameDst;
  cCutter *cutter;
  cDirCopier *copier;
  void ClearPending(void) { usage &= ~ruPending; }
public:
  cRecordingsHandlerEntry(int Usage, const char *FileNameSrc, const char *FileNameDst);
  ~cRecordingsHandlerEntry();
  int Usage(const char *FileName = NULL) const;
  const char *FileNameSrc(void) const { return fileNameSrc; }
  const char *FileNameDst(void) const { return fileNameDst; }
  bool Active(bool &Error);
  };

cRecordingsHandlerEntry::cRecordingsHandlerEntry(int Usage, const char *FileNameSrc, const char *FileNameDst)
{
  usage = Usage;
  fileNameSrc = FileNameSrc;
  fileNameDst = FileNameDst;
  cutter = NULL;
  copier = NULL;
}

cRecordingsHandlerEntry::~cRecordingsHandlerEntry()
{
  delete cutter;
  delete copier;
}

int cRecordingsHandlerEntry::Usage(const char *FileName) const
{
  int u = usage;
  if (FileName && *FileName) {
     if (strcmp(FileName, fileNameSrc) == 0)
        u |= ruSrc;
     else if (strcmp(FileName, fileNameDst) == 0)
        u |= ruDst;
     }
  return u;
}

bool cRecordingsHandlerEntry::Active(bool &Error)
{
  bool CopierFinishedOk = false;
  // First test whether there is an ongoing operation:
  if (cutter) {
     if (cutter->Active())
        return true;
     Error |= cutter->Error();
     delete cutter;
     cutter = NULL;
     }
  else if (copier) {
     if (copier->Active())
        return true;
     Error |= copier->Error();
     CopierFinishedOk = !copier->Error();
     delete copier;
     copier = NULL;
     }
  // Now check if there is something to start:
  if ((Usage() & ruPending) != 0) {
     if ((Usage() & ruCut) != 0) {
        cutter = new cCutter(FileNameSrc());
        cutter->Start();
        }
     else if ((Usage() & (ruMove | ruCopy)) != 0) {
        copier = new cDirCopier(FileNameSrc(), FileNameDst());
        copier->Start();
        }
     ClearPending();
     Recordings.ChangeState();
     return true;
     }
  // Clean up:
  if (CopierFinishedOk && (Usage() & ruMove) != 0) {
     cRecording Recording(FileNameSrc());
     if (Recording.Delete())
        Recordings.DelByName(Recording.FileName());
     }
  Recordings.ChangeState();
  Recordings.TouchUpdate();
  return false;
}

// --- cRecordingsHandler ----------------------------------------------------

cRecordingsHandler RecordingsHandler;

cRecordingsHandler::cRecordingsHandler(void)
{
  finished = true;
  error = false;
}

cRecordingsHandler::~cRecordingsHandler()
{
}

cRecordingsHandlerEntry *cRecordingsHandler::Get(const char *FileName)
{
  if (FileName && *FileName) {
     for (cRecordingsHandlerEntry *r = operations.First(); r; r = operations.Next(r)) {
         if (strcmp(FileName, r->FileNameSrc()) == 0 || strcmp(FileName, r->FileNameDst()) == 0)
            return r;
         }
     }
  return NULL;
}

bool cRecordingsHandler::Add(int Usage, const char *FileNameSrc, const char *FileNameDst)
{
  dsyslog("recordings handler add %d '%s' '%s'", Usage, FileNameSrc, FileNameDst);
  cMutexLock MutexLock(&mutex);
  if (Usage == ruCut || Usage == ruMove || Usage == ruCopy) {
     if (FileNameSrc && *FileNameSrc) {
        if (Usage == ruCut || FileNameDst && *FileNameDst) {
           cString fnd;
           if (Usage == ruCut && !FileNameDst)
              FileNameDst = fnd = cCutter::EditedFileName(FileNameSrc);
           if (!Get(FileNameSrc) && !Get(FileNameDst)) {
              Usage |= ruPending;
              operations.Add(new cRecordingsHandlerEntry(Usage, FileNameSrc, FileNameDst));
              finished = false;
              Active(); // start it right away if possible
              Recordings.ChangeState();
              return true;
              }
           else
              esyslog("ERROR: file name already present in recordings handler add %d '%s' '%s'", Usage, FileNameSrc, FileNameDst);
           }
        else
           esyslog("ERROR: missing dst file name in recordings handler add %d '%s' '%s'", Usage, FileNameSrc, FileNameDst);
        }
     else
        esyslog("ERROR: missing src file name in recordings handler add %d '%s' '%s'", Usage, FileNameSrc, FileNameDst);
     }
  else
     esyslog("ERROR: invalid usage in recordings handler add %d '%s' '%s'", Usage, FileNameSrc, FileNameDst);
  return false;
}

void cRecordingsHandler::Del(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  if (cRecordingsHandlerEntry *r = Get(FileName)) {
     operations.Del(r);
     Recordings.ChangeState();
     }
}

void cRecordingsHandler::DelAll(void)
{
  cMutexLock MutexLock(&mutex);
  operations.Clear();
  Recordings.ChangeState();
}

int cRecordingsHandler::GetUsage(const char *FileName)
{
  cMutexLock MutexLock(&mutex);
  if (cRecordingsHandlerEntry *r = Get(FileName))
     return r->Usage(FileName);
  return ruNone;
}

bool cRecordingsHandler::Active(void)
{
  cMutexLock MutexLock(&mutex);
  while (cRecordingsHandlerEntry *r = operations.First()) {
        if (r->Active(error))
           return true;
        else
           operations.Del(r);
        }
  return false;
}

bool cRecordingsHandler::Finished(bool &Error)
{
  cMutexLock MutexLock(&mutex);
  if (!finished && operations.Count() == 0) {
     finished = true;
     Error = error;
     error = false;
     return true;
     }
  return false;
}

// --- cMark -----------------------------------------------------------------

double MarkFramesPerSecond = DEFAULTFRAMESPERSECOND;
cMutex MutexMarkFramesPerSecond;

cMark::cMark(int Position, const char *Comment, double FramesPerSecond)
{
  position = Position;
  comment = Comment;
  framesPerSecond = FramesPerSecond;
}

cMark::~cMark()
{
}

cString cMark::ToText(void)
{
  return cString::sprintf("%s%s%s\n", *IndexToHMSF(position, true, framesPerSecond), Comment() ? " " : "", Comment() ? Comment() : "");
}

bool cMark::Parse(const char *s)
{
  comment = NULL;
  framesPerSecond = MarkFramesPerSecond;
  position = HMSFToIndex(s, framesPerSecond);
  const char *p = strchr(s, ' ');
  if (p) {
     p = skipspace(p);
     if (*p)
        comment = strdup(p);
     }
  return true;
}

bool cMark::Save(FILE *f)
{
  return fprintf(f, "%s", *ToText()) > 0;
}

// --- cMarks ----------------------------------------------------------------

cString cMarks::MarksFileName(const cRecording *Recording)
{
  return AddDirectory(Recording->FileName(), Recording->IsPesRecording() ? MARKSFILESUFFIX ".vdr" : MARKSFILESUFFIX);
}

bool cMarks::Load(const char *RecordingFileName, double FramesPerSecond, bool IsPesRecording)
{
  cMutexLock MutexLock(this);
  recordingFileName = RecordingFileName;
  fileName = AddDirectory(RecordingFileName, IsPesRecording ? MARKSFILESUFFIX ".vdr" : MARKSFILESUFFIX);
  framesPerSecond = FramesPerSecond;
  isPesRecording = IsPesRecording;
  nextUpdate = 0;
  lastFileTime = -1; // the first call to Load() must take place!
  lastChange = 0;
  return Update();
}

bool cMarks::Update(void)
{
  cMutexLock MutexLock(this);
  time_t t = time(NULL);
  if (t > nextUpdate && *fileName) {
     time_t LastModified = LastModifiedTime(fileName);
     if (LastModified != lastFileTime) // change detected, or first run
        lastChange = LastModified > 0 ? LastModified : t;
     int d = t - lastChange;
     if (d < 60)
        d = 1; // check frequently if the file has just been modified
     else if (d < 3600)
        d = 10; // older files are checked less frequently
     else
        d /= 360; // phase out checking for very old files
     nextUpdate = t + d;
     if (LastModified != lastFileTime) { // change detected, or first run
        lastFileTime = LastModified;
        if (lastFileTime == t)
           lastFileTime--; // make sure we don't miss updates in the remaining second
        cMutexLock MutexLock(&MutexMarkFramesPerSecond);
        MarkFramesPerSecond = framesPerSecond;
        if (cConfig<cMark>::Load(fileName)) {
           Align();
           Sort();
           return true;
           }
        }
     }
  return false;
}

bool cMarks::Save(void)
{
  cMutexLock MutexLock(this);
  if (cConfig<cMark>::Save()) {
     lastFileTime = LastModifiedTime(fileName);
     return true;
     }
  return false;
}

void cMarks::Align(void)
{
  cMutexLock MutexLock(this);
  cIndexFile IndexFile(recordingFileName, false, isPesRecording);
  for (cMark *m = First(); m; m = Next(m)) {
      int p = IndexFile.GetClosestIFrame(m->Position());
      if (int d = m->Position() - p) {
         isyslog("aligned editing mark %s to %s (off by %d frame%s)", *IndexToHMSF(m->Position(), true, framesPerSecond), *IndexToHMSF(p, true, framesPerSecond), d, abs(d) > 1 ? "s" : "");
         m->SetPosition(p);
         }
      }
}

void cMarks::Sort(void)
{
  cMutexLock MutexLock(this);
  for (cMark *m1 = First(); m1; m1 = Next(m1)) {
      for (cMark *m2 = Next(m1); m2; m2 = Next(m2)) {
          if (m2->Position() < m1->Position()) {
             swap(m1->position, m2->position);
             swap(m1->comment, m2->comment);
             }
          }
      }
}

void cMarks::Add(int Position)
{
  cMutexLock MutexLock(this);
  cConfig<cMark>::Add(new cMark(Position, NULL, framesPerSecond));
  Sort();
}

cMark *cMarks::Get(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi)) {
      if (mi->Position() == Position)
         return mi;
      }
  return NULL;
}

cMark *cMarks::GetPrev(int Position)
{
  for (cMark *mi = Last(); mi; mi = Prev(mi)) {
      if (mi->Position() < Position)
         return mi;
      }
  return NULL;
}

cMark *cMarks::GetNext(int Position)
{
  for (cMark *mi = First(); mi; mi = Next(mi)) {
      if (mi->Position() > Position)
         return mi;
      }
  return NULL;
}

cMark *cMarks::GetNextBegin(cMark *EndMark)
{
  cMark *BeginMark = EndMark ? Next(EndMark) : First();
  if (BeginMark && EndMark && BeginMark->Position() == EndMark->Position()) {
     while (cMark *NextMark = Next(BeginMark)) {
           if (BeginMark->Position() == NextMark->Position()) { // skip Begin/End at the same position
              if (!(BeginMark = Next(NextMark)))
                 break;
              }
           else
              break;
           }
     }
  return BeginMark;
}

cMark *cMarks::GetNextEnd(cMark *BeginMark)
{
  if (!BeginMark)
     return NULL;
  cMark *EndMark = Next(BeginMark);
  if (EndMark && BeginMark && BeginMark->Position() == EndMark->Position()) {
     while (cMark *NextMark = Next(EndMark)) {
           if (EndMark->Position() == NextMark->Position()) { // skip End/Begin at the same position
              if (!(EndMark = Next(NextMark)))
                 break;
              }
           else
              break;
           }
     }
  return EndMark;
}

int cMarks::GetNumSequences(void)
{
  cMutexLock MutexLock(this);
  int NumSequences = 0;
  if (cMark *BeginMark = GetNextBegin()) {
     while (cMark *EndMark = GetNextEnd(BeginMark)) {
           NumSequences++;
           BeginMark = GetNextBegin(EndMark);
           }
     if (BeginMark) {
        NumSequences++; // the last sequence had no actual "end" mark
        if (NumSequences == 1 && BeginMark->Position() == 0)
           NumSequences = 0; // there is only one actual "begin" mark at offset zero, and no actual "end" mark
        }
     }
  return NumSequences;
}

// --- cRecordingUserCommand -------------------------------------------------

const char *cRecordingUserCommand::command = NULL;

void cRecordingUserCommand::InvokeCommand(const char *State, const char *RecordingFileName, const char *SourceFileName)
{
  if (command) {
    cString cmd;
    if (SourceFileName)
       cmd = cString::sprintf("%s %s \"%s\" \"%s\"", command, State, *strescape(RecordingFileName, "\\\"$"), *strescape(SourceFileName, "\\\"$"));
    else
       cmd = cString::sprintf("%s %s \"%s\"", command, State, *strescape(RecordingFileName, "\\\"$"));
    isyslog("executing '%s'", *cmd);
    SystemExec(cmd);
  }
}

// --- cIndexFileGenerator ---------------------------------------------------

#define IFG_BUFFER_SIZE KILOBYTE(100)

class cIndexFileGenerator : public cThread {
private:
  cString recordingName;
  bool update;
protected:
  virtual void Action(void);
public:
  cIndexFileGenerator(const char *RecordingName, bool Update = false);
  ~cIndexFileGenerator();
  };

cIndexFileGenerator::cIndexFileGenerator(const char *RecordingName, bool Update)
:cThread("index file generator")
,recordingName(RecordingName)
{
  update = Update;
  Start();
}

cIndexFileGenerator::~cIndexFileGenerator()
{
  Cancel(3);
}

void cIndexFileGenerator::Action(void)
{
  bool IndexFileComplete = false;
  bool IndexFileWritten = false;
  bool Rewind = false;
  cFileName FileName(recordingName, false);
  cUnbufferedFile *ReplayFile = FileName.Open();
  cRingBufferLinear Buffer(IFG_BUFFER_SIZE, MIN_TS_PACKETS_FOR_FRAME_DETECTOR * TS_SIZE);
  cPatPmtParser PatPmtParser;
  cFrameDetector FrameDetector;
  cIndexFile IndexFile(recordingName, true, false, false, true);
  int BufferChunks = KILOBYTE(1); // no need to read a lot at the beginning when parsing PAT/PMT
  off_t FileSize = 0;
  off_t FrameOffset = -1;
  uint16_t FileNumber = 1;
  off_t FileOffset = 0;
  int Last = -1;
  if (update) {
     // Look for current index and position to end of it if present:
     bool Independent;
     int Length;
     Last = IndexFile.Last();
     if (Last >= 0 && !IndexFile.Get(Last, &FileNumber, &FileOffset, &Independent, &Length))
        Last = -1; // reset Last if an error occurred
     if (Last >= 0) {
        Rewind = true;
        isyslog("updating index file");
        }
     else
        isyslog("generating index file");
     }
  Skins.QueueMessage(mtInfo, tr("Regenerating index file"));
  bool Stuffed = false;
  while (Running()) {
        // Rewind input file:
        if (Rewind) {
           ReplayFile = FileName.SetOffset(FileNumber, FileOffset);
           FileSize = FileOffset;
           Buffer.Clear();
           Rewind = false;
           }
        // Process data:
        int Length;
        uchar *Data = Buffer.Get(Length);
        if (Data) {
           if (FrameDetector.Synced()) {
              // Step 3 - generate the index:
              if (TsPid(Data) == PATPID)
                 FrameOffset = FileSize; // the PAT/PMT is at the beginning of an I-frame
              int Processed = FrameDetector.Analyze(Data, Length);
              if (Processed > 0) {
                 if (FrameDetector.NewFrame()) {
                    if (IndexFileWritten || Last < 0) // check for first frame and do not write if in update mode
                       IndexFile.Write(FrameDetector.IndependentFrame(), FileName.Number(), FrameOffset >= 0 ? FrameOffset : FileSize);
                    FrameOffset = -1;
                    IndexFileWritten = true;
                    }
                 FileSize += Processed;
                 Buffer.Del(Processed);
                 }
              }
           else if (PatPmtParser.Vpid()) {
              // Step 2 - sync FrameDetector:
              int Processed = FrameDetector.Analyze(Data, Length);
              if (Processed > 0) {
                 if (FrameDetector.Synced()) {
                    // Synced FrameDetector, so rewind for actual processing:
                    Rewind = true;
                    }
                 Buffer.Del(Processed);
                 }
              }
           else {
              // Step 1 - parse PAT/PMT:
              uchar *p = Data;
              while (Length >= TS_SIZE) {
                    int Pid = TsPid(p);
                    if (Pid == PATPID)
                       PatPmtParser.ParsePat(p, TS_SIZE);
                    else if (PatPmtParser.IsPmtPid(Pid))
                       PatPmtParser.ParsePmt(p, TS_SIZE);
                    Length -= TS_SIZE;
                    p += TS_SIZE;
                    if (PatPmtParser.Vpid()) {
                       // Found Vpid, so rewind to sync FrameDetector:
                       FrameDetector.SetPid(PatPmtParser.Vpid(), PatPmtParser.Vtype());
                       BufferChunks = IFG_BUFFER_SIZE;
                       Rewind = true;
                       break;
                       }
                    }
              Buffer.Del(p - Data);
              }
           }
        // Read data:
        else if (ReplayFile) {
           int Result = Buffer.Read(ReplayFile, BufferChunks);
           if (Result == 0) { // EOF
              if (Buffer.Available() > 0 && !Stuffed) {
                 // So the last call to Buffer.Get() returned NULL, but there is still
                 // data in the buffer, and we're at the end of the current TS file.
                 // The remaining data in the buffer is less than what's needed for the
                 // frame detector to analyze frames, so we need to put some stuffing
                 // packets into the buffer to flush out the rest of the data (otherwise
                 // any frames within the remaining data would not be seen here):
                 uchar StuffingPacket[TS_SIZE] = { TS_SYNC_BYTE, 0xFF };
                 for (int i = 0; i <= MIN_TS_PACKETS_FOR_FRAME_DETECTOR; i++)
                     Buffer.Put(StuffingPacket, sizeof(StuffingPacket));
                 Stuffed = true;
                 }
              else {
                 ReplayFile = FileName.NextFile();
                 FileSize = 0;
                 FrameOffset = -1;
                 Buffer.Clear();
                 Stuffed = false;
                 }
              }
           }
        // Recording has been processed:
        else {
           IndexFileComplete = true;
           break;
           }
        }
  if (IndexFileComplete) {
     if (IndexFileWritten) {
        cRecordingInfo RecordingInfo(recordingName);
        if (RecordingInfo.Read()) {
           if (FrameDetector.FramesPerSecond() > 0 && !DoubleEqual(RecordingInfo.FramesPerSecond(), FrameDetector.FramesPerSecond())) {
              RecordingInfo.SetFramesPerSecond(FrameDetector.FramesPerSecond());
              RecordingInfo.Write();
              Recordings.UpdateByName(recordingName);
              }
           }
        Skins.QueueMessage(mtInfo, tr("Index file regeneration complete"));
        return;
        }
     else
        Skins.QueueMessage(mtError, tr("Index file regeneration failed!"));
     }
  // Delete the index file if the recording has not been processed entirely:
  IndexFile.Delete();
}

// --- cIndexFile ------------------------------------------------------------

#define INDEXFILESUFFIX     "/index"

// The maximum time to wait before giving up while catching up on an index file:
#define MAXINDEXCATCHUP    8 // number of retries
#define INDEXCATCHUPWAIT 100 // milliseconds

struct tIndexPes {
  uint32_t offset;
  uchar type;
  uchar number;
  uint16_t reserved;
  };

struct tIndexTs {
  uint64_t offset:40; // up to 1TB per file (not using off_t here - must definitely be exactly 64 bit!)
  int reserved:7;     // reserved for future use
  int independent:1;  // marks frames that can be displayed by themselves (for trick modes)
  uint16_t number:16; // up to 64K files per recording
  tIndexTs(off_t Offset, bool Independent, uint16_t Number)
  {
    offset = Offset;
    reserved = 0;
    independent = Independent;
    number = Number;
  }
  };

#define MAXWAITFORINDEXFILE     10 // max. time to wait for the regenerated index file (seconds)
#define INDEXFILECHECKINTERVAL 500 // ms between checks for existence of the regenerated index file
#define INDEXFILETESTINTERVAL   10 // ms between tests for the size of the index file in case of pausing live video

cIndexFile::cIndexFile(const char *FileName, bool Record, bool IsPesRecording, bool PauseLive, bool Update)
:resumeFile(FileName, IsPesRecording)
{
  f = -1;
  size = 0;
  last = -1;
  index = NULL;
  isPesRecording = IsPesRecording;
  indexFileGenerator = NULL;
  if (FileName) {
     fileName = IndexFileName(FileName, isPesRecording);
     if (!Record && PauseLive) {
        // Wait until the index file contains at least two frames:
        time_t tmax = time(NULL) + MAXWAITFORINDEXFILE;
        while (time(NULL) < tmax && FileSize(fileName) < off_t(2 * sizeof(tIndexTs)))
              cCondWait::SleepMs(INDEXFILETESTINTERVAL);
        }
     int delta = 0;
     if (!Record && access(fileName, R_OK) != 0) {
        // Index file doesn't exist, so try to regenerate it:
        if (!isPesRecording) { // sorry, can only do this for TS recordings
           resumeFile.Delete(); // just in case
           indexFileGenerator = new cIndexFileGenerator(FileName);
           // Wait until the index file exists:
           time_t tmax = time(NULL) + MAXWAITFORINDEXFILE;
           do {
              cCondWait::SleepMs(INDEXFILECHECKINTERVAL); // start with a sleep, to give it a head start
              } while (access(fileName, R_OK) != 0 && time(NULL) < tmax);
           }
        }
     if (access(fileName, R_OK) == 0) {
        struct stat buf;
        if (stat(fileName, &buf) == 0) {
           delta = int(buf.st_size % sizeof(tIndexTs));
           if (delta) {
              delta = sizeof(tIndexTs) - delta;
              esyslog("ERROR: invalid file size (%" PRId64 ") in '%s'", buf.st_size, *fileName);
              }
           last = int((buf.st_size + delta) / sizeof(tIndexTs) - 1);
           if ((!Record || Update) && last >= 0) {
              size = last + 1;
              index = MALLOC(tIndexTs, size);
              if (index) {
                 f = open(fileName, O_RDONLY);
                 if (f >= 0) {
                    if (safe_read(f, index, size_t(buf.st_size)) != buf.st_size) {
                       esyslog("ERROR: can't read from file '%s'", *fileName);
                       free(index);
                       index = NULL;
                       }
                    else if (isPesRecording)
                       ConvertFromPes(index, size);
                    if (!index || time(NULL) - buf.st_mtime >= MININDEXAGE) {
                       close(f);
                       f = -1;
                       }
                    // otherwise we don't close f here, see CatchUp()!
                    }
                 else
                    LOG_ERROR_STR(*fileName);
                 }
              else
                 esyslog("ERROR: can't allocate %zd bytes for index '%s'", size * sizeof(tIndexTs), *fileName);
              }
           }
        else
           LOG_ERROR;
        }
     else if (!Record)
        isyslog("missing index file %s", *fileName);
     if (Record) {
        if ((f = open(fileName, O_WRONLY | O_CREAT | O_APPEND, DEFFILEMODE)) >= 0) {
           if (delta) {
              esyslog("ERROR: padding index file with %d '0' bytes", delta);
              while (delta--)
                    writechar(f, 0);
              }
           }
        else
           LOG_ERROR_STR(*fileName);
        }
     }
}

cIndexFile::~cIndexFile()
{
  if (f >= 0)
     close(f);
  free(index);
  delete indexFileGenerator;
}

cString cIndexFile::IndexFileName(const char *FileName, bool IsPesRecording)
{
  return cString::sprintf("%s%s", FileName, IsPesRecording ? INDEXFILESUFFIX ".vdr" : INDEXFILESUFFIX);
}

void cIndexFile::ConvertFromPes(tIndexTs *IndexTs, int Count)
{
  tIndexPes IndexPes;
  while (Count-- > 0) {
        memcpy(&IndexPes, IndexTs, sizeof(IndexPes));
        IndexTs->offset = IndexPes.offset;
        IndexTs->independent = IndexPes.type == 1; // I_FRAME
        IndexTs->number = IndexPes.number;
        IndexTs++;
        }
}

void cIndexFile::ConvertToPes(tIndexTs *IndexTs, int Count)
{
  tIndexPes IndexPes;
  while (Count-- > 0) {
        IndexPes.offset = uint32_t(IndexTs->offset);
        IndexPes.type = uchar(IndexTs->independent ? 1 : 2); // I_FRAME : "not I_FRAME" (exact frame type doesn't matter)
        IndexPes.number = uchar(IndexTs->number);
        IndexPes.reserved = 0;
        memcpy(IndexTs, &IndexPes, sizeof(*IndexTs));
        IndexTs++;
        }
}

bool cIndexFile::CatchUp(int Index)
{
  // returns true unless something really goes wrong, so that 'index' becomes NULL
  if (index && f >= 0) {
     cMutexLock MutexLock(&mutex);
     // Note that CatchUp() is triggered even if Index is 'last' (and thus valid).
     // This is done to make absolutely sure we don't miss any data at the very end.
     for (int i = 0; i <= MAXINDEXCATCHUP && (Index < 0 || Index >= last); i++) {
         struct stat buf;
         if (fstat(f, &buf) == 0) {
            int newLast = int(buf.st_size / sizeof(tIndexTs) - 1);
            if (newLast > last) {
               int NewSize = size;
               if (NewSize <= newLast) {
                  NewSize *= 2;
                  if (NewSize <= newLast)
                     NewSize = newLast + 1;
                  }
               if (tIndexTs *NewBuffer = (tIndexTs *)realloc(index, NewSize * sizeof(tIndexTs))) {
                  size = NewSize;
                  index = NewBuffer;
                  int offset = (last + 1) * sizeof(tIndexTs);
                  int delta = (newLast - last) * sizeof(tIndexTs);
                  if (lseek(f, offset, SEEK_SET) == offset) {
                     if (safe_read(f, &index[last + 1], delta) != delta) {
                        esyslog("ERROR: can't read from index");
                        free(index);
                        index = NULL;
                        close(f);
                        f = -1;
                        break;
                        }
                     if (isPesRecording)
                        ConvertFromPes(&index[last + 1], newLast - last);
                     last = newLast;
                     }
                  else
                     LOG_ERROR_STR(*fileName);
                  }
               else {
                  esyslog("ERROR: can't realloc() index");
                  break;
                  }
               }
            }
         else
            LOG_ERROR_STR(*fileName);
         if (Index < last)
            break;
         cCondVar CondVar;
         CondVar.TimedWait(mutex, INDEXCATCHUPWAIT);
         }
     }
  return index != NULL;
}

bool cIndexFile::Write(bool Independent, uint16_t FileNumber, off_t FileOffset)
{
  if (f >= 0) {
     tIndexTs i(FileOffset, Independent, FileNumber);
     if (isPesRecording)
        ConvertToPes(&i, 1);
     if (safe_write(f, &i, sizeof(i)) < 0) {
        LOG_ERROR_STR(*fileName);
        close(f);
        f = -1;
        return false;
        }
     last++;
     }
  return f >= 0;
}

bool cIndexFile::Get(int Index, uint16_t *FileNumber, off_t *FileOffset, bool *Independent, int *Length)
{
  if (CatchUp(Index)) {
     if (Index >= 0 && Index <= last) {
        *FileNumber = index[Index].number;
        *FileOffset = index[Index].offset;
        if (Independent)
           *Independent = index[Index].independent;
        if (Length) {
           if (Index < last) {
              uint16_t fn = index[Index + 1].number;
              off_t fo = index[Index + 1].offset;
              if (fn == *FileNumber)
                 *Length = int(fo - *FileOffset);
              else
                 *Length = -1; // this means "everything up to EOF" (the buffer's Read function will act accordingly)
              }
           else
              *Length = -1;
           }
        return true;
        }
     }
  return false;
}

int cIndexFile::GetNextIFrame(int Index, bool Forward, uint16_t *FileNumber, off_t *FileOffset, int *Length)
{
  if (CatchUp()) {
     int d = Forward ? 1 : -1;
     for (;;) {
         Index += d;
         if (Index >= 0 && Index <= last) {
            if (index[Index].independent) {
               uint16_t fn;
               if (!FileNumber)
                  FileNumber = &fn;
               off_t fo;
               if (!FileOffset)
                  FileOffset = &fo;
               *FileNumber = index[Index].number;
               *FileOffset = index[Index].offset;
               if (Length) {
                  if (Index < last) {
                     uint16_t fn = index[Index + 1].number;
                     off_t fo = index[Index + 1].offset;
                     if (fn == *FileNumber)
                        *Length = int(fo - *FileOffset);
                     else
                        *Length = -1; // this means "everything up to EOF" (the buffer's Read function will act accordingly)
                     }
                  else
                     *Length = -1;
                  }
               return Index;
               }
            }
         else
            break;
         }
     }
  return -1;
}

int cIndexFile::GetClosestIFrame(int Index)
{
  if (last > 0) {
     Index = constrain(Index, 0, last);
     if (index[Index].independent)
        return Index;
     int il = Index - 1;
     int ih = Index + 1;
     for (;;) {
         if (il >= 0) {
            if (index[il].independent)
               return il;
            il--;
            }
         else if (ih > last)
            break;
         if (ih <= last) {
            if (index[ih].independent)
               return ih;
            ih++;
            }
         else if (il < 0)
            break;
         }
     }
  return 0;
}

int cIndexFile::Get(uint16_t FileNumber, off_t FileOffset)
{
  if (CatchUp()) {
     //TODO implement binary search!
     int i;
     for (i = 0; i <= last; i++) {
         if (index[i].number > FileNumber || (index[i].number == FileNumber) && off_t(index[i].offset) >= FileOffset)
            break;
         }
     return i;
     }
  return -1;
}

bool cIndexFile::IsStillRecording(void)
{
  return f >= 0;
}

void cIndexFile::Delete(void)
{
  if (*fileName) {
     dsyslog("deleting index file '%s'", *fileName);
     if (f >= 0) {
        close(f);
        f = -1;
        }
     unlink(fileName);
     }
}

int cIndexFile::GetLength(const char *FileName, bool IsPesRecording)
{
  struct stat buf;
  cString s = IndexFileName(FileName, IsPesRecording);
  if (*s && stat(s, &buf) == 0)
     return buf.st_size / (IsPesRecording ? sizeof(tIndexTs) : sizeof(tIndexPes));
  return -1;
}

bool GenerateIndex(const char *FileName, bool Update)
{
  if (DirectoryOk(FileName)) {
     cRecording Recording(FileName);
     if (Recording.Name()) {
        if (!Recording.IsPesRecording()) {
           cString IndexFileName = AddDirectory(FileName, INDEXFILESUFFIX);
           if (!Update)
              unlink(IndexFileName);
           cIndexFileGenerator *IndexFileGenerator = new cIndexFileGenerator(FileName, Update);
           while (IndexFileGenerator->Active())
                 cCondWait::SleepMs(INDEXFILECHECKINTERVAL);
           if (access(IndexFileName, R_OK) == 0)
              return true;
           else
              fprintf(stderr, "cannot create '%s'\n", *IndexFileName);
           }
        else
           fprintf(stderr, "'%s' is not a TS recording\n", FileName);
        }
     else
        fprintf(stderr, "'%s' is not a recording\n", FileName);
     }
  else
     fprintf(stderr, "'%s' is not a directory\n", FileName);
  return false;
}

// --- cFileName -------------------------------------------------------------

#define MAXFILESPERRECORDINGPES 255
#define RECORDFILESUFFIXPES     "/%03d.vdr"
#define MAXFILESPERRECORDINGTS  65535
#define RECORDFILESUFFIXTS      "/%05d.ts"
#define RECORDFILESUFFIXLEN 20 // some additional bytes for safety...

cFileName::cFileName(const char *FileName, bool Record, bool Blocking, bool IsPesRecording)
{
  file = NULL;
  fileNumber = 0;
  record = Record;
  blocking = Blocking;
  isPesRecording = IsPesRecording;
  // Prepare the file name:
  fileName = MALLOC(char, strlen(FileName) + RECORDFILESUFFIXLEN);
  if (!fileName) {
     esyslog("ERROR: can't copy file name '%s'", fileName);
     return;
     }
  strcpy(fileName, FileName);
  pFileNumber = fileName + strlen(fileName);
  SetOffset(1);
}

cFileName::~cFileName()
{
  Close();
  free(fileName);
}

bool cFileName::GetLastPatPmtVersions(int &PatVersion, int &PmtVersion)
{
  if (fileName && !isPesRecording) {
     // Find the last recording file:
     int Number = 1;
     for (; Number <= MAXFILESPERRECORDINGTS + 1; Number++) { // +1 to correctly set Number in case there actually are that many files
         sprintf(pFileNumber, RECORDFILESUFFIXTS, Number);
         if (access(fileName, F_OK) != 0) { // file doesn't exist
            Number--;
            break;
            }
         }
     for (; Number > 0; Number--) {
         // Search for a PAT packet from the end of the file:
         cPatPmtParser PatPmtParser;
         sprintf(pFileNumber, RECORDFILESUFFIXTS, Number);
         int fd = open(fileName, O_RDONLY | O_LARGEFILE, DEFFILEMODE);
         if (fd >= 0) {
            off_t pos = lseek(fd, -TS_SIZE, SEEK_END);
            while (pos >= 0) {
                  // Read and parse the PAT/PMT:
                  uchar buf[TS_SIZE];
                  while (read(fd, buf, sizeof(buf)) == sizeof(buf)) {
                        if (buf[0] == TS_SYNC_BYTE) {
                           int Pid = TsPid(buf);
                           if (Pid == PATPID)
                              PatPmtParser.ParsePat(buf, sizeof(buf));
                           else if (PatPmtParser.IsPmtPid(Pid)) {
                              PatPmtParser.ParsePmt(buf, sizeof(buf));
                              if (PatPmtParser.GetVersions(PatVersion, PmtVersion)) {
                                 close(fd);
                                 return true;
                                 }
                              }
                           else
                              break; // PAT/PMT is always in one sequence
                           }
                        else
                           return false;
                        }
                  pos = lseek(fd, pos - TS_SIZE, SEEK_SET);
                  }
            close(fd);
            }
         else
            break;
         }
     }
  return false;
}

cUnbufferedFile *cFileName::Open(void)
{
  if (!file) {
     int BlockingFlag = blocking ? 0 : O_NONBLOCK;
     if (record) {
        dsyslog("recording to '%s'", fileName);
        file = cVideoDirectory::OpenVideoFile(fileName, O_RDWR | O_CREAT | O_LARGEFILE | BlockingFlag);
        if (!file)
           LOG_ERROR_STR(fileName);
        }
     else {
        if (access(fileName, R_OK) == 0) {
           dsyslog("playing '%s'", fileName);
           file = cUnbufferedFile::Create(fileName, O_RDONLY | O_LARGEFILE | BlockingFlag);
           if (!file)
              LOG_ERROR_STR(fileName);
           }
        else if (errno != ENOENT)
           LOG_ERROR_STR(fileName);
        }
     }
  return file;
}

void cFileName::Close(void)
{
  if (file) {
     if (file->Close() < 0)
        LOG_ERROR_STR(fileName);
     delete file;
     file = NULL;
     }
}

cUnbufferedFile *cFileName::SetOffset(int Number, off_t Offset)
{
  if (fileNumber != Number)
     Close();
  int MaxFilesPerRecording = isPesRecording ? MAXFILESPERRECORDINGPES : MAXFILESPERRECORDINGTS;
  if (0 < Number && Number <= MaxFilesPerRecording) {
     fileNumber = uint16_t(Number);
     sprintf(pFileNumber, isPesRecording ? RECORDFILESUFFIXPES : RECORDFILESUFFIXTS, fileNumber);
     if (record) {
        if (access(fileName, F_OK) == 0) {
           // file exists, check if it has non-zero size
           struct stat buf;
           if (stat(fileName, &buf) == 0) {
              if (buf.st_size != 0)
                 return SetOffset(Number + 1); // file exists and has non zero size, let's try next suffix
              else {
                 // zero size file, remove it
                 dsyslog("cFileName::SetOffset: removing zero-sized file %s", fileName);
                 unlink(fileName);
                 }
              }
           else
              return SetOffset(Number + 1); // error with fstat - should not happen, just to be on the safe side
           }
        else if (errno != ENOENT) { // something serious has happened
           LOG_ERROR_STR(fileName);
           return NULL;
           }
        // found a non existing file suffix
        }
     if (Open() >= 0) {
        if (!record && Offset >= 0 && file && file->Seek(Offset, SEEK_SET) != Offset) {
           LOG_ERROR_STR(fileName);
           return NULL;
           }
        }
     return file;
     }
  esyslog("ERROR: max number of files (%d) exceeded", MaxFilesPerRecording);
  return NULL;
}

cUnbufferedFile *cFileName::NextFile(void)
{
  return SetOffset(fileNumber + 1);
}

// --- Index stuff -----------------------------------------------------------

cString IndexToHMSF(int Index, bool WithFrame, double FramesPerSecond)
{
  const char *Sign = "";
  if (Index < 0) {
     Index = -Index;
     Sign = "-";
     }
  double Seconds;
  int f = int(modf((Index + 0.5) / FramesPerSecond, &Seconds) * FramesPerSecond);
  int s = int(Seconds);
  int m = s / 60 % 60;
  int h = s / 3600;
  s %= 60;
  return cString::sprintf(WithFrame ? "%s%d:%02d:%02d.%02d" : "%s%d:%02d:%02d", Sign, h, m, s, f);
}

int HMSFToIndex(const char *HMSF, double FramesPerSecond)
{
  int h, m, s, f = 0;
  int n = sscanf(HMSF, "%d:%d:%d.%d", &h, &m, &s, &f);
  if (n == 1)
     return h; // plain frame number
  if (n >= 3)
     return int(round((h * 3600 + m * 60 + s) * FramesPerSecond)) + f;
  return 0;
}

int SecondsToFrames(int Seconds, double FramesPerSecond)
{
  return int(round(Seconds * FramesPerSecond));
}

// --- ReadFrame -------------------------------------------------------------

int ReadFrame(cUnbufferedFile *f, uchar *b, int Length, int Max)
{
  if (Length == -1)
     Length = Max; // this means we read up to EOF (see cIndex)
  else if (Length > Max) {
     esyslog("ERROR: frame larger than buffer (%d > %d)", Length, Max);
     Length = Max;
     }
  int r = f->Read(b, Length);
  if (r < 0)
     LOG_ERROR;
  return r;
}

// --- Recordings Sort Mode --------------------------------------------------

eRecordingsSortMode RecordingsSortMode = rsmName;

bool HasRecordingsSortMode(const char *Directory)
{
  return access(AddDirectory(Directory, SORTMODEFILE), R_OK) == 0;
}

void GetRecordingsSortMode(const char *Directory)
{
  RecordingsSortMode = rsmName;
  if (FILE *f = fopen(AddDirectory(Directory, SORTMODEFILE), "r")) {
     char buf[8];
     if (fgets(buf, sizeof(buf), f))
        RecordingsSortMode = eRecordingsSortMode(constrain(atoi(buf), 0, int(rsmTime)));
     fclose(f);
     }
}

void SetRecordingsSortMode(const char *Directory, eRecordingsSortMode SortMode)
{
  if (FILE *f = fopen(AddDirectory(Directory, SORTMODEFILE), "w")) {
     fputs(cString::sprintf("%d\n", SortMode), f);
     fclose(f);
     }
}

void IncRecordingsSortMode(const char *Directory)
{
  GetRecordingsSortMode(Directory);
  RecordingsSortMode = eRecordingsSortMode(int(RecordingsSortMode) + 1);
  if (RecordingsSortMode > rsmTime)
     RecordingsSortMode = eRecordingsSortMode(0);
  SetRecordingsSortMode(Directory, RecordingsSortMode);
}
