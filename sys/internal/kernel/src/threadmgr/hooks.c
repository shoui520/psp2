/*
 * SceKernelThreadMgr: the module's runtime state and the setters of its
 * hooks (trace, CPU switch, priority change, debugger) and test mode.
 */

#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

struct ThreadmgrRuntime s_threadmgrRuntime;

struct SceKernelThreadmgrPrivate *SceThreadmgrForKernel_19440C83(void)
{
	return s_threadmgrRuntime.manager;
}

void SceThreadmgrForDriver_B4A05763(void (*idleEntry)(void))
{
	s_threadmgrRuntime.idleEntry = idleEntry;
}

void SceThreadmgrForDriver_032E8F73(SceKernelSpinlock *lock)
{
	sceKernelSpinlockLowLock(lock);
}

int SceThreadmgrForDriver_0A4AD37A(SceKernelSpinlock *lock)
{
	return sceKernelSpinlockLowTrylock(lock);
}

void SceThreadmgrForDriver_C03799E2(SceKernelSpinlock *lock)
{
	sceKernelSpinlockLowUnlock(lock);
}

SceKernelIntrStatus SceThreadmgrForDriver_A90B1E01(SceKernelSpinlock *lock)
{
	return sceKernelSpinlockLowLockCpuSuspendIntr(lock);
}

SceKernelIntrStatus SceThreadmgrForDriver_F8A6013B(SceKernelSpinlock *lock)
{
	return sceKernelSpinlockLowTrylockCpuSuspendIntr(lock);
}

void SceThreadmgrForDriver_F96FE0D5(SceKernelSpinlock *lock,
				SceKernelIntrStatus state)
{
	sceKernelSpinlockLowUnlockCpuResumeIntr(lock, state);
}
