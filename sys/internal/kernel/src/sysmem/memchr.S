/*
 * memchr: Sony's hand-written scan, a word at a time from the aligned word
 * holding the start, each word xored with the byte repeated four times, so a
 * matching byte becomes a zero byte (uqsub8 from 0x01010101, rev + clz).
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

#ifdef SYSMEM_TZS
	/* a section of its own: the secure world's link keeps only what is called */
	.section .text.memchr, "ax", %progbits
#endif
	.global	memchr
	.type	memchr, %function
	.thumb_func
memchr:
	push	{r4, lr}
	cbz	r2, 3f
	bic	r3, r0, #3
	ldmia	r3!, {r4}
	adds	r2, r2, r0
	it	cs
	movcs.w	r2, #0xffffffff
	subs	r0, r3, r0
	lsls	r0, r0, #3
	bfi	r1, r1, #8, #8
	mov.w	ip, #0x01010101
	bfi	r1, r1, #16, #16
	eors	r4, r1
	lsr.w	r0, ip, r0
	orrs	r0, r4
1:
	uqsub8	r0, ip, r0
	cmp	r3, r2
	bcs	2f
	cbnz	r0, 2f
	ldmia	r3!, {r0}
	eors	r0, r1
	b	1b
2:
	subs	r3, #4
	rev	r0, r0
	clz	r0, r0
	add.w	r0, r3, r0, lsr #3
	cmp	r0, r2
	bcs	3f
	pop	{r4, pc}
3:
	movs	r0, #0
	pop	{r4, pc}
	.size	memchr, . - memchr
