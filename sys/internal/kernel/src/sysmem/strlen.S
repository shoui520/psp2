/*
 * strlen and strnlen: Sony's hand-written length scan, a word at a time from
 * the aligned word holding the start, the bytes before the start masked off;
 * uqsub8 from 0x01010101 marks a zero byte, and rev + clz finds the first
 * one. strlen is strnlen with no limit and falls into it.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	16
	nop.w
	nop.w
	nop.w
	.global	strlen
	.type	strlen, %function
	.thumb_func
strlen:
	mov.w	r1, #0xffffffff
	.size	strlen, . - strlen

	.global	strnlen
	.type	strnlen, %function
	.thumb_func
strnlen:
	cbz	r1, 3f
	push	{lr}
	bic	r3, r0, #3
	ldr.w	lr, [r3], #4
	adds	r1, r1, r0
	it	cs
	movcs.w	r1, #0xffffffff
	subs	r2, r3, r0
	lsls	r2, r2, #3
	mov.w	ip, #0x01010101
	lsr.w	r2, ip, r2
	orr.w	r2, r2, lr
1:
	uqsub8	r2, ip, r2
	cmp	r3, r1
	bcs	2f
	cbnz	r2, 2f
	ldmia	r3!, {r2}
	b	1b
2:
	rev	r2, r2
	clz	r2, r2
	subs	r3, #4
	add.w	r3, r3, r2, lsr #3
	cmp	r3, r1
	ite	ls
	subls	r0, r3, r0
	subhi	r0, r1, r0
	pop	{pc}
3:
	mov	r0, r1
	bx	lr
	.balign	16
	.size	strnlen, . - strnlen
