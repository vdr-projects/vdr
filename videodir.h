/*
 * videodir.h: Functions to maintain the video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.h 3.2 2013/10/11 09:37:48 kls Exp $
 */

#ifndef __VIDEODIR_H
#define __VIDEODIR_H

#include <stdlib.h>
#include "tools.h"

class cVideoDirectory {
private:
  static cString name;
  static cVideoDirectory *current;
  static cVideoDirectory *Current(void);
public:
  cVideoDirectory(void);
  virtual ~cVideoDirectory();
  virtual int FreeMB(int *UsedMB = NULL);
      ///< Returns the total amount (in MB) of free disk space for recording.
      ///< If UsedMB is given, it returns the amount of disk space in use by
      ///< existing recordings (or anything else) on that disk.
  virtual bool Register(const char *FileName);
      ///< By default VDR assumes that the video directory consists of one large
      ///< volume, on which it can store its recordings. A derived cVideoDirectory
      ///< may, for instance, use several separate disks to store recordings.
      ///< The given FileName is the full path name (including the video directory) of
      ///< a recording file ('*.ts') that is about to be opened for writing. If the actual
      ///< file shall be put on an other disk, the derived cVideoDirectory should
      ///< create a symbolic link from the given FileName to the other location.
      ///< Returns true if the operation was successful.
      ///< The default implementation just checks whether the incoming file name really
      ///< is under the video directory.
  virtual bool Rename(const char *OldName, const char *NewName);
      ///< Renames the directory OldName to NewName.
      ///< OldName and NewName are full path names that begin with the name of the
      ///< video directory and end with '*.rec' or '*.del'. Only the base name (the
      ///< rightmost component) of the two names may be different.
      ///< Returns true if the operation was successful.
      ///< The default implementation just calls the system's rename() function.
  virtual bool Move(const char *FromName, const char *ToName);
      ///< Moves the directory FromName to the location ToName. FromName is the full
      ///< path name of a recording's '*.rec' directory. ToName has the same '*.rec'
      ///< part as FromName, but a different directory path above it.
      ///< Returns true if the operation was successful.
      ///< The default implementation just calls the system's rename() function.
  virtual bool Remove(const char *Name);
      ///< Removes the directory with the given Name and everything it contains.
      ///< Name is a full path name that begins with the name of the video directory.
      ///< Returns true if the operation was successful.
      ///< The default implementation calls RemoveFileOrDir().
  virtual void Cleanup(const char *IgnoreFiles[] = NULL);
      ///< Recursively removes all empty directories under the video directory.
      ///< If IgnoreFiles is given, the file names in this (NULL terminated) array
      ///< are ignored when checking whether a directory is empty. These are
      ///< typically "dot files", like e.g. ".sort".
      ///< The default implementation calls RemoveEmptyDirectories().
  virtual bool Contains(const char *Name);
      ///< Checks whether the directory Name is on the same file system as the
      ///< video directory. Name is the full path name of a recording's '*.rec'
      ///< directory. This function is usually called when an ongoing recording
      ///< is about to run out of disk space, and an existing (old) recording needs
      ///< to be deleted. It shall make sure that deleting this old recording will
      ///< actually free up space in the video directory, and not on some other
      ///< device that just happens to be mounted.
      ///< The default implementation calls EntriesOnSameFileSystem().
  static const char *Name(void);
  static void SetName(const char *Name);
  static void Destroy(void);
  static cUnbufferedFile *OpenVideoFile(const char *FileName, int Flags);
  static bool RenameVideoFile(const char *OldName, const char *NewName);
  static bool MoveVideoFile(const char *FromName, const char *ToName);
  static bool RemoveVideoFile(const char *FileName);
  static bool VideoFileSpaceAvailable(int SizeMB);
  static int VideoDiskSpace(int *FreeMB = NULL, int *UsedMB = NULL); // returns the used disk space in percent
  static cString PrefixVideoFileName(const char *FileName, char Prefix);
  static void RemoveEmptyVideoDirectories(const char *IgnoreFiles[] = NULL);
  static bool IsOnVideoDirectoryFileSystem(const char *FileName);
  };

class cVideoDiskUsage {
private:
  static int state;
  static time_t lastChecked;
  static int usedPercent;
  static int freeMB;
  static int freeMinutes;
public:
  static bool HasChanged(int &State);
    ///< Returns true if the usage of the video disk space has changed since the last
    ///< call to this function with the given State variable. The caller should
    ///< initialize State to -1, and it will be set to the current internal state
    ///< value of the video disk usage checker upon return. Future calls with the same
    ///< State variable can then quickly check for changes.
  static void ForceCheck(void) { lastChecked = 0; }
    ///< To avoid unnecessary load, the video disk usage is only actually checked
    ///< every DISKSPACECHEK seconds. Calling ForceCheck() makes sure that the next call
    ///< to HasChanged() will check the disk usage immediately. This is useful in case
    ///< some files have been deleted and the result shall be displayed instantly.
  static cString String(void);
    ///< Returns a localized string of the form "Disk nn%  -  hh:mm free".
    ///< This function is mainly for use in skins that want to retain the display of the
    ///< free disk space in the menu title, as was the case until VDR version 1.7.27.
    ///< An implicit call to HasChanged() is done in this function, to make sure the
    ///< returned value is up to date.
  static int UsedPercent(void) { return usedPercent; }
    ///< Returns the used space of the video disk in percent.
    ///< The caller should call HasChanged() first, to make sure the value is up to date.
  static int FreeMB(void) { return freeMB; }
    ///< Returns the amount of free space on the video disk in MB.
    ///< The caller should call HasChanged() first, to make sure the value is up to date.
  static int FreeMinutes(void) { return freeMinutes; }
    ///< Returns the number of minutes that can still be recorded on the video disk.
    ///< This is an estimate and depends on the data rate of the existing recordings.
    ///< There is no guarantee that this value will actually be met.
    ///< The caller should call HasChanged() first, to make sure the value is up to date.
  };

#endif //__VIDEODIR_H
