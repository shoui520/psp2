/*
 * The first handlers of the undefined instruction, prefetch abort and data
 * abort chains. Each builds the SceExcpmgrContext on the exception-mode
 * stack, saves the CPU state, enters the kernel's address space and calls the
 * registered callback and the C policy (ClassifyAbortInstruction for a data
 * abort, PatchUserInstruction for an undefined instruction). A result
 * resumes the interrupted code in the tail; otherwise the handler counts the
 * nesting level, continues in System mode on this core's exception stack with
 * the frame copied there (d0-d31 stored first) and jumps to the next handler
 * of its chain, r0 = context, r1 = 3.
 */
	.syntax	unified
	.arch	armv7-a
	.fpu	neon
	.section .text.excpmgr_initial, "ax", %progbits
	.arm
#include "excpmgr_asm.h"

/* Save the eight words of the PMU counters of the interrupted code. */
	.macro	SAVE_PMU
	mrc	p15, 0, r5, c9, c13, 0		@ PMCCNTR
	mrc	p15, 0, r1, c9, c12, 0		@ PMCR
	mrc	p15, 0, r2, c9, c12, 1		@ PMCNTENSET
	mrc	p15, 0, r3, c9, c12, 1
	mrc	p15, 0, r4, c9, c12, 5		@ PMSELR
	mrc	p15, 0, r6, c9, c14, 0		@ PMUSERENR
	str	r1, [sp, #CTX_PMCR]
	str	r2, [sp, #CTX_PMCNTENSET]
	str	r3, [sp, #CTX_PMCNTENSET2]
	str	r4, [sp, #CTX_PMSELR]
	str	r5, [sp, #CTX_PMCCNTR]
	str	r6, [sp, #CTX_PMUSERENR]
	.irp	counter, 0, 1, 2, 3, 4, 5
	mov	r1, #\counter
	mcr	p15, 0, r1, c9, c12, 5		@ PMSELR
	mrc	p15, 0, r2, c9, c13, 1		@ PMXEVTYPER
	mrc	p15, 0, r3, c9, c13, 2		@ PMXEVCNTR
	str	r2, [sp, #CTX_PMU_COUNTER + \counter * 8]
	str	r3, [sp, #CTX_PMU_COUNTER + \counter * 8 + 4]
	.endr
	.endm

/*
 * Save the CPU state the C policy and the later handlers read: VFP (after
 * enabling it), the context ID, thread pointers, fault status, PAR, TEEHBR,
 * DBGDSCR, the trace area word and the waypoint buffer.
 */
	.macro	SAVE_CPU_STATE excpcode
	mrc	p15, 0, r1, c1, c0, 2		@ CPACR
	str	r1, [sp, #CTX_CPACR]
	mov	r1, #0xf00000
	mcr	p15, 0, r1, c1, c0, 2		@ enable cp10/cp11
	vmrs	r2, fpexc
	vmrs	r3, fpscr
	str	r2, [sp, #CTX_FPEXC]
	str	r3, [sp, #CTX_FPSCR]
	movw	r1, #0
	movt	r1, #0x4000
	vmsr	fpexc, r1
	mov	r1, #0x3000000
	vmsr	fpscr, r1
	mov	r1, #\excpcode
	str	r1, [sp, #CTX_EXCP_CODE]
	mrc	p15, 0, r1, c2, c0, 1		@ TTBR1
	str	r1, [sp, #CTX_TTBR1]
	mrc	p15, 0, r1, c13, c0, 1		@ CONTEXTIDR
	str	r1, [sp, #CTX_CONTEXT_ID]
	mrc	p15, 0, r1, c13, c0, 2		@ TPIDRURW
	str	r1, [sp, #CTX_TPIDRURW]
	mrc	p15, 0, r1, c13, c0, 3		@ TPIDRURO
	str	r1, [sp, #CTX_TPIDRURO]
	mrc	p15, 0, r1, c13, c0, 4		@ TPIDRPRW
	str	r1, [sp, #CTX_THREAD]
	mrc	p15, 0, r1, c3, c0, 0		@ DACR
	str	r1, [sp, #CTX_DACR]
	mrc	p15, 0, r1, c5, c0, 0		@ DFSR
	str	r1, [sp, #CTX_DFSR]
	mrc	p15, 0, r1, c5, c0, 1		@ IFSR
	str	r1, [sp, #CTX_IFSR]
	mrc	p15, 0, r1, c6, c0, 0		@ DFAR
	str	r1, [sp, #CTX_DFAR]
	mrc	p15, 0, r1, c6, c0, 2		@ IFAR
	str	r1, [sp, #CTX_IFAR]
	mrc	p15, 0, r1, c7, c4, 0		@ PAR
	str	r1, [sp, #CTX_PAR]
	mrc	p14, 6, r1, c1, c0, 0		@ TEEHBR
	str	r1, [sp, #CTX_TEEHBR]
	mrc	p14, 0, r1, c0, c1, 0		@ DBGDSCR
	str	r1, [sp, #CTX_DBGDSCR]
	mrc	p15, 0, r1, c12, c0, 0		@ VBAR: this core's vector bank
	ldr	r1, [r1, #VECTOR_TRACE_AREA]
	cmp	r1, #0
	beq	1f
	ldr	r2, [r1, #TRACE_CONTROL]
	str	r2, [sp, #CTX_WAYPOINT_CONTROL]
	adds	r1, r1, #TRACE_WAYPOINT
	adds	r2, sp, #CTX_WAYPOINT
	.rept	32
	ldm	r1!, {r4-r7}
	stm	r2!, {r4-r7}
	.endr
1:
	.endm

/*
 * Stop the kernel's own hardware breakpoints and watchpoints (when set up) and
 * switch to the kernel address space: ASID 0, the kernel TTBR1 and context
 * ID, the kernel domain access, no current thread.
 */
	.macro	ENTER_KERNEL
	mov	r1, #0
	movw	r2, #:lower16:g_excpData + DATA_KERNEL_BREAKPOINTS
	movt	r2, #:upper16:g_excpData + DATA_KERNEL_BREAKPOINTS
	ldr	r2, [r2]
	cmp	r2, #0
	.irp	index, 0, 1, 2, 3, 4, 5
	mcrne	p14, 0, r1, c0, c\index, 5	@ DBGBCR
	.endr
	movw	r2, #:lower16:g_excpData + DATA_KERNEL_WATCHPOINTS
	movt	r2, #:upper16:g_excpData + DATA_KERNEL_WATCHPOINTS
	ldr	r2, [r2]
	cmp	r2, #0
	.irp	index, 0, 1, 2, 3
	mcrne	p14, 0, r1, c0, c\index, 7	@ DBGWCR
	.endr
	movw	r2, #:lower16:g_excpData + DATA_KERNEL_MMU
	movt	r2, #:upper16:g_excpData + DATA_KERNEL_MMU
	ldr	r2, [r2]
	ldr	r3, [r2]
	ldr	r4, [r2, #8]
	mov	r5, #0
	mcr	p15, 0, r5, c13, c0, 1		@ CONTEXTIDR
	isb
	mcr	p15, 0, r3, c2, c0, 1		@ TTBR1
	isb
	mcr	p15, 0, r4, c13, c0, 1		@ CONTEXTIDR
	movw	r3, #:lower16:DACR_KERNEL
	movt	r3, #:upper16:DACR_KERNEL
	mcr	p15, 0, r3, c3, c0, 0		@ DACR
	mov	r1, #0
	mcr	p15, 0, r1, c13, c0, 4		@ TPIDRPRW
	mcr	p15, 0, r1, c13, c0, 3		@ TPIDRURO
	.endm

/*
 * The fault state the handler chain may have changed, written back for the
 * resumed code: fault status and address, user thread registers, address
 * space, domain access, PAR, VFP and the coprocessor access control.
 */
	.macro	RESTORE_FAULT_STATE
	ldr	r1, [sp, #CTX_DFSR]
	mcr	p15, 0, r1, c5, c0, 0		@ DFSR
	ldr	r1, [sp, #CTX_DFAR]
	mcr	p15, 0, r1, c6, c0, 0		@ DFAR
	ldr	r1, [r0, #CTX_TPIDRURO]
	mcr	p15, 0, r1, c13, c0, 3
	ldr	r1, [r0, #CTX_THREAD]
	mcr	p15, 0, r1, c13, c0, 4
	ldr	r1, [r0, #CTX_TTBR1]
	ldr	r2, [r0, #CTX_CONTEXT_ID]
	mov	r3, #0
	mcr	p15, 0, r3, c13, c0, 1
	isb
	mcr	p15, 0, r1, c2, c0, 1
	isb
	mcr	p15, 0, r2, c13, c0, 1
	ldr	r1, [r0, #CTX_DACR]
	mcr	p15, 0, r1, c3, c0, 0
	ldr	r1, [r0, #CTX_PAR]
	mcr	p15, 0, r1, c7, c4, 0
	ldr	r2, [r0, #CTX_FPEXC]
	ldr	r3, [r0, #CTX_FPSCR]
	vmsr	fpexc, r2
	vmsr	fpscr, r3
	ldr	r1, [r0, #CTX_CPACR]
	mcr	p15, 0, r1, c1, c0, 2
	.endm

/*
 * First handler of the undef/pabt/dabt chains. Build the context frame on the
 * exception-mode stack and save the CPU state, enter the kernel's address
 * space and call the registered callback and the C policy
 * (ClassifyAbortInstruction for a data abort, PatchUserInstruction for an
 * undefined instruction: a result continues at \tail, which resumes the
 * interrupted code). Otherwise count the nesting level, continue in System
 * mode on this core's exception stack with the frame copied there (d0-d31
 * stored first), and jump to the next handler of the chain.
 *
 * \lrfix: the bytes subtracted from lr to get the faulting address; thumb
 * for the undefined instruction, whose size depends on the T bit.
 */
	.macro	INITIAL_HANDLER name, excpcode, callback, lrfix, policy, tail
	HANDLER	\name
	subs	sp, sp, #CTX_SIZE
	.irp	reg, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	str	r\reg, [sp, #\reg * 4]
	.endr
	SAVE_PMU
	add	r1, sp, #CTX_SP
	stm	r1, {sp, lr}^			@ banked user registers
	.ifc	\lrfix, thumb
	mrs	r1, spsr
	str	r1, [sp, #CTX_CPSR]
	tst	r1, #0x20
	subeq	lr, lr, #4
	subne	lr, lr, #2
	str	lr, [sp, #CTX_PC]
	.else
	sub	lr, lr, #\lrfix
	str	lr, [sp, #CTX_PC]
	mrs	lr, spsr
	str	lr, [sp, #CTX_CPSR]
	.endif
	SAVE_CPU_STATE \excpcode
	ENTER_KERNEL
	mov	r0, sp
	push	{r0-r3, ip, lr}
	movw	ip, #:lower16:\callback
	movt	ip, #:upper16:\callback
	ldr	ip, [ip]
	cmp	ip, #0
	blxne	ip				@ callback(context, 0, kernelMmu, DACR)
	pop	{r0-r3, ip, lr}
	.ifc	\policy, classify
	push	{r0-r3, ip, lr}
	ldr	r1, [r0, #CTX_CPSR]
	ldr	r0, [r0, #CTX_PC]
	blx	ClassifyAbortInstruction
	cmp	r0, #0
	movne	r4, r0
	pop	{r0-r3, ip, lr}
	bne	\tail
	.endif
	.ifc	\policy, patch
	push	{r0-r3, ip, lr}
	blx	PatchUserInstruction
	cmp	r0, #0
	movne	r4, r0
	pop	{r0-r3, ip, lr}
	bne	\tail
	.endif
	NEST_COUNT_ADDRESS
	ldr	r3, [r2]
	add	r3, r3, #1
	str	r3, [r2]
	adds	sp, sp, #CTX_SIZE
	cps	#0x1f				@ System mode: user SP/LR banked
	movw	r2, #:lower16:g_excpData + DATA_STACK_TOP
	movt	r2, #:upper16:g_excpData + DATA_STACK_TOP
	add	r2, r2, r1, lsl #2
	ldr	fp, [r2]
	mov	sp, fp				@ this core's exception stack
	subs	sp, sp, #CTX_SIZE
	mov	fp, r0				@ frame on the exception-mode stack
	mov	r1, sp
	.rept	8
	ldm	r0!, {r2-r9}
	stm	r1!, {r2-r9}
	.endr
	mov	r0, sp
	adds	r1, sp, #CTX_VFP
	vstmia	r1!, {d0-d15}
	vstmia	r1!, {d16-d31}
	push	{r0, lr}
	add	r0, r0, #CTX_WAYPOINT
	add	r1, fp, #CTX_WAYPOINT
	mov	r2, #0x200
	bl	memcpy				@ the waypoints
	pop	{r0, lr}
	mov	r1, #3
	movw	r2, #:lower16:\name
	movt	r2, #:upper16:\name
	ldr	r2, [r2]			@ next handler of the chain
	add	r2, r2, #8
	mov	pc, r2
	END_FUNCTION \name
	.endm

	INITIAL_HANDLER initialDabtHandler, 4, g_dabtCallback, 8, classify, initialDabtResume

/*
 * A data abort the policy handled: r4 is 12 (resume at the address the
 * unprivileged access helper staged in r12), a negative error (the faulting
 * instruction returns it in r0 and is skipped) or the new PC.
 */
	FUNCTION initialDabtResume
	cmp	r4, #12
	moveq	r4, ip
	beq	1f
	cmp	r4, #0
	strmi	r4, [r0]
	ldr	r4, [r0, #CTX_PC]
	addmi	r4, r4, #4
1:	str	r4, [r0, #CTX_PC]
	RESTORE_FAULT_STATE
	bl	excpRestoreDebugRegisters
	REARM_TRACE
	adds	sp, sp, #CTX_SIZE
	RETURN_FROM_CONTEXT
	END_FUNCTION initialDabtResume

	INITIAL_HANDLER initialPabtHandler, 3, g_pabtCallback, 4, none, 0

	INITIAL_HANDLER initialUndefHandler, 1, g_undefCallback, thumb, patch, initialUndefResume

/* An undefined instruction the policy patched: r4 is the PC to resume at. */
	FUNCTION initialUndefResume
	str	r4, [r0, #CTX_PC]
	RESTORE_FAULT_STATE
	NEST_COUNT_ADDRESS
	ldr	r3, [r2]
	subs	r3, r3, #1
	bne	excpPanicNested
	str	r3, [r2]
	bl	excpRestoreDebugRegisters
	REARM_TRACE
	adds	sp, sp, #CTX_SIZE
	RETURN_FROM_CONTEXT
	END_FUNCTION initialUndefResume
