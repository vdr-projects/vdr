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

#ifndef __IMDCT_C_H__
#define __IMDCT_C_H__

#include "cmplx.h"

int imdct_init_c (void);

void fft_128p_c (complex_t *);
void fft_64p_c (complex_t *);

void imdct512_pre_ifft_twiddle_c (const int *pmt, complex_t *buf, float *data, float *xcos_sin_sse);
void imdct512_post_ifft_twiddle_c (complex_t *buf, float *xcos_sin_sse);
void imdct512_window_delay_c (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);
void imdct512_window_delay_nol_c (complex_t *buf, float *data_ptr, float *window_prt, float *delay_prt);

#endif
