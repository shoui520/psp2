/*
 * Sony's hand-written DES for SceKernelUtils: the initial and final
 * permutations as five swap steps on the block halves (r0, r1), one Feistel
 * round function (DesRound) and the key schedule. 4.00 rewrote all of them:
 * its round looks up a combined S-box/P table (desRoundTable) and builds
 * the schedule from multiplied bit gathers, and the parity adjustment
 * (SceKernelUtilsForDriver_919C12E2) became NEON; 3.6x indexes the paired
 * S-box nibbles in desSubstitution, which must be 256-byte aligned, and
 * keeps its parity adjustment in C (des_parity.c).
 */
#include <fw.h>

	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	2
#if FW_SINCE(0x04000000)
	.global	DesInitialPermutation
	.type	DesInitialPermutation, %function
	.thumb_func
DesInitialPermutation:
	eor.w	r2, r0, r1, lsr #4
	and.w	r2, r2, #0xf0f0f0f
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #4
	eor.w	r2, r0, r1, lsr #16
	movt	r2, #0
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #16
	eor.w	r2, r1, r0, lsr #2
	and.w	r2, r2, #0x33333333
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #2
	eor.w	r2, r1, r0, lsr #8
	and.w	r2, r2, #0xff00ff
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #8
	eor.w	r2, r0, r1, lsr #1
	and.w	r2, r2, #0x55555555
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #1
	bx	lr
	.size	DesInitialPermutation, . - DesInitialPermutation

	.global	DesFinalPermutation
	.type	DesFinalPermutation, %function
	.thumb_func
DesFinalPermutation:
	eor.w	r2, r0, r1, lsr #1
	and.w	r2, r2, #0x55555555
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #1
	eor.w	r2, r1, r0, lsr #8
	and.w	r2, r2, #0xff00ff
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #8
	eor.w	r2, r1, r0, lsr #2
	and.w	r2, r2, #0x33333333
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #2
	eor.w	r2, r0, r1, lsr #16
	movt	r2, #0
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #16
	eor.w	r2, r0, r1, lsr #4
	and.w	r2, r2, #0xf0f0f0f
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #4
	bx	lr
	.size	DesFinalPermutation, . - DesFinalPermutation

	.global	DesRound
	.type	DesRound, %function
	.thumb_func
DesRound:
	and.w	r3, r1, #252
	and.w	r2, r0, #252
	push.w	{r4, r5, r6, r7, r8, lr}
	movw	r4, #:lower16:desRoundTable+12
	movt	r4, #:upper16:desRoundTable+12
	ldr	r3, [r4, r3]
	ldr	r2, [r4, r2]
	ubfx	r6, r0, #10, #6
	ubfx	r8, r0, #26, #6
	ubfx	r7, r1, #10, #6
	ubfx	lr, r1, #26, #6
	ubfx	ip, r0, #18, #6
	ubfx	r5, r1, #18, #6
	ldr.w	r6, [r4, r6, lsl #2]
	ldr.w	r8, [r4, r8, lsl #2]
	ldr.w	r7, [r4, r7, lsl #2]
	ldr.w	lr, [r4, lr, lsl #2]
	eor.w	r6, r6, r8
	ldr.w	ip, [r4, ip, lsl #2]
	ldr.w	r5, [r4, r5, lsl #2]
	ldmdb	r4, {r0, r1, r4}
	eor.w	r7, r7, lr
	eor.w	r2, r2, ip
	eor.w	r3, r3, r5
	and.w	r6, r6, r0
	and.w	r7, r7, r0
	and.w	r2, r2, r0
	and.w	r0, r0, r3
	eor.w	r8, r8, r6
	eor.w	lr, lr, r7
	eor.w	ip, ip, r2
	eor.w	r0, r0, r5
	bic.w	r8, r8, r1
	and.w	lr, lr, r1
	bic.w	ip, ip, r1
	and.w	r0, r0, r1
	add	r8, lr
	add	r0, ip
	bic.w	r8, r8, r4
	and.w	r0, r0, r4
	add	r0, r8
	pop.w	{r4, r5, r6, r7, r8, pc}
	.size	DesRound, . - DesRound

	.global	SceKernelUtilsForDriver_AE6C45FB
	.type	SceKernelUtilsForDriver_AE6C45FB, %function
	.thumb_func
SceKernelUtilsForDriver_AE6C45FB:
	push	{r4, r5, r6, r7, lr}
	movs	r1, #7
	movw	r7, #0x408
	movt	r7, #0x102
.LkeyGather:
	lsrs	r3, r3, #1
	lsrs	r2, r2, #1
	and.w	ip, r3, #0x1010101
	and.w	r6, r2, #0x1010101
	add.w	r6, r6, ip, lsl #4
	muls	r6, r7
	lsls	r4, r4, #8
	add.w	r4, r4, r5, lsr #24
	lsls	r5, r5, #8
	add.w	r5, r5, r6, lsr #24
	subs	r1, #1
	bne.n	.LkeyGather
	lsls	r4, r4, #8
	add.w	r4, r4, r5, lsr #28
	rev	r4, r4
	movw	r6, #:lower16:desKeySelection
	movt	r6, #:upper16:desKeySelection
	ldr.w	lr, [r6, #48]
.LkeyRound:
	and.w	r1, lr, #3
	bfi	r4, r4, #28, #4
	bfi	r5, r5, #28, #4
	lsrs	r4, r1
	lsrs	r5, r1
	movs	r7, #46
.LkeySelect:
	ldrh	r1, [r6, r7]
	lsrs.w	ip, r5, r1
	adcs	r3, r3
	lsrs	r1, r1, #8
	lsrs.w	ip, r4, r1
	adcs	r2, r2
	subs	r7, #2
	bge.n	.LkeySelect
	lsls	r1, r3, #8
	bfi	r1, r3, #12, #12
	lsrs	r7, r2, #12
	bfi	r1, r7, #4, #12
	bfi	r3, r3, #4, #6
	lsls	r3, r3, #14
	bfi	r3, r7, #10, #6
	lsrs	r7, r2, #6
	bfi	r3, r2, #2, #6
	bfi	r1, r7, #2, #6
	movs.w	lr, lr, lsr #2
	stmia	r0!, {r1, r3}
	bne.n	.LkeyRound
	movs	r0, #0
	pop	{r4, r5, r6, r7, pc}
	.size	SceKernelUtilsForDriver_AE6C45FB, . - SceKernelUtilsForDriver_AE6C45FB

	.global	SceKernelUtilsForDriver_919C12E2
	.type	SceKernelUtilsForDriver_919C12E2, %function
	.thumb_func
SceKernelUtilsForDriver_919C12E2:
	vmov	d16, r0, r1
	vmov.i8	d17, #1
	vcnt.8	d18, d16
	vbic	d17, d17, d18
	veor	d16, d17, d16
	vmov	r0, r1, d16
	bx	lr
	.size	SceKernelUtilsForDriver_919C12E2, . - SceKernelUtilsForDriver_919C12E2
#else
	.global	DesInitialPermutation
	.type	DesInitialPermutation, %function
	.thumb_func
DesInitialPermutation:
	eor.w	r2, r0, r1, lsr #4
	mov.w	r3, #0xf0f0f0f
	ands	r2, r3
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #4
	eor.w	r2, r0, r1, lsr #16
	bfc	r2, #16, #16
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #16
	eor.w	r2, r1, r0, lsr #2
	mov.w	r3, #0x33333333
	ands	r2, r3
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #2
	eor.w	r2, r1, r0, lsr #8
	mov.w	r3, #0xff00ff
	ands	r2, r3
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #8
	eor.w	r2, r0, r1, lsr #1
	mov.w	r3, #0x55555555
	ands	r2, r3
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #1
	bx	lr
	.size	DesInitialPermutation, . - DesInitialPermutation

	.global	DesFinalPermutation
	.type	DesFinalPermutation, %function
	.thumb_func
DesFinalPermutation:
	eor.w	r2, r0, r1, lsr #1
	mov.w	r3, #0x55555555
	ands	r2, r3
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #1
	eor.w	r2, r1, r0, lsr #8
	mov.w	r3, #0xff00ff
	ands	r2, r3
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #8
	eor.w	r2, r1, r0, lsr #2
	mov.w	r3, #0x33333333
	ands	r2, r3
	eors	r1, r2
	eor.w	r0, r0, r2, lsl #2
	eor.w	r2, r0, r1, lsr #16
	bfc	r2, #16, #16
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #16
	eor.w	r2, r0, r1, lsr #4
	mov.w	r3, #0xf0f0f0f
	ands	r2, r3
	eors	r0, r2
	eor.w	r1, r1, r2, lsl #4
	bx	lr
	.size	DesFinalPermutation, . - DesFinalPermutation

	.global	DesRound
	.type	DesRound, %function
	.thumb_func
DesRound:
	push.w	{r4, r5, r6, r7, r8, r9, sl, fp}
	mov.w	r2, r0, ror #31
	mov.w	ip, r0, lsr #3
	bfi	r2, ip, #10, #14
	bfi	r2, ip, #8, #10
	bfi	r2, ip, #6, #6
	mov.w	r3, r0, ror #9
	mov.w	ip, r0, lsr #15
	bfi	r3, ip, #4, #14
	bfi	r3, ip, #2, #10
	bfi	r3, ip, #0, #6
	ldr	r0, [r1, #0]
	ldr	r1, [r1, #4]
	movw	ip, #:lower16:desSubstitution
	movt	ip, #:upper16:desSubstitution
	eor.w	r4, r2, r0
	ubfx	r5, r4, #6, #6
	ubfx	r6, r4, #12, #6
	ubfx	r7, r4, #18, #6
	eor.w	r8, r3, r1
	ubfx	r9, r8, #6, #6
	ubfx	sl, r8, #12, #6
	ubfx	fp, r8, #18, #6
	bfi	ip, r5, #2, #6
	ldrb.w	r1, [ip, #3]
	bfi	ip, r7, #2, #6
	ldrb.w	r7, [ip, #2]
	bfi	ip, r9, #2, #6
	ldr.w	r9, [ip, #1]
	bfi	ip, fp, #2, #6
	ldr.w	fp, [ip]
	bfi	ip, r4, #2, #6
	ldrb.w	r4, [ip, #3]
	bfi	ip, r6, #2, #6
	ldrb.w	r6, [ip, #2]
	bfi	ip, r8, #2, #6
	ldrb.w	r8, [ip, #1]
	bfi	ip, sl, #2, #6
	ldrb.w	sl, [ip]
	bfi	r1, r7, #8, #8
	bfi	r1, r9, #16, #8
	bfi	r1, fp, #24, #8
	bfi	r1, r4, #0, #4
	bfi	r1, r6, #8, #4
	bfi	r1, r8, #16, #4
	bfi	r1, sl, #24, #4
	mov.w	r0, r1, ror #27
	bfi	r0, r8, #31, #1
	mov.w	r4, r1, lsr #10
	bfi	r0, r4, #27, #3
	bfi	r0, r1, #24, #4
	mov.w	r3, r1, lsr #18
	bfi	r0, r3, #24, #3
	mov.w	r9, r1, lsr #3
	bfi	r0, r9, #24, #2
	mov.w	fp, r1, lsr #14
	bfi	r0, fp, #23, #2
	mov.w	r2, r1, lsr #31
	bfi	r0, r2, #23, #1
	bfi	r0, r6, #20, #2
	bfi	r0, r9, #17, #4
	bfi	r0, sl, #16, #4
	bfi	r0, fp, #18, #1
	bfi	r0, r1, #16, #2
	mov.w	r7, r1, lsr #20
	bfi	r0, r7, #14, #3
	mov.w	r6, r1, lsr #29
	bfi	r0, r6, #14, #2
	bfi	r0, sl, #14, #1
	bfi	r0, r3, #12, #1
	bfi	r0, r1, #11, #1
	bfi	r0, r6, #9, #1
	bfi	r0, r7, #5, #4
	bfi	r0, r4, #4, #4
	bfi	r0, r3, #5, #2
	bfi	r0, r1, #3, #3
	bfi	r0, sl, #2, #3
	bfi	r0, r4, #3, #1
	mov.w	r2, r1, lsr #21
	bfi	r0, r2, #2, #1
	mov.w	r2, r1, lsr #7
	bfi	r0, r2, #0, #1
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp}
	bx	lr
	.size	DesRound, . - DesRound

	.global	SceKernelUtilsForDriver_AE6C45FB
	.type	SceKernelUtilsForDriver_AE6C45FB, %function
	.thumb_func
SceKernelUtilsForDriver_AE6C45FB:
	push.w	{r4, r5, r6, r7, r8, r9, sl, fp}
	mov.w	r1, #57
.LkeyGather:
	mov.w	r5, r5, ror #31
	mov.w	r4, r4, lsl #1
	bfi	r4, r5, #4, #1
	and.w	r8, r1, #31
	lsr.w	r7, r3, r8
	lsr.w	r6, r2, r8
	tst.w	r1, #32
	it	eq
	moveq	r7, r6
	bfi	r5, r7, #4, #1
	subs.w	r1, r1, #8
	ubfx	r6, r1, #6, #1
	adds	r1, r1, r6
	ands.w	r1, r1, #63
	bne.n	.LkeyGather
	rev	r4, r4
	bfi	r4, r4, #4, #24
	mov.w	r4, r4, lsr #4
	mov.w	r5, r5, lsr #4
	movw	r8, #0xaaa5
	movt	r8, #0x6aa9
	movw	r6, #:lower16:desKeySelection
	movt	r6, #:upper16:desKeySelection
.LkeyRound:
	and.w	ip, r8, #3
	bfi	r4, r4, #28, #4
	bfi	r5, r5, #28, #4
	ror.w	r4, r4, ip
	ror.w	r5, r5, ip
	add.w	r7, r6, #48
.LkeySelect:
	subs.w	r7, r7, #2
	ldrh.w	fp, [r7]
	ubfx	sl, fp, #8, #8
	lsr.w	sl, r4, sl
	lsr.w	fp, r5, fp
	mov.w	r2, r2, lsl #1
	mov.w	r3, r3, lsl #1
	bfi	r2, sl, #0, #1
	bfi	r3, fp, #0, #1
	cmp	r6, r7
	bne.n	.LkeySelect
	str	r2, [r0, #0]
	str	r3, [r0, #4]
	adds	r0, #8
	movs.w	r8, r8, lsr #2
	bne.n	.LkeyRound
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp}
	bx	lr
	.size	SceKernelUtilsForDriver_AE6C45FB, . - SceKernelUtilsForDriver_AE6C45FB
#endif
