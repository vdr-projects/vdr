/*
 * remote.h: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.4 2000/04/24 09:46:00 kls Exp $
 */

#ifndef __REMOTE_H
#define __REMOTE_H

#include <stdio.h>
#include <time.h>

class cRcIo {
private:
  int f;
  unsigned char dp, code, mode;
  unsigned short address;
  time_t t;
  int firstTime, lastTime;
  unsigned int lastCommand;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(bool Wait = true);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
public:
  enum { modeH = 'h', modeB = 'b', modeS = 's' };
  cRcIo(char *DeviceName);
  ~cRcIo();
  bool InputAvailable(bool Wait = false);
  void Flush(int WaitSeconds = 0);
  bool SetCode(unsigned char Code, unsigned short Address);
  bool SetMode(unsigned char Mode);
  bool GetCommand(unsigned int *Command, unsigned short *Address = NULL);
  bool Digit(int n, int v);
  bool Number(int n, bool Hex = false);
  void Points(unsigned char Dp) { dp = Dp; }
  bool String(char *s);
  bool DetectCode(unsigned char *Code, unsigned short *Address);
  };

#endif //__REMOTE_H
