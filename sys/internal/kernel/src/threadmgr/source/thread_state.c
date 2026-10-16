/*
 * SceKernelThreadMgr: small thread state operations: the running thread's
 * attributes, the active CPU mask, a thread's exit status.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREAD_ATTR_USER_SETTABLE	0x81000000	/* what a syscall may change */
#define THREAD_ATTR_KERNEL_SETTABLE	0xf0000000	/* what a kernel caller may change */
#define THREAD_PERMISSION_SYSTEM	0x40	/* PLACEHOLDER: names */
#define THREAD_PERMISSION_KERNEL	0x80

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* ksceKernelChangeCurrentThreadAttr: the running thread's attributes, clear then set */
int ksceKernelChangeCurrentThreadAttr(uint32_t clear, uint32_t set)
{
	struct ThreadCB *thread = currentThreadContext();
	SceKernelIntrStatus intr;
	uint32_t saved;
	int result = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ThreadmgrInSyscall()) {
		if ((clear | set) & ~THREAD_ATTR_USER_SETTABLE)
			result = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	} else if ((clear | set) & ~THREAD_ATTR_KERNEL_SETTABLE) {
		result = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	} else if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 7392, "sceKernelChangeCurrentThreadAttr",
					   "*** error: illegal context\n");
		result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = ksceKernelCpuSuspendIntr();
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->currentAttr = (thread->currentAttr & ~clear) | set;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* SceThreadmgrForDriver_86DAE59B: the active CPU mask, for system threads */
uint32_t SceThreadmgrForDriver_86DAE59B(void)
{
	struct ThreadCB *thread = currentThreadContext();
	SceKernelIntrStatus intr;
	uint32_t saved, result = SCE_ERROR_ERRNO_ENOSYS;
	int permission;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	permission = thread != NULL ? thread->permission : 0;
	if (thread != NULL && (permission == THREAD_PERMISSION_SYSTEM ||
			       permission == THREAD_PERMISSION_KERNEL)) {
		intr = ksceKernelCpuSuspendIntr();
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		result = s_threadmgrRuntime.manager->activeCpuMask;
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		ksceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelGetThreadExitStatus: an ended thread's exit status */
int sceKernelGetThreadExitStatus(SceUID uid, int *status)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (uid == 0 || currentThreadContext()->threadId == uid) {
		result = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		SCE_DBG_PRINTF(0, 7507, "*** error: sceKernelGetThreadExitStatus(0x%08X[%s])=0x%08X\n",
			       uid, ksceGUIDName(uid), SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID);
	} else if (status == NULL) {
		result = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else {
		intr = ksceKernelCpuSuspendIntr();
		result = ksceGUIDReferObjectWithClass(uid,
			s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
		if (result < 0) {
			result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			if (ThreadmgrInSyscall())
				result |= SYSCALL_ERROR_BIT;
		} else {
			thread = &object->context;
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			if (!(thread->status & THREAD_STATE_ENDED))
				result = SCE_KERNEL_ERROR_NOT_DORMANT;
			else if (thread->exitStatus == (int)SCE_KERNEL_ERROR_DORMANT)
				result = SCE_KERNEL_ERROR_DORMANT;
			else
				*status = thread->exitStatus;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceGUIDReleaseObject(uid);
		}
		ksceKernelCpuResumeIntr(intr);
	}
	ThreadmgrSetUserRoWord(saved);
	return result;
}
