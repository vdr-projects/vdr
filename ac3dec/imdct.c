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
#include "imdct.h"
#include "imdct_c.h"
#ifdef HAVE_KNI
#include "imdct_kni.h"
#endif
#include "srfft.h"


extern void (*downmix_3f_2r_to_2ch)(float *samples, dm_par_t * dm_par);
extern void (*downmix_3f_1r_to_2ch)(float *samples, dm_par_t * dm_par);
extern void (*downmix_2f_2r_to_2ch)(float *samples, dm_par_t * dm_par);
extern void (*downmix_2f_1r_to_2ch)(float *samples, dm_par_t * dm_par);
extern void (*downmix_3f_0r_to_2ch)(float *samples, dm_par_t * dm_par);


extern void (*stream_sample_2ch_to_s16)(int16_t *s16_samples, float *left, float *right);
extern void (*stream_sample_1ch_to_s16)(int16_t *s16_samples, float *center);

void (*fft_64p) (complex_t *);
void (*imdct_do_512) (float data[],float delay[]);
void (*imdct_do_512_nol) (float data[], float delay[]);

void imdct_do_256 (float data[],float delay[]);

#define N 512

/* static complex_t buf[128]; */
//static complex_t buf[128] __attribute__((aligned(16)));
complex_t buf[128] __attribute__((aligned(16)));

/* Delay buffer for time domain interleaving */
static float delay[6][256];
static float delay1[6][256];

/* Twiddle factors for IMDCT */
static float xcos2[64];
static float xsin2[64];

/* Windowing function for Modified DCT - Thank you acroread */
//static float window[] = {
float window[] = {
	0.00014, 0.00024, 0.00037, 0.00051, 0.00067, 0.00086, 0.00107, 0.00130,
	0.00157, 0.00187, 0.00220, 0.00256, 0.00297, 0.00341, 0.00390, 0.00443,
	0.00501, 0.00564, 0.00632, 0.00706, 0.00785, 0.00871, 0.00962, 0.01061,
	0.01166, 0.01279, 0.01399, 0.01526, 0.01662, 0.01806, 0.01959, 0.02121,
	0.02292, 0.02472, 0.02662, 0.02863, 0.03073, 0.03294, 0.03527, 0.03770,
	0.04025, 0.04292, 0.04571, 0.04862, 0.05165, 0.05481, 0.05810, 0.06153,
	0.06508, 0.06878, 0.07261, 0.07658, 0.08069, 0.08495, 0.08935, 0.09389,
	0.09859, 0.10343, 0.10842, 0.11356, 0.11885, 0.12429, 0.12988, 0.13563,
	0.14152, 0.14757, 0.15376, 0.16011, 0.16661, 0.17325, 0.18005, 0.18699,
	0.19407, 0.20130, 0.20867, 0.21618, 0.22382, 0.23161, 0.23952, 0.24757,
	0.25574, 0.26404, 0.27246, 0.28100, 0.28965, 0.29841, 0.30729, 0.31626,
	0.32533, 0.33450, 0.34376, 0.35311, 0.36253, 0.37204, 0.38161, 0.39126,
	0.40096, 0.41072, 0.42054, 0.43040, 0.44030, 0.45023, 0.46020, 0.47019,
	0.48020, 0.49022, 0.50025, 0.51028, 0.52031, 0.53033, 0.54033, 0.55031,
	0.56026, 0.57019, 0.58007, 0.58991, 0.59970, 0.60944, 0.61912, 0.62873,
	0.63827, 0.64774, 0.65713, 0.66643, 0.67564, 0.68476, 0.69377, 0.70269,
	0.71150, 0.72019, 0.72877, 0.73723, 0.74557, 0.75378, 0.76186, 0.76981,
	0.77762, 0.78530, 0.79283, 0.80022, 0.80747, 0.81457, 0.82151, 0.82831,
	0.83496, 0.84145, 0.84779, 0.85398, 0.86001, 0.86588, 0.87160, 0.87716,
	0.88257, 0.88782, 0.89291, 0.89785, 0.90264, 0.90728, 0.91176, 0.91610,
	0.92028, 0.92432, 0.92822, 0.93197, 0.93558, 0.93906, 0.94240, 0.94560,
	0.94867, 0.95162, 0.95444, 0.95713, 0.95971, 0.96217, 0.96451, 0.96674,
	0.96887, 0.97089, 0.97281, 0.97463, 0.97635, 0.97799, 0.97953, 0.98099,
	0.98236, 0.98366, 0.98488, 0.98602, 0.98710, 0.98811, 0.98905, 0.98994,
	0.99076, 0.99153, 0.99225, 0.99291, 0.99353, 0.99411, 0.99464, 0.99513,
	0.99558, 0.99600, 0.99639, 0.99674, 0.99706, 0.99736, 0.99763, 0.99788,
	0.99811, 0.99831, 0.99850, 0.99867, 0.99882, 0.99895, 0.99908, 0.99919,
	0.99929, 0.99938, 0.99946, 0.99953, 0.99959, 0.99965, 0.99969, 0.99974,
	0.99978, 0.99981, 0.99984, 0.99986, 0.99988, 0.99990, 0.99992, 0.99993,
	0.99994, 0.99995, 0.99996, 0.99997, 0.99998, 0.99998, 0.99998, 0.99999,
	0.99999, 0.99999, 0.99999, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000,
	1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000, 1.00000
};

//static const int pm128[128] =
const int pm128[128] =
{
	0, 16, 32, 48, 64, 80,  96, 112,  8, 40, 72, 104, 24, 56,  88, 120,
	4, 20, 36, 52, 68, 84, 100, 116, 12, 28, 44,  60, 76, 92, 108, 124,
	2, 18, 34, 50, 66, 82,  98, 114, 10, 42, 74, 106, 26, 58,  90, 122,
	6, 22, 38, 54, 70, 86, 102, 118, 14, 46, 78, 110, 30, 62,  94, 126,
	1, 17, 33, 49, 65, 81,  97, 113,  9, 41, 73, 105, 25, 57,  89, 121,
	5, 21, 37, 53, 69, 85, 101, 117, 13, 29, 45,  61, 77, 93, 109, 125,
	3, 19, 35, 51, 67, 83,  99, 115, 11, 43, 75, 107, 27, 59,  91, 123,
	7, 23, 39, 55, 71, 87, 103, 119, 15, 31, 47,  63, 79, 95, 111, 127
}; 

static const int pm64[64] =
{
	0,  8, 16, 24, 32, 40, 48, 56,
	4, 20, 36, 52, 12, 28, 44, 60,
	2, 10, 18, 26, 34, 42, 50, 58,
	6, 14, 22, 30, 38, 46, 54, 62,
	1,  9, 17, 25, 33, 41, 49, 57,
	5, 21, 37, 53, 13, 29, 45, 61,
	3, 11, 19, 27, 35, 43, 51, 59,
	7, 23, 39, 55, 15, 31, 47, 63
};


void imdct_init (void)
 {
	int i;
	float scale = 255.99609372;

#ifdef __i386__
#ifdef HAVE_KNI
	if (!imdct_init_kni ());
	else
#endif
#endif
	if (!imdct_init_c ());

	// More twiddle factors to turn IFFT into IMDCT */
	for (i=0; i < 64; i++) {
		xcos2[i] = cos(2.0f * M_PI * (8*i+1)/(4*N)) * scale;
		xsin2[i] = sin(2.0f * M_PI * (8*i+1)/(4*N)) * scale;
	}
}


void imdct_do_256 (float data[],float delay[])
{
	int i, j, k;
	int p, q;

	float tmp_a_i;
	float tmp_a_r;

	float *data_ptr;
	float *delay_ptr;
	float *window_ptr;

	complex_t *buf1, *buf2;

	buf1 = &buf[0];
	buf2 = &buf[64];

// Pre IFFT complex multiply plus IFFT complex conjugate
	for (k=0; k<64; k++) { 
		/* X1[k] = X[2*k]	*/
		/* X2[k] = X[2*k+1]	*/

		j = pm64[k];
		p = 2 * (128-2*j-1);
		q = 2 * (2 * j);

		/* Z1[k] = (X1[128-2*k-1] + j * X1[2*k]) * (xcos2[k] + j * xsin2[k]); */
		buf1[k].re =         data[p] * xcos2[j] - data[q] * xsin2[j];
		buf1[k].im = -1.0f * (data[q] * xcos2[j] + data[p] * xsin2[j]);
		/* Z2[k] = (X2[128-2*k-1] + j * X2[2*k]) * (xcos2[k] + j * xsin2[k]); */
		buf2[k].re =          data[p + 1] * xcos2[j] - data[q + 1] * xsin2[j];
		buf2[k].im = -1.0f * ( data[q + 1] * xcos2[j] + data[p + 1] * xsin2[j]);
	}

	fft_64p(&buf1[0]);
	fft_64p(&buf2[0]);

#ifdef DEBUG
       //DEBUG FFT
#if 0
	printf ("Post FFT, buf1\n");
	for (i=0; i < 64; i++)
		printf("%d %f %f\n", i, buf_1[i].re, buf_1[i].im);
	printf ("Post FFT, buf2\n");
	for (i=0; i < 64; i++)
		printf("%d %f %f\n", i, buf_2[i].re, buf_2[i].im);
#endif
#endif


	// Post IFFT complex multiply 
	for( i=0; i < 64; i++) {
		tmp_a_r =  buf1[i].re;
		tmp_a_i = -buf1[i].im;
		buf1[i].re =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
		buf1[i].im =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
		tmp_a_r =  buf2[i].re;
		tmp_a_i = -buf2[i].im;
		buf2[i].re =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
		buf2[i].im =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
	}
	
	data_ptr = data;
	delay_ptr = delay;
	window_ptr = window;

	/* Window and convert to real valued signal */
	for(i=0; i< 64; i++) { 
		*data_ptr++  = -buf1[i].im      * *window_ptr++ + *delay_ptr++;
		*data_ptr++  = buf1[64-i-1].re * *window_ptr++ + *delay_ptr++;
	}

	for(i=0; i< 64; i++) {
		*data_ptr++  = -buf1[i].re      * *window_ptr++ + *delay_ptr++;
		*data_ptr++  = buf1[64-i-1].im * *window_ptr++ + *delay_ptr++;
	}
	
	delay_ptr = delay;

	for(i=0; i< 64; i++) {
		*delay_ptr++ = -buf2[i].re      * *--window_ptr;
		*delay_ptr++ =  buf2[64-i-1].im * *--window_ptr;
	}

	for(i=0; i< 64; i++) {
		*delay_ptr++ =  buf2[i].im      * *--window_ptr;
		*delay_ptr++ = -buf2[64-i-1].re * *--window_ptr;
	}
}


/**
 *
 **/

void imdct_do_256_nol (float data[], float delay[])
{
	int i, j, k;
	int p, q;

	float tmp_a_i;
	float tmp_a_r;

	float *data_ptr;
	float *delay_ptr;
	float *window_ptr;

	complex_t *buf1, *buf2;

	buf1 = &buf[0];
	buf2 = &buf[64];

       /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
	for(k=0; k<64; k++) {
               /* X1[k] = X[2*k]  */
               /* X2[k] = X[2*k+1]     */
               j = pm64[k];
               p = 2 * (128-2*j-1);
               q = 2 * (2 * j);

               /* Z1[k] = (X1[128-2*k-1] + j * X1[2*k]) * (xcos2[k] + j * xsin2[k]); */
               buf1[k].re =         data[p] * xcos2[j] - data[q] * xsin2[j];
               buf1[k].im = -1.0f * (data[q] * xcos2[j] + data[p] * xsin2[j]);
               /* Z2[k] = (X2[128-2*k-1] + j * X2[2*k]) * (xcos2[k] + j * xsin2[k]); */
               buf2[k].re =          data[p + 1] * xcos2[j] - data[q + 1] * xsin2[j];
               buf2[k].im = -1.0f * ( data[q + 1] * xcos2[j] + data[p + 1] * xsin2[j]);
       }


       fft_64p(&buf1[0]);
       fft_64p(&buf2[0]);

#ifdef DEBUG
	//DEBUG FFT
#if 0
       printf("Post FFT, buf1\n");
       for (i=0; i < 64; i++)
               printf("%d %f %f\n", i, buf_1[i].re, buf_1[i].im);
       printf("Post FFT, buf2\n");
       for (i=0; i < 64; i++)
               printf("%d %f %f\n", i, buf_2[i].re, buf_2[i].im);
#endif
#endif

       /* Post IFFT complex multiply */
       for( i=0; i < 64; i++) {
               /* y1[n] = z1[n] * (xcos2[n] + j * xs in2[n]) ; */
               tmp_a_r =  buf1[i].re;
               tmp_a_i = -buf1[i].im;
               buf1[i].re =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
               buf1[i].im =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
               /* y2[n] = z2[n] * (xcos2[n] + j * xsin2[n]) ; */
               tmp_a_r =  buf2[i].re;
               tmp_a_i = -buf2[i].im;
               buf2[i].re =(tmp_a_r * xcos2[i])  -  (tmp_a_i  * xsin2[i]);
               buf2[i].im =(tmp_a_r * xsin2[i])  +  (tmp_a_i  * xcos2[i]);
       }
      
       data_ptr = data;
       delay_ptr = delay;
       window_ptr = window;

       /* Window and convert to real valued signal, no overlap */
       for(i=0; i< 64; i++) {
               *data_ptr++  = -buf1[i].im      * *window_ptr++;
               *data_ptr++  = buf1[64-i-1].re * *window_ptr++;
       }

       for(i=0; i< 64; i++) {
               *data_ptr++  = -buf1[i].re      * *window_ptr++ + *delay_ptr++;
               *data_ptr++  = buf1[64-i-1].im * *window_ptr++ + *delay_ptr++;
       }

       delay_ptr = delay;

       for(i=0; i< 64; i++) {
               *delay_ptr++ = -buf2[i].re      * *--window_ptr;
               *delay_ptr++ =  buf2[64-i-1].im * *--window_ptr;
       }

       for(i=0; i< 64; i++) {
               *delay_ptr++ =  buf2[i].im      * *--window_ptr;
               *delay_ptr++ = -buf2[64-i-1].re * *--window_ptr;
	}
}

//FIXME remove - for timing code
///#include <sys/time.h>
//FIXME remove

void imdct (bsi_t *bsi,audblk_t *audblk, stream_samples_t samples, int16_t *s16_samples, dm_par_t* dm_par)
{
	int i;
	int doable = 0;
	float *center=NULL, *left, *right, *left_sur, *right_sur;
	float *delay_left, *delay_right;
	float *delay1_left, *delay1_right, *delay1_center, *delay1_sr, *delay1_sl;
	float right_tmp, left_tmp;
	void (*do_imdct)(float data[], float deley[]);

	// test if dm in frequency is doable
	if (!(doable = audblk->blksw[0]))
		do_imdct = imdct_do_512;
	else
		do_imdct = imdct_do_256;

	// downmix in the frequency domain if all the channels
	// use the same imdct
	for (i=0; i < bsi->nfchans; i++) {
		if (doable != audblk->blksw[i]) {
			do_imdct = NULL;
			break;
		}
	}

	if (do_imdct) {
		//dowmix first and imdct
		switch(bsi->acmod) {
		case 7:		// 3/2
			downmix_3f_2r_to_2ch (samples[0], dm_par);
			break;
		case 6:		// 2/2
			downmix_2f_2r_to_2ch (samples[0], dm_par);
			break;
		case 5:		// 3/1
			downmix_3f_1r_to_2ch (samples[0], dm_par);
			break;
		case 4:		// 2/1
			downmix_2f_1r_to_2ch (samples[0], dm_par);
			break;
		case 3:		// 3/0
			downmix_3f_0r_to_2ch (samples[0], dm_par);
			break;
		case 2:
			break;
		default:	// 1/0
			if (bsi->acmod == 1)
				center = samples[0];
			else if (bsi->acmod == 0)
				center = samples[ac3_config.dual_mono_ch_sel];
			do_imdct(center, delay[0]); // no downmix

			stream_sample_1ch_to_s16 (s16_samples, center);

			return;
			//goto done;
			break;
		}

		do_imdct (samples[0], delay[0]);
		do_imdct (samples[1], delay[1]);
		stream_sample_2ch_to_s16(s16_samples, samples[0], samples[1]);

	} else {		//imdct and then dowmix
		// delay and samples should be saved and mixed
		//fprintf(stderr, "time domain downmix\n");
		for (i=0; i<bsi->nfchans; i++) {
			if (audblk->blksw[i])
				imdct_do_256_nol (samples[i],delay1[i]);
			else
				imdct_do_512_nol (samples[i],delay1[i]);
		}

		// mix the sample, overlap
		switch(bsi->acmod) {
		case 7:		// 3/2
			left = samples[0];
			center = samples[1];
			right = samples[2];
			left_sur = samples[3];
			right_sur = samples[4];
			delay_left = delay[0];
			delay_right = delay[1];
			delay1_left = delay1[0];
			delay1_center = delay1[1];
			delay1_right = delay1[2];
			delay1_sl = delay1[3];
			delay1_sr = delay1[4];

			for (i = 0; i < 256; i++) {
				left_tmp = dm_par->unit * *left++  + dm_par->clev * *center  + dm_par->slev * *left_sur++;
				right_tmp= dm_par->unit * *right++ + dm_par->clev * *center++ + dm_par->slev * *right_sur++;
				*s16_samples++ = (int16_t)(left_tmp + *delay_left);
				*s16_samples++ = (int16_t)(right_tmp + *delay_right);
				*delay_left++ = dm_par->unit * *delay1_left++  + dm_par->clev * *delay1_center  + dm_par->slev * *delay1_sl++;
				*delay_right++ = dm_par->unit * *delay1_right++ + dm_par->clev * *center++ + dm_par->slev * *delay1_sr++;
			}
			break;
		case 6:		// 2/2
			left = samples[0];
			right = samples[1];
			left_sur = samples[2];
			right_sur = samples[3];
			delay_left = delay[0];
			delay_right = delay[1];
			delay1_left = delay1[0];
			delay1_right = delay1[1];
			delay1_sl = delay1[2];
			delay1_sr = delay1[3];

			for (i = 0; i < 256; i++) {
				left_tmp = dm_par->unit * *left++  + dm_par->slev * *left_sur++;
				right_tmp= dm_par->unit * *right++ + dm_par->slev * *right_sur++;
				*s16_samples++ = (int16_t)(left_tmp + *delay_left);
				*s16_samples++ = (int16_t)(right_tmp + *delay_right);
				*delay_left++ = dm_par->unit * *delay1_left++  + dm_par->slev * *delay1_sl++;
				*delay_right++ = dm_par->unit * *delay1_right++ + dm_par->slev * *delay1_sr++;
			}
			break;
		case 5:		// 3/1
			left = samples[0];
			center = samples[1];
			right = samples[2];
			right_sur = samples[3];
			delay_left = delay[0];
			delay_right = delay[1];
			delay1_left = delay1[0];
			delay1_center = delay1[1];
			delay1_right = delay1[2];
			delay1_sl = delay1[3];

			for (i = 0; i < 256; i++) {
				left_tmp = dm_par->unit * *left++  + dm_par->clev * *center  - dm_par->slev * *right_sur;
				right_tmp= dm_par->unit * *right++ + dm_par->clev * *center++ + dm_par->slev * *right_sur++;
				*s16_samples++ = (int16_t)(left_tmp + *delay_left);
				*s16_samples++ = (int16_t)(right_tmp + *delay_right);
				*delay_left++ = dm_par->unit * *delay1_left++  + dm_par->clev * *delay1_center  + dm_par->slev * *delay1_sl;
				*delay_right++ = dm_par->unit * *delay1_right++ + dm_par->clev * *center++ + dm_par->slev * *delay1_sl++;
			}
			break;
		case 4:		// 2/1
			left = samples[0];
			right = samples[1];
			right_sur = samples[2];
			delay_left = delay[0];
			delay_right = delay[1];
			delay1_left = delay1[0];
			delay1_right = delay1[1];
			delay1_sl = delay1[2];

			for (i = 0; i < 256; i++) {
				left_tmp = dm_par->unit * *left++ - dm_par->slev * *right_sur;
				right_tmp= dm_par->unit * *right++ + dm_par->slev * *right_sur++;
				*s16_samples++ = (int16_t)(left_tmp + *delay_left);
				*s16_samples++ = (int16_t)(right_tmp + *delay_right);
				*delay_left++ = dm_par->unit * *delay1_left++ + dm_par->slev * *delay1_sl;
				*delay_right++ = dm_par->unit * *delay1_right++ + dm_par->slev * *delay1_sl++;
			}
			break;
		case 3:		// 3/0
			left = samples[0];
			center = samples[1];
			right = samples[2];
			delay_left = delay[0];
			delay_right = delay[1];
			delay1_left = delay1[0];
			delay1_center = delay1[1];
			delay1_right = delay1[2];

			for (i = 0; i < 256; i++) {
				left_tmp = dm_par->unit * *left++  + dm_par->clev * *center;
				right_tmp= dm_par->unit * *right++ + dm_par->clev * *center++;
				*s16_samples++ = (int16_t)(left_tmp + *delay_left);
				*s16_samples++ = (int16_t)(right_tmp + *delay_right);
				*delay_left++ = dm_par->unit * *delay1_left++  + dm_par->clev * *delay1_center;
				*delay_right++ = dm_par->unit * *delay1_right++ + dm_par->clev * *center++;
			}
			break;
		case 2:		// copy to output
			for (i = 0; i < 256; i++) {
				*s16_samples++ = (int16_t)samples[0][i];
				*s16_samples++ = (int16_t)samples[1][i];
			}
			break;
		}
	}
}
