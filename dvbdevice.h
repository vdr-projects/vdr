/*
 * dvbdevice.h: The DVB device interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: dvbdevice.h 1.2 2002/08/09 16:23:53 kls Exp $
 */

#ifndef __DVBDEVICE_H
#define __DVBDEVICE_H

#ifdef NEWSTRUCT
#include <linux/dvb/frontend.h>
#else
#include <stdlib.h> // FIXME: this is apparently necessary for the ost/... header files
                    // FIXME: shouldn't every header file include ALL the other header
                    // FIXME: files it depends on? The sequence in which header files
                    // FIXME: are included here should not matter - and it should NOT
                    // FIXME: be necessary to include <stdlib.h> here!
#include <ost/frontend.h>
#endif
#include "device.h"
#include "eit.h"

class cDvbDevice : public cDevice {
  friend class cDvbOsd;
private:
  static bool Probe(const char *FileName);
         // Probes for existing DVB devices.
public:
  static bool Initialize(void);
         // Initializes the DVB devices.
         // Must be called before accessing any DVB functions.
private:
  FrontendType frontendType;
#ifdef NEWSTRUCT
  int fd_osd, fd_frontend, fd_audio, fd_video, fd_dvr;
#else
  int fd_osd, fd_frontend, fd_sec, fd_audio, fd_video, fd_dvr;
#endif
  int OsdDeviceHandle(void) const { return fd_osd; }
protected:
  virtual void MakePrimaryDevice(bool On);
public:
  cDvbDevice(int n);
  virtual ~cDvbDevice();
  virtual bool CanBeReUsed(int Frequency, int Vpid);
  virtual bool HasDecoder(void) const;

// Channel facilities

private:
  int frequency;
public:
  virtual bool SetChannelDevice(const cChannel *Channel);

// PID handle facilities

protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);

// Image Grab facilities

public:
  virtual bool GrabImage(const char *FileName, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

// Video format facilities

public:
  virtual void SetVideoFormat(bool VideoFormat16_9);

// Volume facilities

protected:
  virtual void SetVolumeDevice(int Volume);

// EIT facilities

private:
  cSIProcessor *siProcessor;

// Player facilities

protected:
  virtual int SetPlayMode(bool On);
public:
  virtual void TrickSpeed(int Speed);
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual int PlayVideo(const uchar *Data, int Length);
  virtual int PlayAudio(const uchar *Data, int Length);

// Receiver facilities

protected:
  virtual bool OpenDvr(void);
  virtual void CloseDvr(void);
  virtual int GetTSPacket(uchar *Data);
  };

#endif //__DVBDEVICE_H
