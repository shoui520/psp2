/*
 * Sony's hand-written SFMT19937 (SFMT 1.3.3, MEXP 19937) for SceKernelUtils.
 * The context is the word index followed by the 624-word state; the
 * recursion runs on NEON, four words at a time (the SSE2 form of upstream's
 * do_recursion: the shift by one byte with vext, the SR1 shift masked by
 * sfmtMask). Failed preconditions return -1.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	2
/* genrand_int32: the next word, regenerating the state when it is used up. */
	.global	SceKernelUtilsForDriver_BBE4701A
	.type	SceKernelUtilsForDriver_BBE4701A, %function
	.thumb_func
SceKernelUtilsForDriver_BBE4701A:
	ldr	r2, [r0, #0]
	mov.w	ip, #1
	cmp.w	r2, #624
	bge.n	gen_rand_all
	adds	r2, #1
	str	r2, [r0, #0]
	ldr.w	r0, [r0, r2, lsl #2]
	bx	lr
	.size	SceKernelUtilsForDriver_BBE4701A, . - SceKernelUtilsForDriver_BBE4701A

/* genrand_int64: the next two words (an odd index skips one). */
	.global	SceKernelUtilsForDriver_45DEAAD6
	.type	SceKernelUtilsForDriver_45DEAAD6, %function
	.thumb_func
SceKernelUtilsForDriver_45DEAAD6:
	ldr	r2, [r0, #0]
	mov.w	ip, #2
	cmp.w	r2, #624
	bge.n	gen_rand_all
	ands.w	r1, r2, #1
	add.w	r3, r2, #2
	sub.w	r3, r3, r1
	str	r3, [r0, #0]
	add.w	r2, r0, r2, lsl #2
	ldr	r0, [r2, #4]
	ite	eq
	ldreq	r1, [r2, #8]
	movne	r1, #0
	bx	lr
	.size	SceKernelUtilsForDriver_45DEAAD6, . - SceKernelUtilsForDriver_45DEAAD6

/* gen_rand_all, reached from both with ip = 1 or 2: refill the state, take the first words. */
	.type	gen_rand_all, %function
	.thumb_func
gen_rand_all:
	vmov.i32	q9, #0
	movw	r1, #:lower16:sfmtMask
	movt	r1, #:upper16:sfmtMask
	vldmia	r1, {d20-d21}
	adds	r0, #4
	mov.w	r1, #0
	adds.w	r2, r0, #2464
	vldmia	r2!, {d4-d5}
	vldmia	r2, {d6-d7}
	mov	r2, r0
	adds.w	r3, r2, #1952
.LallFirst:
	vldmia	r2, {d0-d1}
	vldmia	r3!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r2!, {d6-d7}
	adds	r1, #1
	cmp	r1, #34
	blt.n	.LallFirst
	subs.w	r3, r2, #544
.LallSecond:
	vldmia	r2, {d0-d1}
	vldmia	r3!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r2!, {d6-d7}
	adds	r1, #1
	cmp	r1, #156
	blt.n	.LallSecond
	str.w	ip, [r0, #-4]
	ldr	r2, [r0, #0]
	cmp.w	ip, #1
	ite	eq
	moveq	r1, #0
	ldrne	r1, [r0, #4]
	mov	r0, r2
	bx	lr
	.size	gen_rand_all, . - gen_rand_all

/* fill_array32: a multiple of four words, halved into fill_array64. */
	.global	SceKernelUtilsForDriver_2B30548B
	.type	SceKernelUtilsForDriver_2B30548B, %function
	.thumb_func
SceKernelUtilsForDriver_2B30548B:
	tst.w	r2, #3
	bne.w	.LarrayBadSize
	mov.w	r2, r2, lsr #1
	.size	SceKernelUtilsForDriver_2B30548B, . - SceKernelUtilsForDriver_2B30548B

/* fill_array64: an even count of at least 312 words, in a fresh state. */
	.global	SceKernelUtilsForDriver_49B41540
	.type	SceKernelUtilsForDriver_49B41540, %function
	.thumb_func
SceKernelUtilsForDriver_49B41540:
	tst.w	r2, #1
	bne.w	.LarrayBadSize
	mov.w	r2, r2, lsr #1
	ldr	r3, [r0, #0]
	cmp.w	r3, #624
	bne.w	.LarrayBadSize
	cmp	r2, #156
	blt.w	.LarrayBadSize
	b.n	gen_rand_array
	.size	SceKernelUtilsForDriver_49B41540, . - SceKernelUtilsForDriver_49B41540

/* gen_rand_array: the recursion straight into the caller's array. */
	.type	gen_rand_array, %function
	.thumb_func
gen_rand_array:
	push	{r4, r5, r6, r7}
	mov.w	r3, #624
	str	r3, [r0, #0]
	vmov.i32	q9, #0
	movw	r3, #:lower16:sfmtMask
	movt	r3, #:upper16:sfmtMask
	vldmia	r3, {d20-d21}
	adds	r0, #4
	adds.w	r3, r0, #2464
	vldmia	r3!, {d4-d5}
	vldmia	r3, {d6-d7}
	mov.w	ip, #0
	mov	r4, r0
	adds.w	r5, r4, #1952
	mov	r6, r1
.LarrayFirst:
	vldmia	r4!, {d0-d1}
	vldmia	r5!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r6!, {d6-d7}
	add.w	ip, ip, #1
	cmp.w	ip, #34
	blt.n	.LarrayFirst
	subs.w	r5, r6, #544
.LarraySecond:
	vldmia	r4!, {d0-d1}
	vldmia	r5!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r6!, {d6-d7}
	add.w	ip, ip, #1
	cmp.w	ip, #156
	blt.n	.LarraySecond
	adds.w	r3, ip, #156
	cmp	r3, r2
	bge.n	.LarrayCopyCheck
	subs.w	r4, r6, #2496
	adds.w	r5, r4, #1952
.LarrayMiddle:
	vldmia	r4!, {d0-d1}
	vldmia	r5!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r6!, {d6-d7}
	add.w	ip, ip, #1
	adds.w	r3, ip, #156
	cmp	r3, r2
	blt.n	.LarrayMiddle
.LarrayCopyCheck:
	mov	r7, r2
	cmp.w	r2, #312
	bge.n	.LarrayLastCheck
	subs.w	r3, r2, #156
	adds.w	r3, r1, r3, lsl #4
.LarrayCopy:
	vldmia	r3!, {d0-d1}
	vstmia	r0!, {d0-d1}
	adds	r7, #1
	cmp.w	r7, #312
	blt.n	.LarrayCopy
.LarrayLastCheck:
	cmp	ip, r2
	bge.n	.LarrayDone
.LarrayLast:
	vldmia	r4!, {d0-d1}
	vldmia	r5!, {d2-d3}
	vext.8	q8, q9, q0, #15
	veor	q0, q0, q8
	vshr.u32	q1, q1, #11
	vand	q1, q1, q10
	veor	q0, q0, q1
	vext.8	q2, q2, q9, #1
	veor	q0, q0, q2
	vshl.s32	q8, q3, #18
	vorr	q2, q3, q3
	veor	q3, q0, q8
	vstmia	r6!, {d6-d7}
	vstmia	r0!, {d6-d7}
	add.w	ip, ip, #1
	cmp	ip, r2
	blt.n	.LarrayLast
.LarrayDone:
	mov.w	r0, #0
	pop	{r4, r5, r6, r7}
	bx	lr
.LarrayBadSize:
	mov.w	r0, #0xffffffff
	bx	lr
	.size	gen_rand_array, . - gen_rand_array

/* period_certification: flip one bit if the parity check fails. */
	.type	period_certification, %function
	.thumb_func
period_certification:
	push	{r4, r5}
	mov.w	r1, #0
	mov.w	r2, #4
	adds	r3, r0, #4
	movw	ip, #:lower16:sfmtParity
	movt	ip, #:upper16:sfmtParity
.LparitySum:
	ldr	r4, [r3, #0]
	adds	r3, #4
	ldr.w	r5, [ip]
	add.w	ip, ip, #4
	and.w	r4, r4, r5
	eor.w	r4, r4, r4, lsr #16
	eor.w	r4, r4, r4, lsr #8
	eor.w	r4, r4, r4, lsr #4
	eor.w	r4, r4, r4, lsr #2
	eor.w	r4, r4, r4, lsr #1
	eors	r1, r4
	adds.w	r2, r2, #0xffffffff
	bne.n	.LparitySum
	ands.w	r1, r1, #1
	bne.n	.LparityDone
	mov.w	r2, #4
	adds	r3, r0, #4
	movw	ip, #:lower16:sfmtParity
	movt	ip, #:upper16:sfmtParity
.LparityFix:
	ldr.w	r4, [ip]
	cbz	r4, .LparityNext
	add.w	r5, r4, #0xffffffff
	eors	r4, r5
	clz	r4, r4
	mov.w	r5, #0x80000000
	lsr.w	r5, r5, r4
	ldr	r1, [r3, #0]
	eors	r1, r5
	str	r1, [r3, #0]
	pop	{r4, r5}
	bx	lr
.LparityNext:
	adds	r3, #4
	add.w	ip, ip, #4
	adds.w	r2, r2, #0xffffffff
	bne.n	.LparityFix
.LparityDone:
	pop	{r4, r5}
	bx	lr
	.size	period_certification, . - period_certification

/* init_gen_rand: seed with one word. */
	.global	SceKernelUtilsForDriver_AB48C3CA
	.type	SceKernelUtilsForDriver_AB48C3CA, %function
	.thumb_func
SceKernelUtilsForDriver_AB48C3CA:
	push	{r4, lr}
	mov.w	r2, #624
	str	r2, [r0, #0]
	str	r1, [r0, #4]
	add.w	r4, r0, #8
	mov.w	ip, #1
	movw	r3, #0x8965
	movt	r3, #0x6c07
.LgenRandFill:
	eor.w	r2, r1, r1, lsr #30
	muls	r2, r3
	add.w	r1, r2, ip
	str	r1, [r4, #0]
	adds	r4, #4
	add.w	ip, ip, #1
	cmp.w	ip, #624
	blt.n	.LgenRandFill
	bl	period_certification
	mov.w	r0, #0
	pop	{r4, pc}
	.size	SceKernelUtilsForDriver_AB48C3CA, . - SceKernelUtilsForDriver_AB48C3CA

/* init_by_array: seed with an array of words. */
	.global	SceKernelUtilsForDriver_33AE1203
	.type	SceKernelUtilsForDriver_33AE1203, %function
	.thumb_func
SceKernelUtilsForDriver_33AE1203:
	push.w	{r4, r5, r6, r7, r8, r9, sl, fp, lr}
	mov.w	r3, #624
	str	r3, [r0, #0]
	adds	r0, #4
	add.w	ip, r0, #2496
	mov	r4, r0
	movw	r5, #0x8b8b
	movt	r5, #0x8b8b
.LinitFill:
	str	r5, [r4, #0]
	adds	r4, #4
	cmp	r4, ip
	bne.n	.LinitFill
	adds	r4, r2, #1
	cmp	r3, r4
	it	lt
	movlt	r3, r4
	adds.w	r3, r3, #0xffffffff
	mov.w	r4, #0
	mov.w	r5, #0xffffffff
	mov	r6, r0
	add.w	r7, r0, #1224
	addw	r8, r0, #2492
	addw	r9, r0, #1268
.LinitMix:
	ldr.w	sl, [r6]
	ldr.w	fp, [r7]
	ldr.w	ip, [r8]
	eor.w	sl, sl, fp
	eor.w	sl, sl, ip
	eor.w	sl, sl, sl, lsr #27
	movw	ip, #0x660d
	movt	ip, #0x0019
	mul.w	sl, sl, ip
	add	fp, sl
	str.w	fp, [r7]
	cbz	r1, .LinitMixed
	cmp	r4, #0
	it	eq
	addeq	sl, r2
	beq.n	.LinitMixed
	ldr.w	ip, [r1]
	adds	r1, #4
	add	sl, ip
	add.w	ip, r5, #1
	cmp	ip, r2
	it	eq
	moveq	r1, #0
.LinitMixed:
	add	sl, r4
	str.w	sl, [r6]
	ldr.w	fp, [r9]
	add	fp, sl
	str.w	fp, [r9]
	adds	r4, #1
	adds	r5, #1
	adds	r6, #4
	adds	r7, #4
	add.w	r8, r8, #4
	add.w	r9, r9, #4
	cmp.w	r4, #624
	it	ge
	movge	r4, #0
	add.w	ip, r0, #2496
	cmp	r6, ip
	it	eq
	moveq	r6, r0
	cmp	r7, ip
	it	eq
	moveq	r7, r0
	cmp	r8, ip
	it	eq
	moveq	r8, r0
	cmp	r9, ip
	it	eq
	moveq	r9, r0
	cmp	r5, r3
	bne.n	.LinitMix
	mov.w	r5, #624
.LinitFinal:
	ldr.w	sl, [r6]
	ldr.w	fp, [r7]
	ldr.w	ip, [r8]
	add	sl, fp
	add	sl, ip
	eor.w	sl, sl, sl, lsr #27
	movw	ip, #0x8b65
	movt	ip, #0x5d58
	mul.w	sl, sl, ip
	eor.w	fp, fp, sl
	str.w	fp, [r7]
	sub.w	sl, sl, r4
	str.w	sl, [r6]
	ldr.w	fp, [r9]
	eor.w	fp, fp, sl
	str.w	fp, [r9]
	adds	r4, #1
	adds.w	r5, r5, #0xffffffff
	adds	r6, #4
	adds	r7, #4
	add.w	r8, r8, #4
	add.w	r9, r9, #4
	cmp.w	r4, #624
	it	ge
	movge	r4, #0
	add.w	ip, r0, #2496
	cmp	r6, ip
	it	eq
	moveq	r6, r0
	cmp	r7, ip
	it	eq
	moveq	r7, r0
	cmp	r8, ip
	it	eq
	moveq	r8, r0
	cmp	r9, ip
	it	eq
	moveq	r9, r0
	cmp	r5, #0
	bne.n	.LinitFinal
	subs.w	r0, r0, #4
	bl	period_certification
	mov.w	r0, #0
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.size	SceKernelUtilsForDriver_33AE1203, . - SceKernelUtilsForDriver_33AE1203

	.section .rodata
	.balign	4
/* upstream's parity: PARITY1..4, the period certification vector */
	.type	sfmtParity, %object
sfmtParity:
	.word	0x00000001, 0x00000000, 0x00000000, 0x13c9e684
	.size	sfmtParity, . - sfmtParity
/* MSK1..4, the SR1 mask of the recursion */
	.type	sfmtMask, %object
sfmtMask:
	.word	0xdfffffef, 0xddfecb7f, 0xbffaffff, 0xbffffff6
	.size	sfmtMask, . - sfmtMask
