/*
 *  downmix_c.c
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
#include "ac3.h"
#include "ac3_internal.h"

#include "debug.h"
#include "downmix.h"
#include "downmix_c.h"


void downmix_3f_2r_to_2ch_c (float *samples, dm_par_t *dm_par)
{
	int i;
	float *left, *right, *center, *left_sur, *right_sur;
	float left_tmp, right_tmp;

	left = samples;
	right = samples + 256 * 2;
	center = samples + 256;
	left_sur = samples + 256 * 3;
	right_sur = samples + 256 * 4;

	for (i=0; i < 256; i++) {
#if defined DOLBY_SURROUND
	  	left_tmp = dm_par->unit * *left  + dm_par->clev * *center - dm_par->slev * (*left_sur + *right_sur);
	  	right_tmp= dm_par->unit * *right++ + dm_par->clev * *center + dm_par->slev * (*left_sur++ + *right_sur++);
#else
		left_tmp = dm_par->unit * *left  + dm_par->clev * *center + dm_par->slev * *left_sur++;
		right_tmp= dm_par->unit * *right++ + dm_par->clev * *center + dm_par->slev * *right_sur++;
#endif
		*left++ = left_tmp;
		*center++ = right_tmp;

	}
}


void downmix_2f_2r_to_2ch_c (float *samples, dm_par_t *dm_par)
{
	int i;
	float *left, *right, *left_sur, *right_sur;
	float left_tmp, right_tmp;
               
	left = &samples[0];
	right = &samples[256];
	left_sur = &samples[512];
	right_sur = &samples[768];

	for (i = 0; i < 256; i++) {
		left_tmp = dm_par->unit * *left  + dm_par->slev * *left_sur++;
		right_tmp= dm_par->unit * *right + dm_par->slev * *right_sur++;
		*left++ = left_tmp;
		*right++ = right_tmp;
	}
}


void downmix_3f_1r_to_2ch_c (float *samples, dm_par_t *dm_par)
{
	int i;
	float *left, *right, *center, *right_sur;
	float left_tmp, right_tmp;

	left = &samples[0];
	right = &samples[512];
	center = &samples[256];
	right_sur = &samples[768];

	for (i = 0; i < 256; i++) {
		left_tmp = dm_par->unit * *left  + dm_par->clev * *center  - dm_par->slev * *right_sur;
		right_tmp= dm_par->unit * *right++ + dm_par->clev * *center + dm_par->slev * *right_sur++;
		*left++ = left_tmp;
		*center++ = right_tmp;
	}
}


void downmix_2f_1r_to_2ch_c (float *samples, dm_par_t *dm_par)
{
	int i;
	float *left, *right, *right_sur;
	float left_tmp, right_tmp;

	left = &samples[0];
	right = &samples[256];
	right_sur = &samples[512];

	for (i = 0; i < 256; i++) {
		left_tmp = dm_par->unit * *left  - dm_par->slev * *right_sur;
		right_tmp= dm_par->unit * *right + dm_par->slev * *right_sur++;
		*left++ = left_tmp;
		*right++ = right_tmp;
	}
}


void downmix_3f_0r_to_2ch_c (float *samples, dm_par_t *dm_par)
{
	int i;
	float *left, *right, *center;
	float left_tmp, right_tmp;

	left = &samples[0];
	center = &samples[256];
	right = &samples[512];

	for (i = 0; i < 256; i++) {
		left_tmp = dm_par->unit * *left  + dm_par->clev * *center;
		right_tmp= dm_par->unit * *right++ + dm_par->clev * *center;
		*left++ = left_tmp;
		*center++ = right_tmp;
	}
}


void stream_sample_2ch_to_s16_c (int16_t *s16_samples, float *left, float *right)
{
	int i;

	for (i=0; i < 256; i++) {
		*s16_samples++ = (int16_t) *left++;
		*s16_samples++ = (int16_t) *right++;
	}
}


void stream_sample_1ch_to_s16_c (int16_t *s16_samples, float *center)
{
	int i;
	float tmp;

	for (i=0; i < 256; i++) {
		*s16_samples++ = tmp = (int16_t) (0.7071f * *center++);
		*s16_samples++ = tmp;
	}
}
