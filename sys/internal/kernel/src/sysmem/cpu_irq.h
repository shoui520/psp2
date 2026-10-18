/* The spinlock primitives in cpu_spinlock.S, and the SCTLR bits cpu.c sets. */
#pragma once

#include <stdint.h>
#include <sysmem/spinlock.h>
#include <sysmem/barrier.h>
#include <excpmgr/excpmgr_cpu.h>

/* SCTLR */
#define SCTLR_A			0x00000002	/* alignment fault checking */
#define SCTLR_C			0x00000004	/* data cache */
#define SCTLR_Z			0x00000800	/* branch prediction */
#define SCTLR_I			0x00001000	/* instruction cache */

int sceKernelGetCpuId(void);

/* the IRQ-off locks return the previous I bit (PSR_I_BIT), the try forms -1 when held */
SceKernelIntrStatus CpuSpinlockLockIrqCore(volatile SceKernelSpinlock *lock);
SceKernelIntrStatus CpuSpinlockLockIrqWithCaller(volatile SceKernelSpinlock *lock, uintptr_t caller);
SceKernelIntrStatus CpuSpinlockTrylockIrqCore(volatile SceKernelSpinlock *lock);
SceKernelIntrStatus CpuSpinlockTrylockIrqWithCaller(volatile SceKernelSpinlock *lock, uintptr_t caller);
void CpuSpinlockUnlock(volatile SceKernelSpinlock *lock);
void CpuSpinlockUnlockIrq(volatile SceKernelSpinlock *lock, SceKernelIntrStatus flags);

SceKernelIntrStatus CpuRWSpinlockWriteLockIrq(volatile SceKernelRWSpinlock *lock);
SceKernelIntrStatus CpuRWSpinlockTryWriteLockIrq(volatile SceKernelRWSpinlock *lock);
void CpuRWSpinlockWriteUnlock(volatile SceKernelRWSpinlock *lock);
void CpuRWSpinlockWriteUnlockIrq(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags);

SceKernelIntrStatus CpuRWSpinlockReadLockIrq(volatile SceKernelRWSpinlock *lock);
SceKernelIntrStatus CpuRWSpinlockTryReadLockIrq(volatile SceKernelRWSpinlock *lock);
void CpuRWSpinlockReadUnlock(volatile SceKernelRWSpinlock *lock);
void CpuRWSpinlockReadUnlockIrq(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags);
