/* 
 *  srfft_kni.c
 *
 *  Copyright (C) Yuqing Deng <Yuqing_Deng@brown.edu> - April 2000
 *
 *  64 and 128 point split radix fft for ac3dec
 *
 *  The algorithm is desribed in the book:
 *  "Computational Frameworks of the Fast Fourier Transform".
 *
 *  The ideas and the the organization of code borrowed from djbfft written by
 *  D. J. Bernstein <djb@cr.py.to>.  djbff can be found at 
 *  http://cr.yp.to/djbfft.html.
 *
 *  srfft.c is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  srfft.c is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __i386__

#include <stdio.h>

#include "srfft_kni.h"
#include "srfftp.h"

void fft_64p_kni(complex_t *a)
{
	fft_8_kni(&a[0]); fft_4_kni(&a[8]); fft_4_kni(&a[12]);
	fft_asmb_kni(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
	fft_8_kni(&a[16]), fft_8_kni(&a[24]);
	fft_asmb_kni(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

	fft_8_kni(&a[32]); fft_4_kni(&a[40]); fft_4_kni(&a[44]);
	fft_asmb_kni(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

	fft_8_kni(&a[48]); fft_4_kni(&a[56]); fft_4_kni(&a[60]);
	fft_asmb_kni(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

	fft_asmb_kni(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);
}


void fft_128p_kni(complex_t *a)
{
	fft_8_kni(&a[0]); fft_4_kni(&a[8]); fft_4_kni(&a[12]);
	fft_asmb_kni(2, &a[0], &a[8], &delta16[0], &delta16_3[0]);
  
	fft_8_kni(&a[16]), fft_8_kni(&a[24]);
	fft_asmb_kni(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

	fft_8_kni(&a[32]); fft_4_kni(&a[40]); fft_4_kni(&a[44]);
	fft_asmb_kni(2, &a[32], &a[40], &delta16[0], &delta16_3[0]);

	fft_8_kni(&a[48]); fft_4_kni(&a[56]); fft_4_kni(&a[60]);
	fft_asmb_kni(2, &a[48], &a[56], &delta16[0], &delta16_3[0]);

	fft_asmb_kni(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

	fft_8_kni(&a[64]); fft_4_kni(&a[72]); fft_4_kni(&a[76]);
	/* fft_16(&a[64]); */
	fft_asmb_kni(2, &a[64], &a[72], &delta16[0], &delta16_3[0]);

	fft_8_kni(&a[80]); fft_8_kni(&a[88]);
  
	/* fft_32(&a[64]); */
	fft_asmb_kni(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

	fft_8_kni(&a[96]); fft_4_kni(&a[104]), fft_4_kni(&a[108]);
	/* fft_16(&a[96]); */
	fft_asmb_kni(2, &a[96], &a[104], &delta16[0], &delta16_3[0]);

	fft_8_kni(&a[112]), fft_8_kni(&a[120]);
	/* fft_32(&a[96]); */
	fft_asmb_kni(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
	/* fft_128(&a[0]); */
	fft_asmb_kni(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}

#endif
