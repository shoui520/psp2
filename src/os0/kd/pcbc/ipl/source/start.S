/*
 * IPL entry (0x88600000): leave exception/boot-vector mode, set up the stack
 * below 0x88800000 and enter main().
 */

	.set	noreorder
	.set	noat

	.section .text.start, "ax"

	.globl	_start
	.type	_start, @function
_start:
	mfc0	$k0, $12		/* Status: clear BEV, ERL, EXL */
	lui	$k1, 0xFFBF
	ori	$k1, $k1, 0xFFF9
	and	$k0, $k0, $k1
	li	$k1, 0
	or	$k0, $k0, $k1
	mtc0	$k0, $12
	move	$k1, $zero
	lui	$sp, 0x8880
	addiu	$sp, $sp, -64
	lui	$v0, %hi(main)
	addiu	$v0, $v0, %lo(main)
	jr	$v0
	nop
	.size	_start, . - _start

