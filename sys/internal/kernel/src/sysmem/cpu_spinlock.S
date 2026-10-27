/*
 * SceCpu: the CPU number and the spinlock primitives, hand-written ldrex/strex
 * sequences that keep no frame. Two entries share one body in the IRQ lock
 * functions (the caller's address from lr, or given in r1).
 *
 * The same object in every firmware. A debug kernel (CPU_DEBUG) wraps the exports
 * in cpu.c, which checks the lock and times how long the IRQs stay off; a release
 * kernel exports these functions directly. The internal names are not Sony's
 * (unknown).
 *
 * A lock word holds the address of its owner (spinlock), or 0x80000000 for a
 * writer and the reader count otherwise (RW spinlock).
 */

/*
 * the secure world's SceCpu (SYSMEM_TZS) is the release kernel's in every firmware, and so
 * is the boot loader's (SYSMEM_NSKBL), which keeps only the locks it takes
 */
#if FW_DEBUG && !defined(SYSMEM_TZS) && !SYSMEM_NSKBL
#define CPU_DEBUG 1
#else
#define CPU_DEBUG 0
#endif

	.syntax unified
	.arch armv7-a
	.thumb
	.text

#define PSR_I_BIT	0x80

/*
 * The interrupt masks the IRQ locks take and give back: the secure world
 * masks FIQs with the IRQs and restores both bits from the saved flags.
 */
#if defined(SYSMEM_TZS) || defined(SYSMEM_SKBL)
#define PSR_MASK_BITS	0xc0
#define IRQ_OFF		cpsid	if
#else
#define PSR_MASK_BITS	PSR_I_BIT
#define IRQ_OFF		cpsid	i
#endif

/* irq_restore flags, scratch: the IRQs (and FIQs) back as flags has them */
	.macro	irq_restore flags, scratch
#if defined(SYSMEM_TZS) || defined(SYSMEM_SKBL)
	mrs	\scratch, cpsr
	lsrs	\flags, \flags, #6
	bfi	\scratch, \flags, #6, #2
	msr	cpsr_c, \scratch
#else
	cbnz	\flags, 1f
	cpsie	i
1:
#endif
	.endm

#define RW_WRITER	0x80000000

/*
 * func NAME, EXPORT: a function, 4-byte aligned, named EXPORT in a release kernel
 * when it has one; endfunc NAME, EXPORT: its end, padded to 4 bytes.
 * sym NAME: a function symbol here (a second entry into the function before it).
 */
	.macro	sym name
	.global	\name
	.type	\name, %function
	.thumb_func
\name:
	.endm

	.macro	func name, export
#ifdef SYSMEM_TZS
	/* a section per function, as the C's: the link keeps what the exports reach */
	.section .text.\name, "ax", %progbits
#endif
	.balign	4
#if CPU_DEBUG
	sym	\name
#else
	.ifb	\export
	sym	\name
	.else
	sym	\export
	.endif
#endif
	.endm

	.macro	size name, export
#if CPU_DEBUG
	.size	\name, . - \name
#else
	.ifb	\export
	.size	\name, . - \name
	.else
	.size	\export, . - \export
	.endif
#endif
	.endm

	.macro	endfunc name, export
	.balign	4
	size	\name, \export
	.endm

/* int sceKernelGetCpuId(void): this CPU's number, 0-3 (MPIDR) */
	func	sceKernelGetCpuId, sceKernelCpuId
	mrc	p15, 0, r0, c0, c0, 5
	and.w	r0, r0, #0xf
	bx	lr
	endfunc	sceKernelGetCpuId, sceKernelCpuId

#if SYSMEM_NSKBL
/*
 * uint32_t CpuSctlrInit(void): the boot loader's SCTLR (MMU, caches, branch
 * prediction and round-robin replacement on; alignment checks, TEX remap, high
 * vectors, SWP and Thumb exceptions off; access flag on), the instruction cache
 * invalidated; returns the value written
 */
	func	CpuSctlrInit
	mrc	p15, 0, r0, c1, c0, 0			@ SCTLR
	bic.w	r0, r0, #0x40000000			@ TE
	orr.w	r0, r0, #0x20000000			@ AFE
	orr.w	r0, r0, #0x4000				@ RR
	bic.w	r0, r0, #0x10000000			@ TRE
	bic.w	r0, r0, #0x2000				@ V
	orr.w	r0, r0, #0x1000				@ I
	orr.w	r0, r0, #0x800				@ Z
	bic.w	r0, r0, #0x400				@ SW
	orr.w	r0, r0, #4				@ C
	bic.w	r0, r0, #2				@ A
	orr.w	r0, r0, #1				@ M
	mcr	p15, 0, r0, c1, c0, 0
	movs	r1, #0
	mcr	p15, 0, r1, c7, c5, 0			@ ICIALLU
	bx	lr
	endfunc	CpuSctlrInit
#if defined(SYSMEM_SKBL)
/*
 * uint32_t CpuActlrInit(void): the instruction cache invalidated, then the boot
 * loader's ACTLR (parity off, alloc in one way on, exclusive cache off, SMP on,
 * WFLZ on, FW on); returns the value written
 */
	func	CpuActlrInit
	movs	r1, #0
	mcr	p15, 0, r1, c7, c5, 0			@ ICIALLU
	mrc	p15, 0, r0, c1, c0, 1			@ ACTLR
	bic.w	r0, r0, #0x200				@ Parity
	orr.w	r0, r0, #0x100				@ Alloc in one way
	bic.w	r0, r0, #0x80				@ EXCL
	orr.w	r0, r0, #0x40				@ SMP
	orr.w	r0, r0, #8				@ WFLZ
	orr.w	r0, r0, #1				@ FW
	mcr	p15, 0, r0, c1, c0, 1
	bx	lr
	endfunc	CpuActlrInit
#endif
#endif

/* void sceKernelSpinlockLowLock(lock): spins (wfe) until the lock is free */
	func	sceKernelSpinlockLowLock
0:	ldrex	r1, [r0]
	cbnz	r1, 1f
	strex	r1, lr, [r0]
	cbnz	r1, 2f
	dmb	sy
	bx	lr
1:	wfe
2:	b	0b
	endfunc	sceKernelSpinlockLowLock

#if !SYSMEM_NSKBL
/* int sceKernelSpinlockLowTrylock(lock): 0, or -1 when the lock is taken */
	func	sceKernelSpinlockLowTrylock
0:	ldrex	r1, [r0]
	cbnz	r1, 1f
	strex	r1, lr, [r0]
	cmp	r1, #0
	bne	0b
	dmb	sy
	movs	r0, #0
	bx	lr
1:	movs.w	r0, #-1
	bx	lr
	endfunc	sceKernelSpinlockLowTrylock
#endif

/* void CpuSpinlockUnlock(lock) */
	func	CpuSpinlockUnlock, sceKernelSpinlockLowUnlock
	movs	r1, #0
	dmb	sy
	str	r1, [r0]
	dsb	sy
	sev
	bx	lr
	endfunc	CpuSpinlockUnlock, sceKernelSpinlockLowUnlock

/*
 * int CpuSpinlockLockIrqCore(lock): takes the lock with the IRQs off and returns
 * the previous I bit; the IRQs are back on while it waits.
 * CpuSpinlockLockIrqWithCaller(lock, caller) records caller as the owner.
 */
	func	CpuSpinlockLockIrqCore, sceKernelSpinlockLowLockCpuSuspendIntr
	mov	r2, lr
1:	mrs	r3, cpsr
	IRQ_OFF
2:	ldrex	r1, [r0]
	cbnz	r1, 3f
	strex	r1, r2, [r0]
	cbnz	r1, 4f
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
3:	msr	cpsr_c, r3
	wfe
	IRQ_OFF
4:	b	2b
#if CPU_DEBUG
	.size	CpuSpinlockLockIrqCore, . - CpuSpinlockLockIrqCore
	sym	CpuSpinlockLockIrqWithCaller
#endif
	mov	r2, r1
	b	1b
	endfunc	CpuSpinlockLockIrqWithCaller, sceKernelSpinlockLowLockCpuSuspendIntr

#if !SYSMEM_NSKBL
/*
 * int CpuSpinlockTrylockIrqCore(lock): as CpuSpinlockLockIrqCore, or -1 with the
 * IRQs as they were when the lock is taken.
 */
	func	CpuSpinlockTrylockIrqCore, sceKernelSpinlockLowTrylockCpuSuspendIntr
	mov	r2, lr
1:	mrs	r3, cpsr
	IRQ_OFF
2:	ldrex	r1, [r0]
	cbnz	r1, 3f
	strex	r1, r2, [r0]
	cmp	r1, #0
	bne	2b
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
3:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
#if CPU_DEBUG
	.size	CpuSpinlockTrylockIrqCore, . - CpuSpinlockTrylockIrqCore
	sym	CpuSpinlockTrylockIrqWithCaller
#endif
	mov	r2, r1
	b	1b
	endfunc	CpuSpinlockTrylockIrqWithCaller, sceKernelSpinlockLowTrylockCpuSuspendIntr
#endif

/* void CpuSpinlockUnlockIrq(lock, flags): unlocks; IRQs on unless flags */
	func	CpuSpinlockUnlockIrq, sceKernelSpinlockLowUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	irq_restore r1, r0
	bx	lr
	endfunc	CpuSpinlockUnlockIrq, sceKernelSpinlockLowUnlockCpuResumeIntr

#if !SYSMEM_NSKBL
/*
 * void sceKernelRWSpinlockLowWriteLock(lock). As in Sony's code, a successful
 * strex loops back (and then waits on its own lock) and a failed one returns.
 */
	func	sceKernelRWSpinlockLowWriteLock
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cbz	r1, 3f
	dmb	sy
	bx	lr
2:	wfe
3:	b	1b
	endfunc	sceKernelRWSpinlockLowWriteLock

/* int SceCpuForDriver_4F7790B4(lock): 0, or -1 when held */
	func	SceCpuForDriver_4F7790B4
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cmp	r1, #0
	bne	1b
	dmb	sy
	movs	r0, #0
	bx	lr
2:	movs.w	r0, #-1
	bx	lr
	endfunc	SceCpuForDriver_4F7790B4

/* void CpuRWSpinlockWriteUnlock(lock) */
	func	CpuRWSpinlockWriteUnlock, sceKernelRWSpinlockLowWriteUnlock
	movs	r1, #0
	dmb	sy
	str	r1, [r0]
	dsb	sy
	sev
	bx	lr
	endfunc	CpuRWSpinlockWriteUnlock, sceKernelRWSpinlockLowWriteUnlock
#endif

/* int CpuRWSpinlockWriteLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockWriteLockIrq, SceCpuForDriver_4C38CE4D
	mrs	r3, cpsr
	movs.w	r2, #RW_WRITER
	IRQ_OFF
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cbnz	r1, 3f
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
2:	msr	cpsr_c, r3
	wfe
	IRQ_OFF
3:	b	1b
	endfunc	CpuRWSpinlockWriteLockIrq, SceCpuForDriver_4C38CE4D

#if !SYSMEM_NSKBL
/* int CpuRWSpinlockTryWriteLockIrq(lock): the previous I bit, or -1 when held */
	func	CpuRWSpinlockTryWriteLockIrq, SceCpuForDriver_DE6482C6
	mrs	r3, cpsr
	IRQ_OFF
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cmp	r1, #0
	bne	1b
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
2:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
	endfunc	CpuRWSpinlockTryWriteLockIrq, SceCpuForDriver_DE6482C6
#endif

/* void CpuRWSpinlockWriteUnlockIrq(lock, flags) */
	func	CpuRWSpinlockWriteUnlockIrq, sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	irq_restore r1, r0
	bx	lr
	endfunc	CpuRWSpinlockWriteUnlockIrq, sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr

#if !SYSMEM_NSKBL
/* void sceKernelRWSpinlockLowReadLock(lock): waits while a writer holds it */
	func	sceKernelRWSpinlockLowReadLock
0:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	1f
	strex	r2, r1, [r0]
	cbz	r2, 2f
1:	wfe
	b.w	0b
2:	dmb	sy
	bx	lr
	endfunc	sceKernelRWSpinlockLowReadLock

/*
 * int SceCpuForDriver_093925BD(lock): 0, or -1 when a writer holds it.
 * As in Sony's code, the retry tests the new count, not the strex status.
 */
	func	SceCpuForDriver_093925BD
0:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	1f
	strex	r2, r1, [r0]
	cmp	r1, #0
	bne	0b
	dmb	sy
	movs	r0, #0
	bx	lr
1:	movs.w	r0, #-1
	bx	lr
	endfunc	SceCpuForDriver_093925BD

/* void CpuRWSpinlockReadUnlock(lock): wakes the waiters when the last reader leaves */
	func	CpuRWSpinlockReadUnlock, sceKernelRWSpinlockLowReadUnlock
	dmb	sy
1:	ldrex	r1, [r0]
	subs	r1, #1
	strex	r2, r1, [r0]
	cmp	r2, #0
	bne	1b
	cbnz	r1, 2f
	dsb	sy
	sev
2:	bx	lr
	endfunc	CpuRWSpinlockReadUnlock, sceKernelRWSpinlockLowReadUnlock
#endif

/* int CpuRWSpinlockReadLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockReadLockIrq, SceCpuForDriver_EC53D007
	mrs	r3, cpsr
	IRQ_OFF
1:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	2f
	strex	r2, r1, [r0]
	cbnz	r2, 3f
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
2:	msr	cpsr_c, r3
	wfe
	IRQ_OFF
3:	b	1b
	endfunc	CpuRWSpinlockReadLockIrq, SceCpuForDriver_EC53D007

#if FW_DEBUG || !SYSMEM_NSKBL	/* not in the release boot loader */
/* int CpuRWSpinlockTryReadLockIrq(lock): the previous I bit, or -1 */
	func	CpuRWSpinlockTryReadLockIrq, SceCpuForDriver_F02467D1
	mrs	r3, cpsr
	IRQ_OFF
1:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	2f
	strex	r2, r1, [r0]
	cmp	r2, #0
	bne	1b
	dmb	sy
	and.w	r0, r3, #PSR_MASK_BITS
	bx	lr
2:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
	endfunc	CpuRWSpinlockTryReadLockIrq, SceCpuForDriver_F02467D1
#endif

/* void CpuRWSpinlockReadUnlockIrq(lock, flags) */
	func	CpuRWSpinlockReadUnlockIrq, sceKernelRWSpinlockLowReadUnlockCpuResumeIntr
	dmb	sy
1:	ldrex	r2, [r0]
	subs	r2, #1
	strex	r3, r2, [r0]
	cmp	r3, #0
	bne	1b
	cbnz	r2, 2f
	dsb	sy
	sev
2:	irq_restore r1, r0
	bx	lr
	endfunc	CpuRWSpinlockReadUnlockIrq, sceKernelRWSpinlockLowReadUnlockCpuResumeIntr
