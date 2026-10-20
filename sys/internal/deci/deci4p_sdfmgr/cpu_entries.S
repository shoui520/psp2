/*
 * The debugger's exception and SDIO interrupt entries. Excpmgr and Intrmgr
 * call them with a live register image; each returns with RFE into the saved
 * context, restoring the system registers, VFP state and the hardware break
 * and watch points of the code it resumes.
 */
	.syntax	unified
	.arch	armv7-a
	.fpu	neon
	.section .text.sdfMgrCpuEntries, "ax", %progbits
	.arm

/* SceExcpmgrContext (excpmgr/excpmgr.h) */
#define CTX_R12			0x30
#define CTX_SP			0x34
#define CTX_PC			0x3c
#define CTX_EXCP_CODE		0x40
#define CTX_CPSR		0x44
#define CTX_CPACR		0x48
#define CTX_FPSCR		0x4c
#define CTX_FPEXC		0x50
#define CTX_CONTEXT_ID		0x54
#define CTX_TPIDRURO		0x5c
#define CTX_THREAD		0x60
#define CTX_TTBR1		0x64
#define CTX_DACR		0x6c
#define CTX_PAR			0x80
#define CTX_VFP			0x100

/* SceExcpmgrData: the kernel's break and watch points */
#define DATA_KERNEL_BREAKPOINTS	0x44
#define DATA_KERNEL_WATCHPOINTS	0x48

/* ThreadCB, its process's hardware break and watch points */
#define THREAD_REGISTERS	0x0c
#define THREAD_VFP		0x10
#define THREAD_VFP_OWNER	0x14
#define THREAD_PROCESS		0x18
#define THREAD_THBP_VALUE	0x1c
#define THREAD_THBP_CONTROL	0x20
#define PROCESS_HW_BREAKPOINTS	0x0c
#define PROCESS_HW_WATCHPOINTS	0x10
#define HW_REGISTERS		0x08

/* ThreadRegisterContext, walked downwards from its end */
#define REGS_KERNEL_END		0xc0
#define REGS_USER_DACR		0x44
#define REGS_KERNEL_DACR	0x48
#define REGS_RETURN_PC		0xc0
#define REGS_RETURN_CPSR	0xc4
#define REGS_PMCCNTR		0xf0

/* ExcpVectorBank (VBAR) and its trace area */
#define VECTOR_TRACE_AREA	0x24
#define VECTOR_NESTING		0x28
#define TRACE_CONTROL		0x100

/* the interrupt stack's guard word, one stack of 0x2000 bytes per level */
#define IRQ_STACK_SIZE		0x2000
#define IRQ_STACK_GUARD		0x1ab4e8df

#define SDIO_INTR_COMPLETE	0x10

	.macro	FUNCTION name
	.global	\name
	.type	\name, %function
\name:
	.endm

	.macro	END_FUNCTION name
	.size	\name, . - \name
	.endm

/* Rearm the hardware trace after the state is restored. */
	.macro	REARM_TRACE
	mrc	p15, 0, r4, c12, c0, 0		@ VBAR
	ldr	r4, [r4, #VECTOR_TRACE_AREA]
	cmp	r4, #0
	mov	r5, #1
	strne	r5, [r4, #TRACE_CONTROL]
	.endm

/*
 * An exception handler: the two link words Excpmgr fills in, then the entry.
 * The C handler gets the context's CPSR and PC; a result with any of its low
 * four bits set passes the exception on to the next handler in the chain.
 */
	.macro	EXCEPTION_ENTRY name, handler
	FUNCTION \name
	.word	0, 0
	ldr	r2, [r0, #CTX_CPSR]
	mov	r4, r0
	ldr	r1, [r0, #CTX_PC]
	stmdb	sp!, {lr}
	sub	sp, sp, #4
	cpsie	i
	blx	\handler
	cpsid	i
	mov	r1, r0
	mov	r0, r4
	add	sp, sp, #4
	ldmia	sp!, {lr}
	and	r2, r1, #0xf
	cmp	r2, #0
	bne	1f
	mrc	p15, 0, r1, c0, c0, 5		@ MPIDR
	and	r1, r1, #0xf
	movw	r2, #:lower16:s_sdfMgrState
	movt	r2, #:upper16:s_sdfMgrState
	ldr	r2, [r2]			@ Excpmgr's data: nesting per core
	add	r2, r2, r1, lsl #2
	ldr	r3, [r2]
	subs	r3, r3, #1
	bne	sdfMgrExcpNested
	str	r3, [r2]
	mov	r1, #0
	mcr	p15, 0, r1, c5, c0, 0		@ DFSR
	mcr	p15, 0, r1, c5, c0, 1		@ IFSR
	mcr	p15, 0, r1, c5, c1, 0		@ ADFSR
	mcr	p15, 0, r1, c5, c1, 1		@ AIFSR
	mcr	p15, 0, r1, c6, c0, 0		@ DFAR
	mcr	p15, 0, r1, c6, c0, 2		@ IFAR
	ldr	r1, [r0, #CTX_THREAD]
	cmp	r2, #0
	mcrne	p15, 0, r1, c13, c0, 4		@ TPIDRPRW
	b	sdfMgrReturnFromException
1:	ldr	r2, =\name
	ldr	r2, [r2]
	add	r2, r2, #8
	mov	pc, r2
	END_FUNCTION \name
	.endm

	EXCEPTION_ENTRY sdfMgrPrefetchAbortEntry, sdfMgrPrefetchAbortHandler
	EXCEPTION_ENTRY sdfMgrDataAbortEntry, sdfMgrDataAbortHandler
	EXCEPTION_ENTRY sdfMgrUndefinedEntry, sdfMgrUndefinedHandler

/* An exception taken inside another one: report it for ever. */
	FUNCTION sdfMgrExcpNested
	push	{r0, r1, r2, r3, ip, lr}
	blx	sceSDfMgrExcpAssert
	pop	{r0, r1, r2, r3, ip, lr}
	b	sdfMgrExcpNested
	END_FUNCTION sdfMgrExcpNested

/*
 * Return into the exception context in r0: its address space, VFP state and
 * CPACR, then the break and watch points of its thread (the kernel's without
 * one) from Abort mode, whose stack takes the RFE frame.
 */
	FUNCTION sdfMgrReturnFromException
	ldr	r1, [r0, #CTX_TPIDRURO]
	mcr	p15, 0, r1, c13, c0, 3
	ldr	r1, [r0, #CTX_TTBR1]
	ldr	r2, [r0, #CTX_CONTEXT_ID]
	mov	r3, #0
	mcr	p15, 0, r3, c13, c0, 1		@ CONTEXTIDR
	isb
	mcr	p15, 0, r1, c2, c0, 1		@ TTBR1
	isb
	mcr	p15, 0, r2, c13, c0, 1
	ldr	r1, [r0, #CTX_DACR]
	mcr	p15, 0, r1, c3, c0, 0
	ldr	r1, [r0, #CTX_PAR]
	mcr	p15, 0, r1, c7, c4, 0
	ldr	r1, [r0, #CTX_EXCP_CODE]
	cmp	r1, #0
	beq	1f				@ no VFP state
	ldr	r2, [r0, #CTX_FPEXC]
	ldr	r3, [r0, #CTX_FPSCR]
	vmsr	fpexc, r2
	vmsr	fpscr, r3
	adds	r2, r0, #CTX_VFP
	vldmia	r2!, {d0-d15}
	vldmia	r2!, {d16-d31}
1:	ldr	r1, [r0, #CTX_CPACR]
	mcr	p15, 0, r1, c1, c0, 2
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrGetResumeBreakpointValue
	mov	r9, r0
	pop	{r0, r1, r2, r3, ip, lr}
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrTakeResumeBreakpointControl
	mov	sl, r0
	pop	{r0, r1, r2, r3, ip, lr}
	cps	#0x17				@ Abort mode
	ldr	r4, [r0, #CTX_THREAD]
	cmp	r4, #0
	movweq	r5, #:lower16:s_sdfMgrState
	movteq	r5, #:upper16:s_sdfMgrState
	ldreq	r5, [r5]
	addeq	r5, r5, #DATA_KERNEL_BREAKPOINTS
	ldreq	r6, [r5]
	ldrne	r5, [r4, #THREAD_PROCESS]
	ldrne	r6, [r5, #PROCESS_HW_BREAKPOINTS]
	cmp	r6, #0
	beq	2f
	add	r6, r6, #HW_REGISTERS
	ldm	r6!, {r7, r8}			@ breakpoint 0: the thread's own
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r9, c0, c1, 4		@ breakpoint 1: the resume point
	mcr	p14, 0, sl, c0, c1, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c2, 4
	mcr	p14, 0, r8, c0, c2, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c3, 4
	mcr	p14, 0, r8, c0, c3, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c4, 4
	mcr	p14, 0, r8, c0, c4, 5
	cmp	r4, #0
	movweq	r5, #:lower16:s_sdfMgrState
	movteq	r5, #:upper16:s_sdfMgrState
	ldreq	r5, [r5]
	addeq	r5, r5, #DATA_KERNEL_WATCHPOINTS
	ldreq	r6, [r5]
	ldrne	r5, [r4, #THREAD_PROCESS]
	ldrne	r6, [r5, #PROCESS_HW_BREAKPOINTS]	@ as Sony's: the breakpoints again
	cmp	r6, #0
	beq	2f
	add	r6, r6, #HW_REGISTERS
	ldm	r6!, {r7, r8}			@ watchpoint 0 is not restored
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c1, 6
	mcr	p14, 0, r8, c0, c1, 7
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c2, 6
	mcr	p14, 0, r8, c0, c2, 7
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c3, 6
	mcr	p14, 0, r8, c0, c3, 7
	cmp	r4, #0
	beq	2f
	ldr	r6, [r4, #THREAD_THBP_VALUE]	@ breakpoint 5: the thread's
	mcr	p14, 0, r6, c0, c5, 4
	ldr	r6, [r4, #THREAD_THBP_CONTROL]
	mcr	p14, 0, r6, c0, c5, 5
2:	REARM_TRACE
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
	.ltorg
	.balign	16, 0
	END_FUNCTION sdfMgrReturnFromException

/*
 * The SDIO interrupt handler, called on Intrmgr's IRQ frame. When the
 * debugger stopped the system (SDIO_INTR_COMPLETE), it ends the interrupt
 * itself and returns into the interrupted context: a thread's saved context
 * when the outermost level ends, else Intrmgr's nested frame.
 */
	FUNCTION sdioIntrEntry
	.word	0, 0
	stmdb	sp!, {lr}
	sub	sp, sp, #4
	blx	sdioIntrHandler
	add	sp, sp, #4
	ldmia	sp!, {lr}
	cmp	r0, #SDIO_INTR_COMPLETE
	beq	1f
	bx	lr
1:	cpsid	i
	pop	{r4, ip}			@ the interrupted thread, the acknowledge
	push	{r0, r1, r2, r3, ip, lr}
	bl	SceIntrmgrForKernel_807B2004
	mov	r5, r0				@ the GIC CPU interface
	pop	{r0, r1, r2, r3, ip, lr}
	mov	lr, r5
	str	ip, [lr, #0x10]			@ end of interrupt
	mrc	p15, 0, r2, c12, c0, 0		@ VBAR
	ldr	r2, [r2, #VECTOR_NESTING]
	ldr	r0, [r2]
	mrc	p15, 0, r3, c0, c0, 5		@ MPIDR
	and	r3, r3, #0xf
	mov	r6, r3
	push	{r0, r1, r2, r3, ip, lr}
	bl	SceIntrmgrForDriver_182EE3E3
	mov	r7, r0
	pop	{r0, r1, r2, r3, ip, lr}
	push	{r0, r1, r2, r3, ip, lr}
	mov	r0, r6
	mov	r1, r7
	bl	SceIntrmgrForKernel_01911612
	mov	r8, r0				@ this level's interrupt stack
	pop	{r0, r1, r2, r3, ip, lr}
	mov	r1, r8
	mov	r3, #IRQ_STACK_SIZE
	mul	r5, r3, r0
	sub	r1, r1, r5
	ldr	r1, [r1]
	movw	r3, #:lower16:IRQ_STACK_GUARD
	movt	r3, #:upper16:IRQ_STACK_GUARD
	cmp	r1, r3
	bne	.LstackBroken
	subs	r0, r0, #1
	str	r0, [r2]
	bne	.LnestedReturn
	cmp	r4, #0
	beq	.LnestedReturn

	/* the outermost level: return into the thread's saved context */
	mcr	p15, 0, r4, c13, c0, 4		@ TPIDRPRW
	ldr	r0, [r4, #THREAD_REGISTERS]
	add	r1, r0, #REGS_KERNEL_END
	ldmdb	r1!, {r2}			@ the GIC priority mask
	push	{r0, r1, r2, r3, ip, lr}
	bl	SceIntrmgrForKernel_807B2004
	mov	r4, r0
	pop	{r0, r1, r2, r3, ip, lr}
	mov	r3, r4
	str	r2, [r3, #4]
	mrc	p15, 0, ip, c13, c0, 4
	ldmdb	r1!, {r4-r8}
	mcr	p14, 6, r8, c1, c0, 0		@ TEEHBR
	mcr	p15, 0, r7, c6, c0, 2		@ IFAR
	mcr	p15, 0, r6, c6, c0, 0		@ DFAR
	mcr	p15, 0, r5, c5, c0, 1		@ IFSR
	mcr	p15, 0, r4, c5, c0, 0		@ DFSR
	ldr	fp, [r0, #REGS_PMCCNTR]
	mcr	p15, 0, fp, c9, c12, 2		@ PMCNTENCLR
	.irp	counter, 5, 4, 3, 2, 1, 0
	mov	r9, #\counter
	ldrd	r4, r5, [r1, #-8]!
	and	r4, r4, #0xff
	mcr	p15, 0, r9, c9, c12, 5		@ PMSELR
	mcr	p15, 0, r4, c9, c13, 1		@ PMXEVTYPER
	mcr	p15, 0, r5, c9, c13, 2		@ PMXEVCNTR
	.endr
	ldr	r8, [r1, #-4]!
	mcr	p15, 0, r8, c9, c13, 0		@ PMCCNTR
	ldmdb	r1!, {r2-r10}
	mcr	p15, 0, sl, c9, c14, 0		@ PMUSERENR
	mcr	p15, 0, r9, c9, c12, 5		@ PMSELR
	mcr	p15, 0, r8, c9, c12, 3		@ PMOVSR
	mcr	p15, 0, r7, c9, c12, 0		@ PMCR
	mcr	p15, 0, r6, c7, c4, 0		@ PAR
	mcr	p15, 0, r3, c13, c0, 3		@ TPIDRURO
	mcr	p15, 0, r2, c13, c0, 2		@ TPIDRURW
	mov	r2, #0
	mcr	p15, 0, r2, c13, c0, 1		@ CONTEXTIDR
	isb
	mcr	p15, 0, r4, c2, c0, 1		@ TTBR1
	isb
	mcr	p15, 0, r5, c13, c0, 1
	ldr	r2, [r1, #-4]!
	mcr	p15, 0, r2, c1, c0, 2		@ CPACR
	sub	r1, r1, #8
	mrc	p15, 0, r4, c13, c0, 4
	ldr	r5, [r4, #THREAD_VFP]
	cmp	r5, #0
	movne	r6, #0x40000000			@ FPEXC.EN
	vmsrne	fpexc, r6
	movne	r6, #0
	vmsrne	fpscr, r6
	vmsrne	fpexc, r6
	mrcne	p15, 0, r6, c12, c0, 0
	ldrne	r6, [r6, #VECTOR_TRACE_AREA]
	ldrne	r7, [r4, #THREAD_VFP_OWNER]
	strne	r7, [r6]
	ldrne	r6, [r5, #0x104]
	vmsrne	fpexc, r6
	ldrne	r6, [r5, #0x100]
	vmsrne	fpscr, r6
	ldr	r2, [r0]
	cmp	r2, #1
	mov	sl, r2
	beq	2f

	/* a thread that gave up the CPU: callee-saved VFP registers only */
	cmp	r5, #0
	beq	1f
	movw	r6, #:lower16:.LvfpPoison
	movt	r6, #:upper16:.LvfpPoison
	vldr	d0, [r6]
	vorr	d1, d0, d0
	vorr	q2, q0, q0
	vorr	q3, q0, q0
	vorr	q8, q0, q0
	vorr	q9, q0, q0
	vorr	q10, q0, q0
	vorr	q11, q0, q0
	vorr	q12, q0, q0
	vorr	q13, q0, q0
	vorr	q14, q0, q0
	vorr	q15, q0, q0
	add	r5, r5, #0x40
	vldmia	r5!, {d8-d15}
1:	ldmdb	r1!, {r3}
	ldmdb	r1!, {r2}
	ldmdb	r1!, {sp}
	b	3f

	/* an interrupted thread: all of its registers */
2:	cmp	r5, #0
	vldmiane r5!, {d0-d15}
	vldmiane r5!, {d16-d31}
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrGetResumeBreakpointValue
	mov	r7, r0
	pop	{r0, r1, r2, r3, ip, lr}
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrTakeResumeBreakpointControl
	mov	r8, r0
	pop	{r0, r1, r2, r3, ip, lr}
	sub	r1, r1, #4
	ldmdb	r1!, {lr}
	ldmdb	r1!, {sp}
	ldr	r2, [r0, #REGS_RETURN_PC]
	ldr	r3, [r0, #REGS_RETURN_CPSR]
3:	cps	#0x12				@ IRQ mode
	and	r5, r3, #0x1f
	cmp	r5, #0x10			@ returning to user mode
	ldreq	r5, [r0, #REGS_USER_DACR]
	ldrne	r5, [r0, #REGS_KERNEL_DACR]
	mcr	p15, 0, r5, c3, c0, 0		@ DACR
	ldr	r5, [r4, #THREAD_PROCESS]
	ldr	r6, [r5, #PROCESS_HW_BREAKPOINTS]
	cmp	r6, #0
	beq	4f
	add	r6, r6, #HW_REGISTERS
	mcr	p14, 0, r7, c0, c1, 4		@ breakpoint 1: the resume point
	mcr	p14, 0, r8, c0, c1, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c0, 4
	mcr	p14, 0, r8, c0, c0, 5
	ldm	r6!, {r7, r8}
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c2, 4
	mcr	p14, 0, r8, c0, c2, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c3, 4
	mcr	p14, 0, r8, c0, c3, 5
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c4, 4
	mcr	p14, 0, r8, c0, c4, 5
	ldr	r6, [r5, #PROCESS_HW_WATCHPOINTS]
	cmp	r6, #0
	beq	4f
	add	r6, r6, #HW_REGISTERS
	ldm	r6!, {r7, r8}
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c1, 6
	mcr	p14, 0, r8, c0, c1, 7
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c2, 6
	mcr	p14, 0, r8, c0, c2, 7
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c3, 6
	mcr	p14, 0, r8, c0, c3, 7
	ldr	r6, [r4, #THREAD_THBP_VALUE]
	mcr	p14, 0, r6, c0, c5, 4
	ldr	r6, [r4, #THREAD_THBP_CONTROL]
	mcr	p14, 0, r6, c0, c5, 5
4:	mrc	p14, 0, r0, c0, c2, 2		@ DBGDSCR
	orr	r0, r0, #0x8800			@ monitor debug, ITR enable
	mcr	p14, 0, r0, c0, c2, 2
	add	sp, sp, #0x38
	cmp	sl, #1
	mcr	p15, 0, fp, c9, c12, 1		@ PMCNTENSET
	beq	5f
	REARM_TRACE
	push	{r2, r3}
	sub	r1, r1, #4
	ldmdb	r1!, {r4-r11}
	mov	r0, #1
	clrex
	rfeia	sp!
5:	REARM_TRACE
	push	{r2, r3}
	ldmdb	r1!, {r0}
	stmdb	sp!, {r0}
	mov	ip, r1
	ldmdb	ip!, {r0-r11}
	ldmia	sp!, {ip}
	clrex
	rfeia	sp!

	/* not the outermost level: return into Intrmgr's nested frame */
.LnestedReturn:
	pop	{r0, r1}
	vmsr	fpexc, r0
	vmsr	fpscr, r1
	vpop	{d0-d15}
	vpop	{d16-d31}
	pop	{r5-r11, ip}
	ldmia	sp!, {r1}
	ldmia	sp!, {lr}
	stmdb	sp!, {r6}
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrGetResumeBreakpointValue
	mov	r6, r0
	pop	{r0, r1, r2, r3, ip, lr}
	mcr	p14, 0, r6, c0, c1, 4
	push	{r0, r1, r2, r3, ip, lr}
	blx	sdfMgrTakeResumeBreakpointControl
	mov	r6, r0
	pop	{r0, r1, r2, r3, ip, lr}
	mov	r0, r6
	ldmia	sp!, {r6}
	mov	sp, r1
	cps	#0x12
	mcr	p14, 0, r0, c0, c1, 5
	mrc	p14, 0, r0, c0, c2, 2
	orr	r0, r0, #0x8800
	mcr	p14, 0, r0, c0, c2, 2
	ldr	r0, [sp, #0x20]
	ldr	r1, [sp, #0x28]
	mov	r2, #0
	mcr	p15, 0, r2, c13, c0, 1
	isb
	mcr	p15, 0, r0, c2, c0, 1
	isb
	mcr	p15, 0, r1, c13, c0, 1
	ldr	r0, [sp, #0x24]
	mcr	p15, 0, r0, c3, c0, 0
	ldr	r0, [sp, #0x2c]
	mcr	p15, 0, r0, c7, c4, 0
	mrc	p15, 0, r0, c12, c0, 0
	ldr	r0, [r4, #VECTOR_TRACE_AREA]	@ as Sony's: r4, not the VBAR just read
	cmp	r0, #0
	mov	r1, #1
	strne	r1, [r0, #TRACE_CONTROL]
	ldr	r0, [sp]
	ldr	r1, [sp, #4]
	ldr	r2, [sp, #8]
	add	sp, sp, #0x30
	clrex
	rfeia	sp!

.LstackBroken:
	mov	r1, ip
	b	.

/* fills the caller-saved VFP registers of a thread that gave up the CPU */
	.balign	8
.LvfpPoison:
	.word	0x7f80dead, 0x7ff8dead
	.balignl 64, 0xe320f000		@ nop
	END_FUNCTION sdioIntrEntry

	.section .note.GNU-stack, "", %progbits
