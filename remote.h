/*
 * remote.h: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.6 2000/06/24 15:52:56 kls Exp $
 */

#ifndef __REMOTE_H
#define __REMOTE_H

#include <stdio.h>
#include <time.h>

class cRcIoBase {
protected:
  time_t t;
  int firstTime, lastTime;
  unsigned int lastCommand;
  cRcIoBase(void);
  virtual ~cRcIoBase();
public:
  enum { modeH = 'h', modeB = 'b', modeS = 's' };
  virtual bool SetCode(unsigned char Code, unsigned short Address) { return true; }
  virtual bool SetMode(unsigned char Mode) { return true; }
  virtual bool Number(int n, bool Hex = false) { return true; }
  virtual void SetPoints(unsigned char Dp, bool On) {}
  virtual bool String(char *s) { return true; }
  virtual bool DetectCode(unsigned char *Code, unsigned short *Address) { return true; }
  virtual void Flush(int WaitSeconds = 0) {}
  virtual bool InputAvailable(bool Wait = false) = 0;
  virtual bool GetCommand(unsigned int *Command, unsigned short *Address = NULL) = 0;
  };

#if defined REMOTE_KBD

class cRcIoKBD : public cRcIoBase {
public:
  cRcIoKBD(void);
  virtual ~cRcIoKBD();
  virtual void Flush(int WaitSeconds = 0);
  virtual bool InputAvailable(bool Wait = false);
  virtual bool GetCommand(unsigned int *Command, unsigned short *Address = NULL);
  };

#elif defined REMOTE_RCU

class cRcIoRCU : public cRcIoBase {
private:
  int f;
  unsigned char dp, code, mode;
  unsigned short address;
  int lastNumber;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(bool Wait = true);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
  bool Digit(int n, int v);
public:
  cRcIoRCU(char *DeviceName);
  virtual ~cRcIoRCU();
  virtual bool SetCode(unsigned char Code, unsigned short Address);
  virtual bool SetMode(unsigned char Mode);
  virtual bool Number(int n, bool Hex = false);
  virtual void SetPoints(unsigned char Dp, bool On);
  virtual bool String(char *s);
  virtual bool DetectCode(unsigned char *Code, unsigned short *Address);
  virtual void Flush(int WaitSeconds = 0);
  virtual bool InputAvailable(bool Wait = false);
  virtual bool GetCommand(unsigned int *Command, unsigned short *Address = NULL);
  };

#elif defined REMOTE_LIRC

class cRcIoLIRC : public cRcIoBase {
private:
  enum { LIRC_BUFFER_SIZE = 128 };
  int f;
  char buf[LIRC_BUFFER_SIZE];
  const char *ReceiveString(void);
public:
  cRcIoLIRC(char *DeviceName);
  virtual ~cRcIoLIRC();
  virtual void Flush(int WaitSeconds = 0);
  virtual bool InputAvailable(bool Wait = false);
  virtual bool GetCommand(unsigned int *Command, unsigned short *Address = NULL);
  };

#else

#error Please define a remote control mode!

#endif

#endif //__REMOTE_H
