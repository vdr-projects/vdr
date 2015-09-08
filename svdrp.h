/*
 * svdrp.h: Simple Video Disk Recorder Protocol
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: svdrp.h 4.4 2015/09/06 12:39:24 kls Exp $
 */

#ifndef __SVDRP_H
#define __SVDRP_H

#include "tools.h"

class cSVDRPCommand {
protected:
  cString serverName;
  cString command;
  cStringList response;
public:
  cSVDRPCommand(const char *ServerName, const char *Command);
      ///< Sets up an SVDRP Command to be executed on the VDR with the given
      ///< ServerName. A list of all available servers can be retrieved by
      ///< calling GetSVDRPServerNames().
      ///< Command is one SVDRP command, followed by optional parameters,
      ///< just as it can be given in a normal SVDRP connection. It doesn't
      ///< need to be terminated with a newline.
  virtual ~cSVDRPCommand();
  bool Execute(void);
      ///< Sends the Command given in the constructor to the remote VDR
      ///< and collects all of the response strings.
      ///< Returns true if the data exchange was successful. Whether or
      ///< not the actual SVDRP command was successful depends on the
      ///< resulting strings from the remote VDR, which can be accessed
      ///< by calling Response(). Execute() can be called any number of
      ///< times. The list of response strings will be cleared before
      ///< the command is actually executed.
  const cStringList *Response(void) const { return &response; }
      ///< Returns the list of strings the remote VDR has sent in response
      ///< to the command. The response strings are exactly as received,
      ///< with the leading three digit reply code and possible continuation
      ///< line indicator ('-') in place.
  const char *Response(int Index) { return (Index >= 0 && Index < response.Size()) ? response[Index] : NULL; }
      ///< This is a convenience function for accessing the response strings.
      ///< Returns the string at the given Index, or NULL if Index is out
      ///< of range.
  int Code(const char *s) { return s ? atoi(s) : 0; }
      ///< Returns the value of the three digit reply code of the given
      ///< response string.
  const char *Value(const char *s) { return s && s[0] && s[1] && s[2] && s[3] ? s + 4 : NULL; }
      ///< Returns the actual value of the given response string, skipping
      ///< the three digit reply code and possible continuation line indicator.
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

#endif //__SVDRP_H
