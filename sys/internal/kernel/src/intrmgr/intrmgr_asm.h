/*
 * Definitions of the interrupt manager's assembly (intrmgr_asm.S): the frame
 * and record offsets (intrmgr.c checks them against the C types) and the
 * macros the code repeats.
 */
#pragma once

/* IntrIrqFrame (the frame the IRQ handler builds on the IRQ stack) */
#define IRQ_FRAME_THREAD	0x10	/* TPIDRPRW of the interrupted code */
#define IRQ_FRAME_PMU		0x14	/* PMCNTENSET of the interrupted code */
#define IRQ_FRAME_R0		0x20
#define IRQ_FRAME_IP		0x34
#define IRQ_FRAME_PC		0x38
#define IRQ_FRAME_STATUS	0x3c

/* IntrHandler (intrmgr_state.h) */
#define INTR_HANDLER_CODE	4
#define INTR_HANDLER_ARG	8
#define INTR_HANDLER_PRIORITY	0xc
#define INTR_HANDLER_COUNT	0x20

#define CPSR_MODE_IRQ		0x12
#define CPSR_MODE_SYSTEM	0x1f

	.macro	LOAD_ADDRESS reg, symbol
	movw	\reg, #:lower16:\symbol
	movt	\reg, #:upper16:\symbol
	.endm

	.macro	LOAD_CONSTANT reg, value
	movw	\reg, #:lower16:\value
	movt	\reg, #:upper16:\value
	.endm

	.macro	LOAD_GLOBAL reg, symbol
	LOAD_ADDRESS \reg, \symbol
	ldr	\reg, [\reg]
	.endm

/*
 * A slot the hook registration patches (a nop that becomes a branch to the
 * hook). C addresses it through three labels, one for the branch offset, one
 * for the cache range and one for the store; the compiler does not merge
 * them, as Sony's code shows.
 */
	.macro	PATCH_SLOT name
	.global	\name
	.type	\name, %object
\name:
	.global	\name\()Clean
	.type	\name\()Clean, %object
\name\()Clean:
	.global	\name\()Patch
	.type	\name\()Patch, %object
\name\()Patch:
	.endm

	.macro	FUNCTION name
	.global	\name
	.type	\name, %function
\name:
	.fnstart
	.cantunwind
	.endm

/* A function whose unwind entry is the empty default one (no .cantunwind). */
	.macro	FUNCTION_DEFAULT_UNWIND name
	.global	\name
	.type	\name, %function
\name:
	.fnstart
	.endm

	.macro	END_FUNCTION name
	.fnend
	.size	\name, . - \name
	.endm

/* Save the six PMU event counters (type and count) at \ptr, advancing it. */
	.macro	SAVE_PMU_COUNTERS ptr, select, type, count
	.irp	counter, 0, 1, 2, 3, 4, 5
	mov	\select, #\counter
	mcr	p15, 0, \select, c9, c12, 5		@ PMSELR
	mrc	p15, 0, \type, c9, c13, 1		@ PMXEVTYPER
	mrc	p15, 0, \count, c9, c13, 2		@ PMXEVCNTR
	strd	\type, \count, [\ptr], #8
	.endr
	.endm

/* Reload the six PMU event counters from just below \ptr (counter 5 first). */
	.macro	RESTORE_PMU_COUNTERS ptr, select, type, count
	.irp	counter, 5, 4, 3, 2, 1, 0
	mov	\select, #\counter
	ldrd	\type, \count, [\ptr, #-8]!
	and	\type, \type, #0xff
	mcr	p15, 0, \select, c9, c12, 5		@ PMSELR
	mcr	p15, 0, \type, c9, c13, 1		@ PMXEVTYPER
	mcr	p15, 0, \count, c9, c13, 2		@ PMXEVCNTR
	.endr
	.endm

/* Hardware breakpoints 2-4: value and control pairs, \ptr at the first pair. */
	.macro	LOAD_BREAKPOINTS ptr, value, control
	.irp	index, 2, 3, 4
	ldm	\ptr!, {\value, \control}
	mcr	p14, 0, \value, c0, c\index, 4		@ DBGBVR
	mcr	p14, 0, \control, c0, c\index, 5	@ DBGBCR
	.endr
	.endm

/* Hardware watchpoints 1-3 */
	.macro	LOAD_WATCHPOINTS ptr, value, control
	.irp	index, 1, 2, 3
	ldm	\ptr!, {\value, \control}
	mcr	p14, 0, \value, c0, c\index, 6		@ DBGWVR
	mcr	p14, 0, \control, c0, c\index, 7	@ DBGWCR
	.endr
	.endm

/* Rearm the hardware trace of this core. */
	.macro	REARM_TRACE scratch, one
	mrc	p15, 0, \scratch, c12, c0, 0		@ VBAR
	ldr	\scratch, [\scratch, #0x24]		@ the core's trace area
	mov	\one, #1
	str	\one, [\scratch, #0x100]
	.endm
