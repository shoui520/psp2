/*
 * Marvell 88W8787 ("Robin") ARMv5TE exception vectors and scatterload runtime
 * (0x00000000 .. 0x00000148 in ITCM).
 *
 * At reset (0x00000000), the core jumps to bootReset (0x000529c0), which
 * configures the SoC clock/bus registers and CP15 control register, then
 * branches to __main (0x00000044). __scatterload processes the 13 entries in
 * Region$$Table (0x00054694 .. 0x00054764): copying and LZ77-decompressing the
 * four RW data segments (ITCM, DTCM, SQRAM0, SQRAM1) and zeroing the five BSS
 * segments before entering __rt_entry -> main (0x0000039c).
 */
	.syntax unified
	.arch armv5te

	.section .boot.vectors, "ax", %progbits
	.arm
	.global bootVectors
bootVectors:
	ldr	pc, .Lvec_reset		/* 0x00 reset */
	ldr	pc, .Lvec_undef		/* 0x04 undefined instruction */
	ldr	pc, .Lvec_swi		/* 0x08 SWI (ROM patch traps) */
	ldr	pc, .Lvec_pabt		/* 0x0c prefetch abort */
	ldr	pc, .Lvec_dabt		/* 0x10 data abort */
	ldr	pc, .Lvec_reserved	/* 0x14 reserved */
	ldr	pc, .Lvec_irq		/* 0x18 IRQ */
	ldr	pc, .Lvec_fiq		/* 0x1c FIQ */

	.word	0xdf0e4109		/* vector signature word */
.Lvec_reset:
	.word	bootReset
.Lvec_undef:
	.word	vectorUndefined
.Lvec_swi:
	.word	vectorSwi
.Lvec_pabt:
	.word	vectorPrefetchAbort
.Lvec_dabt:
	.word	vectorDataAbort
.Lvec_reserved:
	.word	vectorReserved
.Lvec_irq:
	.word	vectorIrq
.Lvec_fiq:
	.word	vectorFiq + 1

	.global __main
__main:
	b	__scatterload
	b	__rt_entry

	.global __scatterload
__scatterload:
	adr	r0, .Lregion_table_Desc
	ldm	r0, {r10, r11}
	add	r10, r10, r0
	add	r11, r11, r0
	sub	r7, r10, #1
.Lscatter_loop:
	cmp	r10, r11
	beq	__rt_entry
	ldm	r10!, {r0, r1, r2, r3}
	adr	lr, .Lscatter_loop
	tst	r3, #1
	subne	pc, r7, r3
	bx	r3

.Lregion_table_Desc:
	.word	__region_table_start - .Lregion_table_Desc
	.word	__region_table_end - .Lregion_table_Desc

/*
 * __decompress1 (0x00000084): ARM-to-Thumb entry + RVCT LZ77 decompressor.
 * r0 = compressed source, r1 = destination, r2 = uncompressed byte length.
 */
	.global __decompress1
__decompress1:
	adr	r12, __decompress1_thumb + 1
	bx	r12

	.thumb
	.thumb_func
__decompress1_thumb:
	adds	r2, r1, r2
.Llz_token:
	ldrb	r3, [r0]
	adds	r0, #1
	lsls	r4, r3, #30
	lsrs	r4, r4, #30
	bne	.Llz_match_len
	ldrb	r4, [r0]
	adds	r0, #1
.Llz_match_len:
	asrs	r5, r3, #4
	bne	.Llz_lit_check
	ldrb	r5, [r0]
	adds	r0, #1
.Llz_lit_check:
	subs	r4, #1
	beq	.Llz_match_check
.Llz_lit_loop:
	ldrb	r6, [r0]
	adds	r0, #1
	strb	r6, [r1]
	adds	r1, #1
	subs	r4, #1
	bne	.Llz_lit_loop
.Llz_match_check:
	cmp	r5, #0
	beq	.Llz_next
	ldrb	r4, [r0]
	lsls	r3, r3, #28
	adds	r0, #1
	lsrs	r3, r3, #30
	subs	r4, r1, r4
	cmp	r3, #3
	bne	.Llz_offset_done
	ldrb	r3, [r0]
	adds	r0, #1
.Llz_offset_done:
	lsls	r3, r3, #8
	subs	r4, r4, r3
	adds	r3, r5, #1
.Llz_copy_loop:
	ldrb	r6, [r4]
	adds	r4, #1
	strb	r6, [r1]
	adds	r1, #1
	subs	r3, #1
	bpl	.Llz_copy_loop
.Llz_next:
	cmp	r1, r2
	bcc	.Llz_token
	bx	lr

	.balign 4
	.arm
/*
 * __scatterload_copy (0x000000e4): copy r2 bytes from [r0] to [r1].
 */
	.global __scatterload_copy
__scatterload_copy:
1:
	subs	r2, r2, #16
	ldmcs	r0!, {r3, r4, r5, r6}
	stmcs	r1!, {r3, r4, r5, r6}
	bhi	1b
	movs	r2, r2, lsl #29
	ldmcs	r0!, {r4, r5}
	stmcs	r1!, {r4, r5}
	ldrmi	r4, [r0]
	strmi	r4, [r1]
	bx	lr

/*
 * __scatterload_zeroinit (0x0000010c): zero r2 bytes at [r1].
 */
	.global __scatterload_zeroinit
__scatterload_zeroinit:
	mov	r3, #0
	mov	r4, #0
	mov	r5, #0
	mov	r6, #0
1:
	subs	r2, r2, #16
	stmcs	r1!, {r3, r4, r5, r6}
	bhi	1b
	movs	r2, r2, lsl #29
	stmcs	r1!, {r4, r5}
	strmi	r3, [r1]
	bx	lr

/*
 * __rt_lib_init_thunk (0x00000138): relative jump to __rt_lib_init_noop.
 */
	.global __rt_lib_init_thunk
__rt_lib_init_thunk:
	ldr	r3, .Llib_init_off
	add	r3, pc, r3
	bx	r3
.Llib_init_off:
	.word	(__rt_lib_init_noop + 1) - (.Llib_init_off)
