/*
 * svdrp.h: Simple Video Disk Recorder Protocol
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: svdrp.h 4.1 2015/04/29 13:10:06 kls Exp $
 */

#ifndef __SVDRP_H
#define __SVDRP_H

void SetSVDRPGrabImageDir(const char *GrabImageDir);
void StartSVDRPHandler(int Port);
void StopSVDRPHandler(void);

#endif //__SVDRP_H
