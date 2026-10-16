/*
 * Exception vector page template and report-only handler boundaries.
 *
 * Only what C cannot express is here: vector instructions executed from the
 * vector page, code entered in an exception mode before any register may be
 * touched (r0-r12, banked user SP/LR, SPSR), the System-mode stack switch,
 * the d0-d31 store, the chain jump with its register arguments, and the
 * exception return (LDM ^). Initial entries are in excpmgr_initial_*.S;
 * context returns are in excpmgr_entry.S. State and exception policy are C.
 *
 * Every handler is a SceExcpmgrHandler: two link words patched by the
 * registration functions, then ARM code entered at +8.
 *
 * This file retains 53 instructions: 32 vector loads and 21 report-entry
 * instructions. Link words and per-core vector data are not instructions.
 */
	.syntax	unified
	.arch	armv7-a
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

	.macro	HANDLER name
	.global	\name
	.type	\name, %object
\name:
	.word	0, 0
	.endm

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
	.endm

	REPORT_HANDLER defaultHandler, sceKernelDefaultHandlerCfunc
	REPORT_HANDLER resetHandler, sceKernelDefaultHandlerResetCfunc
	REPORT_HANDLER svcHandler, sceKernelDefaultHandlerSvcCfunc
	REPORT_HANDLER reservedHandler, sceKernelDefaultHandlerReservedCfunc
	REPORT_HANDLER irqHandler, sceKernelDefaultHandlerIrqCfunc, 4

