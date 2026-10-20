/*
 * SceKernelThreadMgr: mutex.c, the mutexes (SceUIDMutexClass).
 * PLACEHOLDER: the exports are still
 * threadmgr.c stubs.
 */

#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "fast_mutex.h"
#include "priority_change.h"
#include "mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>
#include <intrmgr/intrmgr.h>

#define MUTEX_ITEM_SIZE			0x80	/* the class's object allocation size */
#define USER_TLS_PRIORITY_OFFSET	(-0x7e0)	/* the priority word below the user TLS */
#define SDK_VERSION_BOOST_LIMITS_END	0x01800000	/* from here a boost may cross the user range */

/* SceUIDMutexClass's constructor */
static int uidMutex_Constructor(void *object)
{
	SceUIDMutexObject *mutex = object;

	memset(&mutex->core, 0, sizeof(mutex->core));
	return 0;
}

/* a mutex's information (sceKernelGetMutexInfo's core) */
int getMutexInfo(SceUID mutexId, SceKernelMutexInfo *info)
{
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	int intr, ret;

	*info = (SceKernelMutexInfo){ .size = sizeof(*info) };
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		ret = !ThreadmgrInSyscall() ? 0x80028141 : 0xc0028141;
	} else {
		mutex = (SceUIDMutexObject *)pObject;
		info->mutexId = mutexId;
		strncpy(info->name, SceSysmemForDriver_E655852F(mutexId), sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(&mutex->lock);
		info->attr = mutex->attr;
		info->initCount = mutex->core.initCount;
		info->currentCount = mutex->core.lockCount;
		info->numWaitThreads = mutex->waiting;
		if (mutex->core.owner != NULL)
			info->currentOwnerId = mutex->core.owner->threadId;
		if (mutex->attr & SCE_KERNEL_MUTEX_ATTR_CEILING)
			info->ceilingPriority = mutex->core.ceilingPriority;
		else
			info->ceilingPriority = 0;
		sceKernelSpinlockLowUnlock(&mutex->lock);
		sceGUIDReleaseObject(mutexId);
		ret = 0;
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

SCHED_PROBE(s_acquireMutexChangePriority, "__sched_", "change__pri");

/*
 * Gives the mutex to thread (both locked) and, for a ceiling mutex, boosts
 * the thread to the ceiling; before SDK 0x01800000 a boost may not move a
 * thread into or out of the user priority range.
 */
void acquireMutex(SceUIDMutexObject *mutex, struct ThreadCB *thread, int count)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadCB *current = (struct ThreadCB *)cpu->pRunningThreadCB;
	ListNode *head = (ListNode *)&thread->ownedMutexNext;
	ListNode *link;
	int ceiling, priority, oldPriority;
	const char *threadName;
	SceUInt32 sdkVersion;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 128);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 131);
	mutex->core.lockCount = count;
	mutex->core.owner = thread;
	if (!(mutex->attr & SCE_KERNEL_MUTEX_ATTR_CEILING)) {
		mutex->core.ownerLink.next = head;
		mutex->core.ownerLink.prev = head->prev;
		head->prev = &mutex->core.ownerLink;
		mutex->core.ownerLink.prev->next = &mutex->core.ownerLink;
		return;
	}
	ceiling = mutex->core.ceilingPriority;
	for (link = head->next; link != head; link = link->next)
		if (MUTEX_FROM_OWNER_LINK(link)->core.ceilingPriority > ceiling)
			break;
	mutex->core.ownerLink.next = link;
	mutex->core.ownerLink.prev = link->prev;
	link->prev = &mutex->core.ownerLink;
	priority = thread->boostPriority <= ceiling ? thread->boostPriority : ceiling;
	mutex->core.ownerLink.prev->next = &mutex->core.ownerLink;
	if (ceiling < thread->boostPriority)
		thread->boostPriority = priority;
	oldPriority = thread->currentPriority;
	if (oldPriority <= priority)
		return;
	if (thread->processId == SCE_KERNEL_PROCESS_ID)
		sdkVersion = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(thread->processId, &sdkVersion) < 0)
		sdkVersion = 0;
	if (sdkVersion < SDK_VERSION_BOOST_LIMITS_END && !(mutex->attr & MUTEX_ATTR_KERNEL) &&
	    !s_threadmgrRuntime.manager->testMode) {
		if ((uint32_t)oldPriority - PRIORITY_USER_LOW < PRIORITY_USER_COUNT) {
			if ((uint32_t)priority - PRIORITY_USER_LOW >= PRIORITY_USER_COUNT) {
				threadName = SceSysmemForDriver_E655852F(thread->threadId);
				if (strncmp(threadName, "ScePsp2SwuCui", 31) != 0)
					SCE_PRINTF_WITH_INFO(0, 168, "*** error: illegal priority boost."
						"[mutex ceiling] mutexId=0x%08X[%s], threadId=0x%08X[%s], "
						"%d -> %d\n", mutex->core.mutexId,
						SceSysmemForDriver_E655852F(mutex->core.mutexId), thread->threadId,
						threadName, thread->currentPriority,
						thread->boostPriority);
				priority = PRIORITY_USER_LOW;
				thread->boostPriority = priority;
			}
		} else if ((uint32_t)priority - PRIORITY_USER_LOW < PRIORITY_USER_COUNT) {
			threadName = SceSysmemForDriver_E655852F(thread->threadId);
			if (strncmp(threadName, "ScePsp2SwuCui", 31) != 0)
				SCE_PRINTF_WITH_INFO(0, 179, "*** error: illegal priority boost."
					"[mutex ceiling] mutexId=0x%08X[%s], threadId=0x%08X[%s], "
					"%d -> %d\n", mutex->core.mutexId, SceSysmemForDriver_E655852F(mutex->core.mutexId),
					thread->threadId, threadName, thread->currentPriority,
					thread->boostPriority);
			priority = PRIORITY_USER_LOWEST;
			thread->boostPriority = priority;
		}
	}
	thread->currentPriority = priority;
	if ((thread->currentAttr & THREAD_ATTR_USER) &&
	    sceKernelCopyToUserProc(thread->processId,
				     (void *)(thread->tls + USER_TLS_PRIORITY_OFFSET),
				     &thread->currentPriority, sizeof(thread->currentPriority)) < 0)
		SCE_PANIC(193);
	if (thread == current)
		cpu->priority = thread->boostPriority;
	notifyPriorityChange(thread, oldPriority, s_acquireMutexChangePriority);
}

/* SceUIDMutexClass's revalidation: the first waiter gets the mutex if it is free */
static int uidMutex_RevalidateMethod(void *cls, void *object)
{
	SceUIDMutexObject *mutex = object;
	struct ThreadCB *thread;
	int wake;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2206);
	SCE_DBG_ASSERT_CHECK(mutex->lock != 0, 2209);
	thread = (struct ThreadCB *)mutex->next;
	if ((struct ThreadmgrWaitQueue *)thread == (struct ThreadmgrWaitQueue *)&mutex->next)
		return 0;
	SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2214);
	if (mutex->core.owner != NULL)
		return 0;
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	acquireMutex(mutex, thread, thread->waitMode);
	wake = wakeupAndDistributeThread(thread, (uintptr_t)&mutex->next, 0);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return wake;
}

/* the head of the list of mutexes a thread holds */
#define OWNED_MUTEX_HEAD(thread)	((ListNode *)&(thread)->ownedMutexNext)

/* the priority a thread falls back to: its highest remaining ceiling, else its base */
int findNextCeilingPriority(struct ThreadCB *thread)
{
	SceUIDMutexObject *first;
	ListNode *link;
	SceUInt32 sdkVersion;
	int ceiling, boost;

	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 284);
	SCE_DBG_ASSERT_CHECK_FUNC(thread->lock != 0, 234, "findMutexNextBoostPriority");
	if (thread->processId == SCE_KERNEL_PROCESS_ID)
		sdkVersion = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(thread->processId, &sdkVersion) < 0)
		sdkVersion = 0;
	link = OWNED_MUTEX_HEAD(thread)->next;
	if (link == OWNED_MUTEX_HEAD(thread) || !(MUTEX_FROM_OWNER_LINK(link)->attr & SCE_KERNEL_MUTEX_ATTR_CEILING)) {
		ceiling = PRIORITY_NONE;
	} else {
		first = MUTEX_FROM_OWNER_LINK(link);
		ceiling = first->core.ceilingPriority;
		if (sdkVersion < SDK_VERSION_BOOST_LIMITS_END && !(first->attr & MUTEX_ATTR_KERNEL) &&
		    !s_threadmgrRuntime.manager->testMode) {
			if ((uint32_t)thread->currentPriority - PRIORITY_USER_LOW <
			    PRIORITY_USER_COUNT) {
				if ((uint32_t)ceiling - PRIORITY_USER_LOW >= PRIORITY_USER_COUNT)
					ceiling = PRIORITY_USER_LOW;
			} else if ((uint32_t)ceiling - PRIORITY_USER_LOW < PRIORITY_USER_COUNT) {
				ceiling = PRIORITY_USER_LOWEST;
			}
		}
	}
	boost = findFastMutexNextBoostPriority(thread);
	if (ceiling < boost)
		boost = ceiling;
	thread->boostPriority = boost;
	return thread->basePriority <= boost ? thread->basePriority : boost;
}

/* takes the mutex from its owner and drops the owner's boost */
int releaseMutex(ThreadmgrMutexCore *core)
{
	struct ThreadCB *owner = core->owner;
	int priority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 314);
	core->lockCount = 0;
	core->owner = NULL;
	if (owner == NULL)
		return 0;
	sceKernelSpinlockLowLock(THREAD_LOCK(owner));
	ListLinkRemove(&core->ownerLink);
	priority = findNextCeilingPriority(owner);
	if (owner->currentPriority != priority) {
		sceKernelSpinlockLowUnlock(THREAD_LOCK(owner));
		return changeThreadPriority(owner, priority);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(owner));
	return 0;
}

/*
 * SceUIDMutexClass's destructor: the bound conditions' waiters wake with
 * WAIT_DELETE_MUTEX, the mutex's own with WAIT_DELETE.
 */
static int uidMutex_Destructor(void *object)
{
	SceUIDMutexObject *mutex = object;
	ThreadmgrMutexCore *core = &mutex->core;
	uintptr_t queue = (uintptr_t)&mutex->next;
	ListNode *link, *next;
	SceProcessObject *process;
	SceUIDCondObject *cond;
	volatile int delay;
	uint32_t wake;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2128);
retry:
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&mutex->lock);
	for (link = mutex->core.conds.next; link != &mutex->core.conds; link = next) {
		next = link->next;
		cond = COND_FROM_MUTEX_LINK(link);
		if (SceCpuForDriver_5AC9D394(&cond->lock) < 0) {
			/* the cond holds its lock while it takes ours: back off */
			sceKernelSpinlockLowUnlockCpuResumeIntr(&mutex->lock, intr);
			for (delay = 10; delay != 0; delay--)
				;
			goto retry;
		}
		wakeupWaitObjectAll((uintptr_t)&cond->next, SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX, NULL);
		cond->mutexId = -1;
		sceKernelSpinlockLowUnlock(&cond->lock);
	}
	wake = releaseMutex(core);
	wake |= wakeupWaitObjectAll(queue, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	sceKernelSpinlockLowUnlock(&mutex->lock);
	pid = mutex->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (mutex->attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (mutex->attr & MUTEX_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->mutexCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	memset(core, 0, sizeof(*core));
	return 0;
}

/* SceUIDMutexClass */
void startMutex(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(557);
	ret = uidWaitableClassInit(cls, "SceUIDMutexClass",
		s_threadmgrRuntime.manager->waitableClass, MUTEX_ITEM_SIZE,
		uidMutex_Constructor, (SceClassCallback)uidMutex_Destructor,
		uidDefaultCancelWait, uidMutex_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(2090, 2086), "uidMutexClassInit");
	s_threadmgrRuntime.manager->mutexClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 573, " sizeof(SceUIDMutexObject)=%d\n", sizeof(SceUIDMutexObject));
}

SCHED_PROBE(s_releaseAllMutexChangePriority, "__sched_", "change__pri");

/*
 * An exiting thread gives up every mutex it holds (each goes to its first
 * waiter) and drops back to its base priority.
 */
uint32_t releaseAllMutex(struct ThreadCB *thread)
{
	ListNode *head = (ListNode *)&thread->ownedMutexNext;
	ListNode *link;
	struct ThreadMgrCpuCB *cpu;
	SceUIDMutexObject *mutex;
	volatile int delay;
	uint32_t wake = 0;
	int oldPriority;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 598);
	for (;;) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (link = head->next; link != head; link = head->next) {
			mutex = MUTEX_FROM_OWNER_LINK(link);
			if (SceCpuForDriver_5AC9D394(&mutex->lock) < 0)
				break;
			ListLinkRemove(link);
			mutex->core.lockCount = 0;
			mutex->core.owner = NULL;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			wake |= revalidateWaitState((uintptr_t)&mutex->next);
			sceKernelSpinlockLowUnlock(&mutex->lock);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (link == head)
			break;
		/* the mutex holds its lock while it takes ours: back off */
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; delay != 0; delay--)
			;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	oldPriority = thread->currentPriority;
	thread->boostPriority = PRIORITY_NONE;
	if (oldPriority != thread->basePriority) {
		thread->currentPriority = thread->basePriority;
		if ((thread->currentAttr & THREAD_ATTR_USER) &&
		    sceKernelCopyToUserProc(thread->processId,
					     (void *)(thread->tls + USER_TLS_PRIORITY_OFFSET),
					     &thread->currentPriority,
					     sizeof(thread->currentPriority)) < 0)
			SCE_PANIC(SCE_LINE(642, 645));
		cpu = currentCpuState();
		wake |= 1;
		cpu->priority = thread->currentPriority;
		notifyPriorityChange(thread, oldPriority, s_releaseAllMutexChangePriority);
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return wake;
}

/* closes every mutex PUID a process still holds */
int mutexProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->mutexClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 689);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->mutexClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 707, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(SCE_LINE(705, 708));
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 715);
	}
	return 0;
}

#define MUTEX_KERNEL_ATTR_MASK		0x0000a086	/* what kernel callers may pass */
#define MUTEX_USER_ATTR_MASK		0x00002086	/* what a syscall may pass */
#define MUTEX_OPTION_MAX_SIZE		8
#define CEILING_OFFSET_FLAG		0x10000000	/* the ceiling is an offset from the default */
#define CEILING_OFFSET_BASE		0x10000100
#define CEILING_OFFSET_MIN		(-0x20)
#define CEILING_OFFSET_RANGE		0x40

static inline SceUID OpenByGUID(SceUID pid, SceUID guid)
{
	SceUID puid = scePUIDOpenByGUIDWithFlags(pid, guid, 1);

	SCE_DBG_PRINTF_LEVEL_FUNC(2, 3, 45, "OpenByGUID", "0x%08x=scePUIDOpenByGUID(0x%08x)\n",
				  puid, guid);
	return puid;
}

SceUID sceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount,
			    const SceKernelMutexOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process;
	ProcessBudget *type;
	UIDCreateOpt opt;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uint32_t saved, userRo, uidAttr;
	SceUID typeId, puid, uid;
	int ceiling, intr;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	userRo = ThreadmgrUserRoWord();
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 765, "sceKernelCreateMutex", "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~MUTEX_USER_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 777, "sceKernelCreateMutex", " k: error: sceKernelCreateMutex(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~MUTEX_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF_FUNC(0, 788, "sceKernelCreateMutex", " k: error: sceKernelCreateMutex(%s), attr=0x%08X\n", name,
			       attr);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > MUTEX_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	uidAttr = (attr & SCE_KERNEL_ATTR_OPENABLE) ? UID_ATTR_OPENABLE : 0;
	if ((attr & SCE_KERNEL_MUTEX_ATTR_CEILING) && option == NULL) {
		uid = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
	if (initCount < 0 || (initCount > 1 && !(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	if (userRo & USERRO_IN_SYSCALL) {
		opt.pid = 0;
		opt.heapKind = 0;
		opt.flags = UID_CREATE_OPT_ATTR;
		if (!(attr & SCE_KERNEL_ATTR_OPENABLE)) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED)
				attr |= MUTEX_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED) != 0;
		}
	} else {
		attr |= MUTEX_ATTR_KERNEL;
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.pid = 0;
		opt.heapKind = 0;
	}
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.type = 0;
	opt.attr = uidAttr;
	opt.ownerPid = 0;
	opt.cls = NULL;
	intr = SceCpuForDriver_821FC0EE();
	typeId = -1;
	if (!(attr & SCE_KERNEL_MUTEX_ATTR_CEILING)) {
		ceiling = PRIORITY_NONE;
		goto create;
	}
	ceiling = option->ceilingPriority;
	if (ceiling == 0) {
		ceiling = thread->currentPriority;
	} else if ((uint32_t)ceiling - 1 >= PRIORITY_NONE) {
		/* an offset from the process type's default priority */
		if (!(ceiling & CEILING_OFFSET_FLAG)) {
			uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto resume;
		}
		if ((uint32_t)ceiling - (CEILING_OFFSET_BASE + CEILING_OFFSET_MIN) >=
		    CEILING_OFFSET_RANGE) {
			SCE_DBG_PRINTF_FUNC(0, 851, "sceKernelCreateMutex", "***\n***\n***\n*** error: sceKernelCreateMutex(\"%s\"), "
				"illegal ceiling priority offset(%d)\n*** This will be forbidden by "
				"2011/04/13 !\n***\n***\n***\n", name, ceiling - CEILING_OFFSET_BASE);
			uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto resume;
		}
		uid = sceGUIDGetObject(thread->process->budgetId, &type);
		if (uid < 0)
			goto resume;
		typeId = uid;
		switch (type->type) {
		case PROCESS_TYPE_GAME:
			ceiling = ceiling - CEILING_OFFSET_BASE + 0xa0;
			break;
		case PROCESS_TYPE_MINI_APP:
		case PROCESS_TYPE_SHELL:
		case PROCESS_TYPE_KERNEL:
			ceiling = ceiling - CEILING_OFFSET_BASE + 0x60;
			break;
		default:
			ceiling = ceiling - CEILING_OFFSET_BASE + 0x60;
			SCE_DBG_PRINTF_FUNC(0, 871, "sceKernelCreateMutex", "*** warning: sceKernelCreateMutex(), unknown process "
				"type(0x%08X), [threadId=0x%08X] priority %d\n", type->type,
				thread->threadId, ceiling);
			break;
		}
	}
	if (userRo & USERRO_IN_SYSCALL) {
		if (thread->process->budget->type == PROCESS_TYPE_GAME) {
			if ((uint32_t)ceiling - 0x40 > 0x7f) {
				SCE_DBG_PRINTF_FUNC(0, 883, "sceKernelCreateMutex", "***\n***\n***\n*** error: sceKernelCreateMutex("
					"\"%s\"), illegal ceiling priority for user mode mutex(%d)\n*** "
					"This will be forbidden by 2011/04/13 !\n***\n***\n***\n", name,
					ceiling);
				uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
				goto resume;
			}
		} else if ((uint32_t)ceiling - 0x40 > 0xbb) {
			SCE_DBG_PRINTF_FUNC(0, 893, "sceKernelCreateMutex", "***\n***\n***\n*** error: sceKernelCreateMutex(\"%s\"), "
				"illegal ceiling priority for user mode mutex(%d)\n*** This will be "
				"forbidden by 2011/04/13 !\n***\n***\n***\n", name, ceiling);
			uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
			goto resume;
		}
	} else if ((uint32_t)ceiling - 1 >= PRIORITY_NONE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto resume;
	}
	if (initCount != 0 && ceiling > thread->basePriority) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;
		goto resume;
	}
create:
	process = NULL;
	if (userRo & USERRO_IN_SYSCALL) {
		process = thread->process;
		sceKernelSpinlockLowLock(&process->resourceLock);
		if (attr & SCE_KERNEL_ATTR_OPENABLE) {
			if (process->uidOpenableCount >= 0x100) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 927, "sceKernelCreateMutex", "sceKernelCreateMutex(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidOpenableCount > process->uidOpenablePeak)
				process->uidOpenablePeak = process->uidOpenableCount;
		} else if (attr & MUTEX_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 944, "sceKernelCreateMutex",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->mutexCount > process->mutexPeak)
			process->mutexPeak = process->mutexCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->mutexClass, name,
		&opt, &pObject);
	if (uid < 0) {
		if (userRo & USERRO_IN_SYSCALL)
			goto unaccount;
		goto resume;
	}
	mutex = (SceUIDMutexObject *)pObject;
	mutex->ownerPid = (userRo & USERRO_IN_SYSCALL) ? thread->processId : SCE_KERNEL_PROCESS_ID;
	mutex->core.initCount = initCount;
	mutex->core.lockCount = initCount;
	mutex->attr = attr;
	mutex->core.ceilingPriority = ceiling;
	mutex->core.mutexId = typeId;
	mutex->core.owner = NULL;
	mutex->core.reserved0 = 0;
	mutex->core.conds.next = &mutex->core.conds;
	mutex->core.conds.prev = &mutex->core.conds;
	if (userRo & USERRO_IN_SYSCALL) {
		puid = OpenByGUID(currentThreadContext()->processId, uid);
		SCE_DBG_ASSERT_CHECK_FUNC(puid >= 0, 982, "sceKernelCreateMutex");
		if (puid < 0) {
			sceKernelSpinlockLowUnlock(&mutex->lock);
			uid = sceGUIDClose(uid);
			SCE_DBG_ASSERT_CHECK_FUNC(uid >= 0, 987, "sceKernelCreateMutex");
			uid = puid;
			goto unaccount;
		}
		mutex->core.puid = puid;
		uid = puid;
	}
	if (initCount != 0) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireMutex(mutex, thread, initCount);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	goto resume;
unaccount:
	sceKernelSpinlockLowLock(&process->resourceLock);
	if (attr & SCE_KERNEL_ATTR_OPENABLE)
		process->uidOpenableCount--;
	else if (attr & MUTEX_ATTR_UNLIMITED)
		process->uidUnlimitedCount--;
	else
		process->uidCount--;
	process->mutexCount--;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int sceKernelDeleteMutex(SceUID mutexId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1045, "sceKernelDeleteMutex", "*** error: illegal context. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (SceSysmemForKernel_368F1C36(mutexId) ==
	    s_threadmgrRuntime.manager->mutexClass)
		ret = sceGUIDClose(mutexId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* whether thread may take the mutex now rather than queue (the mutex is locked) */
#define MUTEX_MAY_TAKE(mutex, thread, line) ({						\
	struct ThreadCB *__first;						\
	int __firstPriority, __priority, __take = 0;					\
											\
	if ((mutex)->waiting == 0) {							\
		__take = (mutex)->core.owner == NULL;					\
	} else {									\
		__first = (struct ThreadCB *)(mutex)->next;		\
		SCE_DBG_ASSERT_CHECK(__first->magic == THREAD_CB_MAGIC, line);	\
		if ((mutex)->attr & SCE_KERNEL_MUTEX_ATTR_TH_PRIO) {		\
			sceKernelSpinlockLowLock(THREAD_LOCK(__first));		\
			__firstPriority = __first->currentPriority;			\
			__priority = (thread)->currentPriority;				\
			sceKernelSpinlockLowUnlock(THREAD_LOCK(__first));		\
			if (__priority < __firstPriority)				\
				__take = (mutex)->core.owner == NULL;			\
		}									\
	}										\
	__take;										\
})

/* locks the mutex count times, waiting for it (optionally running callbacks) */
int lockMutex(SceUID mutexId, int count, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uint64_t wakeup;
	uintptr_t queue;
	SceInt32 *flags;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 366, "*** error: illegal context. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 374, "*** error: irq suspended. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
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
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 400,
				"*** error: unknown mutex id. mutexId=0x%08x\n", mutexId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
		}
		goto resume;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	if (count < 1 || (count != 1 && !(mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (mutex->core.owner == thread) {
		if (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) {
			count += mutex->core.lockCount;
			if (count < 0) {
				ret = SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
			} else {
				mutex->core.lockCount = count;
				ret = 0;
			}
		} else if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_MUTEX_RECURSIVE | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_FAIL(424);
			ret = SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
		}
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	queue = (uintptr_t)&mutex->next;
	flags = (SceInt32 *)&thread->flags;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&mutex->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&mutex->lock);
		}
		if (MUTEX_MAY_TAKE(mutex, thread, 473)) {
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			acquireMutex((SceUIDMutexObject *)pObject, thread, count);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
			goto unlock;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		if (!callbacks) {
			ret = waitWaitableObject(mutexId, queue, SCE_KERNEL_WAITTYPE_MUTEX,
						 &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0(flags, THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30(flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(mutexId, queue, SCE_KERNEL_WAITTYPE_MUTEX, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0(flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
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
	sceKernelSpinlockLowUnlock(&mutex->lock);
release:
	sceGUIDReleaseObject(mutexId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_sceKernelLockMutex, SceThreadmgrForDriver_16AC80C5 */
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout)
{
	return lockMutex(mutexId, count, timeout, 0);
}

/* SceThreadmgrForDriver_D06F2886 */
int SceThreadmgrForDriver_D06F2886(SceUID mutexId, int count, SceUInt32 *timeout)
{
	return lockMutex(mutexId, count, timeout, 1);
}

int sceKernelTryLockMutex(SceUID mutexId, int count)
{
	struct ThreadCB *thread;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (SceIntrmgrForDriver_182EE3E3()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1132, "sceKernelTryLockMutex", "*** error: illegal context. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1149, "sceKernelTryLockMutex",
				"*** error: unknown mutex id. mutexId=0x%08x\n", mutexId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
		}
		goto resume;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	if (count < 1 || (count != 1 && !(mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	} else if (mutex->core.owner == thread) {
		if (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) {
			count += mutex->core.lockCount;
			if (count < 0) {
				ret = SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
			} else {
				mutex->core.lockCount = count;
				ret = 0;
			}
		} else {
			ret = SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
		}
	} else {
		ret = SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
		if (mutex->core.owner == NULL) {
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			acquireMutex((SceUIDMutexObject *)pObject, thread, count);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Unlocks count times; at zero the mutex goes to its first waiter. Sony's
 * export and unlockMutexForCond differ only in their checks of the context.
 */
int sceKernelUnlockMutex(SceUID mutexId, int count)
{
	struct ThreadCB *thread, *first;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
#if FW_DEBUG
	const char *name;
#endif
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1234, "sceKernelUnlockMutex", "*** error: illegal context. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1249, "sceKernelUnlockMutex",
				"*** error: unknown mutex id. mutexId=0x%08x\n", mutexId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
		}
		goto resume;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	thread = currentThreadContext();
	if (count < 1 || (count != 1 && !(mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (mutex->core.owner != thread) {
#if FW_DEBUG
		name = SceSysmemForDriver_E655852F(mutexId);
#endif
		if (mutex->core.owner == NULL)
			SCE_DBG_PRINTF_FUNC(0, 1277, "sceKernelUnlockMutex", " k: error: MUTEX_NOT_OWNED, mutexId=0x%08X([%s]), "
				"threadGuid=0x%08X\n", mutexId, name, currentThreadContext()->threadId);
		else
			SCE_DBG_PRINTF_FUNC(0, 1274, "sceKernelUnlockMutex", " k: error: MUTEX_NOT_OWNED, mutexId=0x%08X([%s]), "
				"currentOwner=0x%08X, threadGuid=0x%08X\n", mutexId, name,
				mutex->core.owner->threadId, currentThreadContext()->threadId);
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_MUTEX_NOT_OWNED | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1285, "sceKernelUnlockMutex");
			ret = SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
		}
		goto unlock;
	}
	if (mutex->core.lockCount < count) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1295, "sceKernelUnlockMutex");
			ret = SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;
		}
		goto unlock;
	}
	mutex->core.lockCount -= count;
	ret = 0;
	if (mutex->core.lockCount > 0)
		goto unlock;
	wake = releaseMutex(&mutex->core);
	first = (struct ThreadCB *)mutex->next;
	if ((struct ThreadmgrWaitQueue *)first != (struct ThreadmgrWaitQueue *)&mutex->next) {
		SCE_DBG_ASSERT_CHECK_FUNC(first->magic == THREAD_CB_MAGIC, 1312, "sceKernelUnlockMutex");
		sceKernelSpinlockLowLock(THREAD_LOCK(first));
		acquireMutex((SceUIDMutexObject *)pObject, first, first->waitMode);
		wake |= wakeupAndDistributeThread(first, (uintptr_t)&mutex->next, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
	if (wake & 1)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Wakes every waiter with WAIT_CANCEL and leaves the mutex locked newCount
 * times by the caller (a negative newCount restores initCount).
 */
int sceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads)
{
	struct ThreadCB *thread;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL && newCount != 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1369, "sceKernelCancelMutex", "*** error: illegal context. mutexId=0x%08x[%s]\n",
				      mutexId, SceSysmemForDriver_E655852F(mutexId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	thread = currentThreadContext();
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1388, "sceKernelCancelMutex",
				"*** error: unknown mutex id. mutexId=0x%08x\n", mutexId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
		}
		goto resume;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	if (newCount < 0)
		newCount = mutex->core.initCount;
	if (newCount > 1 && !(mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		sceKernelSpinlockLowUnlock(&mutex->lock);
		sceGUIDReleaseObject(mutexId);
		goto resume;
	}
	wake = releaseMutex(&mutex->core);
	wake |= wakeupWaitObjectAll((uintptr_t)&mutex->next, SCE_KERNEL_ERROR_WAIT_CANCEL,
				    (uint32_t *)numWaitThreads);
	if (newCount > 0) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireMutex((SceUIDMutexObject *)pObject, thread, newCount);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* inside a syscall the owner is reported by its user-visible thread ID */
int sceKernelGetMutexInfo(SceUID mutexId, SceKernelMutexInfo *info)
{
	struct SceUIDThreadObject *owner;
	SceKernelMutexInfo local;
	uint32_t saved;
	SceSize size;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getMutexInfo(mutexId, &local);
		if (ret < 0)
			goto restore;
		if (ThreadmgrInSyscall() && local.currentOwnerId != 0) {
			ret = sceGUIDGetObject(local.currentOwnerId, &owner);
			if (ret < 0) {
				SCE_DBG_PRINTF_FUNC(0, 1541, "sceKernelGetMutexInfo", "*** error: sceKernelGetMutexInfo() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", local.currentOwnerId, ret);
				goto restore;
			}
			local.currentOwnerId = owner->context.userThreadId;
		}
		size = info->size;
		if (size > sizeof(local))
			size = sizeof(local);
		memcpy(info, &local, size);
	}
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_B6DA4669: the information of every mutex visible at level */
int SceThreadmgrForDriver_B6DA4669(int level, SceKernelMutexInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelMutexInfo info;
	SceSize got = 0;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->mutexClass,
					  level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->mutexClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getMutexInfo(vector[i], &info) != 0)
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
end:
	return ret;
}

/* the same for the mutexes a process holds PUIDs of, with user-visible IDs */
int sceKernelGetMutexInfoAllProc(SceUID pid, int level, SceKernelMutexInfo *infos,
				 SceSize size, SceSize *count)
{
	SceSize got = 0;
	SceKernelMutexInfo info;
	SceUInt32 attr;
	struct SceUIDThreadObject *owner;
	SceUID *vector;
	int total, i, ret, err;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = scePUIDGetUIDVectorByClass(pid,
		s_threadmgrRuntime.manager->mutexClass, level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = scePUIDGetUIDVectorByClass(pid,
			s_threadmgrRuntime.manager->mutexClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getMutexInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.mutexId = vector[i];
		if (info.currentOwnerId != 0) {
			err = sceGUIDGetObject(info.currentOwnerId, &owner);
			if (err < 0)
				SCE_DBG_PRINTF(0, 1726, "*** error: sceKernelGetMutexInfoAllProc() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", info.currentOwnerId, err);
			else
				info.currentOwnerId = owner->context.userThreadId;
		}
		if (scePUIDGetAttr(pid, info.mutexId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
			info.attr |= PUID_ATTR_OPENED;
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
end:
	return ret;
}

/*
 * A cond waiter relocking its mutex (interrupts already off): like lockMutex
 * without the context checks; the thread records the cond and the mutex.
 */
int lockMutexForCond(SceUID mutexId, int count, SceUInt32 *timeout, int callbacks, SceUID condId)
{
	struct ThreadCB *thread;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uint64_t wakeup;
	uintptr_t queue;
	SceInt32 *flags;
	int ret, locked = 0;

	thread = currentThreadContext();
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1800);
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall())
			return SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		SCE_DBG_ASSERT_PRINTF(0, 1809, "*** error: unknown mutex id. mutexId=0x%08x\n",
				      mutexId);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	if (count < 1 || (count != 1 && !(mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (mutex->core.owner == thread) {
		if (mutex->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) {
			count += mutex->core.lockCount;
			if (count < 0) {
				ret = SCE_KERNEL_ERROR_MUTEX_LOCK_OVF;
			} else {
				mutex->core.lockCount = count;
				ret = 0;
			}
		} else if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_MUTEX_RECURSIVE | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_FAIL(1833);
			ret = SCE_KERNEL_ERROR_MUTEX_RECURSIVE;
		}
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	queue = (uintptr_t)&mutex->next;
	flags = (SceInt32 *)&thread->flags;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&mutex->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&mutex->lock);
		}
		if (MUTEX_MAY_TAKE(mutex, thread, 1884)) {
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			acquireMutex((SceUIDMutexObject *)pObject, thread, count);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
			goto unlock;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		thread->waitCondId = condId;
		thread->waitMutexId = mutexId;
		if (!callbacks) {
			ret = waitWaitableObject(mutexId, queue,
				SCE_KERNEL_WAITTYPE_COND_MUTEX, &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0(flags, THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				return ret;
			break;
		}
		SceCpuForDriver_BC248C30(flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(mutexId, queue, SCE_KERNEL_WAITTYPE_COND_MUTEX,
					 &wakeup, timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0(flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto deleted;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
deleted:
	/* the mutex went away while the callback variant waited */
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE && callbacks &&
	    (thread->flags & THREAD_FLAG_CALLBACKS)) {
		ret = callCallbackAll(thread);
		if (ret >= 0)
			ret = SCE_KERNEL_ERROR_WAIT_DELETE;
	}
	return ret;
unlock:
	sceKernelSpinlockLowUnlock(&mutex->lock);
release:
	sceGUIDReleaseObject(mutexId);
	return ret;
}

/* a cond waiter giving up its mutex before it sleeps (interrupts already off) */
int unlockMutexForCond(SceUID mutexId, int count)
{
	struct ThreadCB *first;
	SceUIDMutexObject *mutex;
	SceObjectBase *pObject;
	uintptr_t queue;
#if FW_DEBUG
	const char *name;
#endif
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1973);
	ret = sceGUIDReferObjectWithClass(mutexId,
		s_threadmgrRuntime.manager->mutexClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall())
			return SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		SCE_DBG_ASSERT_PRINTF(0, 1982, "*** error: unknown mutex id. mutexId=0x%08x\n",
				      mutexId);
		return SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
	}
	mutex = (SceUIDMutexObject *)pObject;
	sceKernelSpinlockLowLock(&mutex->lock);
	if (currentThreadContext() == mutex->core.owner) {
		if (mutex->core.lockCount < count) {
			if (ThreadmgrInSyscall()) {
				ret = SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF | SYSCALL_ERROR_BIT;
			} else {
				SCE_DBG_ASSERT_FAIL(2021);
				ret = SCE_KERNEL_ERROR_MUTEX_UNLOCK_UDF;
			}
		} else {
			mutex->core.lockCount -= count;
			if (mutex->core.lockCount > 0) {
				ret = 0;
			} else {
				queue = (uintptr_t)&mutex->next;
				releaseMutex(&mutex->core);
				first = (struct ThreadCB *)mutex->next;
				if ((struct ThreadmgrWaitQueue *)first ==
				    (struct ThreadmgrWaitQueue *)&mutex->next) {
					ret = 0;
				} else {
					SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 2038);
					ret = 0;
					sceKernelSpinlockLowLock(THREAD_LOCK(first));
					acquireMutex((SceUIDMutexObject *)pObject, first, first->waitMode);
					wakeupAndDistributeThread(first, queue, 0);
					sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
				}
			}
		}
	} else {
#if FW_DEBUG
		name = SceSysmemForDriver_E655852F(mutexId);
#endif
		if (mutex->core.owner == NULL)
			SCE_DBG_PRINTF(0, 2003, " k: error: MUTEX_NOT_OWNED, mutexId=0x%08X([%s]), "
				"threadGuid=0x%08X\n", mutexId, name, currentThreadContext()->threadId);
		else
			SCE_DBG_PRINTF(0, 2000, " k: error: MUTEX_NOT_OWNED, mutexId=0x%08X([%s]), "
				"currentOwner==0x%08X, threadGuid=0x%08X\n", mutexId, name,
				mutex->core.owner->threadId, currentThreadContext()->threadId);
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_MUTEX_NOT_OWNED | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_FAIL(2011);
			ret = SCE_KERNEL_ERROR_MUTEX_NOT_OWNED;
		}
	}
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
	return ret;
}
