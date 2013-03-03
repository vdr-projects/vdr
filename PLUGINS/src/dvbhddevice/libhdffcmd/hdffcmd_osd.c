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


int HdffCmdOsdConfigure(int OsdDevice, const HdffOsdConfig_t * Config)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_CONFIGURE);
    if (Config->FontAntialiasing)
    {
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    }
    else
    {
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    }
    if (Config->FontKerning)
    {
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    }
    else
    {
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    }
    BitBuffer_SetBits(&cmdBuf, 6, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 16, Config->FontDpi);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdReset(int OsdDevice)
{
    uint8_t cmdData[8];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_RESET);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}


int HdffCmdOsdCreateDisplay(int OsdDevice, uint16_t Width, uint16_t Height,
                            HdffColorType_t ColorType, uint32_t * NewDisplay)
{
    uint8_t cmdData[16];
    uint8_t resultData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int err;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_CREATE_DISPLAY);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    BitBuffer_SetBits(&cmdBuf, 8, ColorType);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    *NewDisplay = HDFF_INVALID_HANDLE;
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (resultData[2] == HDFF_MSG_TYPE_ANSWER)
            {
                *NewDisplay = (resultData[6] << 24)
                            | (resultData[7] << 16)
                            | (resultData[8] << 8)
                            | resultData[9];
            }
            else
                err = -1;
        }
    }
    return err;
}

int HdffCmdOsdDeleteDisplay(int OsdDevice, uint32_t Display)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DELETE_DISPLAY);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdEnableDisplay(int OsdDevice, uint32_t Display, int Enable)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_ENABLE_DISPLAY);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    if (Enable)
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

int HdffCmdOsdSetDisplayOutputRectangle(int OsdDevice, uint32_t Display,
                                        uint16_t X, uint16_t Y,
                                        uint16_t Width, uint16_t Height)
{
    uint8_t cmdData[20];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_SET_DISPLAY_OUTPUT_RECTANGLE);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdSetDisplayClippingArea(int OsdDevice, uint32_t Display,
                                     int Enable, uint16_t X, uint16_t Y,
                                     uint16_t Width, uint16_t Height)
{
    uint8_t cmdData[20];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_SET_DISPLAY_CLIPPLING_AREA);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    if (Enable)
    {
        BitBuffer_SetBits(&cmdBuf, 1, 1);
    }
    else
    {
        BitBuffer_SetBits(&cmdBuf, 1, 0);
    }
    BitBuffer_SetBits(&cmdBuf, 7, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdRenderDisplay(int OsdDevice, uint32_t Display)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_RENDER_DISPLAY);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdSaveRegion(int OsdDevice, uint32_t Display,
                         uint16_t X, uint16_t Y,
                         uint16_t Width, uint16_t Height)
{
    uint8_t cmdData[20];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_SAVE_REGION);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdRestoreRegion(int OsdDevice, uint32_t Display)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_RESTORE_REGION);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}


int HdffCmdOsdCreatePalette(int OsdDevice, HdffColorType_t ColorType,
                            HdffColorFormat_t ColorFormat,
                            uint32_t NumColors, const uint32_t * Colors,
                            uint32_t * NewPalette)
{
    uint8_t cmdData[1060];
    uint8_t resultData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;
    int err;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_CREATE_PALETTE);
    BitBuffer_SetBits(&cmdBuf, 8, ColorType);
    BitBuffer_SetBits(&cmdBuf, 8, ColorFormat);
    if (NumColors > 256)
        NumColors = 256;
    BitBuffer_SetBits(&cmdBuf, 8, NumColors == 256 ? 0 : NumColors);
    for (i = 0; i < NumColors; i++)
    {
        BitBuffer_SetBits(&cmdBuf, 32, Colors[i]);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    *NewPalette = HDFF_INVALID_HANDLE;
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (resultData[2] == HDFF_MSG_TYPE_ANSWER)
            {
                *NewPalette = (resultData[6] << 24)
                            | (resultData[7] << 16)
                            | (resultData[8] << 8)
                            | resultData[9];
            }
            else
                err = -1;
        }
    }
    return err;
}

int HdffCmdOsdDeletePalette(int OsdDevice, uint32_t Palette)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DELETE_PALETTE);
    BitBuffer_SetBits(&cmdBuf, 32, Palette);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdSetDisplayPalette(int OsdDevice, uint32_t Display,
                                uint32_t Palette)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_SET_DISPLAY_PALETTE);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 32, Palette);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdSetPaletteColors(int OsdDevice, uint32_t Palette,
                               HdffColorFormat_t ColorFormat,
                               uint8_t StartColor, uint32_t NumColors,
                               const uint32_t * Colors)
{
    uint8_t cmdData[1060];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_SET_PALETTE_COLORS);
    BitBuffer_SetBits(&cmdBuf, 32, Palette);
    BitBuffer_SetBits(&cmdBuf, 8, ColorFormat);
    BitBuffer_SetBits(&cmdBuf, 8, StartColor);
    if (NumColors > 256)
        NumColors = 256;
    BitBuffer_SetBits(&cmdBuf, 8, NumColors == 256 ? 0 : NumColors);
    for (i = 0; i < NumColors; i++)
    {
        BitBuffer_SetBits(&cmdBuf, 32, Colors[i]);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdCreateFontFace(int OsdDevice, const uint8_t * FontData,
                             uint32_t DataSize, uint32_t * NewFontFace)
{
    uint8_t cmdData[16];
    uint8_t resultData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;
    int err;

    *NewFontFace = HDFF_INVALID_HANDLE;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = FontData;
    osd_data.data_length = DataSize;
    err = ioctl(OsdDevice, OSD_RAW_DATA, &osd_data);
    if (err != 0)
        return err;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_CREATE_FONT_FACE);
    BitBuffer_SetBits(&cmdBuf, 16, osd_data.data_handle);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (resultData[2] == HDFF_MSG_TYPE_ANSWER)
            {
                *NewFontFace = (resultData[6] << 24)
                             | (resultData[7] << 16)
                             | (resultData[8] << 8)
                             | resultData[9];
            }
            else
                err = -1;
        }
    }
    return err;
}

int HdffCmdOsdDeleteFontFace(int OsdDevice, uint32_t FontFace)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DELETE_FONT_FACE);
    BitBuffer_SetBits(&cmdBuf, 32, FontFace);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdCreateFont(int OsdDevice, uint32_t FontFace, uint32_t Size,
                         uint32_t * NewFont)
{
    uint8_t cmdData[16];
    uint8_t resultData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int err;

    *NewFont = HDFF_INVALID_HANDLE;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    osd_cmd.result_data = resultData;
    osd_cmd.result_len = sizeof(resultData);
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_CREATE_FONT);
    BitBuffer_SetBits(&cmdBuf, 32, FontFace);
    BitBuffer_SetBits(&cmdBuf, 32, Size);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    err = ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
    if (err == 0)
    {
        if (osd_cmd.result_len > 0)
        {
            if (resultData[2] == HDFF_MSG_TYPE_ANSWER)
            {
                *NewFont = (resultData[6] << 24)
                         | (resultData[7] << 16)
                         | (resultData[8] << 8)
                         | resultData[9];
            }
            else
                err = -1;
        }
    }
    return err;
}

int HdffCmdOsdDeleteFont(int OsdDevice, uint32_t Font)
{
    uint8_t cmdData[16];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DELETE_FONT);
    BitBuffer_SetBits(&cmdBuf, 32, Font);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}


int HdffCmdOsdDrawRectangle(int OsdDevice, uint32_t Display, uint16_t X,
                            uint16_t Y, uint16_t Width, uint16_t Height,
                            uint32_t Color)
{
    uint8_t cmdData[24];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_RECTANGLE);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawEllipse(int OsdDevice, uint32_t Display, uint16_t CX,
                          uint16_t CY, uint16_t RadiusX, uint16_t RadiusY,
                          uint32_t Color, uint32_t Flags)
{
    uint8_t cmdData[28];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_ELLIPSE);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, CX);
    BitBuffer_SetBits(&cmdBuf, 16, CY);
    BitBuffer_SetBits(&cmdBuf, 16, RadiusX);
    BitBuffer_SetBits(&cmdBuf, 16, RadiusY);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    BitBuffer_SetBits(&cmdBuf, 32, Flags);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawSlope(int OsdDevice, uint32_t Display, uint16_t X,
                        uint16_t Y, uint16_t Width, uint16_t Height,
                        uint32_t Color, uint32_t Type)
{
    uint8_t cmdData[28];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_SLOPE);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, Width);
    BitBuffer_SetBits(&cmdBuf, 16, Height);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    BitBuffer_SetBits(&cmdBuf, 32, Type);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawText(int OsdDevice, uint32_t Display, uint32_t Font,
                       uint16_t X, uint16_t Y, const char * Text,
                       uint32_t Color)
{
    uint8_t cmdData[1060];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;
    int length;

    length = 0;
    while (Text[length])
    {
        length++;
    }
    if (length > 980)
        length = 980;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_TEXT);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 32, Font);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    BitBuffer_SetBits(&cmdBuf, 16, length);
    for (i = 0; i < length; i++)
    {
        BitBuffer_SetBits(&cmdBuf, 8, Text[i]);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawUtf8Text(int OsdDevice, uint32_t Display, uint32_t Font,
                           uint16_t X, uint16_t Y, const char * Text,
                           uint32_t Color)
{
    uint8_t cmdData[1060];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;
    int length;

    length = 0;
    while (Text[length])
    {
        length++;
    }
    if (length > 980)
        length = 980;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_UTF8_TEXT);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 32, Font);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    BitBuffer_SetBits(&cmdBuf, 16, length);
    for (i = 0; i < length; i++)
    {
        BitBuffer_SetBits(&cmdBuf, 8, Text[i]);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawWideText(int OsdDevice, uint32_t Display, uint32_t Font,
                           uint16_t X, uint16_t Y, const uint16_t * Text,
                           uint32_t Color)
{
    uint8_t cmdData[1060];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    int i;
    int length;

    length = 0;
    while (Text[length])
    {
        length++;
    }
    if (length > 480)
        length = 480;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_WIDE_TEXT);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 32, Font);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 32, Color);
    BitBuffer_SetBits(&cmdBuf, 16, length);
    for (i = 0; i < length; i++)
    {
        BitBuffer_SetBits(&cmdBuf, 16, Text[i]);
    }
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}

int HdffCmdOsdDrawBitmap(int OsdDevice, uint32_t Display, uint16_t X,
                         uint16_t Y, const uint8_t * Bitmap, uint16_t BmpWidth,
                         uint16_t BmpHeight, uint32_t BmpSize,
                         HdffColorType_t ColorType, uint32_t Palette)
{
    uint8_t cmdData[32];
    BitBuffer_t cmdBuf;
    osd_raw_cmd_t osd_cmd;
    osd_raw_data_t osd_data;
    int err;

    memset(&osd_data, 0, sizeof(osd_raw_data_t));
    osd_data.data_buffer = Bitmap;
    osd_data.data_length = BmpSize;
    err = ioctl(OsdDevice, OSD_RAW_DATA, &osd_data);
    if (err != 0)
        return err;

    BitBuffer_Init(&cmdBuf, cmdData, sizeof(cmdData));
    memset(&osd_cmd, 0, sizeof(osd_raw_cmd_t));
    osd_cmd.cmd_data = cmdData;
    HdffCmdBuildHeader(&cmdBuf, HDFF_MSG_TYPE_COMMAND, HDFF_MSG_GROUP_OSD,
                       HDFF_MSG_OSD_DRAW_BITMAP);
    BitBuffer_SetBits(&cmdBuf, 32, Display);
    BitBuffer_SetBits(&cmdBuf, 16, X);
    BitBuffer_SetBits(&cmdBuf, 16, Y);
    BitBuffer_SetBits(&cmdBuf, 16, BmpWidth);
    BitBuffer_SetBits(&cmdBuf, 16, BmpHeight);
    BitBuffer_SetBits(&cmdBuf, 8, ColorType);
    BitBuffer_SetBits(&cmdBuf, 6, 0); // reserved
    BitBuffer_SetBits(&cmdBuf, 2, 0); // uncompressed
    BitBuffer_SetBits(&cmdBuf, 32, Palette);
    BitBuffer_SetBits(&cmdBuf, 16, osd_data.data_handle);
    BitBuffer_SetBits(&cmdBuf, 32, 0);
    osd_cmd.cmd_len = HdffCmdSetLength(&cmdBuf);
    return ioctl(OsdDevice, OSD_RAW_CMD, &osd_cmd);
}
