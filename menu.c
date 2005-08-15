/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 1.355 2005/08/14 15:14:29 kls Exp $
 */

#include "menu.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "channels.h"
#include "config.h"
#include "cutter.h"
#include "eitscan.h"
#include "i18n.h"
#include "interface.h"
#include "menuitems.h"
#include "plugin.h"
#include "recording.h"
#include "remote.h"
#include "sources.h"
#include "status.h"
#include "themes.h"
#include "timers.h"
#include "transfer.h"
#include "videodir.h"

#define MENUTIMEOUT     120 // seconds
#define MAXWAIT4EPGINFO   3 // seconds
#define MODETIMEOUT       3 // seconds

#define MAXRECORDCONTROLS (MAXDEVICES * MAXRECEIVERS)
#define MAXINSTANTRECTIME (24 * 60 - 1) // 23:59 hours

#define CHNUMWIDTH  (numdigits(Channels.MaxNumber()) + 1)

// --- cMenuEditCaItem -------------------------------------------------------

class cMenuEditCaItem : public cMenuEditIntItem {
private:
  const cCaDefinition *ca;
  bool allowCardNr;
protected:
  virtual void Set(void);
public:
  cMenuEditCaItem(const char *Name, int *Value, bool AllowCardNr = false);
  eOSState ProcessKey(eKeys Key);
  };

cMenuEditCaItem::cMenuEditCaItem(const char *Name, int *Value, bool AllowCardNr)
:cMenuEditIntItem(Name, Value, 0)
{
  ca = CaDefinitions.Get(*Value);
  allowCardNr = AllowCardNr;
  Set();
}

void cMenuEditCaItem::Set(void)
{
  if (ca)
     SetValue(ca->Description());
  else
     cMenuEditIntItem::Set();
}

eOSState cMenuEditCaItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (ca && ca->Prev()) {
           ca = (cCaDefinition *)ca->Prev();
           *value = ca->Number();
           }
        }
     else if (NORMALKEY(Key) == kRight) {
        if (ca && ca->Next() && (allowCardNr || ((cCaDefinition *)ca->Next())->Number() > MAXDEVICES)) {
           ca = (cCaDefinition *)ca->Next();
           *value = ca->Number();
           }
        }
     else
        return cMenuEditIntItem::ProcessKey(Key);
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditSrcItem ------------------------------------------------------

class cMenuEditSrcItem : public cMenuEditIntItem {
private:
  const cSource *source;
protected:
  virtual void Set(void);
public:
  cMenuEditSrcItem(const char *Name, int *Value);
  eOSState ProcessKey(eKeys Key);
  };

cMenuEditSrcItem::cMenuEditSrcItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 0)
{
  source = Sources.Get(*Value);
  Set();
}

void cMenuEditSrcItem::Set(void)
{
  if (source) {
     char *buffer = NULL;
     asprintf(&buffer, "%s - %s", *cSource::ToString(source->Code()), source->Description());
     SetValue(buffer);
     free(buffer);
     }
  else
     cMenuEditIntItem::Set();
}

eOSState cMenuEditSrcItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (source && source->Prev()) {
           source = (cSource *)source->Prev();
           *value = source->Code();
           }
        }
     else if (NORMALKEY(Key) == kRight) {
        if (source) {
           if (source->Next())
              source = (cSource *)source->Next();
           }
        else
           source = Sources.First();
        if (source)
           *value = source->Code();
        }
     else
        return state; // we don't call cMenuEditIntItem::ProcessKey(Key) here since we don't accept numerical input
     Set();
     state = osContinue;
     }
  return state;
}

// --- cMenuEditMapItem ------------------------------------------------------

class cMenuEditMapItem : public cMenuEditItem {
protected:
  int *value;
  const tChannelParameterMap *map;
  const char *zeroString;
  virtual void Set(void);
public:
  cMenuEditMapItem(const char *Name, int *Value, const tChannelParameterMap *Map, const char *ZeroString = NULL);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditMapItem::cMenuEditMapItem(const char *Name, int *Value, const tChannelParameterMap *Map, const char *ZeroString)
:cMenuEditItem(Name)
{
  value = Value;
  map = Map;
  zeroString = ZeroString;
  Set();
}

void cMenuEditMapItem::Set(void)
{
  int n = MapToUser(*value, map);
  if (n == 999)
     SetValue(tr("auto"));
  else if (n == 0 && zeroString)
     SetValue(zeroString);
  else if (n >= 0) {
     char buf[16];
     snprintf(buf, sizeof(buf), "%d", n);
     SetValue(buf);
     }
  else
     SetValue("???");
}

eOSState cMenuEditMapItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue = *value;
     int n = DriverIndex(*value, map);
     if (NORMALKEY(Key) == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (n-- > 0)
           newValue = map[n].driverValue;
        }
     else if (NORMALKEY(Key) == kRight) {
        if (map[++n].userValue >= 0)
           newValue = map[n].driverValue;
        }
     else
        return state;
     if (newValue != *value) {
        *value = newValue;
        Set();
        }
     state = osContinue;
     }
  return state;
}

// --- cMenuEditChannel ------------------------------------------------------

class cMenuEditChannel : public cOsdMenu {
private:
  cChannel *channel;
  cChannel data;
  char name[256];
  void Setup(void);
public:
  cMenuEditChannel(cChannel *Channel, bool New = false);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditChannel::cMenuEditChannel(cChannel *Channel, bool New)
:cOsdMenu(tr("Edit channel"), 14)
{
  channel = Channel;
  if (channel) {
     data = *channel;
     if (New) {
        channel = NULL;
        data.nid = 0;
        data.tid = 0;
        data.rid = 0;
        }
     Setup();
     }
}

void cMenuEditChannel::Setup(void)
{
  int current = Current();
  char type = **cSource::ToString(data.source);
#define ST(s) if (strchr(s, type))

  Clear();

  // Parameters for all types of sources:
  strn0cpy(name, data.name, sizeof(name));
  Add(new cMenuEditStrItem( tr("Name"),          name, sizeof(name), tr(FileNameChars)));
  Add(new cMenuEditSrcItem( tr("Source"),       &data.source));
  Add(new cMenuEditIntItem( tr("Frequency"),    &data.frequency));
  Add(new cMenuEditIntItem( tr("Vpid"),         &data.vpid,  0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Ppid"),         &data.ppid,  0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Apid1"),        &data.apids[0], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Apid2"),        &data.apids[1], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Dpid1"),        &data.dpids[0], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Dpid2"),        &data.dpids[1], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Tpid"),         &data.tpid,  0, 0x1FFF));
  Add(new cMenuEditCaItem(  tr("CA"),           &data.caids[0], true));//XXX
  Add(new cMenuEditIntItem( tr("Sid"),          &data.sid, 1, 0xFFFF));
  /* XXX not yet used
  Add(new cMenuEditIntItem( tr("Nid"),          &data.nid, 0));
  Add(new cMenuEditIntItem( tr("Tid"),          &data.tid, 0));
  Add(new cMenuEditIntItem( tr("Rid"),          &data.rid, 0));
  XXX*/
  // Parameters for specific types of sources:
  ST(" S ")  Add(new cMenuEditChrItem( tr("Polarization"), &data.polarization, "hvlr"));
  ST("CS ")  Add(new cMenuEditIntItem( tr("Srate"),        &data.srate));
  ST("CST")  Add(new cMenuEditMapItem( tr("Inversion"),    &data.inversion,    InversionValues, tr("off")));
  ST("CST")  Add(new cMenuEditMapItem( tr("CoderateH"),    &data.coderateH,    CoderateValues, tr("none")));
  ST("  T")  Add(new cMenuEditMapItem( tr("CoderateL"),    &data.coderateL,    CoderateValues, tr("none")));
  ST("C T")  Add(new cMenuEditMapItem( tr("Modulation"),   &data.modulation,   ModulationValues, "QPSK"));
  ST("  T")  Add(new cMenuEditMapItem( tr("Bandwidth"),    &data.bandwidth,    BandwidthValues));
  ST("  T")  Add(new cMenuEditMapItem( tr("Transmission"), &data.transmission, TransmissionValues));
  ST("  T")  Add(new cMenuEditMapItem( tr("Guard"),        &data.guard,        GuardValues));
  ST("  T")  Add(new cMenuEditMapItem( tr("Hierarchy"),    &data.hierarchy,    HierarchyValues, tr("none")));

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuEditChannel::ProcessKey(eKeys Key)
{
  int oldSource = data.source;
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     if (Key == kOk) {
        if (Channels.HasUniqueChannelID(&data, channel)) {
           data.name = strcpyrealloc(data.name, name);
           if (channel) {
              *channel = data;
              isyslog("edited channel %d %s", channel->Number(), *data.ToText());
              state = osBack;
              }
           else {
              channel = new cChannel;
              *channel = data;
              Channels.Add(channel);
              Channels.ReNumber();
              isyslog("added channel %d %s", channel->Number(), *data.ToText());
              state = osUser1;
              }
           Channels.SetModified(true);
           }
        else {
           Skins.Message(mtError, tr("Channel settings are not unique!"));
           state = osContinue;
           }
        }
     }
  if (Key != kNone && (data.source & cSource::st_Mask) != (oldSource & cSource::st_Mask))
     Setup();
  return state;
}

// --- cMenuChannelItem ------------------------------------------------------

class cMenuChannelItem : public cOsdItem {
public:
  enum eChannelSortMode { csmNumber, csmName, csmProvider };
private:
  static eChannelSortMode sortMode;
  cChannel *channel;
public:
  cMenuChannelItem(cChannel *Channel);
  static void SetSortMode(eChannelSortMode SortMode) { sortMode = SortMode; }
  static void IncSortMode(void) { sortMode = eChannelSortMode((sortMode == csmProvider) ? csmNumber : sortMode + 1); }
  virtual int Compare(const cListObject &ListObject) const;
  virtual void Set(void);
  cChannel *Channel(void) { return channel; }
  static eChannelSortMode SortMode(void) { return sortMode; }
  };

cMenuChannelItem::eChannelSortMode cMenuChannelItem::sortMode = csmNumber;

cMenuChannelItem::cMenuChannelItem(cChannel *Channel)
{
  channel = Channel;
  if (channel->GroupSep())
     SetSelectable(false);
  Set();
}

int cMenuChannelItem::Compare(const cListObject &ListObject) const
{
  cMenuChannelItem *p = (cMenuChannelItem *)&ListObject;
  int r = -1;
  if (sortMode == csmProvider)
     r = strcoll(channel->Provider(), p->channel->Provider());
  if (sortMode == csmName || r == 0)
     r = strcoll(channel->Name(), p->channel->Name());
  if (sortMode == csmNumber || r == 0)
     r = channel->Number() - p->channel->Number();
  return r;
}

void cMenuChannelItem::Set(void)
{
  char *buffer = NULL;
  if (!channel->GroupSep()) {
     if (sortMode == csmProvider)
        asprintf(&buffer, "%d\t%s - %s", channel->Number(), channel->Provider(), channel->Name());
     else
        asprintf(&buffer, "%d\t%s", channel->Number(), channel->Name());
     }
  else
     asprintf(&buffer, "---\t%s ----------------------------------------------------------------", channel->Name());
  SetText(buffer, false);
}

// --- cMenuChannels ---------------------------------------------------------

class cMenuChannels : public cOsdMenu {
private:
  void Setup(void);
  cChannel *GetChannel(int Index);
  void Propagate(void);
protected:
  eOSState Switch(void);
  eOSState Edit(void);
  eOSState New(void);
  eOSState Delete(void);
  virtual void Move(int From, int To);
public:
  cMenuChannels(void);
  ~cMenuChannels();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuChannels::cMenuChannels(void)
:cOsdMenu(tr("Channels"), CHNUMWIDTH)
{
  Setup();
  Channels.IncBeingEdited();
}

cMenuChannels::~cMenuChannels()
{
  Channels.DecBeingEdited();
}

void cMenuChannels::Setup(void)
{
  cChannel *currentChannel = GetChannel(Current());
  if (!currentChannel)
     currentChannel = Channels.GetByNumber(cDevice::CurrentChannel());
  cMenuChannelItem *currentItem = NULL;
  Clear();
  for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
      if (!channel->GroupSep() || cMenuChannelItem::SortMode() == cMenuChannelItem::csmNumber && *channel->Name()) {
         cMenuChannelItem *item = new cMenuChannelItem(channel);
         Add(item);
         if (channel == currentChannel)
            currentItem = item;
         }
      }
  if (cMenuChannelItem::SortMode() != cMenuChannelItem::csmNumber)
     Sort();
  SetCurrent(currentItem);
  SetHelp(tr("Edit"), tr("New"), tr("Delete"), cMenuChannelItem::SortMode() == cMenuChannelItem::csmNumber ? tr("Mark") : NULL);
  Display();
}

cChannel *cMenuChannels::GetChannel(int Index)
{
  cMenuChannelItem *p = (cMenuChannelItem *)Get(Index);
  return p ? (cChannel *)p->Channel() : NULL;
}

void cMenuChannels::Propagate(void)
{
  Channels.ReNumber();
  for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next())
      ci->Set();
  Display();
  Channels.SetModified(true);
}

eOSState cMenuChannels::Switch(void)
{
  if (HasSubMenu())
     return osContinue;
  cChannel *ch = GetChannel(Current());
  if (ch)
     return cDevice::PrimaryDevice()->SwitchChannel(ch, true) ? osEnd : osContinue;
  return osEnd;
}

eOSState cMenuChannels::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cChannel *ch = GetChannel(Current());
  if (ch)
     return AddSubMenu(new cMenuEditChannel(ch));
  return osContinue;
}

eOSState cMenuChannels::New(void)
{
  if (HasSubMenu())
     return osContinue;
  return AddSubMenu(new cMenuEditChannel(GetChannel(Current()), true));
}

eOSState cMenuChannels::Delete(void)
{
  if (!HasSubMenu() && Count() > 0) {
     int Index = Current();
     cChannel *channel = GetChannel(Current());
     int DeletedChannel = channel->Number();
     // Check if there is a timer using this channel:
     for (cTimer *ti = Timers.First(); ti; ti = Timers.Next(ti)) {
         if (ti->Channel() == channel) {
            Skins.Message(mtError, tr("Channel is being used by a timer!"));
            return osContinue;
            }
         }
     if (Interface->Confirm(tr("Delete channel?"))) {
        Channels.Del(channel);
        cOsdMenu::Del(Index);
        Propagate();
        isyslog("channel %d deleted", DeletedChannel);
        }
     }
  return osContinue;
}

void cMenuChannels::Move(int From, int To)
{
  int CurrentChannelNr = cDevice::CurrentChannel();
  cChannel *CurrentChannel = Channels.GetByNumber(CurrentChannelNr);
  cChannel *FromChannel = GetChannel(From);
  cChannel *ToChannel = GetChannel(To);
  if (FromChannel && ToChannel) {
     int FromNumber = FromChannel->Number();
     int ToNumber = ToChannel->Number();
     Channels.Move(FromChannel, ToChannel);
     cOsdMenu::Move(From, To);
     Propagate();
     isyslog("channel %d moved to %d", FromNumber, ToNumber);
     if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr)
        Channels.SwitchTo(CurrentChannel->Number());
     }
}

eOSState cMenuChannels::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osUser1: {
         cChannel *channel = Channels.Last();
         if (channel) {
            Add(new cMenuChannelItem(channel), true);
            return CloseSubMenu();
            }
         }
         break;
    default:
         if (state == osUnknown) {
            switch (Key) {
              case k0:      cMenuChannelItem::IncSortMode();
                            Setup();
                            break;
              case kOk:     return Switch();
              case kRed:    return Edit();
              case kGreen:  return New();
              case kYellow: return Delete();
              case kBlue:   if (!HasSubMenu() && cMenuChannelItem::SortMode() == cMenuChannelItem::csmNumber)
                               Mark();
                            break;
              default: break;
              }
            }
    }
  return state;
}

// --- cMenuText -------------------------------------------------------------

cMenuText::cMenuText(const char *Title, const char *Text, eDvbFont Font)
:cOsdMenu(Title)
{
  text = NULL;
  SetText(Text);
}

cMenuText::~cMenuText()
{
  free(text);
}

void cMenuText::SetText(const char *Text)
{
  free(text);
  text = Text ? strdup(Text) : NULL;
}

void cMenuText::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetText(text, true);//XXX define control character in text to choose the font???
  cStatus::MsgOsdTextItem(text);
}

eOSState cMenuText::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp);
                  return osContinue;
    default: break;
    }

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
  int channel;
  bool addIfConfirmed;
  cMenuEditDateItem *firstday;
  void SetFirstDayItem(void);
public:
  cMenuEditTimer(cTimer *Timer, bool New = false);
  virtual ~cMenuEditTimer();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditTimer::cMenuEditTimer(cTimer *Timer, bool New)
:cOsdMenu(tr("Edit timer"), 12)
{
  firstday = NULL;
  timer = Timer;
  addIfConfirmed = New;
  if (timer) {
     data = *timer;
     if (New)
        data.SetFlags(tfActive);
     channel = data.Channel()->Number();
     Add(new cMenuEditBitItem( tr("Active"),       &data.flags, tfActive));
     Add(new cMenuEditChanItem(tr("Channel"),      &channel));
     Add(new cMenuEditDateItem(tr("Day"),          &data.day, &data.weekdays));
     Add(new cMenuEditTimeItem(tr("Start"),        &data.start));
     Add(new cMenuEditTimeItem(tr("Stop"),         &data.stop));
     Add(new cMenuEditBitItem( tr("VPS"),          &data.flags, tfVps));
     Add(new cMenuEditIntItem( tr("Priority"),     &data.priority, 0, MAXPRIORITY));
     Add(new cMenuEditIntItem( tr("Lifetime"),     &data.lifetime, 0, MAXLIFETIME));
     Add(new cMenuEditStrItem( tr("File"),          data.file, sizeof(data.file), tr(FileNameChars)));
     SetFirstDayItem();
     }
  Timers.IncBeingEdited();
}

cMenuEditTimer::~cMenuEditTimer()
{
  if (timer && addIfConfirmed)
     delete timer; // apparently it wasn't confirmed
  Timers.DecBeingEdited();
}

void cMenuEditTimer::SetFirstDayItem(void)
{
  if (!firstday && !data.IsSingleEvent()) {
     Add(firstday = new cMenuEditDateItem(tr("First day"), &data.day));
     Display();
     }
  else if (firstday && data.IsSingleEvent()) {
     Del(firstday->Index());
     firstday = NULL;
     Display();
     }
}

eOSState cMenuEditTimer::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     {
                       cChannel *ch = Channels.GetByNumber(channel);
                       if (ch)
                          data.channel = ch;
                       else {
                          Skins.Message(mtError, tr("*** Invalid Channel ***"));
                          break;
                          }
                       if (!*data.file)
                          strcpy(data.file, data.Channel()->ShortName(true));
                       if (timer) {
                          if (memcmp(timer, &data, sizeof(data)) != 0) {
                             *timer = data;
                             if (timer->HasFlags(tfActive))
                                timer->ClrFlags(~tfAll); // allows external programs to mark active timers with values > 0xFFFF and recognize if the user has modified them
                             }
                          if (addIfConfirmed)
                             Timers.Add(timer);
                          timer->Matches();
                          Timers.SetModified();
                          isyslog("timer %s %s (%s)", *timer->ToDescr(), addIfConfirmed ? "added" : "modified", timer->HasFlags(tfActive) ? "active" : "inactive");
                          addIfConfirmed = false;
                          }
                     }
                     return osBack;
       case kRed:
       case kGreen:
       case kYellow:
       case kBlue:   return osContinue;
       default: break;
       }
     }
  if (Key != kNone)
     SetFirstDayItem();
  return state;
}

// --- cMenuTimerItem --------------------------------------------------------

class cMenuTimerItem : public cOsdItem {
private:
  cTimer *timer;
public:
  cMenuTimerItem(cTimer *Timer);
  virtual int Compare(const cListObject &ListObject) const;
  virtual void Set(void);
  cTimer *Timer(void) { return timer; }
  };

cMenuTimerItem::cMenuTimerItem(cTimer *Timer)
{
  timer = Timer;
  Set();
}

int cMenuTimerItem::Compare(const cListObject &ListObject) const
{
  return timer->Compare(*((cMenuTimerItem *)&ListObject)->timer);
}

void cMenuTimerItem::Set(void)
{
  cString day, name("");
  if (timer->WeekDays())
     day = timer->PrintDay(0, timer->WeekDays());
  else if (timer->Day() - time(NULL) < 28 * SECSINDAY) {
     day = itoa(timer->GetMDay(timer->Day()));
     name = WeekDayName(timer->Day());
     }
  else {
     struct tm tm_r;
     time_t Day = timer->Day();
     localtime_r(&Day, &tm_r);
     char buffer[16];
     strftime(buffer, sizeof(buffer), "%Y%m%d", &tm_r);
     day = buffer;
     }
  char *buffer = NULL;
  asprintf(&buffer, "%c\t%d\t%s%s%s\t%02d:%02d\t%02d:%02d\t%s",
                    !(timer->HasFlags(tfActive)) ? ' ' : timer->FirstDay() ? '!' : timer->Recording() ? '#' : '>',
                    timer->Channel()->Number(),
                    *name,
                    *name && **name ? " " : "",
                    *day,
                    timer->Start() / 100,
                    timer->Start() % 100,
                    timer->Stop() / 100,
                    timer->Stop() % 100,
                    timer->File());
  SetText(buffer, false);
}

// --- cMenuTimers -----------------------------------------------------------

class cMenuTimers : public cOsdMenu {
private:
  eOSState Edit(void);
  eOSState New(void);
  eOSState Delete(void);
  eOSState OnOff(void);
  virtual void Move(int From, int To);
  eOSState Summary(void);
  cTimer *CurrentTimer(void);
public:
  cMenuTimers(void);
  virtual ~cMenuTimers();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuTimers::cMenuTimers(void)
:cOsdMenu(tr("Timers"), 2, CHNUMWIDTH, 10, 6, 6)
{
  for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer))
      Add(new cMenuTimerItem(timer));
  if (Setup.SortTimers)
     Sort();
  SetHelp(tr("Edit"), tr("New"), tr("Delete"), Setup.SortTimers ? tr("On/Off") : tr("Mark"));
  Timers.IncBeingEdited();
}

cMenuTimers::~cMenuTimers()
{
  Timers.DecBeingEdited();
}

cTimer *cMenuTimers::CurrentTimer(void)
{
  cMenuTimerItem *item = (cMenuTimerItem *)Get(Current());
  return item ? item->Timer() : NULL;
}

eOSState cMenuTimers::OnOff(void)
{
  cTimer *timer = CurrentTimer();
  if (timer) {
     timer->OnOff();
     RefreshCurrent();
     DisplayCurrent(true);
     if (timer->FirstDay())
        isyslog("timer %s first day set to %s", *timer->ToDescr(), *timer->PrintFirstDay());
     else
        isyslog("timer %s %sactivated", *timer->ToDescr(), timer->HasFlags(tfActive) ? "" : "de");
     Timers.SetModified();
     }
  return osContinue;
}

eOSState cMenuTimers::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  isyslog("editing timer %s", *CurrentTimer()->ToDescr());
  return AddSubMenu(new cMenuEditTimer(CurrentTimer()));
}

eOSState cMenuTimers::New(void)
{
  if (HasSubMenu())
     return osContinue;
  return AddSubMenu(new cMenuEditTimer(new cTimer, true));
}

eOSState cMenuTimers::Delete(void)
{
  // Check if this timer is active:
  cTimer *ti = CurrentTimer();
  if (ti) {
     if (Interface->Confirm(tr("Delete timer?"))) {
        if (ti->Recording()) {
           if (Interface->Confirm(tr("Timer still recording - really delete?"))) {
              ti->Skip();
              cRecordControls::Process(time(NULL));
              }
           else
              return osContinue;
           }
        isyslog("deleting timer %s", *ti->ToDescr());
        Timers.Del(ti);
        cOsdMenu::Del(Current());
        Timers.SetModified();
        Display();
        }
     }
  return osContinue;
}

void cMenuTimers::Move(int From, int To)
{
  Timers.Move(From, To);
  cOsdMenu::Move(From, To);
  Timers.SetModified();
  Display();
  isyslog("timer %d moved to %d", From + 1, To + 1);
}

eOSState cMenuTimers::Summary(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cTimer *ti = CurrentTimer();
  if (ti && !isempty(ti->Summary()))
     return AddSubMenu(new cMenuText(tr("Summary"), ti->Summary()));
  return Edit(); // convenience for people not using the Summary feature ;-)
}

eOSState cMenuTimers::ProcessKey(eKeys Key)
{
  int TimerNumber = HasSubMenu() ? Count() : -1;
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Summary();
       case kRed:    return Edit();
       case kGreen:  return New();
       case kYellow: return Delete();
       case kBlue:   if (Setup.SortTimers)
                        OnOff();
                     else
                        Mark();
                     break;
       default: break;
       }
     }
  if (TimerNumber >= 0 && !HasSubMenu() && Timers.Get(TimerNumber)) {
     // a newly created timer was confirmed with Ok
     Add(new cMenuTimerItem(Timers.Get(TimerNumber)), true);
     Display();
     }
  return state;
}

// --- cMenuEvent ------------------------------------------------------------

class cMenuEvent : public cOsdMenu {
private:
  const cEvent *event;
public:
  cMenuEvent(const cEvent *Event, bool CanSwitch = false);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuEvent::cMenuEvent(const cEvent *Event, bool CanSwitch)
:cOsdMenu(tr("Event"))
{
  event = Event;
  if (event) {
     cChannel *channel = Channels.GetByChannelID(event->ChannelID(), true);
     if (channel) {
        SetTitle(channel->Name());
        SetHelp(tr("Record"), NULL, NULL, CanSwitch ? tr("Switch") : NULL);
        }
     }
}

void cMenuEvent::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetEvent(event);
  cStatus::MsgOsdTextItem(event->Description());
}

eOSState cMenuEvent::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp);
                  return osContinue;
    default: break;
    }

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
  const cEvent *event;
  const cChannel *channel;
  cMenuWhatsOnItem(const cEvent *Event, cChannel *Channel);
};

cMenuWhatsOnItem::cMenuWhatsOnItem(const cEvent *Event, cChannel *Channel)
{
  event = Event;
  channel = Channel;
  char *buffer = NULL;
  int TimerMatch;
  char t = Timers.GetMatch(Event, &TimerMatch) ? (TimerMatch == tmFull) ? 'T' : 't' : ' ';
  char v = event->Vps() && (event->Vps() - event->StartTime()) ? 'V' : ' ';
  char r = event->IsRunning() ? '*' : ' ';
  asprintf(&buffer, "%d\t%.*s\t%s\t%c%c%c\t%s", channel->Number(), 6, channel->ShortName(true), *event->GetTimeString(), t, v, r, event->Title());
  SetText(buffer, false);
}

// --- cMenuWhatsOn ----------------------------------------------------------

class cMenuWhatsOn : public cOsdMenu {
private:
  eOSState Record(void);
  eOSState Switch(void);
  static int currentChannel;
  static const cEvent *scheduleEvent;
public:
  cMenuWhatsOn(const cSchedules *Schedules, bool Now, int CurrentChannelNr);
  static int CurrentChannel(void) { return currentChannel; }
  static void SetCurrentChannel(int ChannelNr) { currentChannel = ChannelNr; }
  static const cEvent *ScheduleEvent(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

int cMenuWhatsOn::currentChannel = 0;
const cEvent *cMenuWhatsOn::scheduleEvent = NULL;

cMenuWhatsOn::cMenuWhatsOn(const cSchedules *Schedules, bool Now, int CurrentChannelNr)
:cOsdMenu(Now ? tr("What's on now?") : tr("What's on next?"), CHNUMWIDTH, 7, 6, 4)
{
  for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
      if (!Channel->GroupSep()) {
         const cSchedule *Schedule = Schedules->GetSchedule(Channel->GetChannelID());
         if (Schedule) {
            const cEvent *Event = Now ? Schedule->GetPresentEvent() : Schedule->GetFollowingEvent();
            if (Event)
               Add(new cMenuWhatsOnItem(Event, Channel), Channel->Number() == CurrentChannelNr);
            }
         }
      }
  currentChannel = CurrentChannelNr;
  SetHelp(Count() ? tr("Record") : NULL, Now ? tr("Next") : tr("Now"), tr("Button$Schedule"), tr("Switch"));
}

const cEvent *cMenuWhatsOn::ScheduleEvent(void)
{
  const cEvent *ei = scheduleEvent;
  scheduleEvent = NULL;
  return ei;
}

eOSState cMenuWhatsOn::Switch(void)
{
  cMenuWhatsOnItem *item = (cMenuWhatsOnItem *)Get(Current());
  if (item) {
     cChannel *channel = Channels.GetByChannelID(item->event->ChannelID(), true);
     if (channel && cDevice::PrimaryDevice()->SwitchChannel(channel, true))
        return osEnd;
     }
  Skins.Message(mtError, tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuWhatsOn::Record(void)
{
  cMenuWhatsOnItem *item = (cMenuWhatsOnItem *)Get(Current());
  if (item) {
     cTimer *timer = new cTimer(item->event);
     cTimer *t = Timers.GetTimer(timer);
     if (t) {
        delete timer;
        timer = t;
        }
     return AddSubMenu(new cMenuEditTimer(timer, !t));
     }
  return osContinue;
}

eOSState cMenuWhatsOn::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRecord:
       case kRed:    return Record();
       case kYellow: state = osBack;
                     // continue with kGreen
       case kGreen:  {
                       cMenuWhatsOnItem *mi = (cMenuWhatsOnItem *)Get(Current());
                       if (mi) {
                          scheduleEvent = mi->event;
                          currentChannel = mi->channel->Number();
                          }
                     }
                     break;
       case kBlue:   return Switch();
       case kOk:     if (Count())
                        return AddSubMenu(new cMenuEvent(((cMenuWhatsOnItem *)Get(Current()))->event, true));
                     break;
       default:      break;
       }
     }
  return state;
}

// --- cMenuScheduleItem -----------------------------------------------------

class cMenuScheduleItem : public cOsdItem {
public:
  const cEvent *event;
  cMenuScheduleItem(const cEvent *Event);
};

cMenuScheduleItem::cMenuScheduleItem(const cEvent *Event)
{
  event = Event;
  char *buffer = NULL;
  int TimerMatch;
  char t = Timers.GetMatch(Event, &TimerMatch) ? (TimerMatch == tmFull) ? 'T' : 't' : ' ';
  char v = event->Vps() && (event->Vps() - event->StartTime()) ? 'V' : ' ';
  char r = event->IsRunning() ? '*' : ' ';
  asprintf(&buffer, "%.*s\t%s\t%c%c%c\t%s", 6, *event->GetDateString(), *event->GetTimeString(), t, v, r, event->Title());
  SetText(buffer, false);
}

// --- cMenuSchedule ---------------------------------------------------------

class cMenuSchedule : public cOsdMenu {
private:
  cSchedulesLock schedulesLock;
  const cSchedules *schedules;
  bool now, next;
  int otherChannel;
  eOSState Record(void);
  eOSState Switch(void);
  void PrepareSchedule(cChannel *Channel);
public:
  cMenuSchedule(void);
  virtual ~cMenuSchedule();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSchedule::cMenuSchedule(void)
:cOsdMenu("", 7, 6, 4)
{
  now = next = false;
  otherChannel = 0;
  cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
  if (channel) {
     cMenuWhatsOn::SetCurrentChannel(channel->Number());
     schedules = cSchedules::Schedules(schedulesLock);
     PrepareSchedule(channel);
     SetHelp(Count() ? tr("Record") : NULL, tr("Now"), tr("Next"));
     }
}

cMenuSchedule::~cMenuSchedule()
{
  cMenuWhatsOn::ScheduleEvent(); // makes sure any posted data is cleared
}

void cMenuSchedule::PrepareSchedule(cChannel *Channel)
{
  Clear();
  char *buffer = NULL;
  asprintf(&buffer, tr("Schedule - %s"), Channel->Name());
  SetTitle(buffer);
  free(buffer);
  if (schedules) {
     const cSchedule *Schedule = schedules->GetSchedule(Channel->GetChannelID());
     if (Schedule) {
        const cEvent *PresentEvent = Schedule->GetPresentEvent(Channel->Number() == cDevice::CurrentChannel());
        time_t now = time(NULL) - Setup.EPGLinger * 60;
        for (const cEvent *Event = Schedule->Events()->First(); Event; Event = Schedule->Events()->Next(Event)) {
            if (Event->EndTime() > now || Event == PresentEvent)
               Add(new cMenuScheduleItem(Event), Event == PresentEvent);
            }
        }
     }
}

eOSState cMenuSchedule::Record(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     cTimer *timer = new cTimer(item->event);
     cTimer *t = Timers.GetTimer(timer);
     if (t) {
        delete timer;
        timer = t;
        }
     return AddSubMenu(new cMenuEditTimer(timer, !t));
     }
  return osContinue;
}

eOSState cMenuSchedule::Switch(void)
{
  if (otherChannel) {
     if (Channels.SwitchTo(otherChannel))
        return osEnd;
     }
  Skins.Message(mtError, tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuSchedule::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRecord:
       case kRed:    return Record();
       case kGreen:  if (schedules) {
                        if (!now && !next) {
                           int ChannelNr = 0;
                           if (Count()) {
                              cChannel *channel = Channels.GetByChannelID(((cMenuScheduleItem *)Get(Current()))->event->ChannelID(), true);
                              if (channel)
                                 ChannelNr = channel->Number();
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
                        return AddSubMenu(new cMenuEvent(((cMenuScheduleItem *)Get(Current()))->event, otherChannel));
                     break;
       default:      break;
       }
     }
  else if (!HasSubMenu()) {
     now = next = false;
     const cEvent *ei = cMenuWhatsOn::ScheduleEvent();
     if (ei) {
        cChannel *channel = Channels.GetByChannelID(ei->ChannelID(), true);
        if (channel) {
           PrepareSchedule(channel);
           if (channel->Number() != cDevice::CurrentChannel()) {
              otherChannel = channel->Number();
              SetHelp(Count() ? tr("Record") : NULL, tr("Now"), tr("Next"), tr("Switch"));
              }
           Display();
           }
        }
     }
  return state;
}

// --- cMenuCommands ---------------------------------------------------------

class cMenuCommands : public cOsdMenu {
private:
  cCommands *commands;
  char *parameters;
  eOSState Execute(void);
public:
  cMenuCommands(const char *Title, cCommands *Commands, const char *Parameters = NULL);
  virtual ~cMenuCommands();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuCommands::cMenuCommands(const char *Title, cCommands *Commands, const char *Parameters)
:cOsdMenu(Title)
{
  SetHasHotkeys();
  commands = Commands;
  parameters = Parameters ? strdup(Parameters) : NULL;
  for (cCommand *command = commands->First(); command; command = commands->Next(command))
      Add(new cOsdItem(hk(command->Title())));
}

cMenuCommands::~cMenuCommands()
{
  free(parameters);
}

eOSState cMenuCommands::Execute(void)
{
  cCommand *command = commands->Get(Current());
  if (command) {
     char *buffer = NULL;
     bool confirmed = true;
     if (command->Confirm()) {
        asprintf(&buffer, "%s?", command->Title());
        confirmed = Interface->Confirm(buffer);
        free(buffer);
        }
     if (confirmed) {
        asprintf(&buffer, "%s...", command->Title());
        Skins.Message(mtStatus, buffer);
        free(buffer);
        const char *Result = command->Execute(parameters);
        Skins.Message(mtStatus, NULL);
        if (Result)
           return AddSubMenu(new cMenuText(command->Title(), Result, fontFix));
        return osEnd;
        }
     }
  return osContinue;
}

eOSState cMenuCommands::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:
       case kGreen:
       case kYellow:
       case kBlue:   return osContinue;
       case kOk:     return Execute();
       default:      break;
       }
     }
  return state;
}

// --- cMenuCam --------------------------------------------------------------

cMenuCam::cMenuCam(cCiMenu *CiMenu)
:cOsdMenu("")
{
  ciMenu = CiMenu;
  selected = false;
  if (ciMenu->Selectable())
     SetHasHotkeys();
  SetTitle(ciMenu->TitleText() ? ciMenu->TitleText() : "CAM");
  for (int i = 0; i < ciMenu->NumEntries(); i++)
      Add(new cOsdItem(hk(ciMenu->Entry(i))));
  //XXX implement a clean way of displaying this:
  Add(new cOsdItem(ciMenu->SubTitleText()));
  Add(new cOsdItem(ciMenu->BottomText()));
  Display();
  dsyslog("CAM: Menu - %s", ciMenu->TitleText());
  lastActivity = time(NULL);
}

cMenuCam::~cMenuCam()
{
  if (!selected)
     ciMenu->Cancel();
  delete ciMenu;
}

eOSState cMenuCam::Select(void)
{
  if (ciMenu->Selectable()) {
     ciMenu->Select(Current());
     selected = true;
     }
  return osEnd;
}

eOSState cMenuCam::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Select();
       default: break;
       }
     }
  if (Key != kNone)
     lastActivity = time(NULL);
  else if (time(NULL) - lastActivity > MENUTIMEOUT)
     state = osEnd;
  return state;
}

// --- cMenuCamEnquiry -------------------------------------------------------

//XXX this is just quick and dirty - make this a separate display object
cMenuCamEnquiry::cMenuCamEnquiry(cCiEnquiry *CiEnquiry)
:cOsdMenu("", 10)
{
  ciEnquiry = CiEnquiry;
  int Length = ciEnquiry->ExpectedLength();
  input = MALLOC(char, Length + 1);
  *input = 0;
  replied = false;
  SetTitle(ciEnquiry->Text() ? ciEnquiry->Text() : "CAM");
  Add(new cMenuEditNumItem("Input", input, Length, ciEnquiry->Blind()));
  Display();
  lastActivity = time(NULL);
}

cMenuCamEnquiry::~cMenuCamEnquiry()
{
  if (!replied)
     ciEnquiry->Cancel();
  free(input);
  delete ciEnquiry;
}

eOSState cMenuCamEnquiry::Reply(void)
{
  //XXX check length???
  ciEnquiry->Reply(input);
  replied = true;
  return osEnd;
}

eOSState cMenuCamEnquiry::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Reply();
       default: break;
       }
     }
  if (Key != kNone)
     lastActivity = time(NULL);
  else if (time(NULL) - lastActivity > MENUTIMEOUT)
     state = osEnd;
  return state;
}

// --- CamControl ------------------------------------------------------------

cOsdObject *CamControl(void)
{
  for (int d = 0; d < cDevice::NumDevices(); d++) {
      cDevice *Device = cDevice::GetDevice(d);
      if (Device) {
         cCiHandler *CiHandler = Device->CiHandler();
         if (CiHandler && CiHandler->HasUserIO()) {
            cCiMenu *CiMenu = CiHandler->GetMenu();
            if (CiMenu)
               return new cMenuCam(CiMenu);
            else {
               cCiEnquiry *CiEnquiry = CiHandler->GetEnquiry();
               if (CiEnquiry)
                  return new cMenuCamEnquiry(CiEnquiry);
               }
            }
         }
      }
  return NULL;
}

// --- cMenuRecording --------------------------------------------------------

class cMenuRecording : public cOsdMenu {
private:
  const cRecording *recording;
public:
  cMenuRecording(const cRecording *Recording);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuRecording::cMenuRecording(const cRecording *Recording)
:cOsdMenu(tr("Recording info"))
{
  recording = Recording;
  if (recording)
     SetHelp(tr("Play"), tr("Rewind"));
}

void cMenuRecording::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetRecording(recording);
  cStatus::MsgOsdTextItem(recording->Info()->Description());
}

eOSState cMenuRecording::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp);
                  return osContinue;
    default: break;
    }

  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:    Key = kOk; // will play the recording, even if recording commands are defined
       case kGreen:  cRemote::Put(Key, true);
                     // continue with osBack to close the info menu and process the key
       case kOk:     return osBack;
       default: break;
       }
     }
  return state;
}

// --- cMenuRecordingItem ----------------------------------------------------

class cMenuRecordingItem : public cOsdItem {
private:
  char *fileName;
  char *name;
  int totalEntries, newEntries;
public:
  cMenuRecordingItem(cRecording *Recording, int Level);
  ~cMenuRecordingItem();
  void IncrementCounter(bool New);
  const char *Name(void) { return name; }
  const char *FileName(void) { return fileName; }
  bool IsDirectory(void) { return name != NULL; }
  };

cMenuRecordingItem::cMenuRecordingItem(cRecording *Recording, int Level)
{
  fileName = strdup(Recording->FileName());
  name = NULL;
  totalEntries = newEntries = 0;
  SetText(Recording->Title('\t', true, Level));
  if (*Text() == '\t')
     name = strdup(Text() + 2); // 'Text() + 2' to skip the two '\t'
}

cMenuRecordingItem::~cMenuRecordingItem()
{
  free(fileName);
  free(name);
}

void cMenuRecordingItem::IncrementCounter(bool New)
{
  totalEntries++;
  if (New)
     newEntries++;
  char *buffer = NULL;
  asprintf(&buffer, "%d\t%d\t%s", totalEntries, newEntries, name);
  SetText(buffer, false);
}

// --- cMenuRecordings -------------------------------------------------------

int cMenuRecordings::helpKeys = -1;

cMenuRecordings::cMenuRecordings(const char *Base, int Level, bool OpenSubMenus)
:cOsdMenu(Base ? Base : tr("Recordings"), 8, 6)
{
  base = Base ? strdup(Base) : NULL;
  level = Setup.RecordingDirs ? Level : -1;
  Display(); // this keeps the higher level menus from showing up briefly when pressing 'Back' during replay
  const char *LastReplayed = cReplayControl::LastReplayed();
  cMenuRecordingItem *LastItem = NULL;
  char *LastItemText = NULL;
  if (!Base)
     Recordings.Sort();
  for (cRecording *recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
      if (!Base || (strstr(recording->Name(), Base) == recording->Name() && recording->Name()[strlen(Base)] == '~')) {
         cMenuRecordingItem *Item = new cMenuRecordingItem(recording, level);
         if (*Item->Text() && (!LastItem || strcmp(Item->Text(), LastItemText) != 0)) {
            Add(Item);
            LastItem = Item;
            free(LastItemText);
            LastItemText = strdup(LastItem->Text()); // must use a copy because of the counters!
            }
         else
            delete Item;
         if (LastItem) {
            if (LastReplayed && strcmp(LastReplayed, recording->FileName()) == 0)
               SetCurrent(LastItem);
            if (LastItem->IsDirectory())
               LastItem->IncrementCounter(recording->IsNew());
            }
         }
      }
  free(LastItemText);
  if (Current() < 0)
     SetCurrent(First());
  else if (OpenSubMenus && Open(true))
     return;
  SetHelpKeys();
}

cMenuRecordings::~cMenuRecordings()
{
  helpKeys = -1;
  free(base);
}

void cMenuRecordings::SetHelpKeys(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  int NewHelpKeys = helpKeys;
  if (ri) {
     if (ri->IsDirectory())
        NewHelpKeys = 1;
     else {
        NewHelpKeys = 2;
        cRecording *recording = GetRecording(ri);
        if (recording && recording->Info()->Title())
           NewHelpKeys = 3;
        }
     }
  if (NewHelpKeys != helpKeys) {
     switch (NewHelpKeys) {
       case 0: SetHelp(NULL); break;
       case 1: SetHelp(tr("Open")); break;
       case 2:
       case 3: SetHelp(RecordingCommands.Count() ? tr("Commands") : tr("Play"), tr("Rewind"), tr("Delete"), NewHelpKeys == 3 ? tr("Info") : NULL);
       }
     helpKeys = NewHelpKeys;
     }
}

cRecording *cMenuRecordings::GetRecording(cMenuRecordingItem *Item)
{
  cRecording *recording = Recordings.GetByName(Item->FileName());
  if (!recording)
     Skins.Message(mtError, tr("Error while accessing recording!"));
  return recording;
}

bool cMenuRecordings::Open(bool OpenSubMenus)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && ri->IsDirectory()) {
     const char *t = ri->Name();
     char *buffer = NULL;
     if (base) {
        asprintf(&buffer, "%s~%s", base, t);
        t = buffer;
        }
     AddSubMenu(new cMenuRecordings(t, level + 1, OpenSubMenus));
     free(buffer);
     return true;
     }
  return false;
}

eOSState cMenuRecordings::Play(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri) {
     if (ri->IsDirectory())
        Open();
     else {
        cRecording *recording = GetRecording(ri);
        if (recording) {
           cReplayControl::SetRecording(recording->FileName(), recording->Title());
           return osReplay;
           }
        }
     }
  return osContinue;
}

eOSState cMenuRecordings::Rewind(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     cDevice::PrimaryDevice()->StopReplay(); // must do this first to be able to rewind the currently replayed recording
     cResumeFile ResumeFile(ri->FileName());
     ResumeFile.Delete();
     return Play();
     }
  return osContinue;
}

eOSState cMenuRecordings::Delete(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     if (Interface->Confirm(tr("Delete recording?"))) {
        cRecordControl *rc = cRecordControls::GetRecordControl(ri->FileName());
        if (rc) {
           if (Interface->Confirm(tr("Timer still recording - really delete?"))) {
              cTimer *timer = rc->Timer();
              if (timer) {
                 timer->Skip();
                 cRecordControls::Process(time(NULL));
                 if (timer->IsSingleEvent()) {
                    isyslog("deleting timer %s", *timer->ToDescr());
                    Timers.Del(timer);
                    }
                 Timers.SetModified();
                 }
              }
           else
              return osContinue;
           }
        cRecording *recording = GetRecording(ri);
        if (recording) {
           if (recording->Delete()) {
              cReplayControl::ClearLastReplayed(ri->FileName());
              cOsdMenu::Del(Current());
              Recordings.Del(recording);
              SetHelpKeys();
              Display();
              if (!Count())
                 return osBack;
              }
           else
              Skins.Message(mtError, tr("Error while deleting recording!"));
           }
        }
     }
  return osContinue;
}

eOSState cMenuRecordings::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     cRecording *recording = GetRecording(ri);
     if (recording && recording->Info()->Title())
        return AddSubMenu(new cMenuRecording(recording));
     }
  return osContinue;
}

eOSState cMenuRecordings::Commands(eKeys Key)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     cRecording *recording = GetRecording(ri);
     if (recording) {
        char *parameter = NULL;
        asprintf(&parameter, "'%s'", recording->FileName());
        cMenuCommands *menu;
        eOSState state = AddSubMenu(menu = new cMenuCommands(tr("Recording commands"), &RecordingCommands, parameter));
        free(parameter);
        if (Key != kNone)
           state = menu->ProcessKey(Key);
        return state;
        }
     }
  return osContinue;
}

eOSState cMenuRecordings::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Play();
       case kRed:    return (helpKeys > 1 && RecordingCommands.Count()) ? Commands() : Play();
       case kGreen:  return Rewind();
       case kYellow: return Delete();
       case kBlue:   return Info();
       case k1...k9: return Commands(Key);
       default: break;
       }
     }
  if (Key == kYellow && HadSubMenu && !HasSubMenu()) {
     // the last recording in a subdirectory was deleted, so let's go back up
     cOsdMenu::Del(Current());
     if (!Count())
        return osBack;
     Display();
     }
  if (!HasSubMenu() && Key != kNone)
     SetHelpKeys();
  return state;
}

// --- cMenuSetupBase --------------------------------------------------------

class cMenuSetupBase : public cMenuSetupPage {
protected:
  cSetup data;
  virtual void Store(void);
public:
  cMenuSetupBase(void);
  };

cMenuSetupBase::cMenuSetupBase(void)
{
  data = Setup;
}

void cMenuSetupBase::Store(void)
{
  Setup = data;
  Setup.Save();
}

// --- cMenuSetupOSD ---------------------------------------------------------

class cMenuSetupOSD : public cMenuSetupBase {
private:
  const char *useSmallFontTexts[3];
  int numSkins;
  int originalSkinIndex;
  int skinIndex;
  const char **skinDescriptions;
  cThemes themes;
  int themeIndex;
  virtual void Set(void);
public:
  cMenuSetupOSD(void);
  virtual ~cMenuSetupOSD();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupOSD::cMenuSetupOSD(void)
{
  numSkins = Skins.Count();
  skinIndex = originalSkinIndex = Skins.Current()->Index();
  skinDescriptions = new const char*[numSkins];
  themes.Load(Skins.Current()->Name());
  themeIndex = Skins.Current()->Theme() ? themes.GetThemeIndex(Skins.Current()->Theme()->Description()) : 0;
  Set();
}

cMenuSetupOSD::~cMenuSetupOSD()
{
  cFont::SetCode(I18nCharSets()[Setup.OSDLanguage]);
  delete skinDescriptions;
}

void cMenuSetupOSD::Set(void)
{
  int current = Current();
  for (cSkin *Skin = Skins.First(); Skin; Skin = Skins.Next(Skin))
      skinDescriptions[Skin->Index()] = Skin->Description();
  useSmallFontTexts[0] = tr("never");
  useSmallFontTexts[1] = tr("skin dependent");
  useSmallFontTexts[2] = tr("always");
  Clear();
  SetSection(tr("OSD"));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Language"),               &data.OSDLanguage, I18nNumLanguages, I18nLanguages()));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Skin"),                   &skinIndex, numSkins, skinDescriptions));
  if (themes.NumThemes())
  Add(new cMenuEditStraItem(tr("Setup.OSD$Theme"),                  &themeIndex, themes.NumThemes(), themes.Descriptions()));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Left"),                   &data.OSDLeft, 0, MAXOSDWIDTH));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Top"),                    &data.OSDTop, 0, MAXOSDHEIGHT));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Width"),                  &data.OSDWidth, MINOSDWIDTH, MAXOSDWIDTH));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Height"),                 &data.OSDHeight, MINOSDHEIGHT, MAXOSDHEIGHT));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Message time (s)"),       &data.OSDMessageTime, 1, 60));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Use small font"),         &data.UseSmallFont, 3, useSmallFontTexts));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Channel info position"),  &data.ChannelInfoPos, tr("bottom"), tr("top")));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Channel info time (s)"),  &data.ChannelInfoTime, 1, 60));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Info on channel switch"), &data.ShowInfoOnChSwitch));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Scroll pages"),           &data.MenuScrollPage));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Scroll wraps"),           &data.MenuScrollWrap));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Sort timers"),            &data.SortTimers));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Recording directories"),  &data.RecordingDirs));
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupOSD::ProcessKey(eKeys Key)
{
  if (Key == kOk) {
     if (skinIndex != originalSkinIndex) {
        cSkin *Skin = Skins.Get(skinIndex);
        if (Skin) {
           strn0cpy(data.OSDSkin, Skin->Name(), sizeof(data.OSDSkin));
           Skins.SetCurrent(Skin->Name());
           }
        }
     if (themes.NumThemes() && Skins.Current()->Theme()) {
        Skins.Current()->Theme()->Load(themes.FileName(themeIndex));
        strn0cpy(data.OSDTheme, themes.Name(themeIndex), sizeof(data.OSDTheme));
        }
     data.OSDWidth &= ~0x07; // OSD width must be a multiple of 8
     }

  int osdLanguage = data.OSDLanguage;
  int oldSkinIndex = skinIndex;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (data.OSDLanguage != osdLanguage || skinIndex != oldSkinIndex) {
     int OriginalOSDLanguage = Setup.OSDLanguage;
     Setup.OSDLanguage = data.OSDLanguage;
     cFont::SetCode(I18nCharSets()[Setup.OSDLanguage]);

     cSkin *Skin = Skins.Get(skinIndex);
     if (Skin) {
        char *d = themes.NumThemes() ? strdup(themes.Descriptions()[themeIndex]) : NULL;
        themes.Load(Skin->Name());
        if (skinIndex != oldSkinIndex)
           themeIndex = d ? themes.GetThemeIndex(d) : 0;
        free(d);
        }
     
     Set();
     Setup.OSDLanguage = OriginalOSDLanguage;
     }
  return state;
}

// --- cMenuSetupEPG ---------------------------------------------------------

class cMenuSetupEPG : public cMenuSetupBase {
private:
  int originalNumLanguages;
  int numLanguages;
  void Setup(void);
public:
  cMenuSetupEPG(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupEPG::cMenuSetupEPG(void)
{
  for (numLanguages = 0; numLanguages < I18nNumLanguages && data.EPGLanguages[numLanguages] >= 0; numLanguages++)
      ;
  originalNumLanguages = numLanguages;
  SetSection(tr("EPG"));
  SetHelp(tr("Scan"));
  Setup();
}

void cMenuSetupEPG::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG scan timeout (h)"),      &data.EPGScanTimeout));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG bugfix level"),          &data.EPGBugfixLevel, 0, MAXEPGBUGFIXLEVEL));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG linger time (min)"),     &data.EPGLinger, 0));
  Add(new cMenuEditBoolItem(tr("Setup.EPG$Set system time"),           &data.SetSystemTime));
  if (data.SetSystemTime)
     Add(new cMenuEditTranItem(tr("Setup.EPG$Use time from transponder"), &data.TimeTransponder, &data.TimeSource));
  Add(new cMenuEditIntItem( tr("Setup.EPG$Preferred languages"),       &numLanguages, 0, I18nNumLanguages));
  for (int i = 0; i < numLanguages; i++)
     Add(new cMenuEditStraItem(tr("Setup.EPG$Preferred language"),     &data.EPGLanguages[i], I18nNumLanguages, I18nLanguages()));

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupEPG::ProcessKey(eKeys Key)
{
  if (Key == kOk) {
     bool Modified = numLanguages != originalNumLanguages;
     if (!Modified) {
        for (int i = 0; i < numLanguages; i++) {
            if (data.EPGLanguages[i] != ::Setup.EPGLanguages[i]) {
               Modified = true;
               break;
               }
            }
        }
     if (Modified)
        cSchedules::ResetVersions();
     }

  int oldnumLanguages = numLanguages;
  int oldSetSystemTime = data.SetSystemTime;

  eOSState state = cMenuSetupBase::ProcessKey(Key);
  if (Key != kNone) {
     if (numLanguages != oldnumLanguages || data.SetSystemTime != oldSetSystemTime) {
        for (int i = oldnumLanguages; i < numLanguages; i++) {
            data.EPGLanguages[i] = 0;
            for (int l = 0; l < I18nNumLanguages; l++) {
                int k;
                for (k = 0; k < oldnumLanguages; k++) {
                    if (data.EPGLanguages[k] == l)
                       break;
                    }
                if (k >= oldnumLanguages) {
                   data.EPGLanguages[i] = l;
                   break;
                   }
                }
            }
        data.EPGLanguages[numLanguages] = -1;
        Setup();
        }
     if (Key == kRed) {
        EITScanner.ForceScan();
        return osEnd;
        }
     }
  return state;
}

// --- cMenuSetupDVB ---------------------------------------------------------

class cMenuSetupDVB : public cMenuSetupBase {
private:
  int originalNumAudioLanguages;
  int numAudioLanguages;
  void Setup(void);
  const char *videoDisplayFormatTexts[3];
  const char *updateChannelsTexts[5];
public:
  cMenuSetupDVB(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupDVB::cMenuSetupDVB(void)
{
  for (numAudioLanguages = 0; numAudioLanguages < I18nNumLanguages && data.AudioLanguages[numAudioLanguages] >= 0; numAudioLanguages++)
      ;
  originalNumAudioLanguages = numAudioLanguages;
  videoDisplayFormatTexts[0] = tr("pan&scan");
  videoDisplayFormatTexts[1] = tr("letterbox");
  videoDisplayFormatTexts[2] = tr("center cut out");
  updateChannelsTexts[0] = tr("no");
  updateChannelsTexts[1] = tr("names only");
  updateChannelsTexts[2] = tr("names and PIDs");
  updateChannelsTexts[3] = tr("add new channels");
  updateChannelsTexts[4] = tr("add new transponders");

  SetSection(tr("DVB"));
  Setup();
}

void cMenuSetupDVB::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditIntItem( tr("Setup.DVB$Primary DVB interface"), &data.PrimaryDVB, 1, cDevice::NumDevices()));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Video format"),          &data.VideoFormat, "4:3", "16:9"));
  if (data.VideoFormat == 0)
     Add(new cMenuEditStraItem(tr("Setup.DVB$Video display format"), &data.VideoDisplayFormat, 3, videoDisplayFormatTexts));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Use Dolby Digital"),     &data.UseDolbyDigital));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Update channels"),       &data.UpdateChannels, 5, updateChannelsTexts));
  Add(new cMenuEditIntItem( tr("Setup.DVB$Audio languages"),       &numAudioLanguages, 0, I18nNumLanguages));
  for (int i = 0; i < numAudioLanguages; i++)
      Add(new cMenuEditStraItem(tr("Setup.DVB$Audio language"),    &data.AudioLanguages[i], I18nNumLanguages, I18nLanguages()));

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupDVB::ProcessKey(eKeys Key)
{
  int oldPrimaryDVB = ::Setup.PrimaryDVB;
  int oldVideoDisplayFormat = ::Setup.VideoDisplayFormat;
  bool oldVideoFormat = ::Setup.VideoFormat;
  bool newVideoFormat = data.VideoFormat;
  int oldnumAudioLanguages = numAudioLanguages;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (Key != kNone) {
     bool DoSetup = data.VideoFormat != newVideoFormat;
     if (numAudioLanguages != oldnumAudioLanguages) {
        for (int i = oldnumAudioLanguages; i < numAudioLanguages; i++) {
            data.AudioLanguages[i] = 0;
            for (int l = 0; l < I18nNumLanguages; l++) {
                int k;
                for (k = 0; k < oldnumAudioLanguages; k++) {
                    if (data.AudioLanguages[k] == l)
                       break;
                    }
                if (k >= oldnumAudioLanguages) {
                   data.AudioLanguages[i] = l;
                   break;
                   }
                }
            }
        data.AudioLanguages[numAudioLanguages] = -1;
        DoSetup = true;
        }
     if (DoSetup)
        Setup();
     }
  if (state == osBack && Key == kOk) {
     if (::Setup.PrimaryDVB != oldPrimaryDVB)
        state = osSwitchDvb;
     if (::Setup.VideoDisplayFormat != oldVideoDisplayFormat)
        cDevice::PrimaryDevice()->SetVideoDisplayFormat(eVideoDisplayFormat(::Setup.VideoDisplayFormat));
     if (::Setup.VideoFormat != oldVideoFormat)
        cDevice::PrimaryDevice()->SetVideoFormat(::Setup.VideoFormat);
     }
  return state;
}

// --- cMenuSetupLNB ---------------------------------------------------------

class cMenuSetupLNB : public cMenuSetupBase {
private:
  void Setup(void);
public:
  cMenuSetupLNB(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupLNB::cMenuSetupLNB(void)
{
  SetSection(tr("LNB"));
  Setup();
}

void cMenuSetupLNB::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditBoolItem(tr("Setup.LNB$Use DiSEqC"),               &data.DiSEqC));
  if (!data.DiSEqC) {
     Add(new cMenuEditIntItem( tr("Setup.LNB$SLOF (MHz)"),               &data.LnbSLOF));
     Add(new cMenuEditIntItem( tr("Setup.LNB$Low LNB frequency (MHz)"),  &data.LnbFrequLo));
     Add(new cMenuEditIntItem( tr("Setup.LNB$High LNB frequency (MHz)"), &data.LnbFrequHi));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupLNB::ProcessKey(eKeys Key)
{
  int oldDiSEqC = data.DiSEqC;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (Key != kNone && data.DiSEqC != oldDiSEqC)
     Setup();
  return state;
}

// --- cMenuSetupCICAM -------------------------------------------------------

class cMenuSetupCICAM : public cMenuSetupBase {
private:
  int helpKeys;
  void SetHelpKeys(void);
  cCiHandler *GetCurrentCiHandler(int *Slot = NULL);
  eOSState Menu(void);
  eOSState Reset(void);
public:
  cMenuSetupCICAM(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupCICAM::cMenuSetupCICAM(void)
{
  helpKeys = -1;
  SetSection(tr("CICAM"));
  for (int d = 0; d < cDevice::NumDevices(); d++) {
      cDevice *Device = cDevice::GetDevice(d);
      cCiHandler *CiHandler = Device->CiHandler();
      for (int Slot = 0; Slot < 2; Slot++) {
          char buffer[32];
          int CardIndex = Device->CardIndex();
          const char *CamName = CiHandler ? CiHandler->GetCamName(Slot) : NULL;
          if (!CamName)
             CamName = "-";
          snprintf(buffer, sizeof(buffer), "%s%d %d\t%s", tr("Setup.CICAM$CICAM DVB"), CardIndex + 1, Slot + 1, CamName);
          Add(new cOsdItem(buffer));
          }
      }
  SetHelpKeys();
}

cCiHandler *cMenuSetupCICAM::GetCurrentCiHandler(int *Slot)
{
  cDevice *Device = cDevice::GetDevice(Current() / 2);
  if (Slot)
     *Slot = Current() % 2;
  return Device ? Device->CiHandler() : NULL;
}

void cMenuSetupCICAM::SetHelpKeys(void)
{
  int NewHelpKeys = helpKeys;
  NewHelpKeys = GetCurrentCiHandler() ? 1 : 0;
  if (NewHelpKeys != helpKeys) {
     switch (NewHelpKeys) {
       case 0: SetHelp(NULL); break;
       case 1: SetHelp(tr("Menu"), tr("Reset"));
       }
     helpKeys = NewHelpKeys;
     }
}

eOSState cMenuSetupCICAM::Menu(void)
{
  int Slot = 0;
  cCiHandler *CiHandler = GetCurrentCiHandler(&Slot);
  if (CiHandler && CiHandler->EnterMenu(Slot))
     return osEnd; // the CAM menu will be executed explicitly from the main loop
  else
     Skins.Message(mtError, tr("Can't open CAM menu!"));
  return osContinue;
}

eOSState cMenuSetupCICAM::Reset(void)
{
  int Slot = 0;
  cCiHandler *CiHandler = GetCurrentCiHandler(&Slot);
  if (CiHandler && CiHandler->Reset(Slot)) {
     Skins.Message(mtInfo, tr("CAM has been reset"));
     return osEnd;
     }
  else
     Skins.Message(mtError, tr("Can't reset CAM!"));
  return osContinue;
}

eOSState cMenuSetupCICAM::ProcessKey(eKeys Key)
{
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:    if (helpKeys == 1)
                        return Menu();
                     break;
       case kGreen:  if (helpKeys == 1)
                        return Reset();
                     break;
       default: break;
       }
     }
  if (Key != kNone)
     SetHelpKeys();
  return state;
}

// --- cMenuSetupRecord ------------------------------------------------------

class cMenuSetupRecord : public cMenuSetupBase {
public:
  cMenuSetupRecord(void);
  };

cMenuSetupRecord::cMenuSetupRecord(void)
{
  SetSection(tr("Recording"));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Margin at start (min)"),     &data.MarginStart));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Margin at stop (min)"),      &data.MarginStop));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Primary limit"),             &data.PrimaryLimit, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Default priority"),          &data.DefaultPriority, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Default lifetime (d)"),      &data.DefaultLifetime, 0, MAXLIFETIME));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Pause priority"),            &data.PausePriority, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Pause lifetime (d)"),        &data.PauseLifetime, 0, MAXLIFETIME));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Use episode name"),          &data.UseSubtitle));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Use VPS"),                   &data.UseVps));
  Add(new cMenuEditIntItem( tr("Setup.Recording$VPS margin (s)"),            &data.VpsMargin, 0));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Mark instant recording"),    &data.MarkInstantRecord));
  Add(new cMenuEditStrItem( tr("Setup.Recording$Name instant recording"),     data.NameInstantRecord, sizeof(data.NameInstantRecord), tr(FileNameChars)));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Instant rec. time (min)"),   &data.InstantRecordTime, 1, MAXINSTANTRECTIME));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Max. video file size (MB)"), &data.MaxVideoFileSize, MINVIDEOFILESIZE, MAXVIDEOFILESIZE));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Split edited files"),        &data.SplitEditedFiles));
}

// --- cMenuSetupReplay ------------------------------------------------------

class cMenuSetupReplay : public cMenuSetupBase {
public:
  cMenuSetupReplay(void);
  };

cMenuSetupReplay::cMenuSetupReplay(void)
{
  SetSection(tr("Replay"));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Multi speed mode"), &data.MultiSpeedMode));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Show replay mode"), &data.ShowReplayMode));
  Add(new cMenuEditIntItem(tr("Setup.Replay$Resume ID"), &data.ResumeID, 0, 99));
}

// --- cMenuSetupMisc --------------------------------------------------------

class cMenuSetupMisc : public cMenuSetupBase {
public:
  cMenuSetupMisc(void);
  };

cMenuSetupMisc::cMenuSetupMisc(void)
{
  SetSection(tr("Miscellaneous"));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. event timeout (min)"),   &data.MinEventTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. user inactivity (min)"), &data.MinUserInactivity));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$SVDRP timeout (s)"),          &data.SVDRPTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Zap timeout (s)"),            &data.ZapTimeout));
}

// --- cMenuSetupPluginItem --------------------------------------------------

class cMenuSetupPluginItem : public cOsdItem {
private:
  int pluginIndex;
public:
  cMenuSetupPluginItem(const char *Name, int Index);
  int PluginIndex(void) { return pluginIndex; }
  };

cMenuSetupPluginItem::cMenuSetupPluginItem(const char *Name, int Index)
:cOsdItem(Name)
{
  pluginIndex = Index;
}

// --- cMenuSetupPlugins -----------------------------------------------------

class cMenuSetupPlugins : public cMenuSetupBase {
public:
  cMenuSetupPlugins(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupPlugins::cMenuSetupPlugins(void)
{
  SetSection(tr("Plugins"));
  SetHasHotkeys();
  for (int i = 0; ; i++) {
      cPlugin *p = cPluginManager::GetPlugin(i);
      if (p) {
         char *buffer = NULL;
         asprintf(&buffer, "%s (%s) - %s", p->Name(), p->Version(), p->Description());
         Add(new cMenuSetupPluginItem(hk(buffer), i));
         free(buffer);
         }
      else
         break;
      }
}

eOSState cMenuSetupPlugins::ProcessKey(eKeys Key)
{
  eOSState state = HasSubMenu() ? cMenuSetupBase::ProcessKey(Key) : cOsdMenu::ProcessKey(Key);

  if (Key == kOk) {
     if (state == osUnknown) {
        cMenuSetupPluginItem *item = (cMenuSetupPluginItem *)Get(Current());
        if (item) {
           cPlugin *p = cPluginManager::GetPlugin(item->PluginIndex());
           if (p) {
              cMenuSetupPage *menu = p->SetupMenu();
              if (menu) {
                 menu->SetPlugin(p);
                 return AddSubMenu(menu);
                 }
              Skins.Message(mtInfo, tr("This plugin has no setup parameters!"));
              }
           }
        }
     else if (state == osContinue)
        Store();
     }
  return state;
}

// --- cMenuSetup ------------------------------------------------------------

class cMenuSetup : public cOsdMenu {
private:
  virtual void Set(void);
  eOSState Restart(void);
public:
  cMenuSetup(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetup::cMenuSetup(void)
:cOsdMenu("")
{
  Set();
}

void cMenuSetup::Set(void)
{
  Clear();
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%s - VDR %s", tr("Setup"), VDRVERSION);
  SetTitle(buffer);
  SetHasHotkeys();
  Add(new cOsdItem(hk(tr("OSD")),           osUser1));
  Add(new cOsdItem(hk(tr("EPG")),           osUser2));
  Add(new cOsdItem(hk(tr("DVB")),           osUser3));
  Add(new cOsdItem(hk(tr("LNB")),           osUser4));
  Add(new cOsdItem(hk(tr("CICAM")),         osUser5));
  Add(new cOsdItem(hk(tr("Recording")),     osUser6));
  Add(new cOsdItem(hk(tr("Replay")),        osUser7));
  Add(new cOsdItem(hk(tr("Miscellaneous")), osUser8));
  if (cPluginManager::HasPlugins())
  Add(new cOsdItem(hk(tr("Plugins")),       osUser9));
  Add(new cOsdItem(hk(tr("Restart")),       osUser10));
}

eOSState cMenuSetup::Restart(void)
{
  if (Interface->Confirm(cRecordControls::Active() ? tr("Recording - restart anyway?") : tr("Really restart?"))) {
     cThread::EmergencyExit(true);
     return osEnd;
     }
  return osContinue;
}

eOSState cMenuSetup::ProcessKey(eKeys Key)
{
  int osdLanguage = Setup.OSDLanguage;
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osUser1: return AddSubMenu(new cMenuSetupOSD);
    case osUser2: return AddSubMenu(new cMenuSetupEPG);
    case osUser3: return AddSubMenu(new cMenuSetupDVB);
    case osUser4: return AddSubMenu(new cMenuSetupLNB);
    case osUser5: return AddSubMenu(new cMenuSetupCICAM);
    case osUser6: return AddSubMenu(new cMenuSetupRecord);
    case osUser7: return AddSubMenu(new cMenuSetupReplay);
    case osUser8: return AddSubMenu(new cMenuSetupMisc);
    case osUser9: return AddSubMenu(new cMenuSetupPlugins);
    case osUser10: return Restart();
    default: ;
    }
  if (Setup.OSDLanguage != osdLanguage) {
     Set();
     if (!HasSubMenu())
        Display();
     }
  return state;
}

// --- cMenuPluginItem -------------------------------------------------------

class cMenuPluginItem : public cOsdItem {
private:
  int pluginIndex;
public:
  cMenuPluginItem(const char *Name, int Index);
  int PluginIndex(void) { return pluginIndex; }
  };

cMenuPluginItem::cMenuPluginItem(const char *Name, int Index)
:cOsdItem(Name, osPlugin)
{
  pluginIndex = Index;
}

// --- cMenuMain -------------------------------------------------------------

#define STOP_RECORDING tr(" Stop recording ")
#define ON_PRIMARY_INTERFACE tr("on primary interface")

cOsdObject *cMenuMain::pluginOsdObject = NULL;

cMenuMain::cMenuMain(bool Replaying, eOSState State, const char *Plugin)
:cOsdMenu("")
{
  replaying = Replaying;
  Set(Plugin);

  // Initial submenus:

  switch (State) {
    case osSchedule:   AddSubMenu(new cMenuSchedule); break;
    case osChannels:   AddSubMenu(new cMenuChannels); break;
    case osTimers:     AddSubMenu(new cMenuTimers); break;
    case osRecordings: AddSubMenu(new cMenuRecordings(NULL, 0, true)); break;
    case osSetup:      AddSubMenu(new cMenuSetup); break;
    case osCommands:   AddSubMenu(new cMenuCommands(tr("Commands"), &Commands)); break;
    case osPlugin:     break; // the actual work is done in Set()
    default: break;
    }
}

cOsdObject *cMenuMain::PluginOsdObject(void)
{
  cOsdObject *o = pluginOsdObject;
  pluginOsdObject = NULL;
  return o;
}

void cMenuMain::Set(const char *Plugin)
{
  Clear();
  //XXX //SetTitle("VDR"); // this is done below, including disk usage
  SetHasHotkeys();

  // Title with disk usage:

#define MB_PER_MINUTE 25.75 // this is just an estimate!

  char buffer[40];
  int FreeMB;
  int Percent = VideoDiskSpace(&FreeMB);
  int Minutes = int(double(FreeMB) / MB_PER_MINUTE);
  int Hours = Minutes / 60;
  Minutes %= 60;
  snprintf(buffer, sizeof(buffer), "%s  -  %s %d%%  -  %2d:%02d %s", tr("VDR"), tr("Disk"), Percent, Hours, Minutes, tr("free"));
  //XXX -> skin function!!!
  SetTitle(buffer);

  // Basic menu items:

  Add(new cOsdItem(hk(tr("Schedule")),   osSchedule));
  Add(new cOsdItem(hk(tr("Channels")),   osChannels));
  Add(new cOsdItem(hk(tr("Timers")),     osTimers));
  Add(new cOsdItem(hk(tr("Recordings")), osRecordings));

  // Plugins:

  for (int i = 0; ; i++) {
      cPlugin *p = cPluginManager::GetPlugin(i);
      if (p) {
         const char *item = p->MainMenuEntry();
         if (item)
            Add(new cMenuPluginItem(hk(item), i), Plugin && strcmp(Plugin, p->Name()) == 0);
         }
      else
         break;
      }

  // More basic menu items:

  Add(new cOsdItem(hk(tr("Setup")),      osSetup));
  if (Commands.Count())
     Add(new cOsdItem(hk(tr("Commands")),  osCommands));

  // Replay control:

  if (replaying)
     Add(new cOsdItem(tr(" Stop replaying"), osStopReplay));

  // Record control:

  if (cRecordControls::StopPrimary()) {
     char *buffer = NULL;
     asprintf(&buffer, "%s%s", STOP_RECORDING, ON_PRIMARY_INTERFACE);
     Add(new cOsdItem(buffer, osStopRecord));
     free(buffer);
     }

  const char *s = NULL;
  while ((s = cRecordControls::GetInstantId(s)) != NULL) {
        char *buffer = NULL;
        asprintf(&buffer, "%s%s", STOP_RECORDING, s);
        Add(new cOsdItem(buffer, osStopRecord));
        free(buffer);
        }

  // Editing control:

  if (cCutter::Active())
     Add(new cOsdItem(tr(" Cancel editing"), osCancelEdit));

  // Color buttons:

  SetHelp(!replaying ? tr("Record") : NULL, tr("Audio"), replaying ? NULL : tr("Pause"), replaying ? tr("Button$Stop") : cReplayControl::LastReplayed() ? tr("Resume") : NULL);
  Display();
  lastActivity = time(NULL);
}

eOSState cMenuMain::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
  int osdLanguage = Setup.OSDLanguage;
  eOSState state = cOsdMenu::ProcessKey(Key);
  HadSubMenu |= HasSubMenu();

  switch (state) {
    case osSchedule:   return AddSubMenu(new cMenuSchedule);
    case osChannels:   return AddSubMenu(new cMenuChannels);
    case osTimers:     return AddSubMenu(new cMenuTimers);
    case osRecordings: return AddSubMenu(new cMenuRecordings);
    case osSetup:      return AddSubMenu(new cMenuSetup);
    case osCommands:   return AddSubMenu(new cMenuCommands(tr("Commands"), &Commands));
    case osStopRecord: if (Interface->Confirm(tr("Stop recording?"))) {
                          cOsdItem *item = Get(Current());
                          if (item) {
                             const char *s = item->Text() + strlen(STOP_RECORDING);
                             if (strcmp(s, ON_PRIMARY_INTERFACE) == 0)
                                cRecordControls::StopPrimary(true);
                             else
                                cRecordControls::Stop(item->Text() + strlen(STOP_RECORDING));
                             return osEnd;
                             }
                          }
                       break;
    case osCancelEdit: if (Interface->Confirm(tr("Cancel editing?"))) {
                          cCutter::Stop();
                          return osEnd;
                          }
                       break;
    case osPlugin:     {
                         cMenuPluginItem *item = (cMenuPluginItem *)Get(Current());
                         if (item) {
                            cPlugin *p = cPluginManager::GetPlugin(item->PluginIndex());
                            if (p) {
                               cOsdObject *menu = p->MainMenuAction();
                               if (menu) {
                                  if (menu->IsMenu())
                                     return AddSubMenu((cOsdMenu *)menu);
                                  else {
                                     pluginOsdObject = menu;
                                     return osPlugin;
                                     }
                                  }
                               }
                            }
                         state = osEnd;
                       }
                       break;
    default: switch (Key) {
               case kRecord:
               case kRed:    if (!HadSubMenu)
                                state = replaying ? osContinue : osRecord;
                             break;
               case kGreen:  if (!HadSubMenu) {
                                cRemote::Put(kAudio, true);
                                state = osEnd;
                                }
                             break;
               case kYellow: if (!HadSubMenu)
                                state = replaying ? osContinue : osPause;
                             break;
               case kBlue:   if (!HadSubMenu)
                                state = replaying ? osStopReplay : cReplayControl::LastReplayed() ? osReplay : osContinue;
                             break;
               default:      break;
               }
    }
  if (Key != kNone) {
     lastActivity = time(NULL);
     if (Setup.OSDLanguage != osdLanguage) {
        Set();
        if (!HasSubMenu())
           Display();
        }
     }
  else if (time(NULL) - lastActivity > MENUTIMEOUT)
     state = osEnd;
  return state;
}

// --- SetTrackDescriptions --------------------------------------------------

static void SetTrackDescriptions(bool Live)
{
  cDevice::PrimaryDevice()->ClrAvailableTracks(true);
  const cComponents *Components = NULL;
  cSchedulesLock SchedulesLock;
  if (Live) {
     cChannel *Channel = Channels.GetByNumber(cDevice::CurrentChannel());
     if (Channel) {
        const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
        if (Schedules) {
           const cSchedule *Schedule = Schedules->GetSchedule(Channel->GetChannelID());
           if (Schedule) {
              const cEvent *Present = Schedule->GetPresentEvent(true);
              if (Present)
                 Components = Present->Components();
              }
           }
        }
     }
  else if (cReplayControl::LastReplayed()) {
     cRecording *Recording = Recordings.GetByName(cReplayControl::LastReplayed());
     if (Recording)
        Components = Recording->Info()->Components();
     }
  if (Components) {
     int indexAudio = 0;
     int indexDolby = 0;
     for (int i = 0; i < Components->NumComponents(); i++) {
         const tComponent *p = Components->Component(i);
         if (p->stream == 2) {
            if (p->type == 0x05)
               cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, indexDolby++, 0, NULL, p->description);
            else
               cDevice::PrimaryDevice()->SetAvailableTrack(ttAudio, indexAudio++, 0, NULL, p->description);
            }
         }
     }
}

// --- cDisplayChannel -------------------------------------------------------

#define DIRECTCHANNELTIMEOUT 1000 //ms

cDisplayChannel::cDisplayChannel(int Number, bool Switched)
:cOsdObject(true)
{
  group = -1;
  withInfo = !Switched || Setup.ShowInfoOnChSwitch;
  displayChannel = Skins.Current()->DisplayChannel(withInfo);
  number = 0;
  channel = Channels.GetByNumber(Number);
  lastPresent = lastFollowing = NULL;
  if (channel) {
     DisplayChannel();
     DisplayInfo();
     displayChannel->Flush();
     }
  lastTime.Set();
}

cDisplayChannel::cDisplayChannel(eKeys FirstKey)
:cOsdObject(true)
{
  group = -1;
  number = 0;
  lastPresent = lastFollowing = NULL;
  lastTime.Set();
  withInfo = Setup.ShowInfoOnChSwitch;
  displayChannel = Skins.Current()->DisplayChannel(withInfo);
  ProcessKey(FirstKey);
}

cDisplayChannel::~cDisplayChannel()
{
  delete displayChannel;
  cStatus::MsgOsdClear();
}

void cDisplayChannel::DisplayChannel(void)
{
  displayChannel->SetChannel(channel, number);
  cStatus::MsgOsdChannel(ChannelString(channel, number));
  lastPresent = lastFollowing = NULL;
}

void cDisplayChannel::DisplayInfo(void)
{
  if (withInfo && channel) {
     cSchedulesLock SchedulesLock;
     const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
     if (Schedules) {
        const cSchedule *Schedule = Schedules->GetSchedule(channel->GetChannelID());
        if (Schedule) {
           const cEvent *Present = Schedule->GetPresentEvent(true);
           const cEvent *Following = Schedule->GetFollowingEvent(true);
           if (Present != lastPresent || Following != lastFollowing) {
              SetTrackDescriptions(true);
              displayChannel->SetEvents(Present, Following);
              cStatus::MsgOsdProgramme(Present ? Present->StartTime() : 0, Present ? Present->Title() : NULL, Present ? Present->ShortText() : NULL, Following ? Following->StartTime() : 0, Following ? Following->Title() : NULL, Following ? Following->ShortText() : NULL);
              lastPresent = Present;
              lastFollowing = Following;
              }
           }
        }
     }
}

void cDisplayChannel::Refresh(void)
{
  channel = Channels.GetByNumber(cDevice::CurrentChannel());
  DisplayChannel();
  displayChannel->SetEvents(NULL, NULL);
  lastTime.Set();
}

eOSState cDisplayChannel::ProcessKey(eKeys Key)
{
  switch (Key) {
    case k0:
         if (number == 0) {
            // keep the "Toggle channels" function working
            cRemote::Put(Key);
            return osEnd;
            }
    case k1 ... k9:
         if (number >= 0) {
            number = number * 10 + Key - k0;
            if (number > 0) {
               channel = Channels.GetByNumber(number);
               displayChannel->SetEvents(NULL, NULL);
               withInfo = false;
               DisplayChannel();
               lastTime.Set();
               // Lets see if there can be any useful further input:
               int n = channel ? number * 10 : 0;
               cChannel *ch = channel;
               while (ch && (ch = Channels.Next(ch)) != NULL) {
                     if (!ch->GroupSep()) {
                        if (n <= ch->Number() && ch->Number() <= n + 9) {
                           n = 0;
                           break;
                           }
                        if (ch->Number() > n)
                           n *= 10;
                        }
                     }
               if (n > 0) {
                  // This channel is the only one that fits the input, so let's take it right away:
                  displayChannel->Flush(); // makes sure the user sees his last input
                  Channels.SwitchTo(number);
                  return osEnd;
                  }
               }
            }
         break;
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
         withInfo = false;
         number = 0;
         if (group < 0) {
            cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
            if (channel)
               group = channel->Index();
            }
         if (group >= 0) {
            int SaveGroup = group;
            if (NORMALKEY(Key) == kRight)
               group = Channels.GetNextGroup(group) ;
            else
               group = Channels.GetPrevGroup(group < 1 ? 1 : group);
            if (group < 0)
               group = SaveGroup;
            channel = Channels.Get(group);
            if (channel) {
               displayChannel->SetEvents(NULL, NULL);
               DisplayChannel();
               if (!channel->GroupSep())
                  group = -1;
               }
            }
         lastTime.Set();
         break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
         cDevice::SwitchChannel(NORMALKEY(Key) == kUp ? 1 : -1);
         // no break here
    case kChanUp|k_Repeat:
    case kChanUp:
    case kChanDn|k_Repeat:
    case kChanDn:
         withInfo = true;
         group = -1;
         number = 0;
         Refresh();
         break;
    case kNone:
         if (number && lastTime.Elapsed() > DIRECTCHANNELTIMEOUT) {
            if (Channels.GetByNumber(number))
               Channels.SwitchTo(number);
            else {
               number = 0;
               channel = NULL;
               DisplayChannel();
               lastTime.Set();
               return osContinue;
               }
            return osEnd;
            }
         break;
    //TODO
    //XXX case kGreen:  return osEventNow;
    //XXX case kYellow: return osEventNext;
    case kOk:     if (group >= 0) {
                     channel = Channels.Get(Channels.GetNextNormal(group));
                     if (channel)
                        Channels.SwitchTo(channel->Number());
                     withInfo = true;
                     group = -1;
                     Refresh();
                     break;
                     }
                  else if (number > 0 && channel)
                     Channels.SwitchTo(number);
                  return osEnd;
    default:      if ((Key & (k_Repeat | k_Release)) == 0) {
                     cRemote::Put(Key);
                     return osEnd;
                     }
    };
  if (lastTime.Elapsed() < (uint64)(Setup.ChannelInfoTime * 1000)) {
     if (!number && group < 0 && channel && channel->Number() != cDevice::CurrentChannel())
        Refresh(); // makes sure a channel switch through the SVDRP CHAN command is displayed
     DisplayInfo();
     displayChannel->Flush();
     return osContinue;
     }
  return osEnd;
}

// --- cDisplayVolume --------------------------------------------------------

#define VOLUMETIMEOUT 1000 //ms
#define MUTETIMEOUT   5000 //ms

cDisplayVolume *cDisplayVolume::currentDisplayVolume = NULL;

cDisplayVolume::cDisplayVolume(void)
:cOsdObject(true)
{
  currentDisplayVolume = this;
  timeout.Set(cDevice::PrimaryDevice()->IsMute() ? MUTETIMEOUT : VOLUMETIMEOUT);
  displayVolume = Skins.Current()->DisplayVolume();
  Show();
}

cDisplayVolume::~cDisplayVolume()
{
  delete displayVolume;
  currentDisplayVolume = NULL;
}

void cDisplayVolume::Show(void)
{
  displayVolume->SetVolume(cDevice::CurrentVolume(), MAXVOLUME, cDevice::PrimaryDevice()->IsMute());
}

cDisplayVolume *cDisplayVolume::Create(void)
{
  if (!currentDisplayVolume)
     new cDisplayVolume;
  return currentDisplayVolume;
}

void cDisplayVolume::Process(eKeys Key)
{
  if (currentDisplayVolume)
     currentDisplayVolume->ProcessKey(Key);
}

eOSState cDisplayVolume::ProcessKey(eKeys Key)
{
  switch (Key) {
    case kVolUp|k_Repeat:
    case kVolUp:
    case kVolDn|k_Repeat:
    case kVolDn:
         Show();
         timeout.Set(VOLUMETIMEOUT);
         break;
    case kMute:
         if (cDevice::PrimaryDevice()->IsMute()) {
            Show();
            timeout.Set(MUTETIMEOUT);
            }
         else
            timeout.Set();
         break;
    case kNone: break;
    default: if ((Key & k_Release) == 0) {
                cRemote::Put(Key);
                return osEnd;
                }
    }
  return timeout.TimedOut() ? osEnd : osContinue;
}

// --- cDisplayTracks --------------------------------------------------------

#define TRACKTIMEOUT 5000 //ms

cDisplayTracks *cDisplayTracks::currentDisplayTracks = NULL;

cDisplayTracks::cDisplayTracks(void)
:cOsdObject(true)
{
  cDevice::PrimaryDevice()->EnsureAudioTrack();
  SetTrackDescriptions(!cDevice::PrimaryDevice()->Replaying() || cTransferControl::ReceiverDevice());
  currentDisplayTracks = this;
  numTracks = track = 0;
  audioChannel = cDevice::PrimaryDevice()->GetAudioChannel();
  eTrackType CurrentAudioTrack = cDevice::PrimaryDevice()->GetCurrentAudioTrack();
  for (int i = ttAudioFirst; i <= ttDolbyLast; i++) {
      const tTrackId *TrackId = cDevice::PrimaryDevice()->GetTrack(eTrackType(i));
      if (TrackId && TrackId->id) {
         types[numTracks] = eTrackType(i);
         descriptions[numTracks] = strdup(*TrackId->description ? TrackId->description : *TrackId->language ? TrackId->language : *itoa(i));
         if (i == CurrentAudioTrack)
            track = numTracks;
         numTracks++;
         }
      }
  timeout.Set(TRACKTIMEOUT);
  displayTracks = Skins.Current()->DisplayTracks(tr("Audio"), numTracks, descriptions);
  Show();
}

cDisplayTracks::~cDisplayTracks()
{
  delete displayTracks;
  currentDisplayTracks = NULL;
  for (int i = 0; i < numTracks; i++)
      free(descriptions[i]);
  cStatus::MsgOsdClear();
}

void cDisplayTracks::Show(void)
{
  int ac = IS_AUDIO_TRACK(types[track]) ? audioChannel : -1;
  displayTracks->SetTrack(track, descriptions);
  displayTracks->SetAudioChannel(ac);
  displayTracks->Flush();
  cStatus::MsgSetAudioTrack(track, descriptions);
  cStatus::MsgSetAudioChannel(ac);
}

cDisplayTracks *cDisplayTracks::Create(void)
{
  if (cDevice::PrimaryDevice()->NumAudioTracks() > 0) {
     if (!currentDisplayTracks)
        new cDisplayTracks;
     return currentDisplayTracks;
     }
  Skins.Message(mtWarning, tr("No audio available!"));
  return NULL;
}

void cDisplayTracks::Process(eKeys Key)
{
  if (currentDisplayTracks)
     currentDisplayTracks->ProcessKey(Key);
}

eOSState cDisplayTracks::ProcessKey(eKeys Key)
{
  int oldTrack = track;
  int oldAudioChannel = audioChannel;
  switch (Key) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
         if (NORMALKEY(Key) == kUp && track > 0)
            track--;
         else if (NORMALKEY(Key) == kDown && track < numTracks - 1)
            track++;
         timeout.Set(TRACKTIMEOUT);
         break;
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight: if (IS_AUDIO_TRACK(types[track])) {
                    static int ac[] = { 1, 0, 2 };
                    audioChannel = ac[cDevice::PrimaryDevice()->GetAudioChannel()];
                    if (NORMALKEY(Key) == kLeft && audioChannel > 0)
                       audioChannel--;
                    else if (NORMALKEY(Key) == kRight && audioChannel < 2)
                       audioChannel++;
                    audioChannel = ac[audioChannel];
                    timeout.Set(TRACKTIMEOUT);
                    }
         break;
    case kAudio|k_Repeat:
    case kAudio:
         if (++track >= numTracks)
            track = 0;
         timeout.Set(TRACKTIMEOUT);
         break;
    case kOk:
         if (track != cDevice::PrimaryDevice()->GetCurrentAudioTrack())
            oldTrack = -1; // make sure we explicitly switch to that track
         timeout.Set();
         break;
    case kNone: break;
    default: if ((Key & k_Release) == 0)
                return osEnd;
    }
  if (track != oldTrack || audioChannel != oldAudioChannel)
     Show();
  if (track != oldTrack) {
     cDevice::PrimaryDevice()->SetCurrentAudioTrack(types[track]);
     Setup.CurrentDolby = IS_DOLBY_TRACK(types[track]);
     }
  if (audioChannel != oldAudioChannel)
     cDevice::PrimaryDevice()->SetAudioChannel(audioChannel);
  return timeout.TimedOut() ? osEnd : osContinue;
}

// --- cRecordControl --------------------------------------------------------

cRecordControl::cRecordControl(cDevice *Device, cTimer *Timer, bool Pause)
{
  // We're going to manipulate an event here, so we need to prevent
  // others from modifying any EPG data:
  cSchedulesLock SchedulesLock;
  cSchedules::Schedules(SchedulesLock);

  event = NULL;
  instantId = NULL;
  fileName = NULL;
  recorder = NULL;
  device = Device;
  if (!device) device = cDevice::PrimaryDevice();//XXX
  timer = Timer;
  if (!timer) {
     timer = new cTimer(true, Pause);
     Timers.Add(timer);
     Timers.SetModified();
     asprintf(&instantId, cDevice::NumDevices() > 1 ? "%s - %d" : "%s", timer->Channel()->Name(), device->CardIndex() + 1);
     }
  timer->SetPending(true);
  timer->SetRecording(true);
  event = timer->Event();

  if (event || GetEvent())
     dsyslog("Title: '%s' Subtitle: '%s'", event->Title(), event->ShortText());
  cRecording Recording(timer, event);
  fileName = strdup(Recording.FileName());

  // crude attempt to avoid duplicate recordings:
  if (cRecordControls::GetRecordControl(fileName)) {
     isyslog("already recording: '%s'", fileName);
     if (Timer) {
        timer->SetPending(false);
        timer->SetRecording(false);
        timer->OnOff();
        }
     else {
        Timers.Del(timer);
        Timers.SetModified();
        if (!cReplayControl::LastReplayed()) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName, Recording.Name());
        }
     timer = NULL;
     return;
     }

  cRecordingUserCommand::InvokeCommand(RUC_BEFORERECORDING, fileName);
  isyslog("record %s", fileName);
  if (MakeDirs(fileName, true)) {
     const cChannel *ch = timer->Channel();
     recorder = new cRecorder(fileName, ch->Ca(), timer->Priority(), ch->Vpid(), ch->Apids(), ch->Dpids(), ch->Spids());
     if (device->AttachReceiver(recorder)) {
        Recording.WriteInfo();
        cStatus::MsgRecording(device, Recording.Name());
        if (!Timer && !cReplayControl::LastReplayed()) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName, Recording.Name());
        Recordings.AddByName(fileName);
        return;
        }
     else
        DELETENULL(recorder);
     }
  if (!Timer) {
     Timers.Del(timer);
     Timers.SetModified();
     timer = NULL;
     }
}

cRecordControl::~cRecordControl()
{
  Stop();
  free(instantId);
  free(fileName);
}

#define INSTANT_REC_EPG_LOOKAHEAD 300 // seconds to look into the EPG data for an instant recording

bool cRecordControl::GetEvent(void)
{
  const cChannel *channel = timer->Channel();
  time_t Time = timer->HasFlags(tfInstant) ? timer->StartTime() + INSTANT_REC_EPG_LOOKAHEAD : timer->StartTime() + (timer->StopTime() - timer->StartTime()) / 2;
  for (int seconds = 0; seconds <= MAXWAIT4EPGINFO; seconds++) {
      {
        cSchedulesLock SchedulesLock;
        const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
        if (Schedules) {
           const cSchedule *Schedule = Schedules->GetSchedule(channel->GetChannelID());
           if (Schedule) {
              event = Schedule->GetEventAround(Time);
              if (event) {
                 if (seconds > 0)
                    dsyslog("got EPG info after %d seconds", seconds);
                 return true;
                 }
              }
           }
      }
      if (seconds == 0)
         dsyslog("waiting for EPG info...");
      sleep(1);
      }
  dsyslog("no EPG info available");
  return false;
}

void cRecordControl::Stop(void)
{
  if (timer) {
     DELETENULL(recorder);
     timer->SetRecording(false);
     timer = NULL;
     cStatus::MsgRecording(device, NULL);
     cRecordingUserCommand::InvokeCommand(RUC_AFTERRECORDING, fileName);
     }
}

bool cRecordControl::Process(time_t t)
{
  if (!recorder || !timer || !timer->Matches(t))
     return false;
  AssertFreeDiskSpace(timer->Priority());
  return true;
}

// --- cRecordControls -------------------------------------------------------

cRecordControl *cRecordControls::RecordControls[MAXRECORDCONTROLS] = { NULL };

bool cRecordControls::Start(cTimer *Timer, bool Pause)
{
  int ch = Timer ? Timer->Channel()->Number() : cDevice::CurrentChannel();
  cChannel *channel = Channels.GetByNumber(ch);

  if (channel) {
     bool NeedsDetachReceivers = false;
     int Priority = Timer ? Timer->Priority() : Pause ? Setup.PausePriority : Setup.DefaultPriority;
     cDevice *device = cDevice::GetDevice(channel, Priority, &NeedsDetachReceivers);
     if (device) {
        if (NeedsDetachReceivers) {
           Stop(device);
           if (device == cTransferControl::ReceiverDevice())
              cControl::Shutdown(); // in case this device was used for Transfer Mode
           }
        dsyslog("switching device %d to channel %d", device->DeviceNumber() + 1, channel->Number());
        if (!device->SwitchChannel(channel, false)) {
           cThread::EmergencyExit(true);
           return false;
           }
        if (!Timer || Timer->Matches()) {
           for (int i = 0; i < MAXRECORDCONTROLS; i++) {
               if (!RecordControls[i]) {
                  RecordControls[i] = new cRecordControl(device, Timer, Pause);
                  return RecordControls[i]->Process(time(NULL));
                  }
               }
           }
        }
     else if (!Timer || (Timer->Priority() >= Setup.PrimaryLimit && !Timer->Pending()))
        isyslog("no free DVB device to record channel %d!", ch);
     }
  else
     esyslog("ERROR: channel %d not defined!", ch);
  return false;
}

void cRecordControls::Stop(const char *InstantId)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         const char *id = RecordControls[i]->InstantId();
         if (id && strcmp(id, InstantId) == 0) {
            cTimer *timer = RecordControls[i]->Timer();
            RecordControls[i]->Stop();
            if (timer) {
               isyslog("deleting timer %s", *timer->ToDescr());
               Timers.Del(timer);
               Timers.SetModified();
               }
            break;
            }
         }
      }
}

void cRecordControls::Stop(cDevice *Device)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (RecordControls[i]->Device() == Device) {
            isyslog("stopping recording on DVB device %d due to higher priority", Device->CardIndex() + 1);
            RecordControls[i]->Stop();
            }
         }
      }
}

bool cRecordControls::StopPrimary(bool DoIt)
{
  if (cDevice::PrimaryDevice()->Receiving()) {
     //XXX+ disabled for the moment - might become obsolete with DVB_DRIVER_VERSION >= 2002090101
     cDevice *device = NULL;//XXX cDevice::GetDevice(cDevice::PrimaryDevice()->Ca(), 0);
     if (device) {
        if (DoIt)
           Stop(cDevice::PrimaryDevice());
        return true;
        }
     }
  return false;
}

bool cRecordControls::PauseLiveVideo(void)
{
  Skins.Message(mtStatus, tr("Pausing live video..."));
  cReplayControl::SetRecording(NULL, NULL); // make sure the new cRecordControl will set cReplayControl::LastReplayed()
  if (Start(NULL, true)) {
     sleep(2); // allow recorded file to fill up enough to start replaying
     cReplayControl *rc = new cReplayControl;
     cControl::Launch(rc);
     cControl::Attach();
     sleep(1); // allow device to replay some frames, so we have a picture
     Skins.Message(mtStatus, NULL);
     rc->ProcessKey(kPause); // pause, allowing replay mode display
     return true;
     }
  Skins.Message(mtStatus, NULL);
  return false;
}

const char *cRecordControls::GetInstantId(const char *LastInstantId)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (!LastInstantId && RecordControls[i]->InstantId())
            return RecordControls[i]->InstantId();
         if (LastInstantId && LastInstantId == RecordControls[i]->InstantId())
            LastInstantId = NULL;
         }
      }
  return NULL;
}

cRecordControl *cRecordControls::GetRecordControl(const char *FileName)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i] && strcmp(RecordControls[i]->FileName(), FileName) == 0)
         return RecordControls[i];
      }
  return NULL;
}

void cRecordControls::Process(time_t t)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (!RecordControls[i]->Process(t))
            DELETENULL(RecordControls[i]);
         }
      }
}

void cRecordControls::ChannelDataModified(cChannel *Channel)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (RecordControls[i]->Timer() && RecordControls[i]->Timer()->Channel() == Channel) {
            if (RecordControls[i]->Device()->ProvidesTransponder(Channel)) { // avoids retune on devices that don't really access the transponder
               isyslog("stopping recording due to modification of channel %d", Channel->Number());
               RecordControls[i]->Stop();
               // This will restart the recording, maybe even from a different
               // device in case conditional access has changed.
               }
            }
         }
      }
}

bool cRecordControls::Active(void)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i])
         return true;
      }
  return false;
}

void cRecordControls::Shutdown(void)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++)
      DELETENULL(RecordControls[i]);
}

// --- cReplayControl --------------------------------------------------------

char *cReplayControl::fileName = NULL;
char *cReplayControl::title = NULL;

cReplayControl::cReplayControl(void)
:cDvbPlayerControl(fileName)
{
  displayReplay = NULL;
  visible = modeOnly = shown = displayFrames = false;
  lastCurrent = lastTotal = -1;
  lastPlay = lastForward = false;
  lastSpeed = -1;
  timeoutShow = 0;
  timeSearchActive = false;
  marks.Load(fileName);
  cRecording Recording(fileName);
  cStatus::MsgReplaying(this, Recording.Name());
}

cReplayControl::~cReplayControl()
{
  Hide();
  cStatus::MsgReplaying(this, NULL);
  Stop();
}

void cReplayControl::SetRecording(const char *FileName, const char *Title)
{
  free(fileName);
  free(title);
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
     free(fileName);
     fileName = NULL;
     }
}

void cReplayControl::ShowTimed(int Seconds)
{
  if (modeOnly)
     Hide();
  if (!visible) {
     shown = ShowProgress(true);
     timeoutShow = (shown && Seconds > 0) ? time(NULL) + Seconds : 0;
     }
}

void cReplayControl::Show(void)
{
  ShowTimed();
}

void cReplayControl::Hide(void)
{
  if (visible) {
     delete displayReplay;
     displayReplay = NULL;
     needsFastResponse = visible = false;
     modeOnly = false;
     lastPlay = lastForward = false;
     lastSpeed = -1;
     timeSearchActive = false;
     }
}

void cReplayControl::ShowMode(void)
{
  if (visible || Setup.ShowReplayMode && !cOsd::IsOpen()) {
     bool Play, Forward;
     int Speed;
     if (GetReplayMode(Play, Forward, Speed) && (!visible || Play != lastPlay || Forward != lastForward || Speed != lastSpeed)) {
        bool NormalPlay = (Play && Speed == -1);

        if (!visible) {
           if (NormalPlay)
              return; // no need to do indicate ">" unless there was a different mode displayed before
           visible = modeOnly = true;
           displayReplay = Skins.Current()->DisplayReplay(modeOnly);
           }

        if (modeOnly && !timeoutShow && NormalPlay)
           timeoutShow = time(NULL) + MODETIMEOUT;
        displayReplay->SetMode(Play, Forward, Speed);
        lastPlay = Play;
        lastForward = Forward;
        lastSpeed = Speed;
        }
     }
}

bool cReplayControl::ShowProgress(bool Initial)
{
  int Current, Total;

  if (GetIndex(Current, Total) && Total > 0) {
     if (!visible) {
        displayReplay = Skins.Current()->DisplayReplay(modeOnly);
        displayReplay->SetMarks(&marks);
        needsFastResponse = visible = true;
        }
     if (Initial) {
        if (title)
           displayReplay->SetTitle(title);
        lastCurrent = lastTotal = -1;
        }
     if (Total != lastTotal) {
        displayReplay->SetTotal(IndexToHMSF(Total));
        if (!Initial)
           displayReplay->Flush();
        }
     if (Current != lastCurrent || Total != lastTotal) {
        displayReplay->SetProgress(Current, Total);
        if (!Initial)
           displayReplay->Flush();
        displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames));
        displayReplay->Flush();
        lastCurrent = Current;
        }
     lastTotal = Total;
     ShowMode();
     return true;
     }
  return false;
}

void cReplayControl::TimeSearchDisplay(void)
{
  char buf[64];
  strcpy(buf, tr("Jump: "));
  int len = strlen(buf);
  char h10 = '0' + (timeSearchTime >> 24);
  char h1  = '0' + ((timeSearchTime & 0x00FF0000) >> 16);
  char m10 = '0' + ((timeSearchTime & 0x0000FF00) >> 8);
  char m1  = '0' + (timeSearchTime & 0x000000FF);
  char ch10 = timeSearchPos > 3 ? h10 : '-';
  char ch1  = timeSearchPos > 2 ? h1  : '-';
  char cm10 = timeSearchPos > 1 ? m10 : '-';
  char cm1  = timeSearchPos > 0 ? m1  : '-';
  sprintf(buf + len, "%c%c:%c%c", ch10, ch1, cm10, cm1);
  displayReplay->SetJump(buf);
}

void cReplayControl::TimeSearchProcess(eKeys Key)
{
#define STAY_SECONDS_OFF_END 10
  int Seconds = (timeSearchTime >> 24) * 36000 + ((timeSearchTime & 0x00FF0000) >> 16) * 3600 + ((timeSearchTime & 0x0000FF00) >> 8) * 600 + (timeSearchTime & 0x000000FF) * 60;
  int Current = (lastCurrent / FRAMESPERSEC);
  int Total = (lastTotal / FRAMESPERSEC);
  switch (Key) {
    case k0 ... k9:
         if (timeSearchPos < 4) {
            timeSearchTime <<= 8;
            timeSearchTime |= Key - k0;
            timeSearchPos++;
            TimeSearchDisplay();
            }
         break;
    case kFastRew:
    case kLeft:
    case kFastFwd:
    case kRight: {
         int dir = ((Key == kRight || Key == kFastFwd) ? 1 : -1);
         if (dir > 0)
            Seconds = min(Total - Current - STAY_SECONDS_OFF_END, Seconds);
         SkipSeconds(Seconds * dir);
         timeSearchActive = false;
         }
         break;
    case kPlay:
    case kUp:
    case kPause:
    case kDown:
         Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
         Goto(Seconds * FRAMESPERSEC, Key == kDown || Key == kPause);
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
        displayReplay->SetJump(NULL);
     ShowMode();
     }
}

void cReplayControl::TimeSearch(void)
{
  timeSearchTime = timeSearchPos = 0;
  timeSearchHide = false;
  if (modeOnly)
     Hide();
  if (!visible) {
     Show();
     if (visible)
        timeSearchHide = true;
     else
        return;
     }
  timeoutShow = 0;
  TimeSearchDisplay();
  timeSearchActive = true;
}

void cReplayControl::MarkToggle(void)
{
  int Current, Total;
  if (GetIndex(Current, Total, true)) {
     cMark *m = marks.Get(Current);
     lastCurrent = -1; // triggers redisplay
     if (m)
        marks.Del(m);
     else {
        marks.Add(Current);
        ShowTimed(2);
        bool Play, Forward;
        int Speed;
        if (GetReplayMode(Play, Forward, Speed) && !Play)
           Goto(Current, true);
        }
     marks.Save();
     }
}

void cReplayControl::MarkJump(bool Forward)
{
  if (marks.Count()) {
     int Current, Total;
     if (GetIndex(Current, Total)) {
        cMark *m = Forward ? marks.GetNext(Current) : marks.GetPrev(Current);
        if (m) {
           Goto(m->position, true);
           displayFrames = true;
           }
        }
     }
}

void cReplayControl::MarkMove(bool Forward)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     cMark *m = marks.Get(Current);
     if (m) {
        displayFrames = true;
        int p = SkipFrames(Forward ? 1 : -1);
        cMark *m2;
        if (Forward) {
           if ((m2 = marks.Next(m)) != NULL && m2->position <= p)
              return;
           }
        else {
           if ((m2 = marks.Prev(m)) != NULL && m2->position >= p)
              return;
           }
        Goto(m->position = p, true);
        marks.Save();
        }
     }
}

void cReplayControl::EditCut(void)
{
  if (fileName) {
     Hide();
     if (!cCutter::Active()) {
        if (!marks.Count())
           Skins.Message(mtError, tr("No editing marks defined!"));
        else if (!cCutter::Start(fileName))
           Skins.Message(mtError, tr("Can't start editing process!"));
        else
           Skins.Message(mtInfo, tr("Editing process started"));
        }
     else
        Skins.Message(mtError, tr("Editing process already active!"));
     ShowMode();
     }
}

void cReplayControl::EditTest(void)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     cMark *m = marks.Get(Current);
     if (!m)
        m = marks.GetNext(Current);
     if (m) {
        if ((m->Index() & 0x01) != 0)
           m = marks.Next(m);
        if (m) {
           Goto(m->position - SecondsToFrames(3));
           Play();
           }
        }
     }
}

eOSState cReplayControl::ProcessKey(eKeys Key)
{
  if (!Active())
     return osEnd;
  if (visible) {
     if (timeoutShow && time(NULL) > timeoutShow) {
        Hide();
        ShowMode();
        timeoutShow = 0;
        }
     else if (modeOnly)
        ShowMode();
     else
        shown = ShowProgress(!shown) || shown;
     }
  bool DisplayedFrames = displayFrames;
  displayFrames = false;
  if (timeSearchActive && Key != kNone) {
     TimeSearchProcess(Key);
     return osContinue;
     }
  bool DoShowMode = true;
  switch (Key) {
    // Positioning:
    case kPlay:
    case kUp:      Play(); break;
    case kPause:
    case kDown:    Pause(); break;
    case kFastRew|k_Release:
    case kLeft|k_Release:
                   if (Setup.MultiSpeedMode) break;
    case kFastRew:
    case kLeft:    Backward(); break;
    case kFastFwd|k_Release:
    case kRight|k_Release:
                   if (Setup.MultiSpeedMode) break;
    case kFastFwd:
    case kRight:   Forward(); break;
    case kRed:     TimeSearch(); break;
    case kGreen|k_Repeat:
    case kGreen:   SkipSeconds(-60); break;
    case kYellow|k_Repeat:
    case kYellow:  SkipSeconds( 60); break;
    case kStop:
    case kBlue:    Hide();
                   Stop();
                   return osEnd;
    default: {
      DoShowMode = false;
      switch (Key) {
        // Editing:
        case kMarkToggle:      MarkToggle(); break;
        case kMarkJumpBack|k_Repeat:
        case kMarkJumpBack:    MarkJump(false); break;
        case kMarkJumpForward|k_Repeat:
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
            case kOk:      if (visible && !modeOnly) {
                              Hide();
                              DoShowMode = true;
                              }
                           else
                              Show();
                           break;
            case kBack:    return osRecordings;
            default:       return osUnknown;
            }
          }
        }
      }
    }
  if (DoShowMode)
     ShowMode();
  return osContinue;
}
