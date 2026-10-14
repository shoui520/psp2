/*
 * SceKernelThreadMgr: the CPU affinity exports. A syscall gives the mask
 * in bits 16-19; a kernel caller may give either half;
 * AFFINITY_CURRENT_CPU means the calling CPU.
 */

#include "private.h"
#include "affinity.h"
#include "priority_change.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define AFFINITY_MASK			0xf
#define AFFINITY_USER_MASK		0x000f0000
#define AFFINITY_CURRENT_CPU		0x40000000
#define PRIORITY_ABSOLUTE_RANGE		0xfe
#define PRIORITY_RELATIVE		0x10000000
#define PRIORITY_RELATIVE_BASE		0x100000e0
#define PRIORITY_RELATIVE_RANGE		0x40
#define PRIORITY_RELATIVE_OFFSET	0x100000a0
#define PRIORITY_RELATIVE_OFFSET_SYSTEM	0x10000060
#define PRIORITY_RELATIVE_OFFSET_OLD	0x10000100
#define PRIORITY_USER_LOW		0x40
#define PRIORITY_USER_RANGE_SYSTEM	0x80
#define PRIORITY_USER_RANGE		0xbc
#define THREAD_ATTR_UNLIMITED		0x00004000
#define AFFINITY_SYSTEM_CPU		0x8
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)
#define CHANGE_FUNC			"sceKernelChangeThreadPriorityAndCpuAffinityMask"

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

/* the mask as a CPU mask (bits 0-3); a syscall's is in bits 16-19 */
static int normalizeAffinityMask(uint32_t *mask)
{
	if (ThreadmgrInSyscall()) {
		if (*mask == AFFINITY_CURRENT_CPU || (*mask & ~AFFINITY_USER_MASK))
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		*mask = (int32_t)*mask >> 16;
	} else if (*mask != AFFINITY_CURRENT_CPU) {
		if ((*mask & ~(AFFINITY_USER_MASK | AFFINITY_MASK)) ||
		    ((*mask & AFFINITY_USER_MASK) && (*mask & AFFINITY_MASK)))
			return SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK;
		if (*mask & AFFINITY_USER_MASK)
			*mask = (int32_t)*mask >> 16;
	}
	return 0;
}

/* sceKernelChangeThreadPriorityAndCpuAffinityMask: both at once (0: the thread's own) */
int sceKernelChangeThreadPriorityAndCpuAffinityMask(SceUID uid, uint32_t priority,
	uint32_t mask, int *previousPriority, uint32_t *previousMask)
{
	struct ThreadCB *current = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved, type;
	bool hasCurrent;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	hasCurrent = currentThreadContext() != NULL;
	if (uid == 0 || priority == 0) {
		if (!hasCurrent) {
			if (uid != 0)
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6920, CHANGE_FUNC,
							   "*** error: illegal context\n");
			else
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6911, CHANGE_FUNC,
							   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		if (uid == 0)
			uid = current->threadId;
		if (priority == 0)
			priority = current->currentPriority;
	}
	if ((priority & PRIORITY_RELATIVE) &&
	    priority - PRIORITY_RELATIVE_BASE >= PRIORITY_RELATIVE_RANGE) {
		SCE_DBG_PRINTF_FUNC(0, 6930, CHANGE_FUNC, "***\n***\n***\n*** error: "
			"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal priority "
			"offset(%d)\n***\n***\n***\n",
			ksceGUIDName(uid), priority - PRIORITY_RELATIVE_OFFSET_OLD);
		result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto out;
	}
	result = normalizeAffinityMask(&mask);
	if (result < 0)
		goto out;
	intr = ksceKernelCpuSuspendIntr();
	result = ksceGUIDReferObjectWithClass(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	if (mask == AFFINITY_CURRENT_CPU) {
		mask = (1u << (ksceKernelCpuId() & 0xff)) & AFFINITY_MASK;
	} else {
		result = checkCpuAffinityMaskPhy(mask, thread->currentAttr, thread->process);
		if (result < 0) {
			ksceGUIDReleaseObject(uid);
			goto resume;
		}
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (priority - 1 >= PRIORITY_ABSOLUTE_RANGE) {
		/* relative to the process type's default */
		if (!(priority & PRIORITY_RELATIVE)) {
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
		type = thread->process->budget->type;
		if (type == PROCESS_TYPE_GAME) {
			if ((thread->currentAttr & THREAD_ATTR_UNLIMITED) &&
			    (thread->currentCpuAffinityMask & AFFINITY_SYSTEM_CPU))
				priority -= PRIORITY_RELATIVE_OFFSET;
			else
				priority -= PRIORITY_RELATIVE_OFFSET_SYSTEM;
		} else {
			priority -= PRIORITY_RELATIVE_OFFSET;
			if (type != PROCESS_TYPE_MINI_APP && type != PROCESS_TYPE_SHELL &&
			    type != PROCESS_TYPE_KERNEL)
				SCE_DBG_PRINTF_FUNC(0, 7017, CHANGE_FUNC, "*** warning: unknown process "
					"type(0x%08X), [threadId=0x%08X] priority %d\n", type, uid, priority);
		}
	}
	if ((int)thread->currentAttr < 0) {
		/* a user thread's range */
		type = thread->process->budget->type;
		if (priority - PRIORITY_USER_LOW >
		    (type == PROCESS_TYPE_GAME ? PRIORITY_USER_RANGE_SYSTEM - 1 :
						   PRIORITY_USER_RANGE - 1)) {
			if (type == PROCESS_TYPE_GAME)
				SCE_DBG_PRINTF_FUNC(0, 7029, CHANGE_FUNC, "***\n***\n***\n*** error: "
					"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal "
					"priority for user mode thread(%d)\n***\n***\n***\n",
					ksceGUIDName(uid), priority);
			else
				SCE_DBG_PRINTF_FUNC(0, 7035, CHANGE_FUNC, "***\n***\n***\n*** error: "
					"sceKernelChangeThreadPriorityAndCpuAffinityMask(\"%s\"), illegal "
					"priority for user mode thread(%d)\n***\n***\n***\n",
					ksceGUIDName(uid), priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
	}
	result = changeThreadPriorityAndCpuAffinityMask(thread, priority, mask, previousPriority,
							previousMask);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(uid);
	if (result == 1 && hasCurrent) {
		switchThread((intr >> 7) & 1);
		result = 1;
	}
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(uid);
resume:
	ksceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* ksceKernelChangeThreadCpuAffinityMask: uid's mask (0: its own); returns the old one */
int ksceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask)
{
	SceKernelIntrStatus intr;
	uint32_t saved, previousMask;
	bool dispatch;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (uid == 0) {
		if (currentThreadContext() == NULL) {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 5737, "sceKernelChangeThreadCpuAffinityMask",
						   "*** error: illegal context\n");
			result = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto out;
		}
		uid = currentThreadContext() != NULL ? currentThreadContext()->threadId :
						       (SceUID)SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	result = normalizeAffinityMask(&mask);
	if (result < 0)
		goto out;
	intr = ksceKernelCpuSuspendIntr();
	result = changeThreadCpuAffinityMask(uid, mask, &previousMask);
	if (result >= 0) {
		dispatch = result == 1;
		result = previousMask;
		if (dispatch && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	}
	ksceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelChangeThreadCpuAffinityMask (user): the old mask in bits 16-19 */
int sceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask)
{
	int result = 0;

	if (uid != 0)
		result = kscePUIDtoGUID(0, uid);
	if (result < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	result = ksceKernelChangeThreadCpuAffinityMask(result, mask);
	return result < 0 ? result : (int)((uint32_t)result << 16);
}
