/*
 * videodir.h: Functions to maintain a distributed video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.h 1.1 2000/07/29 14:08:27 kls Exp $
 */

#ifndef __VIDEODIR_H
#define __VIDEODIR_H

extern const char *VideoDirectory;

int OpenVideoFile(const char *FileName, int Flags);
int CloseVideoFile(int FileHandle);
bool RenameVideoFile(const char *OldName, const char *NewName);
bool RemoveVideoFile(const char *FileName);
bool VideoFileSpaceAvailable(unsigned int SizeMB);

#endif //__VIDEODIR_H
