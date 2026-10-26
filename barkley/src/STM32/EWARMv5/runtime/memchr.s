/*
 * memchr: the first of n bytes equal to c, or 0; bytes until aligned, then a word at a
 * time while no byte of it matches.
 */
	.syntax	unified
	.thumb
	.text

	.global	memchr
	.type	memchr, %function
	.thumb_func
memchr:
	uxtb	r1, r1
1:	lsls	r3, r0, #30
	beq	2f
	subs	r2, r2, #1
	bcc	5f
	ldrb.w	r3, [r0], #1
	cmp	r1, r3
	bne	1b
	b	6f
2:	subs	r2, #8
	bcc	4f
	add.w	r2, r2, #4
	orr.w	r1, r1, r1, lsl #8
	orr.w	r1, r1, r1, lsl #16
3:	ldr.w	r3, [r0], #4
	subs	r2, r2, #4
	itttt	cs
	eorcs	r3, r1
	subcs.w	ip, r3, #0x01010101
	biccs.w	ip, ip, r3
	tstcs.w	ip, #0x80808080
	beq	3b
	uxtb	r1, r1
	subs	r0, r0, #4
4:	adds	r2, #8
7:	ldrb.w	r3, [r0], #1
	subs	r2, r2, #1
	it	cs
	teqcs	r1, r3
	bhi	7b
5:	it	ne
	movne	r0, #1
6:	subs	r0, r0, #1
	bx	lr
	.size	memchr, . - memchr
