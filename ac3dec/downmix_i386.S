/* This is basicly gcc generated.
 * Only the floating point rounding mode loads and saves
 * are removed in the stream_sample_to_s16 functions.
 */                                                                            

#ifdef __i386__

	.file	"downmix.c"
	.version	"01.01"
gcc2_compiled.:
.text
	.align 4
.globl stream_sample_2ch_to_s16_i386
	.type	 stream_sample_2ch_to_s16_i386,@function
stream_sample_2ch_to_s16_i386:
	pushl %ebp
	movl %esp,%ebp
	subl $28,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%edx
	movl 12(%ebp),%ebx
	movl 16(%ebp),%ecx
	movl $255,%esi
	.p2align 4,,7
.L373:
	flds (%ebx)
	fistpl -8(%ebp)
	movl -8(%ebp),%eax
	movw %ax,(%edx)
	addl $2,%edx
	addl $4,%ebx
	flds (%ecx)
	fistpl -8(%ebp)
	movl -8(%ebp),%eax
	movw %ax,(%edx)
	addl $4,%ecx
	addl $2,%edx
	decl %esi
	jns .L373
	popl %ebx
	popl %esi
	popl %edi
	leave
	ret
.Lfe6:
	.size	 stream_sample_2ch_to_s16_i386,.Lfe6-stream_sample_2ch_to_s16_i386
.section	.rodata
	.align 4
.LC46:
	.long 0x3f350481
.text
	.align 4
.globl stream_sample_1ch_to_s16_i386
	.type	 stream_sample_1ch_to_s16_i386,@function
stream_sample_1ch_to_s16_i386:
	pushl %ebp
	movl %esp,%ebp
	subl $16,%esp
	pushl %esi
	pushl %ebx
	movl 8(%ebp),%edx
	movl 12(%ebp),%ecx
	flds .LC46
	movl $255,%ebx
	.p2align 4,,7
.L379:
	flds (%ecx)
	fmul %st(1),%st
	fistpl -8(%ebp)
	movl -8(%ebp),%eax
	movw %ax,-2(%ebp)
	addl $4,%ecx
	flds -2(%ebp)
	fistpl -8(%ebp)
	movl -8(%ebp),%eax
	movw %ax,(%edx)
	addl $2,%edx
	movw %ax,(%edx)
	addl $2,%edx
	decl %ebx
	jns .L379
	fstp %st(0)
	popl %ebx
	popl %esi
	leave
	ret
.Lfe7:
	.size	 stream_sample_1ch_to_s16_i386,.Lfe7-stream_sample_1ch_to_s16_i386
	.ident	"GCC: (GNU) 2.95.3 19991030 (prerelease)"
#endif
