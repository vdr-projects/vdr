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


int HdffCmdMuxSetVideoOut(int OsdDevice, HdffVideoOut_t VideoOut)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_AV_MUX,
                       HDFF_MSG_MUX_SET_VIDEO_OUT);
    BitBuffer_SetBits(&cmdBuf, 4, VideoOut);
    BitBuffer_SetBits(&cmdBuf, 4, 0); // reserved
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdMuxSetVolume(int OsdDevice, uint8_t Volume)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_AV_MUX,
                       HDFF_MSG_MUX_SET_VOLUME);
    BitBuffer_SetBits(&cmdBuf, 8, Volume);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdMuxMuteAudio(int OsdDevice, int Mute)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_AV_MUX,
                       HDFF_MSG_MUX_SET_AUDIO_MUTE);
    BitBuffer_SetBits(&cmdBuf, 1, Mute);
    BitBuffer_SetBits(&cmdBuf, 7, 0); // reserved
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}
