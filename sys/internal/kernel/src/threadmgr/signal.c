/*
 * SceKernelThreadMgr (threadmgr.c): thread signals. A thread sleeps in
 * waitSignal on the manager's signal object until another sends it a
 * signal; a signal sent to a thread not waiting stays pending (one deep).
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "priority_change.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define WAIT_TYPE_SIGNAL		0x00000004
#define WAIT_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c
#define SCE_KERNEL_ERROR_THREAD_SUSPENDED	0x8002802d
#define SCE_KERNEL_ERROR_SIGNAL_PENDING	0x80028121	/* PLACEHOLDER: name */
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */

typedef void (*ThreadmgrSchedSleepHandler)(const struct ThreadmgrSchedProbeDescriptor *probe);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
int sceKernelCheckThreadUserStackSelf(void);
int callCallbackAll(struct ThreadCB *thread);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);

/* the __sched_:sleep probe of a signal wait */
static uint32_t s_signalSleepProbeId;
volatile uintptr_t s_signalSleepHandler;
extern const char s_signalSleepCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_signalSleepDescriptor = {
	.probeId = &s_signalSleepProbeId,
	.provider = "__sched_",
	.name = "sleep",
	.codeSite = s_signalSleepCodeSite,
	.handler = &s_signalSleepHandler,
};

/*
 * The running thread waits for a signal (arg0/arg1 are kept for the
 * debugger), optionally with a timeout and running callbacks.
 */
int waitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrWaitableObject *signal = s_threadmgrRuntime.manager->signalObject;
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrSchedSleepHandler sleep;
	uint64_t wakeup;
	uint32_t saved, waiting;
	int intr, ret, again;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2727, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 2735, "*** error: irq suspended\n");
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	cpu = currentCpuState();
	if (cpu->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((int)thread->currentAttr < 0 && sceKernelCheckThreadUserStackSelf() == 0)
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ksceKernelSpinlockLowLock(&signal->lock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&signal->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto resume;
			cpu = currentCpuState();
			ksceKernelSpinlockLowLock(&signal->lock);
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->flags & THREAD_FLAG_SIGNALED) {
			/* the signal came first */
			ret = 0;
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNALED);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto unlock;
		}
		if ((thread->flags >> 28) != 0 &&
		    ((!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND) &&
		      (signal->attributes & WAIT_ATTR_UNINTERRUPTIBLE)) || ThreadmgrInSyscall())) {
			/* a pending request ends the wait */
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_SIGNAL_DEFERRED);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_RESUMED_WAIT);
			switch (thread->flags & THREAD_FLAG_REQUEST_MASK) {
			case THREAD_FLAG_REQUEST_VM_SUSPEND:
			case THREAD_FLAG_REQUEST_SUSPEND:
				ret = SCE_KERNEL_ERROR_THREAD_SUSPENDED;
				thread->waitResult = ret;
				break;
			case THREAD_FLAG_REQUEST_EXIT_DELETE:
			case THREAD_FLAG_REQUEST_EXIT:
				ret = SCE_KERNEL_ERROR_THREAD_STOPPED;
				thread->waitResult = ret;
				break;
			default:
				SCE_PANIC(SCE_LINE(2807, 2811));
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto unlock;
		}
		SCE_DBG_ASSERT_CHECK(thread->delayId == -1, 2821);
		SCE_DBG_ASSERT_CHECK(thread->timeoutOutput == 0, 2822);
		if (timeout == NULL) {
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP)
				goto cancelled;
		} else {
			ret = uidDelay_AllocObject(&wakeup, (uintptr_t)waitTimeoutHandler);
			if (ret < 0) {
				SCE_DBG_ASSERT_CHECK(ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT, 2827);
				thread->waitResult = ret;
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				*timeout = 0;
				goto unlock;
			}
			thread->delayId = ret;
			thread->remainingTimeLow = (uint32_t)wakeup;
			thread->remainingTimeHigh = (uint32_t)(wakeup >> 32);
			thread->timeoutOutput = (uintptr_t)timeout;
			if ((thread->flags & THREAD_FLAG_CALLBACK_WAKEUP) == THREAD_FLAG_CALLBACK_WAKEUP) {
				cancelDelayTimer(ret, NULL);
				thread->delayId = -1;
				thread->timeoutOutput = 0;
				goto cancelled;
			}
		}
		thread->waitType = WAIT_TYPE_SIGNAL;
		thread->waitId = s_threadmgrRuntime.manager->signalId;
		thread->waitResult = 0;
		SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 2861);
		thread->currentCpuId = -1;
		if (ThreadmgrInSyscall())
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAIT_MARK);
		if (callbacks)
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_WAKE_PENDING);
		thread->waitArg0 = arg0;
		thread->waitArg1 = arg1;
		waiting = ++signal->waiting;
		SCE_DBG_ASSERT_CHECK(waiting != 0, 2873);
		/* the DTrace probe site __sched_:sleep */
		__asm__ volatile(".global s_signalSleepCodeSite\ns_signalSleepCodeSite:");
		self->next = (struct ThreadmgrWaitNode *)&signal->next;
		self->prev = (struct ThreadmgrWaitNode *)signal->prev;
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
		signal->prev = (uintptr_t)self;
		self->prev->next = self;
		ksceKernelSpinlockLowUnlock(&signal->lock);
		sleep = (ThreadmgrSchedSleepHandler)s_signalSleepHandler;
		if (sleep != NULL)
			sleep(&s_signalSleepDescriptor);
		SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 2885);
		cpu->nextRunningThread = 0;
		switchThread(2);
		ksceKernelSpinlockLowLock(&signal->lock);
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = thread->waitResult;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		again = ret == (int)THREADMGR_ERROR_WAIT_CANCEL;
		goto next;
cancelled:
		ret = THREADMGR_ERROR_WAIT_CANCEL;
		thread->waitResult = ret;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		again = 1;
next:
		if (!callbacks)
			again = 0;
	} while (again);
	if (ret > 0)
		ret = 0;
unlock:
	ksceKernelSpinlockLowUnlock(&signal->lock);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the kernel's waits for a signal */
int __attribute__((noinline)) sceKernelWaitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return waitSignal(arg0, arg1, timeout, 0);
}

int __attribute__((noinline)) sceKernelWaitSignalCB(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return waitSignal(arg0, arg1, timeout, 1);
}

/* sends threadId a signal: it wakes if it waits for one, else the signal stays pending */
int sendSignal(SceUID threadId)
{
	struct ThreadmgrWaitableObject *signal = s_threadmgrRuntime.manager->signalObject;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret, wake;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(&signal->lock);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if ((thread->status & SCE_KERNEL_THREAD_STATUS_WAITING) &&
	    thread->waitId == s_threadmgrRuntime.manager->signalId) {
		wake = wakeupAndDistributeThread(thread, (uintptr_t)&signal->next, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceKernelSpinlockLowUnlock(&signal->lock);
		ksceGUIDReleaseObject(threadId);
		ret = 0;
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	} else {
		if (!(thread->flags & THREAD_FLAG_SIGNALED)) {
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNALED);
			ret = 0;
		} else {
			ret = SCE_KERNEL_ERROR_SIGNAL_PENDING;
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceKernelSpinlockLowUnlock(&signal->lock);
		ksceGUIDReleaseObject(threadId);
	}
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the cores of _sceKernelWaitSignal and _sceKernelWaitSignalCB */
int sceKernelWaitSignalForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return sceKernelWaitSignal(arg0, arg1, timeout);
}

int sceKernelWaitSignalCBForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return sceKernelWaitSignalCB(arg0, arg1, timeout);
}

/* sceKernelSendSignal (user) */
int sceKernelSendSignal(SceUID threadId)
{
	SceUID guid;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return sendSignal(guid);
}

/* _sceKernelWaitSignal: the timeout comes in and goes back out */
int _sceKernelWaitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSignalForUser(arg0, arg1, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSignalForUser(arg0, arg1, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitSignalCB */
int _sceKernelWaitSignalCB(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSignalCBForUser(arg0, arg1, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSignalCBForUser(arg0, arg1, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}
