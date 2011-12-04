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

#include "hdffcmd_base.h"

void HdffCmdBuildHeader(BitBuffer_t * MsgBuf, HdffMessageType_t MsgType,
                        HdffMessageGroup_t MsgGroup, HdffMessageId_t MsgId)
{
    BitBuffer_SetBits(MsgBuf, 16, 0); // length field will be set later
    BitBuffer_SetBits(MsgBuf, 6, 0); // reserved
    BitBuffer_SetBits(MsgBuf, 2, MsgType);
    BitBuffer_SetBits(MsgBuf, 8, MsgGroup);
    BitBuffer_SetBits(MsgBuf, 16, MsgId);
}

uint32_t HdffCmdSetLength(BitBuffer_t * MsgBuf)
{
    uint32_t length;

    length = BitBuffer_GetByteLength(MsgBuf) - 2;
    MsgBuf->Data[0] = (uint8_t) (length >> 8);
    MsgBuf->Data[1] = (uint8_t) length;

    return length + 2;
}
