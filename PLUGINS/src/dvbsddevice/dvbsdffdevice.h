/*
 * dvbsdffdevice.h: The DVB SD Full Featured device interface
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbsdffdevice.h 2.16 2013/02/17 13:16:29 kls Exp $
 */

#ifndef __DVBSDFFDEVICE_H
#define __DVBSDFFDEVICE_H

#include <vdr/dvbdevice.h>
#include <vdr/dvbspu.h>

/// The cDvbSdFfDevice implements a DVB device which can be accessed through the Linux DVB driver API.

class cDvbSdFfDevice : public cDvbDevice {
private:
  int fd_osd, fd_audio, fd_video, fd_stc;
  bool outputOnly;
protected:
  virtual void MakePrimaryDevice(bool On);
public:
  cDvbSdFfDevice(int Adapter, int Frontend, bool OutputOnly);
  virtual ~cDvbSdFfDevice();
  virtual bool HasDecoder(void) const;
  virtual bool AvoidRecording(void) const;

// SPU facilities

private:
  cDvbSpuDecoder *spuDecoder;
public:
  virtual cSpuDecoder *GetSpuDecoder(void);

// Channel facilities

public:
  virtual bool ProvidesSource(int Source) const;
  virtual int NumProvidedSystems(void) const;
private:
  void TurnOffLiveMode(bool LiveView);
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);

// PID handle facilities

private:
  bool SetAudioBypass(bool On);
protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);

// Image Grab facilities

private:
  static int devVideoOffset;
  int devVideoIndex;
public:
  virtual uchar *GrabImage(int &Size, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

// Video format facilities

public:
  virtual void SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat);
  virtual void SetVideoFormat(bool VideoFormat16_9);
  virtual eVideoSystem GetVideoSystem(void);
  virtual void GetVideoSize(int &Width, int &Height, double &VideoAspect);
  virtual void GetOsdSize(int &Width, int &Height, double &PixelAspect);

// Track facilities

protected:
  virtual void SetAudioTrackDevice(eTrackType Type);

// Audio facilities

private:
  bool digitalAudio;
protected:
  virtual int GetAudioChannelDevice(void);
  virtual void SetAudioChannelDevice(int AudioChannel);
  virtual void SetVolumeDevice(int Volume);
  virtual void SetDigitalAudioDevice(bool On);

// Player facilities

protected:
  ePlayMode playMode;
  virtual bool CanReplay(void) const;
  virtual bool SetPlayMode(ePlayMode PlayMode);
  virtual int PlayVideo(const uchar *Data, int Length);
  virtual int PlayAudio(const uchar *Data, int Length, uchar Id);
  virtual int PlayTsVideo(const uchar *Data, int Length);
  virtual int PlayTsAudio(const uchar *Data, int Length);
public:
  virtual int64_t GetSTC(void);
  virtual void TrickSpeed(int Speed);
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
  virtual bool Flush(int TimeoutMs = 0);
  };

class cDvbSdFfDeviceProbe : public cDvbDeviceProbe {
private:
  bool outputOnly;
public:
  cDvbSdFfDeviceProbe(void);
  void SetOutputOnly(bool On) { outputOnly = On; }
  virtual bool Probe(int Adapter, int Frontend);
  };

#endif //__DVBSDFFDEVICE_H
