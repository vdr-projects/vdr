/*
 * interface.h: Abstract user interface layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.h 1.6 2000/04/23 14:57:13 kls Exp $
 */

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "config.h"
#include "dvbapi.h"

class cInterface {
public:
  enum { MaxCols = 5 };
private:
  int open;
  int cols[MaxCols];
  eKeys keyFromWait;
  unsigned int GetCh(bool Wait = true);
  void QueryKeys(void);
  void HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor);
  eKeys Wait(int Seconds = 1, bool KeepChar = false);
public:
  cInterface(void);
  void Init(void);
  void Open(int NumCols = MenuColumns, int NumLines = MenuLines);
  void Close(void);
  eKeys GetKey(bool Wait = true);
  void Clear(void);
  void ClearEol(int x, int y, eDvbColor Color = clrBackground);
  void SetCols(int *c);
  void Write(int x, int y, const char *s, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  void WriteText(int x, int y, const char *s, bool Current = false);
  void Title(const char *s);
  void Status(const char *s, eDvbColor FgColor = clrBlack, eDvbColor BgColor = clrCyan);
  void Info(const char *s);
  void Error(const char *s);
  bool Confirm(const char *s);
  void Help(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  void LearnKeys(void);
  void DisplayChannel(int Number, const char *Name = NULL);
  };

extern cInterface Interface;
extern cDvbApi DvbApi; //XXX member of cInterface???

#endif //__INTERFACE_H
