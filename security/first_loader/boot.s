/*
 * first_loader's ROM part, run in place from the Cmep's vector base 0x40000: the vector
 * table, reset (copies the program, boot.c, to its RAM address and enters it), and the
 * signature in the ROM's last 0x80 bytes. FL_BUILD_DATE comes from the Makefile
 * (fw/<date>.mk, as --defsym).
 */

/*
 * The Cmep's vector table, at the vector base 0x40000: reset, and 44 vectors that
 * all end in park, a jump to itself.
 */

	.section .vectors, "ax"
	.global	vectors
vectors:
	jmp	reset
	.rept	43
	jmp	park
	.endr

	.global	park
park:		/* the 45th vector */
	jmp	park

/*
 * Reset: copy the program from ROM to its RAM address, switch the core's
 * configuration, and enter the program with r9 = 0x30000000, r10 = 1.
 */

	.section .reset, "ax"
	.global	reset
reset:
	di
	movh	$1, %hi(__program_start)
	add3	$1, $1, %lo(__program_start)
	movh	$2, %hi(__program_load)
	add3	$2, $2, %lo(__program_load)
	movh	$3, %hi(__program_size)
	add3	$3, $3, %lo(__program_size)
	mov	$5, $1
	bsr	copy

	ldc	$0, $cfg
	mov	$1, -17			/* clear cfg bit 4 */
	and	$0, $1
	or3	$0, $0, 8		/* set cfg bit 3 */
	stc	$0, $cfg

	movh	$9, 0x3000
	add3	$9, $9, 0
	mov	$10, 1
	nop
	nop
	jmp	$5

/* copy(r1 = destination, r2 = source, r3 = bytes): eight bytes at a time */
copy:
	srl	$3, 3
	add	$3, -1
	blti	$3, 0, 2f
	repeat	$3, 1f
	lw	$11, ($2)
	lw	$12, 4($2)
	sw	$11, ($1)
	sw	$12, 4($1)
1:	add	$1, 8
	add	$2, 8
2:	ret

/* Program entry: establish the C runtime state and clear its BSS. */
	.section .text.start, "ax"
	.global	firstLoaderEntry
	jmp	firstLoaderEntry
firstLoaderEntry:
	movu	$2, __bss_start
	movu	$3, __bss_end
	mov	$1, 0
	sub	$3, $2
	srl	$3, 3
	add	$3, -1
	/* The retail loop clears eight bytes per iteration. */
	repeat	$3, 1f
	sw	$1, ($2)
1:
	sw	$1, 4($2)
	add	$2, 8
	movu	$0, 0x5fff0
	mov	$sp, $0
	movh	$gp, %hi(__sdabase)
	add3	$gp, $gp, %lo(__sdabase)
	movh	$tp, 0x6
	add3	$tp, $tp, -0x1860
	mov	$0, 0
	stc	$0, $hi
	stc	$0, $lo
	movh	$1, 0x4
	movh	$2, 0x4
	bsr	firstLoaderMain
	mov	$3, 0
	mov	$4, 0
	mov	$5, 0
	mov	$6, 0
	mov	$7, 0
	mov	$8, 0
	mov	$9, 0
	mov	$10, 0
	mov	$11, 0
	mov	$12, 0
	mov	$tp, 0
	mov	$gp, 0
	mov	$sp, 0
	stc	$3, $sar
	stc	$3, $hi
	stc	$3, $lo
	stc	$3, $tmp
	stc	$3, $epc
	stc	$3, $npc
	movu	$0, 0x40000
	stc	$0, $lp
	movu	$0, 0x5ffe0
	movu	$1, secondLoaderTrampoline
	mov	$2, 7
	repeat	$2, 4f
	lw	$2, ($1)
	add	$1, 4
4:	sw	$2, ($0)
	add	$0, 4
	movu	$0, 0x40000
	mov	$1, 0xff7
	movu	$2, 0x5ffe0
	jmp	$2

/* This 32-byte block is copied to 0x5ffe0 immediately before the handoff. */
	.section .text.handoff, "ax"
	.global	secondLoaderTrampoline
	/* Preserve the original unreferenced word before this block. */
	.byte	0xc8, 0xdc, 0xc0, 0x05
secondLoaderTrampoline:
	repeat	$1, 5f
	nop
5:
	sw	$3, ($0)
	add	$0, 4
	mov	$0, 0
	mov	$1, 0
	mov	$2, 0
	stc	$3, $rpb
	stc	$3, $rpe
	stc	$3, $rpc
	ret
	.space	8, 0

/*
 * The ROM's last 0x80 bytes: the copyright notice, and the build date as BCD
 * (FL_BUILD_DATE, YYYYMMDD) in the final word.
 */

	.section .signature, "a"
	.ascii	"Copyright (C) 2011 Sony Computer Entertainment Inc. All rights reserved."
	.org	0x7c, 0
	.word	FL_BUILD_DATE
