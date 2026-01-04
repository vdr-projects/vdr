/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 5.35 2026/01/04 10:18:40 kls Exp $
 */

#include "menu.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "channels.h"
#include "config.h"
#include "cutter.h"
#include "eitscan.h"
#include "i18n.h"
#include "interface.h"
#include "plugin.h"
#include "recording.h"
#include "remote.h"
#include "shutdown.h"
#include "sourceparams.h"
#include "sources.h"
#include "status.h"
#include "svdrp.h"
#include "themes.h"
#include "timers.h"
#include "transfer.h"
#include "videodir.h"

#define MAXWAIT4EPGINFO   3 // seconds
#define MODETIMEOUT       3 // seconds
#define NEWTIMERLIMIT   120 // seconds until the start time of a new timer created from the Schedule menu,
                            // within which it will go directly into the "Edit timer" menu to allow
                            // further parameter settings
#define DEFERTIMER       60 // seconds by which a timer is deferred in case of problems

#define MAXRECORDCONTROLS (MAXDEVICES * MAXRECEIVERS)
#define MAXINSTANTRECTIME (24 * 60 - 1) // 23:59 hours
#define MAXWAITFORCAMMENU  10 // seconds to wait for the CAM menu to open
#define CAMMENURETRYTIMEOUT 3 // seconds after which opening the CAM menu is retried
#define CAMRESPONSETIMEOUT  5 // seconds to wait for a response from a CAM
#define PROGRESSTIMEOUT   100 // milliseconds to wait before updating the replay progress display
#define MINFREEDISK       300 // minimum free disk space (in MB) required to start recording
#define NODISKSPACEDELTA  300 // seconds between "Not enough disk space to start recording!" messages
#define MAXCHNAMWIDTH      16 // maximum number of characters of channels' short names shown in schedules menus

#define CHNUMWIDTH  (numdigits(cChannels::MaxNumber()) + 1)
#define CHNAMWIDTH  (min(MAXCHNAMWIDTH, cChannels::MaxShortChannelNameLength() + 1))

// --- cMenuEditCaItem -------------------------------------------------------

class cMenuEditCaItem : public cMenuEditIntItem {
protected:
  virtual void Set(void) override;
public:
  cMenuEditCaItem(const char *Name, int *Value);
  eOSState ProcessKey(eKeys Key);
  };

cMenuEditCaItem::cMenuEditCaItem(const char *Name, int *Value)
:cMenuEditIntItem(Name, Value, 0)
{
  Set();
}

void cMenuEditCaItem::Set(void)
{
  if (*value == CA_FTA)
     SetValue(tr("Free To Air"));
  else if (*value >= CA_ENCRYPTED_MIN)
     SetValue(tr("encrypted"));
  else
     cMenuEditIntItem::Set();
}

eOSState cMenuEditCaItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     if (NORMALKEY(Key) == kLeft && *value >= CA_ENCRYPTED_MIN)
        *value = CA_FTA;
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
  virtual void Set(void) override;
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
  if (source)
     SetValue(cString::sprintf("%s - %s", *cSource::ToString(source->Code()), source->Description()));
  else
     cMenuEditIntItem::Set();
}

eOSState cMenuEditSrcItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     bool IsRepeat = Key & k_Repeat;
     Key = NORMALKEY(Key);
     if (Key == kLeft) { // TODO might want to increase the delta if repeated quickly?
        if (source) {
           if (source->Prev())
              source = (cSource *)source->Prev();
           else if (!IsRepeat)
              source = Sources.Last();
           *value = source->Code();
           }
        }
     else if (Key == kRight) {
        if (source) {
           if (source->Next())
              source = (cSource *)source->Next();
           else if (!IsRepeat)
              source = Sources.First();
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

// --- cMenuEditChannel ------------------------------------------------------

class cMenuEditChannel : public cOsdMenu {
private:
  cStateKey *channelsStateKey;
  cChannel *channel;
  cChannel data;
  cSourceParam *sourceParam;
  char name[256];
  void Setup(void);
public:
  cMenuEditChannel(cStateKey *ChannelsStateKey, cChannel *Channel, bool New = false);
  cChannel *Channel(void) { return channel; }
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuEditChannel::cMenuEditChannel(cStateKey *ChannelsStateKey, cChannel *Channel, bool New)
:cOsdMenu(tr("Edit channel"), 16)
{
  SetMenuCategory(mcChannelEdit);
  channelsStateKey = ChannelsStateKey;
  channel = Channel;
  sourceParam = NULL;
  *name = 0;
  if (channel) {
     data = *channel;
     strn0cpy(name, data.name, sizeof(name));
     if (New) {
        channel = NULL;
        // clear non-editable members:
        data.nid = 0;
        data.tid = 0;
        data.rid = 0;
        *data.shortName  = 0;
        *data.provider   = 0;
        *data.portalName = 0;
        }
     }
  Setup();
}

void cMenuEditChannel::Setup(void)
{
  int current = Current();

  Clear();

  // Parameters for all types of sources:
  Add(new cMenuEditStrItem( tr("Name"),          name, sizeof(name)));
  Add(new cMenuEditSrcItem( tr("Source"),       &data.source));
  Add(new cMenuEditIntItem( tr("Frequency"),    &data.frequency));
  Add(new cMenuEditIntItem( tr("Vpid"),         &data.vpid,  0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Ppid"),         &data.ppid,  0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Apid1"),        &data.apids[0], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Apid2"),        &data.apids[1], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Dpid1"),        &data.dpids[0], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Dpid2"),        &data.dpids[1], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Spid1"),        &data.spids[0], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Spid2"),        &data.spids[1], 0, 0x1FFF));
  Add(new cMenuEditIntItem( tr("Tpid"),         &data.tpid,  0, 0x1FFF));
  Add(new cMenuEditCaItem(  tr("CA"),           &data.caids[0]));
  Add(new cMenuEditIntItem( tr("Sid"),          &data.sid, 1, 0xFFFF));
  Add(new cMenuEditIntItem( tr("Nid"),          &data.nid, 0));
  Add(new cMenuEditIntItem( tr("Tid"),          &data.tid, 0));
  /* XXX not yet used
  Add(new cMenuEditIntItem( tr("Rid"),          &data.rid, 0));
  XXX*/
  // Parameters for specific types of sources:
  sourceParam = SourceParams.Get(**cSource::ToString(data.source));
  if (sourceParam) {
     sourceParam->SetData(&data);
     cOsdItem *Item;
     while ((Item = sourceParam->GetOsdItem()) != NULL)
           Add(Item);
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuEditChannel::ProcessKey(eKeys Key)
{
  int oldSource = data.source;
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     if (Key == kOk) {
        cChannels *Channels =cChannels::GetChannelsWrite(*channelsStateKey);
        bool Modified = false;
        if (sourceParam)
           sourceParam->GetData(&data);
        if (Channels->HasUniqueChannelID(&data, channel)) {
           data.name = strcpyrealloc(data.name, name);
           if (channel) {
              *channel = data;
              isyslog("edited channel %d %s", channel->Number(), *channel->ToText());
              state = osBack;
              }
           else {
              channel = new cChannel;
              *channel = data;
              Channels->Add(channel);
              Channels->ReNumber();
              isyslog("added channel %d %s", channel->Number(), *channel->ToText());
              state = osUser1;
              }
           Channels->SetModifiedByUser();
           Modified = true;
           }
        else {
           Skins.Message(mtError, tr("Channel settings are not unique!"));
           state = osContinue;
           }
        channelsStateKey->Remove(Modified);
        }
     }
  if (Key != kNone && (data.source & cSource::st_Mask) != (oldSource & cSource::st_Mask)) {
     LOCK_CHANNELS_WRITE;
     if (sourceParam)
        sourceParam->GetData(&data);
     Setup();
     }
  return state;
}

// --- cMenuChannelItem ------------------------------------------------------

class cMenuChannelItem : public cOsdItem {
public:
  enum eChannelSortMode { csmNumber, csmName, csmProvider };
private:
  static eChannelSortMode sortMode;
  const cChannel *channel;
public:
  cMenuChannelItem(const cChannel *Channel);
  static void SetSortMode(eChannelSortMode SortMode) { sortMode = SortMode; }
  static void IncSortMode(void) { sortMode = eChannelSortMode((sortMode == csmProvider) ? csmNumber : sortMode + 1); }
  static eChannelSortMode SortMode(void) { return sortMode; }
  virtual int Compare(const cListObject &ListObject) const override;
  virtual void Set(void) override;
  const cChannel *Channel(void) { return channel; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable) override;
  };

cMenuChannelItem::eChannelSortMode cMenuChannelItem::sortMode = csmNumber;

cMenuChannelItem::cMenuChannelItem(const cChannel *Channel)
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
  cString buffer;
  if (!channel->GroupSep()) {
     const char *X = *channel->Caids() >= CA_ENCRYPTED_MIN ? "X" : "";
     const char *R = !channel->Vpid() && (*channel->Apids() || *channel->Dpids()) ? "R" : "";
     if (sortMode == csmProvider)
        buffer = cString::sprintf("%d\t%s%s\t%s - %s", channel->Number(), X, R, channel->Provider(), channel->Name());
     else
        buffer = cString::sprintf("%d\t%s%s\t%s", channel->Number(), X, R, channel->Name());
     }
  else
     buffer = cString::sprintf("\t\t%s", channel->Name());
  SetText(buffer);
}

void cMenuChannelItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  LOCK_CHANNELS_READ;
  if (!DisplayMenu->SetItemChannel(channel, Index, Current, Selectable, sortMode == csmProvider))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuChannels ---------------------------------------------------------

#define CHANNELNUMBERTIMEOUT 1000 //ms

class cMenuChannels : public cOsdMenu {
private:
  cStateKey channelsStateKey;
  int number;
  cTimeMs numberTimer;
  void Set(bool Force = false);
  cChannel *GetChannel(int Index);
  void Propagate(cChannels *Channels);
protected:
  eOSState Number(eKeys Key);
  eOSState Switch(void);
  eOSState Edit(void);
  eOSState New(void);
  eOSState Delete(void);
  virtual void Move(int From, int To) override;
public:
  cMenuChannels(void);
  ~cMenuChannels();
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuChannels::cMenuChannels(void)
:cOsdMenu(tr("Channels"), CHNUMWIDTH, 3)
{
  SetMenuCategory(mcChannel);
  number = 0;
  Set();
}

cMenuChannels::~cMenuChannels()
{
}

void cMenuChannels::Set(bool Force)
{
  if (Force)
     channelsStateKey.Reset();
  if (const cChannels *Channels = cChannels::GetChannelsRead(channelsStateKey)) {
     const cChannel *CurrentChannel = GetChannel(Current());
     if (!CurrentChannel)
        CurrentChannel = Channels->GetByNumber(cDevice::CurrentChannel());
     cMenuChannelItem *CurrentItem = NULL;
     Clear();
     for (const cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
         if (!Channel->GroupSep() || cMenuChannelItem::SortMode() == cMenuChannelItem::csmNumber && *Channel->Name()) {
            cMenuChannelItem *Item = new cMenuChannelItem(Channel);
            Add(Item);
            if (Channel == CurrentChannel)
               CurrentItem = Item;
            }
         }
     SetMenuSortMode(cMenuChannelItem::SortMode() == cMenuChannelItem::csmName ? msmName :
                     cMenuChannelItem::SortMode() == cMenuChannelItem::csmProvider ? msmProvider :
                     msmNumber);
     if (cMenuChannelItem::SortMode() != cMenuChannelItem::csmNumber)
        Sort();
     SetCurrent(CurrentItem);
     SetHelp(tr("Button$Edit"), tr("Button$New"), tr("Button$Delete"), tr("Button$Mark"));
     Display();
     channelsStateKey.Remove();
     }
}

cChannel *cMenuChannels::GetChannel(int Index)
{
  cMenuChannelItem *p = (cMenuChannelItem *)Get(Index);
  return p ? (cChannel *)p->Channel() : NULL;
}

void cMenuChannels::Propagate(cChannels *Channels)
{
  Channels->ReNumber();
  for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next())
      ci->Set();
  Display();
  Channels->SetModifiedByUser();
}

eOSState cMenuChannels::Number(eKeys Key)
{
  if (HasSubMenu())
     return osContinue;
  if (numberTimer.TimedOut())
     number = 0;
  if (!number && Key == k0) {
     cMenuChannelItem::IncSortMode();
     Set(true);
     }
  else {
     LOCK_CHANNELS_READ;
     number = number * 10 + Key - k0;
     for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next()) {
         if (!ci->Channel()->GroupSep() && ci->Channel()->Number() == number) {
            DisplayCurrent(false);
            SetCurrent(ci);
            DisplayCurrent(true);
            break;
            }
         }
     numberTimer.Set(CHANNELNUMBERTIMEOUT);
     }
  return osContinue;
}

eOSState cMenuChannels::Switch(void)
{
  if (HasSubMenu())
     return osContinue;
  LOCK_CHANNELS_READ;
  cChannel *ch = GetChannel(Current());
  if (ch)
     return cDevice::PrimaryDevice()->SwitchChannel(ch, true) ? osEnd : osContinue;
  return osEnd;
}

eOSState cMenuChannels::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  LOCK_CHANNELS_READ;
  cChannel *ch = GetChannel(Current());
  if (ch)
     return AddSubMenu(new cMenuEditChannel(&channelsStateKey, ch));
  return osContinue;
}

eOSState cMenuChannels::New(void)
{
  if (HasSubMenu())
     return osContinue;
  LOCK_CHANNELS_READ;
  return AddSubMenu(new cMenuEditChannel(&channelsStateKey, GetChannel(Current()), true));
}

eOSState cMenuChannels::Delete(void)
{
  if (!HasSubMenu() && Count() > 0) {
     LOCK_TIMERS_READ; // must lock timers before channels!
     cChannels *Channels = cChannels::GetChannelsWrite(channelsStateKey);
     int Index = Current();
     cChannel *Channel = GetChannel(Current());
     if (!Channels->Contains(Channel)) {
        channelsStateKey.Remove(false);
        channelsStateKey.Reset(); // makes sure the menu is refreshed
        return osContinue;
        }
     bool Deleted = false;
     int CurrentChannelNr = cDevice::CurrentChannel();
     cChannel *CurrentChannel = Channels->GetByNumber(CurrentChannelNr);
     int DeletedChannel = Channel->Number();
     // Check if there is a timer using this channel:
     if (Timers->UsesChannel(Channel)) {
        channelsStateKey.Remove(false);
        Skins.Message(mtError, tr("Channel is being used by a timer!"));
        return osContinue;
        }
     if (Interface->Confirm(tr("Delete channel?"))) {
        if (CurrentChannel && Channel == CurrentChannel) {
           int n = Channels->GetNextNormal(CurrentChannel->Index());
           if (n < 0)
              n = Channels->GetPrevNormal(CurrentChannel->Index());
           CurrentChannel = Channels->Get(n);
           CurrentChannelNr = 0; // triggers channel switch below
           }
        Channels->Del(Channel);
        cOsdMenu::Del(Index);
        Propagate(Channels);
        isyslog("channel %d deleted", DeletedChannel);
        Deleted = true;
        if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
           if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
              Channels->SwitchTo(CurrentChannel->Number());
           else
              cDevice::SetCurrentChannel(CurrentChannel->Number());
           }
        }
     channelsStateKey.Remove(Deleted);
     }
  return osContinue;
}

void cMenuChannels::Move(int From, int To)
{
  if (cChannels *Channels = cChannels::GetChannelsWrite(channelsStateKey)) {
     int CurrentChannelNr = cDevice::CurrentChannel();
     cChannel *CurrentChannel = Channels->GetByNumber(CurrentChannelNr);
     cChannel *FromChannel = GetChannel(From);
     cChannel *ToChannel = GetChannel(To);
     if (FromChannel && ToChannel) {
        int FromNumber = FromChannel->Number();
        int ToNumber = ToChannel->Number();
        if (Channels->MoveNeedsDecrement(FromChannel, ToChannel)) {
           ToChannel = Channels->Prev(ToChannel); // cListBase::Move() doesn't know about the channel list's numbered groups!
           To--;
           }
        Channels->Move(FromChannel, ToChannel);
        cOsdMenu::Move(From, To);
        SetCurrent(Get(To));
        Propagate(Channels);
        isyslog("channel %d moved to %d", FromNumber, ToNumber);
        if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
           if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
              Channels->SwitchTo(CurrentChannel->Number());
           else
              cDevice::SetCurrentChannel(CurrentChannel->Number());
           }
        }
     channelsStateKey.Remove();
     }
}

eOSState cMenuChannels::ProcessKey(eKeys Key)
{
  if (!HasSubMenu())
     Set(); // react on any changes to the channels list
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osUser1: {
         if (cMenuEditChannel *MenuEditChannel = dynamic_cast<cMenuEditChannel *>(SubMenu())) {
            if (cChannel *Channel = MenuEditChannel->Channel()) {
               LOCK_CHANNELS_READ;
               Add(new cMenuChannelItem(Channel), true);
               return CloseSubMenu();
               }
            }
         }
         break;
    default:
         if (state == osUnknown) {
            switch (int(Key)) {
              case k0 ... k9:
                            return Number(Key);
              case kOk:     return Switch();
              case kRed:    return Edit();
              case kGreen:  return New();
              case kYellow: return Delete();
              case kBlue:   if (!HasSubMenu())
                               Mark();
                            break;
              case kChanUp|k_Repeat:
              case kChanUp:
              case kChanDn|k_Repeat:
              case kChanDn: {
                   LOCK_CHANNELS_READ;
                   int CurrentChannelNr = cDevice::CurrentChannel();
                   for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next()) {
                       if (!ci->Channel()->GroupSep() && ci->Channel()->Number() == CurrentChannelNr) {
                          SetCurrent(ci);
                          Display();
                          break;
                          }
                       }
                   }
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
  SetMenuCategory(mcText);
  text = NULL;
  font = Font;
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
  DisplayMenu()->SetText(text, font == fontFix); //XXX define control character in text to choose the font???
  if (text)
     cStatus::MsgOsdTextItem(text);
}

eOSState cMenuText::ProcessKey(eKeys Key)
{
  switch (int(Key)) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft);
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

// --- cMenuFolderItem -------------------------------------------------------

class cMenuFolderItem : public cOsdItem {
private:
  cNestedItem *folder;
public:
  virtual void Set(void) override;
  cMenuFolderItem(cNestedItem *Folder);
  cNestedItem *Folder(void) { return folder; }
  };

cMenuFolderItem::cMenuFolderItem(cNestedItem *Folder)
:cOsdItem(Folder->Text())
{
  folder = Folder;
  Set();
}

void cMenuFolderItem::Set(void)
{
  if (folder->SubItems() && folder->SubItems()->Count())
     SetText(cString::sprintf("%s...", folder->Text()));
  else
     SetText(folder->Text());
}

// --- cMenuEditFolder -------------------------------------------------------

class cMenuEditFolder : public cOsdMenu {
private:
  cList<cNestedItem> *list;
  cNestedItem *folder;
  char name[PATH_MAX];
  eOSState Confirm(void);
public:
  cMenuEditFolder(const char *Dir, cList<cNestedItem> *List, cNestedItem *Folder = NULL);
  cString GetFolder(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuEditFolder::cMenuEditFolder(const char *Dir, cList<cNestedItem> *List, cNestedItem *Folder)
:cOsdMenu(Folder ? tr("Edit folder") : tr("New folder"), 12)
{
  SetMenuCategory(mcFolder);
  list = List;
  folder = Folder;
  if (folder)
     strn0cpy(name, folder->Text(), sizeof(name));
  else {
     *name = 0;
     cRemote::Put(kRight, true); // go right into string editing mode
     }
  if (!isempty(Dir)) {
     cOsdItem *DirItem = new cOsdItem(Dir);
     DirItem->SetSelectable(false);
     Add(DirItem);
     }
  Add(new cMenuEditStrItem( tr("Name"), name, sizeof(name)));
}

cString cMenuEditFolder::GetFolder(void)
{
  return folder ? folder->Text() : "";
}

eOSState cMenuEditFolder::Confirm(void)
{
  if (!folder || strcmp(folder->Text(), name) != 0) {
     // each name may occur only once in a folder list
     for (cNestedItem *Folder = list->First(); Folder; Folder = list->Next(Folder)) {
         if (strcmp(Folder->Text(), name) == 0) {
            Skins.Message(mtError, tr("Folder name already exists!"));
            return osContinue;
            }
         }
     char *p = strpbrk(name, "\\{}#~"); // FOLDERDELIMCHAR
     if (p) {
        Skins.Message(mtError, cString::sprintf(tr("Folder name must not contain '%c'!"), *p));
        return osContinue;
        }
     }
  if (folder)
     folder->SetText(name);
  else
     list->Add(folder = new cNestedItem(name));
  return osEnd;
}

eOSState cMenuEditFolder::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Confirm();
       case kRed:
       case kGreen:
       case kYellow:
       case kBlue:   return osContinue;
       default: break;
       }
     }
  return state;
}

// --- cMenuFolder -----------------------------------------------------------

cMenuFolder::cMenuFolder(const char *Title, cNestedItemList *NestedItemList, const char *Path)
:cOsdMenu(Title)
{
  SetMenuCategory(mcFolder);
  list = nestedItemList = NestedItemList;
  firstFolder = NULL;
  editing = false;
  helpKeys = -1;
  Set();
  DescendPath(Path);
  Display();
  SetHelpKeys();
}

cMenuFolder::cMenuFolder(const char *Title, cList<cNestedItem> *List, cNestedItemList *NestedItemList, const char *Dir, const char *Path)
:cOsdMenu(Title)
{
  SetMenuCategory(mcFolder);
  list = List;
  nestedItemList = NestedItemList;
  dir = Dir;
  firstFolder = NULL;
  editing = false;
  helpKeys = -1;
  Set();
  DescendPath(Path);
  Display();
  SetHelpKeys();
}

void cMenuFolder::SetHelpKeys(void)
{
  if (HasSubMenu())
     return;
  int NewHelpKeys = 0;
  if (firstFolder)
     NewHelpKeys = 1;
  if (NewHelpKeys != helpKeys) {
     helpKeys = NewHelpKeys;
     SetHelp(NewHelpKeys > 0 ? tr("Button$Open") : NULL, tr("Button$New"), firstFolder ? tr("Button$Delete") : NULL, firstFolder ? tr("Button$Edit") : NULL);
     }
}

#define FOLDERDELIMCHARSUBST 0x01
static void AddRecordingFolders(const cRecordings *Recordings, cList<cNestedItem> *List, char *Path)
{
  if (Path) {
     char *p = strchr(Path, FOLDERDELIMCHARSUBST);
     if (p)
        *p++ = 0;
     cNestedItem *Folder;
     for (Folder = List->First(); Folder; Folder = List->Next(Folder)) {
         if (strcmp(Path, Folder->Text()) == 0)
            break;
         }
     if (!Folder)
        List->Add(Folder = new cNestedItem(Path));
     if (p) {
        Folder->SetSubItems(true);
        AddRecordingFolders(Recordings, Folder->SubItems(), p);
        }
     }
  else {
     cStringList Dirs;
     for (const cRecording *Recording = Recordings->First(); Recording; Recording = Recordings->Next(Recording)) {
         cString Folder = Recording->Folder();
         strreplace((char *)*Folder, FOLDERDELIMCHAR, FOLDERDELIMCHARSUBST); // makes sure parent folders come before subfolders
         if (Dirs.Find(Folder) < 0)
            Dirs.Append(strdup(Folder));
         }
     Dirs.Sort();
     for (int i = 0; i < Dirs.Size(); i++) {
         if (char *s = Dirs[i])
            AddRecordingFolders(Recordings, &Folders, s);
         }
     }
}

void cMenuFolder::Set(const char *CurrentFolder)
{
  static cStateKey RecordingsStateKey;
  if (list == &Folders) {
     if (const cRecordings *Recordings = cRecordings::GetRecordingsRead(RecordingsStateKey)) {
        AddRecordingFolders(Recordings, &Folders, NULL);
        RecordingsStateKey.Remove();
        }
     }
  firstFolder = NULL;
  Clear();
  if (!isempty(dir)) {
     cOsdItem *DirItem = new cOsdItem(dir);
     DirItem->SetSelectable(false);
     Add(DirItem);
     }
  list->Sort();
  for (cNestedItem *Folder = list->First(); Folder; Folder = list->Next(Folder)) {
      cOsdItem *FolderItem = new cMenuFolderItem(Folder);
      Add(FolderItem, CurrentFolder ? strcmp(Folder->Text(), CurrentFolder) == 0 : false);
      if (!firstFolder)
         firstFolder = FolderItem;
      }
}

void cMenuFolder::DescendPath(const char *Path)
{
  if (Path) {
     const char *p = strchr(Path, FOLDERDELIMCHAR);
     if (p) {
        for (cMenuFolderItem *Folder = (cMenuFolderItem *)firstFolder; Folder; Folder = (cMenuFolderItem *)Next(Folder)) {
            if (strncmp(Folder->Folder()->Text(), Path, p - Path) == 0) {
               SetCurrent(Folder);
               if (Folder->Folder()->SubItems() && strchr(p + 1, FOLDERDELIMCHAR))
                  AddSubMenu(new cMenuFolder(Title(), Folder->Folder()->SubItems(), nestedItemList, !isempty(dir) ? *cString::sprintf("%s%c%s", *dir, FOLDERDELIMCHAR, Folder->Folder()->Text()) : Folder->Folder()->Text(), p + 1));
               break;
               }
            }
        }
    }
}

eOSState cMenuFolder::Select(bool Open)
{
  if (firstFolder) {
     cMenuFolderItem *Folder = (cMenuFolderItem *)Get(Current());
     if (Folder) {
        if (Open) {
           Folder->Folder()->SetSubItems(true);
           return AddSubMenu(new cMenuFolder(Title(), Folder->Folder()->SubItems(), nestedItemList, !isempty(dir) ? *cString::sprintf("%s%c%s", *dir, FOLDERDELIMCHAR, Folder->Folder()->Text()) : Folder->Folder()->Text()));
           }
        else
           return osEnd;
        }
     }
  return osContinue;
}

eOSState cMenuFolder::New(void)
{
  editing = true;
  return AddSubMenu(new cMenuEditFolder(dir, list));
}

eOSState cMenuFolder::Delete(void)
{
  if (!HasSubMenu() && firstFolder) {
     cMenuFolderItem *Folder = (cMenuFolderItem *)Get(Current());
     if (Folder && Interface->Confirm(Folder->Folder()->SubItems() ? tr("Delete folder and all sub folders?") : tr("Delete folder?"))) {
        list->Del(Folder->Folder());
        Del(Folder->Index());
        firstFolder = Get(isempty(dir) ? 0 : 1);
        Display();
        SetHelpKeys();
        nestedItemList->Save();
        }
     }
  return osContinue;
}

eOSState cMenuFolder::Edit(void)
{
  if (!HasSubMenu() && firstFolder) {
     cMenuFolderItem *Folder = (cMenuFolderItem *)Get(Current());
     if (Folder) {
        editing = true;
        return AddSubMenu(new cMenuEditFolder(dir, list, Folder->Folder()));
        }
     }
  return osContinue;
}

eOSState cMenuFolder::SetFolder(void)
{
  if (cMenuEditFolder *mef = dynamic_cast<cMenuEditFolder *>(SubMenu())) {
     Set(mef->GetFolder());
     SetHelpKeys();
     Display();
     nestedItemList->Save();
     }
  return CloseSubMenu();
}

cString cMenuFolder::GetFolder(void)
{
  if (firstFolder) {
     cMenuFolderItem *Folder = (cMenuFolderItem *)Get(Current());
     if (Folder) {
        if (cMenuFolder *mf = dynamic_cast<cMenuFolder *>(SubMenu()))
           return cString::sprintf("%s%c%s", Folder->Folder()->Text(), FOLDERDELIMCHAR, *mf->GetFolder());
        return Folder->Folder()->Text();
        }
     }
  return "";
}

eOSState cMenuFolder::ProcessKey(eKeys Key)
{
  if (!HasSubMenu())
     editing = false;
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     return Select(false);
       case kRed:    return Select(true);
       case kGreen:  return New();
       case kYellow: return Delete();
       case kBlue:   return Edit();
       default:      state = osContinue;
       }
     }
  else if (state == osEnd && HasSubMenu() && editing)
     state = SetFolder();
  SetHelpKeys();
  return state;
}

// --- cMenuEditTimer --------------------------------------------------------

static const char *TimerFileMacrosForPattern[] = {
  TIMERMACRO_TITLE,
  TIMERMACRO_EPISODE,
  TIMERMACRO_BEFORE,
  TIMERMACRO_MATCH,
  TIMERMACRO_AFTER,
  "",
  NULL
  };

static const char *TimerFileMacros[] = {
  TIMERMACRO_TITLE,
  TIMERMACRO_EPISODE,
  "",
  NULL
  };

const cTimer *cMenuEditTimer::addedTimer = NULL;

cMenuEditTimer::cMenuEditTimer(cTimer *Timer, bool New)
:cOsdMenu(tr("Edit timer"), 12)
{
  SetMenuCategory(mcTimerEdit);
  addedTimer = NULL;
  pattern = NULL;
  file = NULL;
  day = firstday = NULL;
  timer = Timer;
  addIfConfirmed = New;
  if (timer) {
     data = *timer;
     if (New)
        data.SetFlags(tfActive);
     channel = data.Channel()->Number();
     Add(new cMenuEditBitItem( tr("Active"),       &data.flags, tfActive));
     Add(new cMenuEditChanItem(tr("Channel"),      &channel));
     Add(day = new cMenuEditDateItem(tr("Day"),    &data.day, &data.weekdays));
     Add(new cMenuEditTimeItem(tr("Start"),        &data.start));
     Add(new cMenuEditTimeItem(tr("Stop"),         &data.stop));
     Add(new cMenuEditBitItem( tr("VPS"),          &data.flags, tfVps));
     Add(new cMenuEditIntItem( tr("Priority"),     &data.priority, 0, MAXPRIORITY));
     Add(new cMenuEditIntItem( tr("Lifetime"),     &data.lifetime, 0, MAXLIFETIME));
     Add(file = new cMenuEditStrItem( tr("File"),   data.file, sizeof(data.file)));
     SetFirstDayItem();
     SetPatternItem(true);
     if (data.remote)
        strn0cpy(remote, data.remote, sizeof(remote));
     else
        *remote = 0;
     if (GetSVDRPServerNames(&svdrpServerNames)) {
        svdrpServerNames.Sort(true);
        svdrpServerNames.Insert(strdup(""));
        Add(new cMenuEditStrlItem(tr("Record on"), remote, sizeof(remote), &svdrpServerNames));
        }
     }
  SetHelpKeys();
}

cMenuEditTimer::~cMenuEditTimer()
{
  if (timer && addIfConfirmed)
     delete timer; // apparently it wasn't confirmed
}

const cTimer *cMenuEditTimer::AddedTimer(void)
{
  const cTimer *Timer = addedTimer;
  addedTimer = NULL;
  return Timer;
}

void cMenuEditTimer::SetHelpKeys(void)
{
  SetHelp(tr("Button$Folder"), data.weekdays ? tr("Button$Single") : tr("Button$Repeating"), *data.pattern ? tr("Button$Regular") : tr("Button$Pattern"));
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

void cMenuEditTimer::SetPatternItem(bool Initial)
{
  if (Initial && !*data.pattern) {
     file->SetMacros(TimerFileMacros);
     return;
     }
  if (!pattern) {
     if (data.HasFlags(tfRecording)) {
        Skins.Message(mtWarning, tr("Timer is recording!"));
        return;
        }
     if (!*data.pattern) {
        char *p = strgetlast(data.file, FOLDERDELIMCHAR);
        strn0cpy(data.pattern, p, sizeof(data.pattern));
        }
     Ins(pattern = new cMenuEditStrItem( tr("Pattern"), data.pattern, sizeof(data.pattern)), true, file);
     pattern->SetKeepSpace();
     file->SetMacros(TimerFileMacrosForPattern);
     Display();
     }
  else {
     Del(pattern->Index());
     pattern = NULL;
     *data.pattern = 0;
     file->SetMacros(TimerFileMacros);
     Display();
     }
  SetHelpKeys();
}

eOSState cMenuEditTimer::SetFolder(void)
{
  if (cMenuFolder *mf = dynamic_cast<cMenuFolder *>(SubMenu())) {
     cString Folder = mf->GetFolder();
     char *p = strgetlast(data.file, FOLDERDELIMCHAR);
     if (!isempty(*Folder))
        strn0cpy(data.file, cString::sprintf("%s%c%s", *Folder, FOLDERDELIMCHAR, p), sizeof(data.file));
     else if (p != data.file)
        memmove(data.file, p, strlen(p) + 1);
     SetCurrent(file);
     Display();
     }
  return CloseSubMenu();
}

static bool RemoteTimerError(const cTimer *Timer)
{
  Skins.Message(mtError, cString::sprintf("%s %d@%s!", tr("Error while accessing remote timer"), Timer->Id(), Timer->Remote()));
  return false; // convenience return code
}

static bool HandleRemoteModifications(cTimer *NewTimer, cTimer *OldTimer = NULL)
{
  cString ErrorMessage;
  if (!HandleRemoteTimerModifications(NewTimer, OldTimer, &ErrorMessage)) {
     Skins.QueueMessage(mtError, ErrorMessage);
     return false;
     }
  return true;
}

eOSState cMenuEditTimer::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kOk:     if (timer) {
                        LOCK_TIMERS_WRITE;
                        if (!addIfConfirmed && !Timers->Contains(timer)) {
                           if (cTimer *t = Timers->GetById(timer->Id(), timer->Remote()))
                              timer = t;
                           else {
                              Skins.Message(mtWarning, tr("Timer has been deleted!"));
                              break;
                              }
                           }
                        LOCK_CHANNELS_READ;
                        if (const cChannel *Channel = Channels->GetByNumber(channel))
                           data.channel = Channel;
                        else {
                           Skins.Message(mtError, tr("*** Invalid Channel ***"));
                           break;
                           }
                        if (!*data.file)
                           strcpy(data.file, data.Channel()->ShortName(true));
                        data.SetRemote(*remote ? remote : NULL);
                        if (addIfConfirmed) {
                           *timer = data;
                           Timers->Add(timer);
                           addedTimer = timer;
                           if (!HandleRemoteModifications(timer)) {
                              // must add the timer before HandleRemoteModifications to get proper log messages with timer ids
                              Timers->Del(timer, false);
                              addedTimer = NULL;
                              return osContinue;
                              }
                           }
                        else {
                           if (!HandleRemoteModifications(&data, timer))
                              return osContinue;
                           if (timer->Local() && timer->Recording() && data.Remote())
                              cRecordControls::Stop(timer);
                           if (timer->Remote() && data.Remote())
                              Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
                           if (data.Local() && !timer->IsPatternTimer() && data.IsPatternTimer())
                              data.SetEvent(NULL);
                           *timer = data;
                           }
                        timer->TriggerRespawn();
                        LOCK_SCHEDULES_READ;
                        timer->SetEventFromSchedule(Schedules);
                        timer->Matches();
                        addIfConfirmed = false;
                        }
                     return osBack;
       case kRed:    return AddSubMenu(new cMenuFolder(tr("Select folder"), &Folders, data.file));
       case kGreen:  if (day) {
                        day->ToggleRepeating();
                        SetCurrent(day);
                        SetFirstDayItem();
                        SetHelpKeys();
                        Display();
                        }
                     return osContinue;
       case kYellow: SetPatternItem();
                     return osContinue;
       case kBlue:   return osContinue;
       default: break;
       }
     }
  else if (state == osEnd && HasSubMenu())
     state = SetFolder();
  if (Key != kNone)
     SetFirstDayItem();
  return state;
}

// --- cMenuTimerItem --------------------------------------------------------

class cMenuTimerItem : public cOsdItem {
private:
  const cTimer *timer;
public:
  cMenuTimerItem(const cTimer *Timer);
  virtual int Compare(const cListObject &ListObject) const override;
  virtual void Set(void) override;
  const cTimer *Timer(void) { return timer; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable) override;
  };

cMenuTimerItem::cMenuTimerItem(const cTimer *Timer)
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
     day = timer->PrintDay(0, timer->WeekDays(), false);
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
  const char *File = timer->Pattern();
  if (!*File) {
     LOCK_SCHEDULES_READ;
     if (timer->HasFlags(tfSpawned) && timer->Event() && timer->Event()->Title())
        File = timer->Event()->Title();
     else {
        File = Setup.FoldersInTimerMenu ? NULL : strrchr(timer->File(), FOLDERDELIMCHAR);
        if (File && strcmp(File + 1, TIMERMACRO_TITLE) && strcmp(File + 1, TIMERMACRO_EPISODE))
           File++;
        else
           File = timer->File();
        }
     }
  SetText(cString::sprintf("%c\t%d\t%s%s%s\t%02d:%02d\t%02d:%02d\t%s%s%s%s",
                    !(timer->HasFlags(tfActive)) ? ' ' : timer->FirstDay() ? '!' : timer->Recording() ? '#' : '>',
                    timer->Channel()->Number(),
                    *name,
                    *name && **name ? " " : "",
                    *day,
                    timer->Start() / 100,
                    timer->Start() % 100,
                    timer->Stop() / 100,
                    timer->Stop() % 100,
                    timer->Remote() ? *cString::sprintf("@%s: ", timer->Remote()) : "",
                    timer->IsPatternTimer() ? "{" : "",
                    File,
                    timer->IsPatternTimer() ? "}" : ""));
}

void cMenuTimerItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  LOCK_TIMERS_READ;
  if (!DisplayMenu->SetItemTimer(timer, Index, Current, Selectable))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuTimers -----------------------------------------------------------

class cMenuTimers : public cOsdMenu {
private:
  cStateKey timersStateKey;
  int helpKeys;
  bool showAllTimers;
  void Set(void);
  eOSState Edit(void);
  eOSState New(void);
  eOSState Delete(void);
  eOSState OnOff(void);
  eOSState Info(void);
  cTimer *GetTimer(void);
  void SetHelpKeys(void);
public:
  cMenuTimers(void);
  virtual ~cMenuTimers() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuTimers::cMenuTimers(void)
:cOsdMenu(tr("Timers"), 2, CHNUMWIDTH, 10, 6, 6)
{
  SetMenuCategory(mcTimer);
  helpKeys = -1;
  showAllTimers = true;
  cMenuEditTimer::AddedTimer(); // to clear any leftovers
  Set();
}

cMenuTimers::~cMenuTimers()
{
}

void cMenuTimers::Set(void)
{
  if (const cTimers *Timers = cTimers::GetTimersRead(timersStateKey)) {
     const cTimer *CurrentTimer = GetTimer();
     cMenuTimerItem *CurrentItem = NULL;
     Clear();
     {
       LOCK_SCHEDULES_READ;
       for (const cTimer *Timer = Timers->First(); Timer; Timer = Timers->Next(Timer)) {
           cMenuTimerItem *Item = new cMenuTimerItem(Timer);
           if (showAllTimers || Timer->HasFlags(tfActive)) {
              Add(Item);
              if (CurrentTimer && Timer->Id() == CurrentTimer->Id() && (!Timer->Remote() && !CurrentTimer->Remote() || Timer->Remote() && CurrentTimer->Remote() && strcmp(Timer->Remote(), CurrentTimer->Remote()) == 0))
                 CurrentItem = Item;
              }
           }
     }
     Sort();
     SetCurrent(CurrentItem ? CurrentItem : First());
     SetHelpKeys();
     Display();
     timersStateKey.Remove();
     }
}

cTimer *cMenuTimers::GetTimer(void)
{
  cMenuTimerItem *item = (cMenuTimerItem *)Get(Current());
  return item ? (cTimer *)item->Timer() : NULL;
}

void cMenuTimers::SetHelpKeys(void)
{
  int NewHelpKeys = 0;
  if (const cTimer *Timer = GetTimer()) {
     if (Timer->Event())
        NewHelpKeys = 2;
     else
        NewHelpKeys = 1;
     }
  if (NewHelpKeys != helpKeys) {
     helpKeys = NewHelpKeys;
     SetHelp(helpKeys > 0 ? tr("Button$On/Off") : NULL, tr("Button$New"), helpKeys > 0 ? tr("Button$Delete") : NULL, helpKeys == 2 ? tr("Button$Info") : NULL);
     }
}

eOSState cMenuTimers::OnOff(void)
{
  if (HasSubMenu())
     return osContinue;
  cStateKey StateKey;
  cTimers *Timers = cTimers::GetTimersWrite(StateKey);
  cTimer *Timer = GetTimer();
  if (Timer) {
     Timer->OnOff();
     if (Timer->Remote()) {
        Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
        cStringList Response;
        if (!ExecSVDRPCommand(Timer->Remote(), cString::sprintf("MODT %d %s", Timer->Id(), *Timer->ToText(true)), &Response) || SVDRPCode(Response[0]) != 250)
           RemoteTimerError(Timer);
        }
     {
       LOCK_SCHEDULES_READ;
       Timer->SetEventFromSchedule(Schedules);
     }
     RefreshCurrent();
     DisplayCurrent(true);
     if (Timer->FirstDay())
        isyslog("set first day of timer %s to %s", *Timer->ToDescr(), *Timer->PrintFirstDay());
     else
        isyslog("%sactivated timer %s", Timer->HasFlags(tfActive) ? "" : "de", *Timer->ToDescr());
     }
  StateKey.Remove(Timer != NULL);
  return osContinue;
}

eOSState cMenuTimers::Edit(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  return AddSubMenu(new cMenuEditTimer(GetTimer()));
}

eOSState cMenuTimers::New(void)
{
  if (HasSubMenu())
     return osContinue;
  cTimer *Timer = new cTimer;
  if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost)
     Timer->SetRemote(Setup.SVDRPDefaultHost);
  return AddSubMenu(new cMenuEditTimer(Timer, true));
}

eOSState cMenuTimers::Delete(void)
{
  cTimers *Timers = cTimers::GetTimersWrite(timersStateKey);
  // Check if this timer is active:
  cTimer *Timer = GetTimer();
  if (Timer) {
     bool TimerRecording = Timer->Recording();
     timersStateKey.Remove(false); // must release lock while prompting!
     if (Interface->Confirm(tr("Delete timer?")) && (!TimerRecording || Interface->Confirm(tr("Timer still recording - really delete?")))) {
        Timers = cTimers::GetTimersWrite(timersStateKey);
        Timer = GetTimer();
        if (Timer) {
           if (!Timer->Remote()) {
              Timer->Skip();
              cRecordControls::Process(Timers, time(NULL));
              }
           if (HandleRemoteModifications(NULL, Timer)) {
              if (Timer->Remote())
                 Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
              Timers->Del(Timer);
              cOsdMenu::Del(Current());
              Display();
              }
           }
        }
     else
        return osContinue;
     }
  timersStateKey.Remove(Timer != NULL);
  return osContinue;
}

eOSState cMenuTimers::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  LOCK_TIMERS_READ;
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_READ;
  cTimer *Timer = GetTimer();
  if (Timer && Timer->Event())
     return AddSubMenu(new cMenuEvent(Timers, Channels, Timer->Event()));
  return osContinue;
}

eOSState cMenuTimers::ProcessKey(eKeys Key)
{
  if (!HasSubMenu())
     Set();
  eOSState state = cOsdMenu::ProcessKey(Key);
  bool oldShowAllTimers = showAllTimers;
  if (state == osUnknown) {
     switch (Key) {
       case k0:      showAllTimers = !showAllTimers; break;
       case kOk:     return Edit();
       case kRed:    state = OnOff(); break; // must go through SetHelpKeys()!
       case kGreen:  return New();
       case kYellow: state = Delete(); break;
       case kInfo:
       case kBlue:   return Info();
                     break;
       default: break;
       }
     }
  if (const cTimer *Timer = cMenuEditTimer::AddedTimer()) {
     // a newly created timer was confirmed with Ok and the proper item needs to be added:
     LOCK_TIMERS_READ;
     cMenuTimerItem *CurrentItem = new cMenuTimerItem(Timer);
     Add(CurrentItem, true);
     Sort();
     SetCurrent(CurrentItem);
     SetHelpKeys();
     Display();
     }
  if (showAllTimers != oldShowAllTimers) {
     timersStateKey.Reset();
     Set();
     }
  if (Key != kNone)
     SetHelpKeys();
  return state;
}

// --- cMenuEvent ------------------------------------------------------------

cMenuEvent::cMenuEvent(const cTimers *Timers, const cChannels *Channels, const cEvent *Event, bool CanSwitch, bool Buttons)
:cOsdMenu(tr("Event"))
{
  SetMenuCategory(mcEvent);
  event = Event;
  if (event) {
     if (const cChannel *Channel = Channels->GetByChannelID(event->ChannelID(), true)) {
        SetTitle(Channel->Name());
        if (Buttons) {
           eTimerMatch TimerMatch = tmNone;
           Timers->GetMatch(event, &TimerMatch);
           SetHelp(TimerMatch == tmFull ? tr("Button$Timer") : tr("Button$Record"), NULL, NULL, CanSwitch ? tr("Button$Switch") : NULL);
           }
        }
     }
}

void cMenuEvent::Display(void)
{
  LOCK_SCHEDULES_READ;
  cOsdMenu::Display();
  DisplayMenu()->SetEvent(event);
  if (event->Description())
     cStatus::MsgOsdTextItem(event->Description());
}

eOSState cMenuEvent::ProcessKey(eKeys Key)
{
  switch (int(Key)) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft);
                  return osContinue;
    case kInfo:   return osBack;
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

// --- cMenuScheduleItem -----------------------------------------------------

class cMenuScheduleItem : public cOsdItem {
public:
  enum eScheduleSortMode { ssmAllThis, ssmThisThis, ssmThisAll, ssmAllAll }; // "which event(s) on which channel(s)"
private:
  static eScheduleSortMode sortMode;
public:
  const cEvent *event;
  const cChannel *channel;
  const cTimer *timer;
  bool withDate;
  eTimerMatch timerMatch;
  bool timerActive;
  cMenuScheduleItem(const cTimers *Timers, const cEvent *Event, const cChannel *Channel = NULL, bool WithDate = false);
  static void SetSortMode(eScheduleSortMode SortMode) { sortMode = SortMode; }
  static void IncSortMode(void) { sortMode = eScheduleSortMode((sortMode == ssmAllAll) ? ssmAllThis : sortMode + 1); }
  static eScheduleSortMode SortMode(void) { return sortMode; }
  virtual int Compare(const cListObject &ListObject) const override;
  bool Update(const cTimers *Timers, bool Force = false);
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable) override;
  };

cMenuScheduleItem::eScheduleSortMode cMenuScheduleItem::sortMode = ssmAllThis;

cMenuScheduleItem::cMenuScheduleItem(const cTimers *Timers, const cEvent *Event, const cChannel *Channel, bool WithDate)
{
  event = Event;
  channel = Channel;
  timer = NULL;
  withDate = WithDate;
  timerMatch = tmNone;
  timerActive = false;
  Update(Timers, true);
}

int cMenuScheduleItem::Compare(const cListObject &ListObject) const
{
  cMenuScheduleItem *p = (cMenuScheduleItem *)&ListObject;
  int r = -1;
  if (sortMode != ssmAllThis)
     r = strcoll(event->Title(), p->event->Title());
  if (sortMode == ssmAllThis || r == 0)
     r = event->StartTime() - p->event->StartTime();
  return r;
}

static const char *TimerMatchChars = " tT iI";

bool cMenuScheduleItem::Update(const cTimers *Timers, bool Force)
{
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_READ;
  eTimerMatch OldTimerMatch = timerMatch;
  bool OldTimerActive = timerActive;
  if (event->Schedule() && event->Schedule()->HasTimer())
     timer = Timers->GetMatch(event, &timerMatch);
  if (event->EndTime() < time(NULL) && !event->IsRunning() && (!timer || !timer->Recording()))
     timerMatch = tmNone;
  timerActive = timer && timer->HasFlags(tfActive);
  if (Force || timerMatch != OldTimerMatch || timerActive != OldTimerActive) {
     cString buffer;
     char t = TimerMatchChars[timerMatch + (timerActive ? 0 : 3)];
     char v = event->Vps() && (event->Vps() - event->StartTime()) ? 'V' : ' ';
     char r = event->SeenWithin(30) && event->IsRunning() ? '*' : ' ';
     const char *csn = channel ? channel->ShortName(true) : NULL;
     cString eds = event->GetDateString();
     if (channel && withDate)
        buffer = cString::sprintf("%d\t%.*s\t%.*s\t%s\t%c%c%c\t%s", channel->Number(), Utf8SymChars(csn, 999), csn, Utf8SymChars(eds, 6), *eds, *event->GetTimeString(), t, v, r, event->Title());
     else if (channel)
        buffer = cString::sprintf("%d\t%.*s\t%s\t%c%c%c\t%s", channel->Number(), Utf8SymChars(csn, 999), csn, *event->GetTimeString(), t, v, r, event->Title());
     else
        buffer = cString::sprintf("%.*s\t%s\t%c%c%c\t%s", Utf8SymChars(eds, 6), *eds, *event->GetTimeString(), t, v, r, event->Title());
     SetText(buffer);
     return true;
     }
  return false;
}

void cMenuScheduleItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  LOCK_TIMERS_READ;
  LOCK_CHANNELS_READ;
  LOCK_SCHEDULES_READ;
  if (!DisplayMenu->SetItemEvent(event, Index, Current, Selectable, channel, withDate, timerMatch, timer))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuWhatsOn ----------------------------------------------------------

class cMenuWhatsOn : public cOsdMenu {
private:
  bool now;
  bool canSwitch;
  int helpKeys;
  cStateKey timersStateKey;
  eOSState Record(void);
  eOSState Switch(void);
  static int currentChannel;
  static const cEvent *scheduleEvent;
  bool Update(void);
  void SetHelpKeys(const cChannels *Channels);
public:
  cMenuWhatsOn(const cTimers *Timers, const cChannels *Channels, const cSchedules *Schedules, bool Now, int CurrentChannelNr);
  static int CurrentChannel(void) { return currentChannel; }
  static void SetCurrentChannel(int ChannelNr) { currentChannel = ChannelNr; }
  static const cEvent *ScheduleEvent(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

int cMenuWhatsOn::currentChannel = 0;
const cEvent *cMenuWhatsOn::scheduleEvent = NULL;

cMenuWhatsOn::cMenuWhatsOn(const cTimers *Timers, const cChannels *Channels, const cSchedules *Schedules, bool Now, int CurrentChannelNr)
:cOsdMenu(Now ? tr("What's on now?") : tr("What's on next?"), CHNUMWIDTH, CHNAMWIDTH, 6, 4)
{
  SetMenuCategory(Now ? mcScheduleNow : mcScheduleNext);
  now = Now;
  canSwitch = false;
  helpKeys = 0;
  for (const cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel)) {
      if (!Channel->GroupSep()) {
         if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
            if (const cEvent *Event = Now ? Schedule->GetPresentEvent() : Schedule->GetFollowingEvent())
               Add(new cMenuScheduleItem(Timers, Event, Channel), Channel->Number() == CurrentChannelNr);
            }
         }
      }
  currentChannel = CurrentChannelNr;
  SetHelpKeys(Channels);
}

bool cMenuWhatsOn::Update(void)
{
  bool result = false;
  if (const cTimers *Timers = cTimers::GetTimersRead(timersStateKey)) {
     for (cOsdItem *item = First(); item; item = Next(item)) {
         if (((cMenuScheduleItem *)item)->Update(Timers))
            result = true;
         }
     timersStateKey.Remove();
     }
  return result;
}

void cMenuWhatsOn::SetHelpKeys(const cChannels *Channels)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  canSwitch = false;
  int NewHelpKeys = 0;
  if (item) {
     if (item->timerMatch == tmFull)
        NewHelpKeys |= 0x02; // "Timer"
     else
        NewHelpKeys |= 0x01; // "Record"
     if (now)
        NewHelpKeys |= 0x04; // "Next"
     else
        NewHelpKeys |= 0x08; // "Now"
     if (const cChannel *Channel = Channels->GetByChannelID(item->event->ChannelID(), true)) {
        if (Channel->Number() != cDevice::CurrentChannel()) {
           NewHelpKeys |= 0x10; // "Switch"
           canSwitch = true;
           }
        }
     }
  if (NewHelpKeys != helpKeys) {
     const char *Red[] = { NULL, tr("Button$Record"), tr("Button$Timer") };
     SetHelp(Red[NewHelpKeys & 0x03], now ? tr("Button$Next") : tr("Button$Now"), tr("Button$Schedule"), canSwitch ? tr("Button$Switch") : NULL);
     helpKeys = NewHelpKeys;
     }
}

const cEvent *cMenuWhatsOn::ScheduleEvent(void)
{
  const cEvent *ei = scheduleEvent;
  scheduleEvent = NULL;
  return ei;
}

eOSState cMenuWhatsOn::Switch(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     LOCK_CHANNELS_READ;
     const cChannel *Channel = Channels->GetByChannelID(item->event->ChannelID(), true);
     if (Channel) {
        if (!cDevice::PrimaryDevice()->SwitchChannel(Channel, true))
           Channel = NULL;
        }
     if (Channel)
        return osEnd;
     }
  Skins.Message(mtError, tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuWhatsOn::Record(void)
{
  if (cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current())) {
     LOCK_TIMERS_WRITE;
     LOCK_CHANNELS_READ;
     LOCK_SCHEDULES_READ;
     Timers->SetExplicitModify();
     if (item->timerMatch == tmFull) {
        if (cTimer *Timer = Timers->GetMatch(item->event))
           return AddSubMenu(new cMenuEditTimer(Timer));
        }
     cTimer *Timer = new cTimer(item->event);
     if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost)
        Timer->SetRemote(Setup.SVDRPDefaultHost);
     if (cTimer *t = Timers->GetTimer(Timer)) {
        delete Timer;
        Timer = t;
        return AddSubMenu(new cMenuEditTimer(Timer));
        }
     if (Timer->Matches(0, NEWTIMERLIMIT))
        return AddSubMenu(new cMenuEditTimer(Timer, true));
     Timers->Add(Timer);
     Timers->SetModified();
     if (!HandleRemoteModifications(Timer)) {
        // must add the timer before HandleRemoteModifications to get proper log messages with timer ids
        Timers->Del(Timer);
        }
     else if (Timer->Remote())
        Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
     if (HasSubMenu())
        CloseSubMenu();
     }
  if (Update())
     Display();
  LOCK_CHANNELS_READ;
  SetHelpKeys(Channels);
  return osContinue;
}

eOSState cMenuWhatsOn::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (int(Key)) {
       case kRecord:
       case kRed:    return Record();
       case kYellow: state = osBack;
                     // continue with kGreen
       case kGreen:  {
                       cMenuScheduleItem *mi = (cMenuScheduleItem *)Get(Current());
                       if (mi) {
                          LOCK_CHANNELS_READ;
                          scheduleEvent = mi->event;
                          currentChannel = mi->channel->Number();
                          }
                     }
                     break;
       case kBlue:   if (canSwitch)
                        return Switch();
                     break;
       case kChanUp|k_Repeat:
       case kChanUp:
       case kChanDn|k_Repeat:
       case kChanDn: if (!HasSubMenu()) {
                        LOCK_TIMERS_READ;
                        LOCK_CHANNELS_READ;
                        for (cOsdItem *item = First(); item; item = Next(item)) {
                            if (((cMenuScheduleItem *)item)->channel->Number() == cDevice::CurrentChannel()) {
                               DisplayCurrent(false);
                               SetCurrent(item);
                               DisplayCurrent(true);
                               SetHelpKeys(Channels);
                               break;
                               }
                            }
                        }
                     break;
       case kInfo:
       case kOk:     if (Count()) {
                        LOCK_TIMERS_READ;
                        LOCK_CHANNELS_READ;
                        return AddSubMenu(new cMenuEvent(Timers, Channels, ((cMenuScheduleItem *)Get(Current()))->event, canSwitch, true));
                        }
                     break;
       default:      break;
       }
     }
  else if (!HasSubMenu()) {
     if (HadSubMenu && Update())
        Display();
     if (Key != kNone) {
        LOCK_CHANNELS_READ;
        SetHelpKeys(Channels);
        }
     }
  return state;
}

// --- cMenuSchedule ---------------------------------------------------------

class cMenuSchedule : public cOsdMenu {
private:
  cStateKey timersStateKey;
  cStateKey schedulesStateKey;
  int scheduleState;
  bool now, next;
  bool canSwitch;
  int helpKeys;
  void Set(const cTimers *Timers, const cChannels *Channels, const cChannel *Channel = NULL, bool Force = false);
  eOSState Number(void);
  eOSState Record(void);
  eOSState Switch(void);
  bool PrepareScheduleAllThis(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel);
  bool PrepareScheduleThisThis(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel);
  bool PrepareScheduleThisAll(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel);
  bool PrepareScheduleAllAll(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel);
  bool Update(void);
  void SetHelpKeys(void);
public:
  cMenuSchedule(void);
  virtual ~cMenuSchedule() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSchedule::cMenuSchedule(void)
:cOsdMenu(tr("Schedule"))
{
  SetMenuCategory(mcSchedule);
  scheduleState = -1;
  now = next = false;
  canSwitch = false;
  helpKeys = 0;
  cMenuScheduleItem::SetSortMode(cMenuScheduleItem::ssmAllThis);
  cMenuWhatsOn::SetCurrentChannel(cDevice::CurrentChannel());
  LOCK_TIMERS_READ;
  LOCK_CHANNELS_READ;
  Set(Timers, Channels, NULL, true);
}

cMenuSchedule::~cMenuSchedule()
{
  cMenuWhatsOn::ScheduleEvent(); // makes sure any posted data is cleared
}

void cMenuSchedule::Set(const cTimers *Timers, const cChannels *Channels, const cChannel *Channel, bool Force)
{
  if (Force) {
     schedulesStateKey.Reset();
     scheduleState = -1;
     }
  if (const cSchedules *Schedules = cSchedules::GetSchedulesRead(schedulesStateKey)) {
     cMenuScheduleItem *CurrentItem = (cMenuScheduleItem *)Get(Current());
     const cEvent *Event = NULL;
     if (!Channel) {
        if (CurrentItem) {
           Event = CurrentItem->event;
           Channel = Channels->GetByChannelID(Event->ChannelID(), true);
           }
        else
           Channel = Channels->GetByNumber(cDevice::CurrentChannel());
        }
     bool Refresh = false;
     switch (cMenuScheduleItem::SortMode()) {
       case cMenuScheduleItem::ssmAllThis:  Refresh = PrepareScheduleAllThis(Timers, Schedules, Event, Channel); break;
       case cMenuScheduleItem::ssmThisThis: Refresh = PrepareScheduleThisThis(Timers, Schedules, Event, Channel); break;
       case cMenuScheduleItem::ssmThisAll:  Refresh = Force && PrepareScheduleThisAll(Timers, Schedules, Event, Channel); break;
       case cMenuScheduleItem::ssmAllAll:   Refresh = Force && PrepareScheduleAllAll(Timers, Schedules, Event, Channel); break;
       default: esyslog("ERROR: unknown SortMode %d (%s %d)", cMenuScheduleItem::SortMode(), __FUNCTION__, __LINE__);
       }
     if (Refresh) {
        CurrentItem = (cMenuScheduleItem *)Get(Current());
        Sort();
        SetCurrent(CurrentItem);
        SetHelpKeys();
        Display();
        }
     schedulesStateKey.Remove();
     }
}

bool cMenuSchedule::PrepareScheduleAllThis(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel)
{
  if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
     if (Schedule->Modified(scheduleState)) {
        Clear();
        SetCols(7, 6, 4);
        SetTitle(cString::sprintf(tr("Schedule - %s"), Channel->Name()));
        const cEvent *PresentEvent = Event ? Event : Schedule->GetPresentEvent();
        time_t now = time(NULL) - Setup.EPGLinger * 60;
        for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
            if (ev->EndTime() > now || ev == PresentEvent)
               Add(new cMenuScheduleItem(Timers, ev), ev == PresentEvent);
            }
        return true;
        }
     }
  return false;
}

bool cMenuSchedule::PrepareScheduleThisThis(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel)
{
  if (Event) {
     if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
        if (Schedule->Modified(scheduleState)) {
           Clear();
           SetCols(7, 6, 4);
           SetTitle(cString::sprintf(tr("This event - %s"), Channel->Name()));
           time_t now = time(NULL) - Setup.EPGLinger * 60;
           for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
               if ((ev->EndTime() > now || ev == Event) && !strcmp(ev->Title(), Event->Title()))
                  Add(new cMenuScheduleItem(Timers, ev), ev == Event);
               }
           return true;
           }
        }
     }
  return false;
}

bool cMenuSchedule::PrepareScheduleThisAll(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(CHNUMWIDTH, CHNAMWIDTH, 7, 6, 4);
  SetTitle(tr("This event - all channels"));
  if (Event) {
     LOCK_CHANNELS_READ;
     for (const cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch)) {
         if (const cSchedule *Schedule = Schedules->GetSchedule(ch)) {
            time_t now = time(NULL) - Setup.EPGLinger * 60;
            for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
                if ((ev->EndTime() > now || ev == Event) && !strcmp(ev->Title(), Event->Title()))
                   Add(new cMenuScheduleItem(Timers, ev, ch, true), ev == Event && ch == Channel);
                }
            }
         }
     }
  return true;
}

bool cMenuSchedule::PrepareScheduleAllAll(const cTimers *Timers, const cSchedules *Schedules, const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(CHNUMWIDTH, CHNAMWIDTH, 7, 6, 4);
  SetTitle(tr("All events - all channels"));
  LOCK_CHANNELS_READ;
  cStateKey StateKey;
  for (const cChannel *ch = Channels->First(); ch; ch = Channels->Next(ch)) {
      if (const cSchedule *Schedule = Schedules->GetSchedule(ch)) {
         time_t now = time(NULL) - Setup.EPGLinger * 60;
         for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
             if (ev->EndTime() > now || ev == Event)
                Add(new cMenuScheduleItem(Timers, ev, ch, true), ev == Event && ch == Channel);
             }
         }
      }
  return true;
}

bool cMenuSchedule::Update(void)
{
  bool result = false;
  if (const cTimers *Timers = cTimers::GetTimersRead(timersStateKey)) {
     for (cOsdItem *item = First(); item; item = Next(item)) {
         if (((cMenuScheduleItem *)item)->Update(Timers))
            result = true;
         }
     timersStateKey.Remove();
     }
  return result;
}

void cMenuSchedule::SetHelpKeys(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  canSwitch = false;
  int NewHelpKeys = 0;
  if (item) {
     if (item->timerMatch == tmFull)
        NewHelpKeys |= 0x02; // "Timer"
     else
        NewHelpKeys |= 0x01; // "Record"
     LOCK_CHANNELS_READ;
     if (const cChannel *Channel = Channels->GetByChannelID(item->event->ChannelID(), true)) {
        if (Channel->Number() != cDevice::CurrentChannel()) {
           NewHelpKeys |= 0x10; // "Switch"
           canSwitch = true;
           }
        }
     }
  if (NewHelpKeys != helpKeys) {
     const char *Red[] = { NULL, tr("Button$Record"), tr("Button$Timer") };
     SetHelp(Red[NewHelpKeys & 0x03], tr("Button$Now"), tr("Button$Next"), canSwitch ? tr("Button$Switch") : NULL);
     helpKeys = NewHelpKeys;
     }
}

eOSState cMenuSchedule::Number(void)
{
  cMenuScheduleItem::IncSortMode();
  LOCK_TIMERS_READ;
  LOCK_CHANNELS_READ;
  Set(Timers, Channels, NULL, true);
  return osContinue;
}

eOSState cMenuSchedule::Record(void)
{
  if (cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current())) {
     LOCK_TIMERS_WRITE;
     LOCK_CHANNELS_READ;
     LOCK_SCHEDULES_READ;
     Timers->SetExplicitModify();
     if (item->timerMatch == tmFull) {
        if (cTimer *Timer = Timers->GetMatch(item->event))
           return AddSubMenu(new cMenuEditTimer(Timer));
        }
     cTimer *Timer = new cTimer(item->event);
     if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost)
        Timer->SetRemote(Setup.SVDRPDefaultHost);
     if (cTimer *t = Timers->GetTimer(Timer)) {
        delete Timer;
        Timer = t;
        return AddSubMenu(new cMenuEditTimer(Timer));
        }
     if (Timer->Matches(0, NEWTIMERLIMIT))
        return AddSubMenu(new cMenuEditTimer(Timer, true));
     Timers->Add(Timer);
     Timers->SetModified();
     if (!HandleRemoteModifications(Timer)) {
        // must add the timer before HandleRemoteModifications to get proper log messages with timer ids
        Timers->Del(Timer);
        }
     else if (Timer->Remote())
        Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
     if (HasSubMenu())
        CloseSubMenu();
     }
  if (Update())
     Display();
  SetHelpKeys();
  return osContinue;
}

eOSState cMenuSchedule::Switch(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     LOCK_CHANNELS_READ;
     const cChannel *Channel = NULL;
     if ((Channel = Channels->GetByChannelID(item->event->ChannelID(), true)) != NULL) {
        if (!Channels->SwitchTo(Channel->Number()))
           Channel = NULL;
        }
     if (Channel)
        return osEnd;
     }
  Skins.Message(mtError, tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuSchedule::ProcessKey(eKeys Key)
{
  if (!HasSubMenu()) {
     LOCK_TIMERS_READ;
     LOCK_CHANNELS_READ;
     Set(Timers, Channels); // react on any changes to the schedules list
     }
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (int(Key)) {
       case k0:      return Number();
       case kRecord:
       case kRed:    return Record();
       case kGreen:  {
                       LOCK_TIMERS_READ;
                       LOCK_CHANNELS_READ;
                       LOCK_SCHEDULES_READ;
                       if (!now && !next) {
                          int ChannelNr = 0;
                          if (Count()) {
                             if (const cChannel *Channel = Channels->GetByChannelID(((cMenuScheduleItem *)Get(Current()))->event->ChannelID(), true))
                                ChannelNr = Channel->Number();
                             }
                          now = true;
                          return AddSubMenu(new cMenuWhatsOn(Timers, Channels, Schedules, now, ChannelNr));
                          }
                       now = !now;
                       next = !next;
                       return AddSubMenu(new cMenuWhatsOn(Timers, Channels, Schedules, now, cMenuWhatsOn::CurrentChannel()));
                     }
       case kYellow: {
                       LOCK_TIMERS_READ;
                       LOCK_CHANNELS_READ;
                       LOCK_SCHEDULES_READ;
                       return AddSubMenu(new cMenuWhatsOn(Timers, Channels, Schedules, false, cMenuWhatsOn::CurrentChannel()));
                     }
       case kBlue:   if (canSwitch)
                        return Switch();
                     break;
       case kChanUp|k_Repeat:
       case kChanUp:
       case kChanDn|k_Repeat:
       case kChanDn: if (!HasSubMenu()) {
                        LOCK_TIMERS_READ;
                        LOCK_CHANNELS_READ;
                        if (const cChannel *Channel = Channels->GetByNumber(cDevice::CurrentChannel()))
                           Set(Timers, Channels, Channel, true);
                        }
                     break;
       case kInfo:
       case kOk:     if (Count()) {
                        LOCK_TIMERS_READ;
                        LOCK_CHANNELS_READ;
                        LOCK_SCHEDULES_READ;
                        return AddSubMenu(new cMenuEvent(Timers, Channels, ((cMenuScheduleItem *)Get(Current()))->event, canSwitch, true));
                        }
                     break;
       default:      break;
       }
     }
  else if (!HasSubMenu()) {
     now = next = false;
     if (const cEvent *ei = cMenuWhatsOn::ScheduleEvent()) {
        LOCK_TIMERS_READ;
        LOCK_CHANNELS_READ;
        if (const cChannel *Channel = Channels->GetByChannelID(ei->ChannelID(), true)) {
           cMenuScheduleItem::SetSortMode(cMenuScheduleItem::ssmAllThis);
           Set(Timers, Channels, Channel, true);
           }
        }
     else if (HadSubMenu && Update())
        Display();
     if (Key != kNone)
        SetHelpKeys();
     }
  return state;
}

// --- cMenuCommands ---------------------------------------------------------

cMenuCommands::cMenuCommands(const char *Title, cList<cNestedItem> *Commands, const char *Parameters)
:cOsdMenu(Title)
{
  SetMenuCategory(mcCommand);
  result = NULL;
  SetHasHotkeys();
  commands = Commands;
  parameters = Parameters;
  for (cNestedItem *Command = commands->First(); Command; Command = commands->Next(Command)) {
      const char *s = Command->Text();
      if (Command->SubItems())
         Add(new cOsdItem(hk(cString::sprintf("%s...", s))));
      else if (Parse(s))
         Add(new cOsdItem(hk(title)));
      }
}

cMenuCommands::~cMenuCommands()
{
  free(result);
}

bool cMenuCommands::Parse(const char *s)
{
  const char *p = strchr(s, ':');
  if (p) {
     int l = p - s;
     if (l > 0) {
        char t[l + 1];
        stripspace(strn0cpy(t, s, l + 1));
        l = strlen(t);
        if (l > 1 && t[l - 1] == '?') {
           t[l - 1] = 0;
           confirm = true;
           }
        else
           confirm = false;
        title = t;
        command = skipspace(p + 1);
        return true;
        }
     }
  return false;
}

eOSState cMenuCommands::Execute(void)
{
  cNestedItem *Command = commands->Get(Current());
  if (Command) {
     if (Command->SubItems())
        return AddSubMenu(new cMenuCommands(Title(), Command->SubItems(), parameters));
     if (Parse(Command->Text())) {
        if (!confirm || Interface->Confirm(cString::sprintf("%s?", *title))) {
           Skins.Message(mtStatus, cString::sprintf("%s...", *title));
           free(result);
           result = NULL;
           cString cmdbuf;
           if (!isempty(parameters))
              cmdbuf = cString::sprintf("%s %s", *command, *parameters);
           const char *cmd = *cmdbuf ? *cmdbuf : *command;
           dsyslog("executing command '%s'", cmd);
           cPipe p;
           if (p.Open(cmd, "r")) {
              int l = 0;
              int c;
              while ((c = fgetc(p)) != EOF) {
                    if (l % 20 == 0) {
                       if (char *NewBuffer = (char *)realloc(result, l + 21))
                          result = NewBuffer;
                       else {
                          esyslog("ERROR: out of memory");
                          break;
                          }
                       }
                    result[l++] = char(c);
                    }
              if (result)
                 result[l] = 0;
              p.Close();
              }
           else
              esyslog("ERROR: can't open pipe for command '%s'", cmd);
           Skins.Message(mtStatus, NULL);
           if (result)
              return AddSubMenu(new cMenuText(title, result, fontFix));
           return osEnd;
           }
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

static bool CamMenuIsOpen = false;

class cMenuCam : public cOsdMenu {
private:
  cCamSlot *camSlot;
  cCiMenu *ciMenu;
  cCiEnquiry *ciEnquiry;
  char *input;
  int offset;
  time_t lastCamExchange;
  void GenerateTitle(const char *s = NULL);
  void QueryCam(void);
  void AddMultiLineItem(const char *s);
  void Set(void);
  eOSState Select(void);
public:
  cMenuCam(cCamSlot *CamSlot);
  virtual ~cMenuCam() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuCam::cMenuCam(cCamSlot *CamSlot)
:cOsdMenu("", 1) // tab necessary for enquiry!
{
  SetMenuCategory(mcCam);
  camSlot = CamSlot;
  ciMenu = NULL;
  ciEnquiry = NULL;
  input = NULL;
  offset = 0;
  lastCamExchange = time(NULL);
  SetNeedsFastResponse(true);
  QueryCam();
  CamMenuIsOpen = true;
}

cMenuCam::~cMenuCam()
{
  if (ciMenu)
     ciMenu->Abort();
  delete ciMenu;
  if (ciEnquiry)
     ciEnquiry->Abort();
  delete ciEnquiry;
  free(input);
  CamMenuIsOpen = false;
}

void cMenuCam::GenerateTitle(const char *s)
{
  SetTitle(cString::sprintf("CAM %d - %s", camSlot->SlotNumber(), (s && *s) ? s : camSlot->GetCamName()));
}

void cMenuCam::QueryCam(void)
{
  delete ciMenu;
  ciMenu = NULL;
  delete ciEnquiry;
  ciEnquiry = NULL;
  if (camSlot->HasUserIO()) {
     ciMenu = camSlot->GetMenu();
     ciEnquiry = camSlot->GetEnquiry();
     }
  Set();
}

void cMenuCam::Set(void)
{
  if (ciMenu) {
     Clear();
     free(input);
     input = NULL;
     dsyslog("CAM %d: Menu ------------------", camSlot->SlotNumber());
     offset = 0;
     SetHasHotkeys(ciMenu->Selectable());
     GenerateTitle(ciMenu->TitleText());
     dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->TitleText());
     if (!isempty(ciMenu->SubTitleText())) {
        dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->SubTitleText());
        AddMultiLineItem(ciMenu->SubTitleText());
        offset = Count();
        }
     for (int i = 0; i < ciMenu->NumEntries(); i++) {
         Add(new cOsdItem(hk(ciMenu->Entry(i)), osUnknown, ciMenu->Selectable()));
         dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->Entry(i));
         }
     if (!isempty(ciMenu->BottomText())) {
        AddMultiLineItem(ciMenu->BottomText());
        dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->BottomText());
        }
     cRemote::TriggerLastActivity();
     }
  else if (ciEnquiry) {
     Clear();
     int Length = ciEnquiry->ExpectedLength();
     free(input);
     input = MALLOC(char, Length + 1);
     *input = 0;
     dsyslog("CAM %d: Enquiry ------------------", camSlot->SlotNumber());
     GenerateTitle();
     Add(new cOsdItem(ciEnquiry->Text(), osUnknown, false));
     dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciEnquiry->Text());
     Add(new cOsdItem("", osUnknown, false));
     Add(new cMenuEditNumItem("", input, Length, ciEnquiry->Blind()));
     }
  Display();
}

void cMenuCam::AddMultiLineItem(const char *s)
{
  while (s && *s) {
        const char *p = strchr(s, '\n');
        int l = p ? p - s : strlen(s);
        cOsdItem *item = new cOsdItem;
        item->SetSelectable(false);
        item->SetText(strndup(s, l), false);
        Add(item);
        s = p ? p + 1 : p;
        }
}

eOSState cMenuCam::Select(void)
{
  if (ciMenu) {
     if (ciMenu->Selectable()) {
        ciMenu->Select(Current() - offset);
        dsyslog("CAM %d: select %d", camSlot->SlotNumber(), Current() - offset);
        }
     else
        ciMenu->Cancel();
     }
  else if (ciEnquiry) {
     if (ciEnquiry->ExpectedLength() < 0xFF && int(strlen(input)) != ciEnquiry->ExpectedLength()) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), tr("Please enter %d digits!"), ciEnquiry->ExpectedLength());
        Skins.Message(mtError, buffer);
        return osContinue;
        }
     ciEnquiry->Reply(input);
     dsyslog("CAM %d: entered '%s'", camSlot->SlotNumber(), ciEnquiry->Blind() ? "****" : input);
     }
  QueryCam();
  return osContinue;
}

eOSState cMenuCam::ProcessKey(eKeys Key)
{
  if (!camSlot->HasMMI())
     return osBack;

  eOSState state = cOsdMenu::ProcessKey(Key);

  if (ciMenu || ciEnquiry) {
     lastCamExchange = time(NULL);
     if (state == osUnknown) {
        switch (Key) {
          case kOk: return Select();
          default: break;
          }
        }
     else if (state == osBack) {
        if (ciMenu)
           ciMenu->Cancel();
        if (ciEnquiry)
           ciEnquiry->Cancel();
        QueryCam();
        return osContinue;
        }
     if (ciMenu && ciMenu->HasUpdate()) {
        QueryCam();
        return osContinue;
        }
     }
  else if (time(NULL) - lastCamExchange < CAMRESPONSETIMEOUT)
     QueryCam();
  else {
     Skins.Message(mtError, tr("CAM not responding!"));
     return osBack;
     }
  return state;
}

// --- CamControl ------------------------------------------------------------

cOsdObject *CamControl(void)
{
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
      if (CamSlot->HasUserIO())
         return new cMenuCam(CamSlot);
      }
  return NULL;
}

bool CamMenuActive(void)
{
  return CamMenuIsOpen;
}

// --- cMenuPathEdit ---------------------------------------------------------

#define osUserRecRenamed osUser1
#define osUserRecMoved   osUser2
#define osUserRecRemoved osUser3
#define osUserRecEmpty   osUser4

class cMenuPathEdit : public cOsdMenu {
private:
  cString path;
  cString oldFolder;
  char folder[PATH_MAX];
  char name[NAME_MAX];
  cMenuEditStrItem *folderItem;
  int pathIsInUse;
  eOSState SetFolder(void);
  eOSState Folder(void);
  eOSState ApplyChanges(void);
public:
  cMenuPathEdit(const char *Path);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuPathEdit::cMenuPathEdit(const char *Path)
:cOsdMenu(tr("Edit path"), 12)
{
  SetMenuCategory(mcRecordingEdit);
  path = Path;
  *folder = 0;
  *name = 0;
  const char *s = strrchr(path, FOLDERDELIMCHAR);
  if (s) {
     strn0cpy(folder, cString(path, s), sizeof(folder));
     s++;
     }
  else
     s = path;
  strn0cpy(name, s, sizeof(name));
  {
    LOCK_RECORDINGS_READ;
    pathIsInUse = Recordings->PathIsInUse(path);
  }
  oldFolder = folder;
  cOsdItem *p;
  Add(p = folderItem = new cMenuEditStrItem(tr("Folder"), folder, sizeof(folder)));
  p->SetSelectable(!pathIsInUse);
  Add(p = new cMenuEditStrItem(tr("Name"), name, sizeof(name)));
  p->SetSelectable(!pathIsInUse);
  if (*path) {
     int DirSize = 0;
     {
       LOCK_RECORDINGS_READ;
       for (const cRecording *Recording = Recordings->First(); Recording; Recording = Recordings->Next(Recording)) {
           if (Recording->IsInPath(path)) {
              int FileSizeMB = Recording->FileSizeMB();
              if (FileSizeMB > 0 )
                 DirSize += FileSizeMB;
              }
           }
     }
     if (DirSize > 1023)
        Add(new cOsdItem(cString::sprintf("%s:\t%.2f GB", tr("Size"), DirSize / 1024.), osUnknown, false));
     else
        Add(new cOsdItem(cString::sprintf("%s:\t%d MB", tr("Size"), DirSize), osUnknown, false));
     }
  if (pathIsInUse) {
     Add(new cOsdItem("", osUnknown, false));
     Add(new cOsdItem(tr("This folder is currently in use - no changes are possible!"), osUnknown, false));
     }
  Display();
  if (!pathIsInUse)
     SetHelp(tr("Button$Folder"));
}

eOSState cMenuPathEdit::SetFolder(void)
{
  if (cMenuFolder *mf = dynamic_cast<cMenuFolder *>(SubMenu())) {
     strn0cpy(folder, mf->GetFolder(), sizeof(folder));
     SetCurrent(folderItem);
     Display();
     }
  return CloseSubMenu();
}

eOSState cMenuPathEdit::Folder(void)
{
  return AddSubMenu(new cMenuFolder(tr("Select folder"), &Folders, path));
}

eOSState cMenuPathEdit::ApplyChanges(void)
{
  if (!*name) {
     *name = ' '; // name must not be empty!
     name[1] = 0;
     }
  cString NewPath = *folder ? cString::sprintf("%s%c%s", folder, FOLDERDELIMCHAR, name) : name;
  NewPath.CompactChars(FOLDERDELIMCHAR);
  if (strcmp(NewPath, path)) {
     int NumRecordings = 0;
     {
       LOCK_RECORDINGS_READ;
       NumRecordings = Recordings->GetNumRecordingsInPath(path);
     }
     if (NumRecordings > 1 && !Interface->Confirm(cString::sprintf(tr("Move entire folder containing %d recordings?"), NumRecordings)))
        return osContinue;
     bool Error = false;
     {
       LOCK_RECORDINGS_WRITE;
       Recordings->SetExplicitModify();
       Error = !Recordings->MoveRecordings(path, NewPath);
       if (!Error)
          Recordings->SetModified();
     }
     if (Error) {
        Skins.Message(mtError, tr("Error while moving folder!"));
        return osContinue;
        }
     if (strcmp(folder, oldFolder))
        return osUserRecMoved;
     return osUserRecRenamed;
     }
  return osBack;
}

eOSState cMenuPathEdit::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);
  if (state == osUnknown) {
     if (!pathIsInUse) {
        switch (Key) {
          case kRed: return Folder();
          case kOk:  return ApplyChanges();
          default: break;
          }
        }
     else if (Key == kOk)
        return osBack;
     }
  else if (state == osEnd && HasSubMenu())
     state = SetFolder();
  return state;
}

// --- cMenuRecordingEdit ----------------------------------------------------

class cMenuRecordingEdit : public cOsdMenu {
private:
  const cRecording *recording;
  cString originalFileName;
  cStateKey recordingsStateKey;
  char folder[PATH_MAX];
  char name[NAME_MAX];
  int priority;
  int lifetime;
  cMenuEditStrItem *folderItem;
  cMenuEditStrItem *nameItem;
  const char *buttonFolder;
  const char *buttonAction;
  const char *buttonDeleteMarks;
  const char *actionCancel;
  const char *doCut;
  int recordingIsInUse;
  void Set(void);
  void SetHelpKeys(void);
  bool RefreshRecording(void);
  eOSState SetFolder(void);
  eOSState Folder(void);
  eOSState Action(void);
  eOSState RemoveName(void);
  eOSState DeleteMarks(void);
  eOSState ApplyChanges(void);
public:
  cMenuRecordingEdit(const cRecording *Recording);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuRecordingEdit::cMenuRecordingEdit(const cRecording *Recording)
:cOsdMenu(tr("Edit recording"), 12)
{
  SetMenuCategory(mcRecordingEdit);
  recording = Recording;
  originalFileName = recording->FileName();
  strn0cpy(folder, recording->Folder(), sizeof(folder));
  strn0cpy(name, recording->BaseName(), sizeof(name));
  priority = recording->Priority();
  lifetime = recording->Lifetime();
  folderItem = NULL;
  nameItem = NULL;
  buttonFolder = NULL;
  buttonAction = NULL;
  buttonDeleteMarks = NULL;
  actionCancel = NULL;
  doCut = NULL;
  recordingIsInUse = ruNone;
  Set();
}

void cMenuRecordingEdit::Set(void)
{
  int current = Current();
  Clear();
  recordingIsInUse = recording->IsInUse();
  cOsdItem *p;
  Add(p = folderItem = new cMenuEditStrItem(tr("Folder"), folder, sizeof(folder)));
  p->SetSelectable(!recordingIsInUse);
  Add(p = nameItem = new cMenuEditStrItem(tr("Name"), name, sizeof(name)));
  p->SetSelectable(!recordingIsInUse);
  Add(p = new cMenuEditIntItem(tr("Priority"), &priority, 0, MAXPRIORITY));
  p->SetSelectable(!recordingIsInUse);
  Add(p = new cMenuEditIntItem(tr("Lifetime"), &lifetime, 0, MAXLIFETIME));
  p->SetSelectable(!recordingIsInUse);
  if (recordingIsInUse) {
     Add(new cOsdItem("", osUnknown, false));
     Add(new cOsdItem(tr("This recording is currently in use - no changes are possible!"), osUnknown, false));
     }
  SetCurrent(Get(current));
  Display();
  SetHelpKeys();
}

void cMenuRecordingEdit::SetHelpKeys(void)
{
  buttonFolder = !recordingIsInUse ? tr("Button$Folder") : NULL;
  buttonAction = NULL;
  buttonDeleteMarks = NULL;
  actionCancel = NULL;
  doCut = NULL;
  if ((recordingIsInUse & ruCut) != 0)
     buttonAction = actionCancel = ((recordingIsInUse & ruPending) != 0) ? tr("Button$Cancel cutting") : tr("Button$Stop cutting");
  else if ((recordingIsInUse & ruMove) != 0)
     buttonAction = actionCancel = ((recordingIsInUse & ruPending) != 0) ? tr("Button$Cancel moving") : tr("Button$Stop moving");
  else if ((recordingIsInUse & ruCopy) != 0)
     buttonAction = actionCancel = ((recordingIsInUse & ruPending) != 0) ? tr("Button$Cancel copying") : tr("Button$Stop copying");
  else if (recording->HasMarks()) {
     buttonAction = doCut = tr("Button$Cut");
     buttonDeleteMarks = tr("Button$Delete marks");
     }
  SetHelp(buttonFolder, buttonAction, buttonDeleteMarks);
}

bool cMenuRecordingEdit::RefreshRecording(void)
{
  if (const cRecordings *Recordings = cRecordings::GetRecordingsRead(recordingsStateKey)) {
     if ((recording = Recordings->GetByName(originalFileName)) != NULL)
        Set();
     else {
        recordingsStateKey.Remove();
        Skins.Message(mtWarning, tr("Recording vanished!"));
        return false;
        }
     recordingsStateKey.Remove();
     }
  return true;
}

eOSState cMenuRecordingEdit::SetFolder(void)
{
  if (cMenuFolder *mf = dynamic_cast<cMenuFolder *>(SubMenu())) {
     strn0cpy(folder, mf->GetFolder(), sizeof(folder));
     SetCurrent(folderItem);
     Display();
     }
  return CloseSubMenu();
}

eOSState cMenuRecordingEdit::Folder(void)
{
  return AddSubMenu(new cMenuFolder(tr("Select folder"), &Folders, recording->Name()));
}

eOSState cMenuRecordingEdit::Action(void)
{
  if (actionCancel)
     RecordingsHandler.Del(recording->FileName());
  else if (doCut) {
     if (access(cCutter::EditedFileName(recording->FileName()), F_OK) != 0 || Interface->Confirm(tr("Edited version already exists - overwrite?"))) {
        if (!EnoughFreeDiskSpaceForEdit(recording->FileName()))
           Skins.Message(mtError, tr("Not enough free disk space to start editing process!"));
        else if (!RecordingsHandler.Add(ruCut, recording->FileName()))
           Skins.Message(mtError, tr("Error while queueing recording for cutting!"));
        }
     }
  recordingIsInUse = recording->IsInUse();
  RefreshRecording();
  SetHelpKeys();
  return osContinue;
}

eOSState cMenuRecordingEdit::RemoveName(void)
{
  if (Get(Current()) == nameItem) {
     if (Interface->Confirm(tr("Rename recording to folder name?"))) {
        char *s = strrchr(folder, FOLDERDELIMCHAR);
        if (s)
           *s++ = 0;
        else
           s = folder;
        strn0cpy(name, s, sizeof(name));
        if (s == folder)
           *s = 0;
        Set();
        }
     }
  return osContinue;
}

eOSState cMenuRecordingEdit::DeleteMarks(void)
{
  if (buttonDeleteMarks && Interface->Confirm(tr("Delete editing marks for this recording?"))) {
     if (cMarks::DeleteMarksFile(recording)) {
        SetHelpKeys();
        cMutexLock ControlMutexLock;
        if (cControl *Control = cControl::Control(ControlMutexLock, true)) {
           if (const cRecording *Recording = Control->GetRecording()) {
              if (strcmp(recording->FileName(), Recording->FileName()) == 0)
                 Control->ClearEditingMarks();
              }
           }
        }
     else
        Skins.Message(mtError, tr("Error while deleting editing marks!"));
     }
  return osContinue;
}

eOSState cMenuRecordingEdit::ApplyChanges(void)
{
  cStateKey StateKey;
  cRecordings *Recordings = cRecordings::GetRecordingsWrite(StateKey);
  cRecording *Recording = Recordings->GetByName(recording->FileName());
  if (!Recording) {
     StateKey.Remove(false);
     Skins.Message(mtWarning, tr("Recording vanished!"));
     return osBack;
     }
  bool Modified = false;
  if (priority != recording->Priority() || lifetime != recording->Lifetime()) {
     if (!Recording->ChangePriorityLifetime(priority, lifetime)) {
        StateKey.Remove(Modified);
        Skins.Message(mtError, tr("Error while changing priority/lifetime!"));
        return osContinue;
        }
     Modified = true;
     }
  if (!*name) {
     *name = ' '; // name must not be empty!
     name[1] = 0;
     }
  cString OldFolder = Recording->Folder();
  cString NewName = *folder ? cString::sprintf("%s%c%s", folder, FOLDERDELIMCHAR, name) : name;
  NewName.CompactChars(FOLDERDELIMCHAR);
  if (strcmp(NewName, Recording->Name())) {
     if (!Recording->ChangeName(NewName)) {
        StateKey.Remove(Modified);
        Skins.Message(mtError, tr("Error while changing folder/name!"));
        return osContinue;
        }
     Modified = true;
     }
  if (Modified) {
     eOSState  state = osUserRecRenamed;
     if (strcmp(Recording->Folder(), OldFolder))
        state = osUserRecMoved;
     Recordings->TouchUpdate();
     StateKey.Remove(Modified);
     return state;
     }
  StateKey.Remove(Modified);
  return osBack;
}

eOSState cMenuRecordingEdit::ProcessKey(eKeys Key)
{
  if (!HasSubMenu()) {
     if (!RefreshRecording())
        return osBack; // the recording has vanished, so close this menu
     }
  eOSState state = cOsdMenu::ProcessKey(Key);
  if (state == osUnknown) {
     switch (Key) {
       case k0:      return RemoveName();
       case kRed:    return buttonFolder ? Folder() : osContinue;
       case kGreen:  return buttonAction ? Action() : osContinue;
       case kYellow: return buttonDeleteMarks ? DeleteMarks() : osContinue;
       case kOk:     return !recordingIsInUse ? ApplyChanges() : osBack;
       default: break;
       }
     }
  else if (state == osEnd && HasSubMenu())
     state = SetFolder();
  return state;
}

// --- cMenuRecording --------------------------------------------------------

class cMenuRecording : public cOsdMenu {
private:
  const cRecording *recording;
  cString originalFileName;
  cStateKey recordingsStateKey;
  bool withButtons;
  bool RefreshRecording(void);
public:
  cMenuRecording(const cRecording *Recording, bool WithButtons = false);
  virtual void Display(void) override;
  virtual eOSState ProcessKey(eKeys Key) override;
};

cMenuRecording::cMenuRecording(const cRecording *Recording, bool WithButtons)
:cOsdMenu(tr("Recording info"))
{
  SetMenuCategory(mcRecordingInfo);
  if (cRecordings::GetRecordingsRead(recordingsStateKey)) // initializes recordingsStateKey, so we don't call Display() unnecessarily
     recordingsStateKey.Remove();
  recording = Recording;
  originalFileName = recording->FileName();
  withButtons = WithButtons;
  if (withButtons)
     SetHelp(tr("Button$Play"), tr("Button$Rewind"), NULL, tr("Button$Edit"));
}

bool cMenuRecording::RefreshRecording(void)
{
  if (const cRecordings *Recordings = cRecordings::GetRecordingsRead(recordingsStateKey)) {
     if ((recording = Recordings->GetByName(originalFileName)) != NULL)
        Display();
     else {
        recordingsStateKey.Remove();
        Skins.Message(mtWarning, tr("Recording vanished!"));
        return false;
        }
     recordingsStateKey.Remove();
     }
  return true;
}

void cMenuRecording::Display(void)
{
  if (HasSubMenu()) {
     SubMenu()->Display();
     return;
     }
  cOsdMenu::Display();
  DisplayMenu()->SetRecording(recording);
  if (recording->Info()->Description())
     cStatus::MsgOsdTextItem(recording->Info()->Description());
}

eOSState cMenuRecording::ProcessKey(eKeys Key)
{
  if (HasSubMenu())
     return cOsdMenu::ProcessKey(Key);
  else if (!RefreshRecording())
     return osBack; // the recording has vanished, so close this menu
  switch (int(Key)) {
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
                  DisplayMenu()->Scroll(NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft, NORMALKEY(Key) == kLeft || NORMALKEY(Key) == kRight);
                  cStatus::MsgOsdTextItem(NULL, NORMALKEY(Key) == kUp || NORMALKEY(Key) == kLeft);
                  return osContinue;
    case kInfo:   return osBack;
    default: break;
    }

  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRed:    if (withButtons)
                        Key = kOk; // will play the recording, even if recording commands are defined
       case kGreen:  if (!withButtons)
                        break;
                     cRemote::Put(Key, true);
                     // continue with osBack to close the info menu and process the key
       case kOk:     return osBack;
       case kBlue:   if (withButtons)
                        return AddSubMenu(new cMenuRecordingEdit(recording));
                     break;
       default: break;
       }
     }
  return state;
}

// --- cMenuRecordingItem ----------------------------------------------------

class cMenuRecordingItem : public cOsdItem {
private:
  const cRecording *recording;
  int level;
  char *name;
  int totalEntries, newEntries;
public:
  cMenuRecordingItem(const cRecording *Recording, int Level);
  ~cMenuRecordingItem();
  void IncrementCounter(bool New);
  const char *Name(void) const { return name; }
  int Level(void) const { return level; }
  const cRecording *Recording(void) const { return recording; }
  bool IsDirectory(void) const { return name != NULL; }
  void SetRecording(const cRecording *Recording) { recording = Recording; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable) override;
  };

cMenuRecordingItem::cMenuRecordingItem(const cRecording *Recording, int Level)
{
  recording = Recording;
  level = Level;
  name = NULL;
  totalEntries = newEntries = 0;
  SetText(Recording->Title('\t', true, Level));
  if (*Text() == '\t') // this is a folder
     name = strdup(Text() + 2); // 'Text() + 2' to skip the two '\t'
  else { // this is an actual recording
     int Usage = Recording->IsInUse();
     if ((Usage & ruDst) != 0 && (Usage & (ruMove | ruCopy)) != 0)
        SetSelectable(false);
     }
}

cMenuRecordingItem::~cMenuRecordingItem()
{
  free(name);
}

void cMenuRecordingItem::IncrementCounter(bool New)
{
  totalEntries++;
  if (New)
     newEntries++;
  SetText(cString::sprintf("%d\t\t%d\t%s", totalEntries, newEntries, name));
}

void cMenuRecordingItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  LOCK_RECORDINGS_READ;
  if (!DisplayMenu->SetItemRecording(recording, Index, Current, Selectable, level, totalEntries, newEntries))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuRecordings -------------------------------------------------------

cString cMenuRecordings::path;
cString cMenuRecordings::fileName;

cMenuRecordings::cMenuRecordings(const char *Base, int Level, bool OpenSubMenus, const cRecordingFilter *Filter)
:cOsdMenu(Base ? Base : tr("Recordings"), 9, 6, 6)
{
  SetMenuCategory(mcRecording);
  base = Base ? strdup(Base) : NULL;
  level = Setup.RecordingDirs ? Level : -1;
  filter = Filter;
  helpKeys = -1;
  Display(); // this keeps the higher level menus from showing up briefly when pressing 'Back' during replay
  Set();
  if (Current() < 0)
     SetCurrent(First());
  else if (OpenSubMenus && (cReplayControl::LastReplayed() || *path || *fileName)) {
     if (!*path || Level < strcountchr(path, FOLDERDELIMCHAR)) {
        if (Open(true))
           return;
        }
     }
  SetHelpKeys();
}

cMenuRecordings::~cMenuRecordings()
{
  if (cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current())) {
     if (!ri->IsDirectory())
        SetRecording(ri->Recording()->FileName());
     }
  free(base);
}

void cMenuRecordings::SetHelpKeys(void)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  int NewHelpKeys = 0;
  if (ri) {
     if (ri->IsDirectory())
        NewHelpKeys = 1;
     else
        NewHelpKeys = 2;
     }
  if (NewHelpKeys != helpKeys) {
     switch (NewHelpKeys) {
       case 0: SetHelp(NULL); break;
       case 1: SetHelp(tr("Button$Open"), NULL, NULL, tr("Button$Edit")); break;
       case 2: SetHelp(RecordingCommands.Count() ? tr("Commands") : tr("Button$Play"), tr("Button$Rewind"), tr("Button$Delete"), tr("Button$Info"));
       default: ;
       }
     helpKeys = NewHelpKeys;
     }
}

void cMenuRecordings::Set(bool Refresh)
{
  if (cRecordings::GetRecordingsRead(recordingsStateKey)) {
     recordingsStateKey.Remove();
     cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey); // write access is necessary for sorting!
     const char *CurrentRecording = NULL;
     if (cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current()))
        CurrentRecording = ri->Recording()->FileName();
     if (!CurrentRecording)
        CurrentRecording = *fileName ? *fileName : cReplayControl::LastReplayed();
     int current = Current();
     Clear();
     GetRecordingsSortMode(DirectoryName());
     Recordings->Sort();
     cMenuRecordingItem *CurrentItem = NULL;
     cMenuRecordingItem *LastItem = NULL;
     for (const cRecording *Recording = Recordings->First(); Recording; Recording = Recordings->Next(Recording)) {
         if ((!filter || filter->Filter(Recording)) && (!base || (strstr(Recording->Name(), base) == Recording->Name() && Recording->Name()[strlen(base)] == FOLDERDELIMCHAR))) {
            cMenuRecordingItem *Item = new cMenuRecordingItem(Recording, level);
            cMenuRecordingItem *LastDir = NULL;
            if (Item->IsDirectory()) {
               // Sorting may ignore non-alphanumeric characters, so we need to explicitly handle directories in case they only differ in such characters:
               for (cMenuRecordingItem *p = LastItem; p; p = dynamic_cast<cMenuRecordingItem *>(p->Prev())) {
                   if (p->Name() && strcmp(p->Name(), Item->Name()) == 0) {
                      LastDir = p;
                      break;
                      }
                   }
               }
            if (*Item->Text() && !LastDir) {
               Add(Item);
               LastItem = Item;
               if (Item->IsDirectory())
                  LastDir = Item;
               }
            else
               delete Item;
            if (LastItem || LastDir) {
               if (*path) {
                  if (strcmp(path, Recording->Folder()) == 0)
                     CurrentItem = LastDir ? LastDir : LastItem;
                  }
               else if (CurrentRecording && strcmp(CurrentRecording, Recording->FileName()) == 0)
                  CurrentItem = LastDir ? LastDir : LastItem;
               }
            if (LastDir)
               LastDir->IncrementCounter(Recording->IsNew());
            }
         }
     SetCurrent(CurrentItem);
     if (Current() < 0)
        SetCurrent(Get(current)); // last resort, in case the recording was deleted
     SetMenuSortMode(RecordingsSortMode == rsmName ? msmName : msmTime);
     recordingsStateKey.Remove(false); // sorting doesn't count as a real modification
     if (Refresh)
        Display();
     }
}

void cMenuRecordings::SetRecording(const char *FileName)
{
  fileName = FileName;
}

cString cMenuRecordings::DirectoryName(void)
{
  cString d(cVideoDirectory::Name());
  if (base) {
     char *s = ExchangeChars(strdup(base), true);
     d = AddDirectory(d, s);
     free(s);
     }
  return d;
}

bool cMenuRecordings::Open(bool OpenSubMenus)
{
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && ri->IsDirectory() && (!*path || strcountchr(path, FOLDERDELIMCHAR) > 0)) {
     const char *t = ri->Name();
     cString buffer;
     if (base) {
        buffer = cString::sprintf("%s%c%s", base, FOLDERDELIMCHAR, t);
        t = buffer;
        }
     AddSubMenu(new cMenuRecordings(t, level + 1, OpenSubMenus, filter));
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
        cReplayControl::SetRecording(ri->Recording()->FileName());
        return osReplay;
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
     cResumeFile ResumeFile(ri->Recording()->FileName(), ri->Recording()->IsPesRecording());
     ResumeFile.Delete();
     return Play();
     }
  return osContinue;
}

static bool TimerStillRecording(const char *FileName)
{
  if (cRecordControl *rc = cRecordControls::GetRecordControl(FileName)) {
     // local timer
     if (Interface->Confirm(tr("Timer still recording - really delete?"))) {
        LOCK_TIMERS_WRITE;
        if (cTimer *Timer = rc->Timer()) {
           Timer->Skip();
           cRecordControls::Process(Timers, time(NULL));
           if (Timer->IsSingleEvent()) {
              Timers->Del(Timer);
              isyslog("deleted timer %s", *Timer->ToDescr());
              }
           }
        }
     else
        return true; // user didn't confirm deletion
     }
  else {
     // remote timer
     cString TimerId = GetRecordingTimerId(FileName);
     if (*TimerId) {
        int Id;
        char *RemoteBuf = NULL;
        cString Remote;
        if (2 == sscanf(TimerId, "%d@%m[^ \n]", &Id, &RemoteBuf) && Id != 0) {
           Remote = RemoteBuf;
           free(RemoteBuf);
           if (Interface->Confirm(tr("Timer still recording - really delete?"))) {
              LOCK_TIMERS_WRITE;
              if (cTimer *Timer = Timers->GetById(Id, Remote)) {
                 cTimer OldTimer = *Timer;
                 Timer->Skip();
                 Timers->SetSyncStateKey(StateKeySVDRPRemoteTimersPoll);
                 if (Timer->IsSingleEvent()) {
                    if (HandleRemoteModifications(NULL, Timer))
                       Timers->Del(Timer);
                    else
                       return true; // error while deleting remote timer
                    }
                 else if (!HandleRemoteModifications(Timer, &OldTimer))
                    return true; // error while modifying remote timer
                 }
              }
           else
              return true; // user didn't confirm deletion
           }
        }
     }
  return false;
}

eOSState cMenuRecordings::Delete(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     if (Interface->Confirm(tr("Delete recording?"))) {
        if (TimerStillRecording(ri->Recording()->FileName()))
           return osContinue;
        cString FileName;
        {
          LOCK_RECORDINGS_READ;
          if (const cRecording *Recording = Recordings->GetByName(ri->Recording()->FileName())) {
             FileName = Recording->FileName();
             if (RecordingsHandler.GetUsage(FileName)) {
                if (!Interface->Confirm(tr("Recording is being edited - really delete?")))
                   return osContinue;
                SetNeedsFastResponse(true); // makes sure the edited version is removed from the menu ASAP
                }
             }
          else
             return osContinue; // recording has already been deleted
        }
        RecordingsHandler.Del(FileName); // must do this w/o holding a lock, because the cleanup section in cDirCopier::Action() might request one!
        if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), FileName) == 0)
           cControl::Shutdown();
        cRecordings *Recordings = cRecordings::GetRecordingsWrite(recordingsStateKey);
        Recordings->SetExplicitModify();
        cRecording *Recording = Recordings->GetByName(FileName);
        if (!Recording || Recording->Delete()) {
           cReplayControl::ClearLastReplayed(FileName);
           Recordings->DelByName(FileName);
           cOsdMenu::Del(Current());
           SetHelpKeys();
           cVideoDiskUsage::ForceCheck();
           Recordings->SetModified();
           recordingsStateKey.Remove();
           Display();
           if (!Count())
              return osUserRecEmpty;
           return osUserRecRemoved;
           }
        else
           Skins.Message(mtError, tr("Error while deleting recording!"));
        recordingsStateKey.Remove();
        }
     }
  return osContinue;
}

eOSState cMenuRecordings::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  if (cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current())) {
     if (ri->IsDirectory())
        return AddSubMenu(new cMenuPathEdit(cString(ri->Recording()->Name(), strchrn(ri->Recording()->Name(), FOLDERDELIMCHAR, ri->Level() + 1))));
     else
        return AddSubMenu(new cMenuRecording(ri->Recording(), true));
     }
  return osContinue;
}

eOSState cMenuRecordings::Commands(eKeys Key)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     cMenuCommands *menu;
     eOSState state = AddSubMenu(menu = new cMenuCommands(tr("Recording commands"), &RecordingCommands, cString::sprintf("\"%s\"", *strescape(ri->Recording()->FileName(), "\\\"$"))));
     if (Key != kNone)
        state = menu->ProcessKey(Key);
     return state;
     }
  return osContinue;
}

eOSState cMenuRecordings::Sort(void)
{
  if (HasSubMenu())
     return osContinue;
  if (const cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current()))
     SetRecording(ri->Recording()->FileName()); // makes sure the Recordings menu will reposition to the current recording
  IncRecordingsSortMode(DirectoryName());
  recordingsStateKey.Reset();
  Set(true);
  return osContinue;
}

eOSState cMenuRecordings::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kPlayPause:
       case kPlay:
       case kOk:     return Play();
       case kRed:    return (helpKeys > 1 && RecordingCommands.Count()) ? Commands() : Play();
       case kGreen:  return Rewind();
       case kYellow: return Delete();
       case kInfo:
       case kBlue:   return Info();
       case k0:      return Sort();
       case k1...k9: return Commands(Key);
       default: break;
       }
     }
  else if (state == osUserRecRenamed) {
     // a recording was renamed (within the same folder), so let's refresh the menu
     CloseSubMenu(false); // this is the cMenuRecordingEdit/cMenuPathEdit
     path = NULL;
     fileName = NULL;
     state = osContinue;
     }
  else if (state == osUserRecMoved) {
     // a recording was moved to a different folder, so let's delete the old item
     CloseSubMenu(false); // this is the cMenuRecordingEdit/cMenuPathEdit
     path = NULL;
     fileName = NULL;
     cOsdMenu::Del(Current());
     Set(); // the recording might have been moved into a new subfolder of this folder
     if (!Count())
        return osUserRecEmpty;
     Display();
     state = osUserRecRemoved;
     }
  else if (state == osUserRecRemoved) {
     // a recording was removed from a sub folder, so update the current item
     if (cOsdMenu *m = SubMenu()) {
        if (cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current())) {
           if (cMenuRecordingItem *riSub = (cMenuRecordingItem *)m->Get(m->Current()))
              ri->SetRecording(riSub->Recording());
           }
        }
     // no state change here, this report goes upstream!
     }
  else if (state == osUserRecEmpty) {
     // a subfolder became empty, so let's go back up
     CloseSubMenu(false); // this is the now empty submenu
     cOsdMenu::Del(Current()); // the menu entry of the now empty subfolder
     Set(); // in case a recording was moved into a new subfolder of this folder
     if (base && !Count()) // base: don't go up beyond the top level Recordings menu
        return state;
     Display();
     state = osContinue;
     }
  if (!HasSubMenu()) {
     Set(true);
     if (Key != kNone)
        SetHelpKeys();
     }
  return state;
}

// --- cMenuSetupBase --------------------------------------------------------

class cMenuSetupBase : public cMenuSetupPage {
protected:
  cSetup data;
  virtual void Store(void) override;
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
  cOsdProvider::UpdateOsdSize(true);
  Setup.Save();
}

// --- cMenuSetupOSD ---------------------------------------------------------

class cMenuSetupOSD : public cMenuSetupBase {
private:
  const char *useSmallFontTexts[3];
  const char *recSortModeTexts[2];
  const char *recSortDirTexts[2];
  const char *keyColorTexts[4];
  int osdLanguageIndex;
  int numSkins;
  int originalSkinIndex;
  int skinIndex;
  const char **skinDescriptions;
  cThemes themes;
  int originalThemeIndex;
  int themeIndex;
  cStringList fontOsdNames, fontSmlNames, fontFixNames;
  int fontOsdIndex, fontSmlIndex, fontFixIndex;
  virtual void Set(void);
public:
  cMenuSetupOSD(void);
  virtual ~cMenuSetupOSD() override;
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupOSD::cMenuSetupOSD(void)
{
  SetMenuCategory(mcSetupOsd);
  osdLanguageIndex = I18nCurrentLanguage();
  numSkins = Skins.Count();
  skinIndex = originalSkinIndex = Skins.Current()->Index();
  skinDescriptions = new const char*[numSkins];
  themes.Load(Skins.Current()->Name());
  themeIndex = originalThemeIndex = Skins.Current()->Theme() ? themes.GetThemeIndex(Skins.Current()->Theme()->Description()) : 0;
  cFont::GetAvailableFontNames(&fontOsdNames);
  cFont::GetAvailableFontNames(&fontSmlNames);
  cFont::GetAvailableFontNames(&fontFixNames, true);
  fontOsdNames.Insert(strdup(DefaultFontOsd));
  fontSmlNames.Insert(strdup(DefaultFontSml));
  fontFixNames.Insert(strdup(DefaultFontFix));
  fontOsdIndex = max(0, fontOsdNames.Find(Setup.FontOsd));
  fontSmlIndex = max(0, fontSmlNames.Find(Setup.FontSml));
  fontFixIndex = max(0, fontFixNames.Find(Setup.FontFix));
  Set();
}

cMenuSetupOSD::~cMenuSetupOSD()
{
  delete[] skinDescriptions;
}

void cMenuSetupOSD::Set(void)
{
  int current = Current();
  for (cSkin *Skin = Skins.First(); Skin; Skin = Skins.Next(Skin))
      skinDescriptions[Skin->Index()] = Skin->Description();
  useSmallFontTexts[0] = tr("never");
  useSmallFontTexts[1] = tr("skin dependent");
  useSmallFontTexts[2] = tr("always");
  recSortModeTexts[0] = tr("by name");
  recSortModeTexts[1] = tr("by time");
  recSortDirTexts[0] = tr("ascending");
  recSortDirTexts[1] = tr("descending");
  keyColorTexts[0] = tr("Key$Red");
  keyColorTexts[1] = tr("Key$Green");
  keyColorTexts[2] = tr("Key$Yellow");
  keyColorTexts[3] = tr("Key$Blue");
  Clear();
  SetSection(tr("OSD"));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Language"),               &osdLanguageIndex, I18nNumLanguagesWithLocale(), &I18nLanguages()->At(0)));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Skin"),                   &skinIndex, numSkins, skinDescriptions));
  if (themes.NumThemes())
  Add(new cMenuEditStraItem(tr("Setup.OSD$Theme"),                  &themeIndex, themes.NumThemes(), themes.Descriptions()));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Left (%)"),               &data.OSDLeftP, 0.0, 0.5));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Top (%)"),                &data.OSDTopP, 0.0, 0.5));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Width (%)"),              &data.OSDWidthP, 0.5, 1.0));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Height (%)"),             &data.OSDHeightP, 0.5, 1.0));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Message time (s)"),       &data.OSDMessageTime, 1, 60));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Use small font"),         &data.UseSmallFont, 3, useSmallFontTexts));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Anti-alias"),             &data.AntiAlias));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Default font"),           &fontOsdIndex, fontOsdNames.Size(), &fontOsdNames[0]));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Small font"),             &fontSmlIndex, fontSmlNames.Size(), &fontSmlNames[0]));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Fixed font"),             &fontFixIndex, fontFixNames.Size(), &fontFixNames[0]));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Default font size (%)"),  &data.FontOsdSizeP, 0.01, 0.1, 1));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Small font size (%)"),    &data.FontSmlSizeP, 0.01, 0.1, 1));
  Add(new cMenuEditPrcItem( tr("Setup.OSD$Fixed font size (%)"),    &data.FontFixSizeP, 0.01, 0.1, 1));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Channel info position"),  &data.ChannelInfoPos, tr("bottom"), tr("top")));
  Add(new cMenuEditIntItem( tr("Setup.OSD$Channel info time (s)"),  &data.ChannelInfoTime, 1, 60));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Info on channel switch"), &data.ShowInfoOnChSwitch));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Timeout requested channel info"), &data.TimeoutRequChInfo));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Scroll pages"),           &data.MenuScrollPage));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Scroll wraps"),           &data.MenuScrollWrap));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Menu key closes"),        &data.MenuKeyCloses));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Recording directories"),  &data.RecordingDirs));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Folders in timer menu"),  &data.FoldersInTimerMenu));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Always sort folders first"), &data.AlwaysSortFoldersFirst));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Default sort mode for recordings"), &data.DefaultSortModeRec, 2, recSortModeTexts));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Sorting direction for recordings"), &data.RecSortingDirection, 2, recSortDirTexts));
  Add(new cMenuEditBoolItem(tr("Setup.OSD$Number keys for characters"), &data.NumberKeysForChars));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Color key 0"),            &data.ColorKey0, 4, keyColorTexts));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Color key 1"),            &data.ColorKey1, 4, keyColorTexts));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Color key 2"),            &data.ColorKey2, 4, keyColorTexts));
  Add(new cMenuEditStraItem(tr("Setup.OSD$Color key 3"),            &data.ColorKey3, 4, keyColorTexts));
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupOSD::ProcessKey(eKeys Key)
{
  bool ModifiedAppearance = false;

  if (Key == kOk) {
     I18nSetLocale(data.OSDLanguage);
     if (skinIndex != originalSkinIndex) {
        cSkin *Skin = Skins.Get(skinIndex);
        if (Skin) {
           Utf8Strn0Cpy(data.OSDSkin, Skin->Name(), sizeof(data.OSDSkin));
           Skins.SetCurrent(Skin->Name());
           ModifiedAppearance = true;
           }
        }
     if (themes.NumThemes() && Skins.Current()->Theme()) {
        Skins.Current()->Theme()->Load(themes.FileName(themeIndex));
        Utf8Strn0Cpy(data.OSDTheme, themes.Name(themeIndex), sizeof(data.OSDTheme));
        ModifiedAppearance |= themeIndex != originalThemeIndex;
        }
     if (!(DoubleEqual(data.OSDLeftP, Setup.OSDLeftP) && DoubleEqual(data.OSDTopP, Setup.OSDTopP) && DoubleEqual(data.OSDWidthP, Setup.OSDWidthP) && DoubleEqual(data.OSDHeightP, Setup.OSDHeightP)))
        ModifiedAppearance = true;
     if (data.UseSmallFont != Setup.UseSmallFont || data.AntiAlias != Setup.AntiAlias)
        ModifiedAppearance = true;
     Utf8Strn0Cpy(data.FontOsd, fontOsdNames[fontOsdIndex], sizeof(data.FontOsd));
     Utf8Strn0Cpy(data.FontSml, fontSmlNames[fontSmlIndex], sizeof(data.FontSml));
     Utf8Strn0Cpy(data.FontFix, fontFixNames[fontFixIndex], sizeof(data.FontFix));
     if (strcmp(data.FontOsd, Setup.FontOsd) || !DoubleEqual(data.FontOsdSizeP, Setup.FontOsdSizeP))
        ModifiedAppearance = true;
     if (strcmp(data.FontSml, Setup.FontSml) || !DoubleEqual(data.FontSmlSizeP, Setup.FontSmlSizeP))
        ModifiedAppearance = true;
     if (strcmp(data.FontFix, Setup.FontFix) || !DoubleEqual(data.FontFixSizeP, Setup.FontFixSizeP))
        ModifiedAppearance = true;
     if (data.AlwaysSortFoldersFirst != Setup.AlwaysSortFoldersFirst || data.RecordingDirs != Setup.RecordingDirs || data.RecSortingDirection != Setup.RecSortingDirection) {
        LOCK_RECORDINGS_WRITE;
        Recordings->ClearSortNames();
        }
     }

  int oldSkinIndex = skinIndex;
  int oldOsdLanguageIndex = osdLanguageIndex;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (ModifiedAppearance)
     cOsdProvider::UpdateOsdSize(true);

  if (osdLanguageIndex != oldOsdLanguageIndex || skinIndex != oldSkinIndex) {
     strn0cpy(data.OSDLanguage, I18nLocale(osdLanguageIndex), sizeof(data.OSDLanguage));
     int OriginalOSDLanguage = I18nCurrentLanguage();
     I18nSetLanguage(osdLanguageIndex);

     cSkin *Skin = Skins.Get(skinIndex);
     if (Skin) {
        char *d = themes.NumThemes() ? strdup(themes.Descriptions()[themeIndex]) : NULL;
        themes.Load(Skin->Name());
        if (skinIndex != oldSkinIndex)
           themeIndex = d ? themes.GetThemeIndex(d) : 0;
        free(d);
        }

     Set();
     I18nSetLanguage(OriginalOSDLanguage);
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
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupEPG::cMenuSetupEPG(void)
{
  SetMenuCategory(mcSetupEpg);
  for (numLanguages = 0; numLanguages < I18nLanguages()->Size() && data.EPGLanguages[numLanguages] >= 0; numLanguages++)
      ;
  originalNumLanguages = numLanguages;
  SetSection(tr("EPG"));
  SetHelp(tr("Button$Scan"));
  Setup();
}

void cMenuSetupEPG::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG scan timeout (h)"),      &data.EPGScanTimeout));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG scan max. channel number (0=all)"), &data.EPGScanMaxChannel));
  Add(new cMenuEditBoolItem(tr("Setup.EPG$EPG pause after scan"),      &data.EPGPauseAfterScan));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG bugfix level"),          &data.EPGBugfixLevel, 0, MAXEPGBUGFIXLEVEL));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG linger time (min)"),     &data.EPGLinger, 0));
  Add(new cMenuEditBoolItem(tr("Setup.EPG$Set system time"),           &data.SetSystemTime));
  if (data.SetSystemTime)
     Add(new cMenuEditTranItem(Indent(2, tr("Setup.EPG$Use time from transponder")), &data.TimeTransponder, &data.TimeSource));
  // TRANSLATORS: note the plural!
  Add(new cMenuEditIntItem( tr("Setup.EPG$Preferred languages"),       &numLanguages, 0, I18nLanguages()->Size()));
  for (int i = 0; i < numLanguages; i++)
      // TRANSLATORS: note the singular!
      Add(new cMenuEditStraItem(Indent(2, tr("Setup.EPG$Preferred language")), &data.EPGLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));

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
            for (int l = 0; l < I18nLanguages()->Size(); l++) {
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
  int originalNumSubtitleLanguages;
  int numSubtitleLanguages;
  void Setup(void);
  const char *videoDisplayFormatTexts[3];
  const char *updateChannelsTexts[6];
  const char *displaySubtitlesTexts[3];
  const char *standardComplianceTexts[3];
public:
  cMenuSetupDVB(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupDVB::cMenuSetupDVB(void)
{
  SetMenuCategory(mcSetupDvb);
  for (numAudioLanguages = 0; numAudioLanguages < I18nLanguages()->Size() && data.AudioLanguages[numAudioLanguages] >= 0; numAudioLanguages++)
      ;
  for (numSubtitleLanguages = 0; numSubtitleLanguages < I18nLanguages()->Size() && data.SubtitleLanguages[numSubtitleLanguages] >= 0; numSubtitleLanguages++)
      ;
  originalNumAudioLanguages = numAudioLanguages;
  originalNumSubtitleLanguages = numSubtitleLanguages;
  videoDisplayFormatTexts[0] = tr("pan&scan");
  videoDisplayFormatTexts[1] = tr("letterbox");
  videoDisplayFormatTexts[2] = tr("center cut out");
  updateChannelsTexts[0] = tr("no");
  updateChannelsTexts[1] = tr("names only");
  updateChannelsTexts[2] = tr("PIDs only");
  updateChannelsTexts[3] = tr("names and PIDs");
  updateChannelsTexts[4] = tr("add new channels");
  updateChannelsTexts[5] = tr("add new transponders");
  displaySubtitlesTexts[0] = tr("no");
  displaySubtitlesTexts[1] = tr("always");
  displaySubtitlesTexts[2] = tr("after rewind");
  standardComplianceTexts[0] = "DVB";
  standardComplianceTexts[1] = "ANSI/SCTE";
  standardComplianceTexts[2] = "NORDIG";

  SetSection(tr("DVB"));
  SetHelp(NULL, tr("Button$Audio"), tr("Button$Subtitles"), NULL);
  Setup();
}

void cMenuSetupDVB::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditIntItem( tr("Setup.DVB$Primary DVB interface"), &data.PrimaryDVB, 1, cDevice::NumDevices()));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Standard compliance"),   &data.StandardCompliance, 3, standardComplianceTexts));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Video format"),          &data.VideoFormat, "4:3", "16:9"));
  if (data.VideoFormat == 0)
     Add(new cMenuEditStraItem(tr("Setup.DVB$Video display format"), &data.VideoDisplayFormat, 3, videoDisplayFormatTexts));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Use Dolby Digital"),     &data.UseDolbyDigital));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Update channels"),       &data.UpdateChannels, 6, updateChannelsTexts));
  Add(new cMenuEditIntItem( tr("Setup.DVB$Audio languages"),       &numAudioLanguages, 0, I18nLanguages()->Size()));
  for (int i = 0; i < numAudioLanguages; i++)
      Add(new cMenuEditStraItem(Indent(2, tr("Setup.DVB$Audio language")), &data.AudioLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Display subtitles"),     &data.DisplaySubtitles, 3, displaySubtitlesTexts));
  if (data.DisplaySubtitles != SUBTITLES_NO) {
     Add(new cMenuEditIntItem(Indent(2, tr("Setup.DVB$Subtitle languages")),     &numSubtitleLanguages, 0, I18nLanguages()->Size()));
     for (int i = 0; i < numSubtitleLanguages; i++)
         Add(new cMenuEditStraItem(Indent(4, tr("Setup.DVB$Subtitle language")), &data.SubtitleLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));
     Add(new cMenuEditIntItem(Indent(2, tr("Setup.DVB$Subtitle offset")),                  &data.SubtitleOffset,      -100, 100));
     Add(new cMenuEditIntItem(Indent(2, tr("Setup.DVB$Subtitle foreground transparency")), &data.SubtitleFgTransparency, 0, 9));
     Add(new cMenuEditIntItem(Indent(2, tr("Setup.DVB$Subtitle background transparency")), &data.SubtitleBgTransparency, 0, 10));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupDVB::ProcessKey(eKeys Key)
{
  int oldVideoDisplayFormat = ::Setup.VideoDisplayFormat;
  bool oldVideoFormat = ::Setup.VideoFormat;
  bool newVideoFormat = data.VideoFormat;
  int oldDisplaySubtitles = ::Setup.DisplaySubtitles;
  int newDisplaySubtitles = data.DisplaySubtitles;
  int oldnumAudioLanguages = numAudioLanguages;
  int oldnumSubtitleLanguages = numSubtitleLanguages;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (Key != kNone) {
     switch (Key) {
       case kGreen:  cRemote::Put(kAudio, true);
                     state = osEnd;
                     break;
       case kYellow: cRemote::Put(kSubtitles, true);
                     state = osEnd;
                     break;
       default: {
            bool DoSetup = data.VideoFormat != newVideoFormat;
            DoSetup |= data.DisplaySubtitles != newDisplaySubtitles;
            if (numAudioLanguages != oldnumAudioLanguages) {
               for (int i = oldnumAudioLanguages; i < numAudioLanguages; i++) {
                   data.AudioLanguages[i] = 0;
                   for (int l = 0; l < I18nLanguages()->Size(); l++) {
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
            if (numSubtitleLanguages != oldnumSubtitleLanguages) {
               for (int i = oldnumSubtitleLanguages; i < numSubtitleLanguages; i++) {
                   data.SubtitleLanguages[i] = 0;
                   for (int l = 0; l < I18nLanguages()->Size(); l++) {
                       int k;
                       for (k = 0; k < oldnumSubtitleLanguages; k++) {
                           if (data.SubtitleLanguages[k] == l)
                              break;
                           }
                       if (k >= oldnumSubtitleLanguages) {
                          data.SubtitleLanguages[i] = l;
                          break;
                          }
                       }
                   }
               data.SubtitleLanguages[numSubtitleLanguages] = -1;
               DoSetup = true;
               }
            if (DoSetup)
               Setup();
            }
       }
     }
  if (state == osBack && Key == kOk) {
     if (::Setup.VideoDisplayFormat != oldVideoDisplayFormat)
        cDevice::PrimaryDevice()->SetVideoDisplayFormat(eVideoDisplayFormat(::Setup.VideoDisplayFormat));
     if (::Setup.VideoFormat != oldVideoFormat)
        cDevice::PrimaryDevice()->SetVideoFormat(::Setup.VideoFormat);
     if (::Setup.DisplaySubtitles != oldDisplaySubtitles)
        cDevice::PrimaryDevice()->EnsureSubtitleTrack();
     cDvbSubtitleConverter::SetupChanged();
     }
  return state;
}

// --- cMenuSetupLNB ---------------------------------------------------------

class cMenuSetupLNB : public cMenuSetupBase {
private:
  cSatCableNumbers satCableNumbers;
  void Setup(void);
public:
  cMenuSetupLNB(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupLNB::cMenuSetupLNB(void)
:satCableNumbers(MAXDEVICES)
{
  SetMenuCategory(mcSetupLnb);
  satCableNumbers.FromString(data.DeviceBondings);
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

  int NumSatDevices = 0;
  for (int i = 0; i < cDevice::NumDevices(); i++) {
      if (cDevice::GetDevice(i)->ProvidesSource(cSource::stSat))
         NumSatDevices++;
      }
  if (NumSatDevices > 1) {
     for (int i = 0; i < cDevice::NumDevices(); i++) {
         if (cDevice::GetDevice(i)->ProvidesSource(cSource::stSat))
            Add(new cMenuEditIntItem(cString::sprintf(tr("Setup.LNB$Device %d connected to sat cable"), i + 1), &satCableNumbers.Array()[i], 0, NumSatDevices, tr("Setup.LNB$own")));
         else
            satCableNumbers.Array()[i] = 0;
         }
     }

  Add(new cMenuEditBoolItem(tr("Setup.LNB$Use dish positioner"), &data.UsePositioner));
  if (data.UsePositioner) {
     Add(new cMenuEditIntxItem(tr("Setup.LNB$Site latitude (degrees)"), &data.SiteLat, -900, 900, 10, tr("South"), tr("North")));
     Add(new cMenuEditIntxItem(tr("Setup.LNB$Site longitude (degrees)"), &data.SiteLon, -1800, 1800, 10, tr("West"), tr("East")));
     Add(new cMenuEditIntxItem(tr("Setup.LNB$Max. positioner swing (degrees)"), &data.PositionerSwing, 0, 900, 10));
     Add(new cMenuEditIntxItem(tr("Setup.LNB$Positioner speed (degrees/s)"), &data.PositionerSpeed, 1, 1800, 10));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupLNB::ProcessKey(eKeys Key)
{
  int oldDiSEqC = data.DiSEqC;
  int oldUsePositioner = data.UsePositioner;
  bool DeviceBondingsChanged = false;
  if (Key == kOk) {
     cString NewDeviceBondings = satCableNumbers.ToString();
     DeviceBondingsChanged = strcmp(data.DeviceBondings, NewDeviceBondings) != 0;
     data.DeviceBondings = NewDeviceBondings;
     }
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (Key != kNone && (data.DiSEqC != oldDiSEqC || data.UsePositioner != oldUsePositioner))
     Setup();
  else if (DeviceBondingsChanged)
     cDvbDevice::BondDevices(data.DeviceBondings);
  return state;
}

// --- cMenuSetupCAM ---------------------------------------------------------

class cMenuSetupCAMItem : public cOsdItem {
private:
  cCamSlot *camSlot;
public:
  cMenuSetupCAMItem(cCamSlot *CamSlot);
  cCamSlot *CamSlot(void) { return camSlot; }
  bool Changed(void);
  };

cMenuSetupCAMItem::cMenuSetupCAMItem(cCamSlot *CamSlot)
{
  camSlot = CamSlot;
  SetText("");
  Changed();
}

bool cMenuSetupCAMItem::Changed(void)
{
  cString AssignedDevice("");
  const char *Activating = "";
  const char *CamName = camSlot->GetCamName();
  if (!CamName) {
     switch (camSlot->ModuleStatus()) {
       case msReset:   CamName = tr("CAM reset"); break;
       case msPresent: CamName = tr("CAM present"); break;
       case msReady:   CamName = tr("CAM ready"); break;
       default:        CamName = "-"; break;
       }
     }
  else if (camSlot->IsActivating())
     // TRANSLATORS: note the leading blank!
     Activating = tr(" (activating)");
  cVector<int> DeviceNumbers;
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
      if (CamSlot == camSlot || CamSlot->MasterSlot() == camSlot)
         CamSlot->Devices(DeviceNumbers);
      }
  if (DeviceNumbers.Size() > 0) {
     AssignedDevice = cString::sprintf(" %s", tr("@ device"));
     DeviceNumbers.Sort(CompareInts);
     for (int i = 0; i < DeviceNumbers.Size(); i++)
         AssignedDevice = cString::sprintf("%s %d", *AssignedDevice, DeviceNumbers[i]);
     }

  cString buffer = cString::sprintf(" %d %s%s%s", camSlot->SlotNumber(), CamName, *AssignedDevice, Activating);
  if (strcmp(buffer, Text()) != 0) {
     SetText(buffer);
     return true;
     }
  return false;
}

class cMenuSetupCAM : public cMenuSetupBase {
private:
  int currentChannel;
  const char *activationHelp;
  eOSState Menu(void);
  eOSState Reset(void);
  eOSState Activate(void);
  void SetHelpKeys(void);
public:
  cMenuSetupCAM(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupCAM::cMenuSetupCAM(void)
{
  currentChannel = cDevice::CurrentChannel();
  activationHelp = NULL;
  SetMenuCategory(mcSetupCam);
  SetSection(tr("CAM"));
  SetCols(15);
  SetHasHotkeys();
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot)) {
      if (CamSlot->IsMasterSlot()) // we only list master CAM slots
         Add(new cMenuSetupCAMItem(CamSlot));
      }
  SetHelpKeys();
}

void cMenuSetupCAM::SetHelpKeys(void)
{
  if (HasSubMenu())
     return;
  cMenuSetupCAMItem *item = (cMenuSetupCAMItem *)Get(Current());
  const char *NewActivationHelp = "";
  if (item) {
     cCamSlot *CamSlot = item->CamSlot();
     if (CamSlot->IsActivating())
        NewActivationHelp = tr("Button$Cancel activation");
     else if (CamSlot->CanActivate())
        NewActivationHelp = tr("Button$Activate");
     }
  if (NewActivationHelp != activationHelp) {
     activationHelp = NewActivationHelp;
     SetHelp(tr("Button$Menu"), tr("Button$Reset"), activationHelp);
     }
}

eOSState cMenuSetupCAM::Menu(void)
{
  cMenuSetupCAMItem *item = (cMenuSetupCAMItem *)Get(Current());
  if (item) {
     if (item->CamSlot()->EnterMenu()) {
        Skins.Message(mtStatus, tr("Opening CAM menu..."));
        time_t t0 = time(NULL);
        time_t t1 = t0;
        while (time(NULL) - t0 <= MAXWAITFORCAMMENU) {
              if (item->CamSlot()->HasUserIO())
                 break;
              if (time(NULL) - t1 >= CAMMENURETRYTIMEOUT) {
                 dsyslog("CAM %d: retrying to enter CAM menu...", item->CamSlot()->SlotNumber());
                 item->CamSlot()->EnterMenu();
                 t1 = time(NULL);
                 }
              cCondWait::SleepMs(100);
              }
        Skins.Message(mtStatus, NULL);
        if (item->CamSlot()->HasUserIO())
           return AddSubMenu(new cMenuCam(item->CamSlot()));
        }
     Skins.Message(mtError, tr("Can't open CAM menu!"));
     }
  return osContinue;
}

eOSState cMenuSetupCAM::Activate(void)
{
  cMenuSetupCAMItem *item = (cMenuSetupCAMItem *)Get(Current());
  if (item) {
     cCamSlot *CamSlot = item->CamSlot();
     if (CamSlot->IsActivating())
        CamSlot->CancelActivation();
     else if (CamSlot->CanActivate()) {
        if (CamSlot->Priority() < LIVEPRIORITY) { // don't interrupt recordings
           LOCK_CHANNELS_READ;
           if (const cChannel *Channel = Channels->GetByNumber(cDevice::CurrentChannel())) {
              for (int i = 0; i < cDevice::NumDevices(); i++) {
                  if (cDevice *Device = cDevice::GetDevice(i)) {
                     if (Device->ProvidesChannel(Channel)) {
                        if (Device->Priority() < LIVEPRIORITY) { // don't interrupt recordings
                           if (CamSlot->Assign(Device, true)) { // query
                              cControl::Shutdown(); // must end transfer mode before assigning CAM, otherwise it might be unassigned again
                              CamSlot = CamSlot->MtdSpawn();
                              if (CamSlot->Assign(Device)) {
                                 if (Device->SwitchChannel(Channel, true)) {
                                    CamSlot->StartActivation();
                                    return osContinue;
                                    }
                                 }
                              }
                           }
                        }
                     }
                  }
              }
           }
        Skins.Message(mtError, tr("Can't activate CAM!"));
        }
     }
  return osContinue;
}

eOSState cMenuSetupCAM::Reset(void)
{
  cMenuSetupCAMItem *item = (cMenuSetupCAMItem *)Get(Current());
  if (item) {
     if (!item->CamSlot()->Device() || Interface->Confirm(tr("CAM is in use - really reset?"))) {
        if (!item->CamSlot()->Reset())
           Skins.Message(mtError, tr("Can't reset CAM!"));
        }
     }
  return osContinue;
}

eOSState cMenuSetupCAM::ProcessKey(eKeys Key)
{
  eOSState state = HasSubMenu() ? cMenuSetupBase::ProcessKey(Key) : cOsdMenu::ProcessKey(Key);

  if (!HasSubMenu()) {
     switch (Key) {
       case kOk:
       case kRed:    return Menu();
       case kGreen:  state = Reset(); break;
       case kYellow: state = Activate(); break;
       default: break;
       }
     for (cMenuSetupCAMItem *ci = (cMenuSetupCAMItem *)First(); ci; ci = (cMenuSetupCAMItem *)ci->Next()) {
         if (ci->Changed())
            DisplayItem(ci);
         }
     SetHelpKeys();
     }
  if (currentChannel != cDevice::CurrentChannel())
     state = osEnd;
  return state;
}

// --- cMenuSetupRecord ------------------------------------------------------

class cMenuSetupRecord : public cMenuSetupBase {
private:
  const char *recordKeyHandlingTexts[3];
  const char *pauseKeyHandlingTexts[3];
  const char *delTimeshiftRecTexts[3];
public:
  cMenuSetupRecord(void);
  };

cMenuSetupRecord::cMenuSetupRecord(void)
{
  SetMenuCategory(mcSetupRecord);
  recordKeyHandlingTexts[0] = tr("no instant recording");
  recordKeyHandlingTexts[1] = tr("confirm instant recording");
  recordKeyHandlingTexts[2] = tr("record instantly");
  pauseKeyHandlingTexts[0] = tr("do not pause live video");
  pauseKeyHandlingTexts[1] = tr("confirm pause live video");
  pauseKeyHandlingTexts[2] = tr("pause live video");
  delTimeshiftRecTexts[0] = tr("no");
  delTimeshiftRecTexts[1] = tr("confirm");
  delTimeshiftRecTexts[2] = tr("yes");
  SetSection(tr("Recording"));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Margin at start (min)"),     &data.MarginStart));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Margin at stop (min)"),      &data.MarginStop));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Default priority"),          &data.DefaultPriority, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Default lifetime (d)"),      &data.DefaultLifetime, 0, MAXLIFETIME));
  Add(new cMenuEditStraItem(tr("Setup.Recording$Record key handling"),       &data.RecordKeyHandling, 3, recordKeyHandlingTexts));
  Add(new cMenuEditStraItem(tr("Setup.Recording$Pause key handling"),        &data.PauseKeyHandling, 3, pauseKeyHandlingTexts));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Pause priority"),            &data.PausePriority, 0, MAXPRIORITY));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Pause lifetime (d)"),        &data.PauseLifetime, 0, MAXLIFETIME));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Use episode name"),          &data.UseSubtitle));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Use VPS"),                   &data.UseVps));
  Add(new cMenuEditIntItem( tr("Setup.Recording$VPS margin (s)"),            &data.VpsMargin, 0));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Mark instant recording"),    &data.MarkInstantRecord));
  Add(new cMenuEditStrItem( tr("Setup.Recording$Name instant recording"),     data.NameInstantRecord, sizeof(data.NameInstantRecord)));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Instant rec. time (min)"),   &data.InstantRecordTime, 0, MAXINSTANTRECTIME, tr("Setup.Recording$present event")));
  Add(new cMenuEditIntItem( tr("Setup.Recording$Max. video file size (MB)"), &data.MaxVideoFileSize, MINVIDEOFILESIZE, MAXVIDEOFILESIZETS));
  Add(new cMenuEditBoolItem(tr("Setup.Recording$Split edited files"),        &data.SplitEditedFiles));
  Add(new cMenuEditStraItem(tr("Setup.Recording$Delete timeshift recording"),&data.DelTimeshiftRec, 3, delTimeshiftRecTexts));
}

// --- cMenuSetupReplay ------------------------------------------------------

class cMenuSetupReplay : public cMenuSetupBase {
protected:
  virtual void Store(void) override;
public:
  cMenuSetupReplay(void);
  };

cMenuSetupReplay::cMenuSetupReplay(void)
{
  SetMenuCategory(mcSetupReplay);
  SetSection(tr("Replay"));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Multi speed mode"), &data.MultiSpeedMode));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Show replay mode"), &data.ShowReplayMode));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Show remaining time"), &data.ShowRemainingTime));
  Add(new cMenuEditIntItem( tr("Setup.Replay$Progress display time (s)"), &data.ProgressDisplayTime, 0, 60));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Pause replay when setting mark"), &data.PauseOnMarkSet));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Pause replay when jumping to a mark"), &data.PauseOnMarkJump));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Skip edited parts"), &data.SkipEdited));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Pause replay at last mark"), &data.PauseAtLastMark));
  Add(new cMenuEditIntItem( tr("Setup.Replay$Initial duration for adaptive skipping (s)"), &data.AdaptiveSkipInitial, 10, 600));
  Add(new cMenuEditIntItem( tr("Setup.Replay$Reset timeout for adaptive skipping (s)"), &data.AdaptiveSkipTimeout, 0, 10));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Alternate behavior for adaptive skipping"), &data.AdaptiveSkipAlternate));
  Add(new cMenuEditBoolItem(tr("Setup.Replay$Use Prev/Next keys for adaptive skipping"), &data.AdaptiveSkipPrevNext));
  Add(new cMenuEditIntItem( tr("Setup.Replay$Skip distance with Green/Yellow keys (s)"), &data.SkipSeconds, 5, 600));
  Add(new cMenuEditIntItem( tr("Setup.Replay$Skip distance with Green/Yellow keys in repeat (s)"), &data.SkipSecondsRepeat, 5, 600));
  Add(new cMenuEditIntItem(tr("Setup.Replay$Resume ID"), &data.ResumeID, 0, 99));
}

void cMenuSetupReplay::Store(void)
{
  if (Setup.ResumeID != data.ResumeID) {
     LOCK_RECORDINGS_WRITE;
     Recordings->ResetResume();
     }
  cMenuSetupBase::Store();
}

// --- cMenuSetupMisc --------------------------------------------------------

class cMenuSetupMisc : public cMenuSetupBase {
private:
  const char *svdrpPeeringModeTexts[3];
  const char *showChannelNamesWithSourceTexts[3];
  cStringList svdrpServerNames;
  void Set(void);
public:
  cMenuSetupMisc(void);
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupMisc::cMenuSetupMisc(void)
{
  SetMenuCategory(mcSetupMisc);
  svdrpPeeringModeTexts[0] = tr("off");
  svdrpPeeringModeTexts[1] = tr("any hosts");
  svdrpPeeringModeTexts[2] = tr("only default host");
  showChannelNamesWithSourceTexts[0] = tr("off");
  showChannelNamesWithSourceTexts[1] = tr("type");
  showChannelNamesWithSourceTexts[2] = tr("full");
  SetSection(tr("Miscellaneous"));
  Set();
}

void cMenuSetupMisc::Set(void)
{
  int current = Current();
  Clear();
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. event timeout (min)"),   &data.MinEventTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. user inactivity (min)"), &data.MinUserInactivity));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$SVDRP timeout (s)"),          &data.SVDRPTimeout));
  Add(new cMenuEditStraItem(tr("Setup.Miscellaneous$SVDRP peering"),              &data.SVDRPPeering, 3, svdrpPeeringModeTexts));
  if (data.SVDRPPeering) {
     Add(new cMenuEditStrItem(Indent(2, tr("Setup.Miscellaneous$SVDRP host name")), data.SVDRPHostName, sizeof(data.SVDRPHostName)));
     if (GetSVDRPServerNames(&svdrpServerNames)) {
        svdrpServerNames.Sort(true);
        svdrpServerNames.Insert(strdup(""));
        Add(new cMenuEditStrlItem(Indent(2, tr("Setup.Miscellaneous$SVDRP default host")), data.SVDRPDefaultHost, sizeof(data.SVDRPDefaultHost), &svdrpServerNames));
        }
     }
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Zap timeout (s)"),            &data.ZapTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Channel entry timeout (ms)"), &data.ChannelEntryTimeout, 0));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Remote control repeat delay (ms)"), &data.RcRepeatDelay, 0));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Remote control repeat delta (ms)"), &data.RcRepeatDelta, 0));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Deleted recordings retention (d)"), &data.DeleteRetention, 0));
  Add(new cMenuEditChanItem(tr("Setup.Miscellaneous$Initial channel"),            &data.InitialChannel, tr("Setup.Miscellaneous$as before")));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Initial volume"),             &data.InitialVolume, -1, 255, tr("Setup.Miscellaneous$as before")));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Volume steps"),               &data.VolumeSteps, 5, 255));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Volume linearize"),           &data.VolumeLinearize, -20, 20));
  Add(new cMenuEditBoolItem(tr("Setup.Miscellaneous$Channels wrap"),              &data.ChannelsWrap));
  Add(new cMenuEditStraItem(tr("Setup.Miscellaneous$Show channel names with source"), &data.ShowChannelNamesWithSource, 3, showChannelNamesWithSourceTexts));
  Add(new cMenuEditBoolItem(tr("Setup.Miscellaneous$Emergency exit"),             &data.EmergencyExit));
  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupMisc::ProcessKey(eKeys Key)
{
  bool OldSVDRPPeering = data.SVDRPPeering;
  bool ModifiedSVDRPSettings = false;
  bool ModifiedShowChannelNamesWithSource = false;
  if (Key == kOk) {
     ModifiedSVDRPSettings = data.SVDRPPeering != Setup.SVDRPPeering || strcmp(data.SVDRPHostName, Setup.SVDRPHostName);
     ModifiedShowChannelNamesWithSource = data.ShowChannelNamesWithSource != Setup.ShowChannelNamesWithSource;
     }
  eOSState state = cMenuSetupBase::ProcessKey(Key);
  if (ModifiedShowChannelNamesWithSource) {
     LOCK_CHANNELS_WRITE;
     for (cChannel *Channel = Channels->First(); Channel; Channel = Channels->Next(Channel))
         Channel->UpdateNameSource();
     }
  if (data.SVDRPPeering != OldSVDRPPeering)
     Set();
  if (ModifiedSVDRPSettings) {
     StopSVDRPHandler();
     {
       LOCK_TIMERS_WRITE;
       Timers->SetExplicitModify();
       if (Timers->StoreRemoteTimers(NULL, NULL))
          Timers->SetModified();
     }
     StartSVDRPHandler();
     }
  return state;
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
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetupPlugins::cMenuSetupPlugins(void)
{
  SetMenuCategory(mcSetupPlugins);
  SetSection(tr("Plugins"));
  SetHasHotkeys();
  for (int i = 0; ; i++) {
      cPlugin *p = cPluginManager::GetPlugin(i);
      if (p)
         Add(new cMenuSetupPluginItem(hk(cString::sprintf("%s (%s) - %s", p->Name(), p->Version(), p->Description())), i));
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
     else if (state == osContinue) {
        Store();
        // Reinitialize OSD and skin, in case any plugin setup change has an influence on these:
        cOsdProvider::UpdateOsdSize(true);
        Display();
        }
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
  virtual eOSState ProcessKey(eKeys Key) override;
  };

cMenuSetup::cMenuSetup(void)
:cOsdMenu("")
{
  SetMenuCategory(mcSetup);
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
  Add(new cOsdItem(hk(tr("CAM")),           osUser5));
  Add(new cOsdItem(hk(tr("Recording")),     osUser6));
  Add(new cOsdItem(hk(tr("Replay")),        osUser7));
  Add(new cOsdItem(hk(tr("Miscellaneous")), osUser8));
  if (cPluginManager::HasPlugins())
  Add(new cOsdItem(hk(tr("Plugins")),       osUser9));
  Add(new cOsdItem(hk(tr("Restart")),       osUser10));
}

eOSState cMenuSetup::Restart(void)
{
  if (Interface->Confirm(tr("Really restart?")) && ShutdownHandler.ConfirmRestart(true)) {
     ShutdownHandler.Exit(1);
     return osEnd;
     }
  return osContinue;
}

eOSState cMenuSetup::ProcessKey(eKeys Key)
{
  int osdLanguage = I18nCurrentLanguage();
  eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state) {
    case osUser1: return AddSubMenu(new cMenuSetupOSD);
    case osUser2: return AddSubMenu(new cMenuSetupEPG);
    case osUser3: return AddSubMenu(new cMenuSetupDVB);
    case osUser4: return AddSubMenu(new cMenuSetupLNB);
    case osUser5: return AddSubMenu(new cMenuSetupCAM);
    case osUser6: return AddSubMenu(new cMenuSetupRecord);
    case osUser7: return AddSubMenu(new cMenuSetupReplay);
    case osUser8: return AddSubMenu(new cMenuSetupMisc);
    case osUser9: return AddSubMenu(new cMenuSetupPlugins);
    case osUser10: return Restart();
    default: ;
    }
  if (I18nCurrentLanguage() != osdLanguage) {
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

// TRANSLATORS: note the leading and trailing blanks!
#define STOP_RECORDING trNOOP(" Stop recording ")

cOsdObject *cMenuMain::pluginOsdObject = NULL;

cMenuMain::cMenuMain(eOSState State, bool OpenSubMenus)
:cOsdMenu("")
{
  SetMenuCategory(mcMain);
  replaying = false;
  stopReplayItem = NULL;
  cancelEditingItem = NULL;
  stopRecordingItem = NULL;
  recordControlsState = 0;
  Set();

  // Initial submenus:

  switch (State) {
    case osSchedule:   AddSubMenu(new cMenuSchedule); break;
    case osChannels:   AddSubMenu(new cMenuChannels); break;
    case osTimers:     AddSubMenu(new cMenuTimers); break;
    case osRecordings: AddSubMenu(new cMenuRecordings(NULL, 0, OpenSubMenus)); break;
    case osSetup:      AddSubMenu(new cMenuSetup); break;
    case osCommands:   AddSubMenu(new cMenuCommands(tr("Commands"), &Commands)); break;
    default: break;
    }
}

cOsdObject *cMenuMain::PluginOsdObject(void)
{
  cOsdObject *o = pluginOsdObject;
  pluginOsdObject = NULL;
  return o;
}

void cMenuMain::Set(void)
{
  Clear();
  SetTitle("VDR");
  SetHasHotkeys();

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
            Add(new cMenuPluginItem(hk(item), i));
         }
      else
         break;
      }

  // More basic menu items:

  Add(new cOsdItem(hk(tr("Setup")),      osSetup));
  if (Commands.Count())
     Add(new cOsdItem(hk(tr("Commands")),  osCommands));

  Update(true);

  Display();
}

bool cMenuMain::Update(bool Force)
{
  bool result = false;

  bool NewReplaying = false;
  {
    cMutexLock ControlMutexLock;
    NewReplaying = cControl::Control(ControlMutexLock) != NULL;
  }
  if (Force || NewReplaying != replaying) {
     replaying = NewReplaying;
     // Replay control:
     if (replaying && !stopReplayItem)
        // TRANSLATORS: note the leading blank!
        Add(stopReplayItem = new cOsdItem(tr(" Stop replaying"), osStopReplay));
     else if (stopReplayItem && !replaying) {
        Del(stopReplayItem->Index());
        stopReplayItem = NULL;
        }
     // Color buttons:
     SetHelp(!replaying && Setup.RecordKeyHandling ? tr("Button$Record") : NULL, tr("Button$Audio"), replaying || !Setup.PauseKeyHandling ? NULL : tr("Button$Pause"), replaying ? tr("Button$Stop") : cReplayControl::LastReplayed() ? tr("Button$Resume") : tr("Button$Play"));
     result = true;
     }

  // Editing control:
  bool EditingActive = RecordingsHandler.Active();
  if (EditingActive && !cancelEditingItem) {
     // TRANSLATORS: note the leading blank!
     Add(cancelEditingItem = new cOsdItem(tr(" Cancel editing"), osCancelEdit));
     result = true;
     }
  else if (cancelEditingItem && !EditingActive) {
     Del(cancelEditingItem->Index());
     cancelEditingItem = NULL;
     result = true;
     }

  // Record control:
  if (cRecordControls::StateChanged(recordControlsState)) {
     while (stopRecordingItem) {
           cOsdItem *it = Next(stopRecordingItem);
           Del(stopRecordingItem->Index());
           stopRecordingItem = it;
           }
     const char *s = NULL;
     while ((s = cRecordControls::GetInstantId(s)) != NULL) {
           cOsdItem *item = new cOsdItem(osStopRecord);
           item->SetText(cString::sprintf("%s%s", tr(STOP_RECORDING), s));
           Add(item);
           if (!stopRecordingItem)
              stopRecordingItem = item;
           }
     result = true;
     }

  return result;
}

eOSState cMenuMain::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
  int osdLanguage = I18nCurrentLanguage();
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
                          if (cOsdItem *item = Get(Current())) {
                             cRecordControls::Stop(item->Text() + strlen(tr(STOP_RECORDING)));
                             return osEnd;
                             }
                          }
                       break;
    case osCancelEdit: if (Interface->Confirm(tr("Cancel editing?"))) {
                          RecordingsHandler.DelAll();
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
                                state = replaying || !Setup.RecordKeyHandling ? osContinue : osRecord;
                             break;
               case kGreen:  if (!HadSubMenu) {
                                cRemote::Put(kAudio, true);
                                state = osEnd;
                                }
                             break;
               case kYellow: if (!HadSubMenu)
                                state = replaying || !Setup.PauseKeyHandling ? osContinue : osPause;
                             break;
               case kBlue:   if (!HadSubMenu)
                                state = replaying ? osStopReplay : cReplayControl::LastReplayed() ? osReplay : osRecordings;
                             break;
               default:      break;
               }
    }
  bool DoDisplay = Update();
  if (Key != kNone) {
     if (I18nCurrentLanguage() != osdLanguage) {
        Set();
        if (!HasSubMenu())
           DoDisplay = true;
        }
     }
  if (DoDisplay)
     Display();
  return state;
}

// --- SetTrackDescriptions --------------------------------------------------

void SetTrackDescriptions(int LiveChannel)
{
  cDevice::PrimaryDevice()->ClrAvailableTracks(true);
  const cComponents *Components = NULL;
  if (LiveChannel) {
     LOCK_CHANNELS_READ;
     if (const cChannel *Channel = Channels->GetByNumber(LiveChannel)) {
        LOCK_SCHEDULES_READ;
        if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
           const cEvent *Present = Schedule->GetPresentEvent();
           if (Present)
              Components = Present->Components();
           }
        }
     }
  else if (cReplayControl::NowReplaying()) {
     LOCK_RECORDINGS_READ;
     if (const cRecording *Recording = Recordings->GetByName(cReplayControl::NowReplaying()))
        Components = Recording->Info()->Components();
     }
  if (Components) {
     int indexAudio = 0;
     int indexDolby = 0;
     int indexSubtitle = 0;
     for (int i = 0; i < Components->NumComponents(); i++) {
         const tComponent *p = Components->Component(i);
         switch (p->stream) {
           case 2: if (p->type == 0x05)
                      cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, indexDolby++, 0, LiveChannel ? NULL : p->language, p->description);
                   else
                      cDevice::PrimaryDevice()->SetAvailableTrack(ttAudio, indexAudio++, 0, LiveChannel ? NULL : p->language, p->description);
                   break;
           case 3: cDevice::PrimaryDevice()->SetAvailableTrack(ttSubtitle, indexSubtitle++, 0, LiveChannel ? NULL : p->language, p->description);
                   break;
           case 4: cDevice::PrimaryDevice()->SetAvailableTrack(ttDolby, indexDolby++, 0, LiveChannel ? NULL : p->language, p->description);
                   break;
           default: ;
           }
         }
     }
}

// --- cDisplayChannel -------------------------------------------------------

cDisplayChannel *cDisplayChannel::currentDisplayChannel = NULL;

cDisplayChannel::cDisplayChannel(int Number, bool Switched)
:cOsdObject(true)
{
  currentDisplayChannel = this;
  group = -1;
  withInfo = !Switched || Setup.ShowInfoOnChSwitch;
  displayChannel = Skins.Current()->DisplayChannel(withInfo);
  number = 0;
  timeout = Switched || Setup.TimeoutRequChInfo;
  cOsdProvider::OsdSizeChanged(osdState); // just to get the current state
  positioner = NULL;
  channel = NULL;
  {
    LOCK_CHANNELS_READ;
    channel = Channels->GetByNumber(Number);
    lastPresent = lastFollowing = NULL;
    if (channel) {
       DisplayChannel();
       DisplayInfo();
       }
  }
  if (channel)
     displayChannel->Flush();
  lastTime.Set();
}

cDisplayChannel::cDisplayChannel(eKeys FirstKey)
:cOsdObject(true)
{
  currentDisplayChannel = this;
  group = -1;
  number = 0;
  timeout = true;
  lastPresent = lastFollowing = NULL;
  cOsdProvider::OsdSizeChanged(osdState); // just to get the current state
  lastTime.Set();
  withInfo = Setup.ShowInfoOnChSwitch;
  displayChannel = Skins.Current()->DisplayChannel(withInfo);
  positioner = NULL;
  channel = NULL;
  {
    LOCK_CHANNELS_READ;
    channel = Channels->GetByNumber(cDevice::CurrentChannel());
  }
  ProcessKey(FirstKey);
}

cDisplayChannel::~cDisplayChannel()
{
  delete displayChannel;
  currentDisplayChannel = NULL;
  cStatus::MsgOsdClear();
}

void cDisplayChannel::DisplayChannel(void)
{
  displayChannel->SetChannel(channel, number);
  cStatus::MsgOsdChannel(ChannelString(channel, number));
  lastPresent = lastFollowing = NULL;
  lastTime.Set();
}

void cDisplayChannel::DisplayInfo(void)
{
  if (withInfo && channel) {
     LOCK_SCHEDULES_READ;
     if (const cSchedule *Schedule = Schedules->GetSchedule(channel)) {
        const cEvent *Present = Schedule->GetPresentEvent();
        const cEvent *Following = Schedule->GetFollowingEvent();
        if (Present != lastPresent || Following != lastFollowing) {
           SetTrackDescriptions(channel->Number());
           displayChannel->SetEvents(Present, Following);
           cStatus::MsgOsdProgramme(Present ? Present->StartTime() : 0, Present ? Present->Title() : NULL, Present ? Present->ShortText() : NULL, Following ? Following->StartTime() : 0, Following ? Following->Title() : NULL, Following ? Following->ShortText() : NULL);
           lastPresent = Present;
           lastFollowing = Following;
           lastTime.Set();
           }
        }
     }
}

void cDisplayChannel::Refresh(void)
{
  DisplayChannel();
  displayChannel->SetEvents(NULL, NULL);
}

const cChannel *cDisplayChannel::NextAvailableChannel(const cChannel *Channel, int Direction)
{
  if (Direction) {
     cControl::Shutdown(); // prevents old channel from being shown too long if GetDevice() takes longer
                           // and, if decrypted, this removes the now superfluous PIDs from the CAM, too
     LOCK_CHANNELS_READ;
     while (Channel) {
           Channel = Direction > 0 ? Channels->Next(Channel) : Channels->Prev(Channel);
           if (!Channel && Setup.ChannelsWrap)
              Channel = Direction > 0 ? Channels->First() : Channels->Last();
           if (Channel && !Channel->GroupSep() && cDevice::GetDevice(Channel, LIVEPRIORITY, true, true))
              return Channel;
           }
     }
  return NULL;
}

eOSState cDisplayChannel::ProcessKey(eKeys Key)
{
  if (cOsdProvider::OsdSizeChanged(osdState)) {
     delete displayChannel;
     displayChannel = Skins.Current()->DisplayChannel(withInfo);
     }
  const cChannel *NewChannel = NULL;
  if (Key != kNone)
     lastTime.Set();
  switch (int(Key)) {
    case k0:
         if (number == 0) {
            // keep the "Toggle channels" function working
            cRemote::Put(Key);
            return osEnd;
            }
    case k1 ... k9:
         group = -1;
         if (number >= 0) {
            if (number > cChannels::MaxNumber())
               number = Key - k0;
            else
               number = number * 10 + Key - k0;
            LOCK_CHANNELS_READ
            channel = Channels->GetByNumber(number);
            Refresh();
            withInfo = false;
            // Lets see if there can be any useful further input:
            int n = channel ? number * 10 : 0;
            int m = 10;
            const cChannel *ch = channel;
            while (ch && (ch = Channels->Next(ch)) != NULL) {
                  if (!ch->GroupSep()) {
                     if (n <= ch->Number() && ch->Number() < n + m) {
                        n = 0;
                        break;
                        }
                     if (ch->Number() > n) {
                        n *= 10;
                        m *= 10;
                        }
                     }
                  }
            if (n > 0) {
               // This channel is the only one that fits the input, so let's take it right away:
               NewChannel = channel;
               withInfo = true;
               number = 0;
               Refresh();
               }
            }
         break;
    case kLeft|k_Repeat:
    case kLeft:
    case kRight|k_Repeat:
    case kRight:
    case kNext|k_Repeat:
    case kNext:
    case kPrev|k_Repeat:
    case kPrev: {
         withInfo = false;
         number = 0;
         LOCK_CHANNELS_READ;
         if (group < 0) {
            if (const cChannel *Channel = Channels->GetByNumber(cDevice::CurrentChannel()))
               group = Channel->Index();
            }
         if (group >= 0) {
            int SaveGroup = group;
            if (NORMALKEY(Key) == kRight || NORMALKEY(Key) == kNext)
               group = Channels->GetNextGroup(group) ;
            else
               group = Channels->GetPrevGroup(group < 1 ? 1 : group);
            if (group < 0)
               group = SaveGroup;
            channel = Channels->Get(group);
            if (channel) {
               Refresh();
               if (!channel->GroupSep())
                  group = -1;
               }
            }
         break;
         }
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kChanUp|k_Repeat:
    case kChanUp:
    case kChanDn|k_Repeat:
    case kChanDn: {
         eKeys k = NORMALKEY(Key);
         if (const cChannel *Channel = NextAvailableChannel(channel, (k == kUp || k == kChanUp) ? 1 : -1))
            channel = Channel;
         else if (channel && channel->Number() != cDevice::CurrentChannel())
            Key = k; // immediately switches channel when hitting the beginning/end of the channel list with k_Repeat
         }
         // no break here
    case kUp|k_Release:
    case kDown|k_Release:
    case kChanUp|k_Release:
    case kChanDn|k_Release:
    case kNext|k_Release:
    case kPrev|k_Release:
         if (!(Key & k_Repeat) && channel && channel->Number() != cDevice::CurrentChannel())
            NewChannel = channel;
         withInfo = true;
         group = -1;
         number = 0;
         Refresh();
         break;
    case kNone:
         if (number && Setup.ChannelEntryTimeout && int(lastTime.Elapsed()) > Setup.ChannelEntryTimeout) {
            LOCK_CHANNELS_READ;
            channel = Channels->GetByNumber(number);
            if (channel)
               NewChannel = channel;
            withInfo = true;
            number = 0;
            Refresh();
            lastTime.Set();
            }
         break;
    //TODO
    //XXX case kGreen:  return osEventNow;
    //XXX case kYellow: return osEventNext;
    case kOk: {
         LOCK_CHANNELS_READ;
         if (group >= 0) {
            channel = Channels->Get(Channels->GetNextNormal(group));
            if (channel)
               NewChannel = channel;
            withInfo = true;
            group = -1;
            Refresh();
            }
         else if (number > 0) {
            channel = Channels->GetByNumber(number);
            if (channel)
               NewChannel = channel;
            withInfo = true;
            number = 0;
            Refresh();
            }
         else {
            return osEnd;
            }
         }
         break;
    default:
         if ((Key & (k_Repeat | k_Release)) == 0) {
            cRemote::Put(Key);
            return osEnd;
            }
    };
  if (positioner || !timeout || lastTime.Elapsed() < (uint64_t)(Setup.ChannelInfoTime * 1000)) {
     {
       LOCK_CHANNELS_READ;
       if (Key == kNone && !number && group < 0 && !NewChannel && channel && channel->Number() != cDevice::CurrentChannel()) {
          // makes sure a channel switch through the SVDRP CHAN command is displayed
          channel = Channels->GetByNumber(cDevice::CurrentChannel());
          Refresh();
          lastTime.Set();
          }
       DisplayInfo();
       if (NewChannel) {
          SetTrackDescriptions(NewChannel->Number()); // to make them immediately visible in the channel display
          Channels->SwitchTo(NewChannel->Number());
          SetTrackDescriptions(NewChannel->Number()); // switching the channel has cleared them
          channel = NewChannel;
          }
       const cPositioner *Positioner = cDevice::ActualDevice()->Positioner();
       bool PositionerMoving = Positioner && Positioner->IsMoving();
       SetNeedsFastResponse(PositionerMoving);
       if (!PositionerMoving) {
          if (positioner)
             lastTime.Set(); // to keep the channel display up a few seconds after the target position has been reached
          Positioner = NULL;
          }
       if (Positioner || positioner) // making sure we call SetPositioner(NULL) if there is a switch from "with" to "without" positioner
          displayChannel->SetPositioner(Positioner);
       positioner = Positioner;
     }
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
  switch (int(Key)) {
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
  SetTrackDescriptions(!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring() ? cDevice::CurrentChannel() : 0);
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
  descriptions[numTracks] = NULL;
  timeout.Set(TRACKTIMEOUT);
  displayTracks = Skins.Current()->DisplayTracks(tr("Button$Audio"), numTracks, descriptions);
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
  switch (int(Key)) {
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
         if (types[track] != cDevice::PrimaryDevice()->GetCurrentAudioTrack())
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

// --- cDisplaySubtitleTracks ------------------------------------------------

cDisplaySubtitleTracks *cDisplaySubtitleTracks::currentDisplayTracks = NULL;

cDisplaySubtitleTracks::cDisplaySubtitleTracks(void)
:cOsdObject(true)
{
  SetTrackDescriptions(!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring() ? cDevice::CurrentChannel() : 0);
  currentDisplayTracks = this;
  numTracks = track = 0;
  types[numTracks] = ttNone;
  descriptions[numTracks] = strdup(tr("No subtitles"));
  numTracks++;
  eTrackType CurrentSubtitleTrack = cDevice::PrimaryDevice()->GetCurrentSubtitleTrack();
  for (int i = ttSubtitleFirst; i <= ttSubtitleLast; i++) {
      const tTrackId *TrackId = cDevice::PrimaryDevice()->GetTrack(eTrackType(i));
      if (TrackId && TrackId->id) {
         types[numTracks] = eTrackType(i);
         descriptions[numTracks] = strdup(*TrackId->description ? TrackId->description : *TrackId->language ? TrackId->language : *itoa(i));
         if (i == CurrentSubtitleTrack)
            track = numTracks;
         numTracks++;
         }
      }
  descriptions[numTracks] = NULL;
  timeout.Set(TRACKTIMEOUT);
  displayTracks = Skins.Current()->DisplayTracks(tr("Button$Subtitles"), numTracks, descriptions);
  Show();
}

cDisplaySubtitleTracks::~cDisplaySubtitleTracks()
{
  delete displayTracks;
  currentDisplayTracks = NULL;
  for (int i = 0; i < numTracks; i++)
      free(descriptions[i]);
  cStatus::MsgOsdClear();
}

void cDisplaySubtitleTracks::Show(void)
{
  displayTracks->SetTrack(track, descriptions);
  displayTracks->Flush();
  cStatus::MsgSetSubtitleTrack(track, descriptions);
}

cDisplaySubtitleTracks *cDisplaySubtitleTracks::Create(void)
{
  if (cDevice::PrimaryDevice()->NumSubtitleTracks() > 0) {
     if (!currentDisplayTracks)
        new cDisplaySubtitleTracks;
     return currentDisplayTracks;
     }
  Skins.Message(mtWarning, tr("No subtitles available!"));
  return NULL;
}

void cDisplaySubtitleTracks::Process(eKeys Key)
{
  if (currentDisplayTracks)
     currentDisplayTracks->ProcessKey(Key);
}

eOSState cDisplaySubtitleTracks::ProcessKey(eKeys Key)
{
  int oldTrack = track;
  switch (int(Key)) {
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
    case kSubtitles|k_Repeat:
    case kSubtitles:
         if (++track >= numTracks)
            track = 0;
         timeout.Set(TRACKTIMEOUT);
         break;
    case kOk:
         oldTrack = -1; // make sure we explicitly switch to that track
         timeout.Set();
         break;
    case kNone: break;
    default: if ((Key & k_Release) == 0)
                return osEnd;
    }
  if (track != oldTrack) {
     Show();
     cDevice::PrimaryDevice()->SetCurrentSubtitleTrack(types[track], true);
     if (track == ttNone && Setup.DisplaySubtitles == SUBTITLES_REWIND) {
        cDevice::PrimaryDevice()->SetCurrentSubtitleTrack(types[track], false);
        cDevice::PrimaryDevice()->EnsureSubtitleTrack();
        }
     }
  return timeout.TimedOut() ? osEnd : osContinue;
}

// --- cRecordControl --------------------------------------------------------

cRecordControl::cRecordControl(cDevice *Device, cTimers *Timers, cTimer *Timer, bool Pause)
{
  const char *LastReplayed = cReplayControl::LastReplayed(); // must do this before locking schedules!
  // Whatever happens here, the timers will be modified in some way...
  Timers->SetModified();
  cStateKey ChannelsStateKey;
  // To create a new timer, we need to make shure there is
  // a lock on Channels prior to the Schedules locking below
  if (!Timer)
     cChannels::GetChannelsRead(ChannelsStateKey);
  // We're going to work with an event here, so we need to prevent
  // others from modifying any EPG data:
  cStateKey SchedulesStateKey;
  cSchedules::GetSchedulesRead(SchedulesStateKey);

  event = NULL;
  fileName = NULL;
  recorder = NULL;
  device = Device;
  if (!device) device = cDevice::PrimaryDevice();//XXX
  timer = Timer;
  if (!timer) {
     timer = new cTimer(true, Pause);
     Timers->Add(timer);
     instantId = cString::sprintf(cDevice::NumDevices() > 1 ? "%s - %d" : "%s", timer->Channel()->Name(), device->DeviceNumber() + 1);
     ChannelsStateKey.Remove();
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
        Timers->Del(timer);
        if (!LastReplayed) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName);
        }
     timer = NULL;
     SchedulesStateKey.Remove();
     return;
     }

  cRecordingUserCommand::InvokeCommand(RUC_BEFORERECORDING, fileName);
  isyslog("record %s", fileName);
  if (MakeDirs(fileName, true)) {
     Recording.WriteInfo(); // we write this *before* attaching the recorder to the device, to make sure the info file is present when the recorder needs to update the fps value!
     const cChannel *ch = timer->Channel();
     recorder = new cRecorder(fileName, ch, timer->Priority());
     if (device->AttachReceiver(recorder)) {
        cStatus::MsgRecording(device, Recording.Name(), Recording.FileName(), true);
        if (!Timer && !LastReplayed) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName);
        SchedulesStateKey.Remove();
        LOCK_RECORDINGS_WRITE;
        SetRecordingTimerId(fileName, cString::sprintf("%d@%s", timer->Id(), Setup.SVDRPHostName));
        Recordings->AddByName(fileName);
        return;
        }
     else
        DELETENULL(recorder);
     }
  else
     timer->SetDeferred(DEFERTIMER);
  if (!Timer) {
     Timers->Del(timer);
     timer = NULL;
     }
  SchedulesStateKey.Remove();
}

cRecordControl::~cRecordControl()
{
  Stop();
  free(fileName);
}

#define INSTANT_REC_EPG_LOOKAHEAD 300 // seconds to look into the EPG data for an instant recording

bool cRecordControl::GetEvent(void)
{
  const cChannel *Channel = timer->Channel();
  time_t Time = timer->HasFlags(tfInstant) ? timer->StartTime() + INSTANT_REC_EPG_LOOKAHEAD : timer->StartTime() + (timer->StopTime() - timer->StartTime()) / 2;
  for (int seconds = 0; seconds <= MAXWAIT4EPGINFO; seconds++) {
      {
        LOCK_SCHEDULES_READ;
        if (const cSchedule *Schedule = Schedules->GetSchedule(Channel)) {
           event = Schedule->GetEventAround(Time);
           if (event) {
              if (seconds > 0)
                 dsyslog("got EPG info after %d seconds", seconds);
              return true;
              }
           }
      }
      if (seconds == 0)
         dsyslog("waiting for EPG info...");
      cCondWait::SleepMs(1000);
      }
  dsyslog("no EPG info available");
  return false;
}

void cRecordControl::Stop(bool ExecuteUserCommand)
{
  if (timer) {
     bool Finished = timer->HasFlags(tfActive) && !timer->Matches();
     if (recorder) {
        recorder->Stop();
        int Errors = recorder->Errors();
        isyslog("timer %s %s with %d error%s", *timer->ToDescr(), Finished ? "finished" : "stopped", Errors, Errors != 1 ? "s" : "");
        if (timer->HasFlags(tfAvoid) && Errors == 0 && Finished) {
           const char *p = strgetlast(timer->File(), FOLDERDELIMCHAR);
           DoneRecordingsPattern.Append(p);
           }
        }
     DELETENULL(recorder);
     timer->SetRecording(false);
     timer = NULL;
     SetRecordingTimerId(fileName, NULL);
     cStatus::MsgRecording(device, NULL, fileName, false);
     if (ExecuteUserCommand && Finished)
        cRecordingUserCommand::InvokeCommand(RUC_AFTERRECORDING, fileName);
     }
}

bool cRecordControl::Process(time_t t)
{
  if (!recorder || !recorder->IsAttached() || !timer || !timer->Matches(t)) {
     if (timer)
        timer->SetPending(false);
     return false;
     }
  return true;
}

// --- cRecordControls -------------------------------------------------------

cRecordControl *cRecordControls::RecordControls[MAXRECORDCONTROLS] = { NULL };
int cRecordControls::state = 0;

bool cRecordControls::Start(cTimers *Timers, cTimer *Timer, bool Pause)
{
  static time_t LastNoDiskSpaceMessage = 0;
  int FreeMB = 0;
  if (Timer) {
     AssertFreeDiskSpace(Timer->Priority(), !Timer->Pending());
     Timer->SetPending(true);
     }
  cVideoDirectory::VideoDiskSpace(&FreeMB);
  if (FreeMB < MINFREEDISK) {
     if (!Timer || time(NULL) - LastNoDiskSpaceMessage > NODISKSPACEDELTA) {
        isyslog("not enough disk space to start recording%s%s", Timer ? " timer " : "", Timer ? *Timer->ToDescr() : "");
        Skins.Message(mtWarning, tr("Not enough disk space to start recording!"));
        LastNoDiskSpaceMessage = time(NULL);
        }
     return false;
     }
  LastNoDiskSpaceMessage = 0;

  ChangeState();
  cStateKey StateKey;
  const cChannels *Channels = cChannels::GetChannelsRead(StateKey);
  int ch = Timer ? Timer->Channel()->Number() : cDevice::CurrentChannel();
  if (const cChannel *Channel = Channels->GetByNumber(ch)) {
     int Priority = Timer ? Timer->Priority() : Pause ? Setup.PausePriority : Setup.DefaultPriority;
     cDevice *device = cDevice::GetDevice(Channel, Priority, false);
     if (device) {
        dsyslog("switching device %d to channel %d %s (%s)", device->DeviceNumber() + 1, Channel->Number(), *Channel->GetChannelID().ToString(), Channel->Name());
        if (!device->SwitchChannel(Channel, false)) {
           StateKey.Remove();
           ShutdownHandler.RequestEmergencyExit();
           return false;
           }
        StateKey.Remove();
        Channels = NULL;
        if (!Timer || Timer->Matches()) {
           for (int i = 0; i < MAXRECORDCONTROLS; i++) {
               if (!RecordControls[i]) {
                  RecordControls[i] = new cRecordControl(device, Timers, Timer, Pause);
                  return RecordControls[i]->Process(time(NULL));
                  }
               }
           }
        }
     else if (!Timer || !Timer->Pending()) {
        isyslog("no free DVB device to record channel %d (%s)!", ch, Channel->Name());
        Skins.Message(mtError, tr("No free DVB device to record!"));
        }
     }
  else
     esyslog("ERROR: channel %d not defined!", ch);
  if (Channels)
     StateKey.Remove();
  return false;
}

bool cRecordControls::Start(bool Pause)
{
  LOCK_TIMERS_WRITE;
  return Start(Timers, NULL, Pause);
}

void cRecordControls::Stop(const char *InstantId)
{
  LOCK_TIMERS_WRITE;
  ChangeState();
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         const char *id = RecordControls[i]->InstantId();
         if (id && strcmp(id, InstantId) == 0) {
            cTimer *Timer = RecordControls[i]->Timer();
            RecordControls[i]->Stop();
            if (Timer) {
               Timers->Del(Timer);
               isyslog("deleted timer %s", *Timer->ToDescr());
               }
            break;
            }
         }
      }
}

void cRecordControls::Stop(cTimer *Timer)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (RecordControls[i]->Timer() == Timer) {
            DELETENULL(RecordControls[i]);
            ChangeState();
            break;
            }
         }
      }
}

bool cRecordControls::PauseLiveVideo(void)
{
  Skins.Message(mtStatus, tr("Pausing live video..."));
  cReplayControl::SetRecording(NULL); // make sure the new cRecordControl will set cReplayControl::LastReplayed()
  if (Start(true)) {
     cReplayControl *rc = new cReplayControl(true);
     cControl::Launch(rc);
     cControl::Attach();
     Skins.Message(mtStatus, NULL);
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
  if (FileName) {
     for (int i = 0; i < MAXRECORDCONTROLS; i++) {
         if (RecordControls[i] && strcmp(RecordControls[i]->FileName(), FileName) == 0)
            return RecordControls[i];
         }
     }
  return NULL;
}

cRecordControl *cRecordControls::GetRecordControl(const cTimer *Timer)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i] && RecordControls[i]->Timer() == Timer)
         return RecordControls[i];
      }
  return NULL;
}

bool cRecordControls::Process(cTimers *Timers, time_t t)
{
  bool Result = false;
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (!RecordControls[i]->Process(t)) {
            DELETENULL(RecordControls[i]);
            ChangeState();
            Result = true;
            }
         }
      }
  return Result;
}

void cRecordControls::ChannelDataModified(const cChannel *Channel)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (RecordControls[i]->Timer() && RecordControls[i]->Timer()->Channel() == Channel) {
            if (RecordControls[i]->Device()->ProvidesTransponder(Channel)) { // avoids retune on devices that don't really access the transponder
               isyslog("stopping recording due to modification of channel %d (%s)", Channel->Number(), Channel->Name());
               RecordControls[i]->Stop();
               // This will restart the recording, maybe even from a different
               // device in case conditional access has changed.
               ChangeState();
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
  ChangeState();
}

bool cRecordControls::StateChanged(int &State)
{
  int NewState = state;
  bool Result = State != NewState;
  State = state;
  return Result;
}

// --- cAdaptiveSkipper ------------------------------------------------------

cAdaptiveSkipper::cAdaptiveSkipper(void)
{
  initialValue = NULL;
  currentValue = 0;
  framesPerSecond = 0;
  lastKey = kNone;
}

void cAdaptiveSkipper::Initialize(int *InitialValue, double FramesPerSecond)
{
  initialValue = InitialValue;
  framesPerSecond = FramesPerSecond;
  currentValue = 0;
}

int cAdaptiveSkipper::GetValue(eKeys Key)
{
  if (!initialValue)
     return 0;
  if (timeout.TimedOut()) {
     currentValue = int(round(*initialValue * framesPerSecond));
     lastKey = Key;
     }
  else if (Key != lastKey) {
     currentValue /= 2;
     if (Setup.AdaptiveSkipAlternate)
        lastKey = Key; // only halve the value when the direction is changed
     else
        lastKey = kNone; // once the direction has changed, every further call halves the value
     }
  timeout.Set(Setup.AdaptiveSkipTimeout * 1000);
  return max(currentValue, 1);
}

// --- cReplayControl --------------------------------------------------------

cTimer *cReplayControl::timeshiftTimer = NULL;
cReplayControl *cReplayControl::currentReplayControl = NULL;
cString cReplayControl::fileName;

cReplayControl::cReplayControl(bool PauseLive)
:cDvbPlayerControl(fileName, PauseLive)
{
  cDevice::PrimaryDevice()->SetKeepTracks(PauseLive);
  currentReplayControl = this;
  displayReplay = NULL;
  marksModified = false;
  visible = modeOnly = shown = displayFrames = false;
  lastErrors = 0;
  lastCurrent = lastTotal = -1;
  lastPlay = lastForward = false;
  lastSpeed = -2; // an invalid value
  timeoutShow = 0;
  timeSearchActive = false;
  cRecording Recording(fileName);
  cStatus::MsgReplaying(this, Recording.Name(), Recording.FileName(), true);
  marks.Load(fileName, Recording.FramesPerSecond(), Recording.IsPesRecording());
  SetMarks(&marks);
  adaptiveSkipper.Initialize(&Setup.AdaptiveSkipInitial, Recording.FramesPerSecond());
  SetTrackDescriptions(false);
  if (Setup.ProgressDisplayTime)
     ShowTimed(Setup.ProgressDisplayTime);
}

cReplayControl::~cReplayControl()
{
  cDevice::PrimaryDevice()->SetKeepTracks(false);
  Stop();
  if (currentReplayControl == this)
     currentReplayControl = NULL;
}

void cReplayControl::DelTimeshiftTimer(void)
{
  if (timeshiftTimer) {
     LOCK_TIMERS_WRITE;
     Timers->SetExplicitModify();
     Timers->Del(timeshiftTimer);
     Timers->SetModified();
     isyslog("deleted timer %s", *timeshiftTimer->ToDescr());
     timeshiftTimer = NULL;
     }
}

void cReplayControl::Stop(void)
{
  Hide();
  cStatus::MsgReplaying(this, NULL, fileName, false);
  if (Setup.DelTimeshiftRec && *fileName) {
     cRecordControl* rc = cRecordControls::GetRecordControl(fileName);
     if (rc && rc->InstantId()) {
        if (Active()) {
           if (Setup.DelTimeshiftRec == 2 || Interface->Confirm(tr("Delete timeshift recording?"))) {
              // At this point somewhere up the call stack there may be a lock on the Channels, so we can't
              // lock the Timers here and have to delete this timer later:
              timeshiftTimer = rc->Timer();
              rc->Stop(false); // don't execute user command
              cDvbPlayerControl::Stop();
              bool Error = false;
              {
                LOCK_RECORDINGS_WRITE;
                Recordings->SetExplicitModify();
                if (cRecording *Recording = Recordings->GetByName(fileName)) {
                   if (Recording->Delete()) {
                      Recordings->DelByName(fileName);
                      ClearLastReplayed(fileName);
                      Recordings->SetModified();
                      }
                   else
                      Error = true;
                   }
              }
              if (Error)
                 Skins.Message(mtError, tr("Error while deleting recording!"));
              return;
              }
           }
        }
     }
  cDvbPlayerControl::Stop();
  cMenuRecordings::SetRecording(NULL); // make sure opening the Recordings menu navigates to the last replayed recording
}

void cReplayControl::ClearEditingMarks(void)
{
  cStateKey StateKey;
  marks.Lock(StateKey);
  while (cMark *m = marks.First())
        marks.Del(m);
  StateKey.Remove();
  cStatus::MsgMarksModified(NULL);
}

void cReplayControl::SetRecording(const char *FileName)
{
  fileName = FileName;
}

const char *cReplayControl::NowReplaying(void)
{
  return currentReplayControl ? *fileName : NULL;
}

const char *cReplayControl::LastReplayed(void)
{
  LOCK_RECORDINGS_READ;
  if (!Recordings->GetByName(fileName))
     fileName = NULL;
  return fileName;
}

void cReplayControl::ClearLastReplayed(const char *FileName)
{
  if (*fileName && FileName && strcmp(fileName, FileName) == 0)
     fileName = NULL;
}

void cReplayControl::ShowTimed(int Seconds)
{
  if (modeOnly)
     Hide();
  if (!visible) {
     shown = ShowProgress(true);
     timeoutShow = (shown && Seconds > 0) ? time(NULL) + Seconds : 0;
     }
  else if (timeoutShow && Seconds > 0)
     timeoutShow = time(NULL) + Seconds;
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
     SetNeedsFastResponse(false);
     visible = false;
     modeOnly = false;
     lastPlay = lastForward = false;
     lastSpeed = -2; // an invalid value
     timeSearchActive = false;
     timeoutShow = 0;
     }
  if (marksModified) {
     marks.Save();
     marksModified = false;
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
  if (!(Initial || updateTimer.TimedOut()))
     return visible;
  if (GetFrameNumber(Current, Total) && Total > 0) {
     if (!visible) {
        displayReplay = Skins.Current()->DisplayReplay(modeOnly);
        displayReplay->SetMarks(&marks);
        displayReplay->SetErrors(GetErrors());
        SetNeedsFastResponse(true);
        visible = true;
        }
     if (Initial) {
        if (*fileName) {
           LOCK_RECORDINGS_READ;
           if (const cRecording *Recording = Recordings->GetByName(fileName))
              displayReplay->SetRecording(Recording);
           }
        lastCurrent = lastTotal = -1;
        }
     const cErrors *Errors = GetErrors();
     int NumErrors = Errors ? Errors->Size() : 0;
     if (Current != lastCurrent || Total != lastTotal || NumErrors != lastErrors) {
        if (Setup.ShowRemainingTime || Total != lastTotal) {
           int Index = Total;
           if (Setup.ShowRemainingTime)
              Index = Current - Index;
           displayReplay->SetTotal(IndexToHMSF(Index, false, FramesPerSecond()));
           }
        displayReplay->SetProgress(Current, Total);
        displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames, FramesPerSecond()));
        displayReplay->SetErrors(Errors);
        displayReplay->Flush();
        lastCurrent = Current;
        lastTotal = Total;
        lastErrors = NumErrors;
        }
     ShowMode();
     updateTimer.Set(PROGRESSTIMEOUT);
     return true;
     }
  return false;
}

void cReplayControl::TimeSearchDisplay(void)
{
  char buf[64];
  // TRANSLATORS: note the trailing blank!
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
  int Current = int(round(lastCurrent / FramesPerSecond()));
  int Total = int(round(lastTotal / FramesPerSecond()));
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
    case kPlayPause:
    case kPlay:
    case kUp:
    case kPause:
    case kDown:
    case kOk:
         if (timeSearchPos > 0) {
            Seconds = min(Total - STAY_SECONDS_OFF_END, Seconds);
            bool Still = Key == kDown || Key == kPause || Key == kOk;
            Goto(SecondsToFrames(Seconds, FramesPerSecond()), Still);
            }
         timeSearchActive = false;
         break;
    default:
         if (!(Key & k_Flags)) // ignore repeat/release keys
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
     lastCurrent = -1; // triggers redisplay
     cStateKey StateKey;
     marks.Lock(StateKey);
     if (cMark *m = marks.Get(Current))
        marks.Del(m);
     else {
        marks.Add(Current);
        bool Play, Forward;
        int Speed;
        if (Setup.PauseOnMarkSet || GetReplayMode(Play, Forward, Speed) && !Play) {
           Goto(Current, true);
           displayFrames = true;
           }
        }
     StateKey.Remove();
     ShowTimed(2);
     marksModified = true;
     cStatus::MsgMarksModified(&marks);
     }
}

void cReplayControl::MarkJump(bool Forward)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     if (marks.Count()) {
        if (cMark *m = Forward ? marks.GetNext(Current) : marks.GetPrev(Current)) {
           if (!Setup.PauseOnMarkJump) {
              bool Playing, Fwd;
              int Speed;
              if (GetReplayMode(Playing, Fwd, Speed) && Playing && Forward && m->Position() < Total - SecondsToFrames(3, FramesPerSecond())) {
                 Goto(m->Position());
                 return;
                 }
              }
           Goto(m->Position(), true);
           displayFrames = true;
           return;
           }
        }
     // There are either no marks at all, or we already were at the first or last one,
     // so jump to the very beginning or end:
     Goto(Forward ? Total : 0, true);
     }
}

void cReplayControl::MarkMove(int Frames, bool MarkRequired)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     bool Play, Forward;
     int Speed;
     GetReplayMode(Play, Forward, Speed);
     cMark *m = marks.Get(Current);
     if (!Play && m) {
        displayFrames = true;
        cMark *m2;
        if (Frames > 0) {
           // Handle marks at the same offset:
           while ((m2 = marks.Next(m)) != NULL && m2->Position() == m->Position())
                 m = m2;
           // Don't skip the next mark:
           if ((m2 = marks.Next(m)) != NULL)
              Frames = min(Frames, m2->Position() - m->Position() - 1);
           }
        else {
           // Handle marks at the same offset:
           while ((m2 = marks.Prev(m)) != NULL && m2->Position() == m->Position())
                 m = m2;
           // Don't skip the next mark:
           if ((m2 = marks.Prev(m)) != NULL)
              Frames = -min(-Frames, m->Position() - m2->Position() - 1);
           }
        int p = SkipFrames(Frames);
        m->SetPosition(p);
        Goto(m->Position(), true);
        marksModified = true;
        cStatus::MsgMarksModified(&marks);
        }
     else if (!MarkRequired)
        Goto(SkipFrames(Frames), !Play);
     }
}

void cReplayControl::ErrorJump(bool Forward)
{
  const cErrors *Errors = GetErrors();
  int NumErrors = Errors ? Errors->Size() : 0;
  if (NumErrors > 0) {
     int Current, Total;
     if (GetIndex(Current, Total)) {
        if (Forward) {
           int Offset = 0;
           for (int i = 0; i < NumErrors; i++) {
               int Position = Errors->At(i);
               if (Position > Current + Offset) {
                  int NextIFrame = SkipFrames(Position - Current) + Offset; // this takes us to the I-frame at or right after Position
                  if (NextIFrame > Position) {
                     if (SkipFrames(Offset + 1) == NextIFrame) { // means Current is the I-frame right before Position
                        Offset = NextIFrame - Current;
                        continue;
                        }
                     }
                  Goto(Position, true); // this takes us to the I-frame at or right before Position
                  return;
                  }
               }
           if (Current < Total)
              Goto(Total, true);
           }
        else {
           for (int i = NumErrors - 1; i >= 0; i--) {
               if (Errors->At(i) < Current) {
                  int Position = Errors->At(i);
                  Goto(Position, true); // this takes us to the I-frame at or right before Position
                  return;
                  }
               }
           if (Current > 0)
              Goto(0, true);
           }
        }
     }
}

void cReplayControl::EditCut(void)
{
  if (*fileName) {
     Hide();
     if (!RecordingsHandler.GetUsage(fileName)) {
        if (!marks.Count())
           Skins.Message(mtError, tr("No editing marks defined!"));
        else if (!marks.GetNumSequences())
           Skins.Message(mtError, tr("No editing sequences defined!"));
        else if (access(cCutter::EditedFileName(fileName), F_OK) == 0 && !Interface->Confirm(tr("Edited version already exists - overwrite?")))
           ;
        else if (!EnoughFreeDiskSpaceForEdit(fileName))
           Skins.Message(mtError, tr("Not enough free disk space to start editing process!"));
        else if (!RecordingsHandler.Add(ruCut, fileName))
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
        if ((m->Index() & 0x01) != 0 && !Setup.SkipEdited) // when skipping edited parts we also need to jump to end marks
           m = marks.Next(m);
        if (m)
           Goto(m->Position() - SecondsToFrames(3, FramesPerSecond()));
        }
     }
}

cOsdObject *cReplayControl::GetInfo(void)
{
  LOCK_RECORDINGS_READ;
  if (const cRecording *Recording = Recordings->GetByName(cReplayControl::LastReplayed()))
     return new cMenuRecording(Recording, false);
  return NULL;
}

const cRecording *cReplayControl::GetRecording(void)
{
  LOCK_RECORDINGS_READ;
  if (const cRecording *Recording = Recordings->GetByName(LastReplayed()))
     return Recording;
  return NULL;
}

eOSState cReplayControl::ProcessKey(eKeys Key)
{
  if (!Active())
     return osEnd;
  if (Key == kNone && !marksModified)
     marks.Update();
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
  if (Key == kPlayPause) {
     bool Play, Forward;
     int Speed;
     GetReplayMode(Play, Forward, Speed);
     if (Speed >= 0)
        Key = Play ? kPlay : kPause;
     else
        Key = Play ? kPause : kPlay;
     }
  bool DoShowMode = true;
  switch (int(Key)) {
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
                   SkipSeconds(-Setup.SkipSecondsRepeat); break;
    case kGreen:   SkipSeconds(-Setup.SkipSeconds); break;
    case kYellow|k_Repeat:
                   SkipSeconds(Setup.SkipSecondsRepeat); break;
    case kYellow:  SkipSeconds(Setup.SkipSeconds); break;
    case kStop:
    case kBlue:    Stop();
                   return osEnd;
    default: {
      DoShowMode = false;
      switch (int(Key)) {
        // Editing:
        case kMarkToggle:      MarkToggle(); break;
        case kPrev|k_Repeat:
        case kPrev:            if (Setup.AdaptiveSkipPrevNext) {
                                  MarkMove(-adaptiveSkipper.GetValue(RAWKEY(Key)), false);
                                  break;
                                  }
                               // fall through...
        case kMarkJumpBack|k_Repeat:
        case kMarkJumpBack:    MarkJump(false); break;
        case kNext|k_Repeat:
        case kNext:            if (Setup.AdaptiveSkipPrevNext) {
                                  MarkMove(+adaptiveSkipper.GetValue(RAWKEY(Key)), false);
                                  break;
                                  }
                               // fall through...
        case kMarkJumpForward|k_Repeat:
        case kMarkJumpForward: MarkJump(true); break;
        case kMarkMoveBack|k_Repeat:
        case kMarkMoveBack:    MarkMove(-1, true); break;
        case kMarkMoveForward|k_Repeat:
        case kMarkMoveForward: MarkMove(+1, true); break;
        case kMarkSkipBack|k_Repeat:
        case kMarkSkipBack:    MarkMove(-adaptiveSkipper.GetValue(RAWKEY(Key)), false); break;
        case kMarkSkipForward|k_Repeat:
        case kMarkSkipForward: MarkMove(+adaptiveSkipper.GetValue(RAWKEY(Key)), false); break;
        case kChanUp|k_Repeat:
        case kChanUp:          ErrorJump(true); break;
        case kChanDn|k_Repeat:
        case kChanDn:          ErrorJump(false); break;
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
            case kBack:    Stop();
                           return osRecordings;
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
