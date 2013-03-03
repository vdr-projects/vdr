/**********************************************************************
 *
 * HDFF firmware command interface library
 *
 * Copyright (C) 2011  Andreas Regel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *********************************************************************/

#ifndef HDFFCMD_AV_H
#define HDFFCMD_AV_H

typedef enum HdffAvContainerType_t
{
    HDFF_AV_CONTAINER_PES,
    HDFF_AV_CONTAINER_PES_DVD
} HdffAvContainerType_t;

typedef enum HdffAudioStreamType_t
{
    HDFF_AUDIO_STREAM_INVALID = -1,
    HDFF_AUDIO_STREAM_MPEG1 = 0,
    HDFF_AUDIO_STREAM_MPEG2,
    HDFF_AUDIO_STREAM_AC3,
    HDFF_AUDIO_STREAM_AAC,
    HDFF_AUDIO_STREAM_HE_AAC,
    HDFF_AUDIO_STREAM_PCM,
    HDFF_AUDIO_STREAM_EAC3,
    HDFF_AUDIO_STREAM_DTS
} HdffAudioStreamType_t;

typedef enum HdffVideoStreamType_t
{
    HDFF_VIDEO_STREAM_INVALID = -1,
    HDFF_VIDEO_STREAM_MPEG1 = 0,
    HDFF_VIDEO_STREAM_MPEG2,
    HDFF_VIDEO_STREAM_H264,
    HDFF_VIDEO_STREAM_MPEG4_ASP,
    HDFF_VIDEO_STREAM_VC1
} HdffVideoStreamType_t;

typedef enum HdffTvFormat_t
{
    HDFF_TV_FORMAT_4_BY_3,
    HDFF_TV_FORMAT_16_BY_9
} HdffTvFormat_t;

typedef enum HdffVideoConversion_t
{
    HDFF_VIDEO_CONVERSION_AUTOMATIC,
    HDFF_VIDEO_CONVERSION_LETTERBOX_16_BY_9,
    HDFF_VIDEO_CONVERSION_LETTERBOX_14_BY_9,
    HDFF_VIDEO_CONVERSION_PILLARBOX,
    HDFF_VIDEO_CONVERSION_CENTRE_CUT_OUT,
    HDFF_VIDEO_CONVERSION_ALWAYS_16_BY_9,
    HDFF_VIDEO_CONVERSION_ZOOM_16_BY_9
} HdffVideoConversion_t;

typedef struct HdffVideoFormat_t
{
    int AutomaticEnabled;
    int AfdEnabled;
    HdffTvFormat_t TvFormat;
    HdffVideoConversion_t VideoConversion;
} HdffVideoFormat_t;

typedef enum HdffVideoOutputMode_t
{
    HDFF_VIDEO_OUTPUT_CLONE,
    HDFF_VIDEO_OUTPUT_HD_ONLY
} HdffVideoOutputMode_t;

typedef enum HdffAudioDownmixMode_t
{
    HDFF_AUDIO_DOWNMIX_OFF,
    HDFF_AUDIO_DOWNMIX_ANALOG,
    HDFF_AUDIO_DOWNMIX_ALWAYS,
    HDFF_AUDIO_DOWNMIX_AUTOMATIC,
    HDFF_AUDIO_DOWNMIX_HDMI_ONLY
} HdffAudioDownmixMode_t;


int HdffCmdAvSetPlayMode(int OsdDevice, uint8_t PlayMode, int Realtime);

int HdffCmdAvSetVideoPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid,
                         HdffVideoStreamType_t StreamType);

int HdffCmdAvSetAudioPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid,
                         HdffAudioStreamType_t StreamType,
                         HdffAvContainerType_t ContainerType);

int HdffCmdAvSetPcrPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid);

int HdffCmdAvSetTeletextPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid);

int HdffCmdAvSetVideoWindow(int OsdDevice, uint8_t DecoderIndex, int Enable,
                            uint16_t X, uint16_t Y, uint16_t Width,
                            uint16_t Height);

int HdffCmdAvShowStillImage(int OsdDevice, uint8_t DecoderIndex,
                            const uint8_t * StillImage, int Size,
                            HdffVideoStreamType_t StreamType);

int HdffCmdAvSetDecoderInput(int OsdDevice, uint8_t DecoderIndex,
                             uint8_t DemultiplexerIndex);

int HdffCmdAvSetDemultiplexerInput(int OsdDevice, uint8_t DemultiplexerIndex,
                                   uint8_t TsInputIndex);

int HdffCmdAvSetVideoFormat(int OsdDevice, uint8_t DecoderIndex,
                            const HdffVideoFormat_t * VideoFormat);

int HdffCmdAvSetVideoOutputMode(int OsdDevice, uint8_t DecoderIndex,
                                HdffVideoOutputMode_t OutputMode);

int HdffCmdAvSetStc(int OsdDevice, uint8_t DecoderIndex, uint64_t Stc);

int HdffCmdAvFlushBuffer(int OsdDevice, uint8_t DecoderIndex, int FlushAudio,
                         int FlushVideo);

int HdffCmdAvEnableSync(int OsdDevice, uint8_t DecoderIndex, int SyncAudio,
                        int SyncVideo);

int HdffCmdAvSetVideoSpeed(int OsdDevice, uint8_t DecoderIndex, int32_t Speed);

int HdffCmdAvSetAudioSpeed(int OsdDevice, uint8_t DecoderIndex, int32_t Speed);

int HdffCmdAvEnableVideoAfterStop(int OsdDevice, uint8_t DecoderIndex,
                                  int EnableVideoAfterStop);

int HdffCmdAvSetAudioDelay(int OsdDevice, int16_t Delay);

int HdffCmdAvSetAudioDownmix(int OsdDevice,
                             HdffAudioDownmixMode_t DownmixMode);

int HdffCmdAvSetAudioChannel(int OsdDevice, uint8_t AudioChannel);

int HdffCmdAvSetSyncShift(int OsdDevice, int16_t SyncShift);

int HdffCmdAvMuteAudio(int OsdDevice, uint8_t DecoderIndex, int Mute);

int HdffCmdAvMuteVideo(int OsdDevice, uint8_t DecoderIndex, int Mute);


#endif /* HDFFCMD_AV_H */
