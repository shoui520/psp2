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

/*
 * The ROM's last 0x80 bytes: the copyright notice, and the build date as BCD
 * (FL_BUILD_DATE, YYYYMMDD) in the final word.
 */

	.section .signature, "a"
	.ascii	"Copyright (C) 2011 Sony Computer Entertainment Inc. All rights reserved."
	.org	0x7c, 0
	.word	FL_BUILD_DATE
