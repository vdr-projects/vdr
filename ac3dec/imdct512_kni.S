/* 
 *  imdct512_kni.S
 *
 *  Copyright (C) Yuqing Deng <Yuqing_Deng@brown.edu> - October 2000
 *
 *
 *  imdct512_kni.S is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  imdct512_kni.S is distributed in the hope that it will be useful,
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

.text
	.align 4
.global imdct512_pre_ifft_twiddle_kni
	.type imdct512_pre_ifft_twiddle_kni, @function
imdct512_pre_ifft_twiddle_kni:
	
	pushl %ebp
	movl  %esp, %ebp
	addl  $-4, %esp /* local variable, loop counter */
	
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %edi
	pushl %esi

	movl 8(%ebp), %eax  	/* pmt */
	movl 12(%ebp), %ebx	/* buf */
	movl 16(%ebp), %ecx	/* data */
	movl 20(%ebp), %edx 	/* xcos_sin_sse */
	movl $64, -4(%ebp)
	
	
.loop:
	movl (%eax), %esi
	movl 4(%eax), %edi
	movss (%ecx, %esi, 8), %xmm1 /* 2j */
	movss (%ecx, %edi, 8), %xmm3 /* 2(j+1) */

	shll $1, %esi
	shll $1, %edi

	movaps (%edx, %esi, 8), %xmm0; /* -c_j | -s_j | -s_j | c_j */
	movaps (%edx, %edi, 8), %xmm2; /* -c_j+1 | -s_j+1 | -s_j+1 | c_j+1 */

	negl %esi
	negl %edi

	movss 1020(%ecx, %esi, 4), %xmm4 /* 255-2j */
	addl $8, %eax
	movss 1020(%ecx, %edi, 4), %xmm5 /* 255-2(j+1) */

	shufps $0, %xmm1, %xmm4 /* 2j | 2j | 255-2j | 255-2j */
	shufps $0, %xmm3, %xmm5 /* 2(j+1) | 2(j+1) | 255-2(j+1) | 255-2(j+1) */
	mulps %xmm4, %xmm0
	mulps %xmm5, %xmm2
	movhlps %xmm0, %xmm1
	movhlps %xmm2, %xmm3
	addl $16, %ebx
	addps %xmm1, %xmm0
	addps %xmm3, %xmm2
	movlhps %xmm2, %xmm0
	movaps %xmm0, -16(%ebx)
	decl -4(%ebp)
     	jnz .loop

	popl %esi
	popl %edi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax

	addl $4, %esp
	popl %ebp

	ret
	.p2align 4,0

.global imdct512_post_ifft_twiddle_kni
	.type imdct512_post_ifft_twiddle_kni, @function
imdct512_post_ifft_twiddle_kni:
	
	pushl %ebp
	movl  %esp, %ebp
	
	pushl %eax
	pushl %ebx
	pushl %ecx

	movl 8(%ebp), %eax /* buf[] */
	movl 12(%ebp), %ebx /* xcos_sin_sse[] */
	movl $32, %ecx /* loop counter */

.loop1:
	movaps	(%eax), %xmm0  /*  im1 | re1 | im0 | re0 */

	movaps  (%ebx), %xmm2  /* -c | -s | -s | c */
	movhlps  %xmm0, %xmm1  /* im1 | re1 */
	movaps  16(%ebx), %xmm3  /* -c1 | -s1 | -s1 | c1 */

	shufps $0x50, %xmm0, %xmm0 /* im0 | im0 | re0 | re0 */
	shufps $0x50, %xmm1, %xmm1 /* im1 | im1 | re1 | re1 */

	movaps  16(%eax), %xmm4 /* im3 | re3 | im2 | re2 */

	shufps $0x27, %xmm2, %xmm2 /* c | -s | -s | -c */
	movhlps  %xmm4, %xmm5 /* im3 | re3 */
	shufps $0x27, %xmm3, %xmm3 /* c1 | -s1 | -s1 | -c1 */

	movaps  32(%ebx), %xmm6   /* -c2 | -s2 | -s2 | c2 */
	movaps  48(%ebx), %xmm7   /* -c3 | -s3 | -s3 | c3 */

	shufps $0x50, %xmm4, %xmm4 /* im2 | im2 | re2 | re2 */
	shufps $0x50, %xmm5, %xmm5 /* im3 | im3 | re3 | re3 */

	mulps %xmm2, %xmm0
	mulps %xmm3, %xmm1

	shufps $0x27, %xmm6, %xmm6 /* c2 | -s2 | -s2 | -c2 */
	shufps $0x27, %xmm7, %xmm7 /* c3 | -s3 | -s3 | -c3 */

	movhlps %xmm0, %xmm2
	movhlps %xmm1, %xmm3

	mulps %xmm6, %xmm4
	mulps %xmm7, %xmm5

	addps %xmm2, %xmm0
	addps %xmm3, %xmm1

	movhlps %xmm4, %xmm6
	movhlps %xmm5, %xmm7

	addps %xmm6, %xmm4
	addps %xmm7, %xmm5

	movlhps %xmm1, %xmm0
	movlhps %xmm5, %xmm4

	movaps %xmm0, (%eax)
	movaps %xmm4, 16(%eax)
	addl $64, %ebx
	addl $32, %eax
	decl %ecx
	jnz .loop1

	popl %ecx
	popl %ebx
	popl %eax

	leave
	ret
	.p2align 4,0

.global imdct512_window_delay_kni
	.type imdct512_window_delay_kni, @function
imdct512_window_delay_kni:
	
	pushl %ebp
	movl  %esp, %ebp
	
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi

	movl 20(%ebp), %ebx /* delay */
	movl 16(%ebp), %edx /* window */

	movl 8(%ebp), %eax  /* buf */
	movl $16, %ecx /* loop count */
	leal 516(%eax), %esi /* buf[64].im */
	leal 504(%eax), %edi /* buf[63].re */ 
	movl 12(%ebp), %eax /* data */
.first_128_samples:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | im1 | 0.0 | im0 */
	movlhps %xmm3, %xmm1 /* 0.0 | re1 | 0.0 | re0 */

	movaps (%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	movaps (%ebx), %xmm5 /* d3 | d2 | d1 | d0 */
	shufps $0xb1, %xmm1, %xmm1 /* re1 | 0.0 | re0 | 0.0 */

	movss 16(%esi), %xmm6 /* im2 */
	movss 24(%esi), %xmm7 /* im3 */
	subps %xmm1, %xmm0  /* -re1 | im1 | -re0 | im0 */
	movss -16(%edi), %xmm2 /* re2 */
	movss -24(%edi), %xmm3 /* re3 */
	mulps %xmm4, %xmm0
	movlhps %xmm7, %xmm6 /* 0.0 | im3 | 0.0 | im2 */
	movlhps %xmm3, %xmm2 /* 0.0 | re3 | 0.0 | re2 */
	addps %xmm5, %xmm0
	shufps $0xb1, %xmm2, %xmm2 /* re3 | 0.0 | re2 | 0.0 */
	movaps 16(%edx), %xmm4 /* w7 | w6 | w5 | w4 */
	movaps 16(%ebx), %xmm5 /* d7 | d6 | d5 | d4 */
	subps %xmm2, %xmm6 /* -re3 | im3 | -re2 | im2 */
	addl $32, %edx
	movaps %xmm0, (%eax)
	addl $32, %ebx
	mulps %xmm4, %xmm6
	addl $32, %esi
	addl $32, %eax
	addps %xmm5, %xmm6
	addl $-32, %edi
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .first_128_samples

	movl 8(%ebp), %esi  /* buf[0].re */
	leal 1020(%esi), %edi /* buf[127].im */ 
	movl $16, %ecx /* loop count */
.second_128_samples:

	movss (%esi), %xmm0 /* buf[i].re */
	movss 8(%esi), %xmm2 /* re1 */
	movss (%edi), %xmm1 /* buf[127-i].im */
	movss -8(%edi), %xmm3 /* im1 */

	movlhps %xmm2, %xmm0 /* 0.0 | re1 | 0.0 | re0 */
	movlhps %xmm3, %xmm1 /* 0.0 | im1 | 0.0 | im1 */
	
	movaps (%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	movaps (%ebx), %xmm5 /* d3 | d2 | d1 | d0 */

	shufps $0xb1, %xmm1, %xmm1 /* im1 | 0.0 | im0 | 0.0 */
	movss 16(%esi), %xmm6 /* re2 */
	movss 24(%esi), %xmm7 /* re3 */
	movss -16(%edi), %xmm2 /* im2 */
	movss -24(%edi), %xmm3 /* im3 */
	subps %xmm1, %xmm0 /* -im1 | re1 | -im0 | re0 */
	movlhps %xmm7, %xmm6 /* 0.0 | re3 | 0.0 | re2 */
	movlhps %xmm3, %xmm2 /* 0.0 | im3 | 0.0 | im2 */
	mulps %xmm4, %xmm0
	shufps $0xb1, %xmm2, %xmm2 /* im3 | 0.0 | im2 | 0.0 */
	movaps 16(%edx), %xmm4 /* w7 | w6 | w5 | w4 */
	addl $32, %esi
	subps %xmm2, %xmm6 /* -im3 | re3 | -im2 | re2 */
	addps %xmm5, %xmm0
	mulps %xmm4, %xmm6
	addl $-32, %edi
	movaps 16(%ebx), %xmm5 /* d7 | d6 | d5 | d4 */
	movaps %xmm0, (%eax)
	addps %xmm5, %xmm6
	addl $32, %edx
	addl $32, %eax
	addl $32, %ebx
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .second_128_samples

	movl 8(%ebp), %eax
	leal 512(%eax), %esi /* buf[64].re */
	leal 508(%eax), %edi /* buf[63].im */ 
	movl $16, %ecx /* loop count */
	movl 20(%ebp), %eax /* delay */
.first_128_delay:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | re1 | 0.0 | re0 */
	movlhps %xmm3, %xmm1 /* 0.0 | im1 | 0.0 | im0 */

	movaps -16(%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	shufps $0xb1, %xmm1, %xmm1 /* im1 | 0.0 | im0 | 0.0 */
	movss 16(%esi), %xmm6 /* re2 */
	movss 24(%esi), %xmm7 /* re3 */
	movss -16(%edi), %xmm2 /* im2 */
	movss -24(%edi), %xmm3 /* im3 */
	subps %xmm1, %xmm0 /* -im1 | re1 | -im0 | re0 */
	addl $-32, %edx
	movlhps %xmm7, %xmm6 /* 0.0 | re3 | 0.0 | re2 */
	movlhps %xmm3, %xmm2 /* 0.0 | im3 | 0.0 | im2 */
	mulps %xmm4, %xmm0
	movaps (%edx), %xmm5 /* w7 | w6 | w5 | w4 */
	shufps $0xb1, %xmm2, %xmm2 /* im3 | 0.0 | im2 | 0.0 */
	movaps %xmm0, (%eax)
	addl $32, %esi
	subps %xmm2, %xmm6 /* -im3 | re3 | -im2 | re2 */
	addl $-32, %edi
	mulps %xmm5, %xmm6
	addl $32, %eax
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .first_128_delay

	movl 8(%ebp), %ebx
	leal 4(%ebx), %esi /* buf[0].im */
	leal 1016(%ebx), %edi /* buf[127].re */ 
	movl $16, %ecx /* loop count */
.second_128_delay:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | im1 | 0.0 | im0 */
	movlhps %xmm3, %xmm1 /* 0.0 | re1 | 0.0 | re0 */

	movaps -16(%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	shufps $0xb1, %xmm1, %xmm1 /* re1 | 0.0 | re0 | 0.0 */
	movss 16(%esi), %xmm6 /* im2 */
	movss 24(%esi), %xmm7 /* im3 */
	movss -16(%edi), %xmm2 /* re2 */
	movss -24(%edi), %xmm3 /* re3 */
	subps %xmm0, %xmm1 /* re1 | -im1 | re0 | -im0 */
	addl $-32, %edx
	movlhps %xmm7, %xmm6 /* 0.0 | im3 | 0.0 | im2 */
	movlhps %xmm3, %xmm2 /* 0.0 | re3 | 0.0 | re2 */
	mulps %xmm4, %xmm1
	movaps (%edx), %xmm5 /* w7 | w6 | w5 | w4 */
	shufps $0xb1, %xmm2, %xmm2 /* re3 | 0.0 | re2 | 0.0 */
	movaps %xmm1, (%eax)
	addl $32, %esi
	subps %xmm6, %xmm2 /* re | -im3 | re | -im2 */
	addl $-32, %edi
	mulps %xmm5, %xmm2
	addl $32, %eax
	movaps %xmm2, -16(%eax)
	decl %ecx
	jnz .second_128_delay

	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	
	leave
	ret
	.p2align 4,0
	
.global imdct512_window_delay_nol_kni
	.type imdct512_window_delay_nol_kni, @function
imdct512_window_delay_nol_kni:
	
	pushl %ebp
	movl  %esp, %ebp
	
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi

	/* movl 20(%ebp), %ebx delay */
	movl 16(%ebp), %edx /* window */

	movl 8(%ebp), %eax  /* buf */
	movl $16, %ecx /* loop count */
	leal 516(%eax), %esi /* buf[64].im */
	leal 504(%eax), %edi /* buf[63].re */ 
	movl 12(%ebp), %eax /* data */
.first_128_sample:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | im1 | 0.0 | im0 */
	movlhps %xmm3, %xmm1 /* 0.0 | re1 | 0.0 | re0 */

	movaps (%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	/* movaps (%ebx), %xmm5 d3 | d2 | d1 | d0 */
	shufps $0xb1, %xmm1, %xmm1 /* re1 | 0.0 | re0 | 0.0 */

	movss 16(%esi), %xmm6 /* im2 */
	movss 24(%esi), %xmm7 /* im3 */
	subps %xmm1, %xmm0  /* -re1 | im1 | -re0 | im0 */
	movss -16(%edi), %xmm2 /* re2 */
	movss -24(%edi), %xmm3 /* re3 */
	mulps %xmm4, %xmm0
	movlhps %xmm7, %xmm6 /* 0.0 | im3 | 0.0 | im2 */
	movlhps %xmm3, %xmm2 /* 0.0 | re3 | 0.0 | re2 */
	/* addps %xmm5, %xmm0 */
	shufps $0xb1, %xmm2, %xmm2 /* re3 | 0.0 | re2 | 0.0 */
	movaps 16(%edx), %xmm4 /* w7 | w6 | w5 | w4 */
	/* movaps 16(%ebx), %xmm5  d7 | d6 | d5 | d4 */
	subps %xmm2, %xmm6 /* -re3 | im3 | -re2 | im2 */
	addl $32, %edx
	movaps %xmm0, (%eax)
	/* addl $32, %ebx */
	mulps %xmm4, %xmm6
	addl $32, %esi
	addl $32, %eax
	/* addps %xmm5, %xmm6 */
	addl $-32, %edi
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .first_128_sample

	movl 8(%ebp), %esi  /* buf[0].re */
	leal 1020(%esi), %edi /* buf[127].im */ 
	movl $16, %ecx /* loop count */
.second_128_sample:

	movss (%esi), %xmm0 /* buf[i].re */
	movss 8(%esi), %xmm2 /* re1 */
	movss (%edi), %xmm1 /* buf[127-i].im */
	movss -8(%edi), %xmm3 /* im1 */

	movlhps %xmm2, %xmm0 /* 0.0 | re1 | 0.0 | re0 */
	movlhps %xmm3, %xmm1 /* 0.0 | im1 | 0.0 | im1 */
	
	movaps (%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	/* movaps (%ebx), %xmm5 d3 | d2 | d1 | d0 */

	shufps $0xb1, %xmm1, %xmm1 /* im1 | 0.0 | im0 | 0.0 */
	movss 16(%esi), %xmm6 /* re2 */
	movss 24(%esi), %xmm7 /* re3 */
	movss -16(%edi), %xmm2 /* im2 */
	movss -24(%edi), %xmm3 /* im3 */
	subps %xmm1, %xmm0 /* -im1 | re1 | -im0 | re0 */
	movlhps %xmm7, %xmm6 /* 0.0 | re3 | 0.0 | re2 */
	movlhps %xmm3, %xmm2 /* 0.0 | im3 | 0.0 | im2 */
	mulps %xmm4, %xmm0
	shufps $0xb1, %xmm2, %xmm2 /* im3 | 0.0 | im2 | 0.0 */
	movaps 16(%edx), %xmm4 /* w7 | w6 | w5 | w4 */
	addl $32, %esi
	subps %xmm2, %xmm6 /* -im3 | re3 | -im2 | re2 */
	/* addps %xmm5, %xmm0 */
	mulps %xmm4, %xmm6
	addl $-32, %edi
	/* movaps 16(%ebx), %xmm5  d7 | d6 | d5 | d4 */
	movaps %xmm0, (%eax)
	/* addps %xmm5, %xmm6 */
	addl $32, %edx
	addl $32, %eax
	/* addl $32, %ebx */
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .second_128_sample

	movl 8(%ebp), %eax
	leal 512(%eax), %esi /* buf[64].re */
	leal 508(%eax), %edi /* buf[63].im */ 
	movl $16, %ecx /* loop count */
	movl 20(%ebp), %eax /* delay */
.first_128_delays:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | re1 | 0.0 | re0 */
	movlhps %xmm3, %xmm1 /* 0.0 | im1 | 0.0 | im0 */

	movaps -16(%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	shufps $0xb1, %xmm1, %xmm1 /* im1 | 0.0 | im0 | 0.0 */
	movss 16(%esi), %xmm6 /* re2 */
	movss 24(%esi), %xmm7 /* re3 */
	movss -16(%edi), %xmm2 /* im2 */
	movss -24(%edi), %xmm3 /* im3 */
	subps %xmm1, %xmm0 /* -im1 | re1 | -im0 | re0 */
	addl $-32, %edx
	movlhps %xmm7, %xmm6 /* 0.0 | re3 | 0.0 | re2 */
	movlhps %xmm3, %xmm2 /* 0.0 | im3 | 0.0 | im2 */
	mulps %xmm4, %xmm0
	movaps (%edx), %xmm5 /* w7 | w6 | w5 | w4 */
	shufps $0xb1, %xmm2, %xmm2 /* im3 | 0.0 | im2 | 0.0 */
	movaps %xmm0, (%eax)
	addl $32, %esi
	subps %xmm2, %xmm6 /* -im3 | re3 | -im2 | re2 */
	addl $-32, %edi
	mulps %xmm5, %xmm6
	addl $32, %eax
	movaps %xmm6, -16(%eax)
	decl %ecx
	jnz .first_128_delays

	movl 8(%ebp), %ebx
	leal 4(%ebx), %esi /* buf[0].im */
	leal 1016(%ebx), %edi /* buf[127].re */ 
	movl $16, %ecx /* loop count */
.second_128_delays:

	movss (%esi), %xmm0
	movss 8(%esi), %xmm2
	movss (%edi), %xmm1
	movss -8(%edi), %xmm3

	movlhps %xmm2, %xmm0 /* 0.0 | im1 | 0.0 | im0 */
	movlhps %xmm3, %xmm1 /* 0.0 | re1 | 0.0 | re0 */

	movaps -16(%edx), %xmm4 /* w3 | w2 | w1 | w0 */
	shufps $0xb1, %xmm1, %xmm1 /* re1 | 0.0 | re0 | 0.0 */
	movss 16(%esi), %xmm6 /* im2 */
	movss 24(%esi), %xmm7 /* im3 */
	movss -16(%edi), %xmm2 /* re2 */
	movss -24(%edi), %xmm3 /* re3 */
	subps %xmm0, %xmm1 /* re1 | -im1 | re0 | -im0 */
	addl $-32, %edx
	movlhps %xmm7, %xmm6 /* 0.0 | im3 | 0.0 | im2 */
	movlhps %xmm3, %xmm2 /* 0.0 | re3 | 0.0 | re2 */
	mulps %xmm4, %xmm1
	movaps (%edx), %xmm5 /* w7 | w6 | w5 | w4 */
	shufps $0xb1, %xmm2, %xmm2 /* re3 | 0.0 | re2 | 0.0 */
	movaps %xmm1, (%eax)
	addl $32, %esi
	subps %xmm6, %xmm2 /* re | -im3 | re | -im2 */
	addl $-32, %edi
	mulps %xmm5, %xmm2
	addl $32, %eax
	movaps %xmm2, -16(%eax)
	decl %ecx
	jnz .second_128_delays

	popl %edi
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	
	leave
	ret
	.p2align 4,0
#endif
