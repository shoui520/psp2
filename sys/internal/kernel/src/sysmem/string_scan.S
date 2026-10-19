/*
 * Sony's hand-written string scans (the file name is ours): a word at a time
 * from the aligned word holding the start, the bytes before the start masked
 * off; uqsub8 from 0x01010101 marks a zero byte, and rev + clz finds the
 * first one. strlen is strnlen with no limit and falls into it.
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
	.size	strnlen, . - strnlen

	.balign	16
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
