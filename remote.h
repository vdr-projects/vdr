/*
 * remote.h: Interface to the Remote Control Unit
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.12 2000/10/08 11:19:17 kls Exp $
 */

#ifndef __REMOTE_H
#define __REMOTE_H

#include <stdio.h>
#include <time.h>
#include "thread.h"
#include "tools.h"

class cRcIoBase {
protected:
  time_t t;
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
  virtual void Flush(int WaitMs = 0) {}
  virtual bool InputAvailable(void) = 0;
  virtual bool GetCommand(unsigned int *Command = NULL, bool *Repeat = NULL, bool *Release = NULL) = 0;
  };

#if defined REMOTE_KBD

class cRcIoKBD : public cRcIoBase {
private:
  cFile f;
public:
  cRcIoKBD(void);
  virtual ~cRcIoKBD();
  virtual void Flush(int WaitMs = 0);
  virtual bool InputAvailable(void);
  virtual bool GetCommand(unsigned int *Command = NULL, bool *Repeat = NULL, bool *Release = NULL);
  };

#elif defined REMOTE_RCU

class cRcIoRCU : public cRcIoBase, private cThread {
private:
  int f;
  unsigned char dp, code, mode;
  unsigned short address;
  unsigned short receivedAddress;
  unsigned int receivedCommand;
  bool receivedData, receivedRepeat, receivedRelease;
  int lastNumber;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(int TimeoutMs = 0);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
  bool Digit(int n, int v);
  virtual void Action(void);
public:
  cRcIoRCU(char *DeviceName);
  virtual ~cRcIoRCU();
  virtual bool SetCode(unsigned char Code, unsigned short Address);
  virtual bool SetMode(unsigned char Mode);
  virtual bool Number(int n, bool Hex = false);
  virtual void SetPoints(unsigned char Dp, bool On);
  virtual bool String(char *s);
  virtual bool DetectCode(unsigned char *Code, unsigned short *Address);
  virtual void Flush(int WaitMs = 0);
  virtual bool InputAvailable(void) { return receivedData; }
  virtual bool GetCommand(unsigned int *Command = NULL, bool *Repeat = NULL, bool *Release = NULL);
  };

#elif defined REMOTE_LIRC

class cRcIoLIRC : public cRcIoBase, private cThread {
private:
  enum { LIRC_KEY_BUF = 8, LIRC_BUFFER_SIZE = 128 };
  int f;
  char keyName[LIRC_KEY_BUF];
  bool receivedData, receivedRepeat, receivedRelease;
  virtual void Action(void);
public:
  cRcIoLIRC(char *DeviceName);
  virtual ~cRcIoLIRC();
  virtual bool InputAvailable(void) { return receivedData; }
  virtual bool GetCommand(unsigned int *Command = NULL, bool *Repeat = NULL, bool *Release = NULL);
  };

#else

#error Please define a remote control mode!

#endif

#endif //__REMOTE_H
