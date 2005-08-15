/*
 * remote.h: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.30 2005/08/13 11:28:10 kls Exp $
 */

#ifndef __REMOTE_H
#define __REMOTE_H

#include <stdio.h>
#include <termios.h>
#include <time.h>
#include "keys.h"
#include "thread.h"
#include "tools.h"

class cRemote : public cListObject {
private:
  enum { MaxKeys = MAXKEYSINMACRO };
  static eKeys keys[MaxKeys];
  static int in;
  static int out;
  static cRemote *learning;
  static char *unknownCode;
  static cMutex mutex;
  static cCondVar keyPressed;
  static const char *plugin;
  char *name;
protected:
  cRemote(const char *Name);
  const char *GetSetup(void);
  void PutSetup(const char *Setup);
  bool Put(uint64 Code, bool Repeat = false, bool Release = false);
  bool Put(const char *Code, bool Repeat = false, bool Release = false);
public:
  virtual ~cRemote();
  virtual bool Ready(void) { return true; }
  virtual bool Initialize(void);
  const char *Name(void) { return name; }
  static void SetLearning(cRemote *Learning) { learning = Learning; }
  static void Clear(void);
  static bool Put(eKeys Key, bool AtFront = false);
  static bool PutMacro(eKeys Key);
  static const char *GetPlugin(void) { return plugin; }
  static bool HasKeys(void);
  static eKeys Get(int WaitMs = 1000, char **UnknownCode = NULL);
  };

class cRemotes : public cList<cRemote> {};

extern cRemotes Remotes;

enum eKbdFunc {
  kfNone,
  kfF1 = 0x100,
  kfF2,
  kfF3,
  kfF4,
  kfF5,
  kfF6,
  kfF7,
  kfF8,
  kfF9,
  kfF10,
  kfF11,
  kfF12,
  kfUp,
  kfDown,
  kfLeft,
  kfRight,
  kfHome,
  kfEnd,
  kfPgUp,
  kfPgDown,
  kfIns,
  kfDel,
  };

class cKbdRemote : public cRemote, private cThread {
private:
  static bool kbdAvailable;
  static bool rawMode;
  struct termios savedTm;
  virtual void Action(void);
  int MapCodeToFunc(uint64 Code);
public:
  cKbdRemote(void);
  virtual ~cKbdRemote();
  static bool KbdAvailable(void) { return kbdAvailable; }
  static uint64 MapFuncToCode(int Func);
  static void SetRawMode(bool RawMode);
  };

#endif //__REMOTE_H
