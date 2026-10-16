/*
 * SceKernelThreadMgr (threadmgr.c): waiting for another thread to end or
 * to be suspended. The waiter queues on the thread object (its waitNext
 * list), which exitDeleteThread and the suspension paths wake.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define WAIT_TYPE_THREAD_END		0x00000002
#define WAIT_TYPE_THREAD_SUSPEND	0x00000008	/* PLACEHOLDER: woken when the thread is suspended */
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_DORMANT	0x80028027
#define SCE_KERNEL_ERROR_DELETED	0x8002802a
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
int sceKernelCheckThreadUserStackSelf(void);
int callCallbackAll(struct ThreadCB *thread);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

/* the error for a thread ID the caller cannot name */
static inline int unknownThreadError(void)
{
	if (ThreadmgrInSyscall())
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
}

/* the running thread waits for threadId to end; status receives its exit status */
int waitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, locked;

	locked = 0;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2367, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || self->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2381, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)self->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	ksceKernelSpinlockLowLock(&object->waitLock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (self->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&object->waitLock);
			ret = callCallbackAll(self);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&object->waitLock);
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (object->context.status & THREAD_STATE_ENDED) {
			if (status != NULL)
				*status = object->context.exitStatus;
			ret = 0;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceKernelSpinlockLowLock(THREAD_LOCK(self));
		self->waitArg0 = (uint32_t)status;
		if (!callbacks) {
			ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
						 WAIT_TYPE_THREAD_END, &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&self->flags,
						    THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
					 WAIT_TYPE_THREAD_END, &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		if (!locked)
			goto resume;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&object->waitLock);
release:
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ksceKernelWaitThreadEnd */
int ksceKernelWaitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout)
{
	return waitThreadEnd(threadId, status, timeout, 0);
}

/* ksceKernelWaitThreadEndCB */
int ksceKernelWaitThreadEndCB(SceUID threadId, int *status, SceUInt32 *timeout)
{
	return waitThreadEnd(threadId, status, timeout, 1);
}

/* the running thread waits for threadId to be suspended */
int waitThreadSuspend(SceUID threadId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	uint32_t state;
	uint64_t wakeup;
	int intr, ret, locked;

	locked = 0;
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2545, "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (threadId == 0 || self->threadId == threadId)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2559, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)self->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	ksceKernelSpinlockLowLock(&object->waitLock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (self->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&object->waitLock);
			ret = callCallbackAll(self);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&object->waitLock);
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		state = object->context.status;
		if (state & SCE_KERNEL_THREAD_STATUS_SUSPENDED) {
			ret = 0;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		if (state & SCE_KERNEL_THREAD_STATUS_DORMANT) {
			ret = SCE_KERNEL_ERROR_DORMANT;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
			ret = SCE_KERNEL_ERROR_DELETED;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			break;
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		ksceKernelSpinlockLowLock(THREAD_LOCK(self));
		if (!callbacks) {
			ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
						 WAIT_TYPE_THREAD_SUSPEND, &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&self->flags,
						    THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(threadId, (uintptr_t)&object->waitNext,
					 WAIT_TYPE_THREAD_SUSPEND, &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&self->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		if (!locked)
			goto resume;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&object->waitLock);
release:
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForKernel_392599F1: waits for threadId to be suspended */
int SceThreadmgrForKernel_392599F1(SceUID threadId, SceUInt32 *timeout)
{
	uint32_t saved = ThreadmgrUserRoWord();
	int ret;

	ThreadmgrSetUserRoWord(saved << 16);
	ret = waitThreadSuspend(threadId, timeout, 0);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_614BF2BE: the same, running callbacks */
int SceThreadmgrForKernel_614BF2BE(SceUID threadId, SceUInt32 *timeout)
{
	uint32_t saved = ThreadmgrUserRoWord();
	int ret;

	ThreadmgrSetUserRoWord(saved << 16);
	ret = waitThreadSuspend(threadId, timeout, 1);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}
