/* 
 *  srfft_kni.S
 *
 *  Copyright (C) Yuqing Deng <Yuqing_Deng@brown.edu> - October 2000
 *
 *
 *  srfft_kni.S is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  srfft_kni.S is distributed in the hope that it will be useful,
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

.section	.rodata
	.align 16
hsqrt2:	 .float 0f0.707106781188
	 .float 0f0.707106781188
	 .float 0f-0.707106781188
	 .float 0f-0.707106781188
C_1:	 .float 0f-1.0
	 .float 0f1.0
	 .float 0f-1.0
	 .float 0f1.0
	
.text
	.align 4
.global fft_4_kni
	.type fft_4_kni, @function
fft_4_kni:
	pushl %ebp
	movl %esp, %ebp
	movl 8(%ebp), %eax		/* complex_t * */
	
	movaps (%eax), %xmm0		/* x[1] | x[0] */
	movaps 16(%eax), %xmm2		/* x[3] | x[2] */
	movaps %xmm0, %xmm1		/* x[1] | x[0] */
	addps %xmm2, %xmm0		/* x[1] + x[3] | x[0] + x[2] */
	subps %xmm2, %xmm1		/* x[1] - x[3] | x[0] - x[2] */
	xorps %xmm6, %xmm6
	movhlps %xmm1, %xmm4		/* x[1] - x[3] */
	movhlps %xmm0, %xmm3		/* x[1] + x[3] */
	subss %xmm4, %xmm6		/* -(x[1] - x[3]).re */
	movlhps %xmm1, %xmm0		/* x[0] - x[2] | x[0] + x[2] */
	movss %xmm6, %xmm4		/* (x[1] - x[3]).im | (x[3]-x[1]).re */
	movaps %xmm0, %xmm2		/* x[0] - x[2] | x[0] + x[2] */
	shufps $0x14, %xmm4, %xmm3	/* -i*(x[2] - x[3] | x[2] + x[3] */
	addps %xmm3, %xmm0
	subps %xmm3, %xmm2
	movaps %xmm0, (%eax)
	movaps %xmm2, 16(%eax)

	leave
	ret
	

	.align 4
.global fft_8_kni
	.type fft_8_kni, @function
fft_8_kni:
	pushl %ebp
	movl %esp, %ebp
	movl 8(%ebp), %eax		/* complext_t */

	pushl %ebx
	movlps (%eax), %xmm0		/* x[0] */
	movlps 32(%eax), %xmm1		/* x[4] */
	movhps 16(%eax), %xmm0		/* x[2] | x[0] */
	movhps 48(%eax), %xmm1		/* x[6] | x[4] */
	movaps %xmm0, %xmm2		/* x[2] | x[0] */
	xorps %xmm3, %xmm3
	addps %xmm1, %xmm0		/* x[2] + x[6] | x[0] + x[4] */
	subps %xmm1, %xmm2		/* x[2] - x[6] | x[0] - x[4] */
	movhlps %xmm0, %xmm5		/* x[2] + x[6] */
	movhlps %xmm2, %xmm4
	movlhps %xmm2, %xmm0		/* x[0] - x[4] | x[0] + x[4] */
	subss %xmm4, %xmm3		/* -(x[2]-x[6]).re */
	movaps %xmm0, %xmm7		/* x[0] - x[4] | x[0] + x[4] */
	movss %xmm3, %xmm4		/* (x[2]-x[6]).im | -(x[2]-x[6]).re */
	movlps 8(%eax), %xmm1		/* x[1] */
	shufps $0x14, %xmm4, %xmm5	/* -i*(x[2] - x[6]) | x[2] + x[6] */

	addps %xmm5, %xmm0		/* yt */
	subps %xmm5, %xmm7		/* yb */

	movhps 24(%eax), %xmm1		/* x[3] | x[1] */
	movl $hsqrt2, %ebx
	movlps 40(%eax), %xmm2		/* x[5] */
	movhps 56(%eax), %xmm2		/* /x[7] | x[5] */
	movaps %xmm1, %xmm3		/* x[3] | x[1] */
	addps %xmm2, %xmm1		/* x[3] + x[7] | x[1] + x[5] */
	subps %xmm2, %xmm3		/* x[3] - x[7] | x[1] - x[5] */
	movaps (%ebx), %xmm4		/* -1/sqrt2 | -1/sqrt2 | 1/sqrt2 | 1/sqrt2 */
	movaps %xmm3, %xmm6		/* x[3] - x[7] | x[1] - x[5] */
	mulps %xmm4, %xmm3
	shufps $0xc8, %xmm4, %xmm4	/* -1/sqrt2 | 1/sqrt2 | -1/sqrt2 | 1/sqrt2 */
	shufps $0xb1, %xmm6, %xmm6
	mulps %xmm4, %xmm6
	addps %xmm3, %xmm6		/* (-1-i)/sqrt2 * (x[3]-x[7]) | (1-i)/sqrt2 * (x[1] - x[5] */
	movhlps %xmm1, %xmm5		/* x[3] + x[7] */
	movlhps %xmm6, %xmm1		/* (1+i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
	shufps $0xe4, %xmm6, %xmm5	/* (-1-i)/sqrt2 * (x[3]-x[7]) | x[3]+x[7] */
	movaps %xmm1, %xmm3		/* (1-i)/sqrt2 * (x[1]-x[5]) | x[1]+x[5] */
	movl $C_1, %ebx
	addps %xmm5, %xmm1		/* u */
	subps %xmm5, %xmm3		/* v */
	movaps %xmm0, %xmm2		/* yb */
	movaps %xmm7, %xmm4		/* yt */
	movaps (%ebx), %xmm5
	mulps %xmm5, %xmm3
	addps %xmm1, %xmm0		/* yt + u */
	subps %xmm1, %xmm2		/* yt - u */
	shufps $0xb1, %xmm3, %xmm3		/* -i * v */
	movaps %xmm0, (%eax)
	movaps %xmm2, 32(%eax)
	addps %xmm3, %xmm4		/* yb - i*v */
	subps %xmm3, %xmm7		/* yb + i*v */
	movaps %xmm4, 16(%eax)
	movaps %xmm7, 48(%eax)

	popl %ebx
	leave
	ret

	.align 4
.global fft_asmb_kni 
	.type fft_asmb, @function
fft_asmb_kni:
	pushl %ebp
	movl %esp, %ebp

	subl $4, %esp
		
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi

	movl 8(%ebp),  %ecx		/* k */
	movl 12(%ebp), %eax		/* x */
	movl %ecx, -4(%ebp)		/* k */
	movl 16(%ebp), %ebx		/* wT */
	movl 20(%ebp), %edx		/* d */
	movl 24(%ebp), %esi		/* d3 */
	shll $4, %ecx			/* 16k */
	addl $8, %edx
	leal (%eax, %ecx, 2), %edi
	addl $8, %esi
	
	/* TRANSZERO and TRANS */
	movaps (%eax), %xmm0		/* x[1] | x[0] */
	movaps (%ebx), %xmm1		/* wT[1] | wT[0] */
	movaps (%ebx, %ecx), %xmm2	/* wB[1] | wB[0] */
	movlps (%edx), %xmm3		/* d */
	movlps (%esi), %xmm4		/* d3 */
	movhlps %xmm1, %xmm5		/* wT[1] */
	movhlps %xmm2, %xmm6		/* wB[1] */
	shufps $0x50, %xmm3, %xmm3	/* d[1].im | d[1].im | d[1].re | d[1].re */
	shufps $0x50, %xmm4, %xmm4	/* d3[1].im | d3[1].im | d3[i].re | d3[i].re */
	movlhps %xmm5, %xmm5		/* wT[1] | wT[1] */
	movlhps %xmm6, %xmm6		/* wB[1] | wB[1] */
	mulps %xmm3, %xmm5
	mulps %xmm4, %xmm6
	movhlps %xmm5, %xmm7		/* wT[1].im * d[1].im | wT[1].re * d[1].im */
	movlhps %xmm6, %xmm5		/* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wT[1].im * d[1].re | wT[1].re * d[1].re */
	shufps $0xb1, %xmm6, %xmm7	/* wB[1].re * d3[1].im | wB[i].im * d3[1].im | wT[1].re * d[1].im | wT[1].im * d[1].im */
	movl $C_1, %edi
	movaps (%edi), %xmm4
	mulps %xmm4, %xmm7
	addps %xmm7, %xmm5		/* wB[1] * d3[1] | wT[1] * d[1] */
	movlhps %xmm5, %xmm1		/* d[1] * wT[1] | wT[0] */
	shufps $0xe4, %xmm5, %xmm2	/* d3[1] * wB[1] | wB[0] */
	movaps %xmm1, %xmm3		/* d[1] * wT[1] | wT[0] */
	leal (%eax, %ecx, 2), %edi
	addps %xmm2, %xmm1		/* u */
	subps %xmm2, %xmm3		/* v */
	mulps %xmm4, %xmm3
	movaps (%eax, %ecx), %xmm5	/* xk[1] | xk[0] */
	shufps $0xb1, %xmm3, %xmm3	/* -i * v */
	movaps %xmm0, %xmm2		/* x[1] | x[0] */
	movaps %xmm5, %xmm6		/* xk[1] | xk[0] */
	addps %xmm1, %xmm0
	subps %xmm1, %xmm2
	addps %xmm3, %xmm5
	subps %xmm3, %xmm6
	movaps %xmm0, (%eax)
	movaps %xmm2, (%edi)
	movaps %xmm5, (%eax, %ecx)
	movaps %xmm6, (%edi, %ecx)
	addl $16, %eax
	addl $16, %ebx
	addl $8, %edx
	addl $8, %esi
	decl -4(%ebp)

.loop:
	movaps (%ebx), %xmm0		/* wT[1] | wT[0] */
	movaps (%edx), %xmm1		/* d[1] | d[0] */

	movaps (%ebx, %ecx), %xmm4	/* wB[1] | wB[0] */
	movaps (%esi), %xmm5		/* d3[1] | d3[0] */

	movhlps %xmm0, %xmm2		/* wT[1] */
	movhlps %xmm1, %xmm3		/* d[1] */

	movhlps %xmm4, %xmm6		/* wB[1] */
	movhlps %xmm5, %xmm7		/* d3[1] */

	shufps $0x50, %xmm1, %xmm1	/* d[0].im | d[0].im | d[0].re | d[0].re */
	shufps $0x50, %xmm3, %xmm3	/* d[1].im | d[1].im | d[1].re | d[1].re */

	movlhps %xmm0, %xmm0		/* wT[0] | wT[0] */
	shufps $0x50, %xmm5, %xmm5	/* d3[0].im | d3[0].im | d3[0].re | d3[0].re */
	movlhps %xmm2, %xmm2		/* wT[1] | wT[1] */
	shufps $0x50, %xmm7, %xmm7	/* d3[1].im | d3[1].im | d3[1].re | d3[1].re */

	mulps %xmm1, %xmm0		/* d[0].im * wT[0].im | d[0].im * wT[0].re | d[0].re * wT[0].im | d[0].re * wT[0].re */
	mulps %xmm3, %xmm2		/* d[1].im * wT[1].im | d[1].im * wT[1].re | d[1].re * wT[1].im | d[1].re * wT[1].re */
	movlhps %xmm4, %xmm4		/* wB[0] | wB[0] */
	movlhps %xmm6, %xmm6		/* wB[1] | wB[1] */

	movhlps %xmm0, %xmm1		/* d[0].im * wT[0].im | d[0].im * wT[0].re */
	movlhps %xmm2, %xmm0		/* d[1].re * wT[1].im | d[1].re * wT[1].re | d[0].re * wT[0].im | d[0].re * wT[0].re */
	mulps %xmm5, %xmm4		/* wB[0].im * d3[0].im | wB[0].re * d3[0].im | wB[0].im * d3[0].re | wB[0].re * d3[0].re */
	mulps %xmm7, %xmm6		/* wB[1].im * d3[1].im | wB[1].re * d3[1].im | wB[1].im * d3[1].re | wB[1].re * d3[1].re */
	shufps $0xb1, %xmm2, %xmm1	/* d[1].im * wT[1].re | d[1].im * wT[1].im | d[0].im * wT[0].re | d[0].im * wT[0].im */
	movl $C_1, %edi
	movaps (%edi), %xmm3		/* 1.0 | -1.0 | 1.0 | -1.0 */

	movhlps %xmm4, %xmm5		/* wB[0].im * d3[0].im | wB[0].re * d3[0].im */
	mulps %xmm3, %xmm1		/* d[1].im * wT[1].re | -d[1].im * wT[1].im | d[0].im * wT[0].re | -d[0].im * wT[0].im */
	movlhps %xmm6, %xmm4		/* wB[1].im * d3[1].re | wB[1].re * d3[1].re | wB[0].im * d3[0].re | wB[0].im * d3[0].re */
	addps %xmm1, %xmm0		/* wT[1] * d[1] | wT[0] * d[0] */

	shufps $0xb1, %xmm6, %xmm5	/* wB[1].re * d3[1].im | wB[1].im * d3[1].im | wB[0].re * d3[0].im | wB[0].im * d3[0].im */
	mulps %xmm3, %xmm5		/* wB[1].re * d3[1].im | -wB[1].im * d3[1].im | wB[0].re * d3[0].im | -wB[0].im * d3[0].im */
	addps %xmm5, %xmm4		/* wB[1] * d3[1] | wB[0] * d3[0] */

	movaps %xmm0, %xmm1		/* wT[1] * d[1] | wT[0] * d[0] */
	addps %xmm4, %xmm0		/* u */
	subps %xmm4, %xmm1		/* v */
	movaps (%eax), %xmm6		/* x[1] | x[0] */
	leal (%eax, %ecx, 2), %edi
	mulps %xmm3, %xmm1
	addl $16, %ebx
	addl $16, %esi
	shufps $0xb1, %xmm1, %xmm1	/* -i * v */
	movaps (%eax, %ecx), %xmm7	/* xk[1] | xk[0] */
	movaps %xmm6, %xmm2
	movaps %xmm7, %xmm4
	addps %xmm0, %xmm6
	subps %xmm0, %xmm2
	movaps %xmm6, (%eax)
	movaps %xmm2, (%edi)
	addps %xmm1, %xmm7
	subps %xmm1, %xmm4
	addl $16, %edx
	movaps %xmm7, (%eax, %ecx)
	movaps %xmm4, (%edi, %ecx)

	addl $16, %eax
	decl -4(%ebp)
	jnz .loop

.end:		
	popl %edi	
	popl %esi
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	
	addl $4, %esp
		
	leave
	ret
#endif
