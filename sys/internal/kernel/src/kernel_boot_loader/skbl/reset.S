/*
 * SKBL reset vector page, reset handler and exception entries (ARM, Secure state).
 *
 * The vector page sits at physical 0; every vector jumps into the code at 0x40020000.
 * The reset handler turns the MMU and caches off, invalidates the caches, TLB and branch
 * predictor, configures the Cortex-A9 auxiliary and Non-secure access controls, gives each
 * CPU its stacks in every processor mode (Monitor mode included), enables VFP/NEON, and on
 * CPU 0 sets up the SCU and saves or restores the boot save area. All CPUs then call
 * boot() in System mode with the MMU still off.
 *
 * The monitor vector table, setMonitorStack and the monitor entries are in monitor.S.
 * Not written yet: the TrustZone module archive.
 */

	.syntax	unified
	.arm
	.fpu	neon

/* SCU registers (CPU private region, physical 0x1a000000) */
#define SCU_BASE		0x1a000000
#define SCU_CONTROL		0x00
#define SCU_INVALIDATE_ALL	0x0c
#define SCU_SAC			0x50
#define SCU_SNSAC		0x54
#define SCU_DIAGNOSTIC_CONTROL	0x30	/* bit 0: ARM erratum 764369's workaround */
/* L2 cache controller (PL310) at 0x1a002000: reg1_control at 0x100 */
#define L2C_BASE		0x1a002000
#define L2C_CONTROL		0x100
/* KBL Param, where second_loader leaves it (physical 0x100) */
#define KBL_PARAM		0x100

#include <kernel_boot_loader/kbl_param.h>

	.section .text.vectors, "ax", %progbits
	.global	_start
_start:
	ldr	pc, resetVector
	ldr	pc, undefinedVector
	ldr	pc, svcVector
	ldr	pc, prefetchAbortVector
	ldr	pc, dataAbortVector
	ldr	pc, reservedVector
	ldr	pc, irqVector
	ldr	pc, fiqVector

resetVector:		.word	reset
undefinedVector:	.word	undefinedEntry
svcVector:		.word	svcEntry
prefetchAbortVector:	.word	prefetchAbortEntry
dataAbortVector:	.word	dataAbortEntry
irqVector:		.word	irqEntry
fiqVector:		.word	fiqEntry
/* boot() places the kernel's monitor vectors after this word, 32-byte aligned */
	.global	vectorsEnd
vectorsEnd:
	.word	0xffffffff
/* the reserved vector shares the reset handler's literal */
	.set	reservedVector, resetVector

	.space	0x40
/*
 * The DIP switches boot() turns on for a development kit whose CP has not
 * written any (henkaku wiki, SKBL): 0x80 0x9F, 0xC0-0xC2 0xC5-0xC9 0xCC 0xD2,
 * 0xE4 0xFD. NSKBL's vector page has the same words.
 */
	.global	bootDefaultDipsw
bootDefaultDipsw:
	.word	0, 0, 0, 0
	.word	0x80000001, 0, 0x000413e7, 0x20000010
	.space	0x20

	.section .text.reset, "ax", %progbits
	.global	reset
reset:
	cpsid	if
	clrex

	/* SCTLR: MMU, caches, branch prediction and the high vectors off */
	mov	r0, #0
	bic	r0, r0, #0x40000000		/* TE */
	bic	r0, r0, #0x10000000		/* TRE */
	bic	r0, r0, #0x20000000		/* AFE */
	bic	r0, r0, #0x02000000		/* EE */
	bic	r0, r0, #0x4000			/* RR */
	bic	r0, r0, #0x2000			/* V */
	bic	r0, r0, #0x1000			/* I */
	bic	r0, r0, #0x800			/* Z */
	bic	r0, r0, #0x400			/* SW */
	bic	r0, r0, #2			/* A */
	bic	r0, r0, #4			/* C */
	bic	r0, r0, #1			/* M */
	mcr	p15, 0, r0, c1, c0, 0

	mov	r0, #0
	mcr	p15, 0, r0, c7, c1, 0		/* ICIALLUIS */
	dsb	sy
	mcr	p15, 0, r0, c7, c1, 6		/* BPIALLIS */
	isb	sy
	mcr	p15, 0, r0, c7, c5, 0		/* ICIALLU */
	dsb	sy

	/* invalidate the L1 data cache by set/way: 4 ways, 256 sets of 32 bytes */
1:	mov	r1, #0
2:	orr	r2, r1, r0
	mcr	p15, 0, r2, c7, c6, 2		/* DCISW */
	add	r1, r1, #0x40000000
	cmp	r1, #0
	bne	2b
	add	r0, r0, #32
	cmp	r0, #0x2000
	bne	1b
	dsb	sy

	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 6		/* BPIALL */
	isb	sy
	mcr	p15, 0, r0, c8, c3, 0		/* TLBIALLIS */
	mov	r0, #0
	movw	r1, #:lower16:L2C_BASE
	movt	r1, #:upper16:L2C_BASE
	str	r0, [r1, #L2C_CONTROL]		/* L2 cache off */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0		/* ICIALLU */
	mov	r0, #0
	mcr	p15, 0, r0, c8, c7, 0		/* TLBIALL */
	mcr	p15, 0, r0, c2, c0, 2		/* TTBCR */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 6		/* BPIALL */
	isb	sy

	/* ACTLR: FW, SMP, L1 prefetch, L2 prefetch hint */
	mov	r0, #0
	orr	r0, r0, #0x40
	orr	r0, r0, #1
	orr	r0, r0, #0x100
	orr	r0, r0, #8
	mcr	p15, 0, r0, c1, c0, 1

	/* Cortex-A9 errata workarounds in the Diagnostic Control Register, by revision */
	mrc	p15, 0, r0, c15, c0, 1
	mrc	p15, 0, r1, c0, c0, 0		/* MIDR */
	ands	r2, r1, #15			/* revision */
	ands	r1, r1, #0xf00000		/* variant */
	cmp	r1, #0x200000
	bgt	4f
	blt	3f
	cmp	r2, #4
	bge	5f
	orrs	r0, r0, #0x400000
	orrs	r0, r0, #0x1000
5:	orrs	r0, r0, #0x40
3:	orrs	r0, r0, #0x10
	cmp	r2, #8
	bge	6f
	orrs	r0, r0, #0x100000
6:	orrs	r0, r0, #0x800
	orrs	r0, r0, #0x200000
	mcr	p15, 0, r0, c15, c0, 1
4:	mrc	p15, 0, r1, c1, c0, 0
	bic	r1, r1, #2			/* SCTLR.A: unaligned accesses allowed */
	mcr	p15, 0, r1, c1, c0, 0

	/* r3 = CPU number, r0 = its offset in the 0x400-byte-per-CPU stacks */
	mrc	p15, 0, r3, c0, c0, 5		/* MPIDR */
	and	r3, r3, #15
	mov	r0, #0x400
	mul	r0, r3, r0
	mov	r1, #0

	/* Supervisor (the mode at reset) */
	movw	sp, #:lower16:svcStackTop
	movt	sp, #:upper16:svcStackTop
	sub	sp, sp, r0
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x1b			/* Undefined */
	movw	sp, #:lower16:abortStackTop
	movt	sp, #:upper16:abortStackTop
	sub	sp, sp, r0
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x17			/* Abort: shares the Undefined stack */
	movw	sp, #:lower16:abortStackTop
	movt	sp, #:upper16:abortStackTop
	sub	sp, sp, r0
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x12			/* IRQ */
	movw	sp, #:lower16:irqStackTop
	movt	sp, #:upper16:irqStackTop
	sub	sp, sp, r0
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x11			/* FIQ: no stack */
	movw	r2, #0xbeef
	movt	r2, #0xdead
	mov	r8, r2
	mov	r9, r2
	mov	r10, r2
	mov	r11, r2
	mov	r12, r2
	mov	sp, r1
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x1f			/* System: 0x1000 bytes per CPU */
	movw	sp, #:lower16:systemStackTop
	movt	sp, #:upper16:systemStackTop
	sub	sp, sp, r0, lsl #2
	mov	lr, r1
	msr	spsr_fsxc, r1
	bl	setMonitorStack

	/* NSACR: Non-secure access to CP10/CP11 (VFP/NEON), SMP bit and the TLB lockdown */
	mrc	p15, 0, r0, c1, c1, 0		/* SCR */
	orr	r0, r0, #4			/* SCR.FW */
	mcr	p15, 0, r0, c1, c1, 0
	mrc	p15, 0, r0, c15, c0, 0		/* Power Control */
	orr	r0, r0, #1
	mcr	p15, 0, r0, c15, c0, 0
	mov	r0, #0xf00000			/* CPACR: CP10, CP11 full access */
	mcr	p15, 0, r0, c1, c0, 2
	mov	r0, #0x10c00			/* NSACR: CP10, CP11, NS_SMP */
	mcr	p15, 0, r0, c1, c1, 2
	isb	sy

	/* VFP/NEON on, every register cleared */
	mov	r0, #0x40000000			/* FPEXC.EN */
	vmsr	fpexc, r0
	isb	sy
	mov	r0, #0
	vmsr	fpscr, r0
	vdup.32	q0, r0
	vorr	q1, q0, q0
	vorr	q2, q0, q0
	vorr	q3, q0, q0
	vorr	q4, q0, q0
	vorr	q5, q0, q0
	vorr	q6, q0, q0
	vorr	q7, q0, q0
	vorr	q8, q0, q0
	vorr	q9, q0, q0
	vorr	q10, q0, q0
	vorr	q11, q0, q0
	vorr	q12, q0, q0
	vorr	q13, q0, q0
	vorr	q14, q0, q0
	vorr	q15, q0, q0
	mrc	p15, 0, r0, c1, c1, 2		/* NSACR */
	tst	r0, #0x10000			/* NSACR.PLE: PLE present */
	mov	r0, #1
	mcrne	p15, 0, r0, c11, c1, 0		/* PLEUAR */

	/* CPU 0 goes first; the others wait for its event */
	cmp	r3, #0
	wfene
	bne	9f

	/* SCU: off, invalidate every tag RAM, Secure/Non-secure access for all CPUs, on */
	mov	r1, #SCU_BASE
	mov	r0, #0
	bic	r0, r0, #3
	str	r0, [r1, #SCU_CONTROL]
	mvn	r0, #0
	str	r0, [r1, #SCU_INVALIDATE_ALL]
	ldr	r0, [r1, #SCU_SAC]
	orr	r0, r0, #15
	str	r0, [r1, #SCU_SAC]
	ldr	r0, [r1, #SCU_SNSAC]
	movw	r2, #0xfff
	orr	r0, r0, r2
	str	r0, [r1, #SCU_SNSAC]
	ldr	r0, [r1, #SCU_DIAGNOSTIC_CONTROL]
	orr	r0, r0, #1
	str	r0, [r1, #SCU_DIAGNOSTIC_CONTROL]
	ldr	r0, [r1, #SCU_CONTROL]
	orr	r0, r0, #0x40
	orr	r0, r0, #0x20
	orr	r0, r0, #1
	str	r0, [r1, #SCU_CONTROL]

	/*
	 * SKBL's data, the boot save area (the linker script): a resume that is not a fatal
	 * error's reboot starts from the values the cold boot backed up, marked 0x10; any
	 * other boot backs them up, marked 0.
	 */
	movw	r0, #:lower16:KBL_PARAM
	movt	r0, #:upper16:KBL_PARAM
	ldr	r1, [r0, #SCE_KBL_PARAM_WAKEUP_FACTOR]
	and	r2, r1, #SCE_KBL_WAKEUP_RESUME
	cmp	r2, #SCE_KBL_WAKEUP_RESUME
	bne	7f
	and	r2, r1, #SCE_KBL_WAKEUP_REASON_MASK
	cmp	r2, #SCE_KBL_WAKEUP_PANIC_REBOOT
	beq	7f
	ldr	r1, [r0, #SCE_KBL_PARAM_BOOT_TYPE_INDICATOR_1]
	mov	r2, #BOOT_TYPE_RESUME
	and	r1, r1, r2
	cmp	r1, r2
	bne	7f
	movw	r0, #:lower16:bootSaveArea
	movt	r0, #:upper16:bootSaveArea
	movw	r1, #:lower16:bootSaveAreaBackup
	movt	r1, #:upper16:bootSaveAreaBackup
	sub	r2, r1, r0
	movw	r0, #:lower16:bootSaveArea
	movt	r0, #:upper16:bootSaveArea
	movw	r1, #:lower16:bootSaveAreaBackup
	movt	r1, #:upper16:bootSaveAreaBackup
	blx	memcpy
	movw	r0, #:lower16:bootSaveArea
	movt	r0, #:upper16:bootSaveArea
	mov	r1, #0x10
	str	r1, [r0]
	b	8f
7:	movw	r0, #:lower16:bootSaveArea
	movt	r0, #:upper16:bootSaveArea
	movw	r1, #:lower16:bootSaveAreaBackup
	movt	r1, #:upper16:bootSaveAreaBackup
	sub	r2, r1, r0
	movw	r0, #:lower16:bootSaveAreaBackup
	movt	r0, #:upper16:bootSaveAreaBackup
	movw	r1, #:lower16:bootSaveArea
	movt	r1, #:upper16:bootSaveArea
	blx	memcpy
	movw	r0, #:lower16:bootSaveArea
	movt	r0, #:upper16:bootSaveArea
	mov	r1, #0
	str	r1, [r0]
8:	sev

9:	mrs	r0, cpsr
	bic	r0, r0, #0x100			/* A: asynchronous aborts on */
	msr	cpsr_x, r0
	ldr	pc, [pc, #-4]
	.word	10f
10:	blx	boot

undefinedEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	mov	r0, lr
	blx	boot_und_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^

svcEntry:
	push	{r0-r12, lr}
	mov	r0, lr
	blx	boot_svc_handler
	clrex
	ldm	sp!, {r0-r12, pc}^

prefetchAbortEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	mov	r0, lr
	blx	boot_prefetch_abort_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^

dataAbortEntry:
	sub	lr, lr, #8
	push	{r0-r3, r12, lr}
	mov	r0, lr
	blx	boot_data_abort_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^

irqEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	blx	boot_irq_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^

fiqEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	blx	boot_fiq_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^


/*
 * The boot stacks, 0x400 bytes per CPU in each mode (System: 0x1000, and 0x1000 below
 * the last CPU's that nothing uses).
 */
	.section .bss.stacks, "aw", %nobits
	.balign	8
	.space	0x5000
systemStackTop:
/* boot() clears .bss from here, above the System-mode stack it runs on */
	.global	_fbss
_fbss:
	.space	0x1000
abortStackTop:
	.space	0x1000
svcStackTop:
	.space	0x1000
irqStackTop:
	.space	0x1000
	.global	monitorStackTop
monitorStackTop:
