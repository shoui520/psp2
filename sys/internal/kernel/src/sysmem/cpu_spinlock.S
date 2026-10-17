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
	func	sceKernelGetCpuId, sceKernelCpuId
	mrc	p15, 0, r0, c0, c0, 5
	and.w	r0, r0, #0xf
	bx	lr
	endfunc	sceKernelGetCpuId, sceKernelCpuId

/* void SceCpuForDriver_BF82DEB2(lock): spins (wfe) until the lock is free */
	func	SceCpuForDriver_BF82DEB2
0:	ldrex	r1, [r0]
	cbnz	r1, 1f
	strex	r1, lr, [r0]
	cbnz	r1, 2f
	dmb	sy
	bx	lr
1:	wfe
2:	b	0b
	endfunc	SceCpuForDriver_BF82DEB2

/* int SceCpuForDriver_5AC9D394(lock): 0, or -1 when the lock is taken */
	func	SceCpuForDriver_5AC9D394
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
	endfunc	SceCpuForDriver_5AC9D394

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
	func	CpuSpinlockLockIrqCore, SceCpuForDriver_D32ACE9E
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
	endfunc	CpuSpinlockLockIrqWithCaller, SceCpuForDriver_D32ACE9E

/*
 * int CpuSpinlockTrylockIrqCore(lock): as CpuSpinlockLockIrqCore, or -1 with the
 * IRQs as they were when the lock is taken.
 */
	func	CpuSpinlockTrylockIrqCore, SceCpuForDriver_27C0B340
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
	endfunc	CpuSpinlockTrylockIrqWithCaller, SceCpuForDriver_27C0B340

/* void CpuSpinlockUnlockIrq(lock, flags): unlocks; IRQs on unless flags */
	func	CpuSpinlockUnlockIrq, sceKernelSpinlockLowUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	cbnz	r1, 1f
	cpsie	i
1:	bx	lr
	endfunc	CpuSpinlockUnlockIrq, sceKernelSpinlockLowUnlockCpuResumeIntr

/*
 * void SceCpuForDriver_3F42B434(lock). As in Sony's code, a successful
 * strex loops back (and then waits on its own lock) and a failed one returns.
 */
	func	SceCpuForDriver_3F42B434
	movs.w	r2, #RW_WRITER
1:	ldrex	r1, [r0]
	cbnz	r1, 2f
	strex	r1, r2, [r0]
	cbz	r1, 3f
	dmb	sy
	bx	lr
2:	wfe
3:	b	1b
	endfunc	SceCpuForDriver_3F42B434

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

/* int CpuRWSpinlockWriteLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockWriteLockIrq, SceCpuForDriver_4C38CE4D
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
	endfunc	CpuRWSpinlockWriteLockIrq, SceCpuForDriver_4C38CE4D

/* int CpuRWSpinlockTryWriteLockIrq(lock): the previous I bit, or -1 when held */
	func	CpuRWSpinlockTryWriteLockIrq, SceCpuForDriver_DE6482C6
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
	endfunc	CpuRWSpinlockTryWriteLockIrq, SceCpuForDriver_DE6482C6

/* void CpuRWSpinlockWriteUnlockIrq(lock, flags) */
	func	CpuRWSpinlockWriteUnlockIrq, sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr
	movs	r2, #0
	dmb	sy
	str	r2, [r0]
	dsb	sy
	sev
	cbnz	r1, 1f
	cpsie	i
1:	bx	lr
	endfunc	CpuRWSpinlockWriteUnlockIrq, sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr

/* void SceCpuForDriver_CAC9AE80(lock): waits while a writer holds it */
	func	SceCpuForDriver_CAC9AE80
0:	ldrex	r1, [r0]
	adds	r1, #1
	bmi	1f
	strex	r2, r1, [r0]
	cbz	r2, 2f
1:	wfe
	b.w	0b
2:	dmb	sy
	bx	lr
	endfunc	SceCpuForDriver_CAC9AE80

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

/* int CpuRWSpinlockReadLockIrq(lock): the previous I bit */
	func	CpuRWSpinlockReadLockIrq, SceCpuForDriver_EC53D007
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
	endfunc	CpuRWSpinlockReadLockIrq, SceCpuForDriver_EC53D007

/* int CpuRWSpinlockTryReadLockIrq(lock): the previous I bit, or -1 */
	func	CpuRWSpinlockTryReadLockIrq, SceCpuForDriver_F02467D1
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
	endfunc	CpuRWSpinlockTryReadLockIrq, SceCpuForDriver_F02467D1

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
2:	cbnz	r1, 3f
	cpsie	i
3:	bx	lr
	endfunc	CpuRWSpinlockReadUnlockIrq, sceKernelRWSpinlockLowReadUnlockCpuResumeIntr
