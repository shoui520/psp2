/*
 * SceKernelThreadMgr: event_flag.c, the event flags (SceUIDEventFlagClass).
 * The user syscall wrappers are still in event_flag_queries.c and
 * event_flag_lifecycle.c.
 */

#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "event_flag.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>
#include <intrmgr/intrmgr.h>

#define EVF_ITEM_SIZE			0x80	/* the class's object allocation size */
#define EVF_KERNEL_ATTR_MASK		0x0000b080	/* what kernel callers may pass */
#define EVF_USER_ATTR_MASK		0x00003080	/* what a syscall may pass */
#define EVF_OPTION_MAX_SIZE		4

/* whether pattern satisfies a waiter's request under mode */
static inline int EventFlagMatches(uint32_t pattern, uint32_t request, uint32_t mode)
{
	uint32_t hit = pattern & request;

	if (!(mode & EVF_WAITMODE_OR))
		hit = hit == request;
	return hit;
}

/* the holder's last wait ended: the flag is free again */
static inline void EventFlagDropHolder(ThreadmgrEventFlag *evf)
{
	if (evf->core.holder != NULL && --evf->core.holdCount == 0) {
		evf->core.holder = NULL;
		ListLinkRemove(&evf->core.holderLink);
	}
}

/* SceUIDEventFlagClass's release: a waiter leaves (woken or cancelled) */
static int uidEventFlag_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	ThreadmgrEventFlag *evf = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1746);
	SCE_DBG_ASSERT_CHECK(evf->lock != 0, 1749);
	if (thread->waitResult != (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
		EventFlagDropHolder(evf);
	SCE_DBG_ASSERT_CHECK(evf->waiting != 0, 1764);
	evf->waiting--;
	if (thread->waitPatternOutput != NULL)
		*thread->waitPatternOutput = evf->core.pattern;
	return 0;
}

/* SceUIDEventFlagClass's constructor */
static int uidEventFlag_Constructor(void *object)
{
	ThreadmgrEventFlag *evf = object;

	memset(&evf->core, 0, sizeof(evf->core));
	return 0;
}

/* SceUIDEventFlagClass's destructor: the waiters wake with WAIT_DELETE */
static int uidEventFlag_Destructor(void *object)
{
	ThreadmgrEventFlag *evf = object;
	SceProcessObject *process;
	uint32_t wake;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1686);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&evf->lock);
	wake = wakeupWaitObjectAll((uintptr_t)&evf->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	if (evf->core.holder != NULL) {
		sceKernelSpinlockLowLock(THREAD_LOCK(evf->core.holder));
		ListLinkRemove(&evf->core.holderLink);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(evf->core.holder));
	}
	sceKernelSpinlockLowUnlock(&evf->lock);
	pid = evf->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (evf->attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (evf->attr & EVF_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->eventFlagCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	memset(&evf->core, 0, sizeof(evf->core));
	return 0;
}

/*
 * Waits until the flag's pattern matches (AND or OR of the requested bits),
 * then optionally clears it. Without SCE_KERNEL_EVF_ATTR_MULTI only one thread may wait.
 */
int waitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
		  SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved, current;
	uint64_t wakeup;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF(0, 108, "*** error: illegal context. evfId=0x%08x[%s]\n",
				      evfId, SceSysmemForDriver_E655852F(evfId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (mode & ~(EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) {
		SCE_DBG_PRINTF(0, 118, " k: error: waitEventFlag(0x%08X), waitMode=0x%08X\n", evfId,
			       mode);
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	if ((mode & (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) ==
	    (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) {
		SCE_DBG_PRINTF(0, 126, " k: error: waitEventFlag(0x%08X), waitMode=0x%08X\n", evfId,
			       mode);
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	if (pattern == 0) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 140, "*** error: irq suspended. evfId=0x%08x[%s]\n",
				      evfId, SceSysmemForDriver_E655852F(evfId));
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	thread = currentThreadContext();
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_USER) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 165,
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
		goto resume;
	}
	evf = (ThreadmgrEventFlag *)pObject;
	sceKernelSpinlockLowLock(&evf->lock);
	if (!(evf->attr & SCE_KERNEL_EVF_ATTR_MULTI)) {
		if (evf->core.holder == NULL) {
			ListNode *head =
				(ListNode *)&thread->heldEventFlagNext;

			evf->core.holder = thread;
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			evf->core.holderLink.next = head;
			evf->core.holderLink.prev = head->prev;
			head->prev = &evf->core.holderLink;
			evf->core.holderLink.prev->next = &evf->core.holderLink;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		} else if (evf->core.holder != thread) {
			ret = SCE_KERNEL_ERROR_EVF_MULTI;
			goto unlock;
		}
		evf->core.holdCount++;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			if (outPattern != NULL)
				*outPattern = evf->core.pattern;
			sceKernelSpinlockLowUnlock(&evf->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&evf->lock);
		}
		current = evf->core.pattern;
		if (EventFlagMatches(current, pattern, mode)) {
			if (outPattern != NULL)
				*outPattern = current;
			if (mode & EVF_WAITMODE_CLEAR_PAT)
				current &= ~pattern;
			if (mode & EVF_WAITMODE_CLEAR_ALL)
				current = 0;
			evf->core.pattern = current;
			if (evf->core.holder != NULL && --evf->core.holdCount == 0) {
				evf->core.holder = NULL;
				sceKernelSpinlockLowLock(THREAD_LOCK(thread));
				ListLinkRemove(&evf->core.holderLink);
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			}
			ret = 0;
			goto unlock;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = pattern;
		thread->waitEventMode = mode;
		thread->waitPatternOutput = outPattern;
		if (!callbacks) {
			ret = waitWaitableObject(evfId, (uintptr_t)&evf->next, SCE_KERNEL_WAITTYPE_EVENTFLAG,
						 &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(evfId, (uintptr_t)&evf->next, SCE_KERNEL_WAITTYPE_EVENTFLAG, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&evf->lock);
	sceGUIDReleaseObject(evfId);
	if (!callbacks)
		goto resume;
callbacks:
	if (thread->flags & THREAD_FLAG_CALLBACKS) {
		cbRet = callCallbackAll(thread);
		if (cbRet < 0)
			ret = cbRet;
	}
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&evf->lock);
release:
	sceGUIDReleaseObject(evfId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* an event flag's information (SceThreadmgrForDriver_612277C9's core) */
int getEventFlagInfo(SceUID evfId, SceKernelEventFlagInfo *info)
{
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		ret = !ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_EVF_ID :
			SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
	} else {
		evf = (ThreadmgrEventFlag *)pObject;
		info->evfId = evfId;
		strncpy(info->name, SceSysmemForDriver_E655852F(evfId), sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(&evf->lock);
		info->attr = evf->attr;
		info->initPattern = evf->core.initPattern;
		info->currentPattern = evf->core.pattern;
		info->numWaitThreads = evf->waiting;
		sceKernelSpinlockLowUnlock(&evf->lock);
		sceGUIDReleaseObject(evfId);
		ret = 0;
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDEventFlagClass */
void startEventFlag(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(323);
	ret = uidExtendedClassInit(cls, "SceUIDEventFlagClass",
		s_threadmgrRuntime.manager->waitableClass, EVF_ITEM_SIZE,
		uidEventFlag_Constructor, (SceClassCallback)uidEventFlag_Destructor,
		uidDefaultCancelWait, uidDefault_RevalidateMethod,
		uidEventFlag_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1650, 1648), "uidEventFlagClassInit");
	s_threadmgrRuntime.manager->eventFlagClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 343, " sizeof(SceUIDEventFlagObject)=%d\n",
			     sizeof(ThreadmgrEventFlag));
}

/* an exiting thread lets go of the event flags it holds */
void releaseAllEventFlag(struct ThreadCB *thread)
{
	ListNode *head = (ListNode *)&thread->heldEventFlagNext;
	ListNode *link;
	ThreadmgrEventFlag *evf;
	volatile int delay;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 365);
	for (;;) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (link = head->next; link != head; link = head->next) {
			evf = EVENT_FLAG_FROM_HOLDER_LINK(link);
			if (SceCpuForDriver_5AC9D394(&evf->lock) < 0)
				break;
			if (evf->core.holder != NULL) {
				evf->core.holder = NULL;
				evf->core.holdCount = 0;
				ListLinkRemove(link);
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceKernelSpinlockLowUnlock(&evf->lock);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (link == head)
			break;
		/* the flag holds its lock while it takes ours: back off */
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; --delay != 0;)
			;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/* closes every event flag PUID a process still holds */
int eventFlagProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->eventFlagClass, PUID_VECTOR_LEVEL, NULL, 0,
		NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 422);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->eventFlagClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 440, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(SCE_LINE(437, 441));
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 448);
	}
	return 0;
}

/*
 * Sony's failed-create path decrements the process's cond count, not the
 * event flag count it raised; kept.
 */
SceUID sceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				const SceKernelEventFlagOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved;
	SceUID uid;
	int intr;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 490, "sceKernelCreateEventFlag", "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (name == NULL) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~EVF_USER_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 508, "sceKernelCreateEventFlag", " k: error: sceKernelCreateEventFlag(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~EVF_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF_FUNC(0, 519, "sceKernelCreateEventFlag", " k: error: sceKernelCreateEventFlag(%s), attr=0x%08X\n",
			       name, attr);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > EVF_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (attr & SCE_KERNEL_ATTR_OPENABLE) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = UID_ATTR_OPENABLE;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED)
				attr |= EVF_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED) != 0;
		}
	}
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & SCE_KERNEL_ATTR_OPENABLE) {
			if (process->uidOpenableCount >= 0x100) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 582, "sceKernelCreateEventFlag", "sceKernelCreateEventFlag(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidOpenableCount > process->uidOpenablePeak)
				process->uidOpenablePeak = process->uidOpenableCount;
		} else if (attr & EVF_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 565, "sceKernelCreateEventFlag",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->eventFlagCount > process->eventFlagPeak)
			process->eventFlagPeak = process->eventFlagCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->eventFlagClass,
		name, &opt, &pObject);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (attr & EVF_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->condCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		evf = (ThreadmgrEventFlag *)pObject;
		evf->ownerPid = ThreadmgrInSyscall() ? thread->processId : SCE_KERNEL_PROCESS_ID;
		evf->attr = attr;
		evf->core.pattern = initPattern;
		evf->core.initPattern = initPattern;
		evf->core.holder = NULL;
		evf->core.holdCount = 0;
		sceKernelSpinlockLowUnlock(&evf->lock);	/* the class hands it over locked */
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int sceKernelDeleteEventFlag(SceUID evfId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 664, "sceKernelDeleteEventFlag", "*** error: illegal context. evfId=0x%08x[%s]\n",
				      evfId, SceSysmemForDriver_E655852F(evfId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (SceSysmemForKernel_368F1C36(evfId) ==
	    s_threadmgrRuntime.manager->eventFlagClass)
		ret = sceGUIDClose(evfId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			   SceUInt32 *timeout)
{
	return waitEventFlag(evfId, pattern, mode, outPattern, timeout, 0);
}

int SceThreadmgrForDriver_8A35F714(SceUID evfId, uint32_t pattern, uint32_t mode,
				   uint32_t *outPattern, SceUInt32 *timeout)
{
	return waitEventFlag(evfId, pattern, mode, outPattern, timeout, 1);
}

int sceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern)
{
	struct ThreadCB *thread;
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (pattern == 0) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	if (mode & ~(EVF_WAITMODE_OR | EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	if ((mode & (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) ==
	    (EVF_WAITMODE_CLEAR_ALL | EVF_WAITMODE_CLEAR_PAT)) {
		SCE_DBG_PRINTF_FUNC(0, 898, "sceKernelPollEventFlag", " k: error: waitEventFlag(0x%08X), waitMode=0x%08X\n", evfId,
			       mode);
		ret = SCE_KERNEL_ERROR_ILLEGAL_MODE;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 917, "sceKernelPollEventFlag",
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
		goto resume;
	}
	evf = (ThreadmgrEventFlag *)pObject;
	sceKernelSpinlockLowLock(&evf->lock);
	if (!(evf->attr & SCE_KERNEL_EVF_ATTR_MULTI) && evf->core.holder != NULL &&
	    evf->core.holder != thread) {
		ret = SCE_KERNEL_ERROR_EVF_MULTI;
	} else {
		ret = EventFlagMatches(evf->core.pattern, pattern, mode);
		if (outPattern != NULL)
			*outPattern = evf->core.pattern;
		if (!ret) {
			ret = SCE_KERNEL_ERROR_EVF_COND;
		} else {
			if (mode & EVF_WAITMODE_CLEAR_ALL)
				evf->core.pattern = 0;
			ret = 0;
			if (mode & EVF_WAITMODE_CLEAR_PAT)
				evf->core.pattern &= ~pattern;
		}
	}
	sceKernelSpinlockLowUnlock(&evf->lock);
	sceGUIDReleaseObject(evfId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ORs pattern in and wakes, in queue order, every waiter it satisfies */
int sceKernelSetEventFlag(SceUID evfId, uint32_t pattern)
{
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved, wake, request, mode;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1124, "sceKernelSetEventFlag",
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
		goto resume;
	}
	evf = (ThreadmgrEventFlag *)pObject;
	if (pattern == 0) {
		sceGUIDReleaseObject(evfId);
		ret = 0;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)&evf->next;
	sceKernelSpinlockLowLock(&evf->lock);
	evf->core.pattern |= pattern;
	if (queue->next == (struct ThreadmgrWaitNode *)queue || evf->core.pattern == 0) {
		sceKernelSpinlockLowUnlock(&evf->lock);
		sceGUIDReleaseObject(evfId);
		ret = 0;
		goto resume;
	}
	wake = 0;
	node = queue->next;
	do {
		waiter = (struct ThreadCB *)node;
		next = node->next;
		SCE_DBG_ASSERT_PRINTF_FUNC(waiter->magic == THREAD_CB_MAGIC, 1153, "sceKernelSetEventFlag",
			"*** pWaitCB=%p, PEventFlagCB=%p, pThreadCB=%p, pThreadCB->magic=0x%08X, "
			"&gpThreadMgrCB->threadMgrCBLock=%p\n", queue, &evf->core, waiter,
			waiter->magic, &s_threadmgrRuntime.manager->threadMgrCBLock);
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		request = waiter->waitMode;
		if (EventFlagMatches(evf->core.pattern, request, waiter->waitEventMode)) {
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			mode = waiter->waitEventMode;
			if (mode & EVF_WAITMODE_CLEAR_ALL)
				evf->core.pattern = 0;
			if (mode & EVF_WAITMODE_CLEAR_PAT)
				evf->core.pattern &= ~request;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		node = next;
	} while (node != (struct ThreadmgrWaitNode *)queue && evf->core.pattern != 0);
	sceKernelSpinlockLowUnlock(&evf->lock);
	sceGUIDReleaseObject(evfId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ANDs the pattern with mask */
int sceKernelClearEventFlag(SceUID evfId, uint32_t mask)
{
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1226, "sceKernelClearEventFlag",
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
	} else {
		evf = (ThreadmgrEventFlag *)pObject;
		sceKernelSpinlockLowLock(&evf->lock);
		evf->core.pattern &= mask;
		sceKernelSpinlockLowUnlock(&evf->lock);
		sceGUIDReleaseObject(evfId);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sets the pattern and wakes every waiter with WAIT_CANCEL */
int sceKernelCancelEventFlag(SceUID evfId, uint32_t setPattern, int *numWaitThreads)
{
	ThreadmgrEventFlag *evf;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1282,
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
	} else {
		evf = (ThreadmgrEventFlag *)pObject;
		sceKernelSpinlockLowLock(&evf->lock);
		evf->core.pattern = setPattern;
		wake = wakeupWaitObjectAll((uintptr_t)&evf->next, SCE_KERNEL_ERROR_WAIT_CANCEL,
					   (uint32_t *)numWaitThreads);
		sceKernelSpinlockLowUnlock(&evf->lock);
		sceGUIDReleaseObject(evfId);
		ret = 0;
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	}
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_612277C9(SceUID evfId, SceKernelEventFlagInfo *info)
{
	SceKernelEventFlagInfo local;
	SceSize size;
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (info->size > sizeof(local))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	ret = getEventFlagInfo(evfId, &local);
	if (ret >= 0) {
		size = info->size;
		if (size > sizeof(local))
			size = sizeof(local);
		memcpy(info, &local, size);
	}
	return ret;
}

/* SceThreadmgrForDriver_5870C73A: the information of every event flag visible at level */
int SceThreadmgrForDriver_5870C73A(int level, SceKernelEventFlagInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelEventFlagInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->eventFlagClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->eventFlagClass, level, vector, num,
			&got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getEventFlagInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/*
 * SceThreadmgrForDriver_0EB0ABF0: the same for the event flags process pid
 * holds, named by its PUIDs (opened ones marked)
 */
int SceThreadmgrForDriver_0EB0ABF0(SceUID pid, int level, SceKernelEventFlagInfo *infos,
				   SceSize size, SceSize *count)
{
	SceKernelEventFlagInfo info;
	SceSize got = 0;
	uint32_t attributes;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, s_threadmgrRuntime.manager->eventFlagClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid,
			s_threadmgrRuntime.manager->eventFlagClass, level, vector, num,
			&got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getEventFlagInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.evfId = vector[i];
		if (scePUIDGetAttr(pid, info.evfId, &attributes) >= 0 && (attributes & PUID_ATTR_OPENED))
			info.attr |= 0x80000;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}
