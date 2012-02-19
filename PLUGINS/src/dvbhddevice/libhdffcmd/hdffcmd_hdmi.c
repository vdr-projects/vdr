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


int HdffCmdHdmiSetVideoMode(int OsdDevice, HdffVideoMode_t VideoMode)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_HDMI,
                       HDFF_MSG_HDMI_SET_VIDEO_MODE);
    BitBuffer_SetBits(&cmdBuf, 8, VideoMode);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdHdmiConfigure(int OsdDevice, const HdffHdmiConfig_t * Config)
{
    uint8_t cmdData[24];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    size_t nameLen;
    int i;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_HDMI,
                       HDFF_MSG_HDMI_CONFIGURE);
    BitBuffer_SetBits(&cmdBuf, 1, Config->TransmitAudio ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 1, Config->ForceDviMode ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 1, Config->CecEnabled ? 1 : 0);
    BitBuffer_SetBits(&cmdBuf, 3, Config->VideoModeAdaption);
    BitBuffer_SetBits(&cmdBuf, 6, 0); // reserved
    nameLen = strlen(Config->CecDeviceName);
    if (nameLen > 13)
        nameLen = 13;
    BitBuffer_SetBits(&cmdBuf, 4, nameLen);
    for (i = 0; i < nameLen; i++)
        BitBuffer_SetBits(&cmdBuf, 8, Config->CecDeviceName[i]);

    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdHdmiSendCecCommand(int OsdDevice, HdffCecCommand_t Command)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_HDMI,
                       HDFF_MSG_HDMI_SEND_CEC_COMMAND);
    BitBuffer_SetBits(&cmdBuf, 8, Command);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdHdmiSendRawCecCommand(int OsdDevice, uint8_t Destination,
                                 uint8_t Opcode, const uint8_t * Operand,
                                 uint8_t OperandLength)
{
    uint8_t cmdData[24];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;

    if (OperandLength > 14)
        OperandLength = 14;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_HDMI,
                       HDFF_MSG_HDMI_SEND_RAW_CEC_COMMAND);
    BitBuffer_SetBits(&cmdBuf, 4, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 4, Destination);
    BitBuffer_SetBits(&cmdBuf, 8, Opcode);
    BitBuffer_SetBits(&cmdBuf, 8, OperandLength);
    for (i = 0; i < OperandLength; i++)
        BitBuffer_SetBits(&cmdBuf, 8, Operand[i]);

    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}
