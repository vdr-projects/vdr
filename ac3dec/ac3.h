/* 
 *    ac3.h
 *
 *	Copyright (C) Aaron Holtzman - May 1999
 *
 *  This file is part of ac3dec, a free Dolby AC-3 stream decoder.
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
 *
 */

#define AC3_BUFFER_SIZE (6*1024*16)

#ifndef __AC3_H__
#define __AC3_H__

#ifdef __OMS__
#include <oms/plugin/output_audio.h>
#else
//#include "audio_out.h"
#endif

#include <inttypes.h>

#define AC3_DOLBY_SURR_ENABLE	(1<<0)
#define AC3_ALTIVEC_ENABLE	(1<<1)

typedef struct ac3_config_s {
	// Bit flags that enable various things
	uint32_t flags;
	// Number of discrete channels in final output (for downmixing)
	uint16_t num_output_ch;
	// Which channel of a dual mono stream to select
	uint16_t dual_mono_ch_sel;
} ac3_config_t;

void ac3dec_init (void);

#ifdef __OMS__
size_t ac3dec_decode_data (plugin_output_audio_t *output, uint8_t *data_start, uint8_t *data_end);
#else
size_t ac3dec_decode_data (uint8_t *data_start ,uint8_t *data_end, int ac3reset, int *input_pointer, int *output_pointer, char *ac3_data);
#endif

#endif
