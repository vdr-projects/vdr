/*
 * videodir.c: Functions to maintain a distributed video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.c 2.0 2008/02/16 13:00:03 kls Exp $
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

const char *VideoDirectory = VIDEODIR;

class cVideoDirectory {
private:
  char *name, *stored, *adjusted;
  int length, number, digits;
public:
  cVideoDirectory(void);
  ~cVideoDirectory();
  int FreeMB(int *UsedMB = NULL);
  const char *Name(void) { return name ? name : VideoDirectory; }
  const char *Stored(void) { return stored; }
  int Length(void) { return length; }
  bool IsDistributed(void) { return name != NULL; }
  bool Next(void);
  void Store(void);
  const char *Adjust(const char *FileName);
  };

cVideoDirectory::cVideoDirectory(void)
{
  length = strlen(VideoDirectory);
  name = (VideoDirectory[length - 1] == '0') ? strdup(VideoDirectory) : NULL;
  stored = adjusted = NULL;
  number = -1;
  digits = 0;
}

cVideoDirectory::~cVideoDirectory()
{
  free(name);
  free(stored);
  free(adjusted);
}

int cVideoDirectory::FreeMB(int *UsedMB)
{
  return FreeDiskSpaceMB(name ? name : VideoDirectory, UsedMB);
}

bool cVideoDirectory::Next(void)
{
  if (name) {
     if (number < 0) {
        int l = length;
        while (l-- > 0 && isdigit(name[l]))
              ;
        l++;
        digits = length - l;
        int n = atoi(&name[l]);
        if (n == 0)
           number = n;
        else
           return false; // base video directory must end with zero
        }
     if (++number > 0) {
        char buf[16];
        if (sprintf(buf, "%0*d", digits, number) == digits) {
           strcpy(&name[length - digits], buf);
           return DirectoryOk(name);
           }
        }
     }
  return false;
}

void cVideoDirectory::Store(void)
{
  if (name) {
     free(stored);
     stored = strdup(name);
     }
}

const char *cVideoDirectory::Adjust(const char *FileName)
{
  if (stored) {
     free(adjusted);
     adjusted = strdup(FileName);
     return strncpy(adjusted, stored, length);
     }
  return NULL;
}

cUnbufferedFile *OpenVideoFile(const char *FileName, int Flags)
{
  const char *ActualFileName = FileName;

  // Incoming name must be in base video directory:
  if (strstr(FileName, VideoDirectory) != FileName) {
     esyslog("ERROR: %s not in %s", FileName, VideoDirectory);
     errno = ENOENT; // must set 'errno' - any ideas for a better value?
     return NULL;
     }
  // Are we going to create a new file?
  if ((Flags & O_CREAT) != 0) {
     cVideoDirectory Dir;
     if (Dir.IsDistributed()) {
        // Find the directory with the most free space:
        int MaxFree = Dir.FreeMB();
        while (Dir.Next()) {
              int Free = FreeDiskSpaceMB(Dir.Name());
              if (Free > MaxFree) {
                 Dir.Store();
                 MaxFree = Free;
                 }
              }
        if (Dir.Stored()) {
           ActualFileName = Dir.Adjust(FileName);
           if (!MakeDirs(ActualFileName, false))
              return NULL; // errno has been set by MakeDirs()
           if (symlink(ActualFileName, FileName) < 0) {
              LOG_ERROR_STR(FileName);
              return NULL;
              }
           ActualFileName = strdup(ActualFileName); // must survive Dir!
           }
        }
     }
  cUnbufferedFile *File = cUnbufferedFile::Create(ActualFileName, Flags, DEFFILEMODE);
  if (ActualFileName != FileName)
     free((char *)ActualFileName);
  return File;
}

int CloseVideoFile(cUnbufferedFile *File)
{
  int Result = File->Close();
  delete File;
  return Result;
}

bool RenameVideoFile(const char *OldName, const char *NewName)
{
  // Only the base video directory entry will be renamed, leaving the
  // possible symlinks untouched. Going through all the symlinks and disks
  // would be unnecessary work - maybe later...
  if (rename(OldName, NewName) == -1) {
     LOG_ERROR_STR(OldName);
     return false;
     }
  return true;
}

bool RemoveVideoFile(const char *FileName)
{
  return RemoveFileOrDir(FileName, true);
}

bool VideoFileSpaceAvailable(int SizeMB)
{
  cVideoDirectory Dir;
  if (Dir.IsDistributed()) {
     if (Dir.FreeMB() >= SizeMB * 2) // base directory needs additional space
        return true;
     while (Dir.Next()) {
           if (Dir.FreeMB() >= SizeMB)
              return true;
           }
     return false;
     }
  return Dir.FreeMB() >= SizeMB;
}

int VideoDiskSpace(int *FreeMB, int *UsedMB)
{
  int free = 0, used = 0;
  int deleted = DeletedRecordings.TotalFileSizeMB();
  cVideoDirectory Dir;
  do {
     int u;
     free += Dir.FreeMB(&u);
     used += u;
     } while (Dir.Next());
  if (deleted > used)
     deleted = used; // let's not get beyond 100%
  free += deleted;
  used -= deleted;
  if (FreeMB)
     *FreeMB = free;
  if (UsedMB)
     *UsedMB = used;
  return (free + used) ? used * 100 / (free + used) : 0;
}

cString PrefixVideoFileName(const char *FileName, char Prefix)
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

void RemoveEmptyVideoDirectories(void)
{
  cVideoDirectory Dir;
  do {
     RemoveEmptyDirectories(Dir.Name());
     } while (Dir.Next());
}

bool IsOnVideoDirectoryFileSystem(const char *FileName)
{
  cVideoDirectory Dir;
  do {
     if (EntriesOnSameFileSystem(Dir.Name(), FileName))
        return true;
     } while (Dir.Next());
  return false;
}
