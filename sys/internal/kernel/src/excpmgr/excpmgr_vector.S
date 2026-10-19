/*
 * Exception vector page template, the handlers that only report (default,
 * reset, SVC, reserved, IRQ), the last handlers of the abort and undefined
 * chains, and the exit from the terminal chain.
 *
 * Only what C cannot express: vector instructions executed from the vector
 * page, handlers entered in an exception mode before any register may be
 * touched, CP15 access and the exception return (LDM ^). The policy they
 * call is in excpmgr.c.
 *
 * Every handler is a SceExcpmgrHandler: two link words patched by the
 * registration functions, then ARM code entered at +8.
 */
	.syntax	unified
	.arch	armv7-a
	.fpu	neon
	.section .text.excpmgr_vector, "ax", %progbits
	.arm
#include "excpmgr_asm.h"

/*
 * Vector page template, copied to the reset vector by Init. Four per-core
 * banks of 0x40 bytes (VBAR = base + core * 0x40): eight vectors that load
 * their target from the shared slot table at 0x100, then eight per-core words
 * Init fills in (0x20 sysroot per-core block, 0x24 trace area).
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
 * Handlers that only report (default, reset, SVC, reserved, IRQ): called
 * directly from the vector, so they save the caller-saved registers around
 * the C function and return with LDM ^ (restores CPSR from SPSR).
 */
	.macro	REPORT_HANDLER name, cfunc, lrfix=0
	HANDLER	\name
	.if	\lrfix
	sub	lr, lr, #\lrfix		@ IRQ: return address is lr - 4
	.endif
	push	{r0-r3, ip, lr}
	blx	\cfunc
	clrex
	ldm	sp!, {r0-r3, ip, pc}^
	END_FUNCTION \name
	.endm

	REPORT_HANDLER defaultHandler, sceKernelDefaultHandlerCfunc
	REPORT_HANDLER resetHandler, sceKernelDefaultHandlerResetCfunc
	REPORT_HANDLER svcHandler, sceKernelDefaultHandlerSvcCfunc
	REPORT_HANDLER reservedHandler, sceKernelDefaultHandlerReservedCfunc
	REPORT_HANDLER irqHandler, sceKernelDefaultHandlerIrqCfunc, 4

/*
 * The last handlers of the chains. r0 = context, r1 = the exception reason;
 * reasons 0, 1, 2 and 4 leave the exception state and resume the thread, 3
 * reports the panic, everything else hangs.
 */
	.macro	PANIC_HANDLER name, cfunc
	HANDLER	\name
	movw	r3, #0xffff
	and	r2, r1, r3
	cmp	r2, #5
	addlt	pc, pc, r2, lsl #2
	b	.
	b	excpPanicLeave
	b	excpPanicLeave
	b	excpPanicLeave
	b	1f
	b	excpPanicLeave
1:	push	{r0-r3, ip, lr}
	blx	\cfunc
	pop	{r0-r3, ip, lr}
	b	.
	END_FUNCTION \name
	.endm

	PANIC_HANDLER panicDabtHandler, sceKernelPanicHandlerDabtCfunc
	PANIC_HANDLER panicPabtHandler, sceKernelPanicHandlerPabtCfunc
	PANIC_HANDLER panicUndefHandler, sceKernelPanicHandlerUndefCfunc

/* Leave the terminal exception chain in the interrupted thread. */
	FUNCTION excpPanicLeave
	NEST_COUNT_ADDRESS
	ldr	r3, [r2]
	subs	r3, r3, #1
	bne	.
	str	r3, [r2]
	mov	r1, #0
	mcr	p15, 0, r1, c5, c0, 0		@ DFSR
	mcr	p15, 0, r1, c5, c0, 1		@ IFSR
	mcr	p15, 0, r1, c5, c1, 0		@ ADFSR
	mcr	p15, 0, r1, c5, c1, 1		@ AIFSR
	mcr	p15, 0, r1, c6, c0, 0		@ DFAR
	mcr	p15, 0, r1, c6, c0, 2		@ IFAR
	ldr	r1, [r0, #CTX_THREAD]
	mcr	p15, 0, r1, c13, c0, 4		@ TPIDRPRW
	push	{r0-r3, ip, lr}
	bl	sceKernelSysrootReturnFromExcpToThread
	pop	{r0-r3, ip, lr}
	b	.
	END_FUNCTION excpPanicLeave
