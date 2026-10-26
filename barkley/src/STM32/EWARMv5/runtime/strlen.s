/*
 * strlen: bytes until aligned, then a word at a time while it has no zero byte.
 */
	.syntax	unified
	.thumb
	.text

	.global	strlen
	.type	strlen, %function
	.thumb_func
strlen:
	add.w	r3, r0, #1
	lsls	r1, r0, #30
	beq	2f
1:	ldrb.w	r1, [r0], #1
	cbz	r1, 5f
	lsls	r1, r0, #30
	bne	1b
2:	ldr	r2, [r0]
3:	subs.w	r1, r2, #0x01010101
	bics	r1, r2
	tst.w	r1, #0x80808080
	itt	eq
	ldreq.w	r2, [r0, #4]!
	beq	3b
	ldrb.w	r1, [r0], #1
4:	cbz	r1, 5f
	ldrb.w	r1, [r0], #1
	b	4b
5:	subs	r0, r0, r3
	bx	lr
	.size	strlen, . - strlen
