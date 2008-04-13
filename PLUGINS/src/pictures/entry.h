/*
 * entry.h: Data structure to handle still pictures
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: entry.h 2.0 2008/01/06 12:30:50 kls Exp $
 */

#ifndef _ENTRY_H
#define _ENTRY_H

#include <vdr/tools.h>

class cPictureEntry : public cListObject {
private:
  char *name;
  const cPictureEntry *parent;
  bool isDirectory;
  mutable cList<cPictureEntry> *entries;
  void Load(void) const;
public:
  cPictureEntry(const char *Name, const cPictureEntry *Parent, bool IsDirectory);
  virtual ~cPictureEntry();
  virtual int Compare(const cListObject &ListObject) const;
  const char *Name(void) const { return name; }
  const cPictureEntry *Parent(void) const { return parent; }
  bool IsDirectory(void) const { return isDirectory; }
  cString Path(void) const;
  const cList<cPictureEntry> *Entries(void) const;
  const cPictureEntry *FirstPicture(void) const;
  const cPictureEntry *LastPicture(void) const;
  const cPictureEntry *PrevPicture(const cPictureEntry *This = NULL) const;
  const cPictureEntry *NextPicture(const cPictureEntry *This = NULL) const;
  };

#endif //_ENTRY_H
