/*
 * sourceparams.h: Source parameter handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sourceparams.h 1.1 2010/02/28 11:58:03 kls Exp $
 */

#ifndef __SOURCEPARAMS_H
#define __SOURCEPARAMS_H

#include "channels.h"
#include "osdbase.h"
#include "tools.h"

class cSourceParam : public cListObject {
private:
  char source;
public:
  cSourceParam(char Source, const char *Description);
    ///< Sets up a parameter handler for the given Source.
    ///< Source must be in the range 'A'...'Z', and there can only
    ///< be one cSourceParam for any given source.
    ///< Description contains a short, one line description of this source.
    ///< If a plugin sets up a new cSourceParam, this will also trigger
    ///< defining the appropriate cSource automatically.
    ///< Objects of cSourceParam shall only be created on the heap, and
    ///< shall never be deleted (they will be deleted automatically when
    ///< the program ends).
  char Source(void) const { return source; }
  virtual void SetData(cChannel *Channel) = 0;
    ///< Sets all source specific parameters to those of the given Channel.
    ///< Must also reset a counter to use with later calls to GetOsdItem().
  virtual void GetData(cChannel *Channel) = 0;
    ///< Copies all source specific parameters to the given Channel.
  virtual cOsdItem *GetOsdItem(void) = 0;
    ///< Returns all the OSD items necessary for editing the source
    ///< specific parameters of the channel that was given in the last
    ///< call to SetData(). Each call to GetOsdItem() returns exactly
    ///< one such item. After all items have been fetched, any further
    ///< calls to GetOsdItem() return NULL. After another call to
    ///< SetData(), the OSD items can be fetched again.
  };

class cSourceParams : public cList<cSourceParam> {
public:
  cSourceParam *Get(char Source) const;
  };

extern cSourceParams SourceParams;

#endif //__SOURCEPARAMS_H
