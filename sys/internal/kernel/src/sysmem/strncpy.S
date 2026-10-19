/*
 * strncpy: Sony's hand-written copy. Bytes until the source is word aligned,
 * then words until one holds the terminator or fewer than 4 bytes are left,
 * then bytes; the rest of the destination is cleared with memset.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	16
	.global	strncpy
	.type	strncpy, %function
	.thumb_func
strncpy:
	push	{r0, lr}
	cbz	r2, 2f
1:
	lsls	r3, r1, #30
	beq	3f
	ldrb.w	r3, [r1], #1
	cbz	r3, 7f
	strb.w	r3, [r0], #1
	subs	r2, #1
	bne	1b
2:
	pop	{r0, pc}
3:
	mov.w	ip, #0x01010101
4:
	ldr.w	lr, [r1], #4
	uqsub8	r3, ip, lr
	subs	r2, #4
	bcc	5f
	cbnz	r3, 5f
	str.w	lr, [r0], #4
	b	4b
5:
	adds	r2, #4
	beq	9f
6:
	tst.w	lr, #0xff
	bne	8f
7:
	movs	r1, #0
	bl	memset
	pop	{r0, pc}
8:
	strb.w	lr, [r0], #1
	lsr.w	lr, lr, #8
	subs	r2, #1
	bne	6b
9:
	pop	{r0, pc}
	.size	strncpy, . - strncpy
