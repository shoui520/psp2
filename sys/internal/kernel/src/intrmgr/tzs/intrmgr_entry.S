/*
 * intrmgr_entry.S: the secure monitor's entries, which only assembly can
 * write. Internal names are unknown.
 *
 * monitorFiqEntry	FIQ in Monitor mode: acknowledge the GIC, run the
 *			registered handler on the interrupted stack with the
 *			secure world's VFP enabled, end the interrupt.
 * enableInterrupts	unmask IRQ and FIQ in the secure world.
 * monitorSmcEntry	SMC: ip below 0x100 is a fast call from
 *			g_smcHandlerTable; 0x100-0x4ff is a monitor call from
 *			g_moncallTable, run in System mode on this CPU's
 *			monitor stack with the CPU interface's priority mask
 *			opened to the Non-secure half.
 *
 * SceExcpmgr links each entry through its first two words
 * (sceKernelRegisterMonitorEntryForTZS); the code follows them.
 */
	.syntax unified
	.arch armv7-a
	.arch_extension sec
	.fpu neon
	.arm

/* GicCpuInterface, intrmgr/gic_regs.h */
	.equ	GICC_PRIORITY_MASK,	0x04
	.equ	GICC_ACKNOWLEDGE,	0x0c
	.equ	GICC_END_OF_INTR,	0x10
/* GicDistributor */
	.equ	GICD_CLEAR_ENABLE,	0x180
/* IntrHandler, ../intrmgr_state.h */
	.equ	INTR_HANDLER_SIZE,	0x40
	.equ	INTR_HANDLER_INTR_CODE,	0x04
	.equ	INTR_HANDLER_ARG,	0x08
	.equ	INTR_HANDLER_PRIORITY,	0x0c
	.equ	INTR_HANDLER_COUNT,	0x20

	.equ	INTR_ID_SPURIOUS,	0x3ff	/* 1022 and 1023: no interrupt */
	.equ	INTR_HANDLER_DISABLE,	-1	/* the handler's result: disable it */
	.equ	SCR_NS,			1
	.equ	CPACR_CP10_CP11_FULL,	0xf00000
	.equ	FPEXC_EN,		30
	.equ	CPSR_MODE_SYS,		0x1f
	.equ	CPSR_MODE_MON,		0x16
	.equ	GICC_PRIORITY_NS,	0x80	/* the Non-secure half of the priorities */
	.equ	MONCALL_FIRST,		0x100
	.equ	MONCALL_LAST,		0x500
	.equ	SCRATCH_VALUE,		0xdeadbeef

	.section .text.intrmgr_entry,"ax",%progbits
	.balign	64
	.global	monitorFiqEntry
	.type	monitorFiqEntry, %function
monitorFiqEntry:
	.word	0, 0
	sub	lr, lr, #4
	str	lr, [sp, #-8]
	mrs	lr, spsr
	str	lr, [sp, #-4]
	sub	sp, sp, #8
	stmdb	sp!, {r8, r9, r10, r11, ip}
	movw	r8, #:lower16:g_intrCpuInterface
	movt	r8, #:upper16:g_intrCpuInterface
	ldr	r8, [r8]
	ldr	ip, [r8, #GICC_ACKNOWLEDGE]
	mov	r9, #INTR_ID_SPURIOUS + 1
	sub	r9, r9, #1
	and	r8, ip, r9
	sub	r9, r9, #1
	cmp	r8, r9
	blt	1f
	ldmia	sp!, {r8, r9, r10, r11, ip}
	clrex
	rfeia	sp!
1:	mov	r10, #INTR_HANDLER_SIZE
	mul	r9, r8, r10
	movw	r8, #:lower16:g_intrState
	movt	r8, #:upper16:g_intrState
	add	r9, r9, r8
	ldr	r10, [r9]
	cmp	r10, #0
	bne	2f
	ldmia	sp!, {r8, r9, r10, r11, ip}
	clrex
	rfeia	sp!
2:	mrc	p15, 0, r8, c1, c1, 0		/* SCR: the handler runs secure */
	stmdb	sp!, {r8}
	bic	r8, r8, #SCR_NS
	mcr	p15, 0, r8, c1, c1, 0
	mrc	p15, 0, r8, c0, c0, 5		/* count the interrupt on this CPU */
	and	r8, r8, #0xf
	add	fp, r9, #INTR_HANDLER_COUNT
	add	fp, fp, r8, lsl #2
	ldr	r8, [fp]
	add	r8, r8, #1
	str	r8, [fp]
	mrc	p15, 0, r8, c1, c0, 2		/* CPACR: VFP for the handler */
	stmdb	sp!, {r8}
	mov	r8, #CPACR_CP10_CP11_FULL
	mcr	p15, 0, r8, c1, c0, 2
	vmrs	fp, fpexc
	stmdb	sp!, {fp}
	mov	r8, #1
	lsl	r8, r8, #FPEXC_EN
	vmsr	fpexc, r8
	vpush	{d16-d31}
	vpush	{d0-d15}
	vmrs	fp, fpscr
	stmdb	sp!, {fp}
	push	{r0, r1, r2, r3, r4, r5, r6, r7}
	mov	fp, ip
	ldrh	r0, [r9, #INTR_HANDLER_INTR_CODE]
	ldr	r1, [r9, #INTR_HANDLER_ARG]
	ldrh	r2, [r9, #INTR_HANDLER_PRIORITY]
	sub	sp, sp, #4
	blx	r10
	add	sp, sp, #4
	cmn	r0, #-INTR_HANDLER_DISABLE
	beq	3f
	mov	r1, #INTR_ID_SPURIOUS + 1
	sub	r1, r1, #1
	and	r0, r1, fp
	cmp	r0, #31				/* SGIs and PPIs stay enabled */
	bls	3f
	bic	r1, r0, #0x3e0
	mov	r2, #1
	lsl	r2, r2, r1
	lsr	r1, r0, #5
	lsl	r1, r1, #2
	movw	r10, #:lower16:g_intrDistributor
	movt	r10, #:upper16:g_intrDistributor
	ldr	r10, [r10]
	add	r3, r10, #GICD_CLEAR_ENABLE
	str	r2, [r3, r1]
3:	movw	r10, #:lower16:g_intrCpuInterface
	movt	r10, #:upper16:g_intrCpuInterface
	ldr	r10, [r10]
	str	fp, [r10, #GICC_END_OF_INTR]
	pop	{r0, r1, r2, r3, r4, r5, r6, r7}
	ldmia	sp!, {r8}
	vmsr	fpscr, r8
	vpop	{d0-d15}
	vpop	{d16-d31}
	ldmia	sp!, {r8}
	vmsr	fpexc, r8
	ldmia	sp!, {r8}
	mcr	p15, 0, r8, c1, c0, 2
	ldmia	sp!, {r8}
	mcr	p15, 0, r8, c1, c1, 0
	ldmia	sp!, {r8, r9, r10, r11, ip}
	clrex
	rfeia	sp!
	.size	monitorFiqEntry, . - monitorFiqEntry

	.global	enableInterrupts
	.type	enableInterrupts, %function
enableInterrupts:
	cpsie	if
	bx	lr
	.balign	64
	.size	enableInterrupts, . - enableInterrupts

	.global	monitorSmcEntry
	.type	monitorSmcEntry, %function
monitorSmcEntry:
	.word	0, 0
	str	lr, [sp, #-8]
	mrs	lr, spsr
	str	lr, [sp, #-4]
	sub	sp, sp, #8
	cmp	ip, #MONCALL_LAST
	bcs	1f
	cmp	ip, #MONCALL_FIRST
	bcs	2f
	movw	lr, #:lower16:g_smcHandlerTable
	movt	lr, #:upper16:g_smcHandlerTable
	add	lr, lr, ip, lsl #2
	ldr	ip, [lr]
	cmp	ip, #0
	beq	1f
	blx	ip
1:	movw	ip, #:lower16:SCRATCH_VALUE
	movt	ip, #:upper16:SCRATCH_VALUE
	clrex
	rfeia	sp!
2:	push	{r4, r5, r6, r7, r8, r9, r10, r11}
	mrc	p15, 0, lr, c1, c1, 0		/* SCR: the call runs secure */
	bic	lr, lr, #SCR_NS
	mcr	p15, 0, lr, c1, c1, 0
	cps	#CPSR_MODE_SYS
	mrc	p15, 0, r4, c0, c0, 5		/* this CPU's monitor stack */
	and	r4, r4, #0xf
	movw	r5, #:lower16:g_smcHandlerTable	/* the stacks end where it starts */
	movt	r5, #:upper16:g_smcHandlerTable
	sub	r5, r5, r4, lsl #12
	stmdb	r5!, {sp}
	mov	sp, r5
	stmdb	sp!, {lr}
	movw	r5, #:lower16:g_moncallTable
	movt	r5, #:upper16:g_moncallTable
	ldr	r4, [r5]
	cmp	r4, #0
	beq	3f
	sub	ip, ip, #MONCALL_FIRST
	ldr	r5, [r4, ip, lsl #2]
	cmp	r5, #0
	beq	3f
	movw	lr, #:lower16:g_intrCpuInterface
	movt	lr, #:upper16:g_intrCpuInterface
	ldr	lr, [lr]
	ldr	r4, [lr, #GICC_PRIORITY_MASK]
	stmdb	sp!, {r4}
	bic	r4, r4, #GICC_PRIORITY_NS
	str	r4, [lr, #GICC_PRIORITY_MASK]
	sub	sp, sp, #4
	cpsie	if
	blx	r5
	cpsid	if
	add	sp, sp, #4
	ldmia	sp!, {r4}
	movw	lr, #:lower16:g_intrCpuInterface
	movt	lr, #:upper16:g_intrCpuInterface
	ldr	lr, [lr]
	str	r4, [lr, #GICC_PRIORITY_MASK]
3:	ldmia	sp!, {lr}
	ldmia	sp!, {r4}
	mov	sp, r4
	movw	r2, #:lower16:SCRATCH_VALUE	/* leave no secure values behind */
	movt	r2, #:upper16:SCRATCH_VALUE
	mov	r3, r2
	mov	ip, r2
	cps	#CPSR_MODE_MON
	mrc	p15, 0, lr, c1, c1, 0
	orr	lr, lr, #SCR_NS
	mcr	p15, 0, lr, c1, c1, 0
	pop	{r4, r5, r6, r7, r8, r9, r10, r11}
	clrex
	rfeia	sp!
	.balign	64
	.size	monitorSmcEntry, . - monitorSmcEntry
