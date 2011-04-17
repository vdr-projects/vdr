/*
 * bitbuffer.c: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: bitbuffer.c 1.1 2009/12/29 14:29:20 kls Exp $
 */

#include "bitbuffer.h"
#include <stdlib.h>

cBitBuffer::cBitBuffer(uint32_t MaxLength)
{
    mData = NULL;
    mMaxLength = 0;
    mBitPos = 0;

    if (MaxLength <= 0x10000)
    {
        mData = new uint8_t[MaxLength];
        if (mData)
        {
            mMaxLength = MaxLength * 8;
        }
    }
}

cBitBuffer::~cBitBuffer(void)
{
    if (mData)
        delete[] mData;
}

uint8_t * cBitBuffer::GetData(void)
{
    return mData;
}

uint32_t cBitBuffer::GetMaxLength(void)
{
    return mMaxLength / 8;
}

uint32_t cBitBuffer::GetBits(int NumBits)
{
    return 0;
}

void cBitBuffer::SetBits(int NumBits, uint32_t Data)
{
    uint32_t nextBitPos;
    uint32_t bytePos;
    uint32_t bitsInByte;
    int shift;

    if (NumBits <= 0 || NumBits > 32)
        return;

    nextBitPos = mBitPos + NumBits;

    if (nextBitPos > mMaxLength)
        return;

    bytePos = mBitPos / 8;
    bitsInByte = mBitPos % 8;

    mData[bytePos] &= (uint8_t) (0xFF << (8 - bitsInByte));
    shift = NumBits - (8 - bitsInByte);
    if (shift > 0)
        mData[bytePos] |= (uint8_t) (Data >> shift);
    else
        mData[bytePos] |= (uint8_t) (Data << (-shift));
    NumBits -= 8 - bitsInByte;
    bytePos++;
    while (NumBits > 0)
    {
        shift = NumBits - 8;
        if (shift > 0)
            mData[bytePos] = (uint8_t) (Data >> shift);
        else
            mData[bytePos] = (uint8_t) (Data << (-shift));
        NumBits -= 8;
        bytePos++;
    }
    mBitPos = nextBitPos;
}

uint32_t cBitBuffer::GetByteLength(void)
{
    return (mBitPos + 7) / 8;
}

void cBitBuffer::SetDataByte(uint32_t Position, uint8_t Data)
{
    if (Position < mMaxLength)
        mData[Position] = Data;
}
