/*
 * Secure module entry, at the load address 0x80b000: stack and gp, the stack guard from
 * Bigmac's random number register, the constructors, sm_main, the destructors, then
 * syscall 1 (unload) with sm_main's result, and sleep.
 */

	.section .text.entry, "ax"
	.global	_start
_start:
	movh	$sp, %hi(__stack_top)
	add3	$sp, $sp, %lo(__stack_top)
	movh	$gp, %hi(__gp)
	add3	$gp, $gp, %lo(__gp)

	add	$sp, -16
	sw	$1, ($sp)
	sw	$2, 4($sp)
	sw	$3, 8($sp)
	sw	$4, 12($sp)
	movh	$1, 0xe005		/* Bigmac */
	or3	$1, $1, 0x3c		/* random number */
	movu	$2, __stack_chk_guard
	lw	$3, ($1)
	sw	$3, ($2)
	bsr	call_ctors
	lw	$4, 12($sp)
	lw	$3, 8($sp)
	lw	$2, 4($sp)
	lw	$1, ($sp)
	add	$sp, 16

	bsr	sm_main
	add	$sp, -4
	sw	$0, ($sp)
	bsr	call_dtors
	mov	$4, 1			/* syscall 1: unload */
	lw	$1, ($sp)
	add	$sp, 4
	swi	0
	nop
1:	sleep
	bra	1b
