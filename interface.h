/*
 * interface.h: Abstract user interface layer
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.h 1.1 2000/02/19 13:36:48 kls Exp $
 */

#ifndef __INTERFACE_H
#define __INTERFACE_H

#include "config.h"

class cInterface {
public:
  enum { MaxCols = 5 };
private:
  int open;
  int cols[MaxCols];
  unsigned int GetCh(void);
  void QueryKeys(void);
  void Write(int x, int y, char *s);
public:
  cInterface(void);
  void Init(void);
  void Open(void);
  void Close(void);
  eKeys GetKey(void);
  void Clear(void);
  void SetCols(int *c);
  void WriteText(int x, int y, char *s, bool Current = false);
  void Info(char *s);
  void Error(char *s);
  void LearnKeys(void);
  void DisplayChannel(int Number, char *Name);
  };

extern cInterface Interface;

#endif //__INTERFACE_H
