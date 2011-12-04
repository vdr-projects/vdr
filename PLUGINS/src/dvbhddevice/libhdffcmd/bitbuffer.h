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

#ifndef BITBUFFER_H
#define BITBUFFER_H

#include <stdint.h>

typedef struct BitBuffer_t
{
    uint8_t * Data;
    uint32_t MaxLength;
    uint32_t BitPos;
} BitBuffer_t;

void BitBuffer_Init(BitBuffer_t * BitBuffer,
                    uint8_t * Data, uint32_t MaxLength);

void BitBuffer_SetBits(BitBuffer_t * BitBuffer, int NumBits, uint32_t Data);

uint32_t BitBuffer_GetByteLength(BitBuffer_t * BitBuffer);

#endif /* BITBUFFER_H */
