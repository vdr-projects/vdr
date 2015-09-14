/*
 * entry.c: Data structure to handle still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: entry.c 4.0 2012/02/17 14:00:28 kls Exp $
 */

#include "entry.h"

cPictureEntry::cPictureEntry(const char *Name, const cPictureEntry *Parent, bool IsDirectory)
{
  name = strdup(Name);
  parent = Parent;
  isDirectory = IsDirectory;
  entries = NULL;
}

cPictureEntry::~cPictureEntry()
{
  free(name);
  delete entries;
}

int cPictureEntry::Compare(const cListObject &ListObject) const
{
  cPictureEntry *p = (cPictureEntry *)&ListObject;
  if (IsDirectory() && !p->IsDirectory())
     return -1;
  if (!IsDirectory() && p->IsDirectory())
     return +1;
  if (IsDirectory())
     return strcoll(name, p->name);
  else
     return strcmp(name, p->name); // correctly sorts dsc01234.jpg and dsc01234a.jpg in case pictures have been "squeezed in"
}

cString cPictureEntry::Path(void) const
{
  return parent ? *AddDirectory(parent->Path(), name) : name;
}

void cPictureEntry::Load(void) const
{
  if (isDirectory && !entries) {
     cString Directory = Path();
     cReadDir d(Directory);
     if (d.Ok()) {
        struct dirent *e;
        while ((e = d.Next()) != NULL) {
              struct stat ds;
              if (stat(AddDirectory(Directory, e->d_name), &ds) == 0) {
                 if (!entries)
                    entries = new cList<cPictureEntry>;
                 entries->Add(new cPictureEntry(e->d_name, this, S_ISDIR(ds.st_mode)));
                 }
              }
        if (entries)
           entries->Sort();
        }
     else
        LOG_ERROR_STR(*Directory);
     }
}

const cList<cPictureEntry> *cPictureEntry::Entries(void) const
{
  Load();
  return entries;
}

const cPictureEntry *cPictureEntry::FirstPicture(void) const
{
  Load();
  if (entries) {
     for (cPictureEntry *pe = entries->First(); pe; pe = entries->Next(pe)) {
         if (pe->IsDirectory()) {
            const cPictureEntry *p = pe->FirstPicture();
            if (p)
               return p;
            }
         else
            return pe;
         }
     }
  return NULL;
}

const cPictureEntry *cPictureEntry::LastPicture(void) const
{
  Load();
  if (entries) {
     for (cPictureEntry *pe = entries->Last(); pe; pe = entries->Prev(pe)) {
         if (pe->IsDirectory()) {
            const cPictureEntry *p = pe->LastPicture();
            if (p)
               return p;
            }
         else
            return pe;
         }
     }
  return NULL;
}

const cPictureEntry *cPictureEntry::PrevPicture(const cPictureEntry *This) const
{
  if (This) {
     const cPictureEntry *pe = (cPictureEntry *)entries->Prev(This);
     if (pe) {
        if (pe->IsDirectory()) {
           const cPictureEntry *p = pe->LastPicture();
           if (p)
              return p;
           return PrevPicture(pe);
           }
        return pe;
        }
     }
  if (parent)
     return parent->PrevPicture(this);
  return NULL;
}

const cPictureEntry *cPictureEntry::NextPicture(const cPictureEntry *This) const
{
  if (This) {
     cPictureEntry *pe = (cPictureEntry *)entries->Next(This);
     if (pe) {
        if (pe->IsDirectory()) {
           const cPictureEntry *p = pe->FirstPicture();
           if (p)
              return p;
           return NextPicture(pe);
           }
        return pe;
        }
     }
  else if (IsDirectory()) {
     const cPictureEntry *p = FirstPicture();
     if (p)
        return p;
     }
  if (parent)
     return parent->NextPicture(this);
  return NULL;
}
