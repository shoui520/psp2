/*
 * Handwritten Thumb-2 NEON memory copy, move and fill helpers for static PAF
 * builds (0.996-era libpaf_stdc.a linked into cui_setupper and psp2swu).
 */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global sce_paf_memcpy_neon
	.type sce_paf_memcpy_neon, %function
	.thumb_func
sce_paf_memcpy_neon:
	.fnstart
	cbz r2, .Lmemcpy_ret
	pld [r1]
	pld [r1, #0x20]
	pld [r1, #0x40]
	pld [r1, #0x60]
	mov ip, r0
	subs r3, r1, r0
	tst.w r3, #3
	bne .Lmemcpy_unaligned
.Lmemcpy_align_word:
	tst.w r0, #3
	beq .Lmemcpy_align_cache
	subs r2, #1
	ldrb.w r3, [r1], #1
	strb.w r3, [r0], #1
	bne .Lmemcpy_align_word
	mov r0, ip
.Lmemcpy_ret:
	bx lr
.Lmemcpy_align_cache:
	tst.w r0, #0x1f
	beq .Lmemcpy_blocks
	subs r2, #4
	blo .Lmemcpy_tail_word
	ldr.w r3, [r1], #4
	str.w r3, [r0], #4
	bne .Lmemcpy_align_cache
	mov r0, ip
	bx lr
.Lmemcpy_blocks:
	subs r2, #0x20
	blo .Lmemcpy_tail_block
	vldmia r1!, {d0-d3}
	pld [r1, #0x80]
	vstmia r0!, {d0-d3}
	bne .Lmemcpy_blocks
	mov r0, ip
	bx lr
.Lmemcpy_unaligned:
	tst.w r0, #0x1f
	beq .Lmemcpy_unaligned_check
	subs r2, #1
	ldrb.w r3, [r1], #1
	strb.w r3, [r0], #1
	bne .Lmemcpy_unaligned
	mov r0, ip
	bx lr
.Lmemcpy_unaligned_check:
	cmp r2, #0x20
	blo .Lmemcpy_tail
	subs r3, r1, r0
	ands.w r3, r3, #3
	eors r1, r3
	vldmia r1!, {d0-d1}
	cmp r3, #1
	beq .Lmemcpy_shift_one
	cmp r3, #2
	beq .Lmemcpy_shift_two
	b .Lmemcpy_shift_three
.Lmemcpy_shift_one:
	cmp r2, #0x2c
	sub.w r2, r2, #0x20
	blo .Lmemcpy_unaligned_done
	vldmia r1!, {d2-d5}
	pld [r1, #0x80]
	vext.8 q0, q0, q1, #1
	vext.8 q1, q1, q2, #1
	vstmia r0!, {d0-d3}
	vmov q0, q2
	b .Lmemcpy_shift_one
.Lmemcpy_shift_two:
	cmp r2, #0x2b
	sub.w r2, r2, #0x20
	blo .Lmemcpy_unaligned_done
	vldmia r1!, {d2-d5}
	pld [r1, #0x80]
	vext.16 q0, q0, q1, #1
	vext.16 q1, q1, q2, #1
	vstmia r0!, {d0-d3}
	vmov q0, q2
	b .Lmemcpy_shift_two
.Lmemcpy_shift_three:
	cmp r2, #0x2a
	sub.w r2, r2, #0x20
	blo .Lmemcpy_unaligned_done
	vldmia r1!, {d2-d5}
	pld [r1, #0x80]
	vext.8 q0, q0, q1, #3
	vext.8 q1, q1, q2, #3
	vstmia r0!, {d0-d3}
	vmov q0, q2
	b .Lmemcpy_shift_three
.Lmemcpy_unaligned_done:
	subs r1, #0x10
	eors r1, r3
.Lmemcpy_tail_block:
	adds r2, #0x1c
.Lmemcpy_tail_word:
	adds r2, #4
.Lmemcpy_tail:
	subs r2, #1
	ldrb.w r3, [r1], #1
	strb.w r3, [r0], #1
	bne .Lmemcpy_tail
	mov r0, ip
	bx lr
	.fnend
	.size sce_paf_memcpy_neon, . - sce_paf_memcpy_neon

	.global sce_paf_memmove_neon
	.type sce_paf_memmove_neon, %function
	.thumb_func
sce_paf_memmove_neon:
	.fnstart
	subs r3, r1, r0
	beq .Lmemmove_ret
	cbz r2, .Lmemmove_ret
	bls .Lmemmove_backward
	movw ip, #:lower16:sce_paf_memcpy_neon
	movt ip, #:upper16:sce_paf_memcpy_neon
	bx ip
.Lmemmove_backward:
	mov ip, r0
	adds r0, r0, r2
	adds r1, r1, r2
	cmn.w r3, #0x20
	bhs .Lmemmove_tail
	pld [r1]
	pld [r1, #-0x20]
	pld [r1, #-0x40]
	pld [r1, #-0x60]
	subs r3, r1, r0
	tst.w r3, #3
	bne .Lmemmove_unaligned
.Lmemmove_align_word:
	tst.w r0, #3
	beq .Lmemmove_align_cache
	subs r2, #1
	ldrb.w r3, [r1, #-1]!
	strb.w r3, [r0, #-1]!
	bne .Lmemmove_align_word
	mov r0, ip
.Lmemmove_ret:
	bx lr
.Lmemmove_align_cache:
	tst.w r0, #0x1f
	beq .Lmemmove_blocks
	subs r2, #4
	blo .Lmemmove_tail_word
	ldr.w r3, [r1, #-4]!
	str.w r3, [r0, #-4]!
	bne .Lmemmove_align_cache
	mov r0, ip
	bx lr
.Lmemmove_blocks:
	subs r2, #0x20
	blo .Lmemmove_tail_block
	vldmdb r1!, {d0-d3}
	pld [r1, #-0x80]
	vstmdb r0!, {d0-d3}
	bne .Lmemmove_blocks
	mov r0, ip
	bx lr
.Lmemmove_unaligned:
	tst.w r0, #0x1f
	beq .Lmemmove_unaligned_check
	subs r2, #1
	ldrb.w r3, [r1, #-1]!
	strb.w r3, [r0, #-1]!
	bne .Lmemmove_unaligned
	mov r0, ip
	bx lr
.Lmemmove_unaligned_check:
	cmp r2, #0x20
	blo .Lmemmove_tail
	subs r3, r1, r0
	ands.w r3, r3, #3
	eors r1, r3
	vldmia r1, {d4-d5}
	cmp r3, #1
	beq .Lmemmove_shift_one
	cmp r3, #2
	beq .Lmemmove_shift_two
	b .Lmemmove_shift_three
.Lmemmove_shift_one:
	cmp r2, #0x2c
	sub.w r2, r2, #0x20
	blo .Lmemmove_unaligned_done
	vldmdb r1!, {d0-d3}
	pld [r1, #-0x80]
	vext.8 q2, q1, q2, #1
	vext.8 q1, q0, q1, #1
	vstmdb r0!, {d2-d5}
	vmov q2, q0
	b .Lmemmove_shift_one
.Lmemmove_shift_two:
	cmp r2, #0x2b
	sub.w r2, r2, #0x20
	blo .Lmemmove_unaligned_done
	vldmdb r1!, {d0-d3}
	pld [r1, #-0x80]
	vext.16 q2, q1, q2, #1
	vext.16 q1, q0, q1, #1
	vstmdb r0!, {d2-d5}
	vmov q2, q0
	b .Lmemmove_shift_two
.Lmemmove_shift_three:
	cmp r2, #0x2a
	sub.w r2, r2, #0x20
	blo .Lmemmove_unaligned_done
	vldmdb r1!, {d0-d3}
	pld [r1, #-0x80]
	vext.8 q2, q1, q2, #3
	vext.8 q1, q0, q1, #3
	vstmdb r0!, {d2-d5}
	vmov q2, q0
	b .Lmemmove_shift_three
.Lmemmove_unaligned_done:
	eors r1, r3
.Lmemmove_tail_block:
	adds r2, #0x1c
.Lmemmove_tail_word:
	adds r2, #4
.Lmemmove_tail:
	subs r2, #1
	ldrb.w r3, [r1, #-1]!
	strb.w r3, [r0, #-1]!
	bne .Lmemmove_tail
	mov r0, ip
	bx lr
	.fnend
	.size sce_paf_memmove_neon, . - sce_paf_memmove_neon

	.global sce_paf_memset_neon
	.type sce_paf_memset_neon, %function
	.thumb_func
sce_paf_memset_neon:
	.fnstart
	cbz r2, .Lmemset_ret
	vdup.8 q0, r1
	bfi r1, r1, #8, #8
	bfi r1, r1, #0x10, #0x10
	vmov q1, q0
	mov ip, r0
.Lmemset_align_word:
	tst.w r0, #3
	beq .Lmemset_align_cache
	subs r2, #1
	strb.w r1, [r0], #1
	bne .Lmemset_align_word
	mov r0, ip
.Lmemset_ret:
	bx lr
.Lmemset_align_cache:
	tst.w r0, #0x1f
	beq .Lmemset_blocks
	subs r2, #4
	blo .Lmemset_tail_word
	str.w r1, [r0], #4
	bne .Lmemset_align_cache
	mov r0, ip
	bx lr
.Lmemset_blocks:
	subs r2, #0x20
	blt .Lmemset_tail_block
	vstmia r0!, {d0-d3}
	bne .Lmemset_blocks
	mov r0, ip
	bx lr
.Lmemset_tail_block:
	adds r2, #0x1c
.Lmemset_tail_word:
	adds r2, #4
.Lmemset_tail:
	subs r2, #1
	strb.w r1, [r0], #1
	bne .Lmemset_tail
	mov r0, ip
	bx lr
	.fnend
	.size sce_paf_memset_neon, . - sce_paf_memset_neon

	.global sce_paf_memfill32
	.type sce_paf_memfill32, %function
	.thumb_func
sce_paf_memfill32:
	.fnstart
	cbz r2, .Lfill_ret
	mov ip, r0
.Lfill_align_word:
	tst.w r0, #3
	beq .Lfill_align_cache
	subs r2, #1
	strb.w r1, [r0], #1
	mov.w r1, r1, ror #8
	bne .Lfill_align_word
	mov r0, ip
.Lfill_ret:
	bx lr
.Lfill_align_cache:
	tst.w r0, #0x1f
	beq .Lfill_blocks
	subs r2, #4
	blo .Lfill_tail_word
	str.w r1, [r0], #4
	bne .Lfill_align_cache
	mov r0, ip
	bx lr
.Lfill_blocks:
	vdup.32 q0, r1
	vmov q1, q0
.Lfill_block_loop:
	subs r2, #0x20
	blt .Lfill_tail_block
	vstmia r0!, {d0-d3}
	bne .Lfill_block_loop
	mov r0, ip
	bx lr
.Lfill_tail_block:
	adds r2, #0x1c
.Lfill_tail_word:
	adds r2, #4
.Lfill_tail:
	subs r2, #1
	strb.w r1, [r0], #1
	mov.w r1, r1, ror #8
	bne .Lfill_tail
	mov r0, ip
	bx lr
	.fnend
	.size sce_paf_memfill32, . - sce_paf_memfill32
