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

#include <string.h>

#include "bitbuffer.h"

void BitBuffer_Init(BitBuffer_t * BitBuffer,
                    uint8_t * Data, uint32_t MaxLength)
{
    memset(Data, 0, MaxLength);
    BitBuffer->Data = Data;
    BitBuffer->MaxLength = MaxLength * 8;
    BitBuffer->BitPos = 0;
}

void BitBuffer_SetBits(BitBuffer_t * BitBuffer, int NumBits, uint32_t Data)
{
    uint32_t nextBitPos;
    uint32_t bytePos;
    uint32_t bitsInByte;
    int shift;

    if (NumBits <= 0 || NumBits > 32)
        return;

    nextBitPos = BitBuffer->BitPos + NumBits;

    if (nextBitPos > BitBuffer->MaxLength)
        return;

    bytePos = BitBuffer->BitPos / 8;
    bitsInByte = BitBuffer->BitPos % 8;

    BitBuffer->Data[bytePos] &= (uint8_t) (0xFF << (8 - bitsInByte));
    shift = NumBits - (8 - bitsInByte);
    if (shift > 0)
        BitBuffer->Data[bytePos] |= (uint8_t) (Data >> shift);
    else
        BitBuffer->Data[bytePos] |= (uint8_t) (Data << (-shift));
    NumBits -= 8 - bitsInByte;
    bytePos++;
    while (NumBits > 0)
    {
        shift = NumBits - 8;
        if (shift > 0)
            BitBuffer->Data[bytePos] = (uint8_t) (Data >> shift);
        else
            BitBuffer->Data[bytePos] = (uint8_t) (Data << (-shift));
        NumBits -= 8;
        bytePos++;
    }
    BitBuffer->BitPos = nextBitPos;
}

uint32_t BitBuffer_GetByteLength(BitBuffer_t * BitBuffer)
{
    return (BitBuffer->BitPos + 7) / 8;
}
