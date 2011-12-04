/*
 * interface.c: Abstract user interface layer
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: interface.c 2.1 2011/12/04 14:52:38 kls Exp $
 */

#include "interface.h"
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "i18n.h"
#include "status.h"

cInterface *Interface = NULL;

cInterface::cInterface(int SVDRPport)
{
  interrupted = false;
  SVDRP = NULL;
  if (SVDRPport)
     SVDRP = new cSVDRP(SVDRPport);
}

cInterface::~cInterface()
{
  delete SVDRP;
}

eKeys cInterface::GetKey(bool Wait)
{
  if (!cRemote::HasKeys())
     Skins.Flush();
  if (SVDRP) {
     if (SVDRP->Process())
        Wait = false;
     }
  if (!cRemote::IsLearning())
     return cRemote::Get(Wait ? 1000 : 10);
  else
     return kNone;
}

eKeys cInterface::Wait(int Seconds, bool KeepChar)
{
  if (Seconds == 0)
     Seconds = Setup.OSDMessageTime;
  Skins.Flush();
  eKeys Key = kNone;
  time_t timeout = time(NULL) + Seconds;
  for (;;) {
      Key = GetKey();
      if (ISRAWKEY(Key) || time(NULL) > timeout || interrupted)
         break;
      }
  if (KeepChar && ISRAWKEY(Key) || Key == k_Plugin)
     cRemote::Put(Key);
  interrupted = false;
  return Key;
}

bool cInterface::Confirm(const char *s, int Seconds, bool WaitForTimeout)
{
  isyslog("confirm: %s", s);
  eKeys k = Skins.Message(mtWarning, s, Seconds);
  bool result = WaitForTimeout ? k == kNone : k == kOk;
  isyslog("%sconfirmed", result ? "" : "not ");
  return result;
}

bool cInterface::QueryKeys(cRemote *Remote, cSkinDisplayMenu *DisplayMenu)
{
  DisplayMenu->SetItem(tr("Phase 1: Detecting RC code type"), 2, false, false);
  DisplayMenu->SetItem(tr("Press any key on the RC unit"), 4, false, false);
  DisplayMenu->Flush();
  if (Remote->Initialize()) {
     DisplayMenu->SetItem(tr("RC code detected!"), 4, false, false);
     DisplayMenu->SetItem(tr("Do not press any key..."), 5, false, false);
     DisplayMenu->Flush();
     cCondWait::SleepMs(3000);
     DisplayMenu->SetItem("", 4, false, false);
     DisplayMenu->SetItem("", 5, false, false);

     DisplayMenu->SetItem(tr("Phase 2: Learning specific key codes"), 2, false, false);
     eKeys NewKey = kUp;
     while (NewKey != kNone) {
           DisplayMenu->SetItem(cString::sprintf(tr("Press key for '%s'"), cKey::ToString(NewKey, true)), 4, false, false);
           cRemote::Clear();
           DisplayMenu->Flush();
           for (eKeys k = NewKey; k == NewKey; ) {
               char *NewCode = NULL;
               eKeys Key = cRemote::Get(100, &NewCode);
               switch (Key) {
                 case kUp:   if (NewKey > kUp) {
                                NewKey = eKeys(NewKey - 1);
                                cKey *last = Keys.Last();
                                if (last && last->Key() == NewKey)
                                   Keys.Del(last);
                                }
                             break;
                 case kDown: DisplayMenu->SetItem(tr("Press 'Up' to confirm"), 4, false, false);
                             DisplayMenu->SetItem(tr("Press 'Down' to continue"), 5, false, false);
                             DisplayMenu->SetItem("", 6, false, false);
                             DisplayMenu->SetItem("", 7, false, false);
                             DisplayMenu->SetItem("", 8, false, false);
                             DisplayMenu->Flush();
                             for (;;) {
                                 Key = cRemote::Get(100);
                                 if (Key == kUp) {
                                    DisplayMenu->Clear();
                                    return true;
                                    }
                                 else if (Key == kDown) {
                                    DisplayMenu->SetItem("", 5, false, false);
                                    k = kNone; // breaks the outer for() loop
                                    break;
                                    }
                                 }
                             break;
                 case kMenu: NewKey = eKeys(NewKey + 1);
                             break;
                 case kNone: if (NewCode) {
                                dsyslog("new %s code: %s = %s", Remote->Name(), NewCode, cKey::ToString(NewKey));
                                Keys.Add(new cKey(Remote->Name(), NewCode, NewKey));
                                NewKey = eKeys(NewKey + 1);
                                free(NewCode);
                                }
                             break;
                 default:    break;
                 }
               }
           if (NewKey > kUp)
              DisplayMenu->SetItem(tr("(press 'Up' to go back)"), 6, false, false);
           else
              DisplayMenu->SetItem("", 6, false, false);
           if (NewKey > kDown)
              DisplayMenu->SetItem(tr("(press 'Down' to end key definition)"), 7, false, false);
           else
              DisplayMenu->SetItem("", 7, false, false);
           if (NewKey > kMenu)
              DisplayMenu->SetItem(tr("(press 'Menu' to skip this key)"), 8, false, false);
           else
              DisplayMenu->SetItem("", 8, false, false);
           }
     return true;
     }
  return false;
}

void cInterface::LearnKeys(void)
{
  for (cRemote *Remote = Remotes.First(); Remote; Remote = Remotes.Next(Remote)) {
      if (!Remote->Ready()) {
         esyslog("ERROR: remote control %s not ready!", Remote->Name());
         continue;
         }
      bool known = Keys.KnowsRemote(Remote->Name());
      dsyslog("remote control %s - %s", Remote->Name(), known ? "keys known" : "learning keys");
      if (!known) {
         cSkinDisplayMenu *DisplayMenu = Skins.Current()->DisplayMenu();
         char Headline[256];
         snprintf(Headline, sizeof(Headline), tr("Learning Remote Control Keys"));
         cRemote::Clear();
         DisplayMenu->SetTitle(Headline);
         DisplayMenu->SetItem(Remote->Name(), 0, false, false);
         cRemote::SetLearning(Remote);
         bool rc = QueryKeys(Remote, DisplayMenu);
         cRemote::SetLearning(NULL);
         DisplayMenu->Clear();
         if (!rc) {
            delete DisplayMenu;
            continue;
            }
         DisplayMenu->SetItem(Remote->Name(), 0, false, false);
         DisplayMenu->SetItem(tr("Phase 3: Saving key codes"), 2, false, false);
         DisplayMenu->SetItem(tr("Press 'Up' to save, 'Down' to cancel"), 4, false, false);
         for (;;) {
             eKeys key = GetKey();
             if (key == kUp) {
                Keys.Save();
                delete DisplayMenu;
                break;
                }
             else if (key == kDown) {
                Keys.Load();
                delete DisplayMenu;
                break;
                }
             }
         }
      }
}
