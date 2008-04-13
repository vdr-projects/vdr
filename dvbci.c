/*
 * dvbci.h: Common Interface for DVB devices
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbci.c 2.0 2007/01/04 12:49:10 kls Exp $
 */

#include "dvbci.h"
#include <linux/dvb/ca.h>
#include <sys/ioctl.h>
#include "device.h"

// --- cDvbCiAdapter ---------------------------------------------------------

cDvbCiAdapter::cDvbCiAdapter(cDevice *Device, int Fd)
{
  device = Device;
  SetDescription("CI adapter on device %d", device->DeviceNumber());
  fd = Fd;
  ca_caps_t Caps;
  if (ioctl(fd, CA_GET_CAP, &Caps) == 0) {
     if ((Caps.slot_type & CA_CI_LINK) != 0) {
        int NumSlots = Caps.slot_num;
        if (NumSlots > 0) {
           for (int i = 0; i < NumSlots; i++)
               new cCamSlot(this);
           Start();
           }
        else
           esyslog("ERROR: no CAM slots found on device %d", device->DeviceNumber());
        }
     else
        isyslog("device %d doesn't support CI link layer interface", device->DeviceNumber());
     }
  else
     esyslog("ERROR: can't get CA capabilities on device %d", device->DeviceNumber());
}

cDvbCiAdapter::~cDvbCiAdapter()
{
  Cancel(3);
}

int cDvbCiAdapter::Read(uint8_t *Buffer, int MaxLength)
{
  if (Buffer && MaxLength > 0) {
     struct pollfd pfd[1];
     pfd[0].fd = fd;
     pfd[0].events = POLLIN;
     if (poll(pfd, 1, CAM_READ_TIMEOUT) > 0 && (pfd[0].revents & POLLIN)) {
        int n = safe_read(fd, Buffer, MaxLength);
        if (n >= 0)
           return n;
        esyslog("ERROR: can't read from CI adapter on device %d: %m", device->DeviceNumber());
        }
     }
  return 0;
}

void cDvbCiAdapter::Write(const uint8_t *Buffer, int Length)
{
  if (Buffer && Length > 0) {
     if (safe_write(fd, Buffer, Length) != Length)
        esyslog("ERROR: can't write to CI adapter on device %d: %m", device->DeviceNumber());
     }
}

bool cDvbCiAdapter::Reset(int Slot)
{
  if (ioctl(fd, CA_RESET, 1 << Slot) != -1)
     return true;
  else
     esyslog("ERROR: can't reset CAM slot %d on device %d: %m", Slot, device->DeviceNumber());
  return false;
}

eModuleStatus cDvbCiAdapter::ModuleStatus(int Slot)
{
  ca_slot_info_t sinfo;
  sinfo.num = Slot;
  if (ioctl(fd, CA_GET_SLOT_INFO, &sinfo) != -1) {
     if ((sinfo.flags & CA_CI_MODULE_READY) != 0)
        return msReady;
     else if ((sinfo.flags & CA_CI_MODULE_PRESENT) != 0)
        return msPresent;
     }
  else
     esyslog("ERROR: can't get info of CAM slot %d on device %d: %m", Slot, device->DeviceNumber());
  return msNone;
}

bool cDvbCiAdapter::Assign(cDevice *Device, bool Query)
{
  // The CI is hardwired to its device, so there's not really much to do here
  if (Device)
     return Device == device;
  return true;
}

cDvbCiAdapter *cDvbCiAdapter::CreateCiAdapter(cDevice *Device, int Fd)
{
  // TODO check whether a CI is actually present?
  if (Device)
     return new cDvbCiAdapter(Device, Fd);
  return NULL;
}
