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

#ifndef HDFFCMD_HDMI_H
#define HDFFCMD_HDMI_H


typedef enum HdffVideoMode_t
{
    HDFF_VIDEO_MODE_576P50 = 18,
    HDFF_VIDEO_MODE_720P50 = 19,
    HDFF_VIDEO_MODE_1080I50 = 20,
    HDFF_VIDEO_MODE_576I50 = 22
} HdffVideoMode_t;

typedef enum HdffVideoModeAdaption_t
{
    HDFF_VIDEO_MODE_ADAPT_OFF,
    HDFF_VIDEO_MODE_ADAPT_FRAME_RATE,
    HDFF_VIDEO_MODE_ADAPT_ONLY_FOR_HD,
    HDFF_VIDEO_MODE_ADAPT_ALWAYS
} HdffVideoModeAdaption_t;

typedef struct HdffHdmiConfig_t
{
    int TransmitAudio;
    int ForceDviMode;
    int CecEnabled;
    HdffVideoModeAdaption_t VideoModeAdaption;
    char CecDeviceName[14];
} HdffHdmiConfig_t;

typedef enum HdffCecCommand_t
{
    HDFF_CEC_COMMAND_TV_ON,
    HDFF_CEC_COMMAND_TV_OFF,
    HDFF_CEC_COMMAND_ACTIVE_SOURCE,
    HDFF_CEC_COMMAND_INACTIVE_SOURCE
} HdffCecCommand_t;


int HdffCmdHdmiSetVideoMode(int OsdDevice, HdffVideoMode_t VideoMode);

int HdffCmdHdmiConfigure(int OsdDevice, const HdffHdmiConfig_t * Config);

int HdffCmdHdmiSendCecCommand(int OsdDevice, HdffCecCommand_t Command);

int HdffCmdHdmiSendRawCecCommand(int OsdDevice, uint8_t Destination,
                                 uint8_t Opcode, const uint8_t * Operand,
                                 uint8_t OperandLength);

#endif /* HDFFCMD_HDMI_H */
