/*
 * svdrp.h: Simple Video Disk Recorder Protocol
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: svdrp.h 4.7 2017/06/30 09:49:39 kls Exp $
 */

#ifndef __SVDRP_H
#define __SVDRP_H

#include "tools.h"

enum eSvdrpPeerModes {
  spmOff  = 0,
  spmAny  = 1,
  spmOnly = 2,
  };

enum eSvdrpFetchFlags {
  sffNone   = 0b0000,
  sffTimers = 0b0001,
  };

void SetSVDRPPorts(int TcpPort, int UdpPort);
void SetSVDRPGrabImageDir(const char *GrabImageDir);
void StartSVDRPServerHandler(void);
void StartSVDRPClientHandler(void);
void StopSVDRPServerHandler(void);
void StopSVDRPClientHandler(void);
void SendSVDRPDiscover(const char *Address = NULL);
bool GetSVDRPServerNames(cStringList *ServerNames, eSvdrpFetchFlags FetchFlag = sffNone);
     ///< Gets a list of all available VDRs this VDR is connected to via SVDRP,
     ///< and stores it in the given ServerNames list. The list is cleared
     ///< before getting the server names.
     ///< If FetchFlag is given, only the server names for which the local
     ///< client has this flag set will be returned, and the client's flag
     ///< will be cleared.
     ///< Returns true if the resulting list is not empty.
bool ExecSVDRPCommand(const char *ServerName, const char *Command, cStringList *Response = NULL);
     ///< Sends the given SVDRP Command string to the remote VDR identified
     ///< by ServerName and collects all of the response strings in Response.
     ///< If no Response parameter is given, the response from command execution
     ///< is ignored.
     ///< Returns true if the data exchange was successful. Whether or
     ///< not the actual SVDRP command was successful depends on the
     ///< resulting strings from the remote VDR, which can be accessed
     ///< through Response. If Response is given, it will be cleared before
     ///< the command is actually executed.
void BroadcastSVDRPCommand(const char *Command);
     ///< Sends the given SVDRP Command string to all remote VDRs.
inline int SVDRPCode(const char *s) { return s ? atoi(s) : 0; }
     ///< Returns the value of the three digit reply code of the given
     ///< SVDRP response string.
inline const char *SVDRPValue(const char *s) { return s && s[0] && s[1] && s[2] && s[3] ? s + 4 : NULL; }
     ///< Returns the actual value of the given SVDRP response string, skipping
     ///< the three digit reply code and possible continuation line indicator.

#endif //__SVDRP_H
