/*
 * SceCpu: the CPU number and the spinlock primitives, hand-written ldrex/strex
 * sequences that keep no frame. Two entries share one body in the IRQ lock
 * functions (the caller's address from lr, or given in r1).
 *
 * The same object in every firmware. A debug kernel (FW_DEBUG) wraps the exports
 * in cpu.c, which checks the lock and times how long the IRQs stay off; a release
 * kernel exports these functions directly. The internal names are not Sony's
 * (unknown).
 *
 * A lock word holds the address of its owner (spinlock), or 0x80000000 for a
 * writer and the reader count otherwise (RW spinlock).
 */

	.syntax unified
	.arch armv7-a
	.thumb
	.text

#define PSR_I_BIT	0x80
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
	.balign	4
#if FW_DEBUG
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
#if FW_DEBUG
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
	func	sceKernelGetCpuId, ksceKernelCpuId
	mrc	p15, 0, r0, c0, c0, 5
	and.w	r0, r0, #0xf
	bx	lr
	endfunc	sceKernelGetCpuId, ksceKernelCpuId

/* void ksceKernelSpinlockLowLock(lock): spins (wfe) until the lock is free */
	func	ksceKernelSpinlockLowLock
0:	ldrex	r1, [r0]
	cbnz	r1, 1f
	strex	r1, lr, [r0]
	cbnz	r1, 2f
	dmb	sy
	bx	lr
1:	wfe
2:	b	0b
	endfunc	ksceKernelSpinlockLowLock

/* int ksceKernelSpinlockLowTrylock(lock): 0, or -1 when the lock is taken */
	func	ksceKernelSpinlockLowTrylock
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
	endfunc	ksceKernelSpinlockLowTrylock

/* void CpuSpinlockUnlock(lock) */
	func	CpuSpinlockUnlock, ksceKernelSpinlockLowUnlock
	movs	r1, #0
	dmb	sy
	str	r1, [r0]
	dsb	sy
	sev
	bx	lr
	endfunc	CpuSpinlockUnlock, ksceKernelSpinlockLowUnlock

/*
 * int CpuSpinlockLockIrqCore(lock): takes the lock with the IRQs off and returns
 * the previous I bit; the IRQs are back on while it waits.
 * CpuSpinlockLockIrqWithCaller(lock, caller) records caller as the owner.
 */
	func	CpuSpinlockLockIrqCore, ksceKernelSpinlockLowLockCpuSuspendIntr
	mov	r2, lr
1:	mrs	r3, cpsr
	cpsid	i
2:	ldrex	r1, [r0]
	cbnz	r1, 3f
	strex	r1, r2, [r0]
	cbnz	r1, 4f
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
3:	msr	cpsr_c, r3
	wfe
	cpsid	i
4:	b	2b
#if FW_DEBUG
	.size	CpuSpinlockLockIrqCore, . - CpuSpinlockLockIrqCore
	sym	CpuSpinlockLockIrqWithCaller
#endif
	mov	r2, r1
	b	1b
	endfunc	CpuSpinlockLockIrqWithCaller, ksceKernelSpinlockLowLockCpuSuspendIntr

/*
 * int CpuSpinlockTrylockIrqCore(lock): as CpuSpinlockLockIrqCore, or -1 with the
 * IRQs as they were when the lock is taken.
 */
	func	CpuSpinlockTrylockIrqCore, ksceKernelSpinlockLowTrylockCpuSuspendIntr
	mov	r2, lr
1:	mrs	r3, cpsr
	cpsid	i
2:	ldrex	r1, [r0]
	cbnz	r1, 3f
	strex	r1, r2, [r0]
	cmp	r1, #0
	bne	2b
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
3:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
#if FW_DEBUG
	.size	CpuSpinlockTrylockIrqCore, . - CpuSpinlockTrylockIrqCore
	sym	CpuSpinlockTrylockIrqWithCaller
#endif
	mov	r2, r1
	b	1b
	endfunc	CpuSpinlockTrylockIrqWithCaller, ksceKernelSpinlockLowTrylockCpuSuspendIntr

/* void CpuSpinlockUnlockIrq(lock, flags): unlocks; IRQs on unless flags */
	func	CpuSpinlockUnlockIrq, ksceKernelSpinlockLowUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	cbnz	r1, 1f
	cpsie	i
1:	bx	lr
	endfunc	CpuSpinlockUnlockIrq, ksceKernelSpinlockLowUnlockCpuResumeIntr

/*
 * void ksceKernelRWSpinlockLowWriteLock(lock). As in Sony's code, a successful
 * strex loops back (and then waits on its own lock) and a failed one returns.
 */
	func	ksceKernelRWSpinlockLowWriteLock
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cbz	r1, 3f
	dmb	sy
	bx	lr
2:	wfe
3:	b	1b
	endfunc	ksceKernelRWSpinlockLowWriteLock

/* int ksceKernelRWSpinlockLowTryWriteLock(lock): 0, or -1 when held */
	func	ksceKernelRWSpinlockLowTryWriteLock
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
	endfunc	ksceKernelRWSpinlockLowTryWriteLock

/* void CpuRWSpinlockWriteUnlock(lock) */
	func	CpuRWSpinlockWriteUnlock, ksceKernelRWSpinlockLowWriteUnlock
	movs	r1, #0
	dmb	sy
	str	r1, [r0]
	dsb	sy
	sev
	bx	lr
	endfunc	CpuRWSpinlockWriteUnlock, ksceKernelRWSpinlockLowWriteUnlock

/* int CpuRWSpinlockWriteLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockWriteLockIrq, ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr
	mrs	r3, cpsr
	movs.w	r2, #RW_WRITER
	cpsid	i
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cbnz	r1, 3f
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
2:	msr	cpsr_c, r3
	wfe
	cpsid	i
3:	b	1b
	endfunc	CpuRWSpinlockWriteLockIrq, ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr

/* int CpuRWSpinlockTryWriteLockIrq(lock): the previous I bit, or -1 when held */
	func	CpuRWSpinlockTryWriteLockIrq, ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr
	mrs	r3, cpsr
	cpsid	i
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cmp	r1, #0
	bne	1b
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
2:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
	endfunc	CpuRWSpinlockTryWriteLockIrq, ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr

/* void CpuRWSpinlockWriteUnlockIrq(lock, flags) */
	func	CpuRWSpinlockWriteUnlockIrq, ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	cbnz	r1, 1f
	cpsie	i
1:	bx	lr
	endfunc	CpuRWSpinlockWriteUnlockIrq, ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr

/* void ksceKernelRWSpinlockLowReadLock(lock): waits while a writer holds it */
	func	ksceKernelRWSpinlockLowReadLock
0:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	1f
	strex	r2, r1, [r0]
	cbz	r2, 2f
1:	wfe
	b.w	0b
2:	dmb	sy
	bx	lr
	endfunc	ksceKernelRWSpinlockLowReadLock

/*
 * int ksceKernelRWSpinlockLowTryReadLock(lock): 0, or -1 when a writer holds it.
 * As in Sony's code, the retry tests the new count, not the strex status.
 */
	func	ksceKernelRWSpinlockLowTryReadLock
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
	endfunc	ksceKernelRWSpinlockLowTryReadLock

/* void CpuRWSpinlockReadUnlock(lock): wakes the waiters when the last reader leaves */
	func	CpuRWSpinlockReadUnlock, ksceKernelRWSpinlockLowReadUnlock
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
	endfunc	CpuRWSpinlockReadUnlock, ksceKernelRWSpinlockLowReadUnlock

/* int CpuRWSpinlockReadLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockReadLockIrq, ksceKernelRWSpinlockLowReadLockCpuSuspendIntr
	mrs	r3, cpsr
	cpsid	i
1:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	2f
	strex	r2, r1, [r0]
	cbnz	r2, 3f
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
2:	msr	cpsr_c, r3
	wfe
	cpsid	i
3:	b	1b
	endfunc	CpuRWSpinlockReadLockIrq, ksceKernelRWSpinlockLowReadLockCpuSuspendIntr

/* int CpuRWSpinlockTryReadLockIrq(lock): the previous I bit, or -1 */
	func	CpuRWSpinlockTryReadLockIrq, ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr
	mrs	r3, cpsr
	cpsid	i
1:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	2f
	strex	r2, r1, [r0]
	cmp	r2, #0
	bne	1b
	dmb	sy
	and.w	r0, r3, #PSR_I_BIT
	bx	lr
2:	msr	cpsr_c, r3
	movs.w	r0, #-1
	bx	lr
	endfunc	CpuRWSpinlockTryReadLockIrq, ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr

/* void CpuRWSpinlockReadUnlockIrq(lock, flags) */
	func	CpuRWSpinlockReadUnlockIrq, ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr
	dmb	sy
1:	ldrex	r2, [r0]
	subs	r2, #1
	strex	r3, r2, [r0]
	cmp	r3, #0
	bne	1b
	cbnz	r2, 2f
	dsb	sy
	sev
2:	cbnz	r1, 3f
	cpsie	i
3:	bx	lr
	endfunc	CpuRWSpinlockReadUnlockIrq, ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr
