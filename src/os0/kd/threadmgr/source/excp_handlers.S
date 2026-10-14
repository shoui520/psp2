/*
 * SceKernelThreadMgr: what C cannot express at the bottom of the CPUs.
 *
 *   poisonVfpRegisters: every VFP/NEON register set to a signalling-NaN
 *   pattern at boot, so a read of one never written stands out.
 *
 *   idleThreadEntry: an idle thread's loop. It waits for interrupts with
 *   the exception vectors, the IRQ stack and the VBAR words preloaded, until
 *   SceThreadmgrForDriver_B4A05763 gives it an entry to run instead.
 *
 *   The exception handlers ThreadMgr adds to excpmgr's chains (ARM code).
 *   Each one is an excpmgr handler: a word excpmgr links to the next
 *   handler of its chain, a reserved word, then the code, entered with r0
 *   the exception context and left by going on to the next handler with r1
 *   what this one decided.
 *     threadmgrAbortHandler (data abort), threadmgrPrefetchHandler,
 *     threadmgrUndefHandler: the faulting thread's registers are saved in
 *     its register and VFP contexts, then threadMgrExcpHandlerCfunc_0
 *     decides.
 *     threadmgrDebug{Abort,Prefetch,Undef}Handler: excpmgr's debugger
 *     requests (r1 low half): 0 resume the thread
 *     (threadMgrExcpHandlerCfunc_2), 1 stop its process
 *     (threadMgrExcpHandlerCfunc_3).
 */
.syntax unified
.arch armv7-a
.fpu neon
.text

/* the excpmgr exception context */
.equ EXCP_CTX_R0,		0x00	/* r0-r14 */
.equ EXCP_CTX_PC,		0x3c
.equ EXCP_CTX_CPSR,		0x44
.equ EXCP_CTX_CPACR,		0x48
.equ EXCP_CTX_FPSCR,		0x4c
.equ EXCP_CTX_FPEXC,		0x50
.equ EXCP_CTX_CONTEXTIDR,	0x54
.equ EXCP_CTX_UNK58,		0x58
.equ EXCP_CTX_TPIDRURO,		0x5c
.equ EXCP_CTX_THREAD,		0x60	/* the running thread's ThreadCB */
.equ EXCP_CTX_TTBR,		0x64
.equ EXCP_CTX_SYS_LR,		0x6c	/* PLACEHOLDER: kept when the exception came from SYS mode */
.equ EXCP_CTX_DFSR,		0x70
.equ EXCP_CTX_IFSR,		0x74
.equ EXCP_CTX_FAR,		0x78
.equ EXCP_CTX_UNK7C,		0x7c
.equ EXCP_CTX_UNK80,		0x80
.equ EXCP_CTX_UNK84,		0x84
.equ EXCP_CTX_PMCR,		0x88
.equ EXCP_CTX_UNK8C,		0x8c
.equ EXCP_CTX_UNK90,		0x90
.equ EXCP_CTX_UNK94,		0x94
.equ EXCP_CTX_PMCCNTR,		0x98
.equ EXCP_CTX_PMUSERENR,	0x9c
.equ EXCP_CTX_PMEVENT,		0xa0	/* 6 counters, type and count */
.equ EXCP_CTX_UNKD8,		0xd8
.equ EXCP_CTX_VFP,		0x100	/* d0-d31 */
.equ EXCP_DEBUG_REQUEST_MASK,	0xffff
.equ EXCP_DEBUG_REQUEST_COUNT,	5

/* ThreadCB and ThreadRegisterContext */
.equ THREAD_THREAD_ID,		0x08
.equ THREAD_REGISTERS,		0x0c
.equ THREAD_VFP_CONTEXT,	0x10
.equ REGS_UNK48,		0x48
.equ REGS_CPACR,		0x4c	/* then unk50 ... pmuserenr */
.equ REGS_PMCCNTR,		0x74
.equ REGS_PMEVENT,		0x78
.equ REGS_DFSR,			0xa8	/* then ifsr, far, unkB4, unkB8 */
.equ REGS_PC,			0xc0	/* then cpsr */
.equ REGS_UNKF0,		0xf0

.equ CPSR_MODE_MASK,		0x1f
.equ CPSR_MODE_SYS,		0x1f
.equ CPSR_MODE_IRQ,		0x12

/* the kinds threadMgrExcpHandlerCfunc_0 is told of */
.equ EXCP_KIND_PREFETCH_ABORT,	0x100
.equ EXCP_KIND_DATA_ABORT,	0x200
.equ EXCP_KIND_UNDEFINED,	0x400

/* ThreadmgrRuntime.idleEntry */
.equ RUNTIME_IDLE_ENTRY,	0x04
/* VBAR+0x38: PLACEHOLDER: the word the idle loop preloads */
.equ VBAR_IDLE_PRELOAD,		0x38

/* idleThreadEntry */
	.thumb
	.balign 8
.global idleThreadEntry
.type idleThreadEntry, %function
.thumb_func
idleThreadEntry:
	mrs	r3, cpsr
	cpsid	aif, #CPSR_MODE_IRQ
	mov	r1, sp				/* the IRQ stack */
	msr	cpsr_fc, r3
	mrc	p15, 0, r3, c12, c0, 0		/* VBAR */
	ldr	r2, [r3, #VBAR_IDLE_PRELOAD]
	movw	r4, #:lower16:s_threadmgrRuntime + RUNTIME_IDLE_ENTRY
	movt	r4, #:upper16:s_threadmgrRuntime + RUNTIME_IDLE_ENTRY
	b	1f
	.balign 64				/* the loop in one cache line */
1:	ldr	r0, [r4]
	cbnz	r0, 2f
	cpsid	i
	pli	[r3]
	pld	[r3, #32]
	pli	[r2]
	pld	[r2, #-8]
	pld	[r1, #-64]
	pld	[r1, #-32]
	wfi
	cpsie	i
	b	1b
2:	bx	r0
	.balign 64
.size idleThreadEntry, .-idleThreadEntry

/*
 * The fault handlers: the thread's registers into its contexts, then
 * threadMgrExcpHandlerCfunc_0(thread, kind) says what comes next.
 */
.macro faultHandler name, kind
	.arm
	.balign 4
.global \name
.type \name, %object
\name:
	.word	0				/* the next handler (excpmgr's) */
	.word	0
	ldr	r1, [r0, #EXCP_CTX_THREAD]
	cmp	r1, #0
	beq	excpHang
	ldr	r2, [r1, #THREAD_REGISTERS]
	mov	r3, #1				/* full: pc and cpsr at REGS_PC */
	stmia	r2!, {r3}
	add	r3, r0, #EXCP_CTX_R0
	ldmia	r3!, {r4-r10}			/* r0-r6 */
	stmia	r2!, {r4-r10}
	ldmia	r3!, {r4-r11}			/* r7-r14 */
	stmia	r2!, {r4-r11}
	add	r2, r2, #8			/* to REGS_UNK48 */
	ldr	r3, [r1, #THREAD_VFP_CONTEXT]
	cmp	r3, #0
	addne	r4, r0, #EXCP_CTX_VFP
	.rept 8
	ldmiane	r4!, {r5-r12}
	stmiane	r3!, {r5-r12}
	.endr
	ldrne	r4, [r0, #EXCP_CTX_FPSCR]
	ldrne	r5, [r0, #EXCP_CTX_FPEXC]
	stmiane	r3!, {r4, r5}
	ldr	r3, [r0, #EXCP_CTX_CPSR]
	and	r3, r3, #CPSR_MODE_MASK
	cmp	r3, #CPSR_MODE_SYS
	ldreq	r3, [r0, #EXCP_CTX_SYS_LR]
	streq	r3, [r2], #4
	addne	r2, r2, #4
	ldr	r3, [r0, #EXCP_CTX_CPACR]	/* REGS_CPACR ... pmuserenr */
	ldr	r4, [r0, #EXCP_CTX_UNK58]
	ldr	r5, [r0, #EXCP_CTX_TPIDRURO]
	ldr	r6, [r0, #EXCP_CTX_TTBR]
	ldr	r7, [r0, #EXCP_CTX_CONTEXTIDR]
	ldr	r8, [r0, #EXCP_CTX_UNK80]
	ldr	r9, [r0, #EXCP_CTX_PMCR]
	ldr	r10, [r0, #EXCP_CTX_UNK90]
	ldr	r11, [r0, #EXCP_CTX_UNK94]
	ldr	r12, [r0, #EXCP_CTX_PMUSERENR]
	stmia	r2!, {r3-r12}
	ldr	r3, [r0, #EXCP_CTX_UNK8C]
	str	r3, [r2, #REGS_UNKF0 - REGS_PMCCNTR]
	ldr	r4, [r0, #EXCP_CTX_PMCCNTR]
	str	r4, [r2], #4
	add	r7, r0, #EXCP_CTX_PMEVENT
	.rept 6
	ldrd	r4, r5, [r7], #8
	strd	r4, r5, [r2], #8
	.endr
	ldr	r3, [r0, #EXCP_CTX_DFSR]	/* REGS_DFSR ... unkB8 */
	ldr	r4, [r0, #EXCP_CTX_IFSR]
	ldr	r5, [r0, #EXCP_CTX_FAR]
	ldr	r6, [r0, #EXCP_CTX_UNK7C]
	ldr	r7, [r0, #EXCP_CTX_UNK84]
	stmia	r2!, {r3-r7}
	add	r2, r2, #4			/* to REGS_PC */
	ldr	r3, [r0, #EXCP_CTX_PC]
	ldr	r4, [r0, #EXCP_CTX_CPSR]
	ldr	r5, [r0, #EXCP_CTX_UNKD8]
	stmia	r2!, {r3-r5}
	push	{r0-r3, r12, lr}
	ldr	r0, [r0, #EXCP_CTX_THREAD]
	mov	r1, #\kind
	blx	threadMgrExcpHandlerCfunc_0
	mov	r4, r0
	pop	{r0-r3, r12, lr}
	mov	r1, r4
	ldr	r2, =\name
	ldr	r2, [r2]
	add	r2, r2, #8
	mov	pc, r2
.size \name, .-\name
.endm

/* the debugger's requests on a stopped thread */
.macro debugHandler name
	.arm
	.balign 4
.global \name
.type \name, %object
\name:
	.word	0				/* the next handler (excpmgr's) */
	.word	0
	ldr	r3, =EXCP_DEBUG_REQUEST_MASK
	and	r2, r1, r3
	cmp	r2, #EXCP_DEBUG_REQUEST_COUNT
	addlt	pc, pc, r2, lsl #2
	b	.				/* not a request */
	b	1f				/* 0: resume the thread */
	b	2f				/* 1: stop its process */
	b	3f
	b	3f
	b	3f
1:	ldr	r2, [r0, #EXCP_CTX_THREAD]
	cmp	r2, #0
	beq	SceExcpmgrForKernel_3E55B5C3
	push	{r0-r3, r12, lr}
	ldr	r0, [r2, #THREAD_THREAD_ID]
	blx	threadMgrExcpHandlerCfunc_2
	pop	{r0-r3, r12, lr}
	b	3f
2:	push	{r0-r3, r12, lr}
	ldr	r0, [r0, #EXCP_CTX_THREAD]
	blx	threadMgrExcpHandlerCfunc_3
	pop	{r0-r3, r12, lr}
	b	3f
3:	ldr	r2, =\name
	ldr	r2, [r2]
	add	r2, r2, #8
	mov	pc, r2
.size \name, .-\name
.endm

	faultHandler threadmgrAbortHandler, EXCP_KIND_DATA_ABORT
	.ltorg
	faultHandler threadmgrPrefetchHandler, EXCP_KIND_PREFETCH_ABORT
	.ltorg
	faultHandler threadmgrUndefHandler, EXCP_KIND_UNDEFINED
	.ltorg

	.arm
	.balign 4
excpHang:
	b	.				/* an exception without a thread */

	debugHandler threadmgrDebugAbortHandler
	debugHandler threadmgrDebugPrefetchHandler
	debugHandler threadmgrDebugUndefHandler
	.ltorg

.section .note.GNU-stack,"",%progbits
