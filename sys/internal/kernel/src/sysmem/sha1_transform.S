/*
 * Sha1Transform(block, state): Sony's hand-written SHA-1 compression. The
 * sixteen-word schedule is the caller's block, rewritten in place; the round
 * counter runs from -16 to 63 (the first sixteen rounds read the block as it
 * is), a..e live in r7..r11 and the constant in lr. The state pointer and its
 * five words wait on the stack; returns state + 5.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	4
	.global	Sha1Transform
	.type	Sha1Transform, %function
	.thumb_func
Sha1Transform:
	push.w	{r4-r11, lr}
	sub	sp, #24
	ldm.w	r1, {r7-r11}
	stm.w	sp, {r1, r7-r11}
	movw	lr, #0x7999
	mvn	r1, #16
	movt	lr, #0x5a82
	b	.Lround
.Lchoose:
	and.w	r4, r9, r8
	bic.w	r5, r10, r8
.Lsum:
	add	r4, r5
.Ladvance:
	add	r2, r11
	add	r4, lr
	mov	r11, r10
	mov	r10, r9
	mov.w	r9, r8, ror #2
	mov	r8, r7
	adds	r7, r2, r4
.Lround:
	adds	r1, #1
	add.w	r11, r11, r7, ror #27
	and.w	ip, r1, #15
	ldr.w	r2, [r0, ip, lsl #2]
	blt	.Lchoose
	adds	r4, r1, #2
	add.w	r5, r1, #8
	and.w	r4, r4, #15
	and.w	r5, r5, #15
	add.w	r6, r1, #13
	and.w	r6, r6, #15
	ldr.w	r4, [r0, r4, lsl #2]
	ldr.w	r5, [r0, r5, lsl #2]
	ldr.w	r6, [r0, r6, lsl #2]
	eors	r2, r4
	eors	r2, r5
	eors	r2, r6
	cmp	r1, #24
	mov.w	r2, r2, ror #31
	str.w	r2, [r0, ip, lsl #2]
	blt	.Lfirst
	cmp	r1, #44
	movw	lr, #0xc1d6
	eor.w	r4, r8, r9
	movt	lr, #0xca62
	bge	.Llast
	and.w	r5, r8, r9
	movw	lr, #0xbcdc
	and.w	r4, r4, r10
	movt	lr, #0x8f1b
	b	.Lsum
.Lfirst:
	cmp	r1, #4
	blt	.Lchoose
	movw	lr, #0xeba1
	eor.w	r4, r8, r9
	movt	lr, #0x6ed9
	eor.w	r4, r4, r10
	b	.Ladvance
.Llast:
	eor.w	r4, r4, r10
	cmp	r1, #63
	bne	.Ladvance
	add	r4, lr
	add	r11, r2
	add	r11, r4
	pop	{r0-r2, r4-r6}
	add	r1, r11
	add	r2, r7
	add.w	r4, r4, r8, ror #2
	add	r5, r9
	add	r6, r10
	stmia	r0!, {r1, r2, r4-r6}
	pop.w	{r4-r11, pc}
	.size	Sha1Transform, . - Sha1Transform
