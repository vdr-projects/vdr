/*
 * svdrp.h: Simple Video Disk Recorder Protocol
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: svdrp.h 1.1 2000/07/23 14:49:30 kls Exp $
 */

#ifndef __SVDRP_H
#define __SVDRP_H

class cSocket {
private:
  int port;
  int sock;
  int queue;
  void Close(void);
public:
  cSocket(int Port, int Queue = 1);
  ~cSocket();
  bool Open(void);
  int Accept(void);
  };

class cSVDRP {
private:
  cSocket socket;
  int filedes;
  void Close(void);
  bool Send(const char *s, int length = -1);
  void Reply(int Code, const char *fmt, ...);
  void CmdChan(const char *Option);
  void CmdDelc(const char *Option);
  void CmdDelt(const char *Option);
  void CmdHelp(const char *Option);
  void CmdLstc(const char *Option);
  void CmdLstt(const char *Option);
  void CmdModc(const char *Option);
  void CmdModt(const char *Option);
  void CmdMovc(const char *Option);
  void CmdMovt(const char *Option);
  void CmdNewc(const char *Option);
  void CmdNewt(const char *Option);
  void Execute(char *Cmd);
public:
  cSVDRP(int Port);
  ~cSVDRP();
  void Process(void);
  };

#endif //__SVDRP_H
