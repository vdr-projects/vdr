/* 
 *  imdct_kni.c
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

#ifdef __i386__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <mm_accel.h>
#include "ac3.h"
#include "ac3_internal.h"

#include "downmix.h"
#include "imdct_kni.h"
#include "srfft.h"

#define N 512

/* Delay buffer for time domain interleaving */
static float xcos_sin_sse[128 * 4] __attribute__((aligned(16)));

extern void (*imdct_do_512) (float data[],float delay[]);
extern void (*imdct_do_512_nol) (float data[], float delay[]);
extern void (*fft_64p) (complex_t *);

extern const int pm128[];
extern float window[];
extern complex_t buf[128];

extern void fft_64p_kni (complex_t *);
extern void fft_128p_kni (complex_t *);

static void imdct_do_512_kni (float data[], float delay[]);
static void imdct_do_512_nol_kni (float data[], float delay[]);


int imdct_init_kni (void)
{
	uint32_t accel = mm_accel ();

	if (accel & MM_ACCEL_X86_MMXEXT) {
		int i;
		float scale = 255.99609372;

		fprintf (stderr, "Using SSE for IMDCT\n");
		imdct_do_512 = imdct_do_512_kni;
		imdct_do_512_nol = imdct_do_512_nol_kni;
		fft_64p = fft_64p_kni;

		for (i=0; i < 128; i++) {
			float xcos_i = cos(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
                       	float xsin_i = sin(2.0f * M_PI * (8*i+1)/(8*N)) * scale;
                       	xcos_sin_sse[i * 4] = xcos_i;
                       	xcos_sin_sse[i * 4 + 1] = -xsin_i;
                       	xcos_sin_sse[i * 4 + 2] = -xsin_i;
                       	xcos_sin_sse[i * 4 + 3] = -xcos_i;
               	}

		return 0;
	} else
		return -1;
}


static void imdct_do_512_kni (float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_kni (pm128, buf, data, xcos_sin_sse);
	fft_128p_kni (buf);
	imdct512_post_ifft_twiddle_kni (buf, xcos_sin_sse);
	imdct512_window_delay_kni (buf, data, window, delay);
}


static void imdct_do_512_nol_kni (float data[], float delay[])
{
	imdct512_pre_ifft_twiddle_kni (pm128, buf, data, xcos_sin_sse);  
	fft_128p_kni (buf);
	imdct512_post_ifft_twiddle_kni (buf, xcos_sin_sse);
	imdct512_window_delay_nol_kni (buf, data, window, delay);
}

#endif
