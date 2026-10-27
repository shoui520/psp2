/*
 * NSKBL reset vector and exception entries (ARM).
 *
 * The first code that runs in Non-secure state, at 0x51000000. The reset handler
 * turns the MMU and caches off, invalidates the caches, TLB and branch predictor,
 * gives each CPU its stacks in every processor mode, and calls boot() in System
 * mode with the MMU still off. The other vectors save the caller-saved registers
 * and call the C handler with the faulting address.
 */

	.syntax	unified
	.arm

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
	.word	0xffffffff
/* the reserved vector shares the reset handler's literal */
	.set	reservedVector, resetVector

	.space	0x50
/* PLACEHOLDER: four words of unknown meaning, the same in every build (3.20 to 4.00) */
	.word	0x80000001, 0, 0x000413e7, 0x20000010
	.space	0x20

reset:
	cpsid	if
	clrex

	/* SCTLR: MMU, caches, branch prediction and the high vectors off */
	mov	r0, #0
	bic	r0, r0, #0x40000000		/* TE */
	bic	r0, r0, #0x10000000		/* TRE */
	bic	r0, r0, #0x20000000		/* AFE */
	bic	r0, r0, #0x02000000		/* EE */
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
	mcr	p15, 0, r0, c7, c5, 0		/* ICIALLU */
	mov	r0, #0
	mcr	p15, 0, r0, c8, c7, 0		/* TLBIALL */
	mcr	p15, 0, r0, c2, c0, 2		/* TTBCR */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 6		/* BPIALL */
	isb	sy
	mrc	p15, 0, r1, c1, c0, 0
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
	cpsid	if, #0x11			/* FIQ: no stack, FIQs are taken in Secure state */
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

	/* CPU 0 goes first; the others wait for its event */
	cmp	r3, #0
	wfene
	bne	3f
	sev
3:	mrs	r0, cpsr
	bic	r0, r0, #0x100			/* A: asynchronous aborts on */
	msr	cpsr_x, r0
	ldr	pc, [pc, #-4]
	.word	4f
4:	blx	boot

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
 * Name unknown: boot() asks the secure monitor to unmap the secure kernel
 * boot loader (SCE_MONCALL_UNMAP_KBL) before it clears .bss.
 */
	.balign	16
	.global	UnmapKbl
	.type	UnmapKbl, %function
UnmapKbl:
	movw	r12, #0x103			/* SCE_MONCALL_UNMAP_KBL */
	.arch_extension sec
	smc	#0
	bx	lr
	.size	UnmapKbl, . - UnmapKbl

/*
 * The boot stacks, 0x400 bytes per CPU in each mode (System: 0x1000), used until
 * sceKblMain moves to the SceKernelBootStackCoreX memory blocks.
 */
	.section .bss.stacks, "aw", %nobits
	.balign	8
	.space	0x4000
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
