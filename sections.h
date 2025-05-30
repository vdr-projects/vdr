/*
 * sections.h: Section data handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sections.h 5.2 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __SECTIONS_H
#define __SECTIONS_H

#include <time.h>
#include "filter.h"
#include "thread.h"
#include "tools.h"

class cDevice;
class cChannel;
class cFilterHandle;
class cSectionHandlerPrivate;

class cSectionHandler : public cThread {
  friend class cFilter;
private:
  cSectionHandlerPrivate *shp;
  cDevice *device;
  int statusCount;
  bool on, waitForLock;
  bool flush;
  bool startFilters;
  cTimeMs flushTimer;
  cList<cFilter> filters;
  cList<cFilterHandle> filterHandles;
  void Add(const cFilterData *FilterData);
  void Del(const cFilterData *FilterData);
  virtual void Action(void) override;
public:
  cSectionHandler(cDevice *Device);
  virtual ~cSectionHandler() override;
  int Source(void);
  int Transponder(void);
  const cChannel *Channel(void);
  void Attach(cFilter *Filter);
  void Detach(cFilter *Filter);
  void SetChannel(const cChannel *Channel);
  void SetStatus(bool On);
  };

#endif //__SECTIONS_H
