/*
 * remote.h: General Remote Control handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: remote.h 5.1 2025/03/02 11:03:35 kls Exp $
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
  enum { MaxKeys = 2 * MAXKEYSINMACRO };
  static eKeys keys[MaxKeys];
  static int in;
  static int out;
  static cTimeMs repeatTimeout;
  static cRemote *learning;
  static char *unknownCode;
  static cMutex mutex;
  static cCondVar keyPressed;
  static time_t lastActivity;
  static const char *keyMacroPlugin;
  static const char *callPlugin;
  static bool enabled;
  char *name;
protected:
  cRemote(const char *Name);
  const char *GetSetup(void);
  void PutSetup(const char *Setup);
  bool Put(uint64_t Code, bool Repeat = false, bool Release = false);
  bool Put(const char *Code, bool Repeat = false, bool Release = false);
public:
  virtual ~cRemote() override;
  virtual bool Ready(void) { return true; }
  virtual bool Initialize(void);
  const char *Name(void) { return name; }
  static void SetLearning(cRemote *Learning) { learning = Learning; }
  static bool IsLearning() { return learning != NULL; }
  static bool Enabled(void) { return enabled; }
  static void SetEnabled(bool Enabled) { enabled = Enabled; }
  static void Clear(void);
  static bool Put(eKeys Key, bool AtFront = false);
  static bool PutMacro(eKeys Key);
  static bool CallPlugin(const char *Plugin);
      ///< Initiates calling the given plugin's main menu function.
      ///< The Plugin parameter is the name of the plugin, and must be
      ///< a static string. Returns true if the plugin call was successfully
      ///< initiated (the actual call to the plugin's main menu function
      ///< will take place some time later, during the next execution
      ///< of VDR's main loop). If there is already a plugin call pending
      ///< false will be returned and the caller should try again later.
  static const char *GetPlugin(void);
      ///< Returns the name of the plugin that was set with a previous
      ///< call to PutMacro() or CallPlugin(). The internally stored pointer to the
      ///< plugin name will be reset to NULL by this call.
  static bool HasKeys(void);
  static eKeys Get(int WaitMs = 1000, char **UnknownCode = NULL);
  static time_t LastActivity(void) { return lastActivity; }
      ///< Absolute time when last key was delivered by Get().
  static void TriggerLastActivity(void);
      ///< Simulates user activity, for instance to keep the current menu open
      ///< even if no remote control key has been pressed.
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
  bool systemIsUtf8;
  struct termios savedTm;
  virtual void Action(void) override;
  int ReadKey(void);
  uint64_t ReadKeySequence(void);
  int MapCodeToFunc(uint64_t Code);
  void PutKey(uint64_t Code, bool Repeat = false, bool Release = false);
public:
  cKbdRemote(void);
  virtual ~cKbdRemote() override;
  static bool KbdAvailable(void) { return kbdAvailable; }
  static uint64_t MapFuncToCode(int Func);
  static void SetRawMode(bool RawMode);
  };

#endif //__REMOTE_H
