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

#ifndef HDFFCMD_BASE_H
#define HDFFCMD_BASE_H

#include <linux/dvb/osd.h>

#if !defined OSD_RAW_CMD
typedef struct osd_raw_cmd_s {
    const void *cmd_data;
    int cmd_len;
    void *result_data;
    int result_len;
} osd_raw_cmd_t;

typedef struct osd_raw_data_s {
    const void *data_buffer;
    int data_length;
    int data_handle;
} osd_raw_data_t;

#define OSD_RAW_CMD            _IOWR('o', 162, osd_raw_cmd_t)
#define OSD_RAW_DATA           _IOWR('o', 163, osd_raw_data_t)
#endif

#include "bitbuffer.h"
#include "hdffcmd_defs.h"

void HdffCmdBuildHeader(BitBuffer_t * MsgBuf, HdffMessageType_t MsgType,
                        HdffMessageGroup_t MsgGroup, HdffMessageId_t MsgId);

uint32_t HdffCmdSetLength(BitBuffer_t * MsgBuf);

#endif /* HDFFCMD_BASE_H */
