/*
 * interface.h: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.h 1.17 2000/10/29 12:32:12 kls Exp $
 */

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "config.h"
#include "dvbapi.h"
#include "remote.h"
#include "svdrp.h"

class cInterface {
public:
  enum { MaxCols = 5 };
private:
  int open;
  int cols[MaxCols];
  eKeys keyFromWait;
  cSVDRP *SVDRP;
  cRcIoBase *rcIo;
  unsigned int GetCh(bool Wait = true, bool *Repeat = NULL, bool *Release = NULL);
  void QueryKeys(void);
  void HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor);
  eKeys Wait(int Seconds = 1, bool KeepChar = false);
  eKeys DisplayDescription(const cEventInfo *EventInfo);
  int WriteParagraph(int Line, const char *Text);
public:
  cInterface(int SVDRPport = 0);
  ~cInterface();
  void Open(int NumCols = MenuColumns, int NumLines = MenuLines);
  void Close(void);
  eKeys GetKey(bool Wait = true);
  void PutKey(eKeys Key);
  void Clear(void);
  void ClearEol(int x, int y, eDvbColor Color = clrBackground);
  void SetCols(int *c);
  void Write(int x, int y, const char *s, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  void WriteText(int x, int y, const char *s, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  void Title(const char *s);
  void Status(const char *s, eDvbColor FgColor = clrBlack, eDvbColor BgColor = clrCyan);
  void Info(const char *s);
  void Error(const char *s);
  bool Confirm(const char *s);
  void Help(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  void LearnKeys(void);
  eKeys DisplayChannel(int Number, const char *Name = NULL, bool WithInfo = false);
  void DisplayRecording(int Index, bool On);
  bool Recording(void);
  };

extern cInterface *Interface;

#endif //__INTERFACE_H
