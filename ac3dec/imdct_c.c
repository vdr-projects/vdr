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
#include "ac3.h"
#include "ac3_internal.h"

#include "downmix.h"
#include "imdct_c.h"
#include "srfft.h"


#define N 512

extern void (*imdct_do_512) (float data[],float delay[]);
extern void (*imdct_do_512_nol) (float data[], float delay[]);
extern void (*fft_64p) (complex_t *);

extern const int pm128[];
extern float window[];
extern complex_t buf[128];

extern void fft_64p_c (complex_t *);
extern void fft_128p_c (complex_t *);

static void imdct_do_512_c (float data[],float delay[]);
static void imdct_do_512_nol_c (float data[], float delay[]);

/* Twiddle factors for IMDCT */
static float xcos1[128] __attribute__((aligned(16)));
static float xsin1[128] __attribute__((aligned(16)));


int imdct_init_c (void)
{
	int i;
	float scale = 255.99609372;

	imdct_do_512 = imdct_do_512_c;
	imdct_do_512_nol = imdct_do_512_nol_c;
	fft_64p = fft_64p_c;

	/* Twiddle factors to turn IFFT into IMDCT */
         
	for (i=0; i < 128; i++) {
		xcos1[i] = cos(2.0f * M_PI * (8*i+1)/(8*N)) * scale; 
		xsin1[i] = sin(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
	}

	return 0;
}


static void imdct_do_512_c (float data[], float delay[])
{
	int i, j;
	float tmp_a_r, tmp_a_i;
	float *data_ptr;
	float *delay_ptr;
	float *window_ptr;

// 512 IMDCT with source and dest data in 'data'
// Pre IFFT complex multiply plus IFFT complex conjugate

	for( i=0; i < 128; i++) {
		j = pm128[i];
		//a = (data[256-2*j-1] - data[2*j]) * (xcos1[j] + xsin1[j]);
		//c = data[2*j] * xcos1[j];
		//b = data[256-2*j-1] * xsin1[j];
		//buf1[i].re = a - b + c;
		//buf1[i].im = b + c;
		buf[i].re = (data[256-2*j-1] * xcos1[j]) - (data[2*j] * xsin1[j]);
		buf[i].im = -1.0 * (data[2*j] * xcos1[j] + data[256-2*j-1] * xsin1[j]);
	}

	fft_128p_c (&buf[0]);

// Post IFFT complex multiply  plus IFFT complex conjugate
	for (i=0; i < 128; i++) {
		tmp_a_r = buf[i].re;
		tmp_a_i = buf[i].im;
		//a = (tmp_a_r - tmp_a_i) * (xcos1[j] + xsin1[j]);
		//b = tmp_a_r * xsin1[j];
		//c = tmp_a_i * xcos1[j];
		//buf[j].re = a - b + c;
		//buf[j].im = b + c;
		buf[i].re =(tmp_a_r * xcos1[i])  +  (tmp_a_i  * xsin1[i]);
		buf[i].im =(tmp_a_r * xsin1[i])  -  (tmp_a_i  * xcos1[i]);
	}

	data_ptr = data;
	delay_ptr = delay;
	window_ptr = window;

// Window and convert to real valued signal
	for (i=0; i< 64; i++) {
		*data_ptr++   = -buf[64+i].im   * *window_ptr++ + *delay_ptr++;
		*data_ptr++   = buf[64-i-1].re * *window_ptr++ + *delay_ptr++;
	}

	for(i=0; i< 64; i++) {
		*data_ptr++  = -buf[i].re       * *window_ptr++ + *delay_ptr++;
		*data_ptr++  = buf[128-i-1].im * *window_ptr++ + *delay_ptr++;
	}

// The trailing edge of the window goes into the delay line
	delay_ptr = delay;

	for(i=0; i< 64; i++) {
		*delay_ptr++  = -buf[64+i].re   * *--window_ptr;
		*delay_ptr++  =  buf[64-i-1].im * *--window_ptr;
	}

	for(i=0; i<64; i++) {
		*delay_ptr++  =  buf[i].im       * *--window_ptr;
		*delay_ptr++  = -buf[128-i-1].re * *--window_ptr;
	}
}


static void imdct_do_512_nol_c (float data[], float delay[])
{
	int i, j;

	float tmp_a_i;
	float tmp_a_r;

	float *data_ptr;
	float *delay_ptr;
	float *window_ptr;

        //
        // 512 IMDCT with source and dest data in 'data'
        //

	// Pre IFFT complex multiply plus IFFT cmplx conjugate

        for( i=0; i < 128; i++) {
	/* z[i] = (X[256-2*i-1] + j * X[2*i]) * (xcos1[i] + j * xsin1[i]) */
		j = pm128[i];
	//a = (data[256-2*j-1] - data[2*j]) * (xcos1[j] + xsin1[j]);
	//c = data[2*j] * xcos1[j];
	//b = data[256-2*j-1] * xsin1[j];
	//buf1[i].re = a - b + c;

         //buf1[i].im = b + c;
		buf[i].re = (data[256-2*j-1] * xcos1[j]) - (data[2*j] * xsin1[j]);
		buf[i].im = -1.0 * (data[2*j] * xcos1[j] + data[256-2*j-1] * xsin1[j]);
	}
       
	fft_128p_c (&buf[0]);

       /* Post IFFT complex multiply  plus IFFT complex conjugate*/
	for (i=0; i < 128; i++) {
		/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
		/* int j1 = i; */
		tmp_a_r = buf[i].re;
		tmp_a_i = buf[i].im;
		//a = (tmp_a_r - tmp_a_i) * (xcos1[j] + xsin1[j]);
		//b = tmp_a_r * xsin1[j];
		//c = tmp_a_i * xcos1[j];
		//buf[j].re = a - b + c;
		//buf[j].im = b + c;
		buf[i].re =(tmp_a_r * xcos1[i])  +  (tmp_a_i  * xsin1[i]);
		buf[i].im =(tmp_a_r * xsin1[i])  -  (tmp_a_i  * xcos1[i]);
	}
       
	data_ptr = data;
	delay_ptr = delay;
	window_ptr = window;

	/* Window and convert to real valued signal, no overlap here*/
	for (i=0; i< 64; i++) { 
		*data_ptr++   = -buf[64+i].im   * *window_ptr++; 
		*data_ptr++   = buf[64-i-1].re * *window_ptr++; 
	}

	for(i=0; i< 64; i++) { 
		*data_ptr++  = -buf[i].re       * *window_ptr++; 
		*data_ptr++  = buf[128-i-1].im * *window_ptr++; 
	}
       
	/* The trailing edge of the window goes into the delay line */
	delay_ptr = delay;

	for(i=0; i< 64; i++) { 
		*delay_ptr++  = -buf[64+i].re   * *--window_ptr; 
		*delay_ptr++  =  buf[64-i-1].im * *--window_ptr; 
	}

	for(i=0; i<64; i++) {
		*delay_ptr++  =  buf[i].im       * *--window_ptr; 
		*delay_ptr++  = -buf[128-i-1].re * *--window_ptr; 
	}
}
