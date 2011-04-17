/*
 * bitbuffer.h: TODO(short description)
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: bitbuffer.h 1.1 2009/12/29 14:27:03 kls Exp $
 */

#ifndef _HDFF_BITBUFFER_H_
#define _HDFF_BITBUFFER_H_

#include <stdint.h>

class cBitBuffer
{
private:
    uint8_t * mData;
    uint32_t mMaxLength;
    uint32_t mBitPos;
public:
    cBitBuffer(uint32_t MaxLength);
    ~cBitBuffer(void);
    uint8_t * GetData(void);
    uint32_t GetMaxLength(void);
    uint32_t GetBits(int NumBits);
    void SetBits(int NumBits, uint32_t Data);
    uint32_t GetByteLength(void);
    void SetDataByte(uint32_t Position, uint8_t Data);
};

#endif
