/*
 * SceKernelThreadMgr (threadmgr.c): sceKernelDelayThread and its family.
 * The thread sleeps on a delay object until its wakeup time, optionally
 * running its callbacks; a suspend or exit request ends the delay early.
 */

#include <string.h>
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

#define THREAD_ATTR_USER		0x80000000
#define WAIT_TYPE_DELAY			0x00000001
#define DELAY_INTERRUPTIBLE		0x00008000	/* a request ends the delay outside a syscall too */
#define DELAY_MIN_USEC			200
#define DELAY200_MIN_USEC		201
#define SDK_VERSION_DELAY_ZERO_ERROR	0x02100000	/* from 2.10 a delay of 0 is an error */
#define SDK_VERSION_DELAY_JITTER	0x03100000	/* from 3.10 a user delay gets a little jitter */
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c
#define SCE_KERNEL_ERROR_THREAD_SUSPENDED	0x8002802d
#define SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG	0x80020005	/* PLACEHOLDER: name */
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */

typedef void (*ThreadmgrSchedSleepHandler)(const struct ThreadmgrSchedProbeDescriptor *probe);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int sceKernelSysrootBacktrace(SceUID processId, char *buffer, SceSize size,
					SceSize *length, int kind);
extern int sceKernelSysrootPrintBacktrace(SceUID processId, const char *buffer, SceSize length,
					int flags);
extern SceUInt32 ksceKernelGetSystemTimeLow(void);
int sceKernelCheckThreadUserStackSelf(void);
int callCallbackAll(struct ThreadCB *thread);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);

/* the __sched_:sleep probe of a delay */
static uint32_t s_delaySleepProbeId;
volatile uintptr_t s_delaySleepHandler;
extern const char s_delaySleepCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_delaySleepDescriptor = {
	.probeId = &s_delaySleepProbeId,
	.provider = "__sched_",
	.name = "sleep",
	.codeSite = s_delaySleepCodeSite,
	.handler = &s_delaySleepHandler,
};

/* the delay object's timeout: the thread wakes if it still waits */
void delayTimeupHandler(struct ThreadCB *thread)
{
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2259);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (thread->status & SCE_KERNEL_THREAD_STATUS_WAITING)
		wakeupAndDistributeThread(thread, 0, 0);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/*
 * The running thread sleeps usec microseconds (at least DELAY_MIN_USEC);
 * flags DELAY_INTERRUPTIBLE, remaining receives the time left when a
 * request ended the sleep, callbacks runs them while it sleeps.
 */
int delayThread(SceUInt32 usec, uint32_t flags, SceUInt32 *remaining, int callbacks)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;
	ThreadmgrSchedSleepHandler sleep;
	SceUInt32 version, delay, jittered;
	uint64_t wakeup;
	SceSize length;
	char buffer[0x200];
	uint32_t saved;
	int intr, ret, result;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 2037, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (thread->processId == THREADMGR_KERNEL_PID) {
		version = SDK_VERSION_KERNEL_THREAD;
		if (usec == 0)
			goto zero;
	} else if (SceSysrootForDriver_CAE820B2(thread->processId, &version) < 0) {
		version = 0;
	} else if (usec == 0 && version >= SDK_VERSION_DELAY_ZERO_ERROR) {
		goto zero;
	}
	delay = 0;
	if (usec != 0) {
		delay = usec;
		if (usec - 1 < DELAY_MIN_USEC - 1)
			delay = DELAY_MIN_USEC;
		if (version >= SDK_VERSION_DELAY_JITTER && ThreadmgrInSyscall()) {
			jittered = delay + *(uint8_t *)&thread->runClock;
			if (jittered < usec)
				jittered = usec;
			delay = jittered;
		}
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		memset(buffer, 0, sizeof(buffer));
		if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer), &length, 3) >= 0)
			sceKernelSysrootPrintBacktrace(THREADMGR_KERNEL_PID, buffer, length, 0);
		SCE_DBG_PRINTF(0, 2088, " k: error: delayThread(usec 0x%08X) irq suspended\n", delay);
		SCE_DBG_ASSERT_FAIL(2090);
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
	getWakeupTime(&wakeup, delay);
	result = 0;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ret = callCallbackAll(thread);
			if (ret < 0)
				break;
			cpu = currentCpuState();
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if ((thread->flags >> 28) != 0 &&
		    ((!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND) &&
		      (flags & DELAY_INTERRUPTIBLE)) || ThreadmgrInSyscall())) {
			/* a pending request ends the delay */
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
				SCE_PANIC(SCE_LINE(2148, 2150));
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (remaining != NULL)
				*remaining = (uint32_t)wakeup +
					(uint32_t)thread->process->time -
					ksceKernelGetSystemTimeLow();
			break;
		}
		SCE_DBG_ASSERT_CHECK(thread->delayId == -1, 2162);
		SCE_DBG_ASSERT_CHECK(thread->timeoutOutput == 0, 2163);
		ret = uidDelay_AllocObject(&wakeup, (uintptr_t)delayTimeupHandler);
		if (ret < 0) {
			if (ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
				/* already past */
				ret = 0;
				if (remaining != NULL)
					*remaining = 0;
			} else {
				SCE_DBG_ASSERT_FAIL(2172);
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			break;
		}
		thread->delayId = ret;
		thread->waitArg0 = usec;
		thread->remainingTimeLow = (uint32_t)wakeup;
		thread->remainingTimeHigh = (uint32_t)(wakeup >> 32);
		thread->timeoutOutput = (uintptr_t)remaining;
		thread->waitArg1 = 0;
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			/* callbacks came in meanwhile: run them first */
			cancelDelayTimer(ret, NULL);
			thread->waitResult = THREADMGR_ERROR_WAIT_CANCEL;
			thread->delayId = -1;
			thread->timeoutOutput = 0;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		} else {
			thread->waitId = -1;
			thread->waitResult = 0;
			thread->waitType = WAIT_TYPE_DELAY;
			SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 2207);
			thread->currentCpuId = -1;
			if (ThreadmgrInSyscall() || (flags & DELAY_INTERRUPTIBLE))
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
							   THREAD_FLAG_WAIT_MARK);
			if (callbacks)
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
							   THREAD_FLAG_WAKE_PENDING);
			/* the DTrace probe site __sched_:sleep */
			__asm__ volatile(".global s_delaySleepCodeSite\ns_delaySleepCodeSite:");
			thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
			ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->timedWaitLock);
			head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->timedWaitNext;
			self->next = head;
			self->prev = head->prev;
			head->prev = self;
			self->prev->next = self;
			ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->timedWaitLock);
			sleep = (ThreadmgrSchedSleepHandler)s_delaySleepHandler;
			if (sleep != NULL)
				sleep(&s_delaySleepDescriptor);
			SCE_DBG_ASSERT_LEVEL(0, cpu == currentCpuState(), 2227);
			cpu->nextRunningThread = 0;
			switchThread(2);
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_WAKE_PENDING);
			result = thread->waitResult;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		}
		ret = result;
	} while (callbacks && result == (int)THREADMGR_ERROR_WAIT_CANCEL);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
zero:
	if (ThreadmgrInSyscall()) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG | SYSCALL_ERROR_BIT;
	} else {
		SCE_DBG_ASSERT_PRINTF(0, 2055,
			"*** error: delayThread(0), processId=0x%08X[%s], threadId=0x%08X[%s]\n",
			thread->processId, ksceGUIDName(thread->processId),
			thread->threadId, ksceGUIDName(thread->threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT_ARG;
	}
	goto restore;
}

/* sceKernelDelayThread */
int sceKernelDelayThread(SceUInt32 usec)
{
	return delayThread(usec, 0, NULL, 0);
}

/* sceKernelDelayThread200: at least DELAY200_MIN_USEC */
int sceKernelDelayThread200(SceUInt32 usec)
{
	SCE_DBG_PRINTF_LEVEL(2, 3, 2287, "(0x%08x)\n", usec);
	if (usec < DELAY200_MIN_USEC)
		usec = DELAY200_MIN_USEC;
	return delayThread(usec, 0, NULL, 0);
}

/* sceKernelDelayThreadCB */
int sceKernelDelayThreadCB(SceUInt32 usec)
{
	return delayThread(usec, 0, NULL, 1);
}

/* sceKernelDelayThreadCB200 */
int sceKernelDelayThreadCB200(SceUInt32 usec)
{
	SCE_DBG_PRINTF_LEVEL(2, 3, 2310, "(0x%08x)\n", usec);
	if (usec < DELAY200_MIN_USEC)
		usec = DELAY200_MIN_USEC;
	return delayThread(usec, 0, NULL, 1);
}

/* SceThreadmgrForDriver_E9A99C50: an interruptible delay that reports the time left */
int SceThreadmgrForDriver_E9A99C50(SceUInt32 usec, SceUInt32 *remaining)
{
	return delayThread(usec, DELAY_INTERRUPTIBLE, remaining, 0);
}

/* SceThreadmgrForDriver_2B60B793: the same, running callbacks */
int SceThreadmgrForDriver_2B60B793(SceUInt32 usec, SceUInt32 *remaining)
{
	return delayThread(usec, DELAY_INTERRUPTIBLE, remaining, 1);
}
