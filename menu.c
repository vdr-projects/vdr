/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'osm.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 1.1 2000/02/19 13:36:48 kls Exp $
 */

#include "menu.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "dvbapi.h"

const char *FileNameChars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789/-.# ";//TODO more?

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
  virtual eOSStatus ProcessKey(eKeys Key);
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

eOSStatus cMenuEditIntItem::ProcessKey(eKeys Key)
{
  eOSStatus status = cMenuEditItem::ProcessKey(Key);

  if (status == osUnknown) {
     int newValue;
     if (k0 <= Key && Key <= k9) {
        if (fresh) {
           *value = 0;
           fresh = false;
           }
        newValue = *value  * 10 + (Key - k0);
        }
     else if (Key == kLeft) { // TODO might want to increase the delta if repeated quickly?
        newValue = *value - 1;
        fresh = true;
        }
     else if (Key == kRight) {
        newValue = *value + 1;
        fresh = true;
        }
     else
        return status;
     if ((!fresh || min <= newValue) && newValue <= max) {
        *value = newValue;
        Set();
        }
     status = osContinue;
     }
  return status;
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
:cMenuEditIntItem(Name, Value, 1, Channels.Count())
{
  Set();
}

void cMenuEditChanItem::Set(void)
{
  char buf[255];
  cChannel *channel = Channels.Get(*value - 1);
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
  virtual eOSStatus ProcessKey(eKeys Key);
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

eOSStatus cMenuEditDayItem::ProcessKey(eKeys Key)
{
  switch (Key) {
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
  virtual eOSStatus ProcessKey(eKeys Key);
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

eOSStatus cMenuEditTimeItem::ProcessKey(eKeys Key)
{
  eOSStatus status = cMenuEditItem::ProcessKey(Key);

  if (status == osUnknown) {
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
     else if (Key == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (--mm < 0) {
           mm = 59;
           if (--hh < 0)
              hh = 23;
           }
        fresh = true;
        }
     else if (Key == kRight) {
        if (++mm > 59) {
           mm = 0;
           if (++hh > 23)
              hh = 0;
           }
        fresh = true;
        }
     else
        return status;
     *value = hh * 100 + mm;
     Set();
     status = osContinue;
     }
  return status;
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
  virtual eOSStatus ProcessKey(eKeys Key);
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

eOSStatus cMenuEditChrItem::ProcessKey(eKeys Key)
{
  eOSStatus status = cMenuEditItem::ProcessKey(Key);

  if (status == osUnknown) {
     if (Key == kLeft) {
        if (current > allowed)
           current--;
        }
     else if (Key == kRight) {
        if (*(current + 1))
           current++;
        }
     else
        return status;
     *value = *current;
     Set();
     status = osContinue;
     }
  return status;
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
  virtual eOSStatus ProcessKey(eKeys Key);
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
     char *s = value[pos] != ' ' ? value + pos + 1 : "";
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

eOSStatus cMenuEditStrItem::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kLeft:  if (pos > 0) {
                    if (value[pos] == ' ')
                       value[pos] = 0;
                    pos--;
                    }
                 break;
    case kRight: if (pos < length && value[pos] != ' ') {
                    if (++pos >= int(strlen(value))) {
                       value[pos] = ' ';
                       value[pos + 1] = 0;
                       }
                    }
                 break;
    case kUp:
    case kDown:  if (pos >= 0)
                    value[pos] = Inc(value[pos], Key == kUp);
                 else
                    return cMenuEditItem::ProcessKey(Key);
                 break;
    case kOk:    if (pos >= 0) {
                    if (value[pos] == ' ')
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
  virtual eOSStatus ProcessKey(eKeys Key);
  };

cMenuEditChannel::cMenuEditChannel(int Index)
:cOsdMenu("Edit channel", 14)
{
  channel = Channels.Get(Index);
  if (channel) {
     data = *channel;
     Add(new cMenuEditStrItem("Name",          data.name, sizeof(data.name), FileNameChars));
     Add(new cMenuEditIntItem("Frequency",    &data.frequency, 10000, 13000)); //TODO exact limits???
     Add(new cMenuEditChrItem("Polarization", &data.polarization, "hv"));
     Add(new cMenuEditIntItem("Diseqc",       &data.diseqc, 0, 10)); //TODO exact limits???
     Add(new cMenuEditIntItem("Srate",        &data.srate, 22000, 27500)); //TODO exact limits - toggle???
     Add(new cMenuEditIntItem("Vpid",         &data.vpid, 0, 10000)); //TODO exact limits???
     Add(new cMenuEditIntItem("Apid",         &data.apid, 0, 10000)); //TODO exact limits???
     }
}

eOSStatus cMenuEditChannel::ProcessKey(eKeys Key)
{
  eOSStatus status = cOsdMenu::ProcessKey(Key);

  if (status == osUnknown) {
     if (Key == kOk) {
        if (channel)
           *channel = data;
        Channels.Save();
        status = osBack;
        }
     }
  return status;
}

// --- cMenuChannelItem ------------------------------------------------------

class cMenuChannelItem : public cOsdItem {
private:
  int index;
  cChannel *channel;
public:
  cMenuChannelItem(int Index, cChannel *Channel);
  virtual void Set(void);
  };

cMenuChannelItem::cMenuChannelItem(int Index, cChannel *Channel)
{
  index = Index;
  channel = Channel;
  Set();
}

void cMenuChannelItem::Set(void)
{
  char *buffer = NULL;
  asprintf(&buffer, "%d\t%s", index + 1, channel->name); // user visible channel numbers start with '1'
  SetText(buffer, false);
}

// --- cMenuChannels ---------------------------------------------------------

class cMenuChannels : public cOsdMenu {
public:
  cMenuChannels(void);
  virtual eOSStatus ProcessKey(eKeys Key);
  };

cMenuChannels::cMenuChannels(void)
:cOsdMenu("Channels", 4)
{
  //TODO
  int i = 0;
  cChannel *channel;

  while ((channel = Channels.Get(i)) != NULL) {
        Add(new cMenuChannelItem(i, channel), i == CurrentChannel);
        i++;
        }
}

eOSStatus cMenuChannels::ProcessKey(eKeys Key)
{
  eOSStatus status = cOsdMenu::ProcessKey(Key);

  if (status == osUnknown) {
     switch (Key) {
       //TODO need to block this if we are already editing a channel!
       case kRight: return AddSubMenu(new cMenuEditChannel(Current()));
       case kOk:    {
                      cChannel *ch = Channels.Get(Current());
                      if (ch)
                         ch->Switch();
                      return osEnd;
                    }
       default: break;
       }
     }
  return status;
}

// --- cMenuEditTimer --------------------------------------------------------

class cMenuEditTimer : public cOsdMenu {
private:
  cTimer *timer;
  cTimer data;
public:
  cMenuEditTimer(int Index);
  virtual eOSStatus ProcessKey(eKeys Key);
  };

cMenuEditTimer::cMenuEditTimer(int Index)
:cOsdMenu("Edit timer", 10)
{
  timer = Timers.Get(Index);
  if (timer) {
     data = *timer;
     Add(new cMenuEditBoolItem("Active",       &data.active));
     Add(new cMenuEditChanItem("Channel",      &data.channel)); 
     Add(new cMenuEditDayItem( "Day",          &data.day)); 
     Add(new cMenuEditTimeItem("Start",        &data.start)); 
     Add(new cMenuEditTimeItem("Stop",         &data.stop)); 
//TODO VPS???
     Add(new cMenuEditChrItem( "Quality",      &data.quality, DvbQuality));
     Add(new cMenuEditIntItem( "Priority",     &data.priority, 0, 99));
     Add(new cMenuEditIntItem( "Lifetime",     &data.lifetime, 0, 99));
     Add(new cMenuEditStrItem( "File",          data.file, sizeof(data.file), FileNameChars));
     }
}

eOSStatus cMenuEditTimer::ProcessKey(eKeys Key)
{
  eOSStatus status = cOsdMenu::ProcessKey(Key);

  if (status == osUnknown) {
     if (Key == kOk) {
        if (timer && memcmp(timer, &data, sizeof(data)) != 0) {
           *timer = data;
           Timers.Save();
           isyslog(LOG_INFO, "timer %d modified (%s)", timer->Index() + 1, timer->active ? "active" : "inactive");
           }
        status = osBack;
        }
     }
  return status;
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
  asprintf(&buffer, "%d\t%c\t%d\t%s\t%02d:%02d\t%02d:%02d", index + 1,
                    timer->active ? '>' : ' ', 
                    timer->channel, 
                    timer->PrintDay(timer->day), 
                    timer->start / 100,
                    timer->start % 100,
                    timer->stop / 100,
                    timer->stop % 100); // user visible timer numbers start with '1'
  SetText(buffer, false);
}

// --- cMenuTimer ------------------------------------------------------------

class cMenuTimer : public cOsdMenu {
public:
  cMenuTimer(void);
  virtual eOSStatus ProcessKey(eKeys Key);
  };

cMenuTimer::cMenuTimer(void)
:cOsdMenu("Timer", 3, 2, 4, 10, 6)
{
  int i = 0;
  cTimer *timer;

  while ((timer = Timers.Get(i)) != NULL) {
        Add(new cMenuTimerItem(i, timer));
        i++;
        }
}

eOSStatus cMenuTimer::ProcessKey(eKeys Key)
{
  eOSStatus status = cOsdMenu::ProcessKey(Key);

  if (status == osUnknown) {
     switch (Key) {
       //TODO need to block this if we are already editing a channel!
       case kOk: return AddSubMenu(new cMenuEditTimer(Current()));
       //TODO new timer
       //TODO delete timer
       case kLeft:
       case kRight: 
            {
              cTimer *timer = Timers.Get(Current());
              if (timer) {
                 timer->active = (Key == kRight);
                 isyslog(LOG_INFO, "timer %d %sactivated", timer->Index() + 1, timer->active ? "" : "de");
                 RefreshCurrent();
                 DisplayCurrent(true);
                 Timers.Save();
                 }
            }
       default: break;
       }
     }
  return status;
}

// --- cMenuMain -------------------------------------------------------------

cMenuMain::cMenuMain(void)
:cOsdMenu("Main")
{
  //TODO
  Add(new cOsdItem("Channels",   osChannels));
  Add(new cOsdItem("Timer",      osTimer));
  Add(new cOsdItem("Recordings", osRecordings));
}

eOSStatus cMenuMain::ProcessKey(eKeys Key)
{
  eOSStatus status = cOsdMenu::ProcessKey(Key);

  switch (status) {
    case osChannels: return AddSubMenu(new cMenuChannels);
    case osTimer:    return AddSubMenu(new cMenuTimer);
    //TODO Replay
    default: break;
    }
  return status;
}

