/*
 * menuitems.h: General purpose menu items
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menuitems.h 5.1 2020/12/26 15:49:01 kls Exp $
 */

#ifndef __MENUITEMS_H
#define __MENUITEMS_H

#include <limits.h>
#include "dvbdevice.h"
#include "osdbase.h"

extern const char *FileNameChars;

class cMenuEditItem : public cOsdItem {
private:
  char *name;
  const char *helpRed, *helpGreen, *helpYellow, *helpBlue;
  bool helpDisplayed;
protected:
  void SetHelp(const char *Red, const char *Green = NULL, const char *Yellow = NULL, const char *Blue = NULL);
public:
  cMenuEditItem(const char *Name);
  ~cMenuEditItem();
  void SetValue(const char *Value);
  bool DisplayHelp(bool Current);
  };

class cMenuEditIntItem : public cMenuEditItem {
protected:
  int *value;
  int min, max;
  const char *minString, *maxString;
  virtual void Set(void);
public:
  cMenuEditIntItem(const char *Name, int *Value, int Min = 0, int Max = INT_MAX, const char *MinString = NULL, const char *MaxString = NULL);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditBoolItem : public cMenuEditIntItem {
protected:
  const char *falseString, *trueString;
  virtual void Set(void);
public:
  cMenuEditBoolItem(const char *Name, int *Value, const char *FalseString = NULL, const char *TrueString = NULL);
  };

class cMenuEditBitItem : public cMenuEditBoolItem {
protected:
  uint *value;
  uint mask;
  int bit;
  virtual void Set(void);
public:
  cMenuEditBitItem(const char *Name, uint *Value, uint Mask, const char *FalseString = NULL, const char *TrueString = NULL);
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

class cMenuEditIntxItem : public cMenuEditIntItem {
private:
  int factor;
  const char *negString, *posString;
  void SetHelpKeys(void);
  virtual void Set(void);
public:
  cMenuEditIntxItem(const char *Name, int *Value, int Min = INT_MIN, int Max = INT_MAX, int Factor = 1, const char *NegString = NULL, const char *PosString = NULL);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditPrcItem : public cMenuEditItem {
protected:
  double *value;
  double min, max;
  int decimals;
  int factor;
  virtual void Set(void);
public:
  cMenuEditPrcItem(const char *Name, double *Value, double Min = 0.0, double Max = 1.0, int Decimals = 0);
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
  const char *allowed;
  int pos, offset;
  bool keepSpace;
  const char **macros;
  int macro, lastMacro;
  bool insert, newchar, uppercase;
  int lengthUtf8;
  uint *valueUtf8;
  uint *allowedUtf8;
  uint *charMapUtf8;
  uint *currentCharUtf8;
  eKeys lastKey;
  cTimeMs autoAdvanceTimeout;
  void SetHelpKeys(void);
  uint *IsAllowed(uint c);
  void AdvancePos(void);
  virtual void Set(void);
  uint Inc(uint c, bool Up);
  void Type(uint c);
  void Insert(void);
  void Delete(void);
  void InsertMacro(void);
protected:
  void EnterEditMode(void);
  void LeaveEditMode(bool SaveValue = false);
  bool InEditMode(void) { return valueUtf8 != NULL; }
public:
  cMenuEditStrItem(const char *Name, char *Value, int Length, const char *Allowed = NULL);
  ~cMenuEditStrItem();
  void SetKeepSpace(void) { keepSpace = true; }
  void SetMacros(const char **Macros);
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

class cMenuEditStrlItem : public cMenuEditIntItem {
private:
  const cStringList *strings;
  int index;
  char *value;
  int length;
protected:
  virtual void Set(void);
public:
  cMenuEditStrlItem(const char *Name, char *Value, int Length, const cStringList *Strings);
  };

class cMenuEditChanItem : public cMenuEditIntItem {
protected:
  const char *noneString;
  int dummyValue;
  cString *channelID;
  virtual void Set(void);
public:
  cMenuEditChanItem(const char *Name, int *Value, const char *NoneString = NULL);
  cMenuEditChanItem(const char *Name, cString *ChannelID, const char *NoneString = NULL);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditTranItem : public cMenuEditChanItem {
private:
  int number;
  int *source;
  int *transponder;
public:
  cMenuEditTranItem(const char *Name, int *Value, int *Source);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditDateItem : public cMenuEditItem {
private:
  static int days[];
  time_t *value;
  int *weekdays;
  time_t oldvalue;
  int oldweekdays;
  int dayindex;
  int FindDayIndex(int WeekDays);
  virtual void Set(void);
public:
  cMenuEditDateItem(const char *Name, time_t *Value, int *WeekDays = NULL);
  void ToggleRepeating(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditTimeItem : public cMenuEditItem {
protected:
  int *value;
  int hh, mm;
  int pos;
  virtual void Set(void);
public:
  cMenuEditTimeItem(const char *Name, int *Value);
  virtual eOSState ProcessKey(eKeys Key);
  };

class cMenuEditMapItem : public cMenuEditItem {
protected:
  int *value;
  const tDvbParameterMap *map;
  const char *zeroString;
  virtual void Set(void);
public:
  cMenuEditMapItem(const char *Name, int *Value, const tDvbParameterMap *Map, const char *ZeroString = NULL);
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
