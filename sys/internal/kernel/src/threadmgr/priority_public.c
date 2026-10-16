/*
 * SceKernelThreadMgr: the priority change exports. A priority 0x100000e0-
 * 0x1000011f is relative to the process type's default; older SDKs may not
 * move a thread between the shared and the per-CPU priorities.
 */

#include "private.h"
#include "priority_change.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define PRIORITY_ABSOLUTE_RANGE		0xfe	/* 1-0xfe */
#define PRIORITY_RELATIVE		0x10000000
#define PRIORITY_RELATIVE_BASE		0x100000e0
#define PRIORITY_RELATIVE_RANGE		0x40
#define PRIORITY_RELATIVE_OFFSET	0x100000a0
#define PRIORITY_RELATIVE_OFFSET_SYSTEM	0x10000060
#define PRIORITY_RELATIVE_OFFSET_OLD	0x10000100
#define PRIORITY_USER_LOW		0x40
#define PRIORITY_USER_RANGE_SYSTEM	0x80
#define PRIORITY_USER_RANGE		0xbc
#define PRIORITY_SHARED_LOW		0x80
#define PRIORITY_SHARED_RANGE		0x7c
#define THREAD_ATTR_UNLIMITED		0x00004000
#define AFFINITY_SYSTEM_CPU		0x8
#define SDK_VERSION_AFFINITY_CHECK	0x01800000
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION	0x80028026
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)

#define CHANGE_PRIORITY_FORMAT \
	"***\n***\n***\n*** error: sceKernelChangeThreadPriority(\"%s\"), illegal priority for user " \
	"mode thread(%d)\n*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n"
#define CHANGE_PRIORITY_FUNC			"sceKernelChangeThreadPriority2"

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

static inline int sharedPriority(uint32_t priority)
{
	return priority - PRIORITY_SHARED_LOW < PRIORITY_SHARED_RANGE;
}

/* ksceKernelChangeThreadPriority: uid's priority (0: its own); returns the old one */
int ksceKernelChangeThreadPriority(SceUID uid, uint32_t priority)
{
	struct ThreadCB *current = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	SceKernelIntrStatus intr;
	uint32_t saved, type, version, previous;
	bool hasCurrent;
	int result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	hasCurrent = currentThreadContext() != NULL;
	if (uid == 0 || priority == 0) {
		if (!hasCurrent) {
			if (uid != 0)
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6210, CHANGE_PRIORITY_FUNC,
							   "*** error: illegal context\n");
			else
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 6201, CHANGE_PRIORITY_FUNC,
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
		SCE_DBG_PRINTF_FUNC(0, 6220, CHANGE_PRIORITY_FUNC, "***\n***\n***\n*** error: "
			"sceKernelChangeThreadPriority(\"%s\"), illegal priority offset(%d)\n"
			"*** This will be forbidden by 2011/04/13 !\n***\n***\n***\n",
			ksceGUIDName(uid), priority - PRIORITY_RELATIVE_OFFSET_OLD);
		result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto out;
	}
	intr = ksceKernelCpuSuspendIntr();
	result = ksceGUIDReferObjectWithClass(uid,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (result < 0) {
		result = ThreadmgrInSyscall() ? (int)USER_ERROR_UNKNOWN_THREAD_ID :
						(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
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
				SCE_DBG_PRINTF_FUNC(0, 6263, CHANGE_PRIORITY_FUNC, "*** warning: unknown process "
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
				SCE_DBG_PRINTF_FUNC(0, 6276, CHANGE_PRIORITY_FUNC, CHANGE_PRIORITY_FORMAT,
						    ksceGUIDName(uid), priority);
			else
				SCE_DBG_PRINTF_FUNC(0, 6283, CHANGE_PRIORITY_FUNC, CHANGE_PRIORITY_FORMAT,
						    ksceGUIDName(uid), priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto unlock;
		}
	}
	if (thread->processId == THREADMGR_KERNEL_PID)
		version = THREADMGR_KERNEL_SYSVER;
	else if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0)
		version = 0;
	if (version < SDK_VERSION_AFFINITY_CHECK && !s_threadmgrRuntime.manager->testMode) {
		/* older SDKs: not between the shared and the per-CPU priorities */
		previous = thread->currentPriority;
		if (sharedPriority(previous) != sharedPriority(priority)) {
			if (sharedPriority(previous))
				SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(6297, 6303), CHANGE_PRIORITY_FUNC,
					"*** error: illegal priority change. threadId=0x%08X[%s], "
					"%d -> %d\n", uid, ksceGUIDName(thread->threadId),
					thread->currentPriority, priority);
			else
				SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(6307, 6313), CHANGE_PRIORITY_FUNC,
					"*** error: illegal priority change. threadId=0x%08X[%s], "
					"%d -> %d\n", uid, ksceGUIDName(thread->threadId),
					thread->currentPriority, priority);
			result = SCE_KERNEL_ERROR_ILLEGAL_THREAD_PARAM_COMBINATION;
			goto unlock;
		}
	}
	previous = thread->currentPriority;
	thread->basePriority = priority;
	if ((int)priority <= thread->boostPriority) {
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		result = changeThreadPriority(thread, priority);
		ksceGUIDReleaseObject(uid);
		if (result == 1 && hasCurrent)
			switchThread((intr >> 7) & 1);
		if (result >= 0)
			result = previous;
		goto resume;
	}
	result = previous;
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(uid);
resume:
	ksceKernelCpuResumeIntr(intr);
out:
	ThreadmgrSetUserRoWord(saved);
	return result;
}

/* sceKernelChangeThreadPriorityForDriver: 0 or an error */
int sceKernelChangeThreadPriorityForDriver(SceUID uid, uint32_t priority)
{
	int result = ksceKernelChangeThreadPriority(uid, priority);

	SCE_DBG_ASSERT_CHECK_FUNC(result != 0, 6367, "sceKernelChangeThreadPriority");
	return result < 0 ? result : 0;
}

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr_for_user.c"

/* sceKernelChangeThreadPriority2 (user): the old priority */
int sceKernelChangeThreadPriority2(SceUID uid, uint32_t priority)
{
	int result = 0;

	if (uid != 0)
		result = kscePUIDtoGUID(0, uid);
	if (result < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	result = ksceKernelChangeThreadPriority(result, priority);
	SCE_DBG_ASSERT_CHECK_FUNC(result != 0, 267, "sceKernelChangeThreadPriority2ForUser");
	return result;
}

/* sceKernelChangeThreadPriority (user): 0 or an error */
int sceKernelChangeThreadPriority(SceUID uid, uint32_t priority)
{
	int result = sceKernelChangeThreadPriority2(uid, priority);

	return result < 0 ? result : 0;
}
