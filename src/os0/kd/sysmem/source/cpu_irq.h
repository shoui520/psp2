/* The spinlock primitives in cpu_spinlock.S, and the SCTLR bits cpu.c sets. */
#pragma once

#include <stdint.h>
#include <sce/barrier.h>
#include <sce/excpmgr_cpu.h>

/* SCTLR */
#define SCTLR_A			0x00000002	/* alignment fault checking */
#define SCTLR_C			0x00000004	/* data cache */
#define SCTLR_Z			0x00000800	/* branch prediction */
#define SCTLR_I			0x00001000	/* instruction cache */

int sceKernelGetCpuId(void);

/* the IRQ-off locks return the previous I bit (PSR_I_BIT), the try forms -1 when held */
int CpuSpinlockLockIrqCore(volatile uint32_t *lock);
int CpuSpinlockLockIrqWithCaller(volatile uint32_t *lock, uintptr_t caller);
int CpuSpinlockTrylockIrqCore(volatile uint32_t *lock);
int CpuSpinlockTrylockIrqWithCaller(volatile uint32_t *lock, uintptr_t caller);
void CpuSpinlockUnlock(volatile uint32_t *lock);
void CpuSpinlockUnlockIrq(volatile uint32_t *lock, uint32_t flags);

int CpuRWSpinlockWriteLockIrq(volatile uint32_t *lock);
int CpuRWSpinlockTryWriteLockIrq(volatile uint32_t *lock);
void CpuRWSpinlockWriteUnlock(volatile uint32_t *lock);
void CpuRWSpinlockWriteUnlockIrq(volatile uint32_t *lock, uint32_t flags);

int CpuRWSpinlockReadLockIrq(volatile uint32_t *lock);
int CpuRWSpinlockTryReadLockIrq(volatile uint32_t *lock);
void CpuRWSpinlockReadUnlock(volatile uint32_t *lock);
void CpuRWSpinlockReadUnlockIrq(volatile uint32_t *lock, uint32_t flags);
