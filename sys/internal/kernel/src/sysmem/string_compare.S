/*
 * Sony's hand-written compares (the file name is ours):
 * strncmp, in ARM: stops at the count, a terminator or a difference;
 * timingsafe_memcmp: every byte is read, with barriers around each, and the
 * first difference decides the sign.
 */
	.syntax unified
	.arch	armv7-a
	.text

	.arm
	.balign	4
	.global	strncmp
	.type	strncmp, %function
strncmp:
	cmp	r2, #0
	moveq	r0, #0
	moveq	pc, lr
	adds	ip, r0, r2
	movcs	ip, #0
	sub	ip, ip, #1
1:
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	ip, r0
	cmpcs	r2, #1
	cmpcs	r2, r3
	beq	1b
	sub	r0, r2, r3
	bx	lr
	.size	strncmp, . - strncmp

	.thumb
	.global	timingsafe_memcmp
	.type	timingsafe_memcmp, %function
	.thumb_func
timingsafe_memcmp:
	push	{r4-r7}
	mov.w	r7, #0
	mov.w	r3, #0
	cbz	r2, 2f
1:
	dmb	sy
	ldrb.w	r4, [r0], #1
	ldrb.w	r5, [r1], #1
	sub.w	r4, r4, r5
	rsb.w	r5, r4, #0
	mov.w	r4, r4, asr #31
	mov.w	r6, r5, asr #31
	mov.w	r5, r5, lsr #31
	orr.w	r5, r5, r4
	orr.w	r4, r4, r6
	mvn.w	r6, r7
	and.w	r5, r5, r6
	orr.w	r7, r7, r4
	orr.w	r3, r3, r5
	adds.w	r2, r2, #0xffffffff
	isb	sy
	bne	1b
2:
	mov	r0, r3
	pop	{r4-r7}
	bx	lr
	.size	timingsafe_memcmp, . - timingsafe_memcmp
