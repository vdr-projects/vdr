/*
 * remote.h: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.5 2000/05/07 09:27:54 kls Exp $
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
  int lastNumber;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(bool Wait = true);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
  bool Digit(int n, int v);
public:
  enum { modeH = 'h', modeB = 'b', modeS = 's' };
  cRcIo(char *DeviceName);
  ~cRcIo();
  bool InputAvailable(bool Wait = false);
  void Flush(int WaitSeconds = 0);
  bool SetCode(unsigned char Code, unsigned short Address);
  bool SetMode(unsigned char Mode);
  bool GetCommand(unsigned int *Command, unsigned short *Address = NULL);
  bool Number(int n, bool Hex = false);
  void SetPoints(unsigned char Dp, bool On);
  bool String(char *s);
  bool DetectCode(unsigned char *Code, unsigned short *Address);
  };

#endif //__REMOTE_H
