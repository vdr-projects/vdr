/*
 * videodir.h: Functions to maintain a distributed video directory
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: videodir.h 1.3 2001/02/11 13:12:50 kls Exp $
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
void RemoveEmptyVideoDirectories(void);

#endif //__VIDEODIR_H
