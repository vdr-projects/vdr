/*
 * sections.h: Section data handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sections.h 1.2 2004/01/10 11:42:49 kls Exp $
 */

#ifndef __SECTIONS_H
#define __SECTIONS_H

#include <time.h>
#include "filter.h"
#include "thread.h"
#include "tools.h"

class cDevice;
class cFilterHandle;

class cSectionHandler : public cThread {
  friend class cFilter;
private:
  cDevice *device;
  bool active;
  int source;
  int transponder;
  int statusCount;
  bool on;
  time_t lastIncompleteSection;
  cList<cFilter> filters;
  cList<cFilterHandle> filterHandles;
  void Add(const cFilterData *FilterData);
  void Del(const cFilterData *FilterData);
  virtual void Action(void);
public:
  cSectionHandler(cDevice *Device);
  virtual ~cSectionHandler();
  int Source(void) { return source; }
  int Transponder(void) { return transponder; }
  void Attach(cFilter *Filter);
  void Detach(cFilter *Filter);
  void SetSource(int Source, int Transponder);
  void SetStatus(bool On);
  };

#endif //__SECTIONS_H
