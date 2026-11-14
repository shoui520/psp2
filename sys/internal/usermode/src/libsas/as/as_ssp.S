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

/*
 * A second copy of the handler with its own unwind entry, which nothing calls, in Sony's link
 * of libpaf, then an epilogue (pop {r4-r6, pc}) and a halfword that nothing reaches either, up
 * to the next object. What they were built from is not known (name ours); they are kept as
 * the instructions they decode to so the layout is Sony's.
 */
	.global	sceSasAsStackChkFailUnused
	.type	sceSasAsStackChkFailUnused, %function
	.thumb_func
sceSasAsStackChkFailUnused:
	bkpt	0x81
	nop
	pop	{r4, r5, r6, pc}
	nop
	strh	r0, [r6, #14]
	nop
	.size	sceSasAsStackChkFailUnused, . - sceSasAsStackChkFailUnused

	.bss
	.balign	4
	.global	sceSasAsStackChkGuard
	.type	sceSasAsStackChkGuard, %object
sceSasAsStackChkGuard:
	.space	4
	.size	sceSasAsStackChkGuard, 4
