/*
 * backtrace_entry.S - entry points of the backtracer
 *
 * The backtrace starts at the caller's own registers, which C cannot read:
 * each entry stores r0-lr and its own address (as pc) in a register frame
 * on the stack, replaces the saved sp with the stack pointer the core
 * function sees, and calls the core function with the caller's four
 * register arguments, the caller's fifth (stack) argument and the frame.
 */

	.syntax	unified
	.arm
	.text

	.macro	BACKTRACE_ENTRY name, core
	.global	\name
	.type	\name, %function
\name:
	push	{r4, fp, lr}
	add	fp, sp, #8
	sub	sp, sp, #64			@ SceUInt32 regs[16]
	stm	sp, {r0-r12, sp, lr}
	ldr	r4, =\name
	str	r4, [fp, #-12]			@ regs[15]: this function
	mov	r4, sp
	sub	sp, sp, #4
	stmfd	sp!, {r4}			@ argument 6: regs
	ldr	r4, [fp, #4]
	stmfd	sp!, {r4}			@ argument 5: the caller's flags
	str	sp, [fp, #-20]			@ regs[13]: the stack of the core
	blx	\core
	sub	sp, fp, #8
	pop	{r4, fp, lr}
	bx	lr
	.size	\name, . - \name
	.endm

	BACKTRACE_ENTRY SceBacktraceForDriver_166B9C8C, _sceKernelBacktraceCore
	BACKTRACE_ENTRY SceBacktraceForDriver_888E99B8, _sceKernelBacktraceForKernelCore

	.ltorg
