/*
 * The stack guard libsas's GCC code checks its frames against when it is linked statically into
 * an SNC module (as_ssp.h binds GCC's __stack_chk_guard and __stack_chk_fail to these): a guard
 * word that stays zero, and a failure handler that stops at a breakpoint.
 */
	.syntax	unified
	.thumb

	.text
	.balign	2
	.global	sceSasAsStackChkFail
	.type	sceSasAsStackChkFail, %function
	.thumb_func
sceSasAsStackChkFail:
	bkpt	0x81
	nop
	.size	sceSasAsStackChkFail, . - sceSasAsStackChkFail

	.bss
	.balign	4
	.global	sceSasAsStackChkGuard
	.type	sceSasAsStackChkGuard, %object
sceSasAsStackChkGuard:
	.space	4
	.size	sceSasAsStackChkGuard, 4
