/*
 * SceKernelThreadMgr: event_flag.c, the event flags (SceUIDEventFlagClass).
 * The user syscall wrappers are still in event_flag_queries.c and
 * event_flag_lifecycle.c (agent/match-notes/os0/kd/threadmgr.txt).
 */

#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "event_flag.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "event_flag.c"

#define EVF_ITEM_SIZE			0x80	/* the class's object allocation size */
#define EVF_KERNEL_ATTR_MASK		0x0000b080	/* what kernel callers may pass */
#define EVF_USER_ATTR_MASK		0x00003080	/* what a syscall may pass */
#define EVF_OPTION_MAX_SIZE		4
#define EVF_UID_ATTR_LW			0x0000c000	/* PLACEHOLDER: name */
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define WAIT_TYPE_EVENT_FLAG		0x10
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_ILLEGAL_MODE	0x80020010
#define SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS 0x80028002
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_WAIT_CANCEL	0x80028007
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_UNKNOWN_EVF_ID	0x800280e1
#define SCE_KERNEL_ERROR_EVF_MULTI	0x800280e2
#define SCE_KERNEL_ERROR_EVF_COND	0x800280e3

typedef struct ThreadmgrEventFlagOptParam {
	SceSize size;
} ThreadmgrEventFlagOptParam;

extern const char *ksceGUIDName(SceUID guid);
extern char *strncpy(char *dst, const char *src, SceSize n);
extern void *memset(void *dst, int c, SceSize n);
extern void *memcpy(void *dst, const void *src, SceSize n);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID guid);
extern int ksceGUIDClose(SceUID guid);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int scePUIDGetAttr(SceUID processId, SceUID uid, uint32_t *attributes);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern int kscePUIDClose(SceUID pid, SceUID puid);
int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

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
	if (thread->waitResult != (int)THREADMGR_ERROR_WAIT_CANCEL)
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&evf->lock);
	wake = wakeupWaitObjectAll((uintptr_t)&evf->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	if (evf->core.holder != NULL) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(evf->core.holder));
		ListLinkRemove(&evf->core.holderLink);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(evf->core.holder));
	}
	ksceKernelSpinlockLowUnlock(&evf->lock);
	pid = evf->ownerPid;
	if (ksceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (evf->attr & EVF_ATTR_LW)
				process->uidLwCount--;
			else if (evf->attr & EVF_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->eventFlagCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		ksceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	memset(&evf->core, 0, sizeof(evf->core));
	return 0;
}

/*
 * Waits until the flag's pattern matches (AND or OR of the requested bits),
 * then optionally clears it. Without EVF_ATTR_MULTI only one thread may wait.
 */
int waitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
		  SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrEventFlag *evf;
	uint32_t saved, current;
	uint64_t wakeup;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 108, "*** error: illegal context. evfId=0x%08x[%s]\n",
				      evfId, ksceGUIDName(evfId));
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
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 140, "*** error: irq suspended. evfId=0x%08x[%s]\n",
				      evfId, ksceGUIDName(evfId));
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	thread = currentThreadContext();
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_CHECK_STACK) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
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
	ksceKernelSpinlockLowLock(&evf->lock);
	if (!(evf->attr & EVF_ATTR_MULTI)) {
		if (evf->core.holder == NULL) {
			ListNode *head =
				(ListNode *)&thread->heldEventFlagNext;

			evf->core.holder = thread;
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			evf->core.holderLink.next = head;
			evf->core.holderLink.prev = head->prev;
			head->prev = &evf->core.holderLink;
			evf->core.holderLink.prev->next = &evf->core.holderLink;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
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
			ksceKernelSpinlockLowUnlock(&evf->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&evf->lock);
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
				ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
				ListLinkRemove(&evf->core.holderLink);
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			}
			ret = 0;
			goto unlock;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = pattern;
		thread->waitEventMode = mode;
		thread->waitPatternOutput = outPattern;
		if (!callbacks) {
			ret = waitWaitableObject(evfId, (uintptr_t)&evf->next, WAIT_TYPE_EVENT_FLAG,
						 &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(evfId, (uintptr_t)&evf->next, WAIT_TYPE_EVENT_FLAG, &wakeup,
					 timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&evf->lock);
	ksceGUIDReleaseObject(evfId);
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
	ksceKernelSpinlockLowUnlock(&evf->lock);
release:
	ksceGUIDReleaseObject(evfId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* an event flag's information (ksceKernelGetEventFlagInfo's core) */
int getEventFlagInfo(SceUID evfId, ThreadmgrEventFlagInfo *info)
{
	ThreadmgrEventFlag *evf;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT :
					     SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
	} else {
		info->evfId = evfId;
		strncpy(info->name, ksceGUIDName(evfId), sizeof(info->name) - 1);
		ksceKernelSpinlockLowLock(&evf->lock);
		info->attr = evf->attr;
		info->initPattern = evf->core.initPattern;
		info->currentPattern = evf->core.pattern;
		info->numWaitThreads = evf->waiting;
		ksceKernelSpinlockLowUnlock(&evf->lock);
		ksceGUIDReleaseObject(evfId);
		ret = 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDEventFlagClass */
void startEventFlag(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = ksceKernelAllocHeapMemory(manager->heapId, 0x38);
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
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (link = head->next; link != head; link = head->next) {
			evf = EVENT_FLAG_FROM_HOLDER_LINK(link);
			if (ksceKernelSpinlockLowTrylock(&evf->lock) < 0)
				break;
			if (evf->core.holder != NULL) {
				evf->core.holder = NULL;
				evf->core.holdCount = 0;
				ListLinkRemove(link);
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceKernelSpinlockLowUnlock(&evf->lock);
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (link == head)
			break;
		/* the flag holds its lock while it takes ours: back off */
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; --delay != 0;)
			;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
}

/* closes every event flag PUID a process still holds */
int eventFlagProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid,
		s_threadmgrRuntime.manager->eventFlagClass, PUID_VECTOR_LEVEL, NULL, 0,
		NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 422);
		num = SceSysmemForKernel_08C05493(process->pid,
			s_threadmgrRuntime.manager->eventFlagClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = ksceKernelCpuSuspendIntr();
		ret = kscePUIDClose(process->pid, vector[i]);
		ksceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 440, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(SCE_LINE(437, 441));
		}
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 448);
	}
	return 0;
}

/*
 * Sony's failed-create path decrements the process's cond count, not the
 * event flag count it raised; kept.
 */
SceUID ksceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				 const ThreadmgrEventFlagOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrEventFlag *evf;
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
	if (attr & EVF_ATTR_LW) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = EVF_UID_ATTR_LW;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
				attr |= EVF_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		}
	}
	if (!ThreadmgrInSyscall()) {
		intr = ksceKernelCpuSuspendIntr();
	} else {
		process = thread->process;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & EVF_ATTR_LW) {
			if (process->uidLwCount >= 0x100) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 582, "sceKernelCreateEventFlag", "sceKernelCreateEventFlag(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & EVF_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
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
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->eventFlagClass,
		name, (SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)&evf);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & EVF_ATTR_LW)
				process->uidLwCount--;
			else if (attr & EVF_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->condCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		evf->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
		evf->attr = attr;
		evf->core.pattern = initPattern;
		evf->core.initPattern = initPattern;
		evf->core.holder = NULL;
		evf->core.holdCount = 0;
		ksceKernelSpinlockLowUnlock(&evf->lock);	/* the class hands it over locked */
	}
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int ksceKernelDeleteEventFlag(SceUID evfId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 664, "sceKernelDeleteEventFlag", "*** error: illegal context. evfId=0x%08x[%s]\n",
				      evfId, ksceGUIDName(evfId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (SceSysmemForKernel_368F1C36(evfId) ==
	    s_threadmgrRuntime.manager->eventFlagClass)
		ret = ksceGUIDClose(evfId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			    SceUInt32 *timeout)
{
	return waitEventFlag(evfId, pattern, mode, outPattern, timeout, 0);
}

int ksceKernelWaitEventFlagCB(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			      SceUInt32 *timeout)
{
	return waitEventFlag(evfId, pattern, mode, outPattern, timeout, 1);
}

int ksceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern)
{
	struct ThreadCB *thread;
	ThreadmgrEventFlag *evf;
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
	intr = ksceKernelCpuSuspendIntr();
	thread = currentThreadContext();
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
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
	ksceKernelSpinlockLowLock(&evf->lock);
	if (!(evf->attr & EVF_ATTR_MULTI) && evf->core.holder != NULL && evf->core.holder != thread) {
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
	ksceKernelSpinlockLowUnlock(&evf->lock);
	ksceGUIDReleaseObject(evfId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ORs pattern in and wakes, in queue order, every waiter it satisfies */
int ksceKernelSetEventFlag(SceUID evfId, uint32_t pattern)
{
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrEventFlag *evf;
	uint32_t saved, wake, request, mode;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
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
	if (pattern == 0) {
		ksceGUIDReleaseObject(evfId);
		ret = 0;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)&evf->next;
	ksceKernelSpinlockLowLock(&evf->lock);
	evf->core.pattern |= pattern;
	if (queue->next == (struct ThreadmgrWaitNode *)queue || evf->core.pattern == 0) {
		ksceKernelSpinlockLowUnlock(&evf->lock);
		ksceGUIDReleaseObject(evfId);
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
		ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		request = waiter->waitMode;
		if (EventFlagMatches(evf->core.pattern, request, waiter->waitEventMode)) {
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			mode = waiter->waitEventMode;
			if (mode & EVF_WAITMODE_CLEAR_ALL)
				evf->core.pattern = 0;
			if (mode & EVF_WAITMODE_CLEAR_PAT)
				evf->core.pattern &= ~request;
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		node = next;
	} while (node != (struct ThreadmgrWaitNode *)queue && evf->core.pattern != 0);
	ksceKernelSpinlockLowUnlock(&evf->lock);
	ksceGUIDReleaseObject(evfId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ANDs the pattern with mask */
int ksceKernelClearEventFlag(SceUID evfId, uint32_t mask)
{
	ThreadmgrEventFlag *evf;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1226, "sceKernelClearEventFlag",
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
	} else {
		ksceKernelSpinlockLowLock(&evf->lock);
		evf->core.pattern &= mask;
		ksceKernelSpinlockLowUnlock(&evf->lock);
		ksceGUIDReleaseObject(evfId);
	}
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sets the pattern and wakes every waiter with WAIT_CANCEL */
int sceKernelCancelEventFlag(SceUID evfId, uint32_t setPattern, int *numWaitThreads)
{
	ThreadmgrEventFlag *evf;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(evfId,
		s_threadmgrRuntime.manager->eventFlagClass, (SceObjectBase **)&evf);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1282,
				"*** error: unknown eventflag id. evfId=0x%08x\n", evfId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
		}
	} else {
		ksceKernelSpinlockLowLock(&evf->lock);
		evf->core.pattern = setPattern;
		wake = wakeupWaitObjectAll((uintptr_t)&evf->next, SCE_KERNEL_ERROR_WAIT_CANCEL,
					   (uint32_t *)numWaitThreads);
		ksceKernelSpinlockLowUnlock(&evf->lock);
		ksceGUIDReleaseObject(evfId);
		ret = 0;
		if ((wake & 1) && currentThreadContext() != NULL)
			switchThread((intr >> 7) & 1);
	}
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelGetEventFlagInfo(SceUID evfId, ThreadmgrEventFlagInfo *info)
{
	ThreadmgrEventFlagInfo local;
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
int SceThreadmgrForDriver_5870C73A(int level, ThreadmgrEventFlagInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrEventFlagInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->eventFlagClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->eventFlagClass, level, vector, num,
			&got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/*
 * SceThreadmgrForDriver_0EB0ABF0: the same for the event flags process pid
 * holds, named by its PUIDs (opened ones marked)
 */
int SceThreadmgrForDriver_0EB0ABF0(SceUID pid, int level, ThreadmgrEventFlagInfo *infos,
				   SceSize size, SceSize *count)
{
	ThreadmgrEventFlagInfo info;
	SceSize got = 0;
	uint32_t attributes;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid, s_threadmgrRuntime.manager->eventFlagClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid,
			s_threadmgrRuntime.manager->eventFlagClass, level, vector, num,
			&got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getEventFlagInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}
