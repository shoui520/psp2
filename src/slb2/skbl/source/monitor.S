/*
 * SKBL's secure monitor (ARM, Monitor mode).
 *
 * monitorVectors is the monitor vector table boot() copies into the kernel's
 * vector page, ahead of setMonitorStack; it uses only the SMC, abort, IRQ
 * and FIQ slots. The SMC entry passes r12 (the call code) to
 * monitor_call_handler and returns to the caller; the other entries report
 * the exception through the C handlers in boot.c. jumpToNonSecure leaves the
 * Secure world for good: it returns in Non-secure Supervisor mode, IRQs
 * masked, at the address in r0.
 */

	.syntax	unified
	.arm

#define SCR_NS		1
#define PSR_MODE_SVC	0x13
#define PSR_I		0x80

	.text
	.balign	4
	.global	monitorVectors
	.type	monitorVectors, %function
monitorVectors:
	.word	0				/* reset: unused */
	.word	0				/* undefined: unused */
	ldr	pc, smcVector
	ldr	pc, prefetchAbortVector
	ldr	pc, dataAbortVector
	.word	0				/* reserved */
	ldr	pc, irqVector
	ldr	pc, fiqVector

smcVector:		.word	smcEntry
prefetchAbortVector:	.word	prefetchAbortEntry
dataAbortVector:	.word	dataAbortEntry
irqVector:		.word	irqEntry
fiqVector:		.word	fiqEntry
	.size	monitorVectors, . - monitorVectors

/* Monitor mode stack, 0x400 bytes per CPU; returns in System mode */
	.global	setMonitorStack
	.type	setMonitorStack, %function
setMonitorStack:
	mrc	p15, 0, r3, c0, c0, 5		/* MPIDR */
	and	r3, r3, #15
	mov	r0, #0x400
	mul	r0, r3, r0
	cpsid	if, #0x16			/* Monitor */
	ldr	sp, =monitorStackTop
	sub	sp, sp, r0
	mov	r1, #0
	mov	lr, r1
	msr	spsr_fsxc, r1
	cpsid	if, #0x1f			/* System */
	bx	lr
	.size	setMonitorStack, . - setMonitorStack

	.type	smcEntry, %function
smcEntry:
	stmdb	sp!, {lr}
	mov	r0, r12
	blx	monitor_call_handler
	clrex
	ldm	sp!, {pc}^
	.size	smcEntry, . - smcEntry

	.type	prefetchAbortEntry, %function
prefetchAbortEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	blx	monitor_prefetch_abort_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^
	.size	prefetchAbortEntry, . - prefetchAbortEntry

	.type	dataAbortEntry, %function
dataAbortEntry:
	sub	lr, lr, #8
	push	{r0-r3, r12, lr}
	blx	monitor_data_abort_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^
	.size	dataAbortEntry, . - dataAbortEntry

	.type	irqEntry, %function
irqEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	blx	monitor_irq_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^
	.size	irqEntry, . - irqEntry

	.type	fiqEntry, %function
fiqEntry:
	sub	lr, lr, #4
	push	{r0-r3, r12, lr}
	blx	monitor_fiq_handler
	clrex
	ldm	sp!, {r0-r3, r12, pc}^
	.size	fiqEntry, . - fiqEntry

/* the return frame on the monitor stack: the address, then Supervisor, IRQs masked */
	.global	jumpToNonSecure
	.type	jumpToNonSecure, %function
jumpToNonSecure:
	cpsid	if, #0x16			/* Monitor */
	movw	r1, #PSR_MODE_SVC | PSR_I
	str	r1, [sp, #-4]!
	str	r0, [sp, #-4]!
	mrc	p15, 0, r1, c1, c1, 0		/* SCR */
	orr	r1, r1, #SCR_NS
	mcr	p15, 0, r1, c1, c1, 0
	rfeia	sp!
	.size	jumpToNonSecure, . - jumpToNonSecure
	.ltorg
