/*****
*
* This file is part of the OMS program.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#ifndef __DOWNMIX_C_H__
#define __DOWNMIX_C_H__

void downmix_3f_2r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_3f_1r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_2f_2r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_2f_1r_to_2ch_c(float *samples, dm_par_t * dm_par);
void downmix_3f_0r_to_2ch_c(float *samples, dm_par_t * dm_par);            
void stream_sample_2ch_to_s16_c(int16_t *s16_samples, float *left, float *right);
void stream_sample_1ch_to_s16_c(int16_t *s16_samples, float *center);      

#endif
