/* 
 *  bitstream.c
 *
 *	Copyright (C) Aaron Holtzman - Dec 1999
 *
 *  This file is part of ac3dec, a free AC-3 audio decoder
 *	
 *  ac3dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  ac3dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <bswap.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ac3.h"
#include "ac3_internal.h"
#include "bitstream.h"


uint32_t bits_left = 0;
uint64_t current_word;
uint64_t *buffer_start = 0;


static inline uint64_t getdword (void)
{
	return be2me_64 (*buffer_start++);
}


static inline void bitstream_fill_current (void)
{
	//current_word = bswap_64 (*buffer_start++);
	current_word = getdword ();
}


uint32_t bitstream_get_bh (uint32_t num_bits)
{
	uint32_t result;

	num_bits -= bits_left;
	result = (current_word << (64 - bits_left)) >> (64 - bits_left);

	bitstream_fill_current();

	if(num_bits != 0)
		result = (result << num_bits) | (current_word >> (64 - num_bits));
	
	bits_left = 64 - num_bits;

	return result;
}


void bitstream_init (uint8_t *start)
{
	//initialize the start of the buffer
	buffer_start = (uint64_t *) start;
	bits_left = 0;
}
