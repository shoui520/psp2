/* Shared IRQ spinlock operations; native caller entries stay in cpu.c. */
#pragma once

#include <stdint.h>
#include <sce/barrier.h>
#include <sce/excpmgr_cpu.h>
#include "../../../../common/sysclib/atomic.h"

/* SCTLR */
#define SCTLR_A			0x00000002	/* alignment fault checking */
#define SCTLR_C			0x00000004	/* data cache */
#define SCTLR_Z			0x00000800	/* branch prediction */
#define SCTLR_I			0x00001000	/* instruction cache */

static __attribute__((always_inline)) inline
int SpinlockLockIrq(volatile uint32_t *lock, uintptr_t caller)
{
	uint32_t status;

	status = readCpsr();
	disableIrq();
	for (;;) {
		if (AtomicLoadReserved32(lock)) {
			writeCpsrControl(status);
			wfe();
			disableIrq();
			continue;
		}
		if (!AtomicStoreReserved32(lock, caller))
			break;
	}
	AtomicFullBarrier();
	return status & PSR_I_BIT;
}

static __attribute__((always_inline)) inline
int RWSpinlockReadLockIrq(volatile uint32_t *lock)
{
	uint32_t status, next;

	status = readCpsr();
	disableIrq();
	for (;;) {
		next = AtomicLoadReserved32(lock) + 1;
		if ((int32_t)next < 0) {
			writeCpsrControl(status);
			wfe();
			disableIrq();
			continue;
		}
		if (!AtomicStoreReserved32(lock, next))
			break;
	}
	AtomicFullBarrier();
	return status & PSR_I_BIT;
}

static __attribute__((always_inline)) inline
int RWSpinlockTryReadLockIrq(volatile uint32_t *lock)
{
	uint32_t status, next;

	status = readCpsr();
	disableIrq();
	do {
		next = AtomicLoadReserved32(lock) + 1;
		if ((int32_t)next < 0) {
			writeCpsrControl(status);
			return -1;
		}
	} while (AtomicStoreReserved32(lock, next));
	AtomicFullBarrier();
	return status & PSR_I_BIT;
}

static __attribute__((always_inline)) inline
void SpinlockUnlockIrq(volatile uint32_t *lock, uint32_t flags)
{
	AtomicFullBarrier();
	*lock = 0;
	dsb();
	sev();
	if (!flags)
		enableIrq();
}

static __attribute__((always_inline)) inline
void RWSpinlockReadUnlock(volatile uint32_t *lock)
{
	uint32_t next;

	AtomicFullBarrier();
	do {
		next = AtomicLoadReserved32(lock) - 1;
	} while (AtomicStoreReserved32(lock, next));
	if (!next)
		dsb();
		sev();
}

static __attribute__((always_inline)) inline
void RWSpinlockReadUnlockIrq(volatile uint32_t *lock, uint32_t flags)
{
	RWSpinlockReadUnlock(lock);
	if (!flags)
		enableIrq();
}

static __attribute__((always_inline)) inline
int SpinlockTrylockIrq(volatile uint32_t *lock, uintptr_t caller)
{
	uint32_t status;

	status = readCpsr();
	disableIrq();
	do {
		if (AtomicLoadReserved32(lock)) {
			writeCpsrControl(status);
			return -1;
		}
	} while (AtomicStoreReserved32(lock, caller));
	AtomicFullBarrier();
	return status & PSR_I_BIT;
}
