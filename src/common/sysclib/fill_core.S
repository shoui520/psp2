/*
 * SetMemoryCore: Sony's hand-scheduled memset (memset in release builds).
 * The byte is spread over q0; 16 bytes or more are an unaligned first
 * store, aligned stm loop and an unaligned last store that overlap; less is
 * an 8-byte pair or a word, half-word and byte by the size's bits.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	16
	.global	SetMemoryCore
	.type	SetMemoryCore, %function
	.thumb_func
SetMemoryCore:
#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
	.global	memset
	.type	memset, %function
	.thumb_set memset, SetMemoryCore
#endif
	vdup.8	q0, r1
	subs	r2, #16
	push	{r0, lr}
	vmov	ip, r1, d0
	bcc	.Lsmall
	add	r2, r0
	vst1.8	{d0-d1}, [r0]!
	beq	.Ldone
	bic	r0, r0, #15
	subs	r2, r2, r0
	bls	.Llast
	vmov	lr, r3, d0
.Lblocks:
	subs	r2, #16
	stmia	r0!, {r1, r3, ip, lr}
	bhi	.Lblocks
.Llast:
	adds	r0, r0, r2
	vst1.8	{d0-d1}, [r0]
.Ldone:
	pop	{r0, pc}
.Lsmall:
	lsls	r3, r2, #29
	bcc	.Lwords
	vst1.8	{d0}, [r0]!
	adds	r3, r0, r2
	vst1.8	{d1}, [r3]
	pop	{r0, pc}
.Lwords:
	it	mi
	strmi	r1, [r0], #4
	lsls	r3, r2, #31
	it	cs
	strhcs	ip, [r0], #2
	it	mi
	strbmi	r1, [r0]
	pop	{r0, pc}
	.balign	16
	.size	SetMemoryCore, . - SetMemoryCore
