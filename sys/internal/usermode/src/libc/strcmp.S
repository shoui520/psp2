/*
 * strcmp: Sony's hand-written compare, a byte at a time through the
 * distance between the two strings; the difference of the first differing
 * bytes, or the first string's byte at the second's terminator.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.global	strcmp
	.type	strcmp, %function
	.thumb_func
strcmp:
	subs	r3, r0, r1
1:
	ldrb	r0, [r1, r3]
	ldrb	r2, [r1], #1
	cbz	r2, 2f
	subs	r0, r0, r2
	beq	1b
2:
	bx	lr
	.size	strcmp, . - strcmp
