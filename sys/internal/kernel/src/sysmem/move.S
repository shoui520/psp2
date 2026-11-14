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

#ifndef MEMMOVE_AFTER_MEMCPY
	.balign	16
#endif
	.global	MoveMemoryCore
	.type	MoveMemoryCore, %function
	.thumb_func
MoveMemoryCore:
#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
	.global	memmove
	.type	memmove, %function
	.thumb_set memmove, MoveMemoryCore
#endif
	cbz	r2, .Lmove_return
	subs	r3, r0, r1
	beq	.Lmove_return
	cmp	r3, r2
#ifdef MEMMOVE_AFTER_MEMCPY
	bcs	.Lcopy_forward		/* in this object: a short branch */
#else
	bcs.w	CopyMemoryForward
#endif
	push	{r0, r4, lr}
	adds	r0, r0, r2
	adds	r1, r1, r2
	subs	r2, #32
	bcc	.Lmove_tail
	subs	r2, r0, r2
.Lmove_align_word:
	lsls	r3, r0, #30
	beq	.Lmove_align_block
	ldrb	r3, [r1, #-1]!
	strb	r3, [r0, #-1]!
	b	.Lmove_align_word
.Lmove_return:
	bx	lr
.Lmove_align_copy:
	ldr	ip, [r1, #-4]!
	str	ip, [r0, #-4]!
.Lmove_align_block:
	lsls	r3, r0, #27
	bne	.Lmove_align_copy
	subs	r2, r0, r2
	bcc	.Lmove_tail
	lsls	r3, r1, #30
	bne	.Lmove_unaligned
.Lmove_aligned:
	pld	[r1, #-0x60]
	ldmdb	r1!, {r3, r4, ip, lr}
	vldmdb	r1!, {d0-d1}
	subs	r2, #32
	stmdb	r0!, {r3, r4, ip, lr}
	vstmdb	r0!, {d0-d1}
	bcs	.Lmove_aligned
.Lmove_tail:
	adds	r2, #28
	blt	.Lmove_half
.Lmove_words:
	ldr	ip, [r1, #-4]!
	subs	r2, #4
	str	ip, [r0, #-4]!
	bge	.Lmove_words
.Lmove_half:
	lsls	r3, r2, #31
	bcc	.Lmove_byte
	ldrh	r3, [r1, #-2]!
	strh	r3, [r0, #-2]!
.Lmove_byte:
	bpl	.Lmove_done
	ldrb.w	r3, [r1, #-1]
	strb.w	r3, [r0, #-1]
.Lmove_done:
	pop	{r0, r4, pc}
.Lmove_unaligned:
	pld	[r1, #-0x60]
	subs	r1, #32
	vld1.8	{d0-d3}, [r1]
	subs	r2, #32
	vstmdb	r0!, {d0-d3}
	bcs	.Lmove_unaligned
	b	.Lmove_tail
	.size	MoveMemoryCore, . - MoveMemoryCore
