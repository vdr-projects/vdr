/* 
 *  srfft.c
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

#include <stdio.h>

#include "srfft.h"
#include "srfftp.h"

void fft_8 (complex_t *x);

void fft_4(complex_t *x)
{
  /* delta_p = 1 here */
  /* x[k] = sum_{i=0..3} x[i] * w^{i*k}, w=e^{-2*pi/4} 
   */

  register float yt_r, yt_i, yb_r, yb_i, u_r, u_i, vi_r, vi_i;
  
  yt_r = x[0].re;
  yb_r = yt_r - x[2].re;
  yt_r += x[2].re;

  u_r = x[1].re;
  vi_i = x[3].re - u_r;
  u_r += x[3].re;
  
  u_i = x[1].im;
  vi_r = u_i - x[3].im;
  u_i += x[3].im;

  yt_i = yt_r;
  yt_i += u_r;
  x[0].re = yt_i;
  yt_r -= u_r;
  x[2].re = yt_r;
  yt_i = yb_r;
  yt_i += vi_r;
  x[1].re = yt_i;
  yb_r -= vi_r;
  x[3].re = yb_r;

  yt_i = x[0].im;
  yb_i = yt_i - x[2].im;
  yt_i += x[2].im;

  yt_r = yt_i;
  yt_r += u_i;
  x[0].im = yt_r;
  yt_i -= u_i;
  x[2].im = yt_i;
  yt_r = yb_i;
  yt_r += vi_i;
  x[1].im = yt_r;
  yb_i -= vi_i;
  x[3].im = yb_i;
}


void fft_8 (complex_t *x)
{
  /* delta_p = diag{1, sqrt(i)} here */
  /* x[k] = sum_{i=0..7} x[i] * w^{i*k}, w=e^{-2*pi/8} 
   */
  register float wT1_r, wT1_i, wB1_r, wB1_i, wT2_r, wT2_i, wB2_r, wB2_i;
  
  wT1_r = x[1].re;
  wT1_i = x[1].im;
  wB1_r = x[3].re;
  wB1_i = x[3].im;

  x[1] = x[2];
  x[2] = x[4];
  x[3] = x[6];
  fft_4(&x[0]);

  
  /* x[0] x[4] */
  wT2_r = x[5].re;
  wT2_r += x[7].re;
  wT2_r += wT1_r;
  wT2_r += wB1_r;
  wT2_i = wT2_r;
  wT2_r += x[0].re;
  wT2_i = x[0].re - wT2_i;
  x[0].re = wT2_r;
  x[4].re = wT2_i;

  wT2_i = x[5].im;
  wT2_i += x[7].im;
  wT2_i += wT1_i;
  wT2_i += wB1_i;
  wT2_r = wT2_i;
  wT2_r += x[0].im;
  wT2_i = x[0].im - wT2_i;
  x[0].im = wT2_r;
  x[4].im = wT2_i;
  
  /* x[2] x[6] */
  wT2_r = x[5].im;
  wT2_r -= x[7].im;
  wT2_r += wT1_i;
  wT2_r -= wB1_i;
  wT2_i = wT2_r;
  wT2_r += x[2].re;
  wT2_i = x[2].re - wT2_i;
  x[2].re = wT2_r;
  x[6].re = wT2_i;

  wT2_i = x[5].re;
  wT2_i -= x[7].re;
  wT2_i += wT1_r;
  wT2_i -= wB1_r;
  wT2_r = wT2_i;
  wT2_r += x[2].im;
  wT2_i = x[2].im - wT2_i;
  x[2].im = wT2_i;
  x[6].im = wT2_r;
  

  /* x[1] x[5] */
  wT2_r = wT1_r;
  wT2_r += wB1_i;
  wT2_r -= x[5].re;
  wT2_r -= x[7].im;
  wT2_i = wT1_i;
  wT2_i -= wB1_r;
  wT2_i -= x[5].im;
  wT2_i += x[7].re;

  wB2_r = wT2_r;
  wB2_r += wT2_i;
  wT2_i -= wT2_r;
  wB2_r *= HSQRT2;
  wT2_i *= HSQRT2;
  wT2_r = wB2_r;
  wB2_r += x[1].re;
  wT2_r =  x[1].re - wT2_r;

  wB2_i = x[5].re;
  x[1].re = wB2_r;
  x[5].re = wT2_r;

  wT2_r = wT2_i;
  wT2_r += x[1].im;
  wT2_i = x[1].im - wT2_i;
  wB2_r = x[5].im;
  x[1].im = wT2_r;
  x[5].im = wT2_i;

  /* x[3] x[7] */
  wT1_r -= wB1_i;
  wT1_i += wB1_r;
  wB1_r = wB2_i - x[7].im;
  wB1_i = wB2_r + x[7].re;
  wT1_r -= wB1_r;
  wT1_i -= wB1_i;
  wB1_r = wT1_r + wT1_i;
  wB1_r *= HSQRT2;
  wT1_i -= wT1_r;
  wT1_i *= HSQRT2;
  wB2_r = x[3].re;
  wB2_i = wB2_r + wT1_i;
  wB2_r -= wT1_i;
  x[3].re = wB2_i;
  x[7].re = wB2_r;
  wB2_i = x[3].im;
  wB2_r = wB2_i + wB1_r;
  wB2_i -= wB1_r;
  x[3].im = wB2_i;
  x[7].im = wB2_r;
}


void fft_asmb(int k, complex_t *x, complex_t *wTB,
	     const complex_t *d, const complex_t *d_3)
{
  register complex_t  *x2k, *x3k, *x4k, *wB;
  register float a_r, a_i, a1_r, a1_i, u_r, u_i, v_r, v_i;

  x2k = x + 2 * k;
  x3k = x2k + 2 * k;
  x4k = x3k + 2 * k;
  wB = wTB + 2 * k;
  
  TRANSZERO(x[0],x2k[0],x3k[0],x4k[0]);
  TRANS(x[1],x2k[1],x3k[1],x4k[1],wTB[1],wB[1],d[1],d_3[1]);
  
  --k;
  for(;;) {
     TRANS(x[2],x2k[2],x3k[2],x4k[2],wTB[2],wB[2],d[2],d_3[2]);
     TRANS(x[3],x2k[3],x3k[3],x4k[3],wTB[3],wB[3],d[3],d_3[3]);
     if (!--k) break;
     x += 2;
     x2k += 2;
     x3k += 2;
     x4k += 2;
     d += 2;
     d_3 += 2;
     wTB += 2;
     wB += 2;
  }
 
}

void fft_asmb16(complex_t *x, complex_t *wTB)
{
  register float a_r, a_i, a1_r, a1_i, u_r, u_i, v_r, v_i;
  int k = 2;

  /* transform x[0], x[8], x[4], x[12] */
  TRANSZERO(x[0],x[4],x[8],x[12]);

  /* transform x[1], x[9], x[5], x[13] */
  TRANS(x[1],x[5],x[9],x[13],wTB[1],wTB[5],delta16[1],delta16_3[1]);

  /* transform x[2], x[10], x[6], x[14] */
  TRANSHALF_16(x[2],x[6],x[10],x[14]);

  /* transform x[3], x[11], x[7], x[15] */
  TRANS(x[3],x[7],x[11],x[15],wTB[3],wTB[7],delta16[3],delta16_3[3]);

} 


void fft_64p_c (complex_t *a)
{
  fft_8(&a[0]); fft_4(&a[8]); fft_4(&a[12]);
  fft_asmb16(&a[0], &a[8]);
  
  fft_8(&a[16]), fft_8(&a[24]);
  fft_asmb(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

  fft_8(&a[32]); fft_4(&a[40]); fft_4(&a[44]);
  fft_asmb16(&a[32], &a[40]);

  fft_8(&a[48]); fft_4(&a[56]); fft_4(&a[60]);
  fft_asmb16(&a[48], &a[56]);

  fft_asmb(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);
}


void fft_128p_c (complex_t *a)
{
  fft_8(&a[0]); fft_4(&a[8]); fft_4(&a[12]);
  fft_asmb16(&a[0], &a[8]);
  
  fft_8(&a[16]), fft_8(&a[24]);
  fft_asmb(4, &a[0], &a[16],&delta32[0], &delta32_3[0]);

  fft_8(&a[32]); fft_4(&a[40]); fft_4(&a[44]);
  fft_asmb16(&a[32], &a[40]);

  fft_8(&a[48]); fft_4(&a[56]); fft_4(&a[60]);
  fft_asmb16(&a[48], &a[56]);

  fft_asmb(8, &a[0], &a[32],&delta64[0], &delta64_3[0]);

  fft_8(&a[64]); fft_4(&a[72]); fft_4(&a[76]);
  /* fft_16(&a[64]); */
  fft_asmb16(&a[64], &a[72]);

  fft_8(&a[80]); fft_8(&a[88]);
  
  /* fft_32(&a[64]); */
  fft_asmb(4, &a[64], &a[80],&delta32[0], &delta32_3[0]);

  fft_8(&a[96]); fft_4(&a[104]), fft_4(&a[108]);
  /* fft_16(&a[96]); */
  fft_asmb16(&a[96], &a[104]);

  fft_8(&a[112]), fft_8(&a[120]);
  /* fft_32(&a[96]); */
  fft_asmb(4, &a[96], &a[112], &delta32[0], &delta32_3[0]);
  
  /* fft_128(&a[0]); */
  fft_asmb(16, &a[0], &a[64], &delta128[0], &delta128_3[0]);
}
