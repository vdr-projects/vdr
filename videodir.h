/*
 * videodir.h: Functions to maintain a distributed video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.h 1.4 2002/01/27 12:37:20 kls Exp $
 */

#ifndef __VIDEODIR_H
#define __VIDEODIR_H

#include <stdlib.h>

extern const char *VideoDirectory;

int OpenVideoFile(const char *FileName, int Flags);
int CloseVideoFile(int FileHandle);
bool RenameVideoFile(const char *OldName, const char *NewName);
bool RemoveVideoFile(const char *FileName);
bool VideoFileSpaceAvailable(int SizeMB);
int VideoDiskSpace(int *FreeMB = NULL, int *UsedMB = NULL); // returns the used disk space in percent
const char *PrefixVideoFileName(const char *FileName, char Prefix);
void RemoveEmptyVideoDirectories(void);

#endif //__VIDEODIR_H
