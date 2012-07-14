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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

#include "hdffcmd.h"
#include "hdffcmd_base.h"
#include "hdffcmd_defs.h"

int HdffCmdGetFirmwareVersion(int OsdDevice, uint32_t * Version, char * String,
                              uint32_t MaxLength)
{
    uint8_t cmdData[8];
    uint8_t resultData[64];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int err;

    if (Version == NULL)
        return -EINVAL;

    *Version = 0;
    if (String)
        String[0] = 0;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_GENERIC,
                       HDFF_MSG_GEN_GET_FIRMWARE_VERSION);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (String)
            {
                uint8_t textLength = resultData[9];
                if (textLength >= MaxLength)
                    textLength = MaxLength - 1;
                memcpy(String, &resultData[10], textLength);
                String[textLength] = 0;
            }
            *Version = (resultData[6] << 16)
                     | (resultData[7] << 8)
                     | resultData[8];
        }
    }
    return err;
}

int HdffCmdGetInterfaceVersion(int OsdDevice, uint32_t * Version, char * String,
                               uint32_t MaxLength)
{
    uint8_t cmdData[8];
    uint8_t resultData[64];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int err;

    if (Version == NULL)
        return -EINVAL;

    *Version = 0;
    if (String)
        String[0] = 0;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_GENERIC,
                       HDFF_MSG_GEN_GET_INTERFACE_VERSION);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (String)
            {
                uint8_t textLength = resultData[9];
                if (textLength >= MaxLength)
                    textLength = MaxLength - 1;
                memcpy(String, &resultData[10], textLength);
                String[textLength] = 0;
            }
            *Version = (resultData[6] << 16)
                     | (resultData[7] << 8)
                     | resultData[8];
        }
    }
    return err;
}

int HdffCmdGetCopyrights(int OsdDevice, uint8_t Index, char * String,
                         uint32_t MaxLength)
{
    uint8_t cmdData[8];
    uint8_t resultData[280];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int err;

    if (String == NULL)
        return -EINVAL;

    String[0] = 0;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_GENERIC,
                       HDFF_MSG_GEN_GET_COPYRIGHTS);
    BitBuffer_SetBits(&cmdBuf, 8, Index);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            uint8_t index = resultData[6];
            uint8_t textLen = resultData[7];
            if (index == Index && textLen > 0)
            {
                if (textLen >= MaxLength)
                {
                    textLen = MaxLength - 1;
                }
                memcpy(String, resultData + 8, textLen);
                String[textLen] = 0;
            }
        }
    }
    return err;
}
