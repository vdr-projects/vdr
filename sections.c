/*
 * sections.c: Section data handling
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: sections.c 1.1 2003/12/22 11:17:38 kls Exp $
 */

#include "sections.h"
#include <unistd.h>
#include "device.h"

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

// --- cSectionHandler -------------------------------------------------------

cSectionHandler::cSectionHandler(cDevice *Device)
:cThread("Section handler")
{
  device = Device;
  active = false;
  source = 0;
  transponder = 0;
  statusCount = 0;
  on = false;
  Start();
}

cSectionHandler::~cSectionHandler()
{
  active = false;
  Cancel(3);
  cFilter *fi;
  while ((fi = filters.First()) != NULL)
        Detach(fi);
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
     fh = new cFilterHandle(*FilterData);
     filterHandles.Add(fh);
     fh->handle = device->OpenFilter(FilterData->pid, FilterData->tid, FilterData->mask);
     }
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
            close(fh->handle);
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

void cSectionHandler::SetSource(int Source, int Transponder)
{
  source = Source;
  transponder = Transponder;
}

void cSectionHandler::SetStatus(bool On)
{
  if (on != On) {
     Lock();
     statusCount++;
     for (cFilter *fi = filters.First(); fi; fi = filters.Next(fi)) {
         fi->SetStatus(false);
         if (On)
            fi->SetStatus(true);
         }
     Unlock();
     on = On;
     }
}

void cSectionHandler::Action(void)
{
  active = true;
  while (active) {

        Lock();
        int NumFilters = filterHandles.Count();
        pollfd pfd[NumFilters];
        for (cFilterHandle *fh = filterHandles.First(); fh; fh = filterHandles.Next(fh)) {
            int i = fh->Index();
            pfd[i].fd = fh->handle;
            pfd[i].events = POLLIN;
            }
        int oldStatusCount = statusCount;
        Unlock();

        if (poll(pfd, NumFilters, 1000) != 0) {
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
                     int r = safe_read(fh->handle, buf, sizeof(buf));
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
                           dsyslog("read incomplete section - len = %d, r = %d", len, r);
                        }
                     }
                  }
               }
           }
        }
}
