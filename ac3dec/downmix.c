/*
 *  imdct.c
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mm_accel.h>

#include "ac3.h"
#include "ac3_internal.h"

#include "debug.h"
#include "downmix.h"
#include "downmix_c.h"
#include "downmix_i386.h"
#ifdef HAVE_KNI
#include "downmix_kni.h"
#endif

void (*downmix_3f_2r_to_2ch)(float *samples, dm_par_t * dm_par);
void (*downmix_3f_1r_to_2ch)(float *samples, dm_par_t * dm_par);
void (*downmix_2f_2r_to_2ch)(float *samples, dm_par_t * dm_par);
void (*downmix_2f_1r_to_2ch)(float *samples, dm_par_t * dm_par);
void (*downmix_3f_0r_to_2ch)(float *samples, dm_par_t * dm_par);
void (*stream_sample_2ch_to_s16)(int16_t *s16_samples, float *left, float *right);
void (*stream_sample_1ch_to_s16)(int16_t *s16_samples, float *center);


void downmix_init()
{
#ifdef __i386__
#ifdef HAVE_KNI
	uint32_t accel = mm_accel ();

// other dowmixing should go here too
	if (accel & MM_ACCEL_X86_MMXEXT) {
		dprintf("Using SSE for downmix\n");
		downmix_3f_2r_to_2ch = downmix_3f_2r_to_2ch_kni;
		downmix_2f_2r_to_2ch = downmix_2f_2r_to_2ch_kni;
		downmix_3f_1r_to_2ch = downmix_3f_1r_to_2ch_kni;
		downmix_2f_1r_to_2ch = downmix_2f_1r_to_2ch_kni;
		downmix_3f_0r_to_2ch = downmix_3f_0r_to_2ch_kni;
		stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_kni;
		stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_kni;
	} else if (accel & MM_ACCEL_X86_3DNOW) {
	} else
#endif
#endif
	{
		downmix_3f_2r_to_2ch = downmix_3f_2r_to_2ch_c;
		downmix_2f_2r_to_2ch = downmix_2f_2r_to_2ch_c;
		downmix_3f_1r_to_2ch = downmix_3f_1r_to_2ch_c;
		downmix_2f_1r_to_2ch = downmix_2f_1r_to_2ch_c;
		downmix_3f_0r_to_2ch = downmix_3f_0r_to_2ch_c;
#ifdef __i386__
#if 1
		stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_c;
		stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_c;
#else
		stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_i386;
		stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_i386;
#endif
#else
		stream_sample_2ch_to_s16 = stream_sample_2ch_to_s16_c;
		stream_sample_1ch_to_s16 = stream_sample_1ch_to_s16_c;
#endif
	}
}
