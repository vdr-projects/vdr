/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 1.36 2000/10/08 16:11:22 kls Exp $
 */

#include "menu.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

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
  virtual void Set(void);
public:
  cMenuEditBoolItem(const char *Name, int *Value);
  };

cMenuEditBoolItem::cMenuEditBoolItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 0, 1)
{
  Set();
}

void cMenuEditBoolItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%s", *value ? "yes" : "no");
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
  snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
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
:cOsdMenu("Edit Channel", 14)
{
  channel = Channels.Get(Index);
  if (channel) {
     data = *channel;
     Add(new cMenuEditStrItem( "Name",          data.name, sizeof(data.name), FileNameChars));
     Add(new cMenuEditIntItem( "Frequency",    &data.frequency, 10000, 13000)); //TODO exact limits???
     Add(new cMenuEditChrItem( "Polarization", &data.polarization, "hv"));
     Add(new cMenuEditIntItem( "Diseqc",       &data.diseqc, 0, 10)); //TODO exact limits???
     Add(new cMenuEditIntItem( "Srate",        &data.srate, 22000, 27500)); //TODO exact limits - toggle???
     Add(new cMenuEditIntItem( "Vpid",         &data.vpid, 0, 10000)); //TODO exact limits???
     Add(new cMenuEditIntItem( "Apid",         &data.apid, 0, 10000)); //TODO exact limits???
     Add(new cMenuEditIntItem( "CA",           &data.ca, 0, cDvbApi::NumDvbApis));
     Add(new cMenuEditIntItem( "Pnr",          &data.pnr, 0));
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
     SetColor(clrWhite, clrBlue);
  Set();
}

void cMenuChannelItem::Set(void)
{
  char *buffer = NULL;
  if (!channel->groupSep)
     asprintf(&buffer, "%d\t%s", channel->number, channel->name );
  else
     asprintf(&buffer, "\t%s", channel->name); 
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
:cOsdMenu("Channels", 4)
{
  //TODO
  int i = 0;
  cChannel *channel;
  int curr = ((channel = Channels.GetByNumber(CurrentChannel)) != NULL) ? channel->Index() : -1;

  while ((channel = Channels.Get(i)) != NULL) {
        Add(new cMenuChannelItem(i, channel), i == curr);
        i++;
        }
  SetHelp("Edit", "New", "Delete", "Mark");
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
            Interface->Error("Channel is being used by a timer!");
            return osContinue;
            }
         }
     if (Interface->Confirm("Delete Channel?")) {
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

// --- cMenuSummary --------------------------------------------------------

class cMenuSummary : public cOsdMenu {
public:
  cMenuSummary(const char *Text);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSummary::cMenuSummary(const char *Text)
:cOsdMenu("Summary")
{
  while (*Text) {
        char line[MenuColumns + 1];
        char *p = line;
        const char *b = NULL;
        *p++ = ' ';
        while (*Text && p - line < MenuColumns - 2) {
              if (isspace(*Text))
                 b = Text; // remember the blank
              if (*Text == '\n')
                 break;
              *p++ = *Text++;
              }
        if (*Text) {
           if (b && Text - b > 0) {
              p -= Text - b;
              Text = b + 1;
              }
           else
              Text++;
           }
        *p = 0;
        Add(new cOsdItem(line, osBack));
        }
}

eOSState cMenuSummary::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown)
     state = osContinue;
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
:cOsdMenu("Edit Timer", 10)
{
  timer = Timers.Get(Index);
  if (timer) {
     data = *timer;
     if (New)
        data.active = 1;
     Add(new cMenuEditBoolItem("Active",       &data.active));
     Add(new cMenuEditChanItem("Channel",      &data.channel)); 
     Add(new cMenuEditDayItem( "Day",          &data.day)); 
     Add(new cMenuEditTimeItem("Start",        &data.start)); 
     Add(new cMenuEditTimeItem("Stop",         &data.stop)); 
//TODO VPS???
     Add(new cMenuEditIntItem( "Priority",     &data.priority, 0, 99));
     Add(new cMenuEditIntItem( "Lifetime",     &data.lifetime, 0, 99));
     Add(new cMenuEditStrItem( "File",          data.file, sizeof(data.file), FileNameChars));
     }
}

eOSState cMenuEditTimer::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     if (Key == kOk) {
        if (!*data.file)
           strcpy(data.file, Channels.GetChannelNameByNumber(data.channel));
        if (timer && memcmp(timer, &data, sizeof(data)) != 0) {
           *timer = data;
           Timers.Save();
           isyslog(LOG_INFO, "timer %d modified (%s)", timer->Index() + 1, timer->active ? "active" : "inactive");
           }
        state = osBack;
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
:cOsdMenu("Timer", 2, 4, 10, 6, 6)
{
  int i = 0;
  cTimer *timer;

  while ((timer = Timers.Get(i)) != NULL) {
        Add(new cMenuTimerItem(i, timer));
        i++;
        }
  SetHelp("Edit", "New", "Delete", "Mark");
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
        if (Interface->Confirm("Delete Timer?")) {
           Timers.Del(Timers.Get(Index));
           cOsdMenu::Del(Index);
           Timers.Save();
           Display();
           isyslog(LOG_INFO, "timer %d deleted", Index + 1);
           }
        }
     else
        Interface->Error("Timer is recording!");
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
     return AddSubMenu(new cMenuSummary(ti->summary));
  return Edit(); // convenience for people not using the Summary feature ;-)
}

eOSState cMenuTimers::ProcessKey(eKeys Key)
{
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
:cOsdMenu("Recordings", 6, 6)
{
  if (Recordings.Load()) {
     const char *lastReplayed = cReplayControl::LastReplayed();
     cRecording *recording = Recordings.First();
     while (recording) {
           Add(new cMenuRecordingItem(recording), lastReplayed && strcmp(lastReplayed, recording->FileName()) == 0);
           recording = Recordings.Next(recording);
           }
     }
  SetHelp("Play", NULL, "Delete", "Summary");
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

eOSState cMenuRecordings::Del(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri) {
//XXX what if this recording's file is currently in use???
//XXX     if (!ti->recording) {
        if (Interface->Confirm("Delete Recording?")) {
           if (ri->recording->Delete()) {
              cReplayControl::ClearLastReplayed(ri->recording->FileName());
              cOsdMenu::Del(Current());
              Display();
              }
           else
              Interface->Error("Error while deleting recording!");
           }
//XXX        }
//XXX     else
//XXX        Interface->Error("Timer is recording!");
     }
  return osContinue;
}

eOSState cMenuRecordings::Summary(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && ri->recording->Summary() && *ri->recording->Summary())
     return AddSubMenu(new cMenuSummary(ri->recording->Summary()));
  return osContinue;
}

eOSState cMenuRecordings::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:
       case kRed:    return Play();
       case kYellow: return Del();
       case kBlue:   return Summary();
       case kMenu:   return osEnd;
       default: break;
       }
     }
  return state;
}

// --- cMenuSetup ------------------------------------------------------------

class cMenuSetup : public cOsdMenu {
private:
  cSetup data;
public:
  cMenuSetup(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetup::cMenuSetup(void)
:cOsdMenu("Setup", 20)
{
  data = Setup;
  Add(new cMenuEditIntItem( "PrimaryDVB",         &data.PrimaryDVB, 1, cDvbApi::NumDvbApis));
  Add(new cMenuEditBoolItem("ShowInfoOnChSwitch", &data.ShowInfoOnChSwitch));
  Add(new cMenuEditBoolItem("MenuScrollPage",     &data.MenuScrollPage));
  Add(new cMenuEditBoolItem("MarkInstantRecord",  &data.MarkInstantRecord));
  Add(new cMenuEditIntItem( "LnbFrequLo",         &data.LnbFrequLo));
  Add(new cMenuEditIntItem( "LnbFrequHi",         &data.LnbFrequHi));
}

eOSState cMenuSetup::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk: state = (Setup.PrimaryDVB != data.PrimaryDVB) ? osSwitchDvb : osBack;
                 Setup = data;
                 Setup.Save();
                 break;
       default: break;
       }
     }
  return state;
}

// --- cMenuMain -------------------------------------------------------------

#define STOP_RECORDING "Stop recording "

cMenuMain::cMenuMain(bool Replaying)
:cOsdMenu("Main")
{
  Add(new cOsdItem("Channels",   osChannels));
  Add(new cOsdItem("Timer",      osTimer));
  Add(new cOsdItem("Recordings", osRecordings));
  Add(new cOsdItem("Setup",      osSetup));
  if (Replaying)
     Add(new cOsdItem("Stop replaying", osStopReplay));
  const char *s = NULL;
  while ((s = cRecordControls::GetInstantId(s)) != NULL) {
        char *buffer = NULL;
        asprintf(&buffer, "%s%s", STOP_RECORDING, s);
        Add(new cOsdItem(buffer, osStopRecord));
        delete buffer;
        }
  SetHelp("Record", NULL, NULL, cReplayControl::LastReplayed() ? "Resume" : NULL);
  Display();
  lastActivity = time(NULL);
}

eOSState cMenuMain::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osChannels:   return AddSubMenu(new cMenuChannels);
    case osTimer:      return AddSubMenu(new cMenuTimers);
    case osRecordings: return AddSubMenu(new cMenuRecordings);
    case osSetup:      return AddSubMenu(new cMenuSetup);
    case osStopRecord: if (Interface->Confirm("Stop Recording?")) {
                          cOsdItem *item = Get(Current());
                          if (item) {
                             cRecordControls::Stop(item->Text() + strlen(STOP_RECORDING));
                             return osEnd;
                             }
                          }
    default: switch (Key) {
               case kMenu: state = osEnd;    break;
               case kRed:  if (!HasSubMenu())
                              state = osRecord;
                           break;
               case kBlue: if (!HasSubMenu())
                              state = osReplay;
                           break;
               default:    break;
               }
    }
  if (Key != kNone)
     lastActivity = time(NULL);
  else if (time(NULL) - lastActivity > MENUTIMEOUT)
     state = osEnd;
  return state;
}

// --- cDirectChannelSelect --------------------------------------------------

#define DIRECTCHANNELTIMEOUT 500 //ms

cDirectChannelSelect::cDirectChannelSelect(eKeys FirstKey)
:cOsdBase(true)
{
  oldNumber = CurrentChannel;
  number = 0;
  lastTime = time_ms();
  Interface->Open(MenuColumns, 1);
  ProcessKey(FirstKey);
}

cDirectChannelSelect::~cDirectChannelSelect()
{
  if (number < 0)
     Interface->DisplayChannel(oldNumber);
  Interface->Close();
}

eOSState cDirectChannelSelect::ProcessKey(eKeys Key)
{
  switch (Key) {
    case k0 ... k9:
         if (number >= 0) {
            number = number * 10 + Key - k0;
            cChannel *channel = Channels.GetByNumber(number);
            const char *Name = channel ? channel->name : "*** Invalid Channel ***";
            int BufSize = MenuColumns + 1;
            char buffer[BufSize];
            snprintf(buffer, BufSize, "%d  %s", number, Name);
            Interface->DisplayChannel(number);
            Interface->Clear();
            Interface->Write(0, 0, buffer);
            lastTime = time_ms();
            if (!channel) {
               number = -1;
               lastTime += 1000;
               }
            }
         break;
    case kNone:
         if (time_ms() - lastTime > DIRECTCHANNELTIMEOUT) {
            if (number > 0 && !Channels.SwitchTo(number))
               number = -1;
            }
         else
            break;
    default: return osEnd;
    };
  return osContinue;
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
     asprintf(&instantId, cDvbApi::NumDvbApis > 1 ? "%s on %d" : "%s", Channels.GetChannelNameByNumber(timer->channel), dvbApi->Index() + 1);
     }
  timer->SetRecording(true);
  Channels.SwitchTo(timer->channel, dvbApi);
  cRecording Recording(timer);
  if (dvbApi->StartRecord(Recording.FileName()))
     Recording.WriteSummary();
  Interface->DisplayRecording(dvbApi->Index(), true);
}

cRecordControl::~cRecordControl()
{
  Stop(true);
  delete instantId;
  Interface->DisplayRecording(dvbApi->Index(), false);
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
     }
}

bool cRecordControl::Process(void)
{
  if (!timer || !timer->Matches())
     return false;
  AssertFreeDiskSpace();
  return true;
}

// --- cRecordControls -------------------------------------------------------

cRecordControl *cRecordControls::RecordControls[MAXDVBAPI] = { NULL };

bool cRecordControls::Start(cTimer *Timer)
{
  int ch = Timer ? Timer->channel : CurrentChannel;
  cChannel *channel = Channels.GetByNumber(ch);

  if (channel) {
     cDvbApi *dvbApi = cDvbApi::GetDvbApi(channel->ca);
     if (dvbApi) {
        for (int i = 0; i < MAXDVBAPI; i++) {
            if (!RecordControls[i]) {
               RecordControls[i] = new cRecordControl(dvbApi, Timer);
               return true;
               }
            }
        }
     else
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

// --- cReplayControl --------------------------------------------------------

char *cReplayControl::fileName = NULL;
char *cReplayControl::title = NULL;

cReplayControl::cReplayControl(void)
{
  dvbApi = cDvbApi::PrimaryDvbApi;//XXX
  visible = shown = false;
  if (fileName)
     dvbApi->StartReplay(fileName, title);
}

cReplayControl::~cReplayControl()
{
  Hide();
  dvbApi->Stop();
}

void cReplayControl::SetRecording(const char *FileName, const char *Title)
{
  delete fileName;
  delete title;
  fileName = FileName ? strdup(FileName) : NULL;
  title = Title ? strdup(Title) : NULL;
}

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

void cReplayControl::Show(void)
{
  if (!visible) {
     Interface->Open(MenuColumns, -3);
     needsFastResponse = visible = true;
     shown = dvbApi->ShowProgress(true);
     }
}

void cReplayControl::Hide(void)
{
  if (visible) {
     Interface->Close();
     needsFastResponse = visible = false;
     }
}

eOSState cReplayControl::ProcessKey(eKeys Key)
{
  if (!dvbApi->Replaying())
     return osEnd;
  if (visible)
     shown = dvbApi->ShowProgress(!shown) || shown;
  switch (Key) {
    case kUp:      dvbApi->Play(); break;
    case kDown:    dvbApi->Pause(); break;
    case kBlue:    Hide();
                   dvbApi->Stop();
                   return osEnd;
    case kLeft:    dvbApi->Backward(); break;
    case kRight:   dvbApi->Forward(); break;
    case kLeft|k_Release:
    case kRight|k_Release:
                   dvbApi->Play(); break;
    case kGreen|k_Repeat:
    case kGreen:   dvbApi->Skip(-60); break;
    case kYellow|k_Repeat:
    case kYellow:  dvbApi->Skip(60); break;
    case kMenu:    Hide(); return osMenu; // allow direct switching to menu
    case kOk:      visible ? Hide() : Show(); break;
    case kBack:    return osRecordings;
    default:       return osUnknown;
    }
  return osContinue;
}

