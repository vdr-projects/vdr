/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 1.101 2001/08/11 15:04:05 kls Exp $
 */

#include "menu.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "eit.h"
#include "i18n.h"

#define MENUTIMEOUT 120 // seconds

const char *FileNameChars = " aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789-.#^";

// --- cMenuEditItem ---------------------------------------------------------

class cMenuEditItem : public cOsdItem {
private:
  const char *name;
  const char *value;
public:
  cMenuEditItem(const char *Name);
  ~cMenuEditItem();
  void SetValue(const char *Value);
  };

cMenuEditItem::cMenuEditItem(const char *Name)
{
  name = strdup(Name);
  value = NULL;
}

cMenuEditItem::~cMenuEditItem()
{
  delete name;
  delete value;
}

void cMenuEditItem::SetValue(const char *Value)
{
  delete value;
  value = strdup(Value);
  char *buffer = NULL;
  asprintf(&buffer, "%s:\t%s", name, value);
  SetText(buffer, false);
  Display();
}

// --- cMenuEditIntItem ------------------------------------------------------

class cMenuEditIntItem : public cMenuEditItem {
protected:
  int *value;
  int min, max;
  virtual void Set(void);
public:
  cMenuEditIntItem(const char *Name, int *Value, int Min = 0, int Max = INT_MAX);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditIntItem::cMenuEditIntItem(const char *Name, int *Value, int Min, int Max)
:cMenuEditItem(Name)
{
  value = Value;
  min = Min;
  max = Max;
  Set();
}

void cMenuEditIntItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", *value);
  SetValue(buf);
}

eOSState cMenuEditIntItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue;
     if (k0 <= Key && Key <= k9) {
        if (fresh) {
           *value = 0;
           fresh = false;
           }
        newValue = *value  * 10 + (Key - k0);
        }
     else if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        newValue = *value - 1;
        fresh = true;
        }
     else if (NORMALKEY(Key) == kRight) {
        newValue = *value + 1;
        fresh = true;
        }
     else
        return state;
     if ((!fresh || min <= newValue) && newValue <= max) {
        *value = newValue;
        Set();
        }
     state = osContinue;
     }
  return state;
}

// --- cMenuEditBoolItem -----------------------------------------------------

class cMenuEditBoolItem : public cMenuEditIntItem {
protected:
  const char *falseString, *trueString;
  virtual void Set(void);
public:
  cMenuEditBoolItem(const char *Name, int *Value, const char *FalseString = NULL, const char *TrueString = NULL);
  };

cMenuEditBoolItem::cMenuEditBoolItem(const char *Name, int *Value, const char *FalseString, const char *TrueString)
:cMenuEditIntItem(Name, Value, 0, 1)
{
  falseString = FalseString ? FalseString : tr("no");
  trueString = TrueString ? TrueString : tr("yes");
  Set();
}

void cMenuEditBoolItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%s", *value ? trueString : falseString);
  SetValue(buf);
}

// --- cMenuEditChanItem -----------------------------------------------------

class cMenuEditChanItem : public cMenuEditIntItem {
protected:
  virtual void Set(void);
public:
  cMenuEditChanItem(const char *Name, int *Value);
  };

cMenuEditChanItem::cMenuEditChanItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 1, Channels.MaxNumber())
{
  Set();
}

void cMenuEditChanItem::Set(void)
{
  char buf[255];
  cChannel *channel = Channels.GetByNumber(*value);
  if (channel)
     snprintf(buf, sizeof(buf), "%d %s", *value, channel->name);
  else
     *buf = 0;
  SetValue(buf);
}

// --- cMenuEditDayItem ------------------------------------------------------

class cMenuEditDayItem : public cMenuEditIntItem {
protected:
  static int days[];
  int d;
  virtual void Set(void);
public:
  cMenuEditDayItem(const char *Name, int *Value);
  virtual eOSState ProcessKey(eKeys Key);
  };

int cMenuEditDayItem::days[] ={ cTimer::ParseDay("M------"),
                                cTimer::ParseDay("-T-----"),
                                cTimer::ParseDay("--W----"),
                                cTimer::ParseDay("---T---"),
                                cTimer::ParseDay("----F--"),
                                cTimer::ParseDay("-----S-"),
                                cTimer::ParseDay("------S"),
                                cTimer::ParseDay("MTWTF--"),
                                cTimer::ParseDay("MTWTFS-"),
                                cTimer::ParseDay("MTWTFSS"),
                                cTimer::ParseDay("-----SS"),
                                0 };

cMenuEditDayItem::cMenuEditDayItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, -INT_MAX, 31)
{
  d = -1;
  if (*value < 0) {
     int n = 0;
     while (days[n]) {
           if (days[n] == *value) {
              d = n;
              break;
              }
           n++;
           }
     }
  Set();
}

void cMenuEditDayItem::Set(void)
{
  SetValue(cTimer::PrintDay(*value));
}

eOSState cMenuEditDayItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kLeft|k_Repeat:
    case kLeft:  if (d > 0)
                    *value = days[--d];
                 else if (d == 0) {
                    *value = 31;
                    d = -1;
                    }
                 else if (*value == 1) {
                    d = sizeof(days) / sizeof(int) - 2;
                    *value = days[d];
                    }
                 else
                    return cMenuEditIntItem::ProcessKey(Key);
                 Set();
                 break;
    case kRight|k_Repeat:
    case kRight: if (d >= 0) {
                    *value = days[++d];
                    if (*value == 0) {
                       *value = 1;
                       d = -1;
                       }
                    }
                 else if (*value == 31) {
                    d = 0;
                    *value = days[d];
                    }
                 else
                    return cMenuEditIntItem::ProcessKey(Key);
                 Set();
                 break;
    default : return cMenuEditIntItem::ProcessKey(Key);
    }
  return osContinue;
}

// --- cMenuEditTimeItem -----------------------------------------------------

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

cMenuEditTimeItem::cMenuEditTimeItem(const char *Name, int *Value)
:cMenuEditItem(Name)
{
  value = Value;
  hh = *value / 100;
  mm = *value % 100;
  pos = 0;
  Set();
}

void cMenuEditTimeItem::Set(void)
{
  char buf[10];
  switch (pos) {
    case 1:  snprintf(buf, sizeof(buf), "%01d-:--", hh / 10); break;
    case 2:  snprintf(buf, sizeof(buf), "%02d:--", hh); break;
    case 3:  snprintf(buf, sizeof(buf), "%02d:%01d-", hh, mm / 10); break;
    default: snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    }
  SetValue(buf);
}

eOSState cMenuEditTimeItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (k0 <= Key && Key <= k9) {
        if (fresh || pos > 3) {
           pos = 0;
           fresh = false;
           }
        int n = Key - k0;
        switch (pos) {
          case 0: if (n <= 2) {
                     hh = n * 10;
                     mm = 0;
                     pos++;
                     }
                  break;
          case 1: if (hh + n <= 23) {
                     hh += n;
                     pos++;
                     }
                  break;
          case 2: if (n <= 5) {
                     mm += n * 10;
                     pos++;
                     }
                  break;
          case 3: if (mm + n <= 59) {
                     mm += n;
                     pos++;
                     }
                  break;
          }
        }
     else if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (--mm < 0) {
           mm = 59;
           if (--hh < 0)
              hh = 23;
           }
        fresh = true;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (++mm > 59) {
           mm = 0;
           if (++hh > 23)
              hh = 0;
           }
        fresh = true;
        }
     else
        return state;
     *value = hh * 100 + mm;
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditChrItem ------------------------------------------------------

class cMenuEditChrItem : public cMenuEditItem {
private:
  char *value;
  const char *allowed;
  const char *current;
  virtual void Set(void);
public:
  cMenuEditChrItem(const char *Name, char *Value, const char *Allowed);
  ~cMenuEditChrItem();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditChrItem::cMenuEditChrItem(const char *Name, char *Value, const char *Allowed)
:cMenuEditItem(Name)
{
  value = Value;
  allowed = strdup(Allowed);
  current = strchr(allowed, *Value);
  if (!current)
     current = allowed;
  Set();
}

cMenuEditChrItem::~cMenuEditChrItem()
{
  delete allowed;
}

void cMenuEditChrItem::Set(void)
{
  char buf[2];
  snprintf(buf, sizeof(buf), "%c", *value);
  SetValue(buf);
}

eOSState cMenuEditChrItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft) {
        if (current > allowed)
           current--;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (*(current + 1))
           current++;
        }
     else
        return state;
     *value = *current;
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditStrItem ------------------------------------------------------

class cMenuEditStrItem : public cMenuEditItem {
private:
  char *value;
  int length;
  const char *allowed;
  int pos;
  virtual void Set(void);
  char Inc(char c, bool Up);
public:
  cMenuEditStrItem(const char *Name, char *Value, int Length, const char *Allowed);
  ~cMenuEditStrItem();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditStrItem::cMenuEditStrItem(const char *Name, char *Value, int Length, const char *Allowed)
:cMenuEditItem(Name)
{
  value = Value;
  length = Length;
  allowed = strdup(Allowed);
  pos = -1;
  Set();
}

cMenuEditStrItem::~cMenuEditStrItem()
{
  delete allowed;
}

void cMenuEditStrItem::Set(void)
{
  char buf[1000];
  if (pos >= 0) {
     strncpy(buf, value, pos);
     const char *s = value[pos] != '^' ? value + pos + 1 : "";
     snprintf(buf + pos, sizeof(buf) - pos - 2, "[%c]%s", *(value + pos), s);
     SetValue(buf);
     }
  else
     SetValue(value);
}

char cMenuEditStrItem::Inc(char c, bool Up)
{
  const char *p = strchr(allowed, c);
  if (!p)
     p = allowed;
  if (Up) {
     if (!*++p)
        p = allowed;
     }
  else if (--p < allowed)
     p = allowed + strlen(allowed) - 1;
  return *p;
}

eOSState cMenuEditStrItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kLeft|k_Repeat:
    case kLeft:  if (pos > 0) {
                    if (value[pos] == '^')
                       value[pos] = 0;
                    pos--;
                    }
                 break;
    case kRight|k_Repeat:
    case kRight: if (pos < length && value[pos] != '^' && (pos < int(strlen(value) - 1) || value[pos] != ' ')) {
                    if (++pos >= int(strlen(value))) {
                       value[pos] = ' ';
                       value[pos + 1] = 0;
                       }
                    }
                 break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:  if (pos >= 0)
                    value[pos] = Inc(value[pos], NORMALKEY(Key) == kUp);
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 break;
    case kOk:    if (pos >= 0) {
                    if (value[pos] == '^')
                       value[pos] = 0;
                    pos = -1;
                    break;
                    }
                 // run into default
    default:     return cMenuEditItem::ProcessKey(Key);
    }
  Set();
  return osContinue;
}

// --- cMenuEditStraItem -----------------------------------------------------

class cMenuEditStraItem : public cMenuEditIntItem {
private:
  const char * const *strings;
protected:
  virtual void Set(void);
public:
  cMenuEditStraItem(const char *Name, int *Value, int NumStrings, const char * const *Strings);
  };

cMenuEditStraItem::cMenuEditStraItem(const char *Name, int *Value, int NumStrings, const char * const *Strings)
:cMenuEditIntItem(Name, Value, 0, NumStrings - 1)
{
  strings = Strings;
  Set();
}

void cMenuEditStraItem::Set(void)
{
  SetValue(strings[*value]);
}

// --- cMenuEditChannel ------------------------------------------------------

class cMenuEditChannel : public cOsdMenu {
private:
  cChannel *channel;
  cChannel data;
public:
  cMenuEditChannel(int Index);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditChannel::cMenuEditChannel(int Index)
:cOsdMenu(tr("Edit Channel"), 14)
{
  channel = Channels.Get(Index);
  if (channel) {
     data = *channel;
     Add(new cMenuEditStrItem( tr("Name"),          data.name, sizeof(data.name), FileNameChars));
     Add(new cMenuEditIntItem( tr("Frequency"),    &data.frequency, 10000, 13000)); //TODO exact limits???
     Add(new cMenuEditChrItem( tr("Polarization"), &data.polarization, "hv"));
     Add(new cMenuEditIntItem( tr("DiSEqC"),       &data.diseqc, 0, 10)); //TODO exact limits???
     Add(new cMenuEditIntItem( tr("Srate"),        &data.srate, 22000, 30000)); //TODO exact limits - toggle???
     Add(new cMenuEditIntItem( tr("Vpid"),         &data.vpid, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("Apid1"),        &data.apid1, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("Apid2"),        &data.apid2, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("Dpid1"),        &data.dpid1, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("Dpid2"),        &data.dpid2, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("Tpid"),         &data.tpid, 0, 0xFFFE));
     Add(new cMenuEditIntItem( tr("CA"),           &data.ca, 0, cDvbApi::NumDvbApis));
     Add(new cMenuEditIntItem( tr("Pnr"),          &data.pnr, 0));
     }
}

eOSState cMenuEditChannel::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     if (Key == kOk) {
        if (channel)
           *channel = data;
        Channels.Save();
        state = osBack;
        }
     }
  return state;
}

// --- cMenuChannelItem ------------------------------------------------------

class cMenuChannelItem : public cOsdItem {
private:
  int index;
  cChannel *channel;
public:
  cMenuChannelItem(int Index, cChannel *Channel);
  virtual void Set(void);
  void SetIndex(int Index);
  };

cMenuChannelItem::cMenuChannelItem(int Index, cChannel *Channel)
{
  index = Index;
  channel = Channel;
  if (channel->groupSep)
     SetColor(clrCyan, clrBackground);
  Set();
}

void cMenuChannelItem::Set(void)
{
  char *buffer = NULL;
  if (!channel->groupSep)
     asprintf(&buffer, "%d\t%s", channel->number, channel->name );
  else
     asprintf(&buffer, "---\t%s ----------------------------------------------------------------", channel->name);
  SetText(buffer, false);
}

void cMenuChannelItem::SetIndex(int Index)
{
  index = Index;
  Set();
}

// --- cMenuChannels ---------------------------------------------------------

class cMenuChannels : public cOsdMenu {
protected:
  eOSState Switch(void);
  eOSState Edit(void);
  eOSState New(void);
  eOSState Del(void);
  virtual void Move(int From, int To);
public:
  cMenuChannels(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuChannels::cMenuChannels(void)
:cOsdMenu(tr("Channels"), 4)
{
  //TODO
  int i = 0;
  cChannel *channel;
  int curr = ((channel = Channels.GetByNumber(cDvbApi::CurrentChannel())) != NULL) ? channel->Index() : -1;

  while ((channel = Channels.Get(i)) != NULL) {
        Add(new cMenuChannelItem(i, channel), i == curr);
        i++;
        }
  SetHelp(tr("Edit"), tr("New"), tr("Delete"), tr("Mark"));
}

eOSState cMenuChannels::Switch(void)
{
  cChannel *ch = Channels.Get(Current());
  if (ch)
     ch->Switch();
  return osEnd;
}

eOSState cMenuChannels::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  isyslog(LOG_INFO, "editing channel %d", Current() + 1);
  return AddSubMenu(new cMenuEditChannel(Current()));
}

eOSState cMenuChannels::New(void)
{
  if (HasSubMenu())
     return osContinue;
  cChannel *channel = new cChannel(Channels.Get(Current()));
  Channels.Add(channel);
  Channels.ReNumber();
  Add(new cMenuChannelItem(channel->Index()/*XXX*/, channel), true);
  Channels.Save();
  isyslog(LOG_INFO, "channel %d added", channel->number);
  return AddSubMenu(new cMenuEditChannel(Current()));
}

eOSState cMenuChannels::Del(void)
{
  if (Count() > 0) {
     int Index = Current();
     cChannel *channel = Channels.Get(Index);
     int DeletedChannel = channel->number;
     // Check if there is a timer using this channel:
     for (cTimer *ti = Timers.First(); ti; ti = (cTimer *)ti->Next()) {
         if (ti->channel == DeletedChannel) {
            Interface->Error(tr("Channel is being used by a timer!"));
            return osContinue;
            }
         }
     if (Interface->Confirm(tr("Delete channel?"))) {
        // Move and renumber the channels:
        Channels.Del(channel);
        Channels.ReNumber();
        cOsdMenu::Del(Index);
        int i = 0;
        for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next())
            ci->SetIndex(i++);
        Channels.Save();
        isyslog(LOG_INFO, "channel %d deleted", DeletedChannel);
        // Fix the timers:
        bool TimersModified = false;
        for (cTimer *ti = Timers.First(); ti; ti = (cTimer *)ti->Next()) {
            int OldChannel = ti->channel;
            if (ti->channel > DeletedChannel)
               ti->channel--;
            if (ti->channel != OldChannel) {
               TimersModified = true;
               isyslog(LOG_INFO, "timer %d: channel changed from %d to %d", ti->Index() + 1, OldChannel, ti->channel);
               }
            }
        if (TimersModified)
           Timers.Save();
        Display();
        }
     }
  return osContinue;
}

void cMenuChannels::Move(int From, int To)
{
  int FromNumber = Channels.Get(From)->number;
  int ToNumber = Channels.Get(To)->number;
  // Move and renumber the channels:
  Channels.Move(From, To);
  Channels.ReNumber();
  cOsdMenu::Move(From, To);
  int i = 0;
  for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next())
      ci->SetIndex(i++);
  Channels.Save();
  isyslog(LOG_INFO, "channel %d moved to %d", FromNumber, ToNumber);
  // Fix the timers:
  bool TimersModified = false;
  From++; // user visible channel numbers start with '1'
  To++;
  for (cTimer *ti = Timers.First(); ti; ti = (cTimer *)ti->Next()) {
      int OldChannel = ti->channel;
      if (ti->channel == FromNumber)
         ti->channel = ToNumber;
      else if (ti->channel > FromNumber && ti->channel <= ToNumber)
         ti->channel--;
      else if (ti->channel < FromNumber && ti->channel >= ToNumber)
         ti->channel++;
      if (ti->channel != OldChannel) {
         TimersModified = true;
         isyslog(LOG_INFO, "timer %d: channel changed from %d to %d", ti->Index() + 1, OldChannel, ti->channel);
         }
      }
  if (TimersModified)
     Timers.Save();
  Display();
}

eOSState cMenuChannels::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Switch();
       case kRed:    return Edit();
       case kGreen:  return New();
       case kYellow: return Del();
       case kBlue:   Mark(); break;
       default: break;
       }
     }
  return state;
}

// --- cMenuTextItem ---------------------------------------------------------

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
  void ScrollUp(void);
  void ScrollDown(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuTextItem::cMenuTextItem(const char *Text, int X, int Y, int W, int H, eDvbColor FgColor, eDvbColor BgColor, eDvbFont Font)
{
  x = X;
  y = Y;
  w = W;
  h = H;
  fgColor = FgColor;
  bgColor = BgColor;
  font = Font;
  offset = 0;
  eDvbFont oldFont = Interface->SetFont(font);
  text = Interface->WrapText(Text, w - 1, &lines);
  Interface->SetFont(oldFont);
  if (h < 0)
     h = lines;
}

cMenuTextItem::~cMenuTextItem()
{
  delete text;
}

void cMenuTextItem::Clear(void)
{
  Interface->Fill(x, y, w, h, bgColor);
}

void cMenuTextItem::Display(int Offset, eDvbColor FgColor, eDvbColor BgColor)
{
  int l = 0;
  char *t = text;
  eDvbFont oldFont = Interface->SetFont(font);
  while (*t) {
        char *n = strchr(t, '\n');
        if (l >= offset) {
           if (n)
              *n = 0;
           Interface->Write(x, y + l - offset, t, fgColor, bgColor);
           if (n)
              *n = '\n';
           else
              break;
           }
        if (!n)
           break;
        t = n + 1;
        if (++l >= h + offset)
           break;
        }
  Interface->SetFont(oldFont);
  // scroll indicators use inverted color scheme!
  if (CanScrollUp())   Interface->Write(x + w - 1, y,         "^", bgColor, fgColor);
  if (CanScrollDown()) Interface->Write(x + w - 1, y + h - 1, "v", bgColor, fgColor);
}

void cMenuTextItem::ScrollUp(void)
{
  if (CanScrollUp()) {
     Clear();
     offset--;
     Display();
     }
}

void cMenuTextItem::ScrollDown(void)
{
  if (CanScrollDown()) {
     Clear();
     offset++;
     Display();
     }
}

eOSState cMenuTextItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kUp|k_Repeat:
    case kUp:            ScrollUp();   break;
    case kDown|k_Repeat:
    case kDown:          ScrollDown(); break;
    default:             return osUnknown;
    }
  return osContinue;
}

// --- cMenuText -------------------------------------------------------------

class cMenuText : public cOsdMenu {
public:
  cMenuText(const char *Title, const char *Text, eDvbFont Font = fontOsd);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuText::cMenuText(const char *Title, const char *Text, eDvbFont Font)
:cOsdMenu(Title)
{
  Add(new cMenuTextItem(Text, 1, 2, Setup.OSDwidth - 2, MAXOSDITEMS, clrWhite, clrBackground, Font));
}

eOSState cMenuText::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk: return osBack;
       default:  state = osContinue;
       }
     }
  return state;
}

// --- cMenuEditTimer --------------------------------------------------------

class cMenuEditTimer : public cOsdMenu {
private:
  cTimer *timer;
  cTimer data;
public:
  cMenuEditTimer(int Index, bool New = false);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditTimer::cMenuEditTimer(int Index, bool New)
:cOsdMenu(tr("Edit Timer"), 12)
{
  timer = Timers.Get(Index);
  if (timer) {
     data = *timer;
     if (New)
        data.active = 1;
     Add(new cMenuEditBoolItem(tr("Active"),       &data.active));
     Add(new cMenuEditChanItem(tr("Channel"),      &data.channel));
     Add(new cMenuEditDayItem( tr("Day"),          &data.day));
     Add(new cMenuEditTimeItem(tr("Start"),        &data.start));
     Add(new cMenuEditTimeItem(tr("Stop"),         &data.stop));
//TODO VPS???
     Add(new cMenuEditIntItem( tr("Priority"),     &data.priority, 0, MAXPRIORITY));
     Add(new cMenuEditIntItem( tr("Lifetime"),     &data.lifetime, 0, MAXLIFETIME));
     Add(new cMenuEditStrItem( tr("File"),          data.file, sizeof(data.file), FileNameChars));
     }
}

eOSState cMenuEditTimer::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     if (!*data.file)
                        strcpy(data.file, Channels.GetChannelNameByNumber(data.channel));
                     if (timer && memcmp(timer, &data, sizeof(data)) != 0) {
                        *timer = data;
                        if (timer->active)
                           timer->active = 1; // allows external programs to mark active timers with values > 1 and recognize if the user has modified them
                        Timers.Save();
                        isyslog(LOG_INFO, "timer %d modified (%s)", timer->Index() + 1, timer->active ? "active" : "inactive");
                        }
                     return osBack;
       case kRed:
       case kGreen:
       case kYellow:
       case kBlue:   return osContinue;
       default: break;
       }
     }
  return state;
}

// --- cMenuTimerItem --------------------------------------------------------

class cMenuTimerItem : public cOsdItem {
private:
  int index;
  cTimer *timer;
public:
  cMenuTimerItem(int Index, cTimer *Timer);
  virtual void Set(void);
  };

cMenuTimerItem::cMenuTimerItem(int Index, cTimer *Timer)
{
  index = Index;
  timer = Timer;
  Set();
}

void cMenuTimerItem::Set(void)
{
  char *buffer = NULL;
  asprintf(&buffer, "%c\t%d\t%s\t%02d:%02d\t%02d:%02d\t%s",
                    timer->active ? '>' : ' ',
                    timer->channel,
                    timer->PrintDay(timer->day),
                    timer->start / 100,
                    timer->start % 100,
                    timer->stop / 100,
                    timer->stop % 100,
                    timer->file);
  SetText(buffer, false);
}

// --- cMenuTimers -----------------------------------------------------------

class cMenuTimers : public cOsdMenu {
private:
  eOSState Activate(bool On);
  eOSState Edit(void);
  eOSState New(void);
  eOSState Del(void);
  virtual void Move(int From, int To);
  eOSState Summary(void);
public:
  cMenuTimers(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuTimers::cMenuTimers(void)
:cOsdMenu(tr("Timers"), 2, 4, 10, 6, 6)
{
  int i = 0;
  cTimer *timer;

  while ((timer = Timers.Get(i)) != NULL) {
        Add(new cMenuTimerItem(i, timer));
        i++;
        }
  SetHelp(tr("Edit"), tr("New"), tr("Delete"), tr("Mark"));
}

eOSState cMenuTimers::Activate(bool On)
{
  cTimer *timer = Timers.Get(Current());
  if (timer && timer->active != On) {
     timer->active = On;
     RefreshCurrent();
     DisplayCurrent(true);
     isyslog(LOG_INFO, "timer %d %sactivated", timer->Index() + 1, timer->active ? "" : "de");
     Timers.Save();
     }
  return osContinue;
}

eOSState cMenuTimers::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  isyslog(LOG_INFO, "editing timer %d", Current() + 1);
  return AddSubMenu(new cMenuEditTimer(Current()));
}

eOSState cMenuTimers::New(void)
{
  if (HasSubMenu())
     return osContinue;
  cTimer *timer = new cTimer;
  Timers.Add(timer);
  Add(new cMenuTimerItem(timer->Index()/*XXX*/, timer), true);
  Timers.Save();
  isyslog(LOG_INFO, "timer %d added", timer->Index() + 1);
  return AddSubMenu(new cMenuEditTimer(Current(), true));
}

eOSState cMenuTimers::Del(void)
{
  // Check if this timer is active:
  int Index = Current();
  cTimer *ti = Timers.Get(Index);
  if (ti) {
     if (!ti->recording) {
        if (Interface->Confirm(tr("Delete timer?"))) {
           Timers.Del(Timers.Get(Index));
           cOsdMenu::Del(Index);
           Timers.Save();
           Display();
           isyslog(LOG_INFO, "timer %d deleted", Index + 1);
           }
        }
     else
        Interface->Error(tr("Timer is recording!"));
     }
  return osContinue;
}

void cMenuTimers::Move(int From, int To)
{
  Timers.Move(From, To);
  cOsdMenu::Move(From, To);
  Timers.Save();
  Display();
  isyslog(LOG_INFO, "timer %d moved to %d", From + 1, To + 1);
}

eOSState cMenuTimers::Summary(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cTimer *ti = Timers.Get(Current());
  if (ti && ti->summary && *ti->summary)
     return AddSubMenu(new cMenuText(tr("Summary"), ti->summary));
  return Edit(); // convenience for people not using the Summary feature ;-)
}

eOSState cMenuTimers::ProcessKey(eKeys Key)
{
  // Must do these before calling cOsdMenu::ProcessKey() because cOsdMenu
  // uses them to page up/down:
  if (!HasSubMenu()) {
     switch (Key) {
       case kLeft:
       case kRight:  return Activate(Key == kRight);
       default: break;
       }
     }

  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kLeft:
       case kRight:  return Activate(Key == kRight);
       case kOk:     return Summary();
       case kRed:    return Edit();
       case kGreen:  return New();
       case kYellow: return Del();
       case kBlue:   Mark(); break;
       default: break;
       }
     }
  return state;
}

// --- cMenuEvent ------------------------------------------------------------

class cMenuEvent : public cOsdMenu {
private:
  const cEventInfo *eventInfo;
public:
  cMenuEvent(const cEventInfo *EventInfo, bool CanSwitch = false);
  cMenuEvent(bool Now);
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuEvent::cMenuEvent(const cEventInfo *EventInfo, bool CanSwitch)
:cOsdMenu(tr("Event"))
{
  eventInfo = EventInfo;
  if (eventInfo) {
     cChannel *channel = Channels.GetByServiceID(eventInfo->GetServiceID());
     if (channel) {
        char *buffer;
        asprintf(&buffer, "%-17.*s\t%.*s  %s - %s", 17, channel->name, 5, eventInfo->GetDate(), eventInfo->GetTimeString(), eventInfo->GetEndTimeString());
        SetTitle(buffer, false);
        int Line = 2;
        cMenuTextItem *item;
        const char *Title = eventInfo->GetTitle();
        const char *Subtitle = eventInfo->GetSubtitle();
        const char *ExtendedDescription = eventInfo->GetExtendedDescription();
        // Some channels send a 'Subtitle' that should actually be the 'ExtendedDescription'
        // (their 'ExtendedDescription' is then empty). In order to handle this correctly
        // we silently shift that text to where it belongs.
        // The German TV station 'VOX' is notorious for this - why can't they do it correctly
        // like all the others? Well, at least like those who actually send the full range
        // of information (like, e.g., 'Sat.1'). Some stations (like 'RTL') don't even
        // bother sending anything but the 'Title'...
        if (isempty(ExtendedDescription) && !isempty(Subtitle) && int(strlen(Subtitle)) > 2 * Setup.OSDwidth) {
           ExtendedDescription = Subtitle;
           Subtitle = NULL;
           }
        if (!isempty(Title)) {
           Add(item = new cMenuTextItem(Title, 1, Line, Setup.OSDwidth - 2, -1, clrCyan));
           Line += item->Height() + 1;
           }
        if (!isempty(Subtitle)) {
           Add(item = new cMenuTextItem(Subtitle, 1, Line, Setup.OSDwidth - 2, -1, clrYellow));
           Line += item->Height() + 1;
           }
        if (!isempty(ExtendedDescription))
           Add(new cMenuTextItem(ExtendedDescription, 1, Line, Setup.OSDwidth - 2, Height() - Line - 2, clrCyan), true);
        SetHelp(tr("Record"), NULL, NULL, CanSwitch ? tr("Switch") : NULL);
        }
     }
}

eOSState cMenuEvent::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kGreen:
       case kYellow: return osContinue;
       case kOk:     return osBack;
       default: break;
       }
     }
  return state;
}

// --- cMenuWhatsOnItem ------------------------------------------------------

class cMenuWhatsOnItem : public cOsdItem {
public:
  const cEventInfo *eventInfo;
  cMenuWhatsOnItem(const cEventInfo *EventInfo);
};

cMenuWhatsOnItem::cMenuWhatsOnItem(const cEventInfo *EventInfo)
{
  eventInfo = EventInfo;
  char *buffer = NULL;
  cChannel *channel = Channels.GetByNumber(eventInfo->GetChannelNumber());
  asprintf(&buffer, "%d\t%.*s\t%.*s\t%s", eventInfo->GetChannelNumber(), 6, channel ? channel->name : "???", 5, eventInfo->GetTimeString(), eventInfo->GetTitle());
  SetText(buffer, false);
}

// --- cMenuWhatsOn ----------------------------------------------------------

class cMenuWhatsOn : public cOsdMenu {
private:
  eOSState Record(void);
  eOSState Switch(void);
  static int currentChannel;
  static const cEventInfo *scheduleEventInfo;
public:
  cMenuWhatsOn(const cSchedules *Schedules, bool Now, int CurrentChannelNr);
  static int CurrentChannel(void) { return currentChannel; }
  static void SetCurrentChannel(int ChannelNr) { currentChannel = ChannelNr; }
  static const cEventInfo *ScheduleEventInfo(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

int cMenuWhatsOn::currentChannel = 0;
const cEventInfo *cMenuWhatsOn::scheduleEventInfo = NULL;

static int CompareEventChannel(const void *p1, const void *p2)
{
  return (int)( (*(const cEventInfo **)p1)->GetChannelNumber() - (*(const cEventInfo **)p2)->GetChannelNumber());
}

cMenuWhatsOn::cMenuWhatsOn(const cSchedules *Schedules, bool Now, int CurrentChannelNr)
:cOsdMenu(Now ? tr("What's on now?") : tr("What's on next?"), 4, 7, 6)
{
  const cSchedule *Schedule = Schedules->First();
  const cEventInfo **pArray = NULL;
  int num = 0;

  while (Schedule) {
        pArray = (const cEventInfo **)realloc(pArray, (num + 1) * sizeof(cEventInfo *));

        pArray[num] = Now ? Schedule->GetPresentEvent() : Schedule->GetFollowingEvent();
        if (pArray[num]) {
           cChannel *channel = Channels.GetByServiceID(pArray[num]->GetServiceID());
           if (channel) {
              pArray[num]->SetChannelNumber(channel->number);
              num++;
              }
           }
        Schedule = (const cSchedule *)Schedules->Next(Schedule);
        }

  qsort(pArray, num, sizeof(cEventInfo *), CompareEventChannel);

  for (int a = 0; a < num; a++)
      Add(new cMenuWhatsOnItem(pArray[a]), pArray[a]->GetChannelNumber() == CurrentChannelNr);

  currentChannel = CurrentChannelNr;
  delete pArray;
  SetHelp(tr("Record"), Now ? tr("Next") : tr("Now"), tr("Schedule"), tr("Switch"));
}

const cEventInfo *cMenuWhatsOn::ScheduleEventInfo(void)
{
  const cEventInfo *ei = scheduleEventInfo;
  scheduleEventInfo = NULL;
  return ei;
}

eOSState cMenuWhatsOn::Switch(void)
{
  cMenuWhatsOnItem *item = (cMenuWhatsOnItem *)Get(Current());
  if (item) {
     cChannel *channel = Channels.GetByServiceID(item->eventInfo->GetServiceID());
     if (channel && channel->Switch())
        return osEnd;
     }
  Interface->Error(tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuWhatsOn::Record(void)
{
  cMenuWhatsOnItem *item = (cMenuWhatsOnItem *)Get(Current());
  if (item) {
     cTimer *timer = new cTimer(item->eventInfo);
     cTimer *t = Timers.GetTimer(timer);
     if (!t) {
        Timers.Add(timer);
        Timers.Save();
        isyslog(LOG_INFO, "timer %d added", timer->Index() + 1);
        }
     else {
        delete timer;
        timer = t;
        }
     return AddSubMenu(new cMenuEditTimer(timer->Index(), true));
     }
  return osContinue;
}

eOSState cMenuWhatsOn::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:    return Record();
       case kYellow: state = osBack;
                     // continue with kGreen
       case kGreen:  {
                       cMenuWhatsOnItem *mi = (cMenuWhatsOnItem *)Get(Current());
                       if (mi) {
                          scheduleEventInfo = mi->eventInfo;
                          currentChannel = mi->eventInfo->GetChannelNumber();
                          }
                     }
                     break;
       case kBlue:   return Switch();
       case kOk:     if (Count())
                        return AddSubMenu(new cMenuEvent(((cMenuWhatsOnItem *)Get(Current()))->eventInfo, true));
                     break;
       default:      break;
       }
     }
  return state;
}

// --- cMenuScheduleItem -----------------------------------------------------

class cMenuScheduleItem : public cOsdItem {
public:
  const cEventInfo *eventInfo;
  cMenuScheduleItem(const cEventInfo *EventInfo);
};

cMenuScheduleItem::cMenuScheduleItem(const cEventInfo *EventInfo)
{
  eventInfo = EventInfo;
  char *buffer = NULL;
  asprintf(&buffer, "%.*s\t%.*s\t%s", 5, eventInfo->GetDate(), 5, eventInfo->GetTimeString(), eventInfo->GetTitle());
  SetText(buffer, false);
}

// --- cMenuSchedule ---------------------------------------------------------

class cMenuSchedule : public cOsdMenu {
private:
  cThreadLock threadLock;
  const cSchedules *schedules;
  bool now, next;
  int otherChannel;
  eOSState Record(void);
  eOSState Switch(void);
  void PrepareSchedule(cChannel *Channel);
public:
  cMenuSchedule(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSchedule::cMenuSchedule(void)
:cOsdMenu("", 6, 6)
{
  now = next = false;
  otherChannel = 0;
  cChannel *channel = Channels.GetByNumber(cDvbApi::CurrentChannel());
  if (channel) {
     cMenuWhatsOn::SetCurrentChannel(channel->number);
     schedules = cDvbApi::PrimaryDvbApi->Schedules(&threadLock);
     PrepareSchedule(channel);
     SetHelp(tr("Record"), tr("Now"), tr("Next"));
     }
}

static int CompareEventTime(const void *p1, const void *p2)
{
  return (int)((*(cEventInfo **)p1)->GetTime() - (*(cEventInfo **)p2)->GetTime());
}

void cMenuSchedule::PrepareSchedule(cChannel *Channel)
{
  Clear();
  char *buffer = NULL;
  asprintf(&buffer, tr("Schedule - %s"), Channel->name);
  SetTitle(buffer, false);
  if (schedules) {
     const cSchedule *Schedule = Channel->pnr ? schedules->GetSchedule(Channel->pnr) : schedules->GetSchedule();
     int num = Schedule->NumEvents();
     const cEventInfo **pArray = (const cEventInfo **)malloc(num * sizeof(cEventInfo *));
     if (pArray) {
        time_t now = time(NULL);
        int numreal = 0;
        for (int a = 0; a < num; a++) {
            const cEventInfo *EventInfo = Schedule->GetEventNumber(a);
            if (EventInfo->GetTime() + EventInfo->GetDuration() > now)
               pArray[numreal++] = EventInfo;
            }

        qsort(pArray, numreal, sizeof(cEventInfo *), CompareEventTime);

        for (int a = 0; a < numreal; a++)
            Add(new cMenuScheduleItem(pArray[a]));
        delete pArray;
        }
     }
}

eOSState cMenuSchedule::Record(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     cTimer *timer = new cTimer(item->eventInfo);
     cTimer *t = Timers.GetTimer(timer);
     if (!t) {
        Timers.Add(timer);
        Timers.Save();
        isyslog(LOG_INFO, "timer %d added", timer->Index() + 1);
        }
     else {
        delete timer;
        timer = t;
        }
     return AddSubMenu(new cMenuEditTimer(timer->Index(), true));
     }
  return osContinue;
}

eOSState cMenuSchedule::Switch(void)
{
  if (otherChannel) {
     if (Channels.SwitchTo(otherChannel))
        return osEnd;
     }
  Interface->Error(tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuSchedule::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:    return Record();
       case kGreen:  if (schedules) {
                        if (!now && !next) {
                           int ChannelNr = 0;
                           if (Count()) {
                              cChannel *channel = Channels.GetByServiceID(((cMenuScheduleItem *)Get(Current()))->eventInfo->GetServiceID());
                              if (channel)
                                 ChannelNr = channel->number;
                              }
                           now = true;
                           return AddSubMenu(new cMenuWhatsOn(schedules, true, ChannelNr));
                           }
                        now = !now;
                        next = !next;
                        return AddSubMenu(new cMenuWhatsOn(schedules, now, cMenuWhatsOn::CurrentChannel()));
                        }
       case kYellow: if (schedules)
                        return AddSubMenu(new cMenuWhatsOn(schedules, false, cMenuWhatsOn::CurrentChannel()));
                     break;
       case kBlue:   if (Count())
                        return Switch();
                     break;
       case kOk:     if (Count())
                        return AddSubMenu(new cMenuEvent(((cMenuScheduleItem *)Get(Current()))->eventInfo, otherChannel));
                     break;
       default:      break;
       }
     }
  else if (!HasSubMenu()) {
     now = next = false;
     const cEventInfo *ei = cMenuWhatsOn::ScheduleEventInfo();
     if (ei) {
        cChannel *channel = Channels.GetByServiceID(ei->GetServiceID());
        if (channel) {
           PrepareSchedule(channel);
           if (channel->number != cDvbApi::CurrentChannel()) {
              otherChannel = channel->number;
              SetHelp(tr("Record"), tr("Now"), tr("Next"), tr("Switch"));
              }
           Display();
           }
        }
     }
  return state;
}

// --- cMenuRecordingItem ----------------------------------------------------

class cMenuRecordingItem : public cOsdItem {
public:
  cRecording *recording;
  cMenuRecordingItem(cRecording *Recording);
  virtual void Set(void);
  };

cMenuRecordingItem::cMenuRecordingItem(cRecording *Recording)
{
  recording = Recording;
  Set();
}

void cMenuRecordingItem::Set(void)
{
  SetText(recording->Title('\t', true));
}

// --- cMenuRecordings -------------------------------------------------------

cMenuRecordings::cMenuRecordings(void)
:cOsdMenu(tr("Recordings"), 6, 6)
{
  if (Recordings.Load()) {
     const char *lastReplayed = cReplayControl::LastReplayed();
     cRecording *recording = Recordings.First();
     while (recording) {
           Add(new cMenuRecordingItem(recording), lastReplayed && strcmp(lastReplayed, recording->FileName()) == 0);
           recording = Recordings.Next(recording);
           }
     }
  SetHelp(tr("Play"), tr("Rewind"), tr("Delete"), tr("Summary"));
  Display();
}

eOSState cMenuRecordings::Play(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri) {
     cReplayControl::SetRecording(ri->recording->FileName(), ri->recording->Title());
     return osReplay;
     }
  return osContinue;
}

eOSState cMenuRecordings::Rewind(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri) {
     cDvbApi::PrimaryDvbApi->StopReplay(); // must do this first to be able to rewind the currently replayed recording
     cResumeFile ResumeFile(ri->recording->FileName());
     ResumeFile.Delete();
     return Play();
     }
  return osContinue;
}

eOSState cMenuRecordings::Del(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri) {
//XXX what if this recording's file is currently in use???
//XXX     if (!ti->recording) {
        if (Interface->Confirm(tr("Delete recording?"))) {
           if (ri->recording->Delete()) {
              cReplayControl::ClearLastReplayed(ri->recording->FileName());
              cOsdMenu::Del(Current());
              Display();
              }
           else
              Interface->Error(tr("Error while deleting recording!"));
           }
//XXX        }
//XXX     else
//XXX        Interface->Error(tr("Timer is recording!"));
     }
  return osContinue;
}

eOSState cMenuRecordings::Summary(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && ri->recording->Summary() && *ri->recording->Summary())
     return AddSubMenu(new cMenuText(tr("Summary"), ri->recording->Summary()));
  return osContinue;
}

eOSState cMenuRecordings::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:
       case kRed:    return Play();
       case kGreen:  return Rewind();
       case kYellow: return Del();
       case kBlue:   return Summary();
       case kMenu:   return osEnd;
       default: break;
       }
     }
  return state;
}

#ifdef DVDSUPPORT
// --- cMenuDVDItem ----------------------------------------------------------

class cMenuDVDItem : public cOsdItem {
  private:
  int title;
  int chapters;
  virtual void Set(void);
public:
  cMenuDVDItem(int Title, int Chapters);
  int Title(void) { return title; }
  };

cMenuDVDItem::cMenuDVDItem(int Title, int Chapters)
{
  title = Title;
  chapters = Chapters;
  Set();
}

void cMenuDVDItem::Set(void)
{
  char *buffer = NULL;
  asprintf(&buffer, " %2d.\tTitle - \t%2d\tChapters", title + 1, chapters);
  SetText(buffer, false);
}

// --- cMenuDVD --------------------------------------------------------------

cMenuDVD::cMenuDVD(void)
:cOsdMenu(tr("DVD"), 5, 8, 3)
{
  if ((dvd = cDVD::getDVD())) {
     dvd->Open();
     ifo_handle_t *vmg = dvd->openVMG();
     if (vmg) {
        dsyslog(LOG_INFO, "DVD: vmg: %p", vmg);//XXX
        tt_srpt_t *tt_srpt = vmg->tt_srpt;
        dsyslog(LOG_INFO, "DVD: tt_srpt: %p", tt_srpt);//XXX
        for (int i = 0; i < tt_srpt->nr_of_srpts; i++)
            Add(new cMenuDVDItem(i, tt_srpt->title[i].nr_of_ptts));
        }
     }
  SetHelp(tr("Play"), NULL, NULL, NULL);
  Display();
}

eOSState cMenuDVD::Play(void)
{
  cMenuDVDItem *ri = (cMenuDVDItem *)Get(Current());
  if (ri) {
     cReplayControl::SetDVD(dvd, ri->Title());
     isyslog(LOG_INFO, "DVD: playing title %d", ri->Title());
     return osReplay;
     }
  return osContinue;
}

eOSState cMenuDVD::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:
       case kRed:    return Play();
       case kMenu:   return osEnd;
       default: break;
       }
     }
  return state;
}
#endif //DVDSUPPORT

// --- cMenuSetup ------------------------------------------------------------

class cMenuSetup : public cOsdMenu {
private:
  cSetup data;
  int osdLanguage;
  void Set(void);
public:
  cMenuSetup(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetup::cMenuSetup(void)
:cOsdMenu("", 25)
{
  data = Setup;
  osdLanguage = Setup.OSDLanguage;
  Set();
}

void cMenuSetup::Set(void)
{
  Clear();
  SetTitle(tr("Setup"));
  Add(new cMenuEditStraItem(tr("OSD-Language"),       &data.OSDLanguage, NumLanguages, Languages()));
  Add(new cMenuEditIntItem( tr("PrimaryDVB"),         &data.PrimaryDVB, 1, cDvbApi::NumDvbApis));
  Add(new cMenuEditBoolItem(tr("ShowInfoOnChSwitch"), &data.ShowInfoOnChSwitch));
  Add(new cMenuEditBoolItem(tr("MenuScrollPage"),     &data.MenuScrollPage));
  Add(new cMenuEditBoolItem(tr("MarkInstantRecord"),  &data.MarkInstantRecord));
  Add(new cMenuEditIntItem( tr("LnbSLOF"),            &data.LnbSLOF));
  Add(new cMenuEditIntItem( tr("LnbFrequLo"),         &data.LnbFrequLo));
  Add(new cMenuEditIntItem( tr("LnbFrequHi"),         &data.LnbFrequHi));
  Add(new cMenuEditBoolItem(tr("DiSEqC"),             &data.DiSEqC));
  Add(new cMenuEditBoolItem(tr("SetSystemTime"),      &data.SetSystemTime));
  Add(new cMenuEditIntItem( tr("MarginStart"),        &data.MarginStart));
  Add(new cMenuEditIntItem( tr("MarginStop"),         &data.MarginStop));
  Add(new cMenuEditIntItem( tr("EPGScanTimeout"),     &data.EPGScanTimeout));
  Add(new cMenuEditIntItem( tr("SVDRPTimeout"),       &data.SVDRPTimeout));
  Add(new cMenuEditIntItem( tr("PrimaryLimit"),       &data.PrimaryLimit, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("DefaultPriority"),    &data.DefaultPriority, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("DefaultLifetime"),    &data.DefaultLifetime, 0, MAXLIFETIME));
  Add(new cMenuEditBoolItem(tr("VideoFormat"),        &data.VideoFormat, "4:3", "16:9"));
  Add(new cMenuEditBoolItem(tr("ChannelInfoPos"),     &data.ChannelInfoPos, tr("bottom"), tr("top")));
  Add(new cMenuEditIntItem( tr("OSDwidth"),           &data.OSDwidth, MINOSDWIDTH, MAXOSDWIDTH));
  Add(new cMenuEditIntItem( tr("OSDheight"),          &data.OSDheight, MINOSDHEIGHT, MAXOSDHEIGHT));
}

eOSState cMenuSetup::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk: state = (Setup.PrimaryDVB != data.PrimaryDVB) ? osSwitchDvb : osEnd;
                 cDvbApi::PrimaryDvbApi->SetUseTSTime(data.SetSystemTime);
                 cDvbApi::PrimaryDvbApi->SetVideoFormat(data.VideoFormat ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3);
                 Setup = data;
                 Setup.Save();
                 break;
       default: break;
       }
     }
  if (data.OSDLanguage != osdLanguage) {
     int OriginalOSDLanguage = Setup.OSDLanguage;
     Setup.OSDLanguage = data.OSDLanguage;
     Set();
     Display();
     osdLanguage = data.OSDLanguage;
     Setup.OSDLanguage = OriginalOSDLanguage;
     }
  return state;
}

// --- cMenuCommands ---------------------------------------------------------

class cMenuCommands : public cOsdMenu {
private:
  eOSState Execute(void);
public:
  cMenuCommands(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuCommands::cMenuCommands(void)
:cOsdMenu(tr("Commands"))
{
  int i = 0;
  cCommand *command;

  while ((command = Commands.Get(i)) != NULL) {
        Add(new cOsdItem(command->Title()));
        i++;
        }
  SetHasHotkeys();
}

eOSState cMenuCommands::Execute(void)
{
  cCommand *command = Commands.Get(Current());
  if (command) {
     const char *Result = command->Execute();
     if (Result)
        return AddSubMenu(new cMenuText(command->Title(), Result, fontFix));
     }
  return osContinue;
}

eOSState cMenuCommands::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:  return Execute();
       default:   break;
       }
     }
  return state;
}

// --- cMenuMain -------------------------------------------------------------

#define STOP_RECORDING tr(" Stop recording ")

cMenuMain::cMenuMain(bool Replaying)
:cOsdMenu(tr("Main"))
{
  digit = 0;
  Add(new cOsdItem(hk(tr("Schedule")),   osSchedule));
  Add(new cOsdItem(hk(tr("Channels")),   osChannels));
  Add(new cOsdItem(hk(tr("Timers")),     osTimers));
  Add(new cOsdItem(hk(tr("Recordings")), osRecordings));
#ifdef DVDSUPPORT
  if (cDVD::DriveExists())
  Add(new cOsdItem(hk(tr("DVD")),        osDVD));
#endif //DVDSUPPORT
  Add(new cOsdItem(hk(tr("Setup")),      osSetup));
  if (Commands.Count())
     Add(new cOsdItem(hk(tr("Commands")),  osCommands));
  if (Replaying)
     Add(new cOsdItem(tr(" Stop replaying"), osStopReplay));
  const char *s = NULL;
  while ((s = cRecordControls::GetInstantId(s)) != NULL) {
        char *buffer = NULL;
        asprintf(&buffer, "%s%s", STOP_RECORDING, s);
        Add(new cOsdItem(buffer, osStopRecord));
        delete buffer;
        }
  if (cVideoCutter::Active())
     Add(new cOsdItem(hk(tr(" Cancel editing")), osCancelEdit));
  const char *DVDbutton =
#ifdef DVDSUPPORT
                          cDVD::DiscOk() ? tr("Eject") : NULL;
#else
                          NULL;
#endif //DVDSUPPORT
  SetHelp(tr("Record"), cDvbApi::PrimaryDvbApi->CanToggleAudioTrack() ? tr("Language") : NULL, DVDbutton, cReplayControl::LastReplayed() ? tr("Resume") : NULL);
  Display();
  lastActivity = time(NULL);
  SetHasHotkeys();
}

const char *cMenuMain::hk(const char *s)
{
  static char buffer[32];
  if (digit < 9) {
     snprintf(buffer, sizeof(buffer), " %d %s", ++digit, s);
     return buffer;
     }
  else
     return s;
}

eOSState cMenuMain::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osSchedule:   return AddSubMenu(new cMenuSchedule);
    case osChannels:   return AddSubMenu(new cMenuChannels);
    case osTimers:     return AddSubMenu(new cMenuTimers);
    case osRecordings: return AddSubMenu(new cMenuRecordings);
#ifdef DVDSUPPORT
    case osDVD:        return AddSubMenu(new cMenuDVD);
#endif //DVDSUPPORT
    case osSetup:      return AddSubMenu(new cMenuSetup);
    case osCommands:   return AddSubMenu(new cMenuCommands);
    case osStopRecord: if (Interface->Confirm(tr("Stop recording?"))) {
                          cOsdItem *item = Get(Current());
                          if (item) {
                             cRecordControls::Stop(item->Text() + strlen(STOP_RECORDING));
                             return osEnd;
                             }
                          }
                       break;
    case osCancelEdit: if (Interface->Confirm(tr("Cancel editing?"))) {
                          cVideoCutter::Stop();
                          return osEnd;
                          }
                       break;
    default: switch (Key) {
               case kMenu:   state = osEnd;    break;
               case kRed:    if (!HasSubMenu())
                                state = osRecord;
                             break;
               case kGreen:  if (!HasSubMenu()) {
                                if (cDvbApi::PrimaryDvbApi->CanToggleAudioTrack()) {
                                   Interface->Clear();
                                   cDvbApi::PrimaryDvbApi->ToggleAudioTrack();
                                   state = osEnd;
                                   }
                                }
                             break;
#ifdef DVDSUPPORT
               case kYellow: if (!HasSubMenu()) {
                                if (cDVD::DiscOk()) {
                                   cDVD::Eject();
                                   state = osEnd;
                                   }
                                }
                             break;
#endif //DVDSUPPORT
               case kBlue:   if (!HasSubMenu())
                                state = osReplay;
                             break;
               default:      break;
               }
    }
  if (Key != kNone)
     lastActivity = time(NULL);
  else if (time(NULL) - lastActivity > MENUTIMEOUT)
     state = osEnd;
  return state;
}

// --- cDisplayChannel -------------------------------------------------------

#define DIRECTCHANNELTIMEOUT 1000 //ms
#define INFOTIMEOUT          5000 //ms

cDisplayChannel::cDisplayChannel(int Number, bool Switched, bool Group)
:cOsdBase(true)
{
  group = Group;
  withInfo = !group && (!Switched || Setup.ShowInfoOnChSwitch);
  lines = 0;
  oldNumber = number = 0;
  cChannel *channel = Group ? Channels.Get(Number) : Channels.GetByNumber(Number);
  Interface->Open(Setup.OSDwidth, Setup.ChannelInfoPos ? 5 : -5);
  if (channel) {
     DisplayChannel(channel);
     DisplayInfo();
     }
  lastTime = time_ms();
}

cDisplayChannel::cDisplayChannel(eKeys FirstKey)
:cOsdBase(true)
{
  oldNumber = cDvbApi::CurrentChannel();
  number = 0;
  lastTime = time_ms();
  Interface->Open(Setup.OSDwidth, Setup.ChannelInfoPos ? 5 : -5);
  ProcessKey(FirstKey);
}

cDisplayChannel::~cDisplayChannel()
{
  if (number < 0)
     Interface->DisplayChannelNumber(oldNumber);
  Interface->Close();
}

void cDisplayChannel::DisplayChannel(const cChannel *Channel)
{
  if (Channel && Channel->number > 0)
     Interface->DisplayChannelNumber(Channel->number);
  int BufSize = Width() + 1;
  char buffer[BufSize];
  if (Channel && Channel->number > 0)
     snprintf(buffer, BufSize, "%d%s  %s", Channel->number, number ? "-" : "", Channel->name);
  else
     snprintf(buffer, BufSize, "%s", Channel ? Channel->name : tr("*** Invalid Channel ***"));
  Interface->Fill(0, 0, Setup.OSDwidth, 1, clrBackground);
  Interface->Write(0, 0, buffer);
  time_t t = time(NULL);
  struct tm *now = localtime(&t);
  snprintf(buffer, BufSize, "%02d:%02d", now->tm_hour, now->tm_min);
  Interface->Write(-5, 0, buffer);
}

void cDisplayChannel::DisplayInfo(void)
{
  if (withInfo) {
     const cEventInfo *Present = NULL, *Following = NULL;
     cThreadLock ThreadLock;
     const cSchedules *Schedules = cDvbApi::PrimaryDvbApi->Schedules(&ThreadLock);
     if (Schedules) {
        const cSchedule *Schedule = Schedules->GetSchedule();
        if (Schedule) {
           const char *PresentTitle = NULL, *PresentSubtitle = NULL, *FollowingTitle = NULL, *FollowingSubtitle = NULL;
           int Lines = 0;
           if ((Present = Schedule->GetPresentEvent()) != NULL) {
              PresentTitle = Present->GetTitle();
              if (!isempty(PresentTitle))
                 Lines++;
              PresentSubtitle = Present->GetSubtitle();
              if (!isempty(PresentSubtitle))
                 Lines++;
              }
           if ((Following = Schedule->GetFollowingEvent()) != NULL) {
              FollowingTitle = Following->GetTitle();
              if (!isempty(FollowingTitle))
                 Lines++;
              FollowingSubtitle = Following->GetSubtitle();
              if (!isempty(FollowingSubtitle))
                 Lines++;
              }
           if (Lines > lines) {
              const int t = 6;
              int l = 1;
              Interface->Fill(0, 1, Setup.OSDwidth, Lines, clrBackground);
              if (!isempty(PresentTitle)) {
                 Interface->Write(0, l, Present->GetTimeString(), clrYellow, clrBackground);
                 Interface->Write(t, l, PresentTitle, clrCyan, clrBackground);
                 l++;
                 }
              if (!isempty(PresentSubtitle)) {
                 Interface->Write(t, l, PresentSubtitle, clrCyan, clrBackground);
                 l++;
                 }
              if (!isempty(FollowingTitle)) {
                 Interface->Write(0, l, Following->GetTimeString(), clrYellow, clrBackground);
                 Interface->Write(t, l, FollowingTitle, clrCyan, clrBackground);
                 l++;
                 }
              if (!isempty(FollowingSubtitle)) {
                 Interface->Write(t, l, FollowingSubtitle, clrCyan, clrBackground);
                 }
              Interface->Flush();
              lines = Lines;
              lastTime = time_ms();
              }
           }
        }
     }
}

eOSState cDisplayChannel::ProcessKey(eKeys Key)
{
  switch (Key) {
    case k0:
         if (number == 0) {
            // keep the "Toggle channels" function working
            Interface->PutKey(Key);
            return osEnd;
            }
    case k1 ... k9:
         if (number >= 0) {
            number = number * 10 + Key - k0;
            if (number > 0) {
               cChannel *channel = Channels.GetByNumber(number);
               DisplayChannel(channel);
               lastTime = time_ms();
               if (!channel) {
                  number = -1;
                  lastTime += 1000;
                  }
               }
            }
         break;
    case kNone:
         if (number && time_ms() - lastTime > DIRECTCHANNELTIMEOUT) {
            if (number > 0 && !Channels.SwitchTo(number))
               number = -1;
            return osEnd;
            }
         break;
    //TODO
    //XXX case kGreen:  return osEventNow;
    //XXX case kYellow: return osEventNext;
    case kOk:     if (group)
                     Channels.SwitchTo(Channels.Get(Channels.GetNextNormal(CurrentGroup))->number);
                  return osEnd;
    default:      Interface->PutKey(Key);
                  return osEnd;
    };
  if (time_ms() - lastTime < INFOTIMEOUT) {
     DisplayInfo();
     return osContinue;
     }
  return osEnd;
}

// --- cRecordControl --------------------------------------------------------

cRecordControl::cRecordControl(cDvbApi *DvbApi, cTimer *Timer)
{
  instantId = NULL;
  dvbApi = DvbApi;
  if (!dvbApi) dvbApi = cDvbApi::PrimaryDvbApi;//XXX
  timer = Timer;
  if (!timer) {
     timer = new cTimer(true);
     Timers.Add(timer);
     Timers.Save();
     asprintf(&instantId, cDvbApi::NumDvbApis > 1 ? "%s - %d" : "%s", Channels.GetChannelNameByNumber(timer->channel), dvbApi->CardIndex() + 1);
     }
  timer->SetRecording(true);
  if (Channels.SwitchTo(timer->channel, dvbApi)) {
     cRecording Recording(timer);
     if (dvbApi->StartRecord(Recording.FileName(), Channels.GetByNumber(timer->channel)->ca, timer->priority))
        Recording.WriteSummary();
     Interface->DisplayRecording(dvbApi->CardIndex(), true);
     }
  else
     cThread::EmergencyExit(true);
}

cRecordControl::~cRecordControl()
{
  Stop(true);
  delete instantId;
}

void cRecordControl::Stop(bool KeepInstant)
{
  if (timer) {
     dvbApi->StopRecord();
     timer->SetRecording(false);
     if ((IsInstant() && !KeepInstant) || (timer->IsSingleEvent() && !timer->Matches())) {
        // checking timer->Matches() to make sure we don't delete the timer
        // if the program was cancelled before the timer's stop time!
        isyslog(LOG_INFO, "deleting timer %d", timer->Index() + 1);
        Timers.Del(timer);
        Timers.Save();
        }
     timer = NULL;
     Interface->DisplayRecording(dvbApi->CardIndex(), false);
     }
}

bool cRecordControl::Process(void)
{
  if (!timer || !timer->Matches())
     return false;
  AssertFreeDiskSpace(timer->priority);
  return true;
}

// --- cRecordControls -------------------------------------------------------

cRecordControl *cRecordControls::RecordControls[MAXDVBAPI] = { NULL };

bool cRecordControls::Start(cTimer *Timer)
{
  int ch = Timer ? Timer->channel : cDvbApi::CurrentChannel();
  cChannel *channel = Channels.GetByNumber(ch);

  if (channel) {
     cDvbApi *dvbApi = cDvbApi::GetDvbApi(channel->ca, Timer ? Timer->priority : Setup.DefaultPriority);
     if (dvbApi) {
        Stop(dvbApi);
        for (int i = 0; i < MAXDVBAPI; i++) {
            if (!RecordControls[i]) {
               RecordControls[i] = new cRecordControl(dvbApi, Timer);
               return true;
               }
            }
        }
     else if (!Timer || Timer->priority >= Setup.PrimaryLimit)
        esyslog(LOG_ERR, "ERROR: no free DVB device to record channel %d!", ch);
     }
  else
     esyslog(LOG_ERR, "ERROR: channel %d not defined!", ch);
  return false;
}

void cRecordControls::Stop(const char *InstantId)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (RecordControls[i]) {
         const char *id = RecordControls[i]->InstantId();
         if (id && strcmp(id, InstantId) == 0)
            RecordControls[i]->Stop();
         }
      }
}

void cRecordControls::Stop(cDvbApi *DvbApi)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (RecordControls[i]) {
         if (RecordControls[i]->Uses(DvbApi)) {
            isyslog(LOG_INFO, "stopping recording on DVB device %d due to higher priority", DvbApi->CardIndex() + 1);
            RecordControls[i]->Stop(true);
            }
         }
      }
}

const char *cRecordControls::GetInstantId(const char *LastInstantId)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (RecordControls[i]) {
         if (!LastInstantId && RecordControls[i]->InstantId())
            return RecordControls[i]->InstantId();
         if (LastInstantId && LastInstantId == RecordControls[i]->InstantId())
            LastInstantId = NULL;
         }
      }
  return NULL;
}

void cRecordControls::Process(void)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (RecordControls[i]) {
         if (!RecordControls[i]->Process())
            DELETENULL(RecordControls[i]);
         }
      }
}

bool cRecordControls::Active(void)
{
  for (int i = 0; i < MAXDVBAPI; i++) {
      if (RecordControls[i])
         return true;
      }
  return false;
}

// --- cProgressBar ----------------------------------------------------------

class cProgressBar : public cBitmap {
protected:
  int total;
  int Pos(int p) { return p * width / total; }
  void Mark(int x, bool Start, bool Current);
public:
  cProgressBar(int Width, int Height, int Current, int Total, const cMarks &Marks);
  };

cProgressBar::cProgressBar(int Width, int Height, int Current, int Total, const cMarks &Marks)
:cBitmap(Width, Height, 2)
{
  total = Total;
  if (total > 0) {
     int p = Pos(Current);
     Fill(0, 0, p, Height - 1, clrGreen);
     Fill(p + 1, 0, Width - 1, Height - 1, clrWhite);
     bool Start = true;
     for (const cMark *m = Marks.First(); m; m = Marks.Next(m)) {
         int p1 = Pos(m->position);
         if (Start) {
            const cMark *m2 = Marks.Next(m);
            int p2 = Pos(m2 ? m2->position : total);
            int h = Height / 3;
            Fill(p1, h, p2, Height - h, clrRed);
            }
         Mark(p1, Start, m->position == Current);
         Start = !Start;
         }
     }
}

void cProgressBar::Mark(int x, bool Start, bool Current)
{
  Fill(x, 0, x, height - 1, clrBlack);
  const int d = height / (Current ? 3 : 9);
  for (int i = 0; i < d; i++) {
      int h = Start ? i : height - 1 - i;
      Fill(x - d + i, h, x + d - i, h, Current ? clrRed : clrBlack);
      }
}

// --- cReplayControl --------------------------------------------------------

char *cReplayControl::fileName = NULL;
char *cReplayControl::title = NULL;
#ifdef DVDSUPPORT
cDVD *cReplayControl::dvd = NULL;//XXX
int  cReplayControl::titleid = 0;//XXX
#endif //DVDSUPPORT

cReplayControl::cReplayControl(void)
{
  dvbApi = cDvbApi::PrimaryDvbApi;
  visible = shown = displayFrames = false;
  lastCurrent = lastTotal = -1;
  timeoutShow = 0;
  timeSearchActive = false;
  if (fileName) {
     marks.Load(fileName);
     dvbApi->StartReplay(fileName);
     }
#ifdef DVDSUPPORT
  else if (dvd)
     dvbApi->StartDVDplay(dvd, titleid);//XXX
#endif //DVDSUPPORT
}

cReplayControl::~cReplayControl()
{
  Hide();
  dvbApi->StopReplay();
}

void cReplayControl::SetRecording(const char *FileName, const char *Title)
{
  delete fileName;
  delete title;
  fileName = FileName ? strdup(FileName) : NULL;
  title = Title ? strdup(Title) : NULL;
}

#ifdef DVDSUPPORT
void cReplayControl::SetDVD(cDVD *DVD, int Title)//XXX
{
  SetRecording(NULL, NULL);
  dvd = DVD;
  titleid = Title;
}
#endif //DVDSUPPORT

const char *cReplayControl::LastReplayed(void)
{
  return fileName;
}

void cReplayControl::ClearLastReplayed(const char *FileName)
{
  if (fileName && FileName && strcmp(fileName, FileName) == 0) {
     delete fileName;
     fileName = NULL;
     }
}

void cReplayControl::Show(int Seconds)
{
  if (!visible) {
     shown = ShowProgress(true);
     if (shown && Seconds > 0)
        timeoutShow = time(NULL) + Seconds;
     }
}

void cReplayControl::Hide(void)
{
  if (visible) {
     Interface->Close();
     needsFastResponse = visible = false;
     }
}

bool cReplayControl::ShowProgress(bool Initial)
{
  int Current, Total;

  if (dvbApi->GetIndex(Current, Total) && Total > 0) {
     if (!visible) {
        Interface->Open(Setup.OSDwidth, -3);
        needsFastResponse = visible = true;
        }
     if (Initial) {
        Interface->Clear();
        if (title)
           Interface->Write(0, 0, title);
        lastCurrent = lastTotal = -1;
        }
     if (Total != lastTotal) {
        Interface->Write(-7, 2, IndexToHMSF(Total));
        if (!Initial)
           Interface->Flush();
        }
     if (Current != lastCurrent || Total != lastTotal) {
#ifdef DEBUG_OSD
        int p = Width() * Current / Total;
        Interface->Fill(0, 1, p, 1, clrGreen);
        Interface->Fill(p, 1, Width() - p, 1, clrWhite);
#else
        cProgressBar ProgressBar(Width() * dvbApi->CellWidth(), dvbApi->LineHeight(), Current, Total, marks);
        Interface->SetBitmap(0, dvbApi->LineHeight(), ProgressBar);
        if (!Initial)
           Interface->Flush();
#endif
        Interface->Write(0, 2, IndexToHMSF(Current, displayFrames));
        Interface->Flush();
        lastCurrent = Current;
        }
     lastTotal = Total;
     return true;
     }
  return false;
}

void cReplayControl::TimeSearchDisplay(void)
{
  char buf[64];
  int len;
  
  strcpy(buf, tr("Jump: "));
  len = strlen(buf);
  
  switch (timeSearchPos) {
    case 1:  sprintf(buf + len, "%01d-:--", timeSearchHH / 10); break;
    case 2:  sprintf(buf + len, "%02d:--", timeSearchHH); break;
    case 3:  sprintf(buf + len, "%02d:%01d-", timeSearchHH, timeSearchMM / 10); break;
    case 4:  sprintf(buf + len, "%02d:%02d", timeSearchHH, timeSearchMM); break;
    default: sprintf(buf + len, "--:--"); break;
    }

  Interface->Write(12, 2, buf);
}

void cReplayControl::TimeSearchProcess(eKeys Key)
{
  int Seconds = timeSearchHH * 3600 + timeSearchMM * 60;
  switch (Key) {
    case k0 ... k9:
         {
           int n = Key - k0;
           int s = (lastTotal / FRAMESPERSEC);
           int m = s / 60 % 60;
           int h = s / 3600;
           switch (timeSearchPos) {
             case 0: if (n * 10 <= h) {
                        timeSearchHH = n * 10;
                        timeSearchPos++;
                        }
                     break;
             case 1: if (timeSearchHH + n <= h) {
                        timeSearchHH += n;
                        timeSearchPos++;
                        }
                     break;
             case 2: if (n <= 5 && timeSearchHH * 60 + n * 10 <= h * 60 + m) {
                        timeSearchMM += n * 10;
                        timeSearchPos++;
                        }
                     break;
             case 3: if (timeSearchHH * 60 + timeSearchMM + n <= h * 60 + m) {
                        timeSearchMM += n;
                        timeSearchPos++;
                        }
                     break;
             }
           TimeSearchDisplay();
         }
         break;
    case kLeft:
    case kRight:
         dvbApi->SkipSeconds(Seconds * (Key == kRight ? 1 : -1));
         timeSearchActive = false;
         break;
    case kUp:
    case kDown:
         dvbApi->Goto(Seconds * FRAMESPERSEC, Key == kDown);
         timeSearchActive = false;
         break;
    default:
         timeSearchActive = false;
         break;
    }

  if (!timeSearchActive) {
     if (timeSearchHide)
        Hide();
     else
        Interface->Fill(12, 2, Width() - 22, 1, clrBackground);
     }
}

void cReplayControl::TimeSearch(void)
{
  timeSearchHH = timeSearchMM = timeSearchPos = 0;
  timeSearchHide = false;
  if (!visible) {
     Show();
     if (visible)
        timeSearchHide = true;
     else
        return;
     }
  TimeSearchDisplay();
  timeSearchActive = true;
}

void cReplayControl::MarkToggle(void)
{
  int Current, Total;
  if (dvbApi->GetIndex(Current, Total, true)) {
     cMark *m = marks.Get(Current);
     lastCurrent = -1; // triggers redisplay
     if (m)
        marks.Del(m);
     else {
        marks.Add(Current);
        Show(2);
        }
     marks.Save();
     }
}

void cReplayControl::MarkJump(bool Forward)
{
  if (marks.Count()) {
     int Current, Total;
     if (dvbApi->GetIndex(Current, Total)) {
        cMark *m = Forward ? marks.GetNext(Current) : marks.GetPrev(Current);
        if (m)
           dvbApi->Goto(m->position, true);
        }
     displayFrames = true;
     }
}

void cReplayControl::MarkMove(bool Forward)
{
  int Current, Total;
  if (dvbApi->GetIndex(Current, Total)) {
     cMark *m = marks.Get(Current);
     if (m) {
        displayFrames = true;
        int p = dvbApi->SkipFrames(Forward ? 1 : -1);
        cMark *m2;
        if (Forward) {
           if ((m2 = marks.Next(m)) != NULL && m2->position <= p)
              return;
           }
        else {
           if ((m2 = marks.Prev(m)) != NULL && m2->position >= p)
              return;
           }
        dvbApi->Goto(m->position = p, true);
        marks.Save();
        }
     }
}

void cReplayControl::EditCut(void)
{
  Hide();
  if (!cVideoCutter::Active()) {
     if (!cVideoCutter::Start(fileName))
        Interface->Error(tr("Can't start editing process!"));
     else
        Interface->Info(tr("Editing process started"));
     }
  else
     Interface->Error(tr("Editing process already active!"));
}

void cReplayControl::EditTest(void)
{
  int Current, Total;
  if (dvbApi->GetIndex(Current, Total)) {
     cMark *m = marks.Get(Current);
     if (!m)
        m = marks.GetNext(Current);
     if (m) {
        if ((m->Index() & 0x01) != 0)
           m = marks.Next(m);
        if (m) {
           dvbApi->Goto(m->position - dvbApi->SecondsToFrames(3));
           dvbApi->Play();
           }
        }
     }
}

eOSState cReplayControl::ProcessKey(eKeys Key)
{
  if (!dvbApi->Replaying())
     return osEnd;
  if (visible) {
     if (timeoutShow && time(NULL) > timeoutShow) {
        Hide();
        timeoutShow = 0;
        }
     else
        shown = ShowProgress(!shown) || shown;
     }
  bool DisplayedFrames = displayFrames;
  displayFrames = false;
  if (timeSearchActive && Key != kNone) {
     TimeSearchProcess(Key);
     return osContinue;
     }
  switch (Key) {
    // Positioning:
    case kUp:      dvbApi->Play(); break;
    case kDown:    dvbApi->Pause(); break;
    case kLeft|k_Release:
    case kLeft:    dvbApi->Backward(); break;
    case kRight|k_Release:
    case kRight:   dvbApi->Forward(); break;
    case kRed:     TimeSearch(); break;
    case kGreen|k_Repeat:
    case kGreen:   dvbApi->SkipSeconds(-60); break;
    case kYellow|k_Repeat:
    case kYellow:  dvbApi->SkipSeconds(60); break;
    case kBlue:    Hide();
                   dvbApi->StopReplay();
                   return osEnd;
    default: {
      switch (Key) {
        // Editing:
        //XXX should we do this only when the ProgressDisplay is on???
        case kMarkToggle:      MarkToggle(); break;
        case kMarkJumpBack:    MarkJump(false); break;
        case kMarkJumpForward: MarkJump(true); break;
        case kMarkMoveBack|k_Repeat:
        case kMarkMoveBack:    MarkMove(false); break;
        case kMarkMoveForward|k_Repeat:
        case kMarkMoveForward: MarkMove(true); break;
        case kEditCut:         EditCut(); break;
        case kEditTest:        EditTest(); break;
        default: {
          displayFrames = DisplayedFrames;
          switch (Key) {
            // Menu control:
            case kMenu:    Hide(); return osMenu; // allow direct switching to menu
            case kOk:      visible ? Hide() : Show(); break;
            case kBack:    return osRecordings;
            default:       return osUnknown;
            }
          }
        }
      }
    }
  if (DisplayedFrames && !displayFrames)
     Interface->Fill(0, 2, 11, 1, clrBackground);
  return osContinue;
}

