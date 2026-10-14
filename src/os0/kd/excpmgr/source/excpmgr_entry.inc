/* Exception entry register preservation and CPU-mode return boundaries. */
	.syntax unified
	.arch armv7-a
	.fpu neon
	.arm

/* SceExcpmgrContext offsets used by the stubs */
#define CTX_SIZE	0x400
#define CTX_SP		0x34
#define CTX_PC		0x3c
#define CTX_CPSR	0x44
#define CTX_VFP		0x100

	.macro	HANDLER name
	.global	\name
	.type	\name, %object
\name:
	.word	0, 0
	.endm

/*
 * First handler of the undef/pabt/dabt chains (priority 0).
 *
 * Build the context frame on the exception-mode stack: r0-r12, banked user
 * SP/LR, the faulting PC and SPSR. \entry (C) saves the rest and returns this
 * core's exception stack top, or 0 to resume the frame at once. Otherwise
 * continue in System mode on the exception stack, store d0-d31 before any
 * more code runs, copy the frame there (C), and jump to the next handler of
 * the chain with r0 = context, r1 = 3, lr = the interrupted lr.
 */
	.macro	INITIAL_HANDLER name, entry, lrfix
	HANDLER	\name
	subs	sp, sp, #CTX_SIZE
	/* Preserve each incoming register before C can allocate any of them. */
	.irp	saved, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	str	r\saved, [sp, #(\saved * 4)]
	.endr
	add	r0, sp, #CTX_SP
	stm	r0, {sp, lr}^		@ banked user registers
	.ifc	\lrfix, thumb
	mrs	r1, spsr
	str	r1, [sp, #CTX_CPSR]
	tst	r1, #0x20		@ undef: the instruction size depends on T
	subeq	lr, lr, #4
	subne	lr, lr, #2
	str	lr, [sp, #CTX_PC]
	.else
	sub	lr, lr, #\lrfix
	str	lr, [sp, #CTX_PC]
	mrs	lr, spsr
	str	lr, [sp, #CTX_CPSR]
	.endif
	mov	r0, sp
	bl	\entry
	cmp	r0, #0
	beq	excpResumeFrame
	mov	r4, sp			@ frame on the exception-mode stack
	add	sp, sp, #CTX_SIZE
	cps	#0x1f			@ System mode: user SP/LR banked
	sub	sp, r0, #CTX_SIZE
	add	r0, sp, #CTX_VFP
	vstmia	r0!, {d0-d15}
	vstmia	r0, {d16-d31}
	mov	r5, lr
	mov	r0, sp
	mov	r1, r4
	bl	excpCopyFrame
	mov	lr, r5
	mov	r0, sp
	mov	r1, #3
	movw	r2, #:lower16:\name
	movt	r2, #:upper16:\name
	ldr	r2, [r2]		@ next handler of the chain
	add	pc, r2, #8
	.endm

