/* CPU identity and debug-context diagnostics. */
#include <sysmem/cp15.h>
#include <sysmem/uid_guid.h>
#include <stdint.h>
#include <stddef.h>
#include <sysmem/barrier.h>
#include <sysmem/cpu_id.h>
#include <sysmem/debug.h>
#include <threadmgr/threadcb.h>
#include "cpu_irq.h"
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/cpu.h>

/* Name unknown; only the callback prefix is accessed by CPU diagnostics. */
struct CpuLongIrqHandlerRecord {
	int (*notify)(int kind, uint32_t duration, unsigned core,
		       uintptr_t lockedCaller, const void *resumeCaller);
};

#if FW_DEBUG
static volatile uintptr_t s_cpuLastLockCaller[4];
static volatile uint32_t s_cpuIrqDisabledAt[4];
static struct CpuLongIrqHandlerRecord *volatile s_cpuLongIrqHandlerRecord;
#endif

void SceCpuForKernel_B331AB16(struct CpuLongIrqHandlerRecord *record)
{
#if FW_DEBUG
	s_cpuLongIrqHandlerRecord = record;
#else
	(void)record;
#endif
}

#if FW_DEBUG

SceKernelIntrStatus SceCpuForDriver_F02467D1(volatile SceKernelRWSpinlock *lock)
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

SceKernelIntrStatus SceCpuForDriver_4C38CE4D(volatile SceKernelRWSpinlock *lock)
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

SceKernelIntrStatus SceCpuForDriver_DE6482C6(volatile SceKernelRWSpinlock *lock)
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

SceKernelIntrStatus SceCpuForDriver_EC53D007(volatile SceKernelRWSpinlock *lock)
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
SceKernelIntrStatus sceKernelSpinlockLowTrylockCpuSuspendIntr(volatile SceKernelSpinlock *lock)
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

SceKernelIntrStatus sceKernelSpinlockLowLockCpuSuspendIntr(volatile SceKernelSpinlock *lock)
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

int SceCpuForDriver_821FC0EE(void)
{
	int previous = CpuSuspendIntrLow();

	if (!(previous & PSR_I_BIT)) {
		int cpu = SceCpuId();

		s_cpuLastLockCaller[cpu] = (uintptr_t)__builtin_return_address(0);
		s_cpuIrqDisabledAt[cpu] = SceSysrootForKernel_41636522();
	}
	return previous;
}

int sceKernelCpuResumeIntr(int flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(165, "sceKernelCpuResumeIntr"),
			"***\n*** warning: sceKernelCpuResumeIntr() called. But IRQ has been enabled.(lr=%p)\n***\n",
			caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
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
extern __typeof__(CpuSuspendIntrLow) SceCpuForDriver_821FC0EE
	__attribute__((alias("CpuSuspendIntrLow")));
extern __typeof__(CpuResumeIntrLow) sceKernelCpuResumeIntr
	__attribute__((alias("CpuResumeIntrLow")));
#endif

#if FW_DEBUG
int sceKernelCpuId(void)
{
	uint32_t status;
	SceSize length;
	struct SceKernelBacktraceFrameForKernel frames[32];

	if (getCurrentThreadCB() != NULL) {
		status = readCpsr();
		if (!(status & PSR_I_BIT)) {
			SceDebugForDriver_D9703808(1, 0,
				SCE_DBGINFO_FUNC(488, "sceKernelCpuId"),
				"***\n*** warning: sceKernelCpuId() called. But IRQ has been enabled.(lr=%p)\n***\n",
				__builtin_return_address(0));
			memset(frames, 0, sizeof(frames));
			if (sceKernelSysrootBacktrace(0, frames, sizeof(frames),
						       &length, 3) >= 0)
				sceKernelSysrootPrintBacktrace(SCE_KERNEL_PROCESS_ID, frames, length, 0);
			memset(frames, 0, sizeof(frames));
			if (sceKernelSysrootBacktrace(0, frames, sizeof(frames),
						       &length, 2) >= 0)
				sceKernelSysrootPrintBacktrace(0, frames, length, 0);
		}
	}
	return sceKernelGetCpuId();
}
#endif

#if FW_DEBUG
void sceKernelSpinlockLowUnlock(volatile SceKernelSpinlock *lock)
{
	if (!*lock) {
		void *caller = __builtin_return_address(0);

		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(232, "sceKernelSpinlockLowUnlock"),
			"***\n*** warning: sceKernelSpinlockLowUnlock() called. But spinlock has not been locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(233, "sceKernelSpinlockLowUnlock"), caller);
	}
	CpuSpinlockUnlock(lock);
}
#endif

int SceCpuForKernel_050B8EF6(void)
{
	writeBpiall(0);
	dsb();
	isb();
	return 0;
}

int SceCpuForKernel_7B0643E2(void)
{
	writeBpiallis(0);
	dsb();
	isb();
	return 0;
}

void SceCpuForKernel_C6426313(void)
{
	uint32_t control;

	control = readSctlr();
	control |= SCTLR_Z | SCTLR_I | SCTLR_C | SCTLR_A;
	writeSctlr(control);
	nop();
	nop();
}

#if FW_DEBUG
void sceKernelSpinlockLowUnlockCpuResumeIntr(volatile SceKernelSpinlock *lock, SceKernelIntrStatus flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (!*lock) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(252, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelSpinlockLowUnlockCpuResumeIntr() called. But spinlock has not been locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(253, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(259, "sceKernelSpinlockLowUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelSpinlockLowUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
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

void sceKernelRWSpinlockLowReadUnlock(volatile SceKernelRWSpinlock *lock)
{
	if (!*lock || *lock == 0x80000000U) {
		void *caller = __builtin_return_address(0);

		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(330, "sceKernelRWSpinlockLowReadUnlock"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlock() called. But spinlock has not been read locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(331, "sceKernelRWSpinlockLowReadUnlock"), caller);
	}
	CpuRWSpinlockReadUnlock(lock);
}

void sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (!*lock || *lock == 0x80000000U) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(349, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlockCpuResumeIntr() called. But spinlock has not been read locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(350, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(356, "sceKernelRWSpinlockLowReadUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowReadUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
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

void sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(volatile SceKernelRWSpinlock *lock, SceKernelIntrStatus flags)
{
	void *caller = __builtin_return_address(0);
	uint32_t status;

	if (*lock != 0x80000000U) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(445, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr() called. But spinlock has not been write locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(446, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
				caller);
	}
	status = readCpsr();
	if (!(status & PSR_I_BIT)) {
		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(452, "sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr() called. But IRQ has been enabled.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
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

void sceKernelRWSpinlockLowWriteUnlock(volatile SceKernelRWSpinlock *lock)
{
	if (*lock != 0x80000000U) {
		void *caller = __builtin_return_address(0);

		SceDebugForDriver_D9703808(1, 0,
			SCE_DBGINFO_FUNC(426, "sceKernelRWSpinlockLowWriteUnlock"),
			"***\n*** warning: sceKernelRWSpinlockLowWriteUnlock() called. But spinlock has not been write locked.(lock=%p, lr=%p)\n***\n",
			lock, caller);
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_FD753E7A(1, 0,
				SCE_DBGINFO_FUNC(427, "sceKernelRWSpinlockLowWriteUnlock"), caller);
	}
	CpuRWSpinlockWriteUnlock(lock);
}
#endif
