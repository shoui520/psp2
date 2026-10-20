/*
 * SceKernelThreadMgr: rw_lock.c, the reader/writer locks (SceUIDRWLockClass).
 * lockCount counts the readers, or the writer's recursion. A writer waiting
 * (writeWaiters) keeps new readers out.
 */

#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>

#define RW_LOCK_ITEM_SIZE		0x80	/* the class's object allocation size */
#define RW_LOCK_ATTR_KERNEL		0x00000040
#define RW_LOCK_ATTR_UNLIMITED		0x00004000
#define RW_LOCK_KERNEL_ATTR_MASK	0x0000a086	/* what kernel callers may pass */
#define RW_LOCK_USER_ATTR_MASK		0x00002086	/* what a syscall may pass */
#define RW_LOCK_OPTION_MAX_SIZE		4
#define RW_LOCK_MODE_READ		0
#define RW_LOCK_MODE_WRITE		1
#define RW_LOCK_COUNT_MAX		0x7fffffff

/* the rw lock state proper (0x20), cleared by the constructor; releaseRWLock works on it */
typedef struct ThreadmgrRWLockCore {
	const void *option;			/* 0x28 */
	int lockCount;				/* 0x2c */
	ListNode ownerLink;	/* 0x30: in the writer's ownedRWLock list */
	struct ThreadCB *writer;	/* 0x38 */
	int reserved;				/* 0x3c: 0xfe at creation, never read */
	int readWaiters;			/* 0x40 */
	int writeWaiters;			/* 0x44 */
} ThreadmgrRWLockCore;

/* SceUIDRWLockObject (0x48) */
typedef struct ThreadmgrRWLock {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	SceClass *cls;
	struct UIDEntry *uidEntry;		/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitQueue *next;	/* 0x10: the waiters (ThreadmgrWaitQueue from here) */
	struct ThreadmgrWaitQueue *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;			/* 0x1c */
	uint32_t attr;				/* 0x20 */
	uint32_t reserved;			/* 0x24: zeroed */
	ThreadmgrRWLockCore core;		/* 0x28 */
} ThreadmgrRWLock;

_Static_assert(sizeof(ThreadmgrRWLock) == 0x48, "SceUIDRWLockObject size");

#define RW_LOCK_FROM_OWNER_LINK(link) \
	((ThreadmgrRWLock *)((uintptr_t)(link) - offsetof(ThreadmgrRWLock, core.ownerLink)))

/* SceUIDRWLockClass's release: a waiter leaves */
static int uidRWLock_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	ThreadmgrRWLock *rwLock = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2057);
	SCE_DBG_ASSERT_CHECK(rwLock->lock != 0, 2060);
	SCE_DBG_ASSERT_CHECK(rwLock->waiting != 0, 2063);
	rwLock->waiting--;
	if (thread->waitMode == RW_LOCK_MODE_WRITE)
		rwLock->core.writeWaiters--;
	else
		rwLock->core.readWaiters--;
	SCE_DBG_ASSERT_CHECK(rwLock->waiting ==
		(uint32_t)(rwLock->core.readWaiters + rwLock->core.writeWaiters), 2072);
	return 0;
}

/* makes thread (locked) the writer */
static void acquireRWLock(ThreadmgrRWLock *rwLock, struct ThreadCB *thread)
{
	ListNode *head = (ListNode *)&thread->ownedRWLockNext;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 111);
	SCE_DBG_ASSERT_CHECK(thread->lock != 0, 114);
	rwLock->core.writer = thread;
	rwLock->core.ownerLink.prev = head->prev;
	rwLock->core.lockCount++;
	rwLock->core.ownerLink.next = head;
	head->prev = &rwLock->core.ownerLink;
	rwLock->core.ownerLink.prev->next = &rwLock->core.ownerLink;
}

/*
 * SceUIDRWLockClass's revalidation: with no writer waiting every waiter
 * becomes a reader; otherwise, once unlocked, the first writer gets it.
 */
static int uidRWLock_RevalidateMethod(void *cls, void *object)
{
	ThreadmgrRWLock *rwLock = object;
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *waiter;
	uint32_t wake = 0;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1969);
	SCE_DBG_ASSERT_CHECK(rwLock->lock != 0, 1972);
	if (rwLock->waiting == 0 || rwLock->core.writer != NULL)
		return 0;
	if (rwLock->core.writeWaiters == 0) {
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 2011);
			next = node->next;
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			SCE_DBG_ASSERT_CHECK(waiter->waitMode == RW_LOCK_MODE_READ, 2016);
			rwLock->core.lockCount++;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		return wake;
	}
	if (rwLock->core.lockCount != 0)
		return 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 2029);
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
			acquireRWLock(rwLock, waiter);
			wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			break;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	SCE_DBG_ASSERT_CHECK(node != (struct ThreadmgrWaitNode *)queue, 2042);
	return wake;
}

/* takes the lock from its writer */
static int releaseRWLock(ThreadmgrRWLockCore *core)
{
	struct ThreadCB *writer = core->writer;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 194);
	core->writer = NULL;
	core->lockCount = 0;
	if (writer != NULL) {
		sceKernelSpinlockLowLock(THREAD_LOCK(writer));
		ListLinkRemove(&core->ownerLink);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(writer));
	}
	return 0;
}

/* SceUIDRWLockClass's constructor */
static int uidRWLock_Constructor(void *object)
{
	ThreadmgrRWLock *rwLock = object;

	memset(&rwLock->core, 0, sizeof(rwLock->core));
	return 0;
}

/* SceUIDRWLockClass's destructor: the waiters wake with WAIT_DELETE */
static int uidRWLock_Destructor(void *object)
{
	ThreadmgrRWLock *rwLock = object;
	SceProcessObject *process;
	uint32_t wake;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1912);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&rwLock->lock);
	wake = releaseRWLock(&rwLock->core);
	wake |= wakeupWaitObjectAll((uintptr_t)&rwLock->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	pid = rwLock->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (rwLock->attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (rwLock->attr & RW_LOCK_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->rwLockCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	memset(&rwLock->core, 0, sizeof(rwLock->core));
	return 0;
}

/* locks for reading (mode 0) or writing (mode 1), waiting for it */
int lockRWLock(SceUID rwLockId, SceUInt32 *timeout, int mode, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 245, "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 253, "*** error: irq suspended. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
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
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 279,
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	sceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == thread) {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
		if (mode != RW_LOCK_MODE_READ) {
			if (rwLock->attr & SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE) {
				if (rwLock->core.lockCount == RW_LOCK_COUNT_MAX) {
					ret = SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;
				} else {
					ret = 0;
					rwLock->core.lockCount++;
				}
			} else if (ThreadmgrInSyscall()) {
				ret = SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE | SYSCALL_ERROR_BIT;
			} else {
				SCE_DBG_ASSERT_FAIL(302);
				ret = SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
			}
		}
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&rwLock->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&rwLock->lock);
		}
		if (mode == RW_LOCK_MODE_READ) {
			if (rwLock->core.writer == NULL &&
			    rwLock->next == (struct ThreadmgrWaitQueue *)&rwLock->next) {
				if (rwLock->core.lockCount == RW_LOCK_COUNT_MAX) {
					ret = SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;
				} else {
					rwLock->core.lockCount++;
					ret = 0;
				}
				goto unlock;
			}
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			thread->waitMode = RW_LOCK_MODE_READ;
			rwLock->core.readWaiters++;
		} else {
			if (mode != RW_LOCK_MODE_WRITE)
				SCE_PANIC(370);
			if (rwLock->core.writer == NULL && rwLock->core.lockCount == 0) {
				sceKernelSpinlockLowLock(THREAD_LOCK(thread));
				acquireRWLock(rwLock, thread);
				sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				ret = 0;
				goto unlock;
			}
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
			thread->waitMode = RW_LOCK_MODE_WRITE;
			rwLock->core.writeWaiters++;
		}
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		SCE_DBG_ASSERT_CHECK(rwLock->waiting + 1 ==
			(uint32_t)(rwLock->core.readWaiters + rwLock->core.writeWaiters), 387);
		ret = waitWaitableObject(rwLockId, (uintptr_t)&rwLock->next, SCE_KERNEL_WAITTYPE_RW_LOCK,
					 &wakeup, timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		SCE_DBG_ASSERT_PRINTF(rwLock->waiting ==
			(uint32_t)(rwLock->core.readWaiters + rwLock->core.writeWaiters), 399,
			"ret=%08x cpu[%d] A :%d W :%d R :%d %p\n", ret, sceKernelCpuId(),
			rwLock->waiting, rwLock->core.writeWaiters, rwLock->core.readWaiters, thread);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!callbacks) {
			if (!locked)
				goto resume;
			break;
		}
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
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
	sceKernelSpinlockLowUnlock(&rwLock->lock);
release:
	sceGUIDReleaseObject(rwLockId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* an rw lock's information (the info exports' core) */
static int getRWLockInfo(SceUID rwLockId, SceKernelRWLockInfo *info)
{
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *waiter;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	int intr, ret;

	*info = (SceKernelRWLockInfo){ .size = sizeof(*info) };
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		ret = !ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID :
			SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
	} else {
		rwLock = (ThreadmgrRWLock *)pObject;
		info->rwLockId = rwLockId;
		strncpy(info->name, SceSysmemForDriver_E655852F(rwLockId), sizeof(info->name) - 1);
		queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
		sceKernelSpinlockLowLock(&rwLock->lock);
		info->attr = rwLock->attr;
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1566);
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitMode == RW_LOCK_MODE_READ)
				info->numReadWaitThreads++;
			else
				info->numWriteWaitThreads++;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		info->lockCount = rwLock->core.lockCount;
		if (rwLock->core.writer != NULL)
			info->writeOwnerId = rwLock->core.writer->threadId;
		sceKernelSpinlockLowUnlock(&rwLock->lock);
		sceGUIDReleaseObject(rwLockId);
		ret = 0;
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDRWLockClass */
void startRWLock(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(448);
	ret = uidExtendedClassInit(cls, "SceUIDRWLockClass",
		s_threadmgrRuntime.manager->waitableClass, RW_LOCK_ITEM_SIZE,
		uidRWLock_Constructor, (SceClassCallback)uidRWLock_Destructor,
		uidDefaultCancelWait, uidRWLock_RevalidateMethod,
		uidRWLock_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1885, 1882), "uidRWLockClassInit");
	s_threadmgrRuntime.manager->rwLockClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 464, " sizeof(SceUIDRWLockObject)=%d\n", sizeof(ThreadmgrRWLock));
}

/* an exiting thread gives up the rw locks it holds for writing */
uint32_t releaseAllRWLock(struct ThreadCB *thread)
{
	ListNode *head = (ListNode *)&thread->ownedRWLockNext;
	ListNode *link;
	ThreadmgrRWLock *rwLock;
	volatile int delay;
	uint32_t wake = 0;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 491);
	for (;;) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (link = head->next; link != head; link = head->next) {
			rwLock = RW_LOCK_FROM_OWNER_LINK(link);
			if (SceCpuForDriver_5AC9D394(&rwLock->lock) < 0)
				break;
			ListLinkRemove(link);
			rwLock->core.writer = NULL;
			rwLock->core.lockCount = 0;
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			wake |= revalidateWaitState((uintptr_t)&rwLock->next);
			sceKernelSpinlockLowUnlock(&rwLock->lock);
			sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (link == head)
			break;
		/* the lock holds its spinlock while it takes ours: back off */
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; --delay != 0;)
			;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return wake;
}

/* closes every rw lock PUID a process still holds */
int rwLockProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->rwLockClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 591);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->rwLockClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 609, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(SCE_LINE(607, 610));
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 617);
	}
	return 0;
}

SceUID sceKernelCreateRWLock(const char *name, SceUInt32 attr,
			     const SceKernelRWLockOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved, uidAttr;
	SceUID uid;
	int intr;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 658, "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (!ThreadmgrInSyscall()) {
		if (attr & ~RW_LOCK_KERNEL_ATTR_MASK) {
			SCE_DBG_PRINTF(0, 681, " k: error: sceKernelCreateRWLock(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~RW_LOCK_USER_ATTR_MASK) {
		SCE_DBG_PRINTF(0, 670, " k: error: sceKernelCreateRWLock(%s), attr=0x%08X\n", name,
			       attr);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > RW_LOCK_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	uidAttr = (attr & SCE_KERNEL_ATTR_OPENABLE) ? UID_ATTR_OPENABLE : 0;
	if (!ThreadmgrInSyscall())
		attr |= RW_LOCK_ATTR_KERNEL;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.pid = 0;
	opt.heapKind = 0;
	opt.type = 0;
	opt.ownerPid = 0;
	opt.cls = NULL;
	opt.flags = UID_CREATE_OPT_ATTR;
	opt.attr = uidAttr;
	if (ThreadmgrInSyscall() && !(attr & SCE_KERNEL_ATTR_OPENABLE)) {
		opt.pid = thread->processId;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED)
			attr |= RW_LOCK_ATTR_UNLIMITED;
		opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED) != 0;
	}
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & SCE_KERNEL_ATTR_OPENABLE) {
			if (process->uidOpenableCount >= 0x100) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF(0, 731, "sceKernelCreateRWLock(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidOpenableCount > process->uidOpenablePeak)
				process->uidOpenablePeak = process->uidOpenableCount;
		} else if (attr & RW_LOCK_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF(0xb, 748,
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->rwLockCount > process->rwLockPeak)
			process->rwLockPeak = process->rwLockCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->rwLockClass, name,
		&opt, &pObject);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & SCE_KERNEL_ATTR_OPENABLE)
				process->uidOpenableCount--;
			else if (attr & RW_LOCK_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->rwLockCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		rwLock = (ThreadmgrRWLock *)pObject;
		rwLock->core.readWaiters = 0;
		rwLock->ownerPid = ThreadmgrInSyscall() ? thread->processId : SCE_KERNEL_PROCESS_ID;
		rwLock->attr = attr;
		rwLock->core.option = option;
		rwLock->core.lockCount = 0;
		rwLock->core.writeWaiters = 0;
		rwLock->core.reserved = 0xfe;
		rwLock->core.ownerLink.next = &rwLock->core.ownerLink;
		rwLock->core.ownerLink.prev = &rwLock->core.ownerLink;
		sceKernelSpinlockLowUnlock(&rwLock->lock);	/* the class hands it over locked */
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int sceKernelDeleteRWLock(SceUID rwLockId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 829, "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (SceSysmemForKernel_368F1C36(rwLockId) ==
	    s_threadmgrRuntime.manager->rwLockClass)
		ret = sceGUIDClose(rwLockId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_3F5E1D56: ksceKernelLockReadRWLock */
int SceThreadmgrForDriver_3F5E1D56(SceUID rwLockId, SceUInt32 *timeout)
{
	return lockRWLock(rwLockId, timeout, RW_LOCK_MODE_READ, 0);
}

/* SceThreadmgrForDriver_CFA2FFAF: ksceKernelLockReadRWLockCB */
int SceThreadmgrForDriver_CFA2FFAF(SceUID rwLockId, SceUInt32 *timeout)
{
	return lockRWLock(rwLockId, timeout, RW_LOCK_MODE_READ, 1);
}

int sceKernelTryLockReadRWLock(SceUID rwLockId)
{
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 916, "sceKernelTryLockReadRWLock",
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	sceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == NULL &&
	    rwLock->next == (struct ThreadmgrWaitQueue *)&rwLock->next) {
		if (rwLock->core.lockCount == RW_LOCK_COUNT_MAX) {
			SCE_DBG_PRINTF_FUNC(0, 942, "sceKernelTryLockReadRWLock", "*** error: sceKernelTryLockReadRWLock(0x%08X)=0x%08X, "
				"[%s]\n", rwLockId, SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF,
				SceSysmemForDriver_E655852F(rwLockId));
			ret = SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;
		} else {
			rwLock->core.lockCount++;
			ret = 0;
		}
	} else {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
	}
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* drops one reader; the last one hands the lock to the first waiting writer */
int sceKernelUnlockReadRWLock(SceUID rwLockId)
{
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 989, "sceKernelUnlockReadRWLock",
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	sceKernelSpinlockLowLock(&rwLock->lock);
	ret = 0;
	if (rwLock->core.writer != NULL) {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;
	} else if (rwLock->core.lockCount < 1) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1012, "sceKernelUnlockReadRWLock");
			ret = SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF;
		}
	} else if (--rwLock->core.lockCount == 0) {
		if (rwLock->core.writeWaiters != 0) {
			wake = 0;
			for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue;
			     node = node->next) {
				waiter = (struct ThreadCB *)node;
				SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 1073, "sceKernelUnlockReadRWLock");
				sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
				if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
					acquireRWLock(rwLock, waiter);
					wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0) & 1;
					sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					break;
				}
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			}
			SCE_DBG_ASSERT_CHECK_FUNC(node != (struct ThreadmgrWaitNode *)queue, 1086, "sceKernelUnlockReadRWLock");
			sceKernelSpinlockLowUnlock(&rwLock->lock);
			sceGUIDReleaseObject(rwLockId);
			if (wake && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			goto resume;
		}
		SCE_DBG_ASSERT_CHECK_FUNC(rwLock->core.readWaiters == 0, 1046, "sceKernelUnlockReadRWLock");
	}
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_A64B00F6: ksceKernelLockWriteRWLock */
int SceThreadmgrForDriver_A64B00F6(SceUID rwLockId, SceUInt32 *timeout)
{
	return lockRWLock(rwLockId, timeout, RW_LOCK_MODE_WRITE, 0);
}

/* SceThreadmgrForDriver_54566860: ksceKernelLockWriteRWLockCB */
int SceThreadmgrForDriver_54566860(SceUID rwLockId, SceUInt32 *timeout)
{
	return lockRWLock(rwLockId, timeout, RW_LOCK_MODE_WRITE, 1);
}

int sceKernelTryLockWriteRWLock(SceUID rwLockId)
{
	struct ThreadCB *thread;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1166, "sceKernelTryLockWriteRWLock", "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1180, "sceKernelTryLockWriteRWLock",
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	sceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == thread) {
		if (rwLock->attr & SCE_KERNEL_RW_LOCK_ATTR_RECURSIVE) {
			if (rwLock->core.lockCount == RW_LOCK_COUNT_MAX) {
				ret = SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;
			} else {
				ret = 0;
				rwLock->core.lockCount++;
			}
		} else {
			ret = SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;
		}
	} else if (rwLock->core.writer == NULL && rwLock->core.lockCount == 0) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireRWLock(rwLock, thread);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
	}
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The writer's last unlock hands the lock on: to every waiter as readers
 * when no writer waits, else to the first waiting writer.
 */
int sceKernelUnlockWriteRWLock(SceUID rwLockId)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1273, "sceKernelUnlockWriteRWLock", "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1287, "sceKernelUnlockWriteRWLock",
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	sceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer != thread) {
		if (rwLock->core.writer == NULL)
			SCE_DBG_PRINTF_FUNC(0, 1307, "sceKernelUnlockWriteRWLock", " k: error: RW_LOCK_NOT_OWNED, rwLockId=0x%08X([%s]), "
				"threadGuid=0x%08X\n", rwLockId, SceSysmemForDriver_E655852F(rwLockId),
				currentThreadContext()->threadId);
		else
			SCE_DBG_PRINTF_FUNC(0, 1304, "sceKernelUnlockWriteRWLock", " k: error: RW_LOCK_NOT_OWNED, rwLockId=0x%08X([%s]), "
				"currentOwner==0x%08X, threadGuid=0x%08X\n", rwLockId,
				SceSysmemForDriver_E655852F(rwLockId), rwLock->core.writer->threadId,
				currentThreadContext()->threadId);
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1315, "sceKernelUnlockWriteRWLock");
			ret = SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED;
		}
		goto unlock;
	}
	ret = 0;
	if (--rwLock->core.lockCount > 0)
		goto unlock;
	wake = releaseRWLock(&rwLock->core);
	if (rwLock->core.writeWaiters == 0) {
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 1373, "sceKernelUnlockWriteRWLock");
			next = node->next;
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->waitMode == RW_LOCK_MODE_READ, 1378, "sceKernelUnlockWriteRWLock");
			rwLock->core.lockCount++;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
	} else {
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 1354, "sceKernelUnlockWriteRWLock");
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
				acquireRWLock(rwLock, waiter);
				wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
				sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				break;
			}
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		SCE_DBG_ASSERT_CHECK_FUNC(node != (struct ThreadmgrWaitNode *)queue, 1367, "sceKernelUnlockWriteRWLock");
	}
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Wakes every waiter with WAIT_CANCEL and counts them by kind; type 1 then
 * leaves the lock to the caller as writer. Sony's early errors return with
 * TPIDRURO still shifted (kept).
 */
int sceKernelCancelRWLock(SceUID rwLockId, int *numReadWaitThreads, int *numWriteWaitThreads,
			  SceUInt32 type)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrRWLock *rwLock;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	int intr, ret, readers, writers;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL && type == 1) {
		SCE_DBG_ASSERT_PRINTF(0, 1442, "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, SceSysmemForDriver_E655852F(rwLockId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (type > 1) {
		SCE_DBG_PRINTF(0, 1450, "*** error: sceKernelCancelRWLock(0x%08X[%s])=0x%08X, "
			"type=0x%08X\n", rwLockId, SceSysmemForDriver_E655852F(rwLockId), SCE_KERNEL_ERROR_INVALID_FLAGS,
			type);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, &pObject);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1463,
				"*** error: unknown rwlock id. rwLockId=0x%08x\n", rwLockId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
		}
		goto resume;
	}
	rwLock = (ThreadmgrRWLock *)pObject;
	writers = 0;
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	sceKernelSpinlockLowLock(&rwLock->lock);
	wake = releaseRWLock(&rwLock->core);
	readers = 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		waiter = (struct ThreadCB *)node;
		next = node->next;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1479);
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitMode == RW_LOCK_MODE_READ)
			readers++;
		else
			writers++;
		wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue,
						  SCE_KERNEL_ERROR_WAIT_CANCEL);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	if (numReadWaitThreads != NULL)
		*numReadWaitThreads = readers;
	if (numWriteWaitThreads != NULL)
		*numWriteWaitThreads = writers;
	if (type == 1) {
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireRWLock(rwLock, thread);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&rwLock->lock);
	sceGUIDReleaseObject(rwLockId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* inside a syscall the writer is reported by its user-visible thread ID */
int sceKernelGetRWLockInfo(SceUID rwLockId, SceKernelRWLockInfo *info)
{
	struct ThreadmgrThreadObject *owner;
	SceKernelRWLockInfo local;
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
		ret = getRWLockInfo(rwLockId, &local);
		if (ret < 0)
			goto restore;
		if (ThreadmgrInSyscall() && local.writeOwnerId != 0) {
			ret = sceGUIDGetObject(local.writeOwnerId, &owner);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 1630, "*** error: sceKernelGetRWLockInfo() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", local.writeOwnerId, ret);
				goto restore;
			}
			local.writeOwnerId = owner->context.userThreadId;
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

/* SceThreadmgrForDriver_E2B57231: the information of every rw lock visible at level */
int SceThreadmgrForDriver_E2B57231(int level, SceKernelRWLockInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelRWLockInfo info;
	SceSize got = 0;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->rwLockClass,
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
			s_threadmgrRuntime.manager->rwLockClass, level, vector, ret, &got);
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
		if (getRWLockInfo(vector[i], &info) != 0)
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

/* the same for the rw locks a process holds PUIDs of, with user-visible IDs */
int sceKernelGetRWLockInfoAllProc(SceUID pid, int level, SceKernelRWLockInfo *infos,
				  SceSize size, SceSize *count)
{
	SceSize got = 0;
	SceKernelRWLockInfo info;
	SceUInt32 attr;
	struct ThreadmgrThreadObject *owner;
	SceUID *vector;
	int total, i, ret, err;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = scePUIDGetUIDVectorByClass(pid,
		s_threadmgrRuntime.manager->rwLockClass, level, NULL, 0, NULL);
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
			s_threadmgrRuntime.manager->rwLockClass, level, vector, ret, &got);
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
		if (getRWLockInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.rwLockId = vector[i];
		if (info.writeOwnerId != 0) {
			err = sceGUIDGetObject(info.writeOwnerId, &owner);
			if (err < 0)
				SCE_DBG_PRINTF(0, 1817, "*** error: sceKernelGetRWLockInfoAllProc() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", info.writeOwnerId, err);
			else
				info.writeOwnerId = owner->context.userThreadId;
		}
		if (scePUIDGetAttr(pid, info.rwLockId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
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
