/*
 * ci.h: Common Interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: ci.h 1.14 2005/08/20 14:56:11 kls Exp $
 */

#ifndef __CI_H
#define __CI_H

#include <stdint.h>
#include <stdio.h>
#include "thread.h"

class cCiMMI;

class cCiMenu {
  friend class cCiMMI;
private:
  enum { MAX_CIMENU_ENTRIES = 64 }; ///< XXX is there a specified maximum?
  cCiMMI *mmi;
  cMutex mutex;
  bool selectable;
  char *titleText;
  char *subTitleText;
  char *bottomText;
  char *entries[MAX_CIMENU_ENTRIES];
  int numEntries;
  bool AddEntry(char *s);
  cCiMenu(cCiMMI *MMI, bool Selectable);
public:
  ~cCiMenu();
  const char *TitleText(void) { return titleText; }
  const char *SubTitleText(void) { return subTitleText; }
  const char *BottomText(void) { return bottomText; }
  const char *Entry(int n) { return n < numEntries ? entries[n] : NULL; }
  int NumEntries(void) { return numEntries; }
  bool Selectable(void) { return selectable; }
  bool Select(int Index);
  bool Cancel(void);
  };

class cCiEnquiry {
  friend class cCiMMI;
private:
  cCiMMI *mmi;
  cMutex mutex;
  char *text;
  bool blind;
  int expectedLength;
  cCiEnquiry(cCiMMI *MMI);
public:
  ~cCiEnquiry();
  const char *Text(void) { return text; }
  bool Blind(void) { return blind; }
  int ExpectedLength(void) { return expectedLength; }
  bool Reply(const char *s);
  bool Cancel(void);
  };

class cCiCaPmt {
  friend class cCiConditionalAccessSupport;
private:
  int length;
  int esInfoLengthPos;
  uint8_t capmt[2048]; ///< XXX is there a specified maximum?
  int caDescriptorsLength;
  uint8_t caDescriptors[2048];
  bool streamFlag;
  void AddCaDescriptors(int Length, const uint8_t *Data);
public:
  cCiCaPmt(int Source, int Transponder, int ProgramNumber, const unsigned short *CaSystemIds);
  bool Valid(void);
  void AddPid(int Pid, uint8_t StreamType);
  };

#define MAX_CI_SESSION  16 //XXX
#define MAX_CI_SLOT     16

class cCiSession;
class cCiTransportLayer;
class cCiTransportConnection;

class cCiHandler {
private:
  cMutex mutex;
  int fd;
  int numSlots;
  bool newCaSupport;
  bool hasUserIO;
  bool moduleReady[MAX_CI_SLOT];
  cCiSession *sessions[MAX_CI_SESSION];
  cCiTransportLayer *tpl;
  cCiTransportConnection *tc;
  int ResourceIdToInt(const uint8_t *Data);
  bool Send(uint8_t Tag, int SessionId, int ResourceId = 0, int Status = -1);
  cCiSession *GetSessionBySessionId(int SessionId);
  cCiSession *GetSessionByResourceId(int ResourceId, int Slot);
  cCiSession *CreateSession(int ResourceId);
  bool OpenSession(int Length, const uint8_t *Data);
  bool CloseSession(int SessionId);
  int CloseAllSessions(int Slot);
  cCiHandler(int Fd, int NumSlots);
public:
  ~cCiHandler();
  static cCiHandler *CreateCiHandler(const char *FileName);
  int NumSlots(void) { return numSlots; }
  bool Ready(void);
  bool Process(void);
  bool HasUserIO(void) { return hasUserIO; }
  bool EnterMenu(int Slot);
  cCiMenu *GetMenu(void);
  cCiEnquiry *GetEnquiry(void);
  const char *GetCamName(int Slot);
  const unsigned short *GetCaSystemIds(int Slot);
  bool ProvidesCa(const unsigned short *CaSystemIds); //XXX Slot???
  bool SetCaPmt(cCiCaPmt &CaPmt, int Slot);
  bool Reset(int Slot);
  };

#endif //__CI_H
