/*
 * strchr: Sony's hand-written scan for the byte and the terminator together,
 * a word at a time (as memchr); the first of the two found decides.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

#ifdef SYSMEM_TZS
	.section .text.strchr, "ax", %progbits
#endif
	.global	strchr
	.type	strchr, %function
	.thumb_func
strchr:
	push	{r4, lr}
	bic	r3, r0, #3
	ldmia	r3!, {r2}
	subs	r4, r3, r0
	lsls	r4, r4, #3
	bfi	r1, r1, #8, #8
	mov.w	ip, #0x01010101
	bfi	r1, r1, #16, #16
	eors.w	r0, r2, r1
	lsr.w	r4, ip, r4
	orrs	r2, r4
	orrs	r0, r4
1:
	uqsub8	r2, ip, r2
	uqsub8	r0, ip, r0
	cbnz	r2, 2f
	cbnz	r0, 3f
	ldmia	r3!, {r2}
	eor.w	r0, r2, r1
	b	1b
2:
	subs	r1, r2, #1
	orrs	r1, r2
	ands	r0, r1
	beq	4f
3:
	rev	r0, r0
	subs	r3, #4
	clz	r0, r0
	add.w	r0, r3, r0, lsr #3
4:
	pop	{r4, pc}
	.size	strchr, . - strchr
