/*
 * dvbhdffdevice.h: The DVB HD Full Featured device interface
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef __DVBHDFFDEVICE_H
#define __DVBHDFFDEVICE_H

#include "hdffcmd.h"
#include <vdr/dvbdevice.h>
#include <vdr/dvbspu.h>

/// The cDvbHdFfDevice implements a DVB device which can be accessed through the Linux DVB driver API.

class cDvbHdFfDevice : public cDvbDevice {
private:
  int fd_osd, fd_audio, fd_video;
protected:
  virtual void MakePrimaryDevice(bool On);
public:
  static bool Probe(int Adapter, int Frontend);
  cDvbHdFfDevice(int Adapter, int Frontend);
  virtual ~cDvbHdFfDevice();
  virtual bool HasDecoder(void) const;

// SPU facilities

private:
  cDvbSpuDecoder *spuDecoder;
public:
  virtual cSpuDecoder *GetSpuDecoder(void);

// Channel facilities

private:
  void TurnOffLiveMode(bool LiveView);
protected:
  virtual bool SetChannelDevice(const cChannel *Channel, bool LiveView);

// PID handle facilities

protected:
  virtual bool SetPid(cPidHandle *Handle, int Type, bool On);

// Image Grab facilities

public:
  virtual uchar *GrabImage(int &Size, bool Jpeg = true, int Quality = -1, int SizeX = -1, int SizeY = -1);

// Video format facilities

public:
  virtual void SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat);
  virtual void GetVideoSize(int &Width, int &Height, double &VideoAspect);
  virtual void GetOsdSize(int &Width, int &Height, double &PixelAspect);

// Track facilities

protected:
  virtual void SetAudioTrackDevice(eTrackType Type);

// Audio facilities

private:
  int audioChannel;
protected:
  virtual int GetAudioChannelDevice(void);
  virtual void SetAudioChannelDevice(int AudioChannel);
  virtual void SetVolumeDevice(int Volume);

// Player facilities

private:
  int playVideoPid;
  int playAudioPid;
  int playPcrPid;
  bool freezed;
  bool trickMode;
  bool isPlayingVideo;
  bool isTransferMode;
  bool supportsPcrInTransferMode;

  // Pes2Ts conversion stuff
  uint8_t videoCounter;
  uint8_t audioCounter;
  void BuildTsPacket(uint8_t * TsBuffer, bool PusiSet, uint16_t Pid, uint8_t Counter, const uint8_t * Data, uint32_t Length);
  uint32_t PesToTs(uint8_t * TsBuffer, uint16_t Pid, uint8_t & Counter, const uint8_t * Data, uint32_t Length);

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
  virtual cRect CanScaleVideo(const cRect &Rect, int Alignment = taCenter);
  virtual void ScaleVideo(const cRect &Rect = cRect::Null);
#if (APIVERSNUM >= 20103)
  virtual void TrickSpeed(int Speed, bool Forward);
#else
  virtual void TrickSpeed(int Speed);
#endif
  virtual void Clear(void);
  virtual void Play(void);
  virtual void Freeze(void);
  virtual void Mute(void);
  virtual void StillPicture(const uchar *Data, int Length);
  virtual bool Poll(cPoller &Poller, int TimeoutMs = 0);
  virtual bool Flush(int TimeoutMs = 0);

// HDFF specific things

public:
  static HDFF::cHdffCmdIf *GetHdffCmdHandler(void);
private:
  static int devHdffOffset;//TODO
  bool isHdffPrimary;//TODO implicit!
  HDFF::cHdffCmdIf *mHdffCmdIf;
};

class cDvbHdFfDeviceProbe : public cDvbDeviceProbe {
public:
  virtual bool Probe(int Adapter, int Frontend);
  };

#endif //__DVBHDFFDEVICE_H
