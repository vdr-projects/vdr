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

#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

#include "hdffcmd.h"
#include "hdffcmd_base.h"
#include "hdffcmd_defs.h"


int HdffCmdAvSetPlayMode(int OsdDevice, uint8_t PlayMode, int Realtime)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_PLAY_MODE);
    BitBuffer_SetBits(&cmdBuf, 1, Realtime ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 7, PlayMode);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetVideoPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid,
                         HdffVideoStreamType_t StreamType)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_VIDEO_PID);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, StreamType);
    BitBuffer_SetBits(&cmdBuf, 3, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 13, Pid);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetAudioPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid,
                         HdffAudioStreamType_t StreamType,
                         HdffAvContainerType_t ContainerType)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_AUDIO_PID);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, StreamType);
    BitBuffer_SetBits(&cmdBuf, 2, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 1, ContainerType);
    BitBuffer_SetBits(&cmdBuf, 13, Pid);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetPcrPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_PCR_PID);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 3, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 13, Pid);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetTeletextPid(int OsdDevice, uint8_t DecoderIndex, uint16_t Pid)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_TELETEXT_PID);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 3, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 13, Pid);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetVideoWindow(int OsdDevice, uint8_t DecoderIndex, int Enable,
                            uint16_t X, uint16_t Y, uint16_t Width,
                            uint16_t Height)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_VIDEO_WINDOW);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 3, 0); // reserved
    if (Enable)
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    else
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvShowStillImage(int OsdDevice, uint8_t DecoderIndex,
                            const uint8_t * StillImage, int Size,
                            HdffVideoStreamType_t StreamType)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;
    int err;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = StillImage;
    osd_data.data_length = Size;
    err = ioctl(OsdDevice, OSD_RAW_DATA, &osd_data);
    if (err != 0)
        return err;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SHOW_STILL_IMAGE);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, StreamType);
    BitBuffer_SetBits(&cmdBuf, 16, osd_data.data_handle);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetDecoderInput(int OsdDevice, uint8_t DecoderIndex,
                             uint8_t DemultiplexerIndex)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_DECODER_INPUT);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, DemultiplexerIndex);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetDemultiplexerInput(int OsdDevice, uint8_t DemultiplexerIndex,
                                   uint8_t TsInputIndex)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_DEMULTIPLEXER_INPUT);
    BitBuffer_SetBits(&cmdBuf, 4, DemultiplexerIndex);
    BitBuffer_SetBits(&cmdBuf, 4, TsInputIndex);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetVideoFormat(int OsdDevice, uint8_t DecoderIndex,
                            const HdffVideoFormat_t * VideoFormat)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_VIDEO_FORMAT);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 1, VideoFormat->AutomaticEnabled ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 1, VideoFormat->AfdEnabled ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 2, VideoFormat->TvFormat);
    BitBuffer_SetBits(&cmdBuf, 8, VideoFormat->VideoConversion);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetVideoOutputMode(int OsdDevice, uint8_t DecoderIndex,
                                HdffVideoOutputMode_t OutputMode)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_VIDEO_OUTPUT_MODE);
    BitBuffer_SetBits(&cmdBuf, 8, OutputMode);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetStc(int OsdDevice, uint8_t DecoderIndex, uint64_t Stc)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_STC);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 3, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 1, (uint32_t) (Stc >> 32));
    BitBuffer_SetBits(&cmdBuf, 32, (uint32_t) Stc);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvFlushBuffer(int OsdDevice, uint8_t DecoderIndex, int FlushAudio,
                         int FlushVideo)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_FLUSH_BUFFER);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    if (FlushAudio)
    {
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    }
    else
    {
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    }
    if (FlushVideo)
    {
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    }
    else
    {
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvEnableSync(int OsdDevice, uint8_t DecoderIndex, int SyncAudio,
                        int SyncVideo)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_ENABLE_SYNC);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 1, SyncAudio ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 1, SyncVideo ? 1 : 0);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetVideoSpeed(int OsdDevice, uint8_t DecoderIndex, int32_t Speed)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_VIDEO_SPEED);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, 0);
    BitBuffer_SetBits(&cmdBuf, 32, Speed);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetAudioSpeed(int OsdDevice, uint8_t DecoderIndex, int32_t Speed)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_AUDIO_SPEED);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 4, 0);
    BitBuffer_SetBits(&cmdBuf, 32, Speed);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvEnableVideoAfterStop(int OsdDevice, uint8_t DecoderIndex,
                                  int EnableVideoAfterStop)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_ENABLE_VIDEO_AFTER_STOP);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 1, EnableVideoAfterStop ? 1 : 0);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetAudioDelay(int OsdDevice, int16_t Delay)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_AUDIO_DELAY);
    BitBuffer_SetBits(&cmdBuf, 16, Delay);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetAudioDownmix(int OsdDevice, HdffAudioDownmixMode_t DownmixMode)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_AUDIO_DOWNMIX);
    BitBuffer_SetBits(&cmdBuf, 8, DownmixMode);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetAudioChannel(int OsdDevice, uint8_t AudioChannel)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_AUDIO_CHANNEL);
    BitBuffer_SetBits(&cmdBuf, 8, AudioChannel);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvSetSyncShift(int OsdDevice, int16_t SyncShift)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_SET_OPTIONS);
    BitBuffer_SetBits(&cmdBuf, 1, 1);
    BitBuffer_SetBits(&cmdBuf, 31, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 16, SyncShift);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvMuteAudio(int OsdDevice, uint8_t DecoderIndex, int Mute)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_MUTE_AUDIO);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 1, Mute ? 1 : 0);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdAvMuteVideo(int OsdDevice, uint8_t DecoderIndex, int Mute)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND,
                       HDFF_MSG_GROUP_AV_DECODER,
                       HDFF_MSG_AV_MUTE_VIDEO);
    BitBuffer_SetBits(&cmdBuf, 4, DecoderIndex);
    BitBuffer_SetBits(&cmdBuf, 1, Mute ? 1 : 0);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}
