/*
 * __aeabi_memcpy, __aeabi_memcpy4: n bytes copied; bytes until the source is aligned,
 * then 16 bytes at a time (8 at a time, stored a word at a time, when the destination is
 * not aligned), then the rest.
 */
	.syntax	unified
	.thumb
	.text

	.global	__aeabi_memcpy
	.global	__aeabi_memcpy4
	.type	__aeabi_memcpy, %function
	.type	__aeabi_memcpy4, %function
	.thumb_func
__aeabi_memcpy:
	cbz	r2, 6f
	lsls	r3, r1, #30
	beq	2f
1:	ldrb.w	r3, [r1], #1
	strb.w	r3, [r0], #1
	subs	r2, r2, #1
	beq	6f
	lsls	r3, r1, #30
	bne	1b
2:	lsls	r3, r0, #30
	bne	7f
	.thumb_func
__aeabi_memcpy4:
	subs	r2, #16
	bcc	4f
	push	{r4, r5}
3:	ldmia.w	r1!, {r3, r4, r5, ip}
	subs	r2, #16
	stmia.w	r0!, {r3, r4, r5, ip}
	bcs	3b
	pop	{r4, r5}
4:	lsls	r3, r2, #29
	itt	cs
	ldmiacs.w	r1!, {r3, ip}
	stmiacs.w	r0!, {r3, ip}
5:	itt	mi
	ldrmi.w	r3, [r1], #4
	strmi.w	r3, [r0], #4
	lsls	r2, r2, #31
	itt	cs
	ldrhcs.w	r2, [r1], #2
	strhcs.w	r2, [r0], #2
	itt	mi
	ldrbmi	r3, [r1]
	strbmi	r3, [r0]
6:	bx	lr
7:	subs	r2, #8
	bcc	9f
8:	ldmia.w	r1!, {r3, ip}
	subs	r2, #8
	str.w	r3, [r0], #4
	str.w	ip, [r0], #4
	bcs	8b
9:	lsls	r3, r2, #29
	b	5b
	.size	__aeabi_memcpy, __aeabi_memcpy4 - __aeabi_memcpy
	.size	__aeabi_memcpy4, . - __aeabi_memcpy4
