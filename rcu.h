/*
 * rcu.h: RCU remote control
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: rcu.h 1.4 2005/07/31 10:18:00 kls Exp $
 */

#ifndef __RCU_H
#define __RCU_H

#include "remote.h"
#include "status.h"
#include "thread.h"

class cRcuRemote : public cRemote, private cThread, private cStatus {
private:
  enum { modeH = 'h', modeB = 'b', modeS = 's' };
  int f;
  unsigned char dp, code, mode;
  int numberToSend;
  int lastNumber;
  bool receivedCommand;
  bool SendCommand(unsigned char Cmd);
  int ReceiveByte(int TimeoutMs = 0);
  bool SendByteHandshake(unsigned char c);
  bool SendByte(unsigned char c);
  bool Digit(int n, int v);
  bool SetCode(unsigned char Code);
  bool SetMode(unsigned char Mode);
  bool Number(int n, bool Hex = false);
  void SetPoints(unsigned char Dp, bool On);
  bool String(char *s);
  bool DetectCode(unsigned char *Code);
  virtual void Action(void);
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
  virtual void Recording(const cDevice *Device, const char *Name);
public:
  cRcuRemote(const char *DeviceName);
  virtual ~cRcuRemote();
  virtual bool Ready(void);
  virtual bool Initialize(void);
  };

#endif //__RCU_H
