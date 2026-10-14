/*
 * SceKernelThreadMgr: the module's runtime state and the setters of its
 * hooks (trace, CPU switch, priority change, debugger) and test mode.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

struct ThreadmgrRuntime s_threadmgrRuntime;

/* SceThreadmgrForKernel_59D1D5B0: processmgr's process class */
int SceThreadmgrForKernel_59D1D5B0(SceClass *processClass)
{
	s_threadmgrRuntime.manager->processClass = processClass;
	return 0;
}

struct SceKernelThreadmgrPrivate *SceThreadmgrForKernel_19440C83(void)
{
	return s_threadmgrRuntime.manager;
}

void *SceThreadmgrForKernel_565BD2DA(void)
{
	return (void *)s_threadmgrRuntime.manager->threadClass;
}

int sceKernelSetThreadMgrTestMode(void)
{
	if (s_threadmgrRuntime.manager->testMode)
		SCE_DBG_PRINTF(0, 11866, "error: sceKernelSetThreadMgrTestMode(), already set\n");
	s_threadmgrRuntime.manager->testMode = 1;
	return 0;
}

int sceKernelClearThreadMgrTestMode(void)
{
	if (!s_threadmgrRuntime.manager->testMode)
		SCE_DBG_PRINTF(0, 11876, "error: sceKernelClearThreadMgrTestMode(), not set\n");
	s_threadmgrRuntime.manager->testMode = 0;
	return 0;
}

int SceThreadmgrForDriver_5311CFB5(uintptr_t hook, uintptr_t arg)
{
	s_threadmgrRuntime.manager->processDebugSuspendHook = hook;
	s_threadmgrRuntime.manager->processDebugSuspendArg = arg;
	return 0;
}

void SceThreadmgrForDriver_B4A05763(void (*idleEntry)(void))
{
	s_threadmgrRuntime.idleEntry = idleEntry;
}

void SceThreadmgrForKernel_8C07C8EC(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.switchHook = hook;
}

void SceThreadmgrForKernel_F55FDEFB(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.schedTraceHook = hook;
}

void SceThreadmgrForKernel_E2DACBC2(ThreadmgrTraceHook hook)
{
	s_threadmgrRuntime.threadTraceHook = hook;
}

void SceThreadmgrForKernel_A3975A5A(ThreadmgrOnCpuHook hook)
{
	s_threadmgrRuntime.onCpuHook = hook;
}

void SceThreadmgrForKernel_F885ECCA(ThreadmgrOffCpuHook hook)
{
	s_threadmgrRuntime.offCpuHook = hook;
}

void SceThreadmgrForKernel_9605DF7D(ThreadmgrPriorityChangeHook hook)
{
	s_threadmgrRuntime.priorityChangeHook = hook;
}

void SceThreadmgrForDriver_032E8F73(SceKernelSpinlock *lock)
{
	ksceKernelSpinlockLowLock(lock);
}

int SceThreadmgrForDriver_0A4AD37A(SceKernelSpinlock *lock)
{
	return ksceKernelSpinlockLowTrylock(lock);
}

void SceThreadmgrForDriver_C03799E2(SceKernelSpinlock *lock)
{
	ksceKernelSpinlockLowUnlock(lock);
}

SceKernelIntrStatus SceThreadmgrForDriver_A90B1E01(SceKernelSpinlock *lock)
{
	return ksceKernelSpinlockLowLockCpuSuspendIntr(lock);
}

SceKernelIntrStatus SceThreadmgrForDriver_F8A6013B(SceKernelSpinlock *lock)
{
	return ksceKernelSpinlockLowTrylockCpuSuspendIntr(lock);
}

void SceThreadmgrForDriver_F96FE0D5(SceKernelSpinlock *lock,
				SceKernelIntrStatus state)
{
	ksceKernelSpinlockLowUnlockCpuResumeIntr(lock, state);
}
