/*
 * dvbhdffdevice.c: The DVB HD Full Featured device interface
 *
 * See the README file for copyright information and how to reach the author.
 */

#include <stdint.h>

#include "dvbhdffdevice.h"
#include <errno.h>
#include <limits.h>
#include <libsi/si.h>
#include <linux/videodev2.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/video.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vdr/eitscan.h>
#include <vdr/transfer.h>
#include "hdffosd.h"
#include "setup.h"


static uchar *YuvToJpeg(uchar *Mem, int Width, int Height, int &Size, int Quality);


// --- cDvbHdFfDevice --------------------------------------------------------

int cDvbHdFfDevice::devHdffOffset = -1;

cDvbHdFfDevice::cDvbHdFfDevice(int Adapter, int Frontend)
:cDvbDevice(Adapter, Frontend)
{
  spuDecoder = NULL;
  audioChannel = 0;
  playMode = pmNone;
  mHdffCmdIf = NULL;

  // Devices that are only present on cards with decoders:

  fd_osd      = DvbOpen(DEV_DVB_OSD,    adapter, frontend, O_RDWR);
  fd_video    = DvbOpen(DEV_DVB_VIDEO,  adapter, frontend, O_RDWR | O_NONBLOCK);
  fd_audio    = DvbOpen(DEV_DVB_AUDIO,  adapter, frontend, O_RDWR | O_NONBLOCK);

  //TODO missing /dev/video offset calculation

  isHdffPrimary = false;
  if (devHdffOffset < 0) {
     devHdffOffset = adapter;
     isHdffPrimary = true;
     mHdffCmdIf = new HDFF::cHdffCmdIf(fd_osd);

     /* reset some stuff in case the VDR was killed before and had no chance
        to clean up. */
     mHdffCmdIf->CmdOsdReset();

     mHdffCmdIf->CmdAvSetVideoSpeed(0, 100);
     mHdffCmdIf->CmdAvSetAudioSpeed(0, 100);

     mHdffCmdIf->CmdAvEnableVideoAfterStop(0, false);
     mHdffCmdIf->CmdAvSetPcrPid(0, 0);
     mHdffCmdIf->CmdAvSetVideoPid(0, 0, HDFF_VIDEO_STREAM_MPEG1);
     mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_MPEG1);

     ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
     mHdffCmdIf->CmdAvSetDecoderInput(0, 0);
     mHdffCmdIf->CmdAvEnableSync(0, true);
     mHdffCmdIf->CmdAvSetPlayMode(0, true);
     /* reset done */

     mHdffCmdIf->CmdAvSetAudioDelay(gHdffSetup.AudioDelay);
     mHdffCmdIf->CmdAvSetAudioDownmix((HdffAudioDownmixMode_t) gHdffSetup.AudioDownmix);
     mHdffCmdIf->CmdAvSetSyncShift(gHdffSetup.AvSyncShift);
     mHdffCmdIf->CmdMuxSetVideoOut((HdffVideoOut_t) gHdffSetup.AnalogueVideo);
     mHdffCmdIf->CmdHdmiSetVideoMode(gHdffSetup.GetVideoMode());

     HdffHdmiConfig_t hdmiConfig;
     memset(&hdmiConfig, 0, sizeof(hdmiConfig));
     hdmiConfig.TransmitAudio = true;
     hdmiConfig.ForceDviMode = false;
     hdmiConfig.CecEnabled = gHdffSetup.CecEnabled;
     strcpy(hdmiConfig.CecDeviceName, "VDR");
     hdmiConfig.VideoModeAdaption = (HdffVideoModeAdaption_t) gHdffSetup.VideoModeAdaption;
     mHdffCmdIf->CmdHdmiConfigure(&hdmiConfig);

     mHdffCmdIf->CmdRemoteSetProtocol((HdffRemoteProtocol_t) gHdffSetup.RemoteProtocol);
     mHdffCmdIf->CmdRemoteSetAddressFilter(gHdffSetup.RemoteAddress >= 0, gHdffSetup.RemoteAddress);
     }
}

cDvbHdFfDevice::~cDvbHdFfDevice()
{
    delete spuDecoder;
    if (isHdffPrimary)
    {
        delete mHdffCmdIf;
    }
    // We're not explicitly closing any device files here, since this sometimes
    // caused segfaults. Besides, the program is about to terminate anyway...
}

void cDvbHdFfDevice::MakePrimaryDevice(bool On)
{
  if (On)
     new cHdffOsdProvider(mHdffCmdIf);
  cDvbDevice::MakePrimaryDevice(On);
}

bool cDvbHdFfDevice::HasDecoder(void) const
{
  return isHdffPrimary;
}

cSpuDecoder *cDvbHdFfDevice::GetSpuDecoder(void)
{
  if (!spuDecoder && IsPrimaryDevice())
     spuDecoder = new cDvbSpuDecoder();
  return spuDecoder;
}

uchar *cDvbHdFfDevice::GrabImage(int &Size, bool Jpeg, int Quality, int SizeX, int SizeY)
{
    #define BUFFER_SIZE  (sizeof(struct v4l2_pix_format) + 1920 * 1080 * 2)
    int fd;
    uint8_t * buffer;
    uint8_t * result = NULL;

    fd = DvbOpen(DEV_DVB_VIDEO, adapter, frontend, O_RDONLY);
    if (fd < 0) {
        esyslog("GrabImage: failed open DVB video device");
        return NULL;
    }

    buffer = (uint8_t *) malloc(BUFFER_SIZE);
    if (buffer)
    {
        int readBytes;

        readBytes = read(fd, buffer, BUFFER_SIZE);
        if (readBytes < (int) sizeof(struct v4l2_pix_format))
            esyslog("GrabImage: failed reading from DVB video device");
        else {
            struct v4l2_pix_format * pixfmt;
            int dataSize;

            pixfmt = (struct v4l2_pix_format *) buffer;
            dsyslog("GrabImage: Read image of size %d x %d",
                    pixfmt->width, pixfmt->height);
            dataSize = readBytes - sizeof(struct v4l2_pix_format);
            if (dataSize < (int) pixfmt->sizeimage)
                esyslog("GrabImage: image is not complete");
            else {
                if (Jpeg) {
                    uint8_t * temp;
                    temp = (uint8_t *) malloc(pixfmt->width * 3 * pixfmt->height);
                    if (temp) {
                        int numPixels = pixfmt->width * pixfmt->height;
                        uint8_t * destData = temp;
                        uint8_t * srcData = buffer + sizeof(struct v4l2_pix_format);
                        while (numPixels > 0)
                        {
                            destData[0] = srcData[1];
                            destData[1] = srcData[0];
                            destData[2] = srcData[2];
                            destData[3] = srcData[3];
                            destData[4] = srcData[0];
                            destData[5] = srcData[2];
                            srcData += 4;
                            destData += 6;
                            numPixels -= 2;
                        }
                        if (Quality < 0)
                            Quality = 100;
                        result = YuvToJpeg(temp, pixfmt->width, pixfmt->height, Size, Quality);
                        free(temp);
                    }
                }
                else {
                    // convert to PNM:
                    char buf[32];
                    snprintf(buf, sizeof(buf), "P6\n%d\n%d\n255\n",
                             pixfmt->width, pixfmt->height);
                    int l = strlen(buf);
                    Size = l + pixfmt->width * 3 * pixfmt->height;
                    result = (uint8_t *) malloc(Size);
                    if (result)
                    {
                        memcpy(result, buf, l);
                        uint8_t * destData = result + l;
                        uint8_t * srcData = buffer + sizeof(struct v4l2_pix_format);
                        int numPixels = pixfmt->width * pixfmt->height;
                        while (numPixels > 0)
                        {
                            int cb = srcData[0] - 128;
                            int y1 = srcData[1];
                            int cr = srcData[2] - 128;
                            int y2 = srcData[3];
                            int r;
                            int g;
                            int b;

                            r = y1 + (int) (1.402f * cr);
                            g = y1 - (int) (0.344f * cb + 0.714f * cr);
                            b = y1 + (int) (1.772f * cb);
                            destData[0] = r > 255 ? 255 : r < 0 ? 0 : r;
                            destData[1] = g > 255 ? 255 : g < 0 ? 0 : g;
                            destData[2] = b > 255 ? 255 : b < 0 ? 0 : b;
                            r = y2 + (int) (1.402f * cr);
                            g = y2 - (int) (0.344f * cb + 0.714f * cr);
                            b = y2 + (int) (1.772f * cb);
                            destData[3] = r > 255 ? 255 : r < 0 ? 0 : r;
                            destData[4] = g > 255 ? 255 : g < 0 ? 0 : g;
                            destData[5] = b > 255 ? 255 : b < 0 ? 0 : b;

                            srcData += 4;
                            destData += 6;
                            numPixels -= 2;
                        }
                    }
                }
            }
        }
        free(buffer);
    }

    close(fd);

    return result;
}

void cDvbHdFfDevice::SetVideoDisplayFormat(eVideoDisplayFormat VideoDisplayFormat)
{
  //TODO???
  cDevice::SetVideoDisplayFormat(VideoDisplayFormat);
}

void cDvbHdFfDevice::SetVideoFormat(bool VideoFormat16_9)
{
  HdffVideoFormat_t videoFormat;
  videoFormat.AutomaticEnabled = true;
  videoFormat.AfdEnabled = false;
  videoFormat.TvFormat = (HdffTvFormat_t) gHdffSetup.TvFormat;
  videoFormat.VideoConversion = (HdffVideoConversion_t) gHdffSetup.VideoConversion;
  mHdffCmdIf->CmdAvSetVideoFormat(0, &videoFormat);
}

eVideoSystem cDvbHdFfDevice::GetVideoSystem(void)
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

void cDvbHdFfDevice::GetVideoSize(int &Width, int &Height, double &VideoAspect)
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

void cDvbHdFfDevice::GetOsdSize(int &Width, int &Height, double &PixelAspect)
{
  gHdffSetup.GetOsdSize(Width, Height, PixelAspect);
}

bool cDvbHdFfDevice::SetPid(cPidHandle *Handle, int Type, bool On)
{
    //printf("SetPid Type %d, On %d, PID %5d, streamtype %d, handle %d, used %d\n", Type, On, Handle->pid, Handle->streamType, Handle->handle, Handle->used);
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
            if (Type == ptPcr)
                mHdffCmdIf->CmdAvSetPcrPid(0, Handle->pid);
            else if (Type == ptVideo) {
                if (Handle->streamType == 0x1B)
                    mHdffCmdIf->CmdAvSetVideoPid(0, Handle->pid, HDFF_VIDEO_STREAM_H264);
                else
                    mHdffCmdIf->CmdAvSetVideoPid(0, Handle->pid, HDFF_VIDEO_STREAM_MPEG2);
            }
            else if (Type == ptAudio) {
                if (Handle->streamType == 0x03)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_MPEG1);
                else if (Handle->streamType == 0x04)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_MPEG2);
                else if (Handle->streamType == SI::AC3DescriptorTag)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_AC3);
                else if (Handle->streamType == SI::EnhancedAC3DescriptorTag)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_EAC3);
                else if (Handle->streamType == 0x0F)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_AAC);
                else if (Handle->streamType == 0x11)
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_HE_AAC);
                else
                    mHdffCmdIf->CmdAvSetAudioPid(0, Handle->pid, HDFF_AUDIO_STREAM_MPEG1);
            }
            if (!(Type <= ptDolby && Handle->used <= 1)) {
                pesFilterParams.pid     = Handle->pid;
                pesFilterParams.input   = DMX_IN_FRONTEND;
                pesFilterParams.output  = DMX_OUT_TS_TAP;
                pesFilterParams.pes_type= DMX_PES_OTHER;
                pesFilterParams.flags   = DMX_IMMEDIATE_START;
                if (ioctl(Handle->handle, DMX_SET_PES_FILTER, &pesFilterParams) < 0) {
                    LOG_ERROR;
                    return false;
                }
            }
        }
        else if (!Handle->used) {
            CHECK(ioctl(Handle->handle, DMX_STOP));
            if (Type == ptPcr)
                mHdffCmdIf->CmdAvSetPcrPid(0, 0);
            else if (Type == ptVideo)
                mHdffCmdIf->CmdAvSetVideoPid(0, 0, HDFF_VIDEO_STREAM_MPEG1);
            else if (Type == ptAudio)
                mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_MPEG1);
            else if (Type == ptDolby)
                mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_AC3);
            //TODO missing setting to 0x1FFF??? see cDvbDevice::SetPid()
            close(Handle->handle);
            Handle->handle = -1;
        }
    }
    return true;
}

void cDvbHdFfDevice::TurnOffLiveMode(bool LiveView)
{
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

bool cDvbHdFfDevice::SetChannelDevice(const cChannel *Channel, bool LiveView)
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

  //printf("SetChannelDevice Transfer %d, Live %d\n", StartTransferMode, LiveView);

  bool TurnOnLivePIDs = !StartTransferMode && LiveView;

  // Turn off live PIDs if necessary:

  if (TurnOffLivePIDs)
     TurnOffLiveMode(LiveView);

  // Set the tuner:

  if (!cDvbDevice::SetChannelDevice(Channel, LiveView))
     return false;

  // PID settings:

  if (TurnOnLivePIDs) {
     if (!(AddPid(Channel->Ppid(), ptPcr) && AddPid(vpid, ptVideo, Channel->Vtype()) && AddPid(apid ? apid : dpid, ptAudio, apid ? 0 : Channel->Dtype(0)))) {
        esyslog("ERROR: failed to set PIDs for channel %d on device %d", Channel->Number(), CardIndex() + 1);
        return false;
        }
     }
  else if (StartTransferMode)
     cControl::Launch(new cTransferControl(this, Channel));

  return true;
}

int cDvbHdFfDevice::GetAudioChannelDevice(void)
{
  return audioChannel;
}

void cDvbHdFfDevice::SetAudioChannelDevice(int AudioChannel)
{
  mHdffCmdIf->CmdAvSetAudioChannel(AudioChannel);
  audioChannel = AudioChannel;
}

void cDvbHdFfDevice::SetVolumeDevice(int Volume)
{
  mHdffCmdIf->CmdMuxSetVolume(Volume * 100 / 255);
}

void cDvbHdFfDevice::SetDigitalAudioDevice(bool On)
{
  // not needed
}

void cDvbHdFfDevice::SetAudioTrackDevice(eTrackType Type)
{
    //printf("SetAudioTrackDevice %d\n", Type);
    const tTrackId *TrackId = GetTrack(Type);
    if (TrackId && TrackId->id) {
        int streamType = 0;
        cChannel * channel = Channels.GetByNumber(CurrentChannel());
        if (channel) {
            if (IS_AUDIO_TRACK(Type))
                streamType = channel->Atype(Type - ttAudioFirst);
            else if (IS_DOLBY_TRACK(Type))
                streamType = channel->Dtype(Type - ttDolbyFirst);
        }
        //printf("SetAudioTrackDevice new %d %d, current %d\n", TrackId->id, streamType, pidHandles[ptAudio].pid);
        if (pidHandles[ptAudio].pid && pidHandles[ptAudio].pid != TrackId->id) {
            DetachAll(pidHandles[ptAudio].pid);
            if (CamSlot())
                CamSlot()->SetPid(pidHandles[ptAudio].pid, false);
            pidHandles[ptAudio].pid = TrackId->id;
            pidHandles[ptAudio].streamType = streamType;
            SetPid(&pidHandles[ptAudio], ptAudio, true);
            if (CamSlot()) {
                CamSlot()->SetPid(pidHandles[ptAudio].pid, true);
                CamSlot()->StartDecrypting();
            }
        }
    }
}

bool cDvbHdFfDevice::CanReplay(void) const
{
  return cDevice::CanReplay();
}

bool cDvbHdFfDevice::SetPlayMode(ePlayMode PlayMode)
{
    if (PlayMode == pmNone) {
        if (fd_video == -1)
            fd_video = DvbOpen(DEV_DVB_VIDEO,  adapter, frontend, O_RDWR | O_NONBLOCK);
        if (fd_audio == -1)
            fd_audio = DvbOpen(DEV_DVB_AUDIO,  adapter, frontend, O_RDWR | O_NONBLOCK);

        mHdffCmdIf->CmdAvSetVideoSpeed(0, 100);
        mHdffCmdIf->CmdAvSetAudioSpeed(0, 100);

        mHdffCmdIf->CmdAvEnableVideoAfterStop(0, false);
        mHdffCmdIf->CmdAvSetPcrPid(0, 0);
        mHdffCmdIf->CmdAvSetVideoPid(0, 0, HDFF_VIDEO_STREAM_MPEG1);
        mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_MPEG1);

        ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_DEMUX);
        mHdffCmdIf->CmdAvSetDecoderInput(0, 0);
        mHdffCmdIf->CmdAvEnableSync(0, true);
        mHdffCmdIf->CmdAvSetPlayMode(0, true);
        mHdffCmdIf->CmdAvMuteAudio(0, false);
    }
    else {
        if (playMode == pmNone)
            TurnOffLiveMode(true);

        if (PlayMode == pmExtern_THIS_SHOULD_BE_AVOIDED)
        {
            close(fd_video);
            fd_video = -1;
            close(fd_audio);
            fd_audio = -1;
        }
        else
        {
            mHdffCmdIf->CmdAvSetPlayMode(1, Transferring() || (cTransferControl::ReceiverDevice() == this));
            mHdffCmdIf->CmdAvSetStc(0, 100000);
            mHdffCmdIf->CmdAvEnableSync(0, false);
            mHdffCmdIf->CmdAvEnableVideoAfterStop(0, true);

            playVideoPid = -1;
            playAudioPid = -1;
            audioCounter = 0;
            videoCounter = 0;
            freezed = false;
            trickMode = false;
            isPlayingVideo = false;

            mHdffCmdIf->CmdAvSetDecoderInput(0, 2);
            ioctl(fd_video, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY);
        }
    }
    playMode = PlayMode;
    return true;
}

int64_t cDvbHdFfDevice::GetSTC(void)
{
    if (isPlayingVideo)
    {
        if (fd_video >= 0) {
            uint64_t pts;
            if (ioctl(fd_video, VIDEO_GET_PTS, &pts) == -1) {
                esyslog("ERROR: pts %d: %m", CardIndex() + 1);
                return -1;
            }
            //printf("video PTS %lld\n", pts);
            return pts;
        }
    }
    else
    {
        if (fd_audio >= 0) {
            uint64_t pts;
            if (ioctl(fd_audio, AUDIO_GET_PTS, &pts) == -1) {
                esyslog("ERROR: pts %d: %m", CardIndex() + 1);
                return -1;
            }
            //printf("audio PTS %lld\n", pts);
            return pts;
        }
    }
    return -1;
}

cRect cDvbHdFfDevice::CanScaleVideo(const cRect &Rect, int Alignment)
{
    return Rect;
}

void cDvbHdFfDevice::ScaleVideo(const cRect &Rect)
{
    if (Rect == cRect::Null)
    {
        mHdffCmdIf->CmdAvSetVideoWindow(0, false, 0, 0, 0, 0);
    }
    else
    {
        //printf("ScaleVideo: Rect = %d %d %d %d\n", Rect.X(), Rect.Y(), Rect.Width(), Rect.Height());

        int osdWidth;
        int osdHeight;
        double osdPixelAspect;

        GetOsdSize(osdWidth, osdHeight, osdPixelAspect);
        //printf("ScaleVideo: OsdSize = %d %d %g\n", osdWidth, osdHeight, osdPixelAspect);

        // Convert the video window coordinates in 1/10 percent of the display
        // resolution.
        int x = (Rect.X() * 1000 + osdWidth / 2) / osdWidth;
        int y = (Rect.Y() * 1000 + osdHeight / 2) / osdHeight;
        int w = (Rect.Width() * 1000 + osdWidth / 2) / osdWidth;
        int h = (Rect.Height() * 1000 + osdHeight / 2) / osdHeight;
        //printf("ScaleVideo: Win1 = %d %d %d %d\n", x, y, w, h);

        // fix aspect ratio, reposition video
        if (w > h) {
            x += (w - h) / 2;
            w = h;
        }
        else if (w < h) {
            y += (h - w) / 2;
            h = w;
        }

        //printf("ScaleVideo: Win2 = %d %d %d %d\n", x, y, w, h);
        mHdffCmdIf->CmdAvSetVideoWindow(0, true, x, y, w, h);
    }
}

void cDvbHdFfDevice::TrickSpeed(int Speed)
{
  freezed = false;
  mHdffCmdIf->CmdAvEnableSync(0, false);
  mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_MPEG1);
  playAudioPid = -1;
  if (Speed > 0)
     mHdffCmdIf->CmdAvSetVideoSpeed(0, 100 / Speed);
  trickMode = true;
}

void cDvbHdFfDevice::Clear(void)
{
  CHECK(ioctl(fd_video, VIDEO_CLEAR_BUFFER));
  mHdffCmdIf->CmdAvSetVideoPid(0, 0, HDFF_VIDEO_STREAM_MPEG1);
  mHdffCmdIf->CmdAvSetAudioPid(0, 0, HDFF_AUDIO_STREAM_MPEG1);
  playVideoPid = -1;
  playAudioPid = -1;
  cDevice::Clear();
}

void cDvbHdFfDevice::Play(void)
{
    freezed = false;
    trickMode = false;
    if (isPlayingVideo)
        mHdffCmdIf->CmdAvEnableSync(0, true);
    mHdffCmdIf->CmdAvSetVideoSpeed(0, 100);
    mHdffCmdIf->CmdAvSetAudioSpeed(0, 100);
    mHdffCmdIf->CmdAvMuteAudio(0, false);
    cDevice::Play();
}

void cDvbHdFfDevice::Freeze(void)
{
  freezed = true;
  mHdffCmdIf->CmdAvSetVideoSpeed(0, 0);
  mHdffCmdIf->CmdAvSetAudioSpeed(0, 0);
  cDevice::Freeze();
}

void cDvbHdFfDevice::Mute(void)
{
    mHdffCmdIf->CmdAvMuteAudio(0, true);
    cDevice::Mute();
}

static HdffVideoStreamType_t MapVideoStreamTypes(int Vtype)
{
  switch (Vtype) {
    case 0x01: return HDFF_VIDEO_STREAM_MPEG1;
    case 0x02: return HDFF_VIDEO_STREAM_MPEG2;
    case 0x1B: return HDFF_VIDEO_STREAM_H264;
    default: return HDFF_VIDEO_STREAM_MPEG2; // fallback to MPEG2
    }
}

void cDvbHdFfDevice::StillPicture(const uchar *Data, int Length)
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
     mHdffCmdIf->CmdAvShowStillImage(0, (uint8_t *)buf, blen, MapVideoStreamTypes(PatPmtParser()->Vtype()));
     free(buf);
     }
  else {
     // non-PES data
     mHdffCmdIf->CmdAvShowStillImage(0, Data, Length, MapVideoStreamTypes(PatPmtParser()->Vtype()));
     }
}

bool cDvbHdFfDevice::Poll(cPoller &Poller, int TimeoutMs)
{
  Poller.Add(fd_video, true);
  return Poller.Poll(TimeoutMs);
}

bool cDvbHdFfDevice::Flush(int TimeoutMs)
{
  //TODO actually this function should wait until all buffered data has been processed by the card, but how?
  return true;
}

void cDvbHdFfDevice::BuildTsPacket(uint8_t * TsBuffer, bool PusiSet, uint16_t Pid, uint8_t Counter, const uint8_t * Data, uint32_t Length)
{
    TsBuffer[0] = 0x47;
    TsBuffer[1] = PusiSet ? 0x40 : 0x00;
    TsBuffer[1] |= Pid >> 8;
    TsBuffer[2] = Pid & 0xFF;
    if (Length >= 184)
    {
        TsBuffer[3] = 0x10 | Counter;
        memcpy(TsBuffer + 4, Data, 184);
    }
    else
    {
        uint8_t adaptationLength;

        TsBuffer[3] = 0x30 | Counter;
        adaptationLength = 183 - Length;
        TsBuffer[4] = adaptationLength;
        if (adaptationLength > 0)
        {
            TsBuffer[5] = 0x00;
            memset(TsBuffer + 6, 0xFF, adaptationLength - 1);
        }
        memcpy(TsBuffer + 5 + adaptationLength, Data, Length);
    }
}

uint32_t cDvbHdFfDevice::PesToTs(uint8_t * TsBuffer, uint16_t Pid, uint8_t & Counter, const uint8_t * Data, uint32_t Length)
{
    uint32_t tsOffset;
    uint32_t i;

    tsOffset = 0;
    i = 0;
    while (Length > 0)
    {
        BuildTsPacket(TsBuffer + tsOffset, i == 0, Pid, Counter, Data + i * 184, Length);
        if (Length >= 184)
            Length -= 184;
        else
            Length = 0;
        Counter = (Counter + 1) & 15;
        tsOffset += 188;
        i++;
    }
    return tsOffset;
}

int cDvbHdFfDevice::PlayVideo(const uchar *Data, int Length)
{
    if (freezed)
        return -1;
    if (!isPlayingVideo)
    {
        mHdffCmdIf->CmdAvEnableSync(0, true);
        isPlayingVideo = true;
    }
    //TODO: support greater Length
    uint8_t tsBuffer[188 * 16];
    uint32_t tsLength;
    int pid = 100;

    tsLength = PesToTs(tsBuffer, pid, videoCounter, Data, Length);

    if (pid != playVideoPid) {
        playVideoPid = pid;
        mHdffCmdIf->CmdAvSetVideoPid(0, playVideoPid, HDFF_VIDEO_STREAM_MPEG2, true);
    }
    if (WriteAllOrNothing(fd_video, tsBuffer, tsLength, 1000, 10) <= 0)
        Length = 0;
    return Length;
}

int cDvbHdFfDevice::PlayAudio(const uchar *Data, int Length, uchar Id)
{
    if (freezed)
        return -1;
    uint8_t streamId;
    uint8_t tsBuffer[188 * 16];
    uint32_t tsLength;
    HdffAudioStreamType_t streamType = HDFF_AUDIO_STREAM_MPEG1;
    HdffAvContainerType_t containerType = HDFF_AV_CONTAINER_PES;
    int pid;

    streamId = Data[3];
    if (streamId >= 0xC0 && streamId <= 0xDF)
    {
        streamType = HDFF_AUDIO_STREAM_MPEG1;
    }
    else if (streamId == 0xBD)
    {
        const uint8_t * payload = Data + 9 + Data[8];
        if ((payload[0] & 0xF8) == 0xA0)
        {
            containerType = HDFF_AV_CONTAINER_PES_DVD;
            streamType = HDFF_AUDIO_STREAM_PCM;
        }
        else if ((payload[0] & 0xF8) == 0x88)
        {
            containerType = HDFF_AV_CONTAINER_PES_DVD;
            streamType = HDFF_AUDIO_STREAM_DTS;
        }
        else if ((payload[0] & 0xF8) == 0x80)
        {
            containerType = HDFF_AV_CONTAINER_PES_DVD;
            streamType = HDFF_AUDIO_STREAM_AC3;
        }
        else
        {
            streamType = HDFF_AUDIO_STREAM_AC3;
        }
    }
    pid = 200 + (int) streamType;
    tsLength = PesToTs(tsBuffer, pid, audioCounter, Data, Length);

    if (pid != playAudioPid) {
        playAudioPid = pid;
        mHdffCmdIf->CmdAvSetAudioPid(0, playAudioPid, streamType, containerType);
    }
    if (WriteAllOrNothing(fd_video, tsBuffer, tsLength, 1000, 10) <= 0)
        Length = 0;
    return Length;
}

int cDvbHdFfDevice::PlayTsVideo(const uchar *Data, int Length)
{
    if (freezed)
        return -1;
    if (!isPlayingVideo)
    {
        mHdffCmdIf->CmdAvEnableSync(0, true);
        isPlayingVideo = true;
    }

    int pid = TsPid(Data);
    if (pid != playVideoPid) {
        PatPmtParser();
        if (pid == PatPmtParser()->Vpid()) {
            playVideoPid = pid;
            mHdffCmdIf->CmdAvSetVideoPid(0, playVideoPid, MapVideoStreamTypes(PatPmtParser()->Vtype()), true);
        }
    }
    return WriteAllOrNothing(fd_video, Data, Length, 1000, 10);
}

static HdffAudioStreamType_t MapAudioStreamTypes(int Atype)
{
  switch (Atype) {
    case 0x03: return HDFF_AUDIO_STREAM_MPEG1;
    case 0x04: return HDFF_AUDIO_STREAM_MPEG2;
    case SI::AC3DescriptorTag: return HDFF_AUDIO_STREAM_AC3;
    case SI::EnhancedAC3DescriptorTag: return HDFF_AUDIO_STREAM_EAC3;
    case 0x0F: return HDFF_AUDIO_STREAM_AAC;
    case 0x11: return HDFF_AUDIO_STREAM_HE_AAC;
    default: return HDFF_AUDIO_STREAM_MPEG1;
    }
}

int cDvbHdFfDevice::PlayTsAudio(const uchar *Data, int Length)
{
  if (freezed)
    return -1;
  int pid = TsPid(Data);
  if (pid != playAudioPid) {
     playAudioPid = pid;
     int AudioStreamType = -1;
     for (int i = 0; PatPmtParser()->Apid(i); i++) {
         if (playAudioPid == PatPmtParser()->Apid(i)) {
            AudioStreamType = PatPmtParser()->Atype(i);
            break;
            }
         }
     if (AudioStreamType < 0) {
        for (int i = 0; PatPmtParser()->Dpid(i); i++) {
            if (playAudioPid == PatPmtParser()->Dpid(i)) {
               AudioStreamType = PatPmtParser()->Dtype(i);
               break;
               }
            }
        }
     mHdffCmdIf->CmdAvSetAudioPid(0, playAudioPid, MapAudioStreamTypes(AudioStreamType));
     }
  return WriteAllOrNothing(fd_video, Data, Length, 1000, 10);
}

HDFF::cHdffCmdIf *cDvbHdFfDevice::GetHdffCmdHandler(void)
{
  //TODO why not just keep a pointer?
  if (devHdffOffset >= 0) {
     cDvbHdFfDevice *device = (cDvbHdFfDevice *)GetDevice(devHdffOffset);
     if (device)
        return device->mHdffCmdIf;
     }
  return NULL;
}

// --- cDvbHdFfDeviceProbe ---------------------------------------------------

bool cDvbHdFfDeviceProbe::Probe(int Adapter, int Frontend)
{
  static uint32_t SubsystemIds[] = {
    0x13C23009, // Technotrend S2-6400 HDFF development samples
    0x13C2300A, // Technotrend S2-6400 HDFF production version
    0x00000000
    };
  cString FileName;
  cReadLine ReadLine;
  FILE *f = NULL;
  uint32_t SubsystemId = 0;
  FileName = cString::sprintf("/sys/class/dvb/dvb%d.frontend%d/device/subsystem_vendor", Adapter, Frontend);
  if ((f = fopen(FileName, "r")) != NULL) {
     if (char *s = ReadLine.Read(f))
        SubsystemId = strtoul(s, NULL, 0) << 16;
     fclose(f);
     }
  FileName = cString::sprintf("/sys/class/dvb/dvb%d.frontend%d/device/subsystem_device", Adapter, Frontend);
  if ((f = fopen(FileName, "r")) != NULL) {
     if (char *s = ReadLine.Read(f))
        SubsystemId |= strtoul(s, NULL, 0);
     fclose(f);
     }
  for (uint32_t *sid = SubsystemIds; *sid; sid++) {
      if (*sid == SubsystemId) {
         FileName = cString::sprintf("/dev/dvb/adapter%d/osd0", Adapter);
         int fd = open(FileName, O_RDWR);
         if (fd != -1) { //TODO treat the second path of the S2-6400 as a budget device
            close(fd);
            dsyslog("creating cDvbHdFfDevice");
            new cDvbHdFfDevice(Adapter, Frontend);
            return true;
            }
         }
      }
  return false;
}


// --- YuvToJpeg -------------------------------------------------------------

#include <jpeglib.h>

#define JPEGCOMPRESSMEM 4000000

struct tJpegCompressData {
  int size;
  uchar *mem;
  };

static void JpegCompressInitDestination(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     cinfo->dest->free_in_buffer = jcd->size = JPEGCOMPRESSMEM;
     cinfo->dest->next_output_byte = jcd->mem = MALLOC(uchar, jcd->size);
     }
}

static boolean JpegCompressEmptyOutputBuffer(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     int Used = jcd->size;
     int NewSize = jcd->size + JPEGCOMPRESSMEM;
     if (uchar *NewBuffer = (uchar *)realloc(jcd->mem, NewSize)) {
        jcd->size = NewSize;
        jcd->mem = NewBuffer;
        }
     else {
        esyslog("ERROR: out of memory");
        return false;
        }
     if (jcd->mem) {
        cinfo->dest->next_output_byte = jcd->mem + Used;
        cinfo->dest->free_in_buffer = jcd->size - Used;
        return true;
        }
     }
  return false;
}

static void JpegCompressTermDestination(j_compress_ptr cinfo)
{
  tJpegCompressData *jcd = (tJpegCompressData *)cinfo->client_data;
  if (jcd) {
     int Used = cinfo->dest->next_output_byte - jcd->mem;
     if (Used < jcd->size) {
        if (uchar *NewBuffer = (uchar *)realloc(jcd->mem, Used)) {
           jcd->size = Used;
           jcd->mem = NewBuffer;
           }
        else
           esyslog("ERROR: out of memory");
        }
     }
}

static uchar *YuvToJpeg(uchar *Mem, int Width, int Height, int &Size, int Quality)
{
  if (Quality < 0)
     Quality = 0;
  else if (Quality > 100)
     Quality = 100;

  jpeg_destination_mgr jdm;

  jdm.init_destination = JpegCompressInitDestination;
  jdm.empty_output_buffer = JpegCompressEmptyOutputBuffer;
  jdm.term_destination = JpegCompressTermDestination;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  cinfo.dest = &jdm;
  tJpegCompressData jcd;
  cinfo.client_data = &jcd;
  cinfo.image_width = Width;
  cinfo.image_height = Height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_YCbCr;

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, Quality, true);
  jpeg_start_compress(&cinfo, true);

  int rs = Width * 3;
  JSAMPROW rp[Height];
  for (int k = 0; k < Height; k++)
      rp[k] = &Mem[rs * k];
  jpeg_write_scanlines(&cinfo, rp, Height);
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  Size = jcd.size;
  return jcd.mem;
}
