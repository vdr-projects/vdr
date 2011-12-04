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

#ifndef HDFFCMD_MUX_H
#define HDFFCMD_MUX_H


typedef enum HdffVideoOut_t
{
    HDFF_VIDEO_OUT_DISABLED,
    HDFF_VIDEO_OUT_CVBS_RGB,
    HDFF_VIDEO_OUT_CVBS_YUV,
    HDFF_VIDEO_OUT_YC
} HdffVideoOut_t;

typedef enum HdffSlowBlank_t
{
    HDFF_SLOW_BLANK_OFF,
    HDFF_SLOW_BLANK_16_BY_9,
    HDFF_SLOW_BLANK_4_BY_3
} HdffSlowBlank_t;

typedef enum HdffFastBlank_t
{
    HDFF_FAST_BLANK_CVBS,
    HDFF_FAST_BLANK_RGB
} HdffFastBlank_t;


int HdffCmdMuxSetVideoOut(int OsdDevice, HdffVideoOut_t VideoOut);

int HdffCmdMuxSetVolume(int OsdDevice, uint8_t Volume);

int HdffCmdMuxMuteAudio(int OsdDevice, int Mute);

#endif /* HDFFCMD_MUX_H */
