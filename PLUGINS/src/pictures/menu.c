/*
 * menu.c: A menu for still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: menu.c 1.1 2008/01/13 11:35:18 kls Exp $
 */

#include "menu.h"
#include <vdr/tools.h>
#include "entry.h"
#include "player.h"

char PictureDirectory[PATH_MAX] = "";

static bool PathStartsWith(const char *Path, const char *Name)
{
  if (Path && Name) {
     while (*Name) {
           if (*Path++ != *Name++)
              return false;
           }
     if (*Path && *Path != '/')
        return false;
     return true;
     }
  return false;
}

static const char *NextLevel(const char *Path)
{
  if (Path) {
     const char *p = strchr(Path, '/');
     return p ? p + 1 : NULL;
     }
  return Path;
}

cPictureEntry *cPictureMenu::pictures = NULL;

cPictureMenu::cPictureMenu(const cPictureEntry *PictureEntry, const char *Path)
:cOsdMenu(tr("Pictures"))
{
  pictureEntry = PictureEntry;
  if (!pictureEntry)
     pictureEntry = pictures = new cPictureEntry(PictureDirectory, NULL, true);
  if (pictureEntry->Parent()) {
     if (!pictureEntry->Parent()->Parent())
        SetTitle(pictureEntry->Name()); // Year
     else
        SetTitle(cString::sprintf("%s: %s", pictureEntry->Parent()->Name(), *HandleUnderscores(pictureEntry->Name()))); // Year/Description
     }
  Set(Path);
}

cPictureMenu::~cPictureMenu()
{
  if (pictures && pictureEntry && !pictureEntry->Parent())
     DELETENULL(pictures);
}

void cPictureMenu::Set(const char *Path)
{
  Clear();
  const cList<cPictureEntry> *l = pictureEntry->Entries();
  if (l) {
     for (const cPictureEntry *e = l->First(); e; e = l->Next(e)) {
         cString Name = HandleUnderscores(e->Name());
         if (!e->IsDirectory())
            Name.Truncate(-4); // don't display the ".mpg" extension
         Add(new cOsdItem(HandleUnderscores(Name)), PathStartsWith(Path, e->Name()));
         }
     }
  SetHelp(Count() ? trVDR("Button$Play") : NULL, NULL, NULL, cPictureControl::Active() ? trVDR("Button$Stop") : NULL);
  if (Current() >= 0) {
     const char *p = NextLevel(Path);
     if (p)
        SelectItem(p);
     }
}

eOSState cPictureMenu::SelectItem(const char *Path, bool SlideShow)
{
  cOsdItem *Item = Get(Current());
  if (Item) {
     const cList<cPictureEntry> *l = pictureEntry->Entries();
     if (l) {
        cPictureEntry *pe = l->Get(Current());
        if (pe) {
           if (SlideShow) {
              cControl::Launch(new cPictureControl(pictures, pe, true));
              pictures = NULL; // cPictureControl takes ownership
              return osEnd;
              }
           if (pe->IsDirectory())
              return AddSubMenu(new cPictureMenu(pe, Path));
           else if (!Path) {
              cControl::Launch(new cPictureControl(pictures, pe));
              pictures = NULL; // cPictureControl takes ownership
              return osEnd;
              }
           }
        }
     }
  return osContinue;
}

eOSState cPictureMenu::ProcessKey(eKeys Key)
{
  eOSState state = cOsdMenu::ProcessKey(Key);
  if (state == osUnknown) {
     switch (Key) {
       case kRed:
       case kPlay:   return SelectItem(NULL, true);
       case kBlue:
       case kStop:   if (cPictureControl::Active())
                        return osStopReplay;
                     break;
       case kOk:     return SelectItem();
       default:      break;
       }
     }
  return state;
}

cPictureMenu *cPictureMenu::CreatePictureMenu(void)
{
  return new cPictureMenu(NULL, cPictureControl::LastDisplayed());
}
