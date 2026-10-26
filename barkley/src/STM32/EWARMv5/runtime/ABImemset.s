/*
 * __aeabi_memset: n bytes set to c, from the end: bytes until the end is aligned, then
 * 16 bytes at a time, then the rest. __aeabi_memclr: n bytes cleared, from the start a
 * word-aligned block (the fill below).
 */
	.syntax	unified
	.thumb
	.text

	.global	__aeabi_memset
	.type	__aeabi_memset, %function
	.thumb_func
__aeabi_memset:
	bfi	r2, r2, #8, #8
	bfi	r2, r2, #16, #16
	adds	r0, r0, r1
	ands	r3, r0, #3
	beq	1f
	subs	r1, r1, r3
	bcc	3f
	lsls	r3, r3, #31
	it	mi
	strbmi.w	r2, [r0, #-1]!
	it	cs
	strhcs.w	r2, [r0, #-2]!
1:	movs	r3, r2
	push	{r4, r5}
	mov	r4, r2
	mov	r5, r2
2:	subs	r1, #16
	it	cs
	stmdbcs	r0!, {r2, r3, r4, r5}
	bhi	2b
	lsls	r1, r1, #29
	it	cs
	stmdbcs	r0!, {r2, r3}
	it	mi
	strmi.w	r2, [r0, #-4]!
	lsls	r1, r1, #2
	it	cs
	strhcs.w	r2, [r0, #-2]!
	it	mi
	strbmi.w	r2, [r0, #-1]!
	pop	{r4, r5}
	bx	lr
3:	adds	r1, r1, r3
	it	ne
	strbne.w	r2, [r0, #-1]!
	lsls	r3, r1, #31
	it	cs
	strbcs.w	r2, [r0, #-1]!
	bx	lr
	.size	__aeabi_memset, . - __aeabi_memset
