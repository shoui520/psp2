/*
 * MoveMemoryCore: Sony's hand-scheduled memmove. Nothing to do for an empty
 * or in-place move; a destination below the source, or past its end, is a
 * forward copy (CopyMemoryForward). Otherwise it copies from the end down:
 * the destination aligned to 32 bytes, then 32 bytes a loop with ldmdb/stmdb
 * and NEON (VLD1 when the source is not word aligned), prefetching ahead,
 * and finishes with words, a half-word and a byte.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	16
	.global	MoveMemoryCore
	.type	MoveMemoryCore, %function
	.thumb_func
MoveMemoryCore:
#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
	.global	memmove
	.type	memmove, %function
	.thumb_set memmove, MoveMemoryCore
#endif
	cbz	r2, .Lreturn
	subs	r3, r0, r1
	beq	.Lreturn
	cmp	r3, r2
	bcs.w	CopyMemoryForward
	push	{r0, r4, lr}
	adds	r0, r0, r2
	adds	r1, r1, r2
	subs	r2, #32
	bcc	.Ltail
	subs	r2, r0, r2
.Lalign_word:
	lsls	r3, r0, #30
	beq	.Lalign_block
	ldrb	r3, [r1, #-1]!
	strb	r3, [r0, #-1]!
	b	.Lalign_word
.Lreturn:
	bx	lr
.Lalign_copy:
	ldr	ip, [r1, #-4]!
	str	ip, [r0, #-4]!
.Lalign_block:
	lsls	r3, r0, #27
	bne	.Lalign_copy
	subs	r2, r0, r2
	bcc	.Ltail
	lsls	r3, r1, #30
	bne	.Lunaligned
.Laligned:
	pld	[r1, #-0x60]
	ldmdb	r1!, {r3, r4, ip, lr}
	vldmdb	r1!, {d0-d1}
	subs	r2, #32
	stmdb	r0!, {r3, r4, ip, lr}
	vstmdb	r0!, {d0-d1}
	bcs	.Laligned
.Ltail:
	adds	r2, #28
	blt	.Lhalf
.Lwords:
	ldr	ip, [r1, #-4]!
	subs	r2, #4
	str	ip, [r0, #-4]!
	bge	.Lwords
.Lhalf:
	lsls	r3, r2, #31
	bcc	.Lbyte
	ldrh	r3, [r1, #-2]!
	strh	r3, [r0, #-2]!
.Lbyte:
	bpl	.Ldone
	ldrb.w	r3, [r1, #-1]
	strb.w	r3, [r0, #-1]
.Ldone:
	pop	{r0, r4, pc}
.Lunaligned:
	pld	[r1, #-0x60]
	subs	r1, #32
	vld1.8	{d0-d3}, [r1]
	subs	r2, #32
	vstmdb	r0!, {d0-d3}
	bcs	.Lunaligned
	b	.Ltail
	.size	MoveMemoryCore, . - MoveMemoryCore
