/*
 * dvbsdffdevice.h: The DVB SD Full Featured device interface
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: dvbsdffdevice.c 2.35 2013/02/17 13:16:18 kls Exp $
 */

#include "dvbsdffdevice.h"
#include <errno.h>
#include <limits.h>
#include <linux/videodev2.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/video.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vdr/eitscan.h>
#include <vdr/transfer.h>
#include "dvbsdffosd.h"

// --- cDvbSdFfDevice --------------------------------------------------------

int cDvbSdFfDevice::devVideoOffset = -1;

cDvbSdFfDevice::cDvbSdFfDevice(int Adapter, int Frontend, bool OutputOnly)
:cDvbDevice(Adapter, Frontend)
{
  spuDecoder = NULL;
  digitalAudio = false;
  playMode = pmNone;
  outputOnly = OutputOnly;

  // Devices that are only present on cards with decoders:

  fd_osd      = DvbOpen(DEV_DVB_OSD,    adapter, frontend, O_RDWR);
  fd_video    = DvbOpen(DEV_DVB_VIDEO,  adapter, frontend, O_RDWR | O_NONBLOCK);
  fd_audio    = DvbOpen(DEV_DVB_AUDIO,  adapter, frontend, O_RDWR | O_NONBLOCK);
  fd_stc      = DvbOpen(DEV_DVB_DEMUX,  adapter, frontend, O_RDWR);

  // The offset of the /dev/video devices:

  if (devVideoOffset < 0) { // the first one checks this
     FILE *f = NULL;
     char buffer[PATH_MAX];
     for (int ofs = 0; ofs < 100; ofs++) {
         snprintf(buffer, sizeof(buffer), "/proc/video/dev/video%d", ofs);
         if ((f = fopen(buffer, "r")) != NULL) {
            if (fgets(buffer, sizeof(buffer), f)) {
               if (strstr(buffer, "DVB Board")) { // found the _first_ DVB card
                  devVideoOffset = ofs;
                  dsyslog("video device offset is %d", devVideoOffset);
                  break;
                  }
               }
            else
               break;
            fclose(f);
            }
         else
            break;
         }
     if (devVideoOffset < 0)
        devVideoOffset = 0;
     if (f)
        fclose(f);
     }
  devVideoIndex = devVideoOffset >= 0 ? devVideoOffset++ : -1;
}

cDvbSdFfDevice::~cDvbSdFfDevice()
{
  delete spuDecoder;
  // We're not explicitly closing any device files here, since this sometimes
  // caused segfaults. Besides, the program is about to terminate anyway...
}

void cDvbSdFfDevice::MakePrimaryDevice(bool On)
{
  if (On)
     new cDvbOsdProvider(fd_osd);
  cDvbDevice::MakePrimaryDevice(On);
}

bool cDvbSdFfDevice::HasDecoder(void) const
{
  return true;
}

bool cDvbSdFfDevice::AvoidRecording(void) const
{
  return true;
}

cSpuDecoder *cDvbSdFfDevice::GetSpuDecoder(void)
{
  if (!spuDecoder && IsPrimaryDevice())
     spuDecoder = new cDvbSpuDecoder();
  return spuDecoder;
}

uchar *cDvbSdFfDevice::GrabImage(int &Size, bool Jpeg, int Quality, int SizeX, int SizeY)
{
  if (devVideoIndex < 0)
     return NULL;
  char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d", DEV_VIDEO, devVideoIndex);
  int videoDev = open(buffer, O_RDWR);
  if (videoDev >= 0) {
     uchar *result = NULL;
     // set up the size and RGB
     v4l2_format fmt;
     memset(&fmt, 0, sizeof(fmt));
     fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     fmt.fmt.pix.width = SizeX;
     fmt.fmt.pix.height = SizeY;
     fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
     fmt.fmt.pix.field = V4L2_FIELD_ANY;
     if (ioctl(videoDev, VIDIOC_S_FMT, &fmt) == 0) {
        v4l2_requestbuffers reqBuf;
        memset(&reqBuf, 0, sizeof(reqBuf));
        reqBuf.count = 2;
        reqBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        reqBuf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(videoDev, VIDIOC_REQBUFS, &reqBuf) >= 0) {
           v4l2_buffer mbuf;
           memset(&mbuf, 0, sizeof(mbuf));
           mbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
           mbuf.memory = V4L2_MEMORY_MMAP;
           if (ioctl(videoDev, VIDIOC_QUERYBUF, &mbuf) == 0) {
              int msize = mbuf.length;
              unsigned char *mem = (unsigned char *)mmap(0, msize, PROT_READ | PROT_WRITE, MAP_SHARED, videoDev, 0);
              if (mem && mem != (unsigned char *)-1) {
                 v4l2_buffer buf;
                 memset(&buf, 0, sizeof(buf));
                 buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                 buf.memory = V4L2_MEMORY_MMAP;
                 buf.index = 0;
                 if (ioctl(videoDev, VIDIOC_QBUF, &buf) == 0) {
                    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    if (ioctl (videoDev, VIDIOC_STREAMON, &type) == 0) {
                       memset(&buf, 0, sizeof(buf));
                       buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                       buf.memory = V4L2_MEMORY_MMAP;
                       buf.index = 0;
                       if (ioctl(videoDev, VIDIOC_DQBUF, &buf) == 0) {
                          if (ioctl(videoDev, VIDIOC_STREAMOFF, &type) == 0) {
                             // make RGB out of BGR:
                             int memsize = fmt.fmt.pix.width * fmt.fmt.pix.height;
                             unsigned char *mem1 = mem;
                             for (int i = 0; i < memsize; i++) {
                                 unsigned char tmp = mem1[2];
                                 mem1[2] = mem1[0];
                                 mem1[0] = tmp;
                                 mem1 += 3;
                                 }

                             if (Quality < 0)
                                Quality = 100;

                             dsyslog("grabbing to %s %d %d %d", Jpeg ? "JPEG" : "PNM", Quality, fmt.fmt.pix.width, fmt.fmt.pix.height);
                             if (Jpeg) {
                                // convert to JPEG:
                                result = RgbToJpeg(mem, fmt.fmt.pix.width, fmt.fmt.pix.height, Size, Quality);
                                if (!result)
                                   esyslog("ERROR: failed to convert image to JPEG");
                                }
                             else {
                                // convert to PNM:
                                char buf[32];
                                snprintf(buf, sizeof(buf), "P6\n%d\n%d\n255\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
                                int l = strlen(buf);
                                int bytes = memsize * 3;
                                Size = l + bytes;
                                result = MALLOC(uchar, Size);
                                if (result) {
                                   memcpy(result, buf, l);
                                   memcpy(result + l, mem, bytes);
                                   }
                                else
                                   esyslog("ERROR: failed to convert image to PNM");
                                }
                             }
                          else
                             esyslog("ERROR: video device VIDIOC_STREAMOFF failed");
                          }
                       else
                          esyslog("ERROR: video device VIDIOC_DQBUF failed");
                       }
                    else
                       esyslog("ERROR: video device VIDIOC_STREAMON failed");
                    }
                 else
                    esyslog("ERROR: video device VIDIOC_QBUF failed");
                 munmap(mem, msize);
                 }
              else
                 esyslog("ERROR: failed to memmap video device");
              }
           else
              esyslog("ERROR: video device VIDIOC_QUERYBUF failed");
           }
        else
           esyslog("ERROR: video device VIDIOC_REQBUFS failed");
        }
     else
        esyslog("ERROR: video device VIDIOC_S_FMT failed");
     close(videoDev);
     return result;
     }
  else
     LOG_ERROR_STR(buffer);
  return NULL;
}

void cDvbSdFfDevice::SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat)
{
  cDevice::SetVideoDisplayFormat(VideoDisplayFormat);
  if (Setup.VideoFormat) {
     CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_LETTER_BOX));
     }
  else {
     switch (VideoDisplayFormat) {
       case vdfPanAndScan:
            CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_PAN_SCAN));
            break;
       case vdfLetterBox:
            CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_LETTER_BOX));
            break;
       case vdfCenterCutOut:
            CHECK(ioctl(fd_video, VIDEO_SET_DISPLAY_FORMAT, VIDEO_CENTER_CUT_OUT));
            break;
       default: esyslog("ERROR: unknown video display format %d", VideoDisplayFormat);
       }
     }
}

void cDvbSdFfDevice::SetVideoFormat(bool VideoFormat16_9)
{
  CHECK(ioctl(fd_video, VIDEO_SET_FORMAT, VideoFormat16_9 ? VIDEO_FORMAT_16_9 : VIDEO_FORMAT_4_3));
  SetVideoDisplayFormat(eVideoDisplayFormat(Setup.VideoDisplayFormat));
}

eVideoSystem cDvbSdFfDevice::GetVideoSystem(void)
{
  eVideoSystem VideoSystem = vsPAL;
  if (fd_video >= 0) {
     video_size_t vs;
     if (ioctl(fd_video, VIDEO_GET_SIZE, &vs) == 0) {
        if (vs.h == 480 || vs.h == 240)
           VideoSystem = vsNTSC;
        }
     else
        LOG_ERROR;
     }
  return VideoSystem;
}

void cDvbSdFfDevice::GetVideoSize(int &Width, int &Height, double &VideoAspect)
{
  if (fd_video >= 0) {
     video_size_t vs;
     if (ioctl(fd_video, VIDEO_GET_SIZE, &vs) == 0) {
        Width = vs.w;
        Height = vs.h;
        switch (vs.aspect_ratio) {
          default:
          case VIDEO_FORMAT_4_3:   VideoAspect =  4.0 / 3.0; break;
          case VIDEO_FORMAT_16_9:  VideoAspect = 16.0 / 9.0; break;
          case VIDEO_FORMAT_221_1: VideoAspect =       2.21; break;
          }
        return;
        }
     else
        LOG_ERROR;
     }
  cDevice::GetVideoSize(Width, Height, VideoAspect);
}

void cDvbSdFfDevice::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
  if (fd_video >= 0) {
     video_size_t vs;
     if (ioctl(fd_video, VIDEO_GET_SIZE, &vs) == 0) {
        Width = 720;
        if (vs.h != 480 && vs.h != 240)
           Height = 576; // PAL
        else
           Height = 480; // NTSC
        switch (Setup.VideoFormat ? vs.aspect_ratio : VIDEO_FORMAT_4_3) {
          default:
          case VIDEO_FORMAT_4_3:   PixelAspect =  4.0 / 3.0; break;
          case VIDEO_FORMAT_221_1: // FF DVB cards only distinguish between 4:3 and 16:9
          case VIDEO_FORMAT_16_9:  PixelAspect = 16.0 / 9.0; break;
          }
        PixelAspect /= double(Width) / Height;
        return;
        }
     else
        LOG_ERROR;
     }
  cDevice::GetOsdSize(Width, Height, PixelAspect);
}

bool cDvbSdFfDevice::SetAudioBypass(bool On)
{
  if (setTransferModeForDolbyDigital != 1)
     return false;
  return ioctl(fd_audio, AUDIO_SET_BYPASS_MODE, On) == 0;
}

//                                   ptAudio        ptVideo        ptPcr        ptTeletext        ptDolby        ptOther
static dmx_pes_type_t PesTypes[] = { DMX_PES_AUDIO, DMX_PES_VIDEO, DMX_PES_PCR, DMX_PES_TELETEXT, DMX_PES_OTHER, DMX_PES_OTHER };

bool cDvbSdFfDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
  if (Handle->pid) {
     dmx_pes_filter_params pesFilterParams;
     memset(&pesFilterParams, 0, sizeof(pesFilterParams));
     if (On) {
        if (Handle->handle < 0) {
           Handle->handle = DvbOpen(DEV_DVB_DEMUX, adapter, frontend, O_RDWR | O_NONBLOCK, true);
           if (Handle->handle < 0) {
              LOG_ERROR;
              return false;
              }
           }
        pesFilterParams.pid     = Handle->pid;
        pesFilterParams.input   = DMX_IN_FRONTEND;
        pesFilterParams.output  = (Type <= ptTeletext && Handle->used <= 1) ? DMX_OUT_DECODER : DMX_OUT_TS_TAP;
        pesFilterParams.pes_type= PesTypes[Type < ptOther ? Type : ptOther];
        pesFilterParams.flags   = DMX_IMMEDIATE_START;
        if (ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
           LOG_ERROR;
           return false;
           }
        }
     else if (!Handle->used) {
        CHECK(ioctl(Handle->handle, DMX_STOP));
        if (Type <= ptTeletext) {
           pesFilterParams.pid     = 0x1FFF;
           pesFilterParams.input   = DMX_IN_FRONTEND;
           pesFilterParams.output  = DMX_OUT_DECODER;
           pesFilterParams.pes_type= PesTypes[Type];
           pesFilterParams.flags   = DMX_IMMEDIATE_START;
           CHECK(ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams));
           if (PesTypes[Type] == DMX_PES_VIDEO) // let's only do this once
              SetPlayMode(pmNone); // necessary to switch a PID from DMX_PES_VIDEO/AUDIO to DMX_PES_OTHER
           }
        close(Handle->handle);
        Handle->handle = -1;
        }
     }
  return true;
}

bool cDvbSdFfDevice::ProvidesSource(int Source) const
{
  if (outputOnly)
     return false;
  else
     return cDvbDevice::ProvidesSource(Source);
}

int cDvbSdFfDevice::NumProvidedSystems(void) const
{
  if (outputOnly)
     return 0;
  return cDvbDevice::NumProvidedSystems();
}

void cDvbSdFfDevice::TurnOffLiveMode(bool LiveView)
{
  if (LiveView) {
     // Avoid noise while switching:
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
     }

  // Turn off live PIDs:

  DetachAll(pidHandles[ptAudio].pid);
  DetachAll(pidHandles[ptVideo].pid);
  DetachAll(pidHandles[ptPcr].pid);
  DetachAll(pidHandles[ptTeletext].pid);
  DelPid(pidHandles[ptAudio].pid);
  DelPid(pidHandles[ptVideo].pid);
  DelPid(pidHandles[ptPcr].pid, ptPcr);
  DelPid(pidHandles[ptTeletext].pid);
  DelPid(pidHandles[ptDolby].pid);
}

bool cDvbSdFfDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
{
  int apid = Channel->Apid(0);
  int vpid = Channel->Vpid();
  int dpid = Channel->Dpid(0);

  bool DoTune = !IsTunedToTransponder(Channel);

  bool pidHandlesVideo = pidHandles[ptVideo].pid == vpid;
  bool pidHandlesAudio = pidHandles[ptAudio].pid == apid;

  bool TurnOffLivePIDs = DoTune
                         || !IsPrimaryDevice()
                         || LiveView // for a new live view the old PIDs need to be turned off
                         || pidHandlesVideo // for recording the PIDs must be shifted from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                         ;

  bool StartTransferMode = IsPrimaryDevice() && !DoTune
                           && (LiveView && HasPid(vpid ? vpid : apid) && (!pidHandlesVideo || (!pidHandlesAudio && (dpid ? pidHandles[ptAudio].pid != dpid : true)))// the PID is already set as DMX_PES_OTHER
                              || !LiveView && (pidHandlesVideo || pidHandlesAudio) // a recording is going to shift the PIDs from DMX_PES_AUDIO/VIDEO to DMX_PES_OTHER
                              );
  if (CamSlot() && !ChannelCamRelations.CamDecrypt(Channel->GetChannelID(), CamSlot()->SlotNumber()))
     StartTransferMode |= LiveView && IsPrimaryDevice() && Channel->Ca() >= CA_ENCRYPTED_MIN;

  bool TurnOnLivePIDs = !StartTransferMode && LiveView;

  // Turn off live PIDs if necessary:

  if (TurnOffLivePIDs)
     TurnOffLiveMode(LiveView);

  // Set the tuner:

  if (!cDvbDevice::SetChannelDevice(Channel, LiveView))
     return false;

  // PID settings:

  if (TurnOnLivePIDs) {
     SetAudioBypass(false);
     if (!(AddPid(Channel->Ppid(), ptPcr) && AddPid(vpid, ptVideo) && AddPid(apid, ptAudio))) {
        esyslog("ERROR: failed to set PIDs for channel %d on device %d", Channel->Number(), CardIndex() + 1);
        return false;
        }
     if (IsPrimaryDevice())
        AddPid(Channel->Tpid(), ptTeletext);
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true)); // actually one would expect 'false' here, but according to Marco Schluessler <marco@lordzodiac.de> this works
                                                   // to avoid missing audio after replaying a DVD; with 'false' there is an audio disturbance when switching
                                                   // between two channels on the same transponder on DVB-S
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
     }
  else if (StartTransferMode)
     cControl::Launch(new cTransferControl(this, Channel));

  return true;
}

int cDvbSdFfDevice::GetAudioChannelDevice(void)
{
  audio_status_t as;
  CHECK(ioctl(fd_audio, AUDIO_GET_STATUS, &as));
  return as.channel_select;
}

void cDvbSdFfDevice::SetAudioChannelDevice(int AudioChannel)
{
  CHECK(ioctl(fd_audio, AUDIO_CHANNEL_SELECT, AudioChannel));
}

void cDvbSdFfDevice::SetVolumeDevice(int Volume)
{
  if (digitalAudio)
     Volume = 0;
  audio_mixer_t am;
  // conversion for linear volume response:
  am.volume_left = am.volume_right = 2 * Volume - Volume * Volume / 255;
  CHECK(ioctl(fd_audio, AUDIO_SET_MIXER, &am));
}

void cDvbSdFfDevice::SetDigitalAudioDevice(bool On)
{
  if (digitalAudio != On) {
     if (digitalAudio)
        cCondWait::SleepMs(1000); // Wait until any leftover digital data has been flushed
     digitalAudio = On;
     SetVolumeDevice(On || IsMute() ? 0 : CurrentVolume());
     }
}

void cDvbSdFfDevice::SetAudioTrackDevice(eTrackType Type)
{
  const tTrackId *TrackId = GetTrack(Type);
  if (TrackId && TrackId->id) {
     SetAudioBypass(false);
     if (IS_AUDIO_TRACK(Type) || (IS_DOLBY_TRACK(Type) && SetAudioBypass(true))) {
        if (pidHandles[ptAudio].pid && pidHandles[ptAudio].pid != TrackId->id) {
           DetachAll(pidHandles[ptAudio].pid);
           if (CamSlot())
              CamSlot()->SetPid(pidHandles[ptAudio].pid, false);
           pidHandles[ptAudio].pid = TrackId->id;
           SetPid(&pidHandles[ptAudio], ptAudio, true);
           if (CamSlot()) {
              CamSlot()->SetPid(pidHandles[ptAudio].pid, true);
              CamSlot()->StartDecrypting();
              }
           }
        }
     else if (IS_DOLBY_TRACK(Type)) {
        if (setTransferModeForDolbyDigital == 0)
           return;
        // Currently this works only in Transfer Mode
        ForceTransferMode();
        }
     }
}

bool cDvbSdFfDevice::CanReplay(void) const
{
  return cDevice::CanReplay();
}

bool cDvbSdFfDevice::SetPlayMode(ePlayMode PlayMode)
{
  if (PlayMode != pmExtern_THIS_SHOULD_BE_AVOIDED && fd_video < 0 && fd_audio < 0) {
     // reopen the devices
     fd_video = DvbOpen(DEV_DVB_VIDEO, adapter, frontend, O_RDWR | O_NONBLOCK);
     fd_audio = DvbOpen(DEV_DVB_AUDIO, adapter, frontend, O_RDWR | O_NONBLOCK);
     SetVideoFormat(Setup.VideoFormat);
     }

  switch (PlayMode) {
    case pmNone:
         // special handling to return from PCM replay:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));

         CHECK(ioctl(fd_video, VIDEO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_STOP, true));
         CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
         CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
         CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, false));
         break;
    case pmAudioVideo:
    case pmAudioOnlyBlack:
         if (playMode == pmNone)
            TurnOffLiveMode(true);
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, PlayMode == pmAudioVideo));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));
         break;
    case pmAudioOnly:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_audio, AUDIO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, false));
         break;
    case pmVideoOnly:
         CHECK(ioctl(fd_video, VIDEO_SET_BLANK, true));
         CHECK(ioctl(fd_video, VIDEO_STOP, true));
         CHECK(ioctl(fd_audio, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX));
         CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
         CHECK(ioctl(fd_audio, AUDIO_PLAY));
         CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
         CHECK(ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY));
         CHECK(ioctl(fd_video, VIDEO_PLAY));
         break;
    case pmExtern_THIS_SHOULD_BE_AVOIDED:
         close(fd_video);
         close(fd_audio);
         fd_video = fd_audio = -1;
         break;
    default: esyslog("ERROR: unknown playmode %d", PlayMode);
    }
  playMode = PlayMode;
  return true;
}

int64_t cDvbSdFfDevice::GetSTC(void)
{
  if (fd_stc >= 0) {
     struct dmx_stc stc;
     stc.num = 0;
     if (ioctl(fd_stc, DMX_GET_STC, &stc) == -1) {
        esyslog("ERROR: stc %d: %m", CardIndex() + 1);
        return -1;
        }
     return stc.stc / stc.base;
     }
  return -1;
}

void cDvbSdFfDevice::TrickSpeed(int Speed)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_SLOWMOTION, Speed));
}

void cDvbSdFfDevice::Clear(void)
{
  if (fd_video >= 0)
     CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
  if (fd_audio >= 0)
     CHECK(ioctl(fd_audio, AUDIO_CLEAR_BUFFER));
  cDevice::Clear();
}

void cDvbSdFfDevice::Play(void)
{
  if (playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_CONTINUE));
     }
  else {
     if (fd_audio >= 0) {
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, true));
        CHECK(ioctl(fd_audio, AUDIO_CONTINUE));
        }
     if (fd_video >= 0)
        CHECK(ioctl(fd_video, VIDEO_CONTINUE));
     }
  cDevice::Play();
}

void cDvbSdFfDevice::Freeze(void)
{
  if (playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) {
     if (fd_audio >= 0)
        CHECK(ioctl(fd_audio, AUDIO_PAUSE));
     }
  else {
     if (fd_audio >= 0) {
        CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
        CHECK(ioctl(fd_audio, AUDIO_PAUSE));
        }
     if (fd_video >= 0)
        CHECK(ioctl(fd_video, VIDEO_FREEZE));
     }
  cDevice::Freeze();
}

void cDvbSdFfDevice::Mute(void)
{
  if (fd_audio >= 0) {
     CHECK(ioctl(fd_audio, AUDIO_SET_AV_SYNC, false));
     CHECK(ioctl(fd_audio, AUDIO_SET_MUTE, true));
     }
  cDevice::Mute();
}

void cDvbSdFfDevice::StillPicture(const uchar *Data, int Length)
{
  if (!Data || Length < TS_SIZE)
     return;
  if (Data[0] == 0x47) {
     // TS data
     cDevice::StillPicture(Data, Length);
     }
  else if (Data[0] == 0x00 && Data[1] == 0x00 && Data[2] == 0x01 && (Data[3] & 0xF0) == 0xE0) {
     // PES data
     char *buf = MALLOC(char, Length);
     if (!buf)
        return;
     int i = 0;
     int blen = 0;
     while (i < Length - 6) {
           if (Data[i] == 0x00 && Data[i + 1] == 0x00 && Data[i + 2] == 0x01) {
              int len = Data[i + 4] * 256 + Data[i + 5];
              if ((Data[i + 3] & 0xF0) == 0xE0) { // video packet
                 // skip PES header
                 int offs = i + 6;
                 // skip header extension
                 if ((Data[i + 6] & 0xC0) == 0x80) {
                    // MPEG-2 PES header
                    if (Data[i + 8] >= Length)
                       break;
                    offs += 3;
                    offs += Data[i + 8];
                    len -= 3;
                    len -= Data[i + 8];
                    if (len < 0 || offs + len > Length)
                       break;
                    }
                 else {
                    // MPEG-1 PES header
                    while (offs < Length && len > 0 && Data[offs] == 0xFF) {
                          offs++;
                          len--;
                          }
                    if (offs <= Length - 2 && len >= 2 && (Data[offs] & 0xC0) == 0x40) {
                       offs += 2;
                       len -= 2;
                       }
                    if (offs <= Length - 5 && len >= 5 && (Data[offs] & 0xF0) == 0x20) {
                       offs += 5;
                       len -= 5;
                       }
                    else if (offs <= Length - 10 && len >= 10 && (Data[offs] & 0xF0) == 0x30) {
                       offs += 10;
                       len -= 10;
                       }
                    else if (offs < Length && len > 0) {
                       offs++;
                       len--;
                       }
                    }
                 if (blen + len > Length) // invalid PES length field
                    break;
                 memcpy(&buf[blen], &Data[offs], len);
                 i = offs + len;
                 blen += len;
                 }
              else if (Data[i + 3] >= 0xBD && Data[i + 3] <= 0xDF) // other PES packets
                 i += len + 6;
              else
                 i++;
              }
           else
              i++;
           }
     video_still_picture sp = { buf, blen };
     CHECK(ioctl(fd_video, VIDEO_STILLPICTURE, &sp));
     free(buf);
     }
  else {
     // non-PES data
     video_still_picture sp = { (char *)Data, Length };
     CHECK(ioctl(fd_video, VIDEO_STILLPICTURE, &sp));
     }
}

bool cDvbSdFfDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  Poller.Add((playMode == pmAudioOnly || playMode == pmAudioOnlyBlack) ? fd_audio : fd_video, true);
  return Poller.Poll(TimeoutMs);
}

bool cDvbSdFfDevice::Flush(int TimeoutMs)
{
  //TODO actually this function should wait until all buffered data has been processed by the card, but how?
  return true;
}

int cDvbSdFfDevice::PlayVideo(const uchar *Data, int Length)
{
  return WriteAllOrNothing(fd_video, Data, Length, 1000, 10);
}

int cDvbSdFfDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
{
  return WriteAllOrNothing(fd_audio, Data, Length, 1000, 10);
}

int cDvbSdFfDevice::PlayTsVideo(const uchar *Data, int Length)
{
  return WriteAllOrNothing(fd_video, Data, Length, 1000, 10);
}

int cDvbSdFfDevice::PlayTsAudio(const uchar *Data, int Length)
{
  return WriteAllOrNothing(fd_audio, Data, Length, 1000, 10);
}

// --- cDvbSdFfDeviceProbe ---------------------------------------------------

cDvbSdFfDeviceProbe::cDvbSdFfDeviceProbe(void)
{
  outputOnly = false;
}

bool cDvbSdFfDeviceProbe::Probe(int Adapter, int Frontend)
{
  static uint32_t SubsystemIds[] = {
    0x110A0000, // Fujitsu Siemens DVB-C
    0x13C20000, // Technotrend/Hauppauge WinTV DVB-S rev1.X or Fujitsu Siemens DVB-C
    0x13C20001, // Technotrend/Hauppauge WinTV DVB-T rev1.X
    0x13C20002, // Technotrend/Hauppauge WinTV DVB-C rev2.X
    0x13C20003, // Technotrend/Hauppauge WinTV Nexus-S rev2.X
    0x13C20004, // Galaxis DVB-S rev1.3
    0x13C20006, // Fujitsu Siemens DVB-S rev1.6
    0x13C20008, // Technotrend/Hauppauge DVB-T
    0x13C2000A, // Technotrend/Hauppauge WinTV Nexus-CA rev1.X
    0x13C2000E, // Technotrend/Hauppauge WinTV Nexus-S rev2.3
    0x13C21002, // Technotrend/Hauppauge WinTV DVB-S rev1.3 SE
    0x00000000
    };
  uint32_t SubsystemId = GetSubsystemId(Adapter, Frontend);
  for (uint32_t *sid = SubsystemIds; *sid; sid++) {
      if (*sid == SubsystemId) {
         dsyslog("creating cDvbSdFfDevice");
         new cDvbSdFfDevice(Adapter, Frontend, outputOnly);
         return true;
         }
      }
  return false;
}
