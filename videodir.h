/*
 * videodir.h: Functions to maintain a distributed video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.h 1.2 2000/12/24 12:41:10 kls Exp $
 */

#ifndef __VIDEODIR_H
#define __VIDEODIR_H

extern const char *VideoDirectory;

int OpenVideoFile(const char *FileName, int Flags);
int CloseVideoFile(int FileHandle);
bool RenameVideoFile(const char *OldName, const char *NewName);
bool RemoveVideoFile(const char *FileName);
bool VideoFileSpaceAvailable(unsigned int SizeMB);
const char *PrefixVideoFileName(const char *FileName, char Prefix);

#endif //__VIDEODIR_H
