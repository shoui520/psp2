/*
 * Secure-world exception and monitor vector templates and handler stubs.
 *
 * Only what C cannot express is here: vector instructions executed from the
 * vector pages, code entered in an exception mode before any register may be
 * touched (the per-core stack switch, r0-r12, banked user SP/LR, SPSR), the
 * System-mode stack switch, the d0-d31 store, the chain jump with its
 * register arguments, and the exception return (LDM ^ / RFE). The state
 * handling is in excpmgr.c and ../excpmgr_context.h.
 *
 * Every handler is a SceExcpmgrHandler: two link words patched by the
 * registration functions, then ARM code entered at +8.
 */
	.syntax	unified
	.arch	armv7-a
	.arch_extension sec
	.fpu	neon
	.section .text.excpmgr_vector, "ax", %progbits
	.arm

/* SceExcpmgrContext offsets used by the stubs */
#define CTX_SIZE	0x400
#define CTX_SP		0x34
#define CTX_PC		0x3c
#define CTX_CPSR	0x44
#define CTX_VFP		0x100


/*
 * Exception vector page template, copied to the vector page by module_start:
 * four per-core banks of 0x40 bytes (VBAR = base + core * 0x40), eight vectors
 * that load their target from the shared slot table at 0x100, then eight
 * per-core words (0x20: the sysroot per-core block).
 */
	.balign	32
	.global	exceptionVectors
	.type	exceptionVectors, %object
exceptionVectors:
	.rept	4
	.irp	code, 0, 1, 2, 3, 4, 5, 6, 7
	ldr	pc, exceptionSlots + \code * 4
	.endr
	.space	32
	.endr
exceptionSlots:			@ handler + 8 per code, patched by registration
	.word	exceptionVectors
	.rept	7
	.word	defaultHandler
	.endr
	.size	exceptionVectors, . - exceptionVectors

/*
 * Secure monitor vector page template (MVBAR): SMC, prefetch/data abort, IRQ
 * and FIQ load their target from the slot table at +0x20, which
 * sceKernelRegisterMonitorEntry patches.
 */
	.global	monitorVectors
	.type	monitorVectors, %object
monitorVectors:
	.word	0, 0
	ldr	pc, monitorSlots + 2 * 4
	ldr	pc, monitorSlots + 3 * 4
	ldr	pc, monitorSlots + 4 * 4
	.word	0
	ldr	pc, monitorSlots + 6 * 4
	ldr	pc, monitorSlots + 7 * 4
monitorSlots:
	.word	0, 0
	.word	smcHandler
	.word	monitorHandler, monitorHandler
	.word	0
	.word	monitorHandler, monitorHandler
	.global	monitorVectorsEnd
monitorVectorsEnd:
	.size	monitorVectors, . - monitorVectors

	.macro	HANDLER name
	.global	\name
	.type	\name, %object
\name:
	.word	0, 0
	.endm

/*
 * Switch to this core's stack of the exception mode: the mode's SP is parked
 * in FCSEIDR, the core number selects the entry of the mode's table.
 */
	.macro	CORE_STACK table
	mcr	p15, 0, sp, c13, c0, 0
	mrc	p15, 0, sp, c0, c0, 5
	and	sp, sp, #15
	cmp	sp, #1
	movwlt	sp, #:lower16:\table
	movtlt	sp, #:upper16:\table
	movweq	sp, #:lower16:\table + 4
	movteq	sp, #:upper16:\table + 4
	beq	1f
	blt	1f
	cmp	sp, #2
	movweq	sp, #:lower16:\table + 8
	movteq	sp, #:upper16:\table + 8
	movwne	sp, #:lower16:\table + 12
	movtne	sp, #:upper16:\table + 12
1:	ldr	sp, [sp]
	.endm

/*
 * Handlers that only report: called directly from a vector, they save the
 * caller-saved registers around the C function and return with LDM ^.
 */
	.macro	REPORT_HANDLER name, cfunc, stack=0, lrfix=0
	HANDLER	\name
	.ifnc	\stack, 0
	CORE_STACK \stack
	.endif
	.if	\lrfix
	sub	lr, lr, #\lrfix		@ IRQ/FIQ: return address is lr - 4
	.endif
	push	{r0-r3, ip, lr}
	blx	\cfunc
	clrex
	ldm	sp!, {r0-r3, ip, pc}^
	.endm

	REPORT_HANDLER defaultHandler, sceKernelDefaultHandlerCfunc
	REPORT_HANDLER resetHandler, sceKernelDefaultHandlerResetCfunc, g_svcStack
	REPORT_HANDLER svcHandler, sceKernelDefaultHandlerSvcCfunc, g_svcStack
	REPORT_HANDLER reservedHandler, sceKernelDefaultHandlerReservedCfunc
	REPORT_HANDLER irqHandler, sceKernelDefaultHandlerIrqCfunc, g_irqStack, 4
	REPORT_HANDLER fiqHandler, sceKernelDefaultHandlerFiqCfunc, 0, 4
	REPORT_HANDLER monitorHandler, sceKernelDefaultMonitorHandlerCfunc
	REPORT_HANDLER smcHandler, sceKernelDefaultHandlerSmcCfunc

/*
 * Last handler of a chain: r0 = context, r1 = chain argument, already in
 * System mode on the exception stack; the C function never returns. BX is
 * the ARM -> Thumb interworking jump.
 */
	.macro	PANIC_HANDLER name, cfunc
	HANDLER	\name
	movw	ip, #:lower16:\cfunc
	movt	ip, #:upper16:\cfunc
	bx	ip
	.endm

	PANIC_HANDLER panicDabtHandler, excpPanicDabt
	PANIC_HANDLER panicPabtHandler, excpPanicPabt
	PANIC_HANDLER panicUndefHandler, excpPanicUndef

/*
 * First handler of the undef/pabt/dabt chains (priority 0).
 *
 * Build the context frame on this core's stack of the exception mode:
 * r0-r12, banked user SP/LR, the faulting PC and SPSR. \entry (C) saves the
 * rest and returns this core's exception stack top. Continue in System mode
 * there, store d0-d31 before any more code runs, copy the frame (C), and jump
 * to the next handler of the chain with r0 = context, r1 = 3, lr = the
 * interrupted lr.
 */
	.macro	INITIAL_HANDLER name, entry, stack, lrfix
	HANDLER	\name
	CORE_STACK \stack
	sub	sp, sp, #CTX_SIZE
	stm	sp, {r0-r12}
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

	INITIAL_HANDLER initialDabtHandler, excpDabtEntry, g_abortStack, 8
	INITIAL_HANDLER initialPabtHandler, excpPabtEntry, g_abortStack, 4
	INITIAL_HANDLER initialUndefHandler, excpUndefEntry, g_undefStack, thumb

/*
 * Return into a context from excpReturnToContext (System mode): r0 = context;
 * registers are reloaded from it and RFE restores PC and CPSR.
 */
	.global	excpReturnToAbortMode
	.type	excpReturnToAbortMode, %function
excpReturnToAbortMode:
	cps	#0x17
	mov	ip, r0
	ldm	ip!, {r0-r11}
	add	ip, ip, #4
	ldm	ip, {sp, lr}^
	sub	ip, ip, #CTX_SP
	ldr	lr, [ip, #CTX_CPSR]
	push	{lr}
	ldr	lr, [ip, #CTX_PC]
	push	{lr}
	ldr	ip, [ip, #48]		@ r12 last
	clrex
	rfeia	sp!
	.size	excpReturnToAbortMode, . - excpReturnToAbortMode
