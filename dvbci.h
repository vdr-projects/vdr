/*
 * dvbci.h: Common Interface for DVB devices
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbci.h 5.1 2025/03/02 11:03:35 kls Exp $
 */

#ifndef __DVBCI_H
#define __DVBCI_H

#include "ci.h"

class cDvbCiAdapter : public cCiAdapter {
private:
  cDevice *device;
  int fd;
protected:
  virtual int Read(uint8_t *Buffer, int MaxLength) override;
  virtual void Write(const uint8_t *Buffer, int Length) override;
  virtual bool Reset(int Slot) override;
  virtual eModuleStatus ModuleStatus(int Slot) override;
  virtual bool Assign(cDevice *Device, bool Query = false) override;
  cDvbCiAdapter(cDevice *Device, int Fd);
public:
  virtual ~cDvbCiAdapter() override;
  static cDvbCiAdapter *CreateCiAdapter(cDevice *Device, int Fd);
  };

#endif //__DVBCI_H
