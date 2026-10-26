/*
 * strchr: the first c in the string (its terminator included), or 0.
 */
	.syntax	unified
	.thumb
	.text

	.global	strchr
	.type	strchr, %function
	.thumb_func
strchr:
	ldrb	r2, [r0]
	uxtb	r1, r1
1:	cmp	r2, r1
	cbz	r2, 2f
	itt	ne
	ldrbne.w	r2, [r0, #1]!
	bne	1b
2:	it	ne
	movne	r0, #0
	bx	lr
	.size	strchr, . - strchr
