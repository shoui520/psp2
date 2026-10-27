/*
 * The interrupt manager's hand written assembly, one object in Sony's link
 * order: the IRQ handler (ARM) with the thunks of its hooks, the register
 * context save and resume of a thread (ARM), the nesting stack guards and
 * small accessors (Thumb), the supervisor call handler (Thumb), and the copy
 * of the controller state (Thumb).
 */
#include "intrmgr_asm.h"

#define INTR_CPU_STACK_SIZE	0x20000
#define INTR_LEVEL_STACK_SIZE	0x2000
#define INTR_STACK_MAGIC	0x1ab4e8df
#define FRAME_SIZE		0x60
#define FRAME_SYSCALL_AUX	0x34
#define FRAME_ENTRY		0x38
#define FRAME_FPSCR		0x3c
#define FRAME_API_DECLARE	0x40
#define FRAME_TPIDRURW		0x44
#define FRAME_SP		0x50
#define FRAME_LR		0x54
#define FRAME_PC		0x58
#define FRAME_CPSR		0x5c
#define RESULTS_SIZE		16

	.syntax	unified
	.arch	armv7-a
	.arch_extension sec
	.fpu	neon
	.section .text.intrmgr_asm, "ax", %progbits

#if defined(INTRMGR_NSKBL)
	.arm
/*
 * NSKBL's IRQ handler: the boot loader's simpler one, without threads, hooks or
 * traces. It saves r0, r1, r12, lr and the SPSR below the IRQ stack, takes the
 * interrupt (1023: spurious), enters its handler on the System-mode stack of
 * the nesting level, with the VFP state saved and interrupts on, disables an
 * SPI the handler returned other than -1 for, ends the interrupt and returns
 * with RFE.
 */
	FUNCTION interruptEntry
	.word	0, 0
	sub	lr, lr, #4
	str	lr, [sp, #-0x1c]
	str	r0, [sp, #-4]
	str	r1, [sp, #-8]
	str	r12, [sp, #-0x18]
	LOAD_GLOBAL r0, g_intrCpuInterface
	ldr	r12, [r0, #12]				@ GICC_IAR
	ldr	r1, .LspuriousInterrupt
	and	r0, r12, r1
	cmp	r0, r1
	bne	.LfindHandler
	ldr	r12, [sp, #-0x18]
	ldr	r1, [sp, #-8]
	ldr	r0, [sp, #-4]
	movs	pc, lr
.LspuriousInterrupt:
	.word	0x3ff
.LfindHandler:
	mov	lr, #INTR_HANDLER_SIZE
	mul	r1, r0, lr
	LOAD_ADDRESS r0, g_intrState
	add	r1, r1, r0
	ldr	lr, [r1]
	cmp	lr, #0
	bne	.LenterHandler
	ldr	r12, [sp, #-0x18]
	ldr	r1, [sp, #-8]
	ldr	r0, [sp, #-4]
	ldr	lr, [sp, #-0x1c]
	movs	pc, lr
.LenterHandler:
	mrs	lr, spsr
	str	lr, [sp, #-0x20]
	mov	r0, sp
	cps	#CPSR_MODE_SYSTEM
	str	r2, [r0, #-0xc]
	str	r3, [r0, #-0x10]
	mrc	p15, 0, r3, c0, c0, 5			@ MPIDR
	and	r3, r3, #0xf
	mov	r2, #INTR_CPU_STATE_SIZE
	mul	r2, r3, r2
	LOAD_ADDRESS r3, g_intrCpu
	add	r2, r2, r3
	ldr	r3, [r2]				@ nesting
	add	r3, r3, #1
	str	r3, [r2]
	sub	r3, r3, #1
	stmdb	sp!, {r1}
	mrc	p15, 0, r1, c0, c0, 5			@ MPIDR
	and	r1, r1, #0xf
	mov	r2, #INTR_CPU_STACK_SIZE
	mul	r1, r2, r1
	LOAD_ADDRESS r2, g_intrStackGuard
	sub	r2, r2, r1
	mov	r1, #INTR_LEVEL_STACK_SIZE
	mul	r1, r3, r1
	sub	r1, r2, r1
	ldr	r2, [r0, #-4]
	ldr	r3, [r0, #-8]
	stmdb	r1!, {r2, r3}
	ldr	r2, [r0, #-0x1c]
	ldr	r3, [r0, #-0x20]
	stmdb	r1!, {r2, r3}
	ldmia	sp!, {r2}
	stmdb	r1!, {sp, lr}
	mov	sp, r1
	mov	r1, r2
	ldr	lr, [r0, #-0x18]
	push	{r4-r11, lr}
	ldr	r3, [r0, #-0x10]
	ldr	r2, [r0, #-0xc]
	push	{r2, r3}
	vpush	{d16-d31}
	vpush	{d0-d15}
	vmrs	r5, fpexc
	vmrs	r6, fpscr
	push	{r5, r6}
	mov	r3, #0x3000000				@ FPSCR: default NaN, flush to zero
	vmsr	fpscr, r3
	stmdb	sp!, {r12}
	cpsie	i
	mov	r12, r1
	ldr	lr, [r12]
	ldrh	r0, [r12, #INTR_HANDLER_CODE]
	ldr	r1, [r12, #INTR_HANDLER_ARG]
	ldrh	r2, [r12, #INTR_HANDLER_PRIORITY]
	blx	lr
	cpsid	i
	ldmia	sp!, {r12}
	cmn	r0, #1
	beq	.LendInterrupt
	bic	r0, r12, #0x1c00
	cmp	r0, #31
	bls	.LendInterrupt
	bic	r1, r0, #0x3e0
	mov	r2, #1
	mov	r2, r2, lsl r1
	mov	r1, r0, lsr #5
	mov	r1, r1, lsl #2
	LOAD_GLOBAL lr, g_intrDistributor
	add	r3, lr, #0x180				@ GICD_ICENABLER
	str	r2, [r3, r1]
.LendInterrupt:
	LOAD_GLOBAL lr, g_intrCpuInterface
	str	r12, [lr, #16]				@ GICC_EOIR
	mrc	p15, 0, r3, c0, c0, 5			@ MPIDR
	and	r3, r3, #0xf
	mov	r2, #INTR_CPU_STATE_SIZE
	mul	r1, r2, r3
	LOAD_ADDRESS r2, g_intrCpu
	add	r2, r1, r2
	ldr	r0, [r2]
	sub	r0, r0, #1
	str	r0, [r2]
	ldmia	sp!, {r0}
	vmsr	fpexc, r0
	ldmia	sp!, {r0}
	vmsr	fpscr, r0
	vpop	{d0-d15}
	vpop	{d16-d31}
	pop	{r2-r12}
	ldmia	sp!, {r1}
	ldmia	sp!, {lr}
	mov	r0, sp
	mov	sp, r1
	ldm	r0!, {r1}
	str	r1, [sp, #-8]
	ldm	r0!, {r1}
	str	r1, [sp, #-4]
	sub	sp, sp, #8
	ldm	r0!, {r1}
	stmdb	sp!, {r1}
	ldm	r0!, {r1}
	ldmia	sp!, {r0}
	clrex
	rfeia	sp!
	END_FUNCTION interruptEntry

/* int intrmgrSmc(uint32_t paddr, uint32_t command): SCE_MONCALL_SET_SHARED_MEMORY. */
	FUNCTION_DEFAULT_UNWIND intrmgrSmc
	movw	ip, #0x101
	smc	#0
	bx	lr
	END_FUNCTION intrmgrSmc

	FUNCTION_DEFAULT_UNWIND enableInterrupts
	cpsie	i
	bx	lr
	END_FUNCTION enableInterrupts

	FUNCTION_DEFAULT_UNWIND SceKblForKernel_DDB3A1A8
	cpsid	i
	bx	lr
	END_FUNCTION SceKblForKernel_DDB3A1A8

/* nothing in NSKBL calls it (os0: buserror's busErrorSmc10C) */
	FUNCTION_DEFAULT_UNWIND intrmgrSmc10C
	movw	ip, #0x10c
	smc	#0
	bx	lr
	END_FUNCTION intrmgrSmc10C

	.balign	32
#else

	.arm
/*
 * The IRQ exception handler of the interrupt manager and the pieces of it the
 * handler jumps to: the setup of a nested interrupt level, the return from
 * one, the return for an interrupt nobody handles, and the thunks that call
 * the interrupt enter and leave hooks.
 *
 * Only what C cannot express: the handler runs in IRQ mode before any
 * register may be touched, captures the interrupted CPU state (CP15, PMU, VFP
 * and the banked registers), switches stacks and modes and returns with RFE.
 * Every handler is a SceExcpmgrHandler: two link words patched by the
 * registration functions, then ARM code entered at +8.
 *
 * Register use: r0 the frame (IntrIrqFrame) on the IRQ stack, r1 the interrupt
 * record (IntrHandler), r12 the acknowledged interrupt, r4 the interrupted
 * thread (0 when the kernel was interrupted), r11 the interrupted PMU counter
 * enable.
 */
/*
 * A nested interrupt: form the System-mode stack of its level (r3 is the
 * nesting level minus one, r4 the CPU), save the live registers on it and go
 * on to the handler.
 */
	.type	nestedInterrupt, %function
nestedInterrupt:
	add	r3, r3, #1
	LOAD_ADDRESS r2, g_intrStackGuard
	sub	r2, r2, r4, lsl #17
	sub	r4, r2, r3, lsl #13
	ldrd	r2, r3, [r0, #IRQ_FRAME_PC]
	stmdb	r4!, {r2, r3}
	stmdb	r4!, {sp, lr}
	mov	sp, r4
	push	{r5-r12}
	vpush	{d16-d31}
	vpush	{d0-d15}
	vmrs	r5, fpexc
	vmrs	r6, fpscr
	push	{r5, r6}
	ldr	r2, [r0, #IRQ_FRAME_THREAD]
	b	.LdispatchInterrupt

/* The return from a nested interrupt back to the interrupted IRQ frame. */
	.type	returnNested, %function
returnNested:
	pop	{r0, r1}
	vmsr	fpexc, r0
	vmsr	fpscr, r1
	vpop	{d0-d15}
	vpop	{d16-d31}
	REARM_TRACE r12, lr
	pop	{r5-r12}
	ldmfd	sp!, {r1}
	ldmfd	sp!, {lr}
	mov	sp, r1
	cps	#CPSR_MODE_IRQ
	ldm	sp, {r0-r3}
	mcr	p15, 0, r1, c3, c0, 0			@ DACR
	mcr	p15, 0, r3, c7, c4, 0			@ PAR
	mov	r1, #0
	mcr	p15, 0, r1, c13, c0, 1			@ CONTEXTIDR
	isb
	mcr	p15, 0, r0, c2, c0, 1			@ TTBR1
	isb
	mcr	p15, 0, r2, c13, c0, 1			@ CONTEXTIDR
	add	sp, sp, #32
	pop	{r0-r4, r12}
	clrex
	rfeia	sp!

/* An interrupt nobody handles (spurious or without a record). */
	.type	returnUnhandled, %function
returnUnhandled:
	pop	{r0-r4, r12}
	add	sp, sp, #8
	movs	pc, lr
	.balign	16

/* The enter hook of the interrupt dispatch (g_intrEnterIntr). */
	.global	intrEnterHook
	.type	intrEnterHook, %function
intrEnterHook:
	LOAD_GLOBAL r3, g_intrEnterIntr
	cmp	r3, #0
	beq	.LcaptureSystem
	push	{r0, r1, r4, r12}
	ldrh	r4, [r1, #INTR_HANDLER_PRIORITY]
	ldr	r2, [r1, #INTR_HANDLER_ARG]
	movs	r4, r4, lsr #4
	ldr	r1, [r1]
	push	{r0-r2, r4}
	ldr	r0, [sp, #56]
	push	{r0, r1}
	mov	r0, sp
	blx	r3
	add	sp, sp, #24
	pop	{r0, r1, r4, r12}
	b	.LcaptureSystem

/* The leave hook of the interrupt dispatch (g_intrLeaveIntr). */
	.global	intrLeaveHook
	.type	intrLeaveHook, %function
intrLeaveHook:
	LOAD_GLOBAL r3, g_intrLeaveIntr
	cmp	r3, #0
	beq	.LtraceInterrupt
	push	{r0, r12}
	mov	r0, sp
	blx	r3
	pop	{r0, r12}
	b	.LtraceInterrupt
	nop
	nop

/*
 * The IRQ handler. IRQ mode, interrupts off, lr the interrupted pc + 4:
 * acknowledge the interrupt, find its record and enter its handler.
 */
	FUNCTION interruptEntry
	.word	0, 0
	sub	lr, lr, #4
	sub	sp, sp, #4
	push	{r0-r4, r12, lr}
	LOAD_GLOBAL r0, g_intrCpuInterface
	ldr	r12, [r0, #12]				@ GICC_IAR
	ubfx	r0, r12, #0, #10
	cmp	r0, #255
	bhi	returnUnhandled
	LOAD_ADDRESS r1, g_intrState
	ldr	r2, [r1, r0, lsl #6]!
	cmp	r2, #0
	beq	returnUnhandled
	PATCH_SLOT intrEnterBranch
	nop						@ patched to b intrEnterHook
.LcaptureSystem:
	sub	sp, sp, #8
	mrc	p15, 0, lr, c13, c0, 4			@ TPIDRPRW
	mrc	p15, 0, r0, c9, c12, 1			@ PMCNTENSET
	mcr	p15, 0, r0, c9, c12, 2			@ PMCNTENCLR
	stmfd	sp!, {r0}
	mrc	p15, 0, r4, c7, c4, 0			@ PAR
	mrc	p15, 0, r3, c13, c0, 1			@ CONTEXTIDR
	mrc	p15, 0, r2, c3, c0, 0			@ DACR
	mrc	p15, 0, r0, c2, c0, 1			@ TTBR1
	push	{r0, r2-r4, lr}
	mov	r0, #0
	LOAD_GLOBAL lr, g_intrKernelBreakpoints
	cmp	lr, #0
	bne	.LclearBreakpoints
.LbreakpointsCleared:
	LOAD_GLOBAL lr, g_intrKernelWatchpoints
	cmp	lr, #0
	bne	.LclearWatchpoints
.LwatchpointsCleared:
	mcr	p15, 0, r0, c13, c0, 4			@ TPIDRPRW = 0
	LOAD_GLOBAL r0, g_intrSysroot
	ldr	r2, [r0]
	ldr	r3, [r0, #8]
	bfi	r3, r12, #16, #10
	mov	r4, #0
	mcr	p15, 0, r4, c13, c0, 1			@ CONTEXTIDR
	isb
	mcr	p15, 0, r2, c2, c0, 1			@ TTBR1
	isb
	mcr	p15, 0, r3, c13, c0, 1			@ CONTEXTIDR
	movw	lr, #0
	movt	lr, #0x1545
	mcr	p15, 0, lr, c3, c0, 0			@ DACR
	mrs	lr, spsr
	str	lr, [sp, #IRQ_FRAME_STATUS]
	mov	r0, sp
	cps	#CPSR_MODE_SYSTEM
	mrc	p15, 0, r4, c0, c0, 5			@ MPIDR
	and	r4, r4, #15
	add	r2, r1, r4, lsl #2
	ldr	r3, [r2, #INTR_HANDLER_COUNT]
	add	r3, r3, #1
	str	r3, [r2, #INTR_HANDLER_COUNT]		@ count[cpu]++
	LOAD_ADDRESS r2, g_intrCpu
	ldr	r3, [r2, r4, lsl #7]!
	add	r3, r3, #1
	str	r3, [r2]				@ nesting++
	sub	r3, r3, #2
	ldr	r2, [r0, #IRQ_FRAME_THREAD]
	ands	r2, r2, r3, asr #31
	beq	nestedInterrupt
	ldr	r2, [r0, #IRQ_FRAME_THREAD]
	cmp	r2, #0
	beq	nestedInterrupt
	ldr	r3, [r2, #12]
	str	lr, [r3, #60]
	str	sp, [r3, #56]!
	ldr	lr, [r0, #IRQ_FRAME_IP]
	stmdb	r3!, {r5-r11, lr}
	mov	r4, #1
	add	lr, r0, #IRQ_FRAME_R0
	ldm	lr, {r5-r9}
	stmdb	r3!, {r4-r9}
	add	r3, r3, #76
	ldr	r4, [r2, #16]
	cmp	r4, #0
	beq	.LsaveSystem
	vstmia	r4!, {d0-d15}
	vstmia	r4!, {d16-d31}
	vmrs	r5, fpscr
	vmrs	r6, fpexc
	stmia	r4!, {r5, r6}
.LsaveSystem:
	ldr	r4, [r0, #IRQ_FRAME_STATUS]
	and	r4, r4, #31
	cmp	r4, #31
	bne	.LsaveCp15
	ldr	r4, [r0, #4]
	str	r4, [r3, #-4]
.LsaveCp15:
	mrc	p15, 0, r4, c1, c0, 2			@ CPACR
	mrc	p15, 0, r5, c13, c0, 2			@ TPIDRURW
	mrc	p15, 0, r6, c13, c0, 3			@ TPIDRURO
	ldr	r7, [r0]
	ldr	r8, [r0, #8]
	ldr	r9, [r0, #12]
	mrc	p15, 0, r10, c9, c12, 0			@ PMCR
	mrc	p15, 0, r11, c9, c12, 3			@ PMOVSR
	stmia	r3!, {r4-r11}
	mrc	p15, 0, r4, c9, c12, 5			@ PMSELR
	str	r4, [r3], #4
	add	r3, r3, #4
	ldr	r4, [r0, #IRQ_FRAME_PMU]
	str	r4, [r3, #124]
	mrc	p15, 0, r5, c9, c13, 0			@ PMCCNTR
	str	r5, [r3], #4
	SAVE_PMU_COUNTERS r3, r10, r6, r7
	mrc	p15, 0, r4, c5, c0, 0			@ DFSR
	mrc	p15, 0, r5, c5, c0, 1			@ IFSR
	mrc	p15, 0, r6, c6, c0, 0			@ DFAR
	mrc	p15, 0, r7, c6, c0, 2			@ IFAR
	mrc	p14, 6, r8, c1, c0, 0			@ TEEHBR
	LOAD_GLOBAL lr, g_intrCpuInterface
	ldr	r9, [lr, #4]
	ldrd	r10, r11, [r0, #IRQ_FRAME_PC]
	stmia	r3!, {r4-r11}
	mrc	p15, 0, r5, c0, c0, 5
	and	r5, r5, #15
	LOAD_ADDRESS sp, g_intrInitialIrqStack
	sub	sp, sp, r5, lsl #17
	mov	r6, #0xf00000
	mcr	p15, 0, r6, c1, c0, 2			@ CPACR
	mov	r6, #0x40000000
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	mrc	p15, 0, r5, c12, c0, 0			@ VBAR
	ldr	r5, [r5, #0x24]
	mov	r6, #0x87
	str	r6, [r5]
	ldr	r0, [r2, #64]
	cmp	r0, #0x2000
	beq	.LenterHook
.LdispatchInterrupt:
	push	{r2, r12}
	mov	r3, #0x3000000
	vmsr	fpscr, r3
	LOAD_GLOBAL r0, g_intrKernelBreakpoints
	cmp	r0, #0
	beq	.LdispatchReady
	push	{r1, lr}
	bl	sceKernelSpinlockLowLock
	add	r2, r0, #0x18
	LOAD_BREAKPOINTS r2, r3, r4
	isb
	bl	sceKernelSpinlockLowUnlock
	pop	{r1, lr}
	LOAD_GLOBAL r0, g_intrKernelWatchpoints
	cmp	r0, #0
	beq	.LdispatchReady
	push	{r1, lr}
	bl	sceKernelSpinlockLowLock
	add	r2, r0, #0x10
	LOAD_WATCHPOINTS r2, r3, r4
	isb
	bl	sceKernelSpinlockLowUnlock
	pop	{r1, lr}
.LdispatchReady:
	mov	r0, #0
	mcr	p15, 0, r0, c13, c0, 3			@ TPIDRURO = 0
	cpsie	i
	ldr	r12, [r1]
	ldrh	r0, [r1, #INTR_HANDLER_CODE]
	ldrh	r2, [r1, #INTR_HANDLER_PRIORITY]
	ldr	r1, [r1, #INTR_HANDLER_ARG]
	blx	r12
	cpsid	i
	pop	{r4, r12}
	PATCH_SLOT intrLeaveBranch
	nop						@ patched to b intrLeaveHook
.LtraceInterrupt:
	cmn	r0, #1
	beq	.LacknowledgeInterrupt
	bic	r0, r12, #0x1c00
	cmp	r0, #31
	bls	.LacknowledgeInterrupt
	bic	r1, r0, #0x3e0
	mov	r2, #1
	mov	r2, r2, lsl r1
	mov	r1, r0, lsr #5
	mov	r1, r1, lsl #2
	LOAD_GLOBAL lr, g_intrDistributor
	add	r3, lr, #0x180				@ GICD_ICPENDR
	str	r2, [r3, r1]
.LacknowledgeInterrupt:
	LOAD_GLOBAL r11, g_intrCpuInterface
	str	r12, [r11, #16]				@ GICC_EOIR
	mrc	p15, 0, r3, c0, c0, 5
	and	r3, r3, #15
	LOAD_ADDRESS r2, g_intrCpu
	add	r2, r2, r3, lsl #7
	ldr	r0, [r2]
	LOAD_ADDRESS r1, g_intrStackGuard
	sub	r1, r1, r3, lsl #17
	sub	r1, r1, r0, lsl #13
	ldr	r1, [r1]
	movw	r3, #0xe8df
	movt	r3, #0x1ab4
	cmp	r1, r3
	bne	.LstackOverflow
	subs	r0, r0, #1
	str	r0, [r2]
	bne	returnNested
	cmp	r4, #0
	beq	returnNested
	LOAD_ADDRESS r1, g_intrDispatchPending
	ldr	r0, [r1]
	blx	r0
	cmp	r0, #0
	mcr	p15, 0, r4, c13, c0, 4			@ TPIDRPRW
	ldr	r0, [r4, #12]
	beq	.LcontextSelected
	mov	r1, #1
	LOAD_ADDRESS r2, g_intrSetupNextContext
	ldr	r3, [r2]
	blx	r3
.LcontextSelected:
	add	r1, r0, #192
	mrc	p15, 0, r12, c13, c0, 4
	ldmdb	r1!, {r4-r9}
	str	r9, [r11, #4]
	mcr	p14, 6, r8, c1, c0, 0			@ TEEHBR
	mcr	p15, 0, r7, c6, c0, 2			@ IFAR
	mcr	p15, 0, r6, c6, c0, 0			@ DFAR
	mcr	p15, 0, r5, c5, c0, 1			@ IFSR
	mcr	p15, 0, r4, c5, c0, 0			@ DFSR
	ldr	r11, [r0, #240]
	mcr	p15, 0, r11, c9, c12, 2			@ PMCNTENCLR
	RESTORE_PMU_COUNTERS r1, r9, r4, r5
	ldr	r8, [r1, #-4]!
	mcr	p15, 0, r8, c9, c13, 0			@ PMCCNTR
	ldmdb	r1!, {r2-r10}
	mcr	p15, 0, r10, c9, c14, 0			@ PMUSERENR
	mcr	p15, 0, r9, c9, c12, 5			@ PMSELR
	mcr	p15, 0, r8, c9, c12, 3			@ PMOVSR
	mcr	p15, 0, r7, c9, c12, 0			@ PMCR
	mcr	p15, 0, r6, c7, c4, 0			@ PAR
	mcr	p15, 0, r3, c13, c0, 3			@ TPIDRURO
	mcr	p15, 0, r2, c13, c0, 2			@ TPIDRURW
	mov	r2, #0
	mcr	p15, 0, r2, c13, c0, 1			@ CONTEXTIDR
	isb
	mcr	p15, 0, r4, c2, c0, 1			@ TTBR1
	isb
	mcr	p15, 0, r5, c13, c0, 1			@ CONTEXTIDR
	ldr	r2, [r1, #-4]!
	mcr	p15, 0, r2, c1, c0, 2			@ CPACR
	sub	r1, r1, #8
	mrc	p15, 0, r4, c13, c0, 4
	ldr	r5, [r4, #16]
	cmp	r5, #0
	beq	.LvfpSelected
	mov	r6, #0x40000000
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	vmsr	fpexc, r6
	mrc	p15, 0, r6, c12, c0, 0			@ VBAR
	ldr	r6, [r6, #0x24]
	ldr	r7, [r4, #20]
	str	r7, [r6]
	ldr	r6, [r5, #0x104]
	vmsr	fpexc, r6
	ldr	r6, [r5, #0x100]
	vmsr	fpscr, r6
.LvfpSelected:
	ldr	r2, [r0]
	cmp	r2, #1
	mov	r10, r2
	beq	.LresumeException
	cmp	r5, #0
	beq	.LresumeCooperative
	movw	r2, #0xdead
	movt	r2, #0x7f80
	add	r3, r2, #0x780000
	vmov	d0, r2, r3
	vmov	d1, d0
	.irp	q, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15
	vmov	q\q, q0
	.endr
	add	r5, r5, #64
	vldmia	r5!, {d8-d15}
.LresumeCooperative:
	ldr	r3, [r1, #-4]!
	ldr	r2, [r1, #-4]!
	ldr	sp, [r1, #-4]!
	b	.LswitchToIrq
.LresumeException:
	cmp	r5, #0
	beq	.LexceptionFrame
	vldmia	r5!, {d0-d15}
	vldmia	r5!, {d16-d31}
.LexceptionFrame:
	sub	r1, r1, #4
	ldr	lr, [r1, #-4]!
	ldr	sp, [r1, #-4]!
	ldrd	r2, r3, [r0, #192]
.LswitchToIrq:
	cps	#CPSR_MODE_IRQ
	and	r5, r3, #31
	cmp	r5, #16
	ldr	r5, [r0, #72]
	bne	.LsetDacr
	ldr	r5, [r0, #68]
	movw	r7, #0xaa8e
	movt	r7, #0xeaba
	tst	r5, r7
	bne	.LillegalUserDacr
.LsetDacr:
	mcr	p15, 0, r5, c3, c0, 0			@ DACR
	ldr	r5, [r4, #24]
	ldr	r6, [r5, #12]
	cmp	r6, #0
	beq	.LdebugRestored
	push	{r0-r3, r12, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	mov	r0, r6
	add	r6, r6, #0x18
	LOAD_BREAKPOINTS r6, r7, r8
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, r12, lr}
	ldr	r6, [r5, #16]
	cmp	r6, #0
	beq	.LdebugRestored
	push	{r0-r3, r12, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	mov	r0, r6
	add	r6, r6, #0x10
	LOAD_WATCHPOINTS r6, r7, r8
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, r12, lr}
	ldr	r6, [r4, #28]
	mcr	p14, 0, r6, c0, c5, 4			@ DBGBVR5
	ldr	r6, [r4, #32]
	mcr	p14, 0, r6, c0, c5, 5			@ DBGBCR5
.LdebugRestored:
	add	sp, sp, #64
	mcr	p15, 0, r11, c9, c12, 1			@ PMCNTENSET
	push	{r2, r3}
	cmp	r10, #1
	beq	.LreturnException
	sub	r1, r1, #4
	REARM_TRACE r4, r5
	ldmdb	r1!, {r4-r11}
	mov	r0, #1
	clrex
	rfeia	sp!
.LreturnException:
	ldmdb	r1!, {r0}
	stmfd	sp!, {r0}
	REARM_TRACE r4, r5
	mov	r12, r1
	ldmdb	r12!, {r0-r11}
	ldmfd	sp!, {r12}
	clrex
	rfeia	sp!
.LclearBreakpoints:
	mcr	p14, 0, r0, c0, c2, 5			@ DBGBCR2
	mcr	p14, 0, r0, c0, c3, 5
	mcr	p14, 0, r0, c0, c4, 5
	mcr	p14, 0, r0, c0, c5, 5
	b	.LbreakpointsCleared
.LclearWatchpoints:
	mcr	p14, 0, r0, c0, c1, 7			@ DBGWCR1
	mcr	p14, 0, r0, c0, c2, 7
	mcr	p14, 0, r0, c0, c3, 7
	b	.LwatchpointsCleared
.LenterHook:
	push	{r0-r2, r12}
	mov	r0, r12
	mov	r1, r2
	LOAD_GLOBAL r2, g_intrStackGuard
	blx	r2
	pop	{r0-r2, r12}
	b	.LdispatchInterrupt
.LstackOverflow:
	mov	r1, r12
	blx	intrStackOverflow
	b	.
.LillegalUserDacr:
	mov	r0, r4
	mov	r1, r5
	blx	illegalUserDACRCfunc
	bkpt	0
	END_FUNCTION interruptEntry

	.arm
/*
 * The register context of a thread, saved and resumed like setjmp and longjmp
 * (the thread manager's context switch): SceIntrmgrForKernel_71C5F42F saves
 * the callee-saved registers, the CP15, PMU and debug state and, when asked,
 * the VFP state into a ThreadRegisterContext, and returns 0.
 * SceIntrmgrForKernel_08FDB0CE restores one and returns through it with RFE.
 */
/* int save(ThreadRegisterContext *context, uintptr_t vfp): r0 context, r1 VFP image */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_71C5F42F
	mov	r2, #0
	stm	r0!, {r2}
	add	r0, r0, #16
	stm	r0!, {r4-r11}
	add	r0, r0, #4
	stm	r0!, {sp}
	stm	r0!, {lr}
	mrs	r2, cpsr
	orr	r2, r2, #0x20
	stm	r0!, {r2}
	add	r0, r0, #4
	mrc	p15, 0, r2, c3, c0, 0			@ DACR
	str	r2, [r0], #4
	cmp	r1, #0
	beq	.LthreadsaveCp15
	tst	r1, #1
	bicne	r1, r1, #1
	addeq	r1, r1, #0x40
	vstm	r1!, {d8-d15}
	addeq	r1, r1, #0x80
	vmrs	r2, fpscr
	vmrs	r3, fpexc
	stm	r1!, {r2, r3}
.LthreadsaveCp15:
	mrc	p15, 0, r1, c1, c0, 2			@ CPACR
	mrc	p15, 0, r2, c13, c0, 2			@ TPIDRURW
	mrc	p15, 0, r3, c13, c0, 3			@ TPIDRURO
	mrc	p15, 0, r12, c2, c0, 1			@ TTBR1
	stm	r0!, {r1-r3, r12}
	mrc	p15, 0, r1, c13, c0, 1			@ CONTEXTIDR
	mrc	p15, 0, r2, c7, c4, 0			@ PAR
	mrc	p15, 0, r3, c9, c12, 0			@ PMCR
	mrc	p15, 0, r12, c9, c12, 3			@ PMOVSR
	stm	r0!, {r1-r3, r12}
	mrc	p15, 0, r1, c9, c12, 5			@ PMSELR
	str	r1, [r0], #4
	add	r0, r0, #4
	mrc	p15, 0, r1, c9, c12, 1			@ PMCNTENSET
	mcr	p15, 0, r1, c9, c12, 2			@ PMCNTENCLR
	str	r1, [r0, #124]
	mrc	p15, 0, r2, c9, c13, 0			@ PMCCNTR
	str	r2, [r0], #4
	.irp	counter, 0, 1, 2, 3, 4, 5
	mov	r12, #\counter
	mcr	p15, 0, r12, c9, c12, 5			@ PMSELR
	mrc	p15, 0, r3, c9, c13, 1			@ PMXEVTYPER
	str	r3, [r0], #4
	mrc	p15, 0, r3, c9, c13, 2			@ PMXEVCNTR
	str	r3, [r0], #4
	.endr
	mrc	p15, 0, r1, c5, c0, 0			@ DFSR
	mrc	p15, 0, r2, c5, c0, 1			@ IFSR
	mrc	p15, 0, r3, c6, c0, 0			@ DFAR
	mrc	p15, 0, r12, c6, c0, 2			@ IFAR
	stm	r0!, {r1-r3, r12}
	mrc	p14, 6, r1, c1, c0, 0			@ TEEHBR
	stm	r0!, {r1}
	LOAD_GLOBAL r1, g_intrCpuInterface
	ldr	r2, [r1, #4]
	stm	r0!, {r2}
	mov	r0, #0
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_71C5F42F

/*
 * uint32_t resume(ThreadRegisterContext *context, uint32_t flags, uintptr_t vfp):
 * r0 context (r12 keeps it), r2 the VFP image (lr); returns into the context.
 */
	FUNCTION SceIntrmgrForKernel_08FDB0CE
	mov	lr, r2
	mov	r12, r0
	ldr	r3, [r0]
	add	r0, r0, #192
	ldmdb	r0!, {r2}
	LOAD_GLOBAL r1, g_intrCpuInterface
	str	r2, [r1, #4]
	ldmdb	r0!, {r4-r8}
	mcr	p14, 6, r8, c1, c0, 0			@ TEEHBR
	mcr	p15, 0, r7, c6, c0, 2			@ IFAR
	mcr	p15, 0, r6, c6, c0, 0			@ DFAR
	mcr	p15, 0, r5, c5, c0, 1			@ IFSR
	mcr	p15, 0, r4, c5, c0, 0			@ DFSR
	ldr	r11, [r12, #240]
	mcr	p15, 0, r11, c9, c12, 2			@ PMCNTENCLR
	RESTORE_PMU_COUNTERS r0, r9, r4, r5
	ldr	r8, [r0, #-4]!
	mcr	p15, 0, r8, c9, c13, 0			@ PMCCNTR
	ldmdb	r0!, {r4-r10}
	mcr	p15, 0, r10, c9, c14, 0			@ PMUSERENR
	mcr	p15, 0, r9, c9, c12, 5			@ PMSELR
	mcr	p15, 0, r8, c9, c12, 3			@ PMOVSR
	mcr	p15, 0, r7, c9, c12, 0			@ PMCR
	mcr	p15, 0, r6, c7, c4, 0			@ PAR
	mov	r6, #0
	mcr	p15, 0, r6, c13, c0, 1			@ CONTEXTIDR
	isb
	mcr	p15, 0, r4, c2, c0, 1			@ TTBR1
	isb
	mcr	p15, 0, r5, c13, c0, 1			@ CONTEXTIDR
	ldmdb	r0!, {r4-r6}
	mcr	p15, 0, r6, c13, c0, 3			@ TPIDRURO
	mcr	p15, 0, r5, c13, c0, 2			@ TPIDRURW
	mcr	p15, 0, r4, c1, c0, 2			@ CPACR
	sub	r0, r0, #8
	mrc	p15, 0, r4, c13, c0, 4			@ TPIDRPRW
	cmp	lr, #0
	beq	.LthreadVfpRestored
	mov	r6, #0x40000000
	vmsr	fpexc, r6
	mov	r6, #0
	vmsr	fpscr, r6
	vmsr	fpexc, r6
	mrc	p15, 0, r6, c12, c0, 0			@ VBAR
	ldr	r6, [r6, #0x24]
	ldr	r7, [r4, #20]
	str	r7, [r6]
.LthreadVfpRestored:
	cmp	r3, #1
	mov	r10, r3
	beq	.LthreadresumeException
	cmp	lr, #0
	beq	.LthreadresumeCooperative
	tst	lr, #1
	movne	lr, lr, lsr #1
	movne	lr, lr, lsl #1
	ldrne	r6, [lr, #0x44]
	ldreq	r6, [lr, #0x104]
	vmsr	fpexc, r6
	ldrne	r6, [lr, #0x40]
	ldreq	r6, [lr, #0x100]
	vmsr	fpscr, r6
	movw	r2, #0xdead
	movt	r2, #0x7f80
	add	r3, r2, #0x780000
	vmov	d0, r2, r3
	vmov	d1, d0
	.irp	q, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15
	vmov	q\q, q0
	.endr
	addeq	lr, lr, #0x40
	vldm	lr!, {d8-d15}
.LthreadresumeCooperative:
	ldr	r3, [r0, #-4]!
	ldr	r2, [r0, #-4]!
	ldr	sp, [r0, #-4]!
	b	.LthreadswitchToIrq
.LthreadresumeException:
	cmp	lr, #0
	beq	.LthreadexceptionFrame
	ldr	r6, [lr, #0x104]
	vmsr	fpexc, r6
	ldr	r6, [lr, #0x100]
	vmsr	fpscr, r6
	vldm	lr!, {d0-d15}
	vldm	lr!, {d16-d31}
.LthreadexceptionFrame:
	sub	r0, r0, #4
	ldr	lr, [r0, #-4]!
	ldr	sp, [r0, #-4]!
	ldrd	r2, r3, [r12, #192]
.LthreadswitchToIrq:
	cps	#CPSR_MODE_IRQ
	and	r5, r3, #31
	cmp	r5, #16
	ldr	r5, [r12, #72]
	bne	.LthreadsetDacr
	ldr	r5, [r12, #68]
	movw	r7, #0xaa8e
	movt	r7, #0xeaba
	tst	r5, r7
	bne	.LthreadillegalUserDacr
.LthreadsetDacr:
	mcr	p15, 0, r5, c3, c0, 0			@ DACR
	ldr	r5, [r4, #24]
	ldr	r6, [r5, #12]
	cmp	r6, #0
	beq	.LthreaddebugRestored
	push	{r0-r3, r12, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	mov	r0, r6
	add	r6, r6, #0x18
	LOAD_BREAKPOINTS r6, r7, r8
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, r12, lr}
	ldr	r6, [r5, #16]
	cmp	r6, #0
	beq	.LthreaddebugRestored
	push	{r0-r3, r12, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	mov	r0, r6
	add	r6, r6, #0x10
	LOAD_WATCHPOINTS r6, r7, r8
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, r12, lr}
	ldr	r6, [r4, #28]
	mcr	p14, 0, r6, c0, c5, 4			@ DBGBVR5
	ldr	r6, [r4, #32]
	mcr	p14, 0, r6, c0, c5, 5			@ DBGBCR5
.LthreaddebugRestored:
	cmp	r10, #1
	mcr	p15, 0, r11, c9, c12, 1			@ PMCNTENSET
	beq	.LthreadreturnException
	push	{r2, r3}
	REARM_TRACE r4, r5
	sub	r0, r0, #4
	ldmdb	r0!, {r4-r11}
	mov	r0, #1
	clrex
	rfeia	sp!
.LthreadreturnException:
	push	{r2, r3}
	ldmdb	r0!, {r1}
	stmfd	sp!, {r1}
	REARM_TRACE r4, r5
	mov	r12, r0
	ldmdb	r12!, {r0-r11}
	ldmfd	sp!, {r12}
	clrex
	rfeia	sp!
.LthreadillegalUserDacr:
	mov	r0, r4
	mov	r1, r5
	blx	illegalUserDACRCfunc
	bkpt	0
	END_FUNCTION SceIntrmgrForKernel_08FDB0CE

	.thumb
/*
 * The nesting stacks of the interrupt handler and their guards: every stack
 * of the 16 levels of the 4 cores starts with the word INTR_STACK_MAGIC,
 * which the interrupt entry checks again on the way out. The small hand
 * written Thumb functions that set the guards, give the other modules the
 * interrupt manager's variables and call the secure kernel.
 */
/* void initStackGuards(uint32_t magic): write magic into the guard of every stack. */
	FUNCTION_DEFAULT_UNWIND initStackGuards
	.thumb_func
	LOAD_ADDRESS r1, g_intrStacks
	mov.w	r2, #INTR_LEVEL_STACK_SIZE * 64
	add.w	r3, r1, r2
.LnextStack:
	str	r0, [r1]
	add.w	r1, r1, #INTR_LEVEL_STACK_SIZE
	cmp	r3, r1
	bne.n	.LnextStack
	bx	lr
	END_FUNCTION initStackGuards

/* int checkStackGuard(void): the nesting level of this core, -1 when its guard is overwritten. */
	FUNCTION_DEFAULT_UNWIND checkStackGuard
	.thumb_func
	mrc	p15, 0, r3, c0, c0, 5			@ MPIDR
	and.w	r3, r3, #15
	mov.w	r2, #0x80				@ one IntrCpuState
	mul.w	r1, r2, r3
	LOAD_ADDRESS r2, g_intrCpu
	add	r2, r1
	ldr	r0, [r2]				@ the nesting level
	LOAD_ADDRESS r1, g_intrStackGuard
	mov.w	r2, #INTR_CPU_STACK_SIZE
	mul.w	r2, r3, r2
	sub.w	r1, r1, r2
	mov.w	r3, #INTR_LEVEL_STACK_SIZE
	mul.w	r2, r3, r0
	sub.w	r1, r1, r2
	ldr	r1, [r1]
	LOAD_CONSTANT r3, INTR_STACK_MAGIC
	cmp	r1, r3
	it	ne
	movne.w	r0, #-1
	bx	lr
	END_FUNCTION checkStackGuard

/* void *SceIntrmgrForKernel_01911612(uint32_t cpu, uint32_t level): the guard of a core's level (1-16). */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_01911612
	.thumb_func
	cmp	r0, #4
	bcs.n	.LnoGuard
	cmp	r1, #1
	bcc.n	.LnoGuard
	cmp	r1, #16
	bhi.n	.LnoGuard
	LOAD_ADDRESS r2, g_intrStackGuard
	mov.w	r3, #INTR_CPU_STACK_SIZE
	mul.w	ip, r0, r3
	sub.w	r2, r2, ip
	mov.w	r3, #INTR_LEVEL_STACK_SIZE
	sub.w	r1, r1, #1
	mul.w	ip, r3, r1
	sub.w	r2, r2, ip
	mov	r0, r2
	bx	lr
.LnoGuard:
	mov.w	r0, #0
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_01911612

/* void SceIntrmgrForKernel_BCD3BB05(void): enable IRQs. */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_BCD3BB05
	.thumb_func
	cpsie	i
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_BCD3BB05

/* void SceIntrmgrForKernel_9CB39268(void *handler): threadmgr's callThreadmgrBeforeIntrHandler. */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_9CB39268
	.thumb_func
	LOAD_ADDRESS r1, g_intrStackGuard
	str	r0, [r1]
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_9CB39268

/* void SceIntrmgrForKernel_46F04D82(void *handler): the dispatch of pending work. */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_46F04D82
	.thumb_func
	LOAD_ADDRESS r1, g_intrDispatchPending
	str	r0, [r1]
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_46F04D82

/* void SceIntrmgrForKernel_764F140A(void *handler): the setup of the next context. */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_764F140A
	.thumb_func
	LOAD_ADDRESS r1, g_intrSetupNextContext
	str	r0, [r1]
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_764F140A

/* void SceIntrmgrForKernel_04E635AA(ThreadMgrCpuCB *const cpu[4]): threadmgr's per-core blocks. */
	FUNCTION_DEFAULT_UNWIND SceIntrmgrForKernel_04E635AA
	.thumb_func
	mov.w	r3, #0
.LcopyCpu:
	ldr	r1, [r0, r3]
	LOAD_ADDRESS r2, g_intrThreadmgrCpu
	str	r1, [r2, r3]
	add.w	r3, r3, #4
	cmp	r3, #16
	blt.n	.LcopyCpu
	bx	lr
	END_FUNCTION SceIntrmgrForKernel_04E635AA

/* int intrmgrSmc(uint32_t paddr, uint32_t command): SCE_MONCALL_SET_SHARED_MEMORY. */
	FUNCTION_DEFAULT_UNWIND intrmgrSmc
	.thumb_func
	movw	ip, #0x101
	smc	#0
	bx	lr
	END_FUNCTION intrmgrSmc

	.balign	64

	.thumb
/*
 * The supervisor call handler of the interrupt manager and the thunks that
 * call the syscall enter and leave hooks.
 *
 * Only what C cannot express: the handler runs in Supervisor mode before any
 * register may be touched, captures the user registers into the kernel stack
 * of the thread (ThreadSyscallFrame), switches the DACR to the kernel's,
 * calls the system call and returns to the user with RFE. Like the IRQ
 * handler it is a SceExcpmgrHandler, two link words and Thumb code at +8.
 *
 * Register use: r0-r3 the arguments of the system call, ip its number, then
 * its entry; r3 the thread (TPIDRPRW) on the way back.
 */
/*
 * One unwind region covers the hooks and the handler (Sony's exception index
 * has a single entry at the first hook, whose address has no Thumb bit; the
 * handler's has).
 */
	.fnstart
	.cantunwind

/* The enter hook of the system call (g_intrEnterSyscall). */
	.global	syscallEnterHook
	.type	syscallEnterHook, %function
syscallEnterHook:
	LOAD_ADDRESS r2, g_intrEnterSyscall
	ldr	r2, [r2]
	cbz	r2, .LenterHookDone
	mov	r0, sp
	blx	r2
	ldr.w	ip, [sp, #FRAME_ENTRY]
.LenterHookDone:
	b.n	.LcallSyscall

/* The leave hook of the system call (g_intrLeaveSyscall). */
	.global	syscallLeaveHook
	.type	syscallLeaveHook, %function
syscallLeaveHook:
	LOAD_ADDRESS r2, g_intrLeaveSyscall
	ldr	r2, [r2]
	cbz	r2, .LleaveHookDone
	add	r0, sp, #16
	mov	r1, sp
	blx	r2
	mrc	p15, 0, r3, c13, c0, 4			@ TPIDRPRW
.LleaveHookDone:
	b.n	.LrestoreSyscall

	.type	syscallInvalidMode, %function
syscallInvalidMode:
	bkpt	0
	nop.w
	nop.w

/*
 * The supervisor call handler. Supervisor mode, interrupts off, lr the
 * return address: r0-r3 and lr are saved below sp until the thread's kernel
 * stack takes them.
 */
	.global	syscallEntry
	.type	syscallEntry, %function
	.thumb_func
syscallEntry:
	.word	0, 0
	stmdb	sp, {r0, r1, r2, r3}
	mrs	r2, SPSR
	strd	lr, r2, [sp, #-24]
	and.w	r2, r2, #31
	subs	r2, #16
	bne.n	syscallInvalidMode
	mov	r0, sp
	cps	#CPSR_MODE_SYSTEM
	mrc	p15, 0, r1, c13, c0, 4			@ TPIDRPRW
	mrc	p15, 0, r3, c3, c0, 0			@ DACR
	str.w	r3, [r0, #-28]
	movt	r2, #0x1545				@ the kernel's DACR
	ldr	r3, [r1, #12]				@ the register context
	str	r2, [r3, #72]
	mcr	p15, 0, r2, c3, c0, 0			@ DACR
	movs	r2, #1
	mcr	p15, 0, r2, c13, c0, 3			@ TPIDRURO
	ldr	r2, [r1, #40]
	str	r2, [r1, #44]
	mov	r2, sp
	ldr.w	sp, [r1, #48]				@ the kernel stack of the thread
	ldr	r3, [r1, #12]
	ldr.w	r3, [r3, #252]
	sub	sp, #FRAME_SIZE
	str	r3, [sp, #FRAME_SYSCALL_AUX]
	str.w	sp, [r1, #68]
	ldrd	r1, r3, [r0, #-24]
	strd	r1, r3, [sp, #FRAME_PC]
	strd	r2, lr, [sp, #FRAME_SP]
	movs	r3, #0
	mrc	p15, 0, r2, c13, c0, 2			@ TPIDRURW
	strd	r3, r2, [sp, #FRAME_API_DECLARE]
	mov	lr, r0
	ldmdb	lr, {r0, r1, r2, r3}
	stmia.w	sp, {r0-r12}
	cmp.w	ip, #4096
	bcs.w	.LinvalidSyscall
	LOAD_GLOBAL r0, g_intrSyscall
	ldr.w	ip, [r0, ip, lsl #2]
	vmrs	r1, fpscr
	strd	ip, r1, [sp, #FRAME_ENTRY]
	ldr.w	r1, [lr, #-28]				@ the user's DACR
	LOAD_CONSTANT r0, 0xeabaaa8e
	tst	r1, r0
	bne.w	.LsyscallIllegalUserDacr
	mov.w	r1, #0x3000000				@ default NaN, flush to zero
	vmsr	fpscr, r1
	PATCH_SLOT syscallEnterBranch
	nop						@ patched to b syscallEnterHook
.LcallSyscall:
	ldmia.w	sp, {r0, r1, r2, r3}
	cpsie	i
	blx	ip
	cpsid	i
	push	{r0, r1, r2, r3}
	mrc	p15, 0, r2, c13, c0, 3			@ TPIDRURO
	cmp	r2, #1
	bne.n	.LinvalidApiStat
.LcheckFastMutex:
	mrc	p15, 0, r3, c13, c0, 4			@ TPIDRPRW
	add.w	r2, r3, #104
	ldr.w	ip, [r2]
	cmp	r2, ip
	bne.n	.LfastMutexOwned
.LfastMutexChecked:
	ldrd	r2, ip, [r3, #36]
	str.w	ip, [r3, #44]
	ands.w	r2, r2, #0xf0000000
	bne.w	.LtrapThread
	ldr.w	ip, [r3, #112]
	cmp.w	ip, #0
	bne.n	.LdebugSuspended
	ldr	r2, [sp, #RESULTS_SIZE + FRAME_API_DECLARE]
	subs	r2, #1
	bhi.n	.LapiDeclareAbove
	bne.n	.LcompareResult
	movs	r2, r1
	bpl.n	.LsyscallDone
.LcompareResult:
	cmp.w	r2, r0, asr #30
	beq.n	.LresultClass
	LOAD_CONSTANT lr, 0x8002710c
	cmp	r0, lr
	beq.n	.LwaitCancelled
	PATCH_SLOT syscallLeaveBranch
.LsyscallDone:
	nop						@ patched to b syscallLeaveHook
.LrestoreSyscall:
	ldr	r2, [sp, #RESULTS_SIZE + FRAME_SYSCALL_AUX]
	ldr.w	ip, [r3, #12]
	str.w	r2, [ip, #252]
	mov.w	r2, #0
	str	r2, [r3, #68]
	ldr	r2, [r3, #52]
	mcr	p15, 0, r2, c13, c0, 3			@ TPIDRURO
.LclearFlags:
	ldrex	lr, [r3, #36]
	bic.w	ip, lr, #0xa000
	strex	ip, ip, [r3, #36]
	cmp.w	ip, #0
	bne.n	.LclearFlags
	mrc	p15, 0, r2, c3, c0, 0			@ DACR
	ldr	r3, [r3, #12]
	str	r2, [r3, #72]
	ldr	r2, [r3, #68]
	LOAD_CONSTANT r3, 0x55555575
	ands	r2, r3
	mcr	p15, 0, r2, c3, c0, 0			@ DACR
	pop	{r0, r1, r2, r3}
	tst.w	lr, #0x2000
	bne.n	.LrestoreCalleeRegisters
.LreturnToUser:
	ldr.w	ip, [sp, #FRAME_API_DECLARE]
	cmp.w	ip, #4
	bcs.n	.LrestoreUserState
	ands.w	ip, ip, #1
	LOAD_CONSTANT r3, 0xdeadbeef
	it	eq
	moveq	r1, r3
	mov	r2, r3
.LrestoreUserState:
	mrc	p15, 0, ip, c12, c0, 0			@ VBAR
	ldr.w	ip, [ip, #36]				@ the core's trace area
	cmp.w	ip, #0
	mov.w	lr, #1
	it	ne
	strne.w	lr, [ip, #256]
	ldr.w	ip, [sp, #FRAME_TPIDRURW]
	mcr	p15, 0, ip, c13, c0, 2			@ TPIDRURW
	ldr.w	ip, [sp, #FRAME_FPSCR]
	vmsr	fpscr, ip
	add.w	ip, sp, #FRAME_PC
	ldr.w	lr, [sp, #FRAME_LR]
	ldr.w	sp, [sp, #FRAME_SP]
	cps	#19
	ldmia.w	ip, {ip, lr}
	stmdb	sp!, {ip, lr}
	LOAD_CONSTANT ip, 0xdeadbeef
	clrex
	rfeia	sp!
.LinvalidApiStat:
	b.n	.LreportApiStat
.LfastMutexOwned:
	bl	invalidFastMutexOwnerCfunc
	ldmia.w	sp, {r0, r1}
	mrc	p15, 0, r3, c13, c0, 4			@ TPIDRPRW
	b.n	.LfastMutexChecked
.LrestoreCalleeRegisters:
	add	r4, sp, #16
	ldmdb	r4, {r4-r11}
	b.n	.LreturnToUser
.LdebugSuspended:
	mov.w	r2, #0x2000000
	b.n	.LtrapThread
.LapiDeclareAbove:
	adds	r2, #1
	bge.n	.LsyscallDone
.LresultClass:
	mov.w	r2, #0x1000000
.LtrapThread:
	add	r0, sp, #16
	mov	r1, sp
	blx	sceKernelSysrootTrapThreadAfterSyscall
	mrc	p15, 0, r3, c13, c0, 4			@ TPIDRPRW
	cmp	r0, #0
	bne.n	.LsyscallDone
	add	sp, #16
	ldr.w	ip, [sp, #FRAME_ENTRY]
	b.n	.LcallSyscall
.LinvalidSyscall:
	sub	sp, #16
.LwaitCancelled:
	mov.w	r0, #0x80000
	mov.w	r1, #0
	mov.w	r2, #0
	bl	callIntrEvent
	mrc	p15, 0, r3, c13, c0, 4			@ TPIDRPRW
	ldr	r2, [r3, #36]
	ands.w	r2, r2, #0xf0000000
	bne.n	.LtrapThread
	b.n	.LdebugSuspended
.LreportApiStat:
	add	r0, sp, #16
	mov	r1, sp
	bl	invalidApiStatCfunc
	ldmia.w	sp, {r0, r1}
	b.n	.LcheckFastMutex
.LsyscallIllegalUserDacr:
	mrc	p15, 0, r0, c13, c0, 4			@ TPIDRPRW
	bl	illegalUserDACRCfunc
	bkpt	0
	.fnend
	.size	syscallEntry, . - syscallEntry
	.balign	64

	.thumb
/*
 * SceIntrmgrForKernel_71E08D71: copy 0x200 bytes, sixteen groups of eight
 * words through r4-r11, and return the end of the destination.
 */
	.global	SceIntrmgrForKernel_71E08D71
	.type	SceIntrmgrForKernel_71E08D71, %function
	.thumb_func
SceIntrmgrForKernel_71E08D71:
	.fnstart
	.save	{r4-r11}
	stmdb	sp!, {r4-r11}
	.rept	16
	ldmia.w	r1!, {r4-r11}
	stmia.w	r0!, {r4-r11}
	.endr
	ldmia.w	sp!, {r4-r11}
	bx	lr
	.fnend
	.size	SceIntrmgrForKernel_71E08D71, . - SceIntrmgrForKernel_71E08D71
#endif
