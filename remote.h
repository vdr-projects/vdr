/*
 * remote.h: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 1.21 2002/12/01 10:39:10 kls Exp $
 */

#ifndef __REMOTE_H
#define __REMOTE_H

#include <stdio.h>
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
  virtual bool Initialize(void) { return true; }
  const char *Name(void) { return name; }
  static void SetLearning(cRemote *Learning) { learning = Learning; }
  static void Clear(void);
  static bool Put(eKeys Key);
  static bool PutMacro(eKeys Key);
  static const char *GetPlugin(void) { return plugin; }
  static eKeys Get(int WaitMs = 1000, char **UnknownCode = NULL);
  };

class cRemotes : public cList<cRemote> {};

extern cRemotes Remotes;

#if defined REMOTE_KBD

class cKbdRemote : public cRemote, private cThread {
private:
  virtual void Action(void);
public:
  cKbdRemote(void);
  virtual ~cKbdRemote();
  };

#endif

#endif //__REMOTE_H
