/*
 * strcpy: Sony's hand-written copy. Bytes until the source is word aligned,
 * then words until one holds the terminator (uqsub8 from 0x01010101), whose
 * bytes are stored up to and including the terminator.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.global	strcpy
	.type	strcpy, %function
	.thumb_func
strcpy:
	push	{r0, lr}
1:
	lsls	r3, r1, #30
	beq	2f
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	cbz	r3, 5f
	b	1b
2:
	mov.w	ip, #0x01010101
3:
	ldmia	r1!, {r3}
	uqsub8	r2, ip, r3
	cbnz	r2, 4f
	str	r3, [r0], #4
	b	3b
4:
	lsls	r2, r3, #24
	strb	r3, [r0], #1
	lsr.w	r3, r3, #8
	bne	4b
5:
	pop	{r0, pc}
	.balign	16
	.size	strcpy, . - strcpy
