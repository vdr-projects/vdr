/*
 * menu.c: The actual menu implementations
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: menu.c 2.82.1.2 2013/04/27 10:32:28 kls Exp $
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
#define CAMMENURETYTIMEOUT  3 // seconds after which opening the CAM menu is retried
#define CAMRESPONSETIMEOUT  5 // seconds to wait for a response from a CAM
#define MINFREEDISK       300 // minimum free disk space (in MB) required to start recording
#define NODISKSPACEDELTA  300 // seconds between "Not enough disk space to start recording!" messages
#define MAXCHNAMWIDTH      16 // maximum number of characters of channels' short names shown in schedules menus

#define CHNUMWIDTH  (numdigits(Channels.MaxNumber()) + 1)
#define CHNAMWIDTH  (min(MAXCHNAMWIDTH, Channels.MaxShortChannelNameLength() + 1))

// --- cMenuEditCaItem -------------------------------------------------------

class cMenuEditCaItem : public cMenuEditIntItem {
protected:
  virtual void Set(void);
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
  cChannel *channel;
  cChannel data;
  cSourceParam *sourceParam;
  char name[256];
  void Setup(void);
public:
  cMenuEditChannel(cChannel *Channel, bool New = false);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditChannel::cMenuEditChannel(cChannel *Channel, bool New)
:cOsdMenu(tr("Edit channel"), 16)
{
  SetMenuCategory(mcChannelEdit);
  channel = Channel;
  sourceParam = NULL;
  *name = 0;
  if (channel) {
     data = *channel;
     strn0cpy(name, data.name, sizeof(name));
     if (New) {
        channel = NULL;
        data.nid = 0;
        data.tid = 0;
        data.rid = 0;
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
  /* XXX not yet used
  Add(new cMenuEditIntItem( tr("Nid"),          &data.nid, 0));
  Add(new cMenuEditIntItem( tr("Tid"),          &data.tid, 0));
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
        if (sourceParam)
           sourceParam->GetData(&data);
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
  if (Key != kNone && (data.source & cSource::st_Mask) != (oldSource & cSource::st_Mask)) {
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
  cChannel *channel;
public:
  cMenuChannelItem(cChannel *Channel);
  static void SetSortMode(eChannelSortMode SortMode) { sortMode = SortMode; }
  static void IncSortMode(void) { sortMode = eChannelSortMode((sortMode == csmProvider) ? csmNumber : sortMode + 1); }
  static eChannelSortMode SortMode(void) { return sortMode; }
  virtual int Compare(const cListObject &ListObject) const;
  virtual void Set(void);
  cChannel *Channel(void) { return channel; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);
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
  cString buffer;
  if (!channel->GroupSep()) {
     if (sortMode == csmProvider)
        buffer = cString::sprintf("%d\t%s - %s", channel->Number(), channel->Provider(), channel->Name());
     else
        buffer = cString::sprintf("%d\t%s", channel->Number(), channel->Name());
     }
  else
     buffer = cString::sprintf("---\t%s ----------------------------------------------------------------", channel->Name());
  SetText(buffer);
}

void cMenuChannelItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  if (!DisplayMenu->SetItemChannel(channel, Index, Current, Selectable, sortMode == csmProvider))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuChannels ---------------------------------------------------------

#define CHANNELNUMBERTIMEOUT 1000 //ms

class cMenuChannels : public cOsdMenu {
private:
  int number;
  cTimeMs numberTimer;
  void Setup(void);
  cChannel *GetChannel(int Index);
  void Propagate(void);
protected:
  eOSState Number(eKeys Key);
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
  SetMenuCategory(mcChannel);
  number = 0;
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
  SetHelp(tr("Button$Edit"), tr("Button$New"), tr("Button$Delete"), tr("Button$Mark"));
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

eOSState cMenuChannels::Number(eKeys Key)
{
  if (HasSubMenu())
     return osContinue;
  if (numberTimer.TimedOut())
     number = 0;
  if (!number && Key == k0) {
     cMenuChannelItem::IncSortMode();
     Setup();
     }
  else {
     number = number * 10 + Key - k0;
     for (cMenuChannelItem *ci = (cMenuChannelItem *)First(); ci; ci = (cMenuChannelItem *)ci->Next()) {
         if (!ci->Channel()->GroupSep() && ci->Channel()->Number() == number) {
            SetCurrent(ci);
            Display();
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
     int CurrentChannelNr = cDevice::CurrentChannel();
     cChannel *CurrentChannel = Channels.GetByNumber(CurrentChannelNr);
     int Index = Current();
     cChannel *channel = GetChannel(Current());
     int DeletedChannel = channel->Number();
     // Check if there is a timer using this channel:
     if (channel->HasTimer()) {
        Skins.Message(mtError, tr("Channel is being used by a timer!"));
        return osContinue;
        }
     if (Interface->Confirm(tr("Delete channel?"))) {
        if (CurrentChannel && channel == CurrentChannel) {
           int n = Channels.GetNextNormal(CurrentChannel->Index());
           if (n < 0)
              n = Channels.GetPrevNormal(CurrentChannel->Index());
           CurrentChannel = Channels.Get(n);
           CurrentChannelNr = 0; // triggers channel switch below
           }
        Channels.Del(channel);
        cOsdMenu::Del(Index);
        Propagate();
        Channels.SetModified(true);
        isyslog("channel %d deleted", DeletedChannel);
        if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
           if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
              Channels.SwitchTo(CurrentChannel->Number());
           else
              cDevice::SetCurrentChannel(CurrentChannel);
           }
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
     Channels.SetModified(true);
     isyslog("channel %d moved to %d", FromNumber, ToNumber);
     if (CurrentChannel && CurrentChannel->Number() != CurrentChannelNr) {
        if (!cDevice::PrimaryDevice()->Replaying() || cDevice::PrimaryDevice()->Transferring())
           Channels.SwitchTo(CurrentChannel->Number());
        else
           cDevice::SetCurrentChannel(CurrentChannel);
        }
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
              case k0 ... k9:
                            return Number(Key);
              case kOk:     return Switch();
              case kRed:    return Edit();
              case kGreen:  return New();
              case kYellow: return Delete();
              case kBlue:   if (!HasSubMenu())
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
  cMenuFolderItem(cNestedItem *Folder);
  cNestedItem *Folder(void) { return folder; }
  };

cMenuFolderItem::cMenuFolderItem(cNestedItem *Folder)
:cOsdItem(Folder->Text())
{
  folder = Folder;
  if (folder->SubItems())
     SetText(cString::sprintf("%s...", folder->Text()));
}

// --- cMenuEditFolder -------------------------------------------------------

class cMenuEditFolder : public cOsdMenu {
private:
  cList<cNestedItem> *list;
  cNestedItem *folder;
  char name[PATH_MAX];
  int subFolder;
  eOSState Confirm(void);
public:
  cMenuEditFolder(const char *Dir, cList<cNestedItem> *List, cNestedItem *Folder = NULL);
  cString GetFolder(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuEditFolder::cMenuEditFolder(const char *Dir, cList<cNestedItem> *List, cNestedItem *Folder)
:cOsdMenu(Folder ? tr("Edit folder") : tr("New folder"), 12)
{
  SetMenuCategory(mcFolder);
  list = List;
  folder = Folder;
  if (folder) {
     strn0cpy(name, folder->Text(), sizeof(name));
     subFolder = folder->SubItems() != NULL;
     }
  else {
     *name = 0;
     subFolder = 0;
     cRemote::Put(kRight, true); // go right into string editing mode
     }
  if (!isempty(Dir)) {
     cOsdItem *DirItem = new cOsdItem(Dir);
     DirItem->SetSelectable(false);
     Add(DirItem);
     }
  Add(new cMenuEditStrItem( tr("Name"), name, sizeof(name)));
  Add(new cMenuEditBoolItem(tr("Sub folder"), &subFolder));
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
  if (folder) {
     folder->SetText(name);
     folder->SetSubItems(subFolder);
     }
  else
     list->Add(folder = new cNestedItem(name, subFolder));
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
  Set();
  SetHelpKeys();
  DescendPath(Path);
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
  Set();
  SetHelpKeys();
  DescendPath(Path);
}

void cMenuFolder::SetHelpKeys(void)
{
  SetHelp(firstFolder ? tr("Button$Select") : NULL, tr("Button$New"), firstFolder ? tr("Button$Delete") : NULL, firstFolder ? tr("Button$Edit") : NULL);
}

void cMenuFolder::Set(const char *CurrentFolder)
{
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
               if (Folder->Folder()->SubItems())
                  AddSubMenu(new cMenuFolder(Title(), Folder->Folder()->SubItems(), nestedItemList, !isempty(dir) ? *cString::sprintf("%s%c%s", *dir, FOLDERDELIMCHAR, Folder->Folder()->Text()) : Folder->Folder()->Text(), p + 1));
               break;
               }
            }
        }
    }
}

eOSState cMenuFolder::Select(void)
{
  if (firstFolder) {
     cMenuFolderItem *Folder = (cMenuFolderItem *)Get(Current());
     if (Folder) {
        if (Folder->Folder()->SubItems())
           return AddSubMenu(new cMenuFolder(Title(), Folder->Folder()->SubItems(), nestedItemList, !isempty(dir) ? *cString::sprintf("%s%c%s", *dir, FOLDERDELIMCHAR, Folder->Folder()->Text()) : Folder->Folder()->Text()));
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
  cMenuEditFolder *mef = (cMenuEditFolder *)SubMenu();
  if (mef) {
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
        cMenuFolder *mf = (cMenuFolder *)SubMenu();
        if (mf)
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
       case kOk:
       case kRed:    return Select();
       case kGreen:  return New();
       case kYellow: return Delete();
       case kBlue:   return Edit();
       default:      state = osContinue;
       }
     }
  else if (state == osEnd && HasSubMenu() && editing)
     state = SetFolder();
  return state;
}

// --- cMenuEditTimer --------------------------------------------------------

cMenuEditTimer::cMenuEditTimer(cTimer *Timer, bool New)
:cOsdMenu(tr("Edit timer"), 12)
{
  SetMenuCategory(mcTimerEdit);
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
     }
  SetHelpKeys();
  Timers.IncBeingEdited();
}

cMenuEditTimer::~cMenuEditTimer()
{
  if (timer && addIfConfirmed)
     delete timer; // apparently it wasn't confirmed
  Timers.DecBeingEdited();
}

void cMenuEditTimer::SetHelpKeys(void)
{
  SetHelp(tr("Button$Folder"), data.weekdays ? tr("Button$Single") : tr("Button$Repeating"));
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

eOSState cMenuEditTimer::SetFolder(void)
{
  cMenuFolder *mf = (cMenuFolder *)SubMenu();
  if (mf) {
     cString Folder = mf->GetFolder();
     char *p = strrchr(data.file, FOLDERDELIMCHAR);
     if (p)
        p++;
     else
        p = data.file;
     if (!isempty(*Folder))
        strn0cpy(data.file, cString::sprintf("%s%c%s", *Folder, FOLDERDELIMCHAR, p), sizeof(data.file));
     else if (p != data.file)
        memmove(data.file, p, strlen(p) + 1);
     SetCurrent(file);
     Display();
     }
  return CloseSubMenu();
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
                          if (memcmp(timer, &data, sizeof(data)) != 0)
                             *timer = data;
                          if (addIfConfirmed)
                             Timers.Add(timer);
                          timer->SetEventFromSchedule();
                          timer->Matches();
                          Timers.SetModified();
                          isyslog("timer %s %s (%s)", *timer->ToDescr(), addIfConfirmed ? "added" : "modified", timer->HasFlags(tfActive) ? "active" : "inactive");
                          addIfConfirmed = false;
                          }
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
       case kYellow:
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
  cTimer *timer;
public:
  cMenuTimerItem(cTimer *Timer);
  virtual int Compare(const cListObject &ListObject) const;
  virtual void Set(void);
  cTimer *Timer(void) { return timer; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);
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
  const char *File = Setup.FoldersInTimerMenu ? NULL : strrchr(timer->File(), FOLDERDELIMCHAR);
  if (File && strcmp(File + 1, TIMERMACRO_TITLE) && strcmp(File + 1, TIMERMACRO_EPISODE))
     File++;
  else
     File = timer->File();
  SetText(cString::sprintf("%c\t%d\t%s%s%s\t%02d:%02d\t%02d:%02d\t%s",
                    !(timer->HasFlags(tfActive)) ? ' ' : timer->FirstDay() ? '!' : timer->Recording() ? '#' : '>',
                    timer->Channel()->Number(),
                    *name,
                    *name && **name ? " " : "",
                    *day,
                    timer->Start() / 100,
                    timer->Start() % 100,
                    timer->Stop() / 100,
                    timer->Stop() % 100,
                    File));
}

void cMenuTimerItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  if (!DisplayMenu->SetItemTimer(timer, Index, Current, Selectable))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuTimers -----------------------------------------------------------

class cMenuTimers : public cOsdMenu {
private:
  int helpKeys;
  eOSState Edit(void);
  eOSState New(void);
  eOSState Delete(void);
  eOSState OnOff(void);
  eOSState Info(void);
  cTimer *CurrentTimer(void);
  void SetHelpKeys(void);
public:
  cMenuTimers(void);
  virtual ~cMenuTimers();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuTimers::cMenuTimers(void)
:cOsdMenu(tr("Timers"), 2, CHNUMWIDTH, 10, 6, 6)
{
  SetMenuCategory(mcTimer);
  helpKeys = -1;
  for (cTimer *timer = Timers.First(); timer; timer = Timers.Next(timer)) {
      timer->SetEventFromSchedule(); // make sure the event is current
      Add(new cMenuTimerItem(timer));
      }
  Sort();
  SetCurrent(First());
  SetHelpKeys();
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

void cMenuTimers::SetHelpKeys(void)
{
  int NewHelpKeys = 0;
  cTimer *timer = CurrentTimer();
  if (timer) {
     if (timer->Event())
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
  cTimer *timer = CurrentTimer();
  if (timer) {
     timer->OnOff();
     timer->SetEventFromSchedule();
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

eOSState cMenuTimers::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cTimer *ti = CurrentTimer();
  if (ti && ti->Event())
     return AddSubMenu(new cMenuEvent(ti->Event()));
  return osContinue;
}

eOSState cMenuTimers::ProcessKey(eKeys Key)
{
  int TimerNumber = HasSubMenu() ? Count() : -1;
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
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
  if (TimerNumber >= 0 && !HasSubMenu() && Timers.Get(TimerNumber)) {
     // a newly created timer was confirmed with Ok
     Add(new cMenuTimerItem(Timers.Get(TimerNumber)), true);
     Display();
     }
  if (Key != kNone)
     SetHelpKeys();
  return state;
}

// --- cMenuEvent ------------------------------------------------------------

cMenuEvent::cMenuEvent(const cEvent *Event, bool CanSwitch, bool Buttons)
:cOsdMenu(tr("Event"))
{
  SetMenuCategory(mcEvent);
  event = Event;
  if (event) {
     cChannel *channel = Channels.GetByChannelID(event->ChannelID(), true);
     if (channel) {
        SetTitle(channel->Name());
        eTimerMatch TimerMatch = tmNone;
        Timers.GetMatch(event, &TimerMatch);
        if (Buttons)
           SetHelp(TimerMatch == tmFull ? tr("Button$Timer") : tr("Button$Record"), NULL, NULL, CanSwitch ? tr("Button$Switch") : NULL);
        }
     }
}

void cMenuEvent::Display(void)
{
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
  bool withDate;
  eTimerMatch timerMatch;
  cMenuScheduleItem(const cEvent *Event, cChannel *Channel = NULL, bool WithDate = false);
  static void SetSortMode(eScheduleSortMode SortMode) { sortMode = SortMode; }
  static void IncSortMode(void) { sortMode = eScheduleSortMode((sortMode == ssmAllAll) ? ssmAllThis : sortMode + 1); }
  static eScheduleSortMode SortMode(void) { return sortMode; }
  virtual int Compare(const cListObject &ListObject) const;
  bool Update(bool Force = false);
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);
  };

cMenuScheduleItem::eScheduleSortMode cMenuScheduleItem::sortMode = ssmAllThis;

cMenuScheduleItem::cMenuScheduleItem(const cEvent *Event, cChannel *Channel, bool WithDate)
{
  event = Event;
  channel = Channel;
  withDate = WithDate;
  timerMatch = tmNone;
  Update(true);
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

static const char *TimerMatchChars = " tT";

bool cMenuScheduleItem::Update(bool Force)
{
  bool result = false;
  eTimerMatch OldTimerMatch = timerMatch;
  Timers.GetMatch(event, &timerMatch);
  if (Force || timerMatch != OldTimerMatch) {
     cString buffer;
     char t = TimerMatchChars[timerMatch];
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
     result = true;
     }
  return result;
}

void cMenuScheduleItem::SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable)
{
  if (!DisplayMenu->SetItemEvent(event, Index, Current, Selectable, channel, withDate, timerMatch))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuWhatsOn ----------------------------------------------------------

class cMenuWhatsOn : public cOsdMenu {
private:
  bool now;
  int helpKeys;
  int timerState;
  eOSState Record(void);
  eOSState Switch(void);
  static int currentChannel;
  static const cEvent *scheduleEvent;
  bool Update(void);
  void SetHelpKeys(void);
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
:cOsdMenu(Now ? tr("What's on now?") : tr("What's on next?"), CHNUMWIDTH, CHNAMWIDTH, 6, 4)
{
  SetMenuCategory(Now ? mcScheduleNow : mcScheduleNext);
  now = Now;
  helpKeys = -1;
  timerState = 0;
  Timers.Modified(timerState);
  for (cChannel *Channel = Channels.First(); Channel; Channel = Channels.Next(Channel)) {
      if (!Channel->GroupSep()) {
         const cSchedule *Schedule = Schedules->GetSchedule(Channel);
         if (Schedule) {
            const cEvent *Event = Now ? Schedule->GetPresentEvent() : Schedule->GetFollowingEvent();
            if (Event)
               Add(new cMenuScheduleItem(Event, Channel), Channel->Number() == CurrentChannelNr);
            }
         }
      }
  currentChannel = CurrentChannelNr;
  Display();
  SetHelpKeys();
}

bool cMenuWhatsOn::Update(void)
{
  bool result = false;
  if (Timers.Modified(timerState)) {
     for (cOsdItem *item = First(); item; item = Next(item)) {
         if (((cMenuScheduleItem *)item)->Update())
            result = true;
         }
     }
  return result;
}

void cMenuWhatsOn::SetHelpKeys(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  int NewHelpKeys = 0;
  if (item) {
     if (item->timerMatch == tmFull)
        NewHelpKeys = 2;
     else
        NewHelpKeys = 1;
     }
  if (NewHelpKeys != helpKeys) {
     const char *Red[] = { NULL, tr("Button$Record"), tr("Button$Timer") };
     SetHelp(Red[NewHelpKeys], now ? tr("Button$Next") : tr("Button$Now"), tr("Button$Schedule"), tr("Button$Switch"));
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
     cChannel *channel = Channels.GetByChannelID(item->event->ChannelID(), true);
     if (channel && cDevice::PrimaryDevice()->SwitchChannel(channel, true))
        return osEnd;
     }
  Skins.Message(mtError, tr("Can't switch channel!"));
  return osContinue;
}

eOSState cMenuWhatsOn::Record(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     if (item->timerMatch == tmFull) {
        eTimerMatch tm = tmNone;
        cTimer *timer = Timers.GetMatch(item->event, &tm);
        if (timer)
           return AddSubMenu(new cMenuEditTimer(timer));
        }
     cTimer *timer = new cTimer(item->event);
     cTimer *t = Timers.GetTimer(timer);
     if (t) {
        delete timer;
        timer = t;
        return AddSubMenu(new cMenuEditTimer(timer));
        }
     else {
        Timers.Add(timer);
        Timers.SetModified();
        isyslog("timer %s added (active)", *timer->ToDescr());
        if (timer->Matches(0, false, NEWTIMERLIMIT))
           return AddSubMenu(new cMenuEditTimer(timer));
        if (HasSubMenu())
           CloseSubMenu();
        if (Update())
           Display();
        SetHelpKeys();
        }
     }
  return osContinue;
}

eOSState cMenuWhatsOn::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case kRecord:
       case kRed:    return Record();
       case kYellow: state = osBack;
                     // continue with kGreen
       case kGreen:  {
                       cMenuScheduleItem *mi = (cMenuScheduleItem *)Get(Current());
                       if (mi) {
                          scheduleEvent = mi->event;
                          currentChannel = mi->channel->Number();
                          }
                     }
                     break;
       case kBlue:   return Switch();
       case kInfo:
       case kOk:     if (Count())
                        return AddSubMenu(new cMenuEvent(((cMenuScheduleItem *)Get(Current()))->event, true, true));
                     break;
       default:      break;
       }
     }
  else if (!HasSubMenu()) {
     if (HadSubMenu && Update())
        Display();
     if (Key != kNone)
        SetHelpKeys();
     }
  return state;
}

// --- cMenuSchedule ---------------------------------------------------------

class cMenuSchedule : public cOsdMenu {
private:
  cSchedulesLock schedulesLock;
  const cSchedules *schedules;
  bool now, next;
  int otherChannel;
  int helpKeys;
  int timerState;
  eOSState Number(void);
  eOSState Record(void);
  eOSState Switch(void);
  void PrepareScheduleAllThis(const cEvent *Event, const cChannel *Channel);
  void PrepareScheduleThisThis(const cEvent *Event, const cChannel *Channel);
  void PrepareScheduleThisAll(const cEvent *Event, const cChannel *Channel);
  void PrepareScheduleAllAll(const cEvent *Event, const cChannel *Channel);
  bool Update(void);
  void SetHelpKeys(void);
public:
  cMenuSchedule(void);
  virtual ~cMenuSchedule();
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSchedule::cMenuSchedule(void)
:cOsdMenu("")
{
  SetMenuCategory(mcSchedule);
  now = next = false;
  otherChannel = 0;
  helpKeys = -1;
  timerState = 0;
  Timers.Modified(timerState);
  cMenuScheduleItem::SetSortMode(cMenuScheduleItem::ssmAllThis);
  cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
  if (channel) {
     cMenuWhatsOn::SetCurrentChannel(channel->Number());
     schedules = cSchedules::Schedules(schedulesLock);
     PrepareScheduleAllThis(NULL, channel);
     SetHelpKeys();
     }
}

cMenuSchedule::~cMenuSchedule()
{
  cMenuWhatsOn::ScheduleEvent(); // makes sure any posted data is cleared
}

void cMenuSchedule::PrepareScheduleAllThis(const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(7, 6, 4);
  SetTitle(cString::sprintf(tr("Schedule - %s"), Channel->Name()));
  if (schedules && Channel) {
     const cSchedule *Schedule = schedules->GetSchedule(Channel);
     if (Schedule) {
        const cEvent *PresentEvent = Event ? Event : Schedule->GetPresentEvent();
        time_t now = time(NULL) - Setup.EPGLinger * 60;
        for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
            if (ev->EndTime() > now || ev == PresentEvent)
               Add(new cMenuScheduleItem(ev), ev == PresentEvent);
            }
        }
     }
}

void cMenuSchedule::PrepareScheduleThisThis(const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(7, 6, 4);
  SetTitle(cString::sprintf(tr("This event - %s"), Channel->Name()));
  if (schedules && Channel && Event) {
     const cSchedule *Schedule = schedules->GetSchedule(Channel);
     if (Schedule) {
        time_t now = time(NULL) - Setup.EPGLinger * 60;
        for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
            if ((ev->EndTime() > now || ev == Event) && !strcmp(ev->Title(), Event->Title()))
               Add(new cMenuScheduleItem(ev), ev == Event);
            }
        }
     }
}

void cMenuSchedule::PrepareScheduleThisAll(const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(CHNUMWIDTH, CHNAMWIDTH, 7, 6, 4);
  SetTitle(tr("This event - all channels"));
  if (schedules && Event) {
     for (cChannel *ch = Channels.First(); ch; ch = Channels.Next(ch)) {
         const cSchedule *Schedule = schedules->GetSchedule(ch);
         if (Schedule) {
            time_t now = time(NULL) - Setup.EPGLinger * 60;
            for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
                if ((ev->EndTime() > now || ev == Event) && !strcmp(ev->Title(), Event->Title()))
                   Add(new cMenuScheduleItem(ev, ch, true), ev == Event && ch == Channel);
                }
            }
         }
     }
}

void cMenuSchedule::PrepareScheduleAllAll(const cEvent *Event, const cChannel *Channel)
{
  Clear();
  SetCols(CHNUMWIDTH, CHNAMWIDTH, 7, 6, 4);
  SetTitle(tr("All events - all channels"));
  if (schedules) {
     for (cChannel *ch = Channels.First(); ch; ch = Channels.Next(ch)) {
         const cSchedule *Schedule = schedules->GetSchedule(ch);
         if (Schedule) {
            time_t now = time(NULL) - Setup.EPGLinger * 60;
            for (const cEvent *ev = Schedule->Events()->First(); ev; ev = Schedule->Events()->Next(ev)) {
                if (ev->EndTime() > now || ev == Event)
                   Add(new cMenuScheduleItem(ev, ch, true), ev == Event && ch == Channel);
                }
            }
         }
     }
}

bool cMenuSchedule::Update(void)
{
  bool result = false;
  if (Timers.Modified(timerState)) {
     for (cOsdItem *item = First(); item; item = Next(item)) {
         if (((cMenuScheduleItem *)item)->Update())
            result = true;
         }
     }
  return result;
}

void cMenuSchedule::SetHelpKeys(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  int NewHelpKeys = 0;
  if (item) {
     if (item->timerMatch == tmFull)
        NewHelpKeys = 2;
     else
        NewHelpKeys = 1;
     }
  if (NewHelpKeys != helpKeys) {
     const char *Red[] = { NULL, tr("Button$Record"), tr("Button$Timer") };
     SetHelp(Red[NewHelpKeys], tr("Button$Now"), tr("Button$Next"));
     helpKeys = NewHelpKeys;
     }
}

eOSState cMenuSchedule::Number(void)
{
  cMenuScheduleItem::IncSortMode();
  cMenuScheduleItem *CurrentItem = (cMenuScheduleItem *)Get(Current());
  const cChannel *Channel = NULL;
  const cEvent *Event = NULL;
  if (CurrentItem) {
     Event = CurrentItem->event;
     Channel = Channels.GetByChannelID(Event->ChannelID(), true);
     }
  else
     Channel = Channels.GetByNumber(cDevice::CurrentChannel());
  switch (cMenuScheduleItem::SortMode()) {
    case cMenuScheduleItem::ssmAllThis:  PrepareScheduleAllThis(Event, Channel); break;
    case cMenuScheduleItem::ssmThisThis: PrepareScheduleThisThis(Event, Channel); break;
    case cMenuScheduleItem::ssmThisAll:  PrepareScheduleThisAll(Event, Channel); break;
    case cMenuScheduleItem::ssmAllAll:   PrepareScheduleAllAll(Event, Channel); break;
    default: esyslog("ERROR: unknown SortMode %d (%s %d)", cMenuScheduleItem::SortMode(), __FUNCTION__, __LINE__);
    }
  CurrentItem = (cMenuScheduleItem *)Get(Current());
  Sort();
  SetCurrent(CurrentItem);
  Display();
  return osContinue;
}

eOSState cMenuSchedule::Record(void)
{
  cMenuScheduleItem *item = (cMenuScheduleItem *)Get(Current());
  if (item) {
     if (item->timerMatch == tmFull) {
        eTimerMatch tm = tmNone;
        cTimer *timer = Timers.GetMatch(item->event, &tm);
        if (timer)
           return AddSubMenu(new cMenuEditTimer(timer));
        }
     cTimer *timer = new cTimer(item->event);
     cTimer *t = Timers.GetTimer(timer);
     if (t) {
        delete timer;
        timer = t;
        return AddSubMenu(new cMenuEditTimer(timer));
        }
     else {
        Timers.Add(timer);
        Timers.SetModified();
        isyslog("timer %s added (active)", *timer->ToDescr());
        if (timer->Matches(0, false, NEWTIMERLIMIT))
           return AddSubMenu(new cMenuEditTimer(timer));
        if (HasSubMenu())
           CloseSubMenu();
        if (Update())
           Display();
        SetHelpKeys();
        }
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
  bool HadSubMenu = HasSubMenu();
  eOSState state = cOsdMenu::ProcessKey(Key);

  if (state == osUnknown) {
     switch (Key) {
       case k0:      return Number();
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
       case kBlue:   if (Count() && otherChannel)
                        return Switch();
                     break;
       case kInfo:
       case kOk:     if (Count())
                        return AddSubMenu(new cMenuEvent(((cMenuScheduleItem *)Get(Current()))->event, otherChannel, true));
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
           cMenuScheduleItem::SetSortMode(cMenuScheduleItem::ssmAllThis);
           PrepareScheduleAllThis(NULL, channel);
           if (channel->Number() != cDevice::CurrentChannel()) {
              otherChannel = channel->Number();
              SetHelp(Count() ? tr("Button$Record") : NULL, tr("Button$Now"), tr("Button$Next"), tr("Button$Switch"));
              }
           Display();
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
  virtual ~cMenuCam();
  virtual eOSState ProcessKey(eKeys Key);
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
     if (*ciMenu->SubTitleText()) {
        dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->SubTitleText());
        AddMultiLineItem(ciMenu->SubTitleText());
        offset = Count();
        }
     for (int i = 0; i < ciMenu->NumEntries(); i++) {
         Add(new cOsdItem(hk(ciMenu->Entry(i)), osUnknown, ciMenu->Selectable()));
         dsyslog("CAM %d: '%s'", camSlot->SlotNumber(), ciMenu->Entry(i));
         }
     if (*ciMenu->BottomText()) {
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
     GenerateTitle();
     Add(new cOsdItem(ciEnquiry->Text(), osUnknown, false));
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

// --- cMenuRecording --------------------------------------------------------

class cMenuRecording : public cOsdMenu {
private:
  const cRecording *recording;
  bool withButtons;
public:
  cMenuRecording(const cRecording *Recording, bool WithButtons = false);
  virtual void Display(void);
  virtual eOSState ProcessKey(eKeys Key);
};

cMenuRecording::cMenuRecording(const cRecording *Recording, bool WithButtons)
:cOsdMenu(tr("Recording info"))
{
  SetMenuCategory(mcRecordingInfo);
  recording = Recording;
  withButtons = WithButtons;
  if (withButtons)
     SetHelp(tr("Button$Play"), tr("Button$Rewind"));
}

void cMenuRecording::Display(void)
{
  cOsdMenu::Display();
  DisplayMenu()->SetRecording(recording);
  if (recording->Info()->Description())
     cStatus::MsgOsdTextItem(recording->Info()->Description());
}

eOSState cMenuRecording::ProcessKey(eKeys Key)
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
       case kRed:    if (withButtons)
                        Key = kOk; // will play the recording, even if recording commands are defined
       case kGreen:  if (!withButtons)
                        break;
                     cRemote::Put(Key, true);
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
  cRecording *recording;
  int level;
  char *name;
  int totalEntries, newEntries;
public:
  cMenuRecordingItem(cRecording *Recording, int Level);
  ~cMenuRecordingItem();
  void IncrementCounter(bool New);
  const char *Name(void) { return name; }
  cRecording *Recording(void) { return recording; }
  bool IsDirectory(void) { return name != NULL; }
  virtual void SetMenuItem(cSkinDisplayMenu *DisplayMenu, int Index, bool Current, bool Selectable);
  };

cMenuRecordingItem::cMenuRecordingItem(cRecording *Recording, int Level)
{
  recording = Recording;
  level = Level;
  name = NULL;
  totalEntries = newEntries = 0;
  SetText(Recording->Title('\t', true, Level));
  if (*Text() == '\t')
     name = strdup(Text() + 2); // 'Text() + 2' to skip the two '\t'
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
  if (!DisplayMenu->SetItemRecording(recording, Index, Current, Selectable, level, totalEntries, newEntries))
     DisplayMenu->SetItem(Text(), Index, Current, Selectable);
}

// --- cMenuRecordings -------------------------------------------------------

cMenuRecordings::cMenuRecordings(const char *Base, int Level, bool OpenSubMenus)
:cOsdMenu(Base ? Base : tr("Recordings"), 9, 6, 6)
{
  SetMenuCategory(mcRecording);
  base = Base ? strdup(Base) : NULL;
  level = Setup.RecordingDirs ? Level : -1;
  Recordings.StateChanged(recordingsState); // just to get the current state
  helpKeys = -1;
  Display(); // this keeps the higher level menus from showing up briefly when pressing 'Back' during replay
  Set();
  if (Current() < 0)
     SetCurrent(First());
  else if (OpenSubMenus && cReplayControl::LastReplayed() && Open(true))
     return;
  Display();
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
  int NewHelpKeys = 0;
  if (ri) {
     if (ri->IsDirectory())
        NewHelpKeys = 1;
     else {
        NewHelpKeys = 2;
        if (ri->Recording()->Info()->Title())
           NewHelpKeys = 3;
        }
     }
  if (NewHelpKeys != helpKeys) {
     switch (NewHelpKeys) {
       case 0: SetHelp(NULL); break;
       case 1: SetHelp(tr("Button$Open")); break;
       case 2:
       case 3: SetHelp(RecordingCommands.Count() ? tr("Commands") : tr("Button$Play"), tr("Button$Rewind"), tr("Button$Delete"), NewHelpKeys == 3 ? tr("Button$Info") : NULL);
       default: ;
       }
     helpKeys = NewHelpKeys;
     }
}

void cMenuRecordings::Set(bool Refresh)
{
  const char *CurrentRecording = cReplayControl::LastReplayed();
  cMenuRecordingItem *LastItem = NULL;
  cThreadLock RecordingsLock(&Recordings);
  if (Refresh) {
     if (cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current()))
        CurrentRecording = ri->Recording()->FileName();
     }
  Clear();
  GetRecordingsSortMode(DirectoryName());
  Recordings.Sort();
  for (cRecording *recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
      if (!base || (strstr(recording->Name(), base) == recording->Name() && recording->Name()[strlen(base)] == FOLDERDELIMCHAR)) {
         cMenuRecordingItem *Item = new cMenuRecordingItem(recording, level);
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
            if (CurrentRecording && strcmp(CurrentRecording, recording->FileName()) == 0)
               SetCurrent(LastDir ? LastDir : LastItem);
            }
         if (LastDir)
            LastDir->IncrementCounter(recording->IsNew());
         }
      }
  if (Refresh)
     Display();
}

cString cMenuRecordings::DirectoryName(void)
{
  cString d(VideoDirectory);
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
  if (ri && ri->IsDirectory()) {
     const char *t = ri->Name();
     cString buffer;
     if (base) {
        buffer = cString::sprintf("%s~%s", base, t);
        t = buffer;
        }
     AddSubMenu(new cMenuRecordings(t, level + 1, OpenSubMenus));
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

eOSState cMenuRecordings::Delete(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory()) {
     if (Interface->Confirm(tr("Delete recording?"))) {
        cRecordControl *rc = cRecordControls::GetRecordControl(ri->Recording()->FileName());
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
        cRecording *recording = ri->Recording();
        cString FileName = recording->FileName();
        if (cCutter::Active(ri->Recording()->FileName())) {
           if (Interface->Confirm(tr("Recording is being edited - really delete?"))) {
              cCutter::Stop();
              recording = Recordings.GetByName(FileName); // cCutter::Stop() might have deleted it if it was the edited version
              // we continue with the code below even if recording is NULL,
              // in order to have the menu updated etc.
              }
           else
              return osContinue;
           }
        if (cReplayControl::NowReplaying() && strcmp(cReplayControl::NowReplaying(), FileName) == 0)
           cControl::Shutdown();
        if (!recording || recording->Delete()) {
           cReplayControl::ClearLastReplayed(FileName);
           Recordings.DelByName(FileName);
           cOsdMenu::Del(Current());
           SetHelpKeys();
           cVideoDiskUsage::ForceCheck();
           Display();
           if (!Count())
              return osBack;
           }
        else
           Skins.Message(mtError, tr("Error while deleting recording!"));
        }
     }
  return osContinue;
}

eOSState cMenuRecordings::Info(void)
{
  if (HasSubMenu() || Count() == 0)
     return osContinue;
  cMenuRecordingItem *ri = (cMenuRecordingItem *)Get(Current());
  if (ri && !ri->IsDirectory() && ri->Recording()->Info()->Title())
     return AddSubMenu(new cMenuRecording(ri->Recording(), true));
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
  IncRecordingsSortMode(DirectoryName());
  Set(true);
  return osContinue;
}

eOSState cMenuRecordings::ProcessKey(eKeys Key)
{
  bool HadSubMenu = HasSubMenu();
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
       case kNone:   if (Recordings.StateChanged(recordingsState))
                        Set(true);
                     break;
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
  if (!HasSubMenu()) {
     if (Key != kNone)
        SetHelpKeys();
     }
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
  cOsdProvider::UpdateOsdSize(true);
  Setup.Save();
}

// --- cMenuSetupOSD ---------------------------------------------------------

class cMenuSetupOSD : public cMenuSetupBase {
private:
  const char *useSmallFontTexts[3];
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
  virtual ~cMenuSetupOSD();
  virtual eOSState ProcessKey(eKeys Key);
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
     if (data.AlwaysSortFoldersFirst != Setup.AlwaysSortFoldersFirst)
        Recordings.ClearSortNames();
     }

  int oldSkinIndex = skinIndex;
  int oldOsdLanguageIndex = osdLanguageIndex;
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (ModifiedAppearance) {
     cOsdProvider::UpdateOsdSize(true);
     SetDisplayMenu();
     }

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
  virtual eOSState ProcessKey(eKeys Key);
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
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG bugfix level"),          &data.EPGBugfixLevel, 0, MAXEPGBUGFIXLEVEL));
  Add(new cMenuEditIntItem( tr("Setup.EPG$EPG linger time (min)"),     &data.EPGLinger, 0));
  Add(new cMenuEditBoolItem(tr("Setup.EPG$Set system time"),           &data.SetSystemTime));
  if (data.SetSystemTime)
     Add(new cMenuEditTranItem(tr("Setup.EPG$Use time from transponder"), &data.TimeTransponder, &data.TimeSource));
  // TRANSLATORS: note the plural!
  Add(new cMenuEditIntItem( tr("Setup.EPG$Preferred languages"),       &numLanguages, 0, I18nLanguages()->Size()));
  for (int i = 0; i < numLanguages; i++)
      // TRANSLATORS: note the singular!
      Add(new cMenuEditStraItem(tr("Setup.EPG$Preferred language"),    &data.EPGLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));

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
  const char *standardComplianceTexts[2];
public:
  cMenuSetupDVB(void);
  virtual eOSState ProcessKey(eKeys Key);
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
  standardComplianceTexts[0] = "DVB";
  standardComplianceTexts[1] = "ANSI/SCTE";

  SetSection(tr("DVB"));
  SetHelp(NULL, tr("Button$Audio"), tr("Button$Subtitles"), NULL);
  Setup();
}

void cMenuSetupDVB::Setup(void)
{
  int current = Current();

  Clear();

  Add(new cMenuEditIntItem( tr("Setup.DVB$Primary DVB interface"), &data.PrimaryDVB, 1, cDevice::NumDevices()));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Standard compliance"),   &data.StandardCompliance, 2, standardComplianceTexts));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Video format"),          &data.VideoFormat, "4:3", "16:9"));
  if (data.VideoFormat == 0)
     Add(new cMenuEditStraItem(tr("Setup.DVB$Video display format"), &data.VideoDisplayFormat, 3, videoDisplayFormatTexts));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Use Dolby Digital"),     &data.UseDolbyDigital));
  Add(new cMenuEditStraItem(tr("Setup.DVB$Update channels"),       &data.UpdateChannels, 6, updateChannelsTexts));
  Add(new cMenuEditIntItem( tr("Setup.DVB$Audio languages"),       &numAudioLanguages, 0, I18nLanguages()->Size()));
  for (int i = 0; i < numAudioLanguages; i++)
      Add(new cMenuEditStraItem(tr("Setup.DVB$Audio language"),    &data.AudioLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));
  Add(new cMenuEditBoolItem(tr("Setup.DVB$Display subtitles"),     &data.DisplaySubtitles));
  if (data.DisplaySubtitles) {
     Add(new cMenuEditIntItem( tr("Setup.DVB$Subtitle languages"),    &numSubtitleLanguages, 0, I18nLanguages()->Size()));
     for (int i = 0; i < numSubtitleLanguages; i++)
         Add(new cMenuEditStraItem(tr("Setup.DVB$Subtitle language"), &data.SubtitleLanguages[i], I18nLanguages()->Size(), &I18nLanguages()->At(0)));
     Add(new cMenuEditIntItem( tr("Setup.DVB$Subtitle offset"),                  &data.SubtitleOffset,      -100, 100));
     Add(new cMenuEditIntItem( tr("Setup.DVB$Subtitle foreground transparency"), &data.SubtitleFgTransparency, 0, 9));
     Add(new cMenuEditIntItem( tr("Setup.DVB$Subtitle background transparency"), &data.SubtitleBgTransparency, 0, 10));
     }

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupDVB::ProcessKey(eKeys Key)
{
  int oldPrimaryDVB = ::Setup.PrimaryDVB;
  int oldVideoDisplayFormat = ::Setup.VideoDisplayFormat;
  bool oldVideoFormat = ::Setup.VideoFormat;
  bool newVideoFormat = data.VideoFormat;
  bool oldDisplaySubtitles = ::Setup.DisplaySubtitles;
  bool newDisplaySubtitles = data.DisplaySubtitles;
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
     if (::Setup.PrimaryDVB != oldPrimaryDVB)
        state = osSwitchDvb;
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
  virtual eOSState ProcessKey(eKeys Key);
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

  SetCurrent(Get(current));
  Display();
}

eOSState cMenuSetupLNB::ProcessKey(eKeys Key)
{
  int oldDiSEqC = data.DiSEqC;
  bool DeviceBondingsChanged = false;
  if (Key == kOk) {
     cString NewDeviceBondings = satCableNumbers.ToString();
     DeviceBondingsChanged = strcmp(data.DeviceBondings, NewDeviceBondings) != 0;
     data.DeviceBondings = NewDeviceBondings;
     }
  eOSState state = cMenuSetupBase::ProcessKey(Key);

  if (Key != kNone && data.DiSEqC != oldDiSEqC)
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
  char buffer[32];
  const char *CamName = camSlot->GetCamName();
  if (!CamName) {
     switch (camSlot->ModuleStatus()) {
       case msReset:   CamName = tr("CAM reset"); break;
       case msPresent: CamName = tr("CAM present"); break;
       case msReady:   CamName = tr("CAM ready"); break;
       default:        CamName = "-"; break;
       }
     }
  snprintf(buffer, sizeof(buffer), " %d %s", camSlot->SlotNumber(), CamName);
  if (strcmp(buffer, Text()) != 0) {
     SetText(buffer);
     return true;
     }
  return false;
}

class cMenuSetupCAM : public cMenuSetupBase {
private:
  eOSState Menu(void);
  eOSState Reset(void);
public:
  cMenuSetupCAM(void);
  virtual eOSState ProcessKey(eKeys Key);
  };

cMenuSetupCAM::cMenuSetupCAM(void)
{
  SetMenuCategory(mcSetupCam);
  SetSection(tr("CAM"));
  SetCols(15);
  SetHasHotkeys();
  for (cCamSlot *CamSlot = CamSlots.First(); CamSlot; CamSlot = CamSlots.Next(CamSlot))
      Add(new cMenuSetupCAMItem(CamSlot));
  SetHelp(tr("Button$Menu"), tr("Button$Reset"));
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
              if (time(NULL) - t1 >= CAMMENURETYTIMEOUT) {
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
       default: break;
       }
     for (cMenuSetupCAMItem *ci = (cMenuSetupCAMItem *)First(); ci; ci = (cMenuSetupCAMItem *)ci->Next()) {
         if (ci->Changed())
            DisplayItem(ci);
         }
     }
  return state;
}

// --- cMenuSetupRecord ------------------------------------------------------

class cMenuSetupRecord : public cMenuSetupBase {
private:
  const char *pauseKeyHandlingTexts[3];
  const char *delTimeshiftRecTexts[3];
public:
  cMenuSetupRecord(void);
  };

cMenuSetupRecord::cMenuSetupRecord(void)
{
  SetMenuCategory(mcSetupRecord);
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
  virtual void Store(void);
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
  Add(new cMenuEditIntItem(tr("Setup.Replay$Resume ID"), &data.ResumeID, 0, 99));
}

void cMenuSetupReplay::Store(void)
{
  if (Setup.ResumeID != data.ResumeID)
     Recordings.ResetResume();
  cMenuSetupBase::Store();
}

// --- cMenuSetupMisc --------------------------------------------------------

class cMenuSetupMisc : public cMenuSetupBase {
public:
  cMenuSetupMisc(void);
  };

cMenuSetupMisc::cMenuSetupMisc(void)
{
  SetMenuCategory(mcSetupMisc);
  SetSection(tr("Miscellaneous"));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. event timeout (min)"),   &data.MinEventTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Min. user inactivity (min)"), &data.MinUserInactivity));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$SVDRP timeout (s)"),          &data.SVDRPTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Zap timeout (s)"),            &data.ZapTimeout));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Channel entry timeout (ms)"), &data.ChannelEntryTimeout, 0));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Remote control repeat delay (ms)"), &data.RcRepeatDelay, 0));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Remote control repeat delta (ms)"), &data.RcRepeatDelta, 0));
  Add(new cMenuEditChanItem(tr("Setup.Miscellaneous$Initial channel"),            &data.InitialChannel, tr("Setup.Miscellaneous$as before")));
  Add(new cMenuEditIntItem( tr("Setup.Miscellaneous$Initial volume"),             &data.InitialVolume, -1, 255, tr("Setup.Miscellaneous$as before")));
  Add(new cMenuEditBoolItem(tr("Setup.Miscellaneous$Channels wrap"),              &data.ChannelsWrap));
  Add(new cMenuEditBoolItem(tr("Setup.Miscellaneous$Show channel names with source"), &data.ShowChannelNamesWithSource));
  Add(new cMenuEditBoolItem(tr("Setup.Miscellaneous$Emergency exit"),             &data.EmergencyExit));
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
        SetDisplayMenu();
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
  virtual eOSState ProcessKey(eKeys Key);
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

cMenuMain::cMenuMain(eOSState State)
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
    case osRecordings: AddSubMenu(new cMenuRecordings(NULL, 0, true)); break;
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

  bool NewReplaying = cControl::Control() != NULL;
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
     SetHelp(!replaying ? tr("Button$Record") : NULL, tr("Button$Audio"), replaying ? NULL : tr("Button$Pause"), replaying ? tr("Button$Stop") : cReplayControl::LastReplayed() ? tr("Button$Resume") : tr("Button$Play"));
     result = true;
     }

  // Editing control:
  bool CutterActive = cCutter::Active();
  if (CutterActive && !cancelEditingItem) {
     // TRANSLATORS: note the leading blank!
     Add(cancelEditingItem = new cOsdItem(tr(" Cancel editing"), osCancelEdit));
     result = true;
     }
  else if (cancelEditingItem && !CutterActive) {
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
                          cOsdItem *item = Get(Current());
                          if (item) {
                             cRecordControls::Stop(item->Text() + strlen(tr(STOP_RECORDING)));
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
                                state = replaying ? osStopReplay : cReplayControl::LastReplayed() ? osReplay : osRecordings;
                             break;
               default:      break;
               }
    }
  if (!HasSubMenu() && Update(HadSubMenu))
     Display();
  if (Key != kNone) {
     if (I18nCurrentLanguage() != osdLanguage) {
        Set();
        if (!HasSubMenu())
           Display();
        }
     }
  return state;
}

// --- SetTrackDescriptions --------------------------------------------------

static void SetTrackDescriptions(int LiveChannel)
{
  cDevice::PrimaryDevice()->ClrAvailableTracks(true);
  const cComponents *Components = NULL;
  cSchedulesLock SchedulesLock;
  if (LiveChannel) {
     cChannel *Channel = Channels.GetByNumber(LiveChannel);
     if (Channel) {
        const cSchedules *Schedules = cSchedules::Schedules(SchedulesLock);
        if (Schedules) {
           const cSchedule *Schedule = Schedules->GetSchedule(Channel);
           if (Schedule) {
              const cEvent *Present = Schedule->GetPresentEvent();
              if (Present)
                 Components = Present->Components();
              }
           }
        }
     }
  else if (cReplayControl::NowReplaying()) {
     cThreadLock RecordingsLock(&Recordings);
     cRecording *Recording = Recordings.GetByName(cReplayControl::NowReplaying());
     if (Recording)
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
  currentDisplayChannel = this;
  group = -1;
  number = 0;
  timeout = true;
  lastPresent = lastFollowing = NULL;
  lastTime.Set();
  withInfo = Setup.ShowInfoOnChSwitch;
  displayChannel = Skins.Current()->DisplayChannel(withInfo);
  channel = Channels.GetByNumber(cDevice::CurrentChannel());
  ProcessKey(FirstKey);
}

cDisplayChannel::~cDisplayChannel()
{
  delete displayChannel;
  cStatus::MsgOsdClear();
  currentDisplayChannel = NULL;
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
        const cSchedule *Schedule = Schedules->GetSchedule(channel);
        if (Schedule) {
           const cEvent *Present = Schedule->GetPresentEvent();
           const cEvent *Following = Schedule->GetFollowingEvent();
           if (Present != lastPresent || Following != lastFollowing) {
              SetTrackDescriptions(channel->Number());
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
  DisplayChannel();
  displayChannel->SetEvents(NULL, NULL);
}

cChannel *cDisplayChannel::NextAvailableChannel(cChannel *Channel, int Direction)
{
  if (Direction) {
     while (Channel) {
           Channel = Direction > 0 ? Channels.Next(Channel) : Channels.Prev(Channel);
           if (!Channel && Setup.ChannelsWrap)
              Channel = Direction > 0 ? Channels.First() : Channels.Last();
           if (Channel && !Channel->GroupSep() && cDevice::GetDevice(Channel, LIVEPRIORITY, true, true))
              return Channel;
           }
     }
  return NULL;
}

eOSState cDisplayChannel::ProcessKey(eKeys Key)
{
  cChannel *NewChannel = NULL;
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
            if (number > Channels.MaxNumber())
               number = Key - k0;
            else
               number = number * 10 + Key - k0;
            channel = Channels.GetByNumber(number);
            Refresh();
            withInfo = false;
            // Lets see if there can be any useful further input:
            int n = channel ? number * 10 : 0;
            int m = 10;
            cChannel *ch = channel;
            while (ch && (ch = Channels.Next(ch)) != NULL) {
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
    case kPrev:
         withInfo = false;
         number = 0;
         if (group < 0) {
            cChannel *channel = Channels.GetByNumber(cDevice::CurrentChannel());
            if (channel)
               group = channel->Index();
            }
         if (group >= 0) {
            int SaveGroup = group;
            if (NORMALKEY(Key) == kRight || NORMALKEY(Key) == kNext)
               group = Channels.GetNextGroup(group) ;
            else
               group = Channels.GetPrevGroup(group < 1 ? 1 : group);
            if (group < 0)
               group = SaveGroup;
            channel = Channels.Get(group);
            if (channel) {
               Refresh();
               if (!channel->GroupSep())
                  group = -1;
               }
            }
         break;
    case kUp|k_Repeat:
    case kUp:
    case kDown|k_Repeat:
    case kDown:
    case kChanUp|k_Repeat:
    case kChanUp:
    case kChanDn|k_Repeat:
    case kChanDn: {
         eKeys k = NORMALKEY(Key);
         cChannel *ch = NextAvailableChannel(channel, (k == kUp || k == kChanUp) ? 1 : -1);
         if (ch)
            channel = ch;
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
            channel = Channels.GetByNumber(number);
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
    case kOk:
         if (group >= 0) {
            channel = Channels.Get(Channels.GetNextNormal(group));
            if (channel)
               NewChannel = channel;
            withInfo = true;
            group = -1;
            Refresh();
            }
         else if (number > 0) {
            channel = Channels.GetByNumber(number);
            if (channel)
               NewChannel = channel;
            withInfo = true;
            number = 0;
            Refresh();
            }
         else
            return osEnd;
         break;
    default:
         if ((Key & (k_Repeat | k_Release)) == 0) {
            cRemote::Put(Key);
            return osEnd;
            }
    };
  if (!timeout || lastTime.Elapsed() < (uint64_t)(Setup.ChannelInfoTime * 1000)) {
     if (Key == kNone && !number && group < 0 && !NewChannel && channel && channel->Number() != cDevice::CurrentChannel()) {
        // makes sure a channel switch through the SVDRP CHAN command is displayed
        channel = Channels.GetByNumber(cDevice::CurrentChannel());
        Refresh();
        lastTime.Set();
        }
     DisplayInfo();
     displayChannel->Flush();
     if (NewChannel) {
        SetTrackDescriptions(NewChannel->Number()); // to make them immediately visible in the channel display
        Channels.SwitchTo(NewChannel->Number());
        SetTrackDescriptions(NewChannel->Number()); // switching the channel has cleared them
        channel = NewChannel;
        }
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
         if (types[track] != cDevice::PrimaryDevice()->GetCurrentSubtitleTrack())
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
     }
  return timeout.TimedOut() ? osEnd : osContinue;
}

// --- cRecordControl --------------------------------------------------------

cRecordControl::cRecordControl(cDevice *Device, cTimer *Timer, bool Pause)
{
  // Whatever happens here, the timers will be modified in some way...
  Timers.SetModified();
  // We're going to manipulate an event here, so we need to prevent
  // others from modifying any EPG data:
  cSchedulesLock SchedulesLock;
  cSchedules::Schedules(SchedulesLock);

  event = NULL;
  fileName = NULL;
  recorder = NULL;
  device = Device;
  if (!device) device = cDevice::PrimaryDevice();//XXX
  timer = Timer;
  if (!timer) {
     timer = new cTimer(true, Pause);
     Timers.Add(timer);
     instantId = cString::sprintf(cDevice::NumDevices() > 1 ? "%s - %d" : "%s", timer->Channel()->Name(), device->CardIndex() + 1);
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
        if (!cReplayControl::LastReplayed()) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName);
        }
     timer = NULL;
     return;
     }

  cRecordingUserCommand::InvokeCommand(RUC_BEFORERECORDING, fileName);
  isyslog("record %s", fileName);
  if (MakeDirs(fileName, true)) {
     const cChannel *ch = timer->Channel();
     recorder = new cRecorder(fileName, ch, timer->Priority());
     if (device->AttachReceiver(recorder)) {
        Recording.WriteInfo();
        cStatus::MsgRecording(device, Recording.Name(), Recording.FileName(), true);
        if (!Timer && !cReplayControl::LastReplayed()) // an instant recording, maybe from cRecordControls::PauseLiveVideo()
           cReplayControl::SetRecording(fileName);
        Recordings.AddByName(fileName);
        if (Timer && !Timer->IsSingleEvent()) {
           char *Directory = strdup(fileName);
           // going up two directory levels to get the series folder
           if (char *p = strrchr(Directory, '/')) {
              while (p > Directory && *--p != '/')
                    ;
              *p = 0;
              if (!HasRecordingsSortMode(Directory)) {
                 dsyslog("setting %s to be sorted by time", Directory);
                 SetRecordingsSortMode(Directory, rsmTime);
                 }
              }
           free(Directory);
           }
        return;
        }
     else
        DELETENULL(recorder);
     }
  else
     timer->SetDeferred(DEFERTIMER);
  if (!Timer) {
     Timers.Del(timer);
     timer = NULL;
     }
}

cRecordControl::~cRecordControl()
{
  Stop();
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
           const cSchedule *Schedule = Schedules->GetSchedule(channel);
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
      cCondWait::SleepMs(1000);
      }
  dsyslog("no EPG info available");
  return false;
}

void cRecordControl::Stop(bool ExecuteUserCommand)
{
  if (timer) {
     DELETENULL(recorder);
     timer->SetRecording(false);
     timer = NULL;
     cStatus::MsgRecording(device, NULL, fileName, false);
     if (ExecuteUserCommand)
        cRecordingUserCommand::InvokeCommand(RUC_AFTERRECORDING, fileName);
     Timers.SetModified();
     }
}

bool cRecordControl::Process(time_t t)
{
  if (!recorder || !recorder->IsAttached() || !timer || !timer->Matches(t)) {
     if (timer)
        timer->SetPending(false);
     return false;
     }
  AssertFreeDiskSpace(timer->Priority());
  return true;
}

// --- cRecordControls -------------------------------------------------------

cRecordControl *cRecordControls::RecordControls[MAXRECORDCONTROLS] = { NULL };
int cRecordControls::state = 0;

bool cRecordControls::Start(cTimer *Timer, bool Pause)
{
  static time_t LastNoDiskSpaceMessage = 0;
  int FreeMB = 0;
  if (Timer) {
     AssertFreeDiskSpace(Timer->Priority(), !Timer->Pending());
     Timer->SetPending(true);
     }
  VideoDiskSpace(&FreeMB);
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
  int ch = Timer ? Timer->Channel()->Number() : cDevice::CurrentChannel();
  cChannel *channel = Channels.GetByNumber(ch);

  if (channel) {
     int Priority = Timer ? Timer->Priority() : Pause ? Setup.PausePriority : Setup.DefaultPriority;
     cDevice *device = cDevice::GetDevice(channel, Priority, false);
     if (device) {
        dsyslog("switching device %d to channel %d", device->DeviceNumber() + 1, channel->Number());
        if (!device->SwitchChannel(channel, false)) {
           ShutdownHandler.RequestEmergencyExit();
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
     else if (!Timer || !Timer->Pending()) {
        isyslog("no free DVB device to record channel %d!", ch);
        Skins.Message(mtError, tr("No free DVB device to record!"));
        }
     }
  else
     esyslog("ERROR: channel %d not defined!", ch);
  return false;
}

void cRecordControls::Stop(const char *InstantId)
{
  ChangeState();
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

bool cRecordControls::PauseLiveVideo(void)
{
  Skins.Message(mtStatus, tr("Pausing live video..."));
  cReplayControl::SetRecording(NULL); // make sure the new cRecordControl will set cReplayControl::LastReplayed()
  if (Start(NULL, true)) {
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

void cRecordControls::Process(time_t t)
{
  for (int i = 0; i < MAXRECORDCONTROLS; i++) {
      if (RecordControls[i]) {
         if (!RecordControls[i]->Process(t)) {
            DELETENULL(RecordControls[i]);
            ChangeState();
            }
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

// --- cReplayControl --------------------------------------------------------

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
  lastCurrent = lastTotal = -1;
  lastPlay = lastForward = false;
  lastSpeed = -2; // an invalid value
  timeoutShow = 0;
  timeSearchActive = false;
  cRecording Recording(fileName);
  cStatus::MsgReplaying(this, Recording.Name(), Recording.FileName(), true);
  marks.Load(fileName, Recording.FramesPerSecond(), Recording.IsPesRecording());
  SetTrackDescriptions(false);
  if (Setup.ProgressDisplayTime)
     ShowTimed(Setup.ProgressDisplayTime);
}

cReplayControl::~cReplayControl()
{
  cDevice::PrimaryDevice()->SetKeepTracks(false);
  Hide();
  cStatus::MsgReplaying(this, NULL, fileName, false);
  Stop();
  if (currentReplayControl == this)
     currentReplayControl = NULL;
}

void cReplayControl::Stop(void)
{
  if (Setup.DelTimeshiftRec && *fileName) {
     cRecordControl* rc = cRecordControls::GetRecordControl(fileName);
     if (rc && rc->InstantId()) {
        if (Active()) {
           if (Setup.DelTimeshiftRec == 2 || Interface->Confirm(tr("Delete timeshift recording?"))) {
              cTimer *timer = rc->Timer();
              rc->Stop(false); // don't execute user command
              if (timer) {
                 isyslog("deleting timer %s", *timer->ToDescr());
                 Timers.Del(timer);
                 Timers.SetModified();
                 }
              cDvbPlayerControl::Stop();
              cRecording *recording = Recordings.GetByName(fileName);
              if (recording) {
                 if (recording->Delete()) {
                    Recordings.DelByName(fileName);
                    ClearLastReplayed(fileName);
                    }
                 else
                    Skins.Message(mtError, tr("Error while deleting recording!"));
                 }
              return;
              }
           }
        }
     }
  cDvbPlayerControl::Stop();
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
  if (!Recordings.GetByName(fileName))
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

  if (GetIndex(Current, Total) && Total > 0) {
     if (!visible) {
        displayReplay = Skins.Current()->DisplayReplay(modeOnly);
        displayReplay->SetMarks(&marks);
        SetNeedsFastResponse(true);
        visible = true;
        }
     if (Initial) {
        if (*fileName) {
           if (cRecording *Recording = Recordings.GetByName(fileName))
              displayReplay->SetRecording(Recording);
           }
        lastCurrent = lastTotal = -1;
        }
     if (Current != lastCurrent || Total != lastTotal) {
        if (Setup.ShowRemainingTime || Total != lastTotal) {
           int Index = Total;
           if (Setup.ShowRemainingTime)
              Index = Current - Index;
           displayReplay->SetTotal(IndexToHMSF(Index, false, FramesPerSecond()));
           if (!Initial)
              displayReplay->Flush();
           }
        displayReplay->SetProgress(Current, Total);
        if (!Initial)
           displayReplay->Flush();
        displayReplay->SetCurrent(IndexToHMSF(Current, displayFrames, FramesPerSecond()));
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
            Goto(SecondsToFrames(Seconds, FramesPerSecond()), Key == kDown || Key == kPause || Key == kOk);
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
     ShowTimed(2);
     marksModified = true;
     }
}

void cReplayControl::MarkJump(bool Forward)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     if (marks.Count()) {
        if (cMark *m = Forward ? marks.GetNext(Current) : marks.GetPrev(Current)) {
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

void cReplayControl::MarkMove(bool Forward)
{
  int Current, Total;
  if (GetIndex(Current, Total)) {
     if (cMark *m = marks.Get(Current)) {
        displayFrames = true;
        int p = SkipFrames(Forward ? 1 : -1);
        cMark *m2;
        if (Forward) {
           while ((m2 = marks.Next(m)) != NULL && m2->Position() == m->Position())
                 m = m2;
           }
        else {
           while ((m2 = marks.Prev(m)) != NULL && m2->Position() == m->Position())
                 m = m2;
           }
        m->SetPosition(p);
        Goto(m->Position(), true);
        marksModified = true;
        }
     }
}

void cReplayControl::EditCut(void)
{
  if (*fileName) {
     Hide();
     if (!cCutter::Active()) {
        if (!marks.Count())
           Skins.Message(mtError, tr("No editing marks defined!"));
        else if (!marks.GetNumSequences())
           Skins.Message(mtError, tr("No editing sequences defined!"));
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
           Goto(m->Position() - SecondsToFrames(3, FramesPerSecond()));
           Play();
           }
        }
     }
}

cOsdObject *cReplayControl::GetInfo(void)
{
  cRecording *Recording = Recordings.GetByName(cReplayControl::LastReplayed());
  if (Recording)
     return new cMenuRecording(Recording, false);
  return NULL;
}

const cRecording *cReplayControl::GetRecording(void)
{
  if (const cRecording *Recording = Recordings.GetByName(LastReplayed()))
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
    case kGreen:   SkipSeconds(-60); break;
    case kYellow|k_Repeat:
    case kYellow:  SkipSeconds( 60); break;
    case kStop:
    case kBlue:    Hide();
                   Stop();
                   return osEnd;
    default: {
      DoShowMode = false;
      switch (int(Key)) {
        // Editing:
        case kMarkToggle:      MarkToggle(); break;
        case kPrev|k_Repeat:
        case kPrev:
        case kMarkJumpBack|k_Repeat:
        case kMarkJumpBack:    MarkJump(false); break;
        case kNext|k_Repeat:
        case kNext:
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
            case kBack:    if (Setup.DelTimeshiftRec) {
                              cRecordControl* rc = cRecordControls::GetRecordControl(fileName);
                              return rc && rc->InstantId() ? osEnd : osRecordings;
                              }
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
