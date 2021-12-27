/*
 * videodir.c: Functions to maintain the video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.c 4.1.1.1 2021/12/27 11:18:40 kls Exp $
 */

#include "videodir.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "recording.h"
#include "tools.h"

cMutex cVideoDirectory::mutex;
cString cVideoDirectory::name;
cVideoDirectory *cVideoDirectory::current = NULL;

cVideoDirectory::cVideoDirectory(void)
{
  mutex.Lock();
  delete current;
  current = this;
  mutex.Unlock();
}

cVideoDirectory::~cVideoDirectory()
{
  mutex.Lock();
  current = NULL;
  mutex.Unlock();
}

cVideoDirectory *cVideoDirectory::Current(void)
{
  mutex.Lock();
  if (!current)
     new cVideoDirectory;
  mutex.Unlock();
  return current;
}

void cVideoDirectory::Destroy(void)
{
  delete current;
}

int cVideoDirectory::FreeMB(int *UsedMB)
{
  return FreeDiskSpaceMB(Name(), UsedMB);
}

const char *cVideoDirectory::Name(void)
{
  return name;
}

void cVideoDirectory::SetName(const char *Name)
{
  name = Name;
}

bool cVideoDirectory::Register(const char *FileName)
{
  // Incoming name must be in base video directory:
  if (strstr(FileName, Name()) != FileName) {
     esyslog("ERROR: %s not in %s", FileName, Name());
     errno = ENOENT; // must set 'errno' - any ideas for a better value?
     return false;
     }
  return true;
}

bool cVideoDirectory::Rename(const char *OldName, const char *NewName)
{
  dsyslog("renaming '%s' to '%s'", OldName, NewName);
  if (rename(OldName, NewName) == -1) {
     LOG_ERROR_STR(NewName);
     return false;
     }
  return true;
}

bool cVideoDirectory::Move(const char *FromName, const char *ToName)
{
  dsyslog("moving '%s' to '%s'", FromName, ToName);
  if (EntriesOnSameFileSystem(FromName, ToName)) {
     if (rename(FromName, ToName) == -1) {
        LOG_ERROR_STR(ToName);
        return false;
        }
     }
  else
     return RecordingsHandler.Add(ruMove, FromName, ToName);
  return true;
}

bool cVideoDirectory::Remove(const char *Name)
{
  return RemoveFileOrDir(Name);
}

void cVideoDirectory::Cleanup(const char *IgnoreFiles[])
{
  RemoveEmptyDirectories(Name(), false, IgnoreFiles);
}

bool cVideoDirectory::Contains(const char *Name)
{
  return EntriesOnSameFileSystem(this->Name(), Name);
}

cUnbufferedFile *cVideoDirectory::OpenVideoFile(const char *FileName, int Flags)
{
  if (Current()->Register(FileName))
     return cUnbufferedFile::Create(FileName, Flags, DEFFILEMODE);
  return NULL;
}

bool cVideoDirectory::RenameVideoFile(const char *OldName, const char *NewName)
{
  return Current()->Rename(OldName, NewName);
}

bool cVideoDirectory::MoveVideoFile(const char *FromName, const char *ToName)
{
  return Current()->Move(FromName, ToName);
}

bool cVideoDirectory::RemoveVideoFile(const char *FileName)
{
  return Current()->Remove(FileName);
}

bool cVideoDirectory::VideoFileSpaceAvailable(int SizeMB)
{
  return Current()->FreeMB() >= SizeMB;
}

int cVideoDirectory::VideoDiskSpace(int *FreeMB, int *UsedMB)
{
  int used = 0;
  int free = Current()->FreeMB(&used);
  LOCK_DELETEDRECORDINGS_READ;
  int deleted = DeletedRecordings->TotalFileSizeMB();
  if (deleted > used)
     deleted = used; // let's not get beyond 100%
  free += deleted;
  used -= deleted;
  if (FreeMB)
     *FreeMB = free;
  if (UsedMB)
     *UsedMB = used;
  return (free + used) ? round(double(used) * 100 / (free + used)) : 0;
}

cString cVideoDirectory::PrefixVideoFileName(const char *FileName, char Prefix)
{
  char PrefixedName[strlen(FileName) + 2];

  const char *p = FileName + strlen(FileName); // p points at the terminating 0
  int n = 2;
  while (p-- > FileName && n > 0) {
        if (*p == '/') {
           if (--n == 0) {
              int l = p - FileName + 1;
              strncpy(PrefixedName, FileName, l);
              PrefixedName[l] = Prefix;
              strcpy(PrefixedName + l + 1, p + 1);
              return PrefixedName;
              }
           }
        }
  return NULL;
}

void cVideoDirectory::RemoveEmptyVideoDirectories(const char *IgnoreFiles[])
{
  Current()->Cleanup(IgnoreFiles);
}

bool cVideoDirectory::IsOnVideoDirectoryFileSystem(const char *FileName)
{
  return Current()->Contains(FileName);
}

// --- cVideoDiskUsage -------------------------------------------------------

#define DISKSPACECHEK     5 // seconds between disk space checks
#define MB_PER_MINUTE 25.75 // this is just an estimate!

int cVideoDiskUsage::state = 0;
time_t cVideoDiskUsage::lastChecked = 0;
int cVideoDiskUsage::usedPercent = 0;
int cVideoDiskUsage::freeMB = 0;
int cVideoDiskUsage::freeMinutes = 0;

bool cVideoDiskUsage::HasChanged(int &State)
{
  if (time(NULL) - lastChecked > DISKSPACECHEK) {
     int FreeMB;
     int UsedPercent = cVideoDirectory::VideoDiskSpace(&FreeMB);
     if (FreeMB != freeMB) {
        usedPercent = UsedPercent;
        freeMB = FreeMB;
        LOCK_RECORDINGS_READ;
        double MBperMinute = Recordings->MBperMinute();
        if (MBperMinute <= 0)
           MBperMinute = MB_PER_MINUTE;
        freeMinutes = int(double(FreeMB) / MBperMinute);
        state++;
        }
     lastChecked = time(NULL);
     }
  if (State != state) {
     State = state;
     return true;
     }
  return false;
}

cString cVideoDiskUsage::String(void)
{
  HasChanged(state);
  return cString::sprintf("%s %d%%  -  %2d:%02d %s", tr("Disk"), usedPercent, freeMinutes / 60, freeMinutes % 60, tr("free"));
}
