/*
 * sections.c: Section data handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sections.c 5.3 2022/01/31 21:21:42 kls Exp $
 */

#include "sections.h"
#include <unistd.h>
#include "channels.h"
#include "device.h"
#include "thread.h"

// --- cFilterHandle----------------------------------------------------------

class cFilterHandle : public cListObject {
public:
  cFilterData filterData;
  int handle;
  int used;
  cFilterHandle(const cFilterData &FilterData);
  };

cFilterHandle::cFilterHandle(const cFilterData &FilterData)
{
  filterData = FilterData;
  handle = -1;
  used = 0;
}

// --- cSectionHandlerPrivate ------------------------------------------------

class cSectionHandlerPrivate {
public:
  cChannel channel;
  };

// --- cSectionHandler -------------------------------------------------------

cSectionHandler::cSectionHandler(cDevice *Device)
:cThread(NULL, true)
{
  shp = new cSectionHandlerPrivate;
  device = Device;
  SetDescription("device %d section handler", device->DeviceNumber() + 1);
  statusCount = 0;
  on = false;
  waitForLock = false;
  flush = false;
  startFilters = false;
  Start();
}

cSectionHandler::~cSectionHandler()
{
  Cancel(3);
  cFilter *fi;
  while ((fi = filters.First()) != NULL)
        Detach(fi);
  delete shp;
}

int cSectionHandler::Source(void)
{
  return shp->channel.Source();
}

int cSectionHandler::Transponder(void)
{
  return shp->channel.Transponder();
}

const cChannel *cSectionHandler::Channel(void)
{
  return &shp->channel;
}

void cSectionHandler::Add(const cFilterData *FilterData)
{
  Lock();
  statusCount++;
  cFilterHandle *fh;
  for (fh = filterHandles.First(); fh; fh = filterHandles.Next(fh)) {
      if (fh->filterData.Is(FilterData->pid, FilterData->tid, FilterData->mask))
         break;
      }
  if (!fh) {
     int handle = device->OpenFilter(FilterData->pid, FilterData->tid, FilterData->mask);
     if (handle >= 0) {
        fh = new cFilterHandle(*FilterData);
        fh->handle = handle;
        filterHandles.Add(fh);
        }
     }
  if (fh)
     fh->used++;
  Unlock();
}

void cSectionHandler::Del(const cFilterData *FilterData)
{
  Lock();
  statusCount++;
  cFilterHandle *fh;
  for (fh = filterHandles.First(); fh; fh = filterHandles.Next(fh)) {
      if (fh->filterData.Is(FilterData->pid, FilterData->tid, FilterData->mask)) {
         if (--fh->used <= 0) {
            device->CloseFilter(fh->handle);
            filterHandles.Del(fh);
            break;
            }
         }
      }
  Unlock();
}

void cSectionHandler::Attach(cFilter *Filter)
{
  Lock();
  statusCount++;
  filters.Add(Filter);
  Filter->sectionHandler = this;
  if (on)
     Filter->SetStatus(true);
  Unlock();
}

void cSectionHandler::Detach(cFilter *Filter)
{
  Lock();
  statusCount++;
  Filter->SetStatus(false);
  Filter->sectionHandler = NULL;
  filters.Del(Filter, false);
  Unlock();
}

void cSectionHandler::SetChannel(const cChannel *Channel)
{
  Lock();
  shp->channel = Channel ? *Channel : cChannel();
  Unlock();
}

void cSectionHandler::SetStatus(bool On)
{
  Lock();
  if (on != On) {
     if (!On || (device->HasLock() && startFilters)) {
        statusCount++;
        for (cFilter *fi = filters.First(); fi; fi = filters.Next(fi)) {
            fi->SetStatus(false);
            if (On)
               fi->SetStatus(true);
            }
        flush = On;
        if (flush)
           flushTimer.Set();
        on = On;
        waitForLock = false;
        }
     else
        waitForLock = On;
     }
  Unlock();
}

#define FLUSH_TIME 100 // ms

void cSectionHandler::Action(void)
{
  while (Running()) {

        Lock();
        if (waitForLock) {
           startFilters = true;
           SetStatus(true);
           startFilters = false;
           }
        int NumFilters = filterHandles.Count();
        pollfd pfd[NumFilters];
        for (cFilterHandle *fh = filterHandles.First(); fh; fh = filterHandles.Next(fh)) {
            int i = fh->Index();
            pfd[i].fd = fh->handle;
            pfd[i].events = POLLIN;
            pfd[i].revents = 0;
            }
        int oldStatusCount = statusCount;
        Unlock();

        if (poll(pfd, NumFilters, (!on || waitForLock) ? 100 : 1000) > 0) {
           for (int i = 0; i < NumFilters; i++) {
               if (pfd[i].revents & POLLIN) {
                  cFilterHandle *fh = NULL;
                  LOCK_THREAD;
                  if (statusCount != oldStatusCount)
                     break;
                  for (fh = filterHandles.First(); fh; fh = filterHandles.Next(fh)) {
                      if (pfd[i].fd == fh->handle)
                         break;
                      }
                  if (fh) {
                     // Read section data:
                     unsigned char buf[4096]; // max. allowed size for any EIT section
                     int r = device->ReadFilter(fh->handle, buf, sizeof(buf));
                     if (flush)
                        continue; // we do the read anyway, to flush any data that might have come from a different transponder
                     if (r > 3) { // minimum number of bytes necessary to get section length
                        int len = (((buf[1] & 0x0F) << 8) | (buf[2] & 0xFF)) + 3;
                        if (len == r) {
                           // Distribute data to all attached filters:
                           int pid = fh->filterData.pid;
                           int tid = buf[0];
                           for (cFilter *fi = filters.First(); fi; fi = filters.Next(fi)) {
                               if (fi->Matches(pid, tid))
                                  fi->Process(pid, tid, buf, len);
                               }
                           }
                        else
                           dsyslog("tp %d (%d/%02X) read incomplete section - len = %d, r = %d", Transponder(), fh->filterData.pid, buf[0], len, r);
                        }
                     }
                  }
               }
           if (flush)
              flush = flushTimer.Elapsed() <= FLUSH_TIME;
           }
        }
}
