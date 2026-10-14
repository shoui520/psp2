/* CPU identity and debug-context diagnostics. */
#include "sce/cp15.h"
#include <stdint.h>
#include <stddef.h>
#include <sce/barrier.h>
#include <sce/cpu_id.h>
#include <sce/debug.h>
#include <sce/threadcb.h>
#include "../../../../common/sysclib/atomic.h"
#include "cpu_irq.h"

extern void *memset(void *destination, int value, size_t size);
extern int sceKernelSysrootBacktrace(int pid, void *buffer, size_t capacity,
				     uint32_t *length, int mode);
extern int sceKernelSysrootPrintBacktrace(uint32_t type, const void *buffer,
				     uint32_t length, int flags);

/* Name unknown; only the callback prefix is accessed by CPU diagnostics. */
struct CpuLongIrqHandlerRecord {
	int (*notify)(int kind, uint32_t duration, unsigned core,
		       uintptr_t lockedCaller, const void *resumeCaller);
};

#if FW_DEBUG
volatile uintptr_t s_cpuLastLockCaller[4];
volatile uint32_t s_cpuIrqDisabledAt[4];
struct CpuLongIrqHandlerRecord *volatile s_cpuLongIrqHandlerRecord;
#endif

void SceCpuForKernel_B331AB16(struct CpuLongIrqHandlerRecord *record)
{
#if FW_DEBUG
	s_cpuLongIrqHandlerRecord = record;
#else
	(void)record;
#endif
}

extern __typeof__(SceCpuForKernel_B331AB16) CpuRegisterLongIrqHandler
	__attribute__((alias("SceCpuForKernel_B331AB16")));

extern uint32_t SceSysrootForKernel_41636522(void);

#if FW_DEBUG

__attribute__((noinline))
int CpuSpinlockLockIrqCore(volatile uint32_t *lock)
{
	return SpinlockLockIrq(lock, (uintptr_t)__builtin_return_address(0));
}

/* Alternate native entry supplies the caller before joining the same body. */
__attribute__((noinline))
int CpuSpinlockLockIrqWithCaller(volatile uint32_t *lock, uintptr_t caller)
{
	return SpinlockLockIrq(lock, caller);
}

__attribute__((noinline))
int CpuSpinlockTrylockIrqCore(volatile uint32_t *lock)
{
	return SpinlockTrylockIrq(lock, (uintptr_t)__builtin_return_address(0));
}

/* Alternate native entry supplies the caller before joining the same body. */
__attribute__((noinline))
int CpuSpinlockTrylockIrqWithCaller(volatile uint32_t *lock, uintptr_t caller)
{
	return SpinlockTrylockIrq(lock, caller);
}

__attribute__((noinline))
int CpuRWSpinlockReadLockIrq(volatile uint32_t *lock)
{
	return RWSpinlockReadLockIrq(lock);
}

__attribute__((noinline))
int CpuRWSpinlockWriteLockIrq(volatile uint32_t *lock)
{
	return SpinlockLockIrq(lock, 0x80000000);
}

__attribute__((noinline))
int CpuRWSpinlockTryWriteLockIrq(volatile uint32_t *lock)
{
	return SpinlockTrylockIrq(lock, 0x80000000);
}

__attribute__((noinline))
int CpuRWSpinlockTryReadLockIrq(volatile uint32_t *lock)
{
	return RWSpinlockTryReadLockIrq(lock);
}

int ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuRWSpinlockTryReadLockIrq(lock);

	if (previous >= 0 && !(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuRWSpinlockWriteLockIrq(lock);

	if (!(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuRWSpinlockTryWriteLockIrq(lock);

	if (previous >= 0 && !(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuRWSpinlockReadLockIrq(lock);

	if (!(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}
#endif

__attribute__((noinline))
int CpuSuspendIntrLow(void)
{
	uint32_t status;

	status = readCpsr();
	status &= PSR_I_BIT;
	if (!status)
		disableIrq();
	return status;
}

__attribute__((noinline))
int CpuResumeIntrLow(int flags)
{
	if (!(flags & PSR_I_BIT))
		enableIrq();
	return (uint32_t)flags >> 8;
}

#if FW_DEBUG
int ksceKernelSpinlockLowTrylockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuSpinlockTrylockIrqWithCaller(lock, caller);

	if (previous >= 0 && !(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelSpinlockLowLockCpuSuspendIntr(volatile uint32_t *lock)
{
	uintptr_t caller = (uintptr_t)__builtin_return_address(0);
	int previous = CpuSpinlockLockIrqWithCaller(lock, caller);

	if (!(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = caller;
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelCpuSuspendIntr(void)
{
	int previous = CpuSuspendIntrLow();

	if (!(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = (uintptr_t)__builtin_return_address(0);
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int ksceKernelCpuResumeIntr(int flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(165, "sceKernelCpuResumeIntr"),
			"***\n*** warning: sceKernelCpuResumeIntr() called. But IRQ has been enabled.(lr=%p)\n***\n",
			caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(166, "sceKernelCpuResumeIntr"), caller);
	} else if (!(flags & PSR_I_BIT)) {
		struct CpuLongIrqHandlerRecord *record;
		uint32_t now, duration;
		int cpu;
		uintptr_t lockedCaller;
		int result;

		cpu = SceCpuId();
		now = SceSysrootForKernel_41636522();
		duration = now - s_cpuIrqDisabledAt[cpu];
		lockedCaller = s_cpuLastLockCaller[cpu];
		result = CpuResumeIntrLow(flags);
		if (duration > 1000) {
			record = s_cpuLongIrqHandlerRecord;
			if (record)
				return record->notify(0, duration, cpu, lockedCaller, caller);
		}
		return result;
	}
	return CpuResumeIntrLow(flags);
}
#else
int ksceKernelSpinlockLowLockCpuSuspendIntr(volatile uint32_t *lock)
{
	return SpinlockLockIrq(lock, (uintptr_t)__builtin_return_address(0));
}

int ksceKernelSpinlockLowTrylockCpuSuspendIntr(volatile uint32_t *lock)
{
	return SpinlockTrylockIrq(lock, (uintptr_t)__builtin_return_address(0));
}

extern __typeof__(CpuSuspendIntrLow) ksceKernelCpuSuspendIntr
	__attribute__((alias("CpuSuspendIntrLow")));
extern __typeof__(CpuResumeIntrLow) ksceKernelCpuResumeIntr
	__attribute__((alias("CpuResumeIntrLow")));
#endif

__attribute__((noinline))
int sceKernelGetCpuId(void)
{
	return readMpidr() & 15;
}

#if FW_DEBUG
int ksceKernelCpuId(void)
{
	uint32_t status;
	uint32_t length;
	char buffer[512];

	if (getCurrentThreadCB() != NULL) {
		status = readCpsr();
		if (!(status & PSR_I_BIT)) {
			ksceKernelPrintfLevelWithInfo(1, 0,
				SCE_DBGINFO_FUNC(488, "sceKernelCpuId"),
				"***\n*** warning: sceKernelCpuId() called. But IRQ has been enabled.(lr=%p)\n***\n",
				__builtin_return_address(0));
			memset(buffer, 0, sizeof(buffer));
			if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer),
						       &length, 3) >= 0)
				sceKernelSysrootPrintBacktrace(SCE_KERNEL_PROCESS_ID, buffer, length, 0);
			memset(buffer, 0, sizeof(buffer));
			if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer),
						       &length, 2) >= 0)
				sceKernelSysrootPrintBacktrace(0, buffer, length, 0);
		}
	}
	return sceKernelGetCpuId();
}
#else
extern __typeof__(sceKernelGetCpuId) ksceKernelCpuId
	__attribute__((alias("sceKernelGetCpuId")));
#endif

void ksceKernelSpinlockLowLock(volatile uint32_t *lock)
{
	uint32_t caller = (uintptr_t)__builtin_return_address(0);

	for (;;) {
		if (AtomicLoadReserved32(lock)) {
			wfe();
			continue;
		}
		if (!AtomicStoreReserved32(lock, caller))
			break;
	}
	AtomicFullBarrier();
}

int ksceKernelSpinlockLowTrylock(volatile uint32_t *lock)
{
	uint32_t caller = (uintptr_t)__builtin_return_address(0);

	do {
		if (AtomicLoadReserved32(lock))
			return -1;
	} while (AtomicStoreReserved32(lock, caller));
	AtomicFullBarrier();
	return 0;
}

#if FW_DEBUG
__attribute__((noinline))
#else
static inline __attribute__((always_inline))
#endif
void CpuSpinlockUnlock(volatile uint32_t *lock)
{
	AtomicFullBarrier();
	*lock = 0;
	dsb();
	sev();
}

void ksceKernelSpinlockLowUnlock(volatile uint32_t *lock)
{
#if FW_DEBUG
	if (!*lock) {
		void *caller = __builtin_return_address(0);

		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(232, "sceKernelSpinlockLowUnlock"),
			"***\n*** warning: sceKernelSpinlockLowUnlock() called. But spinlock has not been locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(233, "sceKernelSpinlockLowUnlock"), caller);
	}
#endif
	CpuSpinlockUnlock(lock);
}

void ksceKernelRWSpinlockLowReadLock(volatile uint32_t *lock)
{
	for (;;) {
		uint32_t next = AtomicLoadReserved32(lock) + 1;

		if ((int32_t)next >= 0 && !AtomicStoreReserved32(lock, next))
			break;
		wfe();
	}
	AtomicFullBarrier();
}

int ksceKernelRWSpinlockLowTryWriteLock(volatile uint32_t *lock)
{
	do {
		if (AtomicLoadReserved32(lock))
			return -1;
	} while (AtomicStoreReserved32(lock, 0x80000000U));
	AtomicFullBarrier();
	return 0;
}

void ksceKernelRWSpinlockLowWriteLock(volatile uint32_t *lock)
{
	for (;;) {
		if (AtomicLoadReserved32(lock)) {
			wfe();
			continue;
		}
		/* Native code returns on failed STREX, and retries on success. */
		if (AtomicStoreReserved32(lock, 0x80000000U))
			break;
	}
	AtomicFullBarrier();
}

int ksceKernelRWSpinlockLowTryReadLock(volatile uint32_t *lock)
{
	uint32_t next;

	do {
		next = AtomicLoadReserved32(lock) + 1;
		if ((int32_t)next < 0)
			return -1;
		/* Native retry tests the incremented word, not STREX status. */
		(void)AtomicStoreReserved32(lock, next);
	} while (next);
	AtomicFullBarrier();
	return 0;
}

uint32_t SceCpuForKernel_9818A20B(uint32_t value)
{
	return value | 0x4a;
}

void ksceKernelCpuBranchPredictorInvalidateAll(void)
{
	uint32_t zero = 0;

	writeBpiall(zero);
	dsb();
	isb();
}

void ksceKernelCpuBranchPredictorInvalidateAllIS(void)
{
	uint32_t zero = 0;

	writeBpiallis(zero);
	dsb();
	isb();
}

uint8_t ksceKernelCpuGetCONTEXTIDR(void)
{
	return readContextId();
}

void ksceKernelCpuUpdateSCTLR(void)
{
	uint32_t control;

	control = readSctlr();
	control |= SCTLR_Z | SCTLR_I | SCTLR_C | SCTLR_A;
	writeSctlr(control);
	nop();
	nop();
}

#if FW_DEBUG
extern __typeof__(CpuSpinlockUnlock) CpuRWSpinlockWriteUnlock
	__attribute__((alias("CpuSpinlockUnlock")));

__attribute__((noinline))
void CpuSpinlockUnlockIrq(volatile uint32_t *lock, uint32_t flags)
{
	SpinlockUnlockIrq(lock, flags);
}

extern __typeof__(CpuSpinlockUnlockIrq) CpuRWSpinlockWriteUnlockIrq
	__attribute__((alias("CpuSpinlockUnlockIrq")));

void ksceKernelSpinlockLowUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (!*lock) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(252, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelSpinlockLowUnlockCpuResumeIntr() called. But spinlock has not been locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(253, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(259, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelSpinlockLowUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(260, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
				caller);
	} else if (!(flags & PSR_I_BIT)) {
		struct CpuLongIrqHandlerRecord *record;
		uint32_t now, duration;
		int cpu;
		uintptr_t lockedCaller;

		cpu = SceCpuId();
		now = SceSysrootForKernel_41636522();
		duration = now - s_cpuIrqDisabledAt[cpu];
		lockedCaller = s_cpuLastLockCaller[cpu];
		CpuSpinlockUnlockIrq(lock, flags);
		if (duration > 1000) {
			record = s_cpuLongIrqHandlerRecord;
			if (record)
				record->notify(1, duration, cpu, lockedCaller, caller);
		}
		return;
	}
	CpuSpinlockUnlockIrq(lock, flags);
}

__attribute__((noinline))
void CpuRWSpinlockReadUnlockIrq(volatile uint32_t *lock, uint32_t flags)
{
	RWSpinlockReadUnlockIrq(lock, flags);
}

__attribute__((noinline))
void CpuRWSpinlockReadUnlock(volatile uint32_t *lock)
{
	RWSpinlockReadUnlock(lock);
}

void ksceKernelRWSpinlockLowReadUnlock(volatile uint32_t *lock)
{
	if (!*lock || *lock == 0x80000000U) {
		void *caller = __builtin_return_address(0);

		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(330, "sceKernelRWSpinlockLowReadUnlock"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlock() called. But spinlock has not been read locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(331, "sceKernelRWSpinlockLowReadUnlock"), caller);
	}
	CpuRWSpinlockReadUnlock(lock);
}

void ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (!*lock || *lock == 0x80000000U) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(349, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlockCpuResumeIntr() called. But spinlock has not been read locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(350, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(356, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(357, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
				caller);
	} else if (!(flags & PSR_I_BIT)) {
		struct CpuLongIrqHandlerRecord *record;
		uint32_t now, duration;
		int cpu;
		uintptr_t lockedCaller;

		cpu = SceCpuId();
		now = SceSysrootForKernel_41636522();
		duration = now - s_cpuIrqDisabledAt[cpu];
		lockedCaller = s_cpuLastLockCaller[cpu];
		CpuRWSpinlockReadUnlockIrq(lock, flags);
		if (duration > 1000) {
			record = s_cpuLongIrqHandlerRecord;
			if (record)
				record->notify(2, duration, cpu, lockedCaller, caller);
		}
		return;
	}
	CpuRWSpinlockReadUnlockIrq(lock, flags);
}

void ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (*lock != 0x80000000U) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(445, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr() called. But spinlock has not been write locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(446, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(452, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(453, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
				caller);
	} else if (!(flags & PSR_I_BIT)) {
		struct CpuLongIrqHandlerRecord *record;
		uint32_t now, duration;
		int cpu;
		uintptr_t lockedCaller;

		cpu = SceCpuId();
		now = SceSysrootForKernel_41636522();
		duration = now - s_cpuIrqDisabledAt[cpu];
		lockedCaller = s_cpuLastLockCaller[cpu];
		CpuRWSpinlockWriteUnlockIrq(lock, flags);
		if (duration > 1000) {
			record = s_cpuLongIrqHandlerRecord;
			if (record)
				record->notify(3, duration, cpu, lockedCaller, caller);
		}
		return;
	}
	CpuRWSpinlockWriteUnlockIrq(lock, flags);
}

void ksceKernelRWSpinlockLowWriteUnlock(volatile uint32_t *lock)
{
	if (*lock != 0x80000000U) {
		void *caller = __builtin_return_address(0);

		ksceKernelPrintfLevelWithInfo(1, 0,
			SCE_DBGINFO_FUNC(426, "sceKernelRWSpinlockLowWriteUnlock"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlock() called. But spinlock has not been write locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelAssertLevel(1, 0,
				SCE_DBGINFO_FUNC(427, "sceKernelRWSpinlockLowWriteUnlock"), caller);
	}
	CpuRWSpinlockWriteUnlock(lock);
}
#else

int ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(volatile uint32_t *lock)
{
	return RWSpinlockReadLockIrq(lock);
}

int ksceKernelRWSpinlockLowTryReadLockCpuSuspendIntr(volatile uint32_t *lock)
{
	return RWSpinlockTryReadLockIrq(lock);
}

int ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(volatile uint32_t *lock)
{
	return SpinlockLockIrq(lock, 0x80000000);
}

int ksceKernelRWSpinlockLowTryWriteLockCpuSuspendIntr(volatile uint32_t *lock)
{
	return SpinlockTrylockIrq(lock, 0x80000000);
}

void ksceKernelSpinlockLowUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	SpinlockUnlockIrq(lock, flags);
}

void ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	SpinlockUnlockIrq(lock, flags);
}

void ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(volatile uint32_t *lock, uint32_t flags)
{
	RWSpinlockReadUnlockIrq(lock, flags);
}

void ksceKernelRWSpinlockLowReadUnlock(volatile uint32_t *lock)
{
	RWSpinlockReadUnlock(lock);
}

void ksceKernelRWSpinlockLowWriteUnlock(volatile uint32_t *lock)
{
	CpuSpinlockUnlock(lock);
}

#endif
