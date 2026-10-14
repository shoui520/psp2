/*
 * __aeabi_uldivmod / __aeabi_ldivmod for GCC-built modules: the quotient
 * from SNC's runtime division (i64ops.c) in r0:r1 and the remainder
 * n - q * d in r2:r3, as the ARM run-time ABI requires.  SNC-built code
 * calls __udivdi3 / __divdi3 directly and does not use these.
 */
	.syntax	unified
	.thumb
	.text

.macro	divmod name, div
	.global	\name
	.type	\name, %function
	.thumb_func
\name:
	push	{r4, r5, r6, r7, lr}
	sub	sp, sp, #4
	mov	r4, r0
	mov	r5, r1
	mov	r6, r2
	mov	r7, r3
	bl	\div
	umull	r2, r3, r0, r6
	mla	r3, r0, r7, r3
	mla	r3, r1, r6, r3
	subs	r2, r4, r2
	sbc	r3, r5, r3
	add	sp, sp, #4
	pop	{r4, r5, r6, r7, pc}
	.size	\name, . - \name
.endm

	divmod	__aeabi_uldivmod, __udivdi3
	divmod	__aeabi_ldivmod, __divdi3
