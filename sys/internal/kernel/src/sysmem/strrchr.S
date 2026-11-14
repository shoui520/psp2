/*
 * strrchr: Sony's hand-written scan, a word at a time to the terminator,
 * remembering the last word holding the byte (as strchr).
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

#ifdef SYSMEM_TZS
	.section .text.strrchr, "ax", %progbits
#endif
	.global	strrchr
	.type	strrchr, %function
	.thumb_func
strrchr:
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
	movs	r4, #1
1:
	uqsub8	r2, ip, r2
	uqsub8	r0, ip, r0
	cbnz	r2, 3f
	cbz	r0, 2f
	clz	r0, r0
	sub.w	r4, r3, r0, lsr #3
2:
	ldmia	r3!, {r2}
	eor.w	r0, r2, r1
	b	1b
3:
	cbz	r1, 5f
	subs	r1, r2, #1
	ands	r0, r1
	beq	4f
	clz	r0, r0
	sub.w	r4, r3, r0, lsr #3
4:
	subs	r0, r4, #1
	pop	{r4, pc}
5:
	rev	r2, r2
	clz	r2, r2
	add.w	r0, r3, r2, lsr #3
	subs	r0, #4
	pop	{r4, pc}
	.size	strrchr, . - strrchr
