/*
 * SceKernelThreadMgr: reading a thread's CPU affinity mask. A thread
 * without one reports its process's default to callers built with an SDK
 * before 2.10; the syscall gives it in bits 16-19.
 */

#include "private.h"
#include "sce/intrmgr.h"
#include "affinity.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* ksceKernelGetThreadCpuAffinityMask: uid's mask (0: the caller's) */
int ksceKernelGetThreadCpuAffinityMask(SceUID uid)
{
	struct ThreadmgrThreadObject *object;
	SceKernelIntrStatus intr;
	uint32_t saved, version;
	int result, inIntr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	inIntr = ksceKernelIsIntrAllowedInCurrentContext();
	if (uid == 0) {
		if (inIntr) {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 5822, "sceKernelGetThreadCpuAffinityMask",
						   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		uid = currentThreadContext() != NULL ? currentThreadContext()->threadId :
						       (SceUID)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	result = ksceGUIDReferObjectWithClass(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		result = object->context.currentCpuAffinityMask;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		if (result == 0 && currentThreadContext() != NULL &&
		    currentThreadContext()->processId != THREADMGR_KERNEL_PID &&
		    (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, &version) < 0 ||
		     version < SDK_VERSION_DEFAULT_AFFINITY))
			result = (object->context.process)->
				cpuAffinity;
		ksceGUIDReleaseObject(uid);
	}
	ksceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelGetThreadCpuAffinityMask (user): the mask in bits 16-19 */
int SceThreadmgr_sceKernelGetThreadCpuAffinityMask(SceUID uid)
{
	int result;

	if (uid != 0) {
		result = kscePUIDtoGUID(0, uid);
		if (result < 0)
			return USER_ERROR_UNKNOWN_THREAD_ID;
		uid = result;
	}
	result = ksceKernelGetThreadCpuAffinityMask(uid);
	return result < 0 ? result : (int)((uint32_t)result << 16);
}
