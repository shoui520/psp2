/*
 * Shared definitions of the exception assembly (excpmgr_vector.S,
 * excpmgr_initial.S, excpmgr_context.S): the SceExcpmgrContext and per-core
 * block offsets (checked against the C types in excpmgr.h), the function and
 * handler macros, and the exception-return sequences.
 */
#pragma once

/* SceExcpmgrContext offsets (excpmgr.h; checked in excpmgr.c) */
#define CTX_SIZE		0x400
#define CTX_PC			0x3c
#define CTX_EXCP_CODE		0x40
#define CTX_CPSR		0x44
#define CTX_CPACR		0x48
#define CTX_FPSCR		0x4c
#define CTX_FPEXC		0x50
#define CTX_CONTEXT_ID		0x54
#define CTX_TPIDRURW		0x58
#define CTX_TPIDRURO		0x5c
#define CTX_THREAD		0x60
#define CTX_TTBR1		0x64
#define CTX_DACR		0x6c
#define CTX_DFSR		0x70
#define CTX_IFSR		0x74
#define CTX_DFAR		0x78
#define CTX_IFAR		0x7c
#define CTX_PAR			0x80
#define CTX_TEEHBR		0x84
#define CTX_PMCR		0x88
#define CTX_PMCNTENSET		0x8c
#define CTX_PMCNTENSET2		0x90
#define CTX_PMSELR		0x94
#define CTX_PMCCNTR		0x98
#define CTX_PMUSERENR		0x9c
#define CTX_PMU_COUNTER		0xa0
#define CTX_WAYPOINT_CONTROL	0xd4
#define CTX_DBGDSCR		0xd8
#define CTX_VFP			0x100
#define CTX_WAYPOINT		0x200
#define CTX_SP			0x34
#define CTX_R12			0x30

/* ExcpVectorBank / ExcpTraceArea */
#define VECTOR_TRACE_AREA	0x24
#define TRACE_CONTROL		0x100
#define TRACE_WAYPOINT		0x200

/* SceExcpmgrData */
#define DATA_STACK_TOP		0x30
#define DATA_KERNEL_MMU		0x40
#define DATA_KERNEL_BREAKPOINTS	0x44
#define DATA_KERNEL_WATCHPOINTS	0x48

/* ThreadCB, the process's hardware breakpoint set */
#define THREAD_PROCESS		0x18
#define THREAD_THBP		0x1c
#define PROCESS_HW_BREAKPOINTS	0x0c
#define HW_REGISTERS		0x08

#define DACR_KERNEL		0x15450000

	.macro	FUNCTION name
	.global	\name
	.type	\name, %function
\name:
	.fnstart
	.cantunwind
	.endm

	.macro	END_FUNCTION name
	.fnend
	.size	\name, . - \name
	.endm

/* the two link words of a handler, inside its function */
	.macro	HANDLER name
	FUNCTION \name
	.word	0, 0
	.endm

/* r0 = &g_excpData.nestCount[cpu], cpu in r1 */
	.macro	NEST_COUNT_ADDRESS
	mrc	p15, 0, r1, c0, c0, 5
	and	r1, r1, #0xf
	movw	r2, #:lower16:g_excpData
	movt	r2, #:upper16:g_excpData
	add	r2, r2, r1, lsl #2
	.endm

/* Rearm the hardware trace after the exception state is restored. */
	.macro	REARM_TRACE
	mrc	p15, 0, r1, c12, c0, 0		@ VBAR
	ldr	r1, [r1, #VECTOR_TRACE_AREA]
	cmp	r1, #0
	mov	r2, #1
	strne	r2, [r1, #TRACE_CONTROL]
	.endm

/*
 * Return into the context in r0 with RFE: r0-r12 are reloaded, the banked
 * user SP/LR restored, and the context's PC and CPSR popped from the
 * exception-mode stack. sp must already be the frame's end.
 */
	.macro	RETURN_FROM_CONTEXT
	adds	ip, r0, #0
	ldm	ip!, {r0-r11}
	adds	ip, ip, #4
	ldm	ip, {sp, lr}^
	subs	ip, ip, #CTX_SP
	ldr	lr, [ip, #CTX_CPSR]
	push	{lr}
	ldr	lr, [ip, #CTX_PC]
	push	{lr}
	ldr	ip, [ip, #CTX_R12]
	clrex
	rfeia	sp!
	.endm
