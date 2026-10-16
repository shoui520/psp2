/*
 * CopyMemoryForward: Sony's hand-scheduled forward copy (memcpy in release
 * builds). It aligns the destination to 32 bytes, then copies 32 bytes a
 * loop with ldm/stm and NEON (VLD1 when the source is not word aligned),
 * prefetching ahead, and finishes with words, a half-word and a byte.
 * The register save keeps the destination in r0 for the return value.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	16
	.global	CopyMemoryForward
	.type	CopyMemoryForward, %function
	.thumb_func
CopyMemoryForward:
#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
	.global	memcpy
	.type	memcpy, %function
	.thumb_set memcpy, CopyMemoryForward
#endif
	push	{r0, r4, lr}
	subs	r2, #32
	bcc	.Ltail
	adds	r2, r2, r0
.Lalign_word:
	lsls	r3, r0, #30
	beq	.Lalign_block
	ldrb	r3, [r1], #1
	strb	r3, [r0], #1
	b	.Lalign_word
.Lalign_copy:
	ldr	ip, [r1], #4
	str	ip, [r0], #4
.Lalign_block:
	lsls	r3, r0, #27
	bne	.Lalign_copy
	subs	r2, r2, r0
	bcc	.Ltail
	lsls	r3, r1, #30
	bne	.Lunaligned
.Laligned:
	ldmia	r1!, {r3, r4, ip, lr}
	pld	[r1, #0xc0]
	vldmia	r1!, {d0-d1}
	subs	r2, #32
	stmia	r0!, {r3, r4, ip, lr}
	vstmia	r0!, {d0-d1}
	bcs	.Laligned
.Ltail:
	adds	r2, #28
	blt	.Lhalf
.Lwords:
	ldr	ip, [r1], #4
	subs	r2, #4
	str	ip, [r0], #4
	bge	.Lwords
.Lhalf:
	lsls	r3, r2, #31
	bcc	.Lbyte
	ldrh	r3, [r1], #2
	strh	r3, [r0], #2
.Lbyte:
	bpl	.Ldone
	ldrb	r3, [r1]
	strb	r3, [r0]
.Ldone:
	pop	{r0, r4, pc}
.Lunaligned:
	vld1.8	{d0-d3}, [r1]!
	pld	[r1, #0x80]
	subs	r2, #32
	vstmia	r0!, {d0-d3}
	bcs	.Lunaligned
	b	.Ltail
	.balign	16
	.size	CopyMemoryForward, . - CopyMemoryForward
