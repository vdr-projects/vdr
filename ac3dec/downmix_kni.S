/* 
 *  downmix_kni.S
 *
 *  Copyright (C) Yuqing Deng <Yuqing_Deng@brown.edu> - October 2000
 *
 *
 *  downmix_kni.S is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  downmix_kni.S is distributed in the hope that it will be useful,
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

.section .rodata
	.align 4
sqrt2:	.float 0f0.7071068
	.p2align 5,0,
	
	.section .text
	
	.align 4
	.global downmix_3f_2r_to_2ch_kni
	.type downmix_3f_2r_to_2ch_kni, @function

downmix_3f_2r_to_2ch_kni:
	pushl	%ebp
	movl  	%esp, %ebp
	
	pushl	%eax
	pushl	%ebx
	pushl	%ecx

	movl 	8(%ebp), %eax /* samples[] */
	movl	12(%ebp), %ebx /* &dm_par */
	movl	$64, %ecx		/* loop counter */

	movss	(%ebx), %xmm5	/* unit */
	shufps	$0, %xmm5, %xmm5	/* unit | unit | unit | unit */

	movss	4(%ebx), %xmm6		/* clev */
	shufps	$0, %xmm6, %xmm6	/* clev | clev | clev | clev */

	movss	8(%ebx), %xmm7		/* slev */
	shufps	$0, %xmm7, %xmm7	/* slev | slev | slev | slev */

.loop:	
	movaps	(%eax), %xmm0 /*  left */
	movaps	2048(%eax), %xmm1 /* right */
	movaps 	1024(%eax), %xmm2	/* center */
	mulps	%xmm5, %xmm0
	mulps	%xmm5, %xmm1
	
	mulps	%xmm6, %xmm2
	movaps	3072(%eax), %xmm3	/* leftsur */
	movaps	4096(%eax), %xmm4	/* rithgsur */
	addps	%xmm2, %xmm0
	addps 	%xmm2, %xmm1

	mulps	%xmm7, %xmm3
	mulps	%xmm7, %xmm4
	addps	%xmm3, %xmm0
	addps	%xmm4, %xmm1

	movaps	%xmm0, (%eax)
	movaps	%xmm1, 1024(%eax)

	addl	$16, %eax
	decl 	%ecx
	jnz	.loop

	popl	%ecx
	popl	%ebx
	popl 	%eax

	leave
	ret
	.p2align 4,,7

	.global downmix_2f_2r_to_2ch_kni
	.type downmix_2f_2r_to_2ch_kni, @function

downmix_2f_2r_to_2ch_kni:
	pushl %ebp
	movl  %esp, %ebp

	pushl %eax
	pushl %ebx
	pushl %ecx

	movl 8(%ebp), %eax /* samples[] */
	movl 12(%ebp), %ebx /* &dm_par */
	movl $64, %ecx	/* loop counter */

	movss (%ebx), %xmm5	/* unit */
	shufps $0, %xmm5, %xmm5 /* unit | unit | unit | unit */

	movss	8(%ebx), %xmm7		/* slev */
	shufps	$0, %xmm7, %xmm7	/* slev | slev | slev | slev */

.loop3:	
	movaps	(%eax), %xmm0 /*  left */
	movaps	1024(%eax), %xmm1 /* right */
	movaps 	2048(%eax), %xmm3	/* leftsur */
	mulps	%xmm5, %xmm0
	mulps	%xmm5, %xmm1
	
	movaps	3072(%eax), %xmm4	/* rightsur */

	mulps	%xmm7, %xmm3
	mulps	%xmm7, %xmm4
	addps	%xmm3, %xmm0
	addps	%xmm4, %xmm1

	movaps	%xmm0, (%eax)
	movaps	%xmm1, 1024(%eax)

	addl	$16, %eax
	decl 	%ecx
	jnz	.loop3

	popl	%ecx
	popl	%ebx
	popl 	%eax

	leave
	ret
	.p2align 4,,7
	
	.global downmix_3f_1r_to_2ch_kni
	.type downmix_3f_1r_to_2ch_kni, @function

downmix_3f_1r_to_2ch_kni:
	pushl	%ebp
	movl  	%esp, %ebp
	
	pushl	%eax
	pushl	%ebx
	pushl	%ecx

	movl 	8(%ebp), %eax /* samples[] */
	movl	12(%ebp), %ebx /* &dm_par */
	movl	$64, %ecx		/* loop counter */

	movss	(%ebx), %xmm5	/* unit */
	shufps	$0, %xmm5, %xmm5	/* unit | unit | unit | unit */

	movss	4(%ebx), %xmm6		/* clev */
	shufps	$0, %xmm6, %xmm6	/* clev | clev | clev | clev */

	movss	8(%ebx), %xmm7		/* slev */
	shufps	$0, %xmm7, %xmm7	/* slev | slev | slev | slev */

.loop4:	
	movaps	(%eax), %xmm0 /*  left */
	movaps	2048(%eax), %xmm1 /* right */
	movaps 	1024(%eax), %xmm2	/* center */
	mulps	%xmm5, %xmm0
	mulps	%xmm5, %xmm1
	
	mulps	%xmm6, %xmm2
	movaps	3072(%eax), %xmm3	/* sur */

	addps	%xmm2, %xmm0
	mulps	%xmm7, %xmm3
	
	addps 	%xmm2, %xmm1

	subps	%xmm3, %xmm0
	addps	%xmm3, %xmm1

	movaps	%xmm0, (%eax)
	movaps	%xmm1, 1024(%eax)

	addl	$16, %eax
	decl 	%ecx
	jnz	.loop4

	popl	%ecx
	popl	%ebx
	popl 	%eax

	leave
	ret
	.p2align 4,,7
		
	.global downmix_2f_1r_to_2ch_kni
	.type downmix_2f_1r_to_2ch_kni, @function

downmix_2f_1r_to_2ch_kni:
	pushl	%ebp
	movl  	%esp, %ebp
	
	pushl	%eax
	pushl	%ebx
	pushl	%ecx

	movl 	8(%ebp), %eax /* samples[] */
	movl	12(%ebp), %ebx /* &dm_par */
	movl	$64, %ecx		/* loop counter */

	movss	(%ebx), %xmm5	/* unit */
	shufps	$0, %xmm5, %xmm5	/* unit | unit | unit | unit */

	movss	8(%ebx), %xmm7		/* slev */
	shufps	$0, %xmm7, %xmm7	/* slev | slev | slev | slev */

.loop5:	
	movaps	(%eax), %xmm0 /*  left */
	movaps	1024(%eax), %xmm1 /* right */
	
	mulps	%xmm5, %xmm0
	mulps	%xmm5, %xmm1
	
	movaps	2048(%eax), %xmm3	/* sur */

	mulps	%xmm7, %xmm3
	
	subps	%xmm3, %xmm0
	addps	%xmm3, %xmm1

	movaps	%xmm0, (%eax)
	movaps	%xmm1, 1024(%eax)

	addl	$16, %eax
	decl 	%ecx
	jnz	.loop5

	popl	%ecx
	popl	%ebx
	popl 	%eax

	leave
	ret
	.p2align 4,,7
	
	.global downmix_3f_0r_to_2ch_kni
	.type downmix_3f_0r_to_2ch_kni, @function

downmix_3f_0r_to_2ch_kni:
	pushl	%ebp
	movl  	%esp, %ebp
	
	pushl	%eax
	pushl	%ebx
	pushl	%ecx

	movl 	8(%ebp), %eax /* samples[] */
	movl	12(%ebp), %ebx /* &dm_par */
	movl	$64, %ecx		/* loop counter */

	movss	(%ebx), %xmm5	/* unit */
	shufps	$0, %xmm5, %xmm5	/* unit | unit | unit | unit */

	movss	4(%ebx), %xmm6		/* clev */
	shufps	$0, %xmm6, %xmm6	/* clev | clev | clev | clev */


.loop6:	
	movaps	(%eax), %xmm0 /*  left */
	movaps	2048(%eax), %xmm1 /* right */
	movaps 	1024(%eax), %xmm2	/* center */
	mulps	%xmm5, %xmm0
	mulps	%xmm5, %xmm1
	
	mulps	%xmm6, %xmm2

	addps	%xmm2, %xmm0
	
	addps 	%xmm2, %xmm1

	movaps	%xmm0, (%eax)
	movaps	%xmm1, 1024(%eax)

	addl	$16, %eax
	decl 	%ecx
	jnz	.loop6

	popl	%ecx
	popl	%ebx
	popl 	%eax

	leave
	ret
	.p2align 4,,7
	
	.global stream_sample_2ch_to_s16_kni
	.type stream_sample_2ch_to_s16_kni, @function

stream_sample_2ch_to_s16_kni:
	pushl %ebp
	movl  %esp, %ebp

	pushl %eax
	pushl %ebx
	pushl %edx
	pushl %ecx

	movl 8(%ebp), %eax	/* s16_samples */
	movl 12(%ebp), %ebx	/* left */
	movl 16(%ebp), %edx	/* right */
	movl $64, %ecx

.loop1:
	movaps (%ebx), %xmm0	/* l3 | l2 | l1 | l0 */
	movaps (%edx), %xmm1	/* r3 | r2 | r1 | r0 */
	movhlps %xmm0, %xmm2	/* l3 | l2 */
	movhlps %xmm1, %xmm3	/* r3 | r2 */
	unpcklps %xmm1, %xmm0	/* r1 | l1 | r0 | l0 */
	unpcklps %xmm3, %xmm2	/* r3 | l3 | r2 | l2 */

	cvtps2pi %xmm0, %mm0	/* r0 l0 --> mm0, int_32 */
	movhlps %xmm0, %xmm0	
	cvtps2pi %xmm0, %mm1	/* r1 l1 --> mm1, int_32 */

	cvtps2pi %xmm2, %mm2	/* r2 l2 --> mm2, int_32 */
	movhlps %xmm2, %xmm2
	cvtps2pi %xmm2, %mm3	/* r3 l3 --> mm3, int_32 */
	packssdw %mm1, %mm0	/* r1 l1 r0 l0 --> mm0, int_16 */
	packssdw %mm3, %mm2	/* r3 l3 r2 l2 --> mm2, int_16 */

	movq %mm0, (%eax)
	movq %mm2, 8(%eax)
	addl $16, %eax
	addl $16, %ebx
	addl $16, %edx

	decl %ecx
	jnz .loop1

	popl %ecx
	popl %edx
	popl %ebx
	popl %eax

	emms

	leave
	ret
	.p2align 4,,7
	
	.global stream_sample_1ch_to_s16_kni
	.type stream_sample_1ch_to_s16_kni, @function

stream_sample_1ch_to_s16_kni:
	pushl %ebp
	movl  %esp, %ebp

	pushl %eax
	pushl %ebx
	pushl %ecx

	movl $sqrt2, %eax
	movss (%eax), %xmm7
	movl 8(%ebp), %eax	/* s16_samples */
	movl 12(%ebp), %ebx	/* left */
	shufps $0, %xmm7, %xmm7
	movl $64, %ecx

.loop2:
	movaps (%ebx), %xmm0	/* c3 | c2 | c1 | c0 */
	mulps %xmm7, %xmm0
	movhlps %xmm0, %xmm2	/* c3 | c2 */

	cvtps2pi %xmm0, %mm0	/* c1 c0 --> mm0, int_32 */
	cvtps2pi %xmm2, %mm1	/* c3 c2 --> mm1, int_32 */

	packssdw %mm0, %mm0	/* c1 c1 c0 c0 --> mm0, int_16 */
	packssdw %mm1, %mm1	/* c3 c3 c2 c2 --> mm1, int_16 */

	movq %mm0, (%eax)
	movq %mm1, 8(%eax)
	addl $16, %eax
	addl $16, %ebx

	decl %ecx
	jnz .loop2

	popl %ecx
	popl %ebx
	popl %eax
	
	emms
	leave
	ret 
#endif
