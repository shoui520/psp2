/*
 * Secure-world exception and monitor vector templates, the handlers that only
 * report, the last handlers of the abort and undefined chains, and the exit
 * from the terminal chain. The first handlers of the chains are os0's
 * (../excpmgr_initial.S, built with EXCPMGR_TZS), and so is the return into a
 * context (../excpmgr_context.S).
 *
 * Only what C cannot express: vector instructions executed from the vector
 * pages, handlers entered in an exception mode before any register may be
 * touched (the per-core stack switch), CP15 access and the exception return
 * (LDM ^). The policy they call is in excpmgr.c.
 *
 * Every handler is a SceExcpmgrHandler: two link words patched by the
 * registration functions, then ARM code entered at +8.
 */
	.syntax	unified
	.arch	armv7-a
	.fpu	neon
	.section .text.excpmgr_vector, "ax", %progbits
	.arm
#include "../excpmgr_asm.h"

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

/*
 * Handlers that only report: called directly from a vector, they save the
 * caller-saved registers around the C function and return with LDM ^
 * (restores CPSR from SPSR). Reset, SVC and IRQ first switch to this core's
 * stack of their mode.
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
	END_FUNCTION \name
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
 * The last handlers of the chains. r0 = context, r1 = the exception reason;
 * reasons 0, 1, 2 and 4 leave the exception state, 3 reports the panic,
 * everything else hangs.
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

/*
 * Leave the terminal exception chain: the secure world has no thread to
 * resume into, so with the fault state cleared and the context's thread
 * installed this core stops.
 */
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
	b	.
	END_FUNCTION excpPanicLeave
