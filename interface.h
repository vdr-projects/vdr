/*
 * interface.h: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.h 1.30 2003/10/05 09:42:53 kls Exp $
 */

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "config.h"
#include "osdbase.h"
#include "remote.h"
#include "svdrp.h"

class cInterface {
public:
  enum { MaxCols = 5 };
private:
  int width, height;
  int open;
  int cols[MaxCols];
  bool interrupted;
  cSVDRP *SVDRP;
  bool QueryKeys(cRemote *Remote);
  void HelpButton(int Index, const char *Text, eDvbColor FgColor, eDvbColor BgColor);
  eKeys Wait(int Seconds = 0, bool KeepChar = false);
public:
  cInterface(int SVDRPport = 0);
  ~cInterface();
  bool IsOpen(void) { return open > 0; }
  void Open(int NumCols = 0, int NumLines = 0);
  void Close(void);
  bool HasSVDRPConnection(void) { return SVDRP && SVDRP->HasConnection(); }
  void Interrupt(void) { interrupted = true; }
  int Width(void) { return width; }
  int Height(void) { return height; }
  eKeys GetKey(bool Wait = true);
  void Clear(void);
  void ClearEol(int x, int y, eDvbColor Color = clrBackground);
  void Fill(int x, int y, int w, int h, eDvbColor color = clrBackground);
  void SetBitmap(int x, int y, const cBitmap &Bitmap);
  void Flush(void);
  void SetCols(int *c);
  const int *GetCols(void) { return cols; }
  eDvbFont SetFont(eDvbFont Font);
  char *WrapText(const char *Text, int Width, int *Height);
  void Write(int x, int y, const char *s, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  void WriteText(int x, int y, const char *s, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  void Title(const char *s);
  void Status(const char *s, eDvbColor FgColor = clrBlack, eDvbColor BgColor = clrCyan);
  void Info(const char *s);
  void Error(const char *s);
  bool Confirm(const char *s, int Seconds = 10, bool WaitForTimeout = false);
  void Help(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
  void LearnKeys(void);
  };

extern cInterface *Interface;

#endif //__INTERFACE_H
