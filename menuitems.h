/*
 * menuitems.h: General purpose menu items
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.h 1.5 2003/01/12 15:06:23 kls Exp $
 */

#ifndef __MENUITEMS_H
#define __MENUITEMS_H

#include "osd.h"

extern const char *FileNameChars;

class cMenuEditItem : public cOsdItem {
private:
  char *name;
  char *value;
public:
  cMenuEditItem(const char *Name);
  ~cMenuEditItem();
  void SetValue(const char *Value);
  };

class cMenuEditIntItem : public cMenuEditItem {
protected:
  int *value;
  int min, max;
  virtual void Set(void);
public:
  cMenuEditIntItem(const char *Name, int *Value, int Min = 0, int Max = INT_MAX);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditBoolItem : public cMenuEditIntItem {
protected:
  const char *falseString, *trueString;
  virtual void Set(void);
public:
  cMenuEditBoolItem(const char *Name, int *Value, const char *FalseString = NULL, const char *TrueString = NULL);
  };

class cMenuEditNumItem : public cMenuEditItem {
protected:
  char *value;
  int length;
  bool blind;
  virtual void Set(void);
public:
  cMenuEditNumItem(const char *Name, char *Value, int Length, bool Blind = false);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditChrItem : public cMenuEditItem {
private:
  char *value;
  char *allowed;
  const char *current;
  virtual void Set(void);
public:
  cMenuEditChrItem(const char *Name, char *Value, const char *Allowed);
  ~cMenuEditChrItem();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditStrItem : public cMenuEditItem {
private:
  char *value;
  int length;
  char *allowed;
  int pos;
  bool insert, newchar, uppercase;
  void SetHelpKeys(void);
  virtual void Set(void);
  char Inc(char c, bool Up);
public:
  cMenuEditStrItem(const char *Name, char *Value, int Length, const char *Allowed);
  ~cMenuEditStrItem();
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditStraItem : public cMenuEditIntItem {
private:
  const char * const *strings;
protected:
  virtual void Set(void);
public:
  cMenuEditStraItem(const char *Name, int *Value, int NumStrings, const char * const *Strings);
  };

class cMenuTextItem : public cOsdItem {
private:
  char *text;
  int x, y, w, h, lines, offset;
  eDvbColor fgColor, bgColor;
  eDvbFont font;
public:
  cMenuTextItem(const char *Text, int X, int Y, int W, int H = -1, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground, eDvbFont Font = fontOsd);
  ~cMenuTextItem();
  int Height(void) { return h; }
  void Clear(void);
  virtual void Display(int Offset = -1, eDvbColor FgColor = clrWhite, eDvbColor BgColor = clrBackground);
  bool CanScrollUp(void) { return offset > 0; }
  bool CanScrollDown(void) { return h + offset < lines; }
  void ScrollUp(bool Page);
  void ScrollDown(bool Page);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cPlugin;

class cMenuSetupPage : public cOsdMenu {
private:
  cPlugin *plugin;
protected:
  void SetSection(const char *Section);
  virtual void Store(void) = 0;
  void SetupStore(const char *Name, const char *Value = NULL);
  void SetupStore(const char *Name, int Value);
public:
  cMenuSetupPage(void);
  virtual eOSState ProcessKey(eKeys Key);
  void SetPlugin(cPlugin *Plugin);
  };

#endif //__MENUITEMS_H
