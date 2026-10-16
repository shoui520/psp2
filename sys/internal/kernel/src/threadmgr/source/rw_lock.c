/*
 * SceKernelThreadMgr: rw_lock.c, the reader/writer locks (SceUIDRWLockClass).
 * lockCount counts the readers, or the writer's recursion. A writer waiting
 * (writeWaiters) keeps new readers out.
 */

#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "mutex.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "rw_lock.c"

#define RW_LOCK_ITEM_SIZE		0x80	/* the class's object allocation size */
#define RW_LOCK_ATTR_RECURSIVE		0x00000002
#define RW_LOCK_ATTR_KERNEL		0x00000040
#define RW_LOCK_ATTR_LW			0x00000080
#define RW_LOCK_ATTR_UNLIMITED		0x00004000
#define RW_LOCK_KERNEL_ATTR_MASK	0x0000a086	/* what kernel callers may pass */
#define RW_LOCK_USER_ATTR_MASK		0x00002086	/* what a syscall may pass */
#define RW_LOCK_OPTION_MAX_SIZE		4
#define RW_LOCK_UID_ATTR_LW		0x0000c000	/* PLACEHOLDER: name */
#define RW_LOCK_MODE_READ		0
#define RW_LOCK_MODE_WRITE		1
#define RW_LOCK_COUNT_MAX		0x7fffffff
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define WAIT_TYPE_RW_LOCK		0x80
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_TYPE	0x8002000a
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS 0x80028002
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_WAIT_CANCEL	0x80028007
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID 0x800281e1
#define SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE 0x800281e2
#define SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF 0x800281e3
#define SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED 0x800281e4
#define SCE_KERNEL_ERROR_RW_LOCK_UNLOCK_UDF 0x800281e5
#define SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK 0x800281e6
#define SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK 0x800281e7

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
	void *uidEntry;				/* 0x08: the GUID entry */
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

typedef struct ThreadmgrRWLockOptParam {
	SceSize size;
} ThreadmgrRWLockOptParam;

/* sceKernelGetRWLockInfo (0x3c) */
typedef struct ThreadmgrRWLockInfo {
	SceSize size;
	SceUID rwLockId;
	char name[32];
	uint32_t attr;				/* 0x28 */
	int lockCount;
	SceUID writeOwnerId;			/* 0x30 */
	int numReadWaitThreads;
	int numWriteWaitThreads;		/* 0x38 */
} ThreadmgrRWLockInfo;

extern const char *ksceGUIDName(SceUID guid);
extern char *strncpy(char *dst, const char *src, SceSize n);
extern void *memset(void *dst, int c, SceSize n);
extern void *memcpy(void *dst, const void *src, SceSize n);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID guid);
extern int ksceGUIDClose(SceUID guid);
extern int ksceGUIDGetObject(SceUID guid, void *object);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int kscePUIDClose(SceUID pid, SceUID puid);
extern int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr);
extern int ksceKernelCpuId(void);
int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

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
			ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			SCE_DBG_ASSERT_CHECK(waiter->waitMode == RW_LOCK_MODE_READ, 2016);
			rwLock->core.lockCount++;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		return wake;
	}
	if (rwLock->core.lockCount != 0)
		return 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 2029);
		ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
			acquireRWLock(rwLock, waiter);
			wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			break;
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
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
		ksceKernelSpinlockLowLock(THREAD_LOCK(writer));
		ListLinkRemove(&core->ownerLink);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(writer));
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&rwLock->lock);
	wake = releaseRWLock(&rwLock->core);
	wake |= wakeupWaitObjectAll((uintptr_t)&rwLock->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	pid = rwLock->ownerPid;
	if (ksceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (rwLock->attr & RW_LOCK_ATTR_LW)
				process->uidLwCount--;
			else if (rwLock->attr & RW_LOCK_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->rwLockCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		ksceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	memset(&rwLock->core, 0, sizeof(rwLock->core));
	return 0;
}

/* locks for reading (mode 0) or writing (mode 1), waiting for it */
int lockRWLock(SceUID rwLockId, SceUInt32 *timeout, int mode, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrRWLock *rwLock;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 245, "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, ksceGUIDName(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 253, "*** error: irq suspended. rwLockId=0x%08x[%s]\n",
				      rwLockId, ksceGUIDName(rwLockId));
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
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	ksceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == thread) {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
		if (mode != RW_LOCK_MODE_READ) {
			if (rwLock->attr & RW_LOCK_ATTR_RECURSIVE) {
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
			ksceKernelSpinlockLowUnlock(&rwLock->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&rwLock->lock);
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
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			thread->waitMode = RW_LOCK_MODE_READ;
			rwLock->core.readWaiters++;
		} else {
			if (mode != RW_LOCK_MODE_WRITE)
				SCE_PANIC(370);
			if (rwLock->core.writer == NULL && rwLock->core.lockCount == 0) {
				ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
				acquireRWLock(rwLock, thread);
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				ret = 0;
				goto unlock;
			}
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
			thread->waitMode = RW_LOCK_MODE_WRITE;
			rwLock->core.writeWaiters++;
		}
		if (callbacks)
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		SCE_DBG_ASSERT_CHECK(rwLock->waiting + 1 ==
			(uint32_t)(rwLock->core.readWaiters + rwLock->core.writeWaiters), 387);
		ret = waitWaitableObject(rwLockId, (uintptr_t)&rwLock->next, WAIT_TYPE_RW_LOCK,
					 &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		SCE_DBG_ASSERT_PRINTF(rwLock->waiting ==
			(uint32_t)(rwLock->core.readWaiters + rwLock->core.writeWaiters), 399,
			"ret=%08x cpu[%d] A :%d W :%d R :%d %p\n", ret, ksceKernelCpuId(),
			rwLock->waiting, rwLock->core.writeWaiters, rwLock->core.readWaiters, thread);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!callbacks) {
			if (!locked)
				goto resume;
			break;
		}
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
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
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
release:
	ksceGUIDReleaseObject(rwLockId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* an rw lock's information (the info exports' core) */
static int getRWLockInfo(SceUID rwLockId, ThreadmgrRWLockInfo *info)
{
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *waiter;
	ThreadmgrRWLock *rwLock;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID | SYSCALL_ERROR_BIT :
					     SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
	} else {
		info->rwLockId = rwLockId;
		strncpy(info->name, ksceGUIDName(rwLockId), sizeof(info->name) - 1);
		queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
		ksceKernelSpinlockLowLock(&rwLock->lock);
		info->attr = rwLock->attr;
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1566);
			ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitMode == RW_LOCK_MODE_READ)
				info->numReadWaitThreads++;
			else
				info->numWriteWaitThreads++;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		info->lockCount = rwLock->core.lockCount;
		if (rwLock->core.writer != NULL)
			info->writeOwnerId = rwLock->core.writer->threadId;
		ksceKernelSpinlockLowUnlock(&rwLock->lock);
		ksceGUIDReleaseObject(rwLockId);
		ret = 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDRWLockClass */
void startRWLock(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = ksceKernelAllocHeapMemory(manager->heapId, 0x38);
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
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		for (link = head->next; link != head; link = head->next) {
			rwLock = RW_LOCK_FROM_OWNER_LINK(link);
			if (ksceKernelSpinlockLowTrylock(&rwLock->lock) < 0)
				break;
			ListLinkRemove(link);
			rwLock->core.writer = NULL;
			rwLock->core.lockCount = 0;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			wake |= revalidateWaitState((uintptr_t)&rwLock->next);
			ksceKernelSpinlockLowUnlock(&rwLock->lock);
			ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		}
		if (link == head)
			break;
		/* the lock holds its spinlock while it takes ours: back off */
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		for (delay = 10; --delay != 0;)
			;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return wake;
}

/* closes every rw lock PUID a process still holds */
int rwLockProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid,
		s_threadmgrRuntime.manager->rwLockClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 591);
		num = SceSysmemForKernel_08C05493(process->pid,
			s_threadmgrRuntime.manager->rwLockClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = ksceKernelCpuSuspendIntr();
		ret = kscePUIDClose(process->pid, vector[i]);
		ksceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 609, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(SCE_LINE(607, 610));
		}
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 617);
	}
	return 0;
}

SceUID sceKernelCreateRWLock(const char *name, SceUInt32 attr,
			     const ThreadmgrRWLockOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrRWLock *rwLock;
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
	uidAttr = (attr & RW_LOCK_ATTR_LW) ? RW_LOCK_UID_ATTR_LW : 0;
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
	if (ThreadmgrInSyscall() && !(attr & RW_LOCK_ATTR_LW)) {
		opt.pid = thread->processId;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
			attr |= RW_LOCK_ATTR_UNLIMITED;
		opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
	}
	if (!ThreadmgrInSyscall()) {
		intr = ksceKernelCpuSuspendIntr();
	} else {
		process = thread->process;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & RW_LOCK_ATTR_LW) {
			if (process->uidLwCount >= 0x100) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF(0, 731, "sceKernelCreateRWLock(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & RW_LOCK_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
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
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->rwLockClass, name,
		(SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)&rwLock);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & RW_LOCK_ATTR_LW)
				process->uidLwCount--;
			else if (attr & RW_LOCK_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->rwLockCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		rwLock->core.readWaiters = 0;
		rwLock->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
		rwLock->attr = attr;
		rwLock->core.option = option;
		rwLock->core.lockCount = 0;
		rwLock->core.writeWaiters = 0;
		rwLock->core.reserved = 0xfe;
		rwLock->core.ownerLink.next = &rwLock->core.ownerLink;
		rwLock->core.ownerLink.prev = &rwLock->core.ownerLink;
		ksceKernelSpinlockLowUnlock(&rwLock->lock);	/* the class hands it over locked */
	}
resume:
	ksceKernelCpuResumeIntr(intr);
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
				      rwLockId, ksceGUIDName(rwLockId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (SceSysmemForKernel_368F1C36(rwLockId) ==
	    s_threadmgrRuntime.manager->rwLockClass)
		ret = ksceGUIDClose(rwLockId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	ksceKernelCpuResumeIntr(intr);
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

int ksceKernelTryLockReadRWLock(SceUID rwLockId)
{
	ThreadmgrRWLock *rwLock;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	ksceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == NULL &&
	    rwLock->next == (struct ThreadmgrWaitQueue *)&rwLock->next) {
		if (rwLock->core.lockCount == RW_LOCK_COUNT_MAX) {
			SCE_DBG_PRINTF_FUNC(0, 942, "sceKernelTryLockReadRWLock", "*** error: sceKernelTryLockReadRWLock(0x%08X)=0x%08X, "
				"[%s]\n", rwLockId, SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF,
				ksceGUIDName(rwLockId));
			ret = SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;
		} else {
			rwLock->core.lockCount++;
			ret = 0;
		}
	} else {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
	}
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* drops one reader; the last one hands the lock to the first waiting writer */
int ksceKernelUnlockReadRWLock(SceUID rwLockId)
{
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrRWLock *rwLock;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	ksceKernelSpinlockLowLock(&rwLock->lock);
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
				ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
				if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
					acquireRWLock(rwLock, waiter);
					wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0) & 1;
					ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
					break;
				}
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			}
			SCE_DBG_ASSERT_CHECK_FUNC(node != (struct ThreadmgrWaitNode *)queue, 1086, "sceKernelUnlockReadRWLock");
			ksceKernelSpinlockLowUnlock(&rwLock->lock);
			ksceGUIDReleaseObject(rwLockId);
			if (wake && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			goto resume;
		}
		SCE_DBG_ASSERT_CHECK_FUNC(rwLock->core.readWaiters == 0, 1046, "sceKernelUnlockReadRWLock");
	}
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
resume:
	ksceKernelCpuResumeIntr(intr);
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

int ksceKernelTryLockWriteRWLock(SceUID rwLockId)
{
	struct ThreadCB *thread;
	ThreadmgrRWLock *rwLock;
	uint32_t saved;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1166, "sceKernelTryLockWriteRWLock", "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, ksceGUIDName(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	ksceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer == thread) {
		if (rwLock->attr & RW_LOCK_ATTR_RECURSIVE) {
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
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireRWLock(rwLock, thread);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;
	}
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The writer's last unlock hands the lock on: to every waiter as readers
 * when no writer waits, else to the first waiting writer.
 */
int ksceKernelUnlockWriteRWLock(SceUID rwLockId)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrRWLock *rwLock;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1273, "sceKernelUnlockWriteRWLock", "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, ksceGUIDName(rwLockId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	ksceKernelSpinlockLowLock(&rwLock->lock);
	if (rwLock->core.writer != thread) {
		if (rwLock->core.writer == NULL)
			SCE_DBG_PRINTF_FUNC(0, 1307, "sceKernelUnlockWriteRWLock", " k: error: RW_LOCK_NOT_OWNED, rwLockId=0x%08X([%s]), "
				"threadGuid=0x%08X\n", rwLockId, ksceGUIDName(rwLockId),
				currentThreadContext()->threadId);
		else
			SCE_DBG_PRINTF_FUNC(0, 1304, "sceKernelUnlockWriteRWLock", " k: error: RW_LOCK_NOT_OWNED, rwLockId=0x%08X([%s]), "
				"currentOwner==0x%08X, threadGuid=0x%08X\n", rwLockId,
				ksceGUIDName(rwLockId), rwLock->core.writer->threadId,
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
			ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->waitMode == RW_LOCK_MODE_READ, 1378, "sceKernelUnlockWriteRWLock");
			rwLock->core.lockCount++;
			wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
	} else {
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
			waiter = (struct ThreadCB *)node;
			SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 1354, "sceKernelUnlockWriteRWLock");
			ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitMode == RW_LOCK_MODE_WRITE) {
				acquireRWLock(rwLock, waiter);
				wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
				break;
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		SCE_DBG_ASSERT_CHECK_FUNC(node != (struct ThreadmgrWaitNode *)queue, 1367, "sceKernelUnlockWriteRWLock");
	}
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
resume:
	ksceKernelCpuResumeIntr(intr);
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
	uint32_t saved, wake;
	int intr, ret, readers, writers;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL && type == 1) {
		SCE_DBG_ASSERT_PRINTF(0, 1442, "*** error: illegal context. rwLockId=0x%08x[%s]\n",
				      rwLockId, ksceGUIDName(rwLockId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (type > 1) {
		SCE_DBG_PRINTF(0, 1450, "*** error: sceKernelCancelRWLock(0x%08X[%s])=0x%08X, "
			"type=0x%08X\n", rwLockId, ksceGUIDName(rwLockId), SCE_KERNEL_ERROR_ILLEGAL_TYPE,
			type);
		return SCE_KERNEL_ERROR_ILLEGAL_TYPE;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(rwLockId,
		s_threadmgrRuntime.manager->rwLockClass, (SceObjectBase **)&rwLock);
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
	writers = 0;
	queue = (struct ThreadmgrWaitQueue *)&rwLock->next;
	ksceKernelSpinlockLowLock(&rwLock->lock);
	wake = releaseRWLock(&rwLock->core);
	readers = 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		waiter = (struct ThreadCB *)node;
		next = node->next;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1479);
		ksceKernelSpinlockLowLock(THREAD_LOCK(waiter));
		if (waiter->waitMode == RW_LOCK_MODE_READ)
			readers++;
		else
			writers++;
		wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue,
						  SCE_KERNEL_ERROR_WAIT_CANCEL);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	if (numReadWaitThreads != NULL)
		*numReadWaitThreads = readers;
	if (numWriteWaitThreads != NULL)
		*numWriteWaitThreads = writers;
	if (type == 1) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		acquireRWLock(rwLock, thread);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	ksceKernelSpinlockLowUnlock(&rwLock->lock);
	ksceGUIDReleaseObject(rwLockId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* inside a syscall the writer is reported by its user-visible thread ID */
int sceKernelGetRWLockInfo(SceUID rwLockId, ThreadmgrRWLockInfo *info)
{
	struct ThreadmgrThreadObject *owner;
	ThreadmgrRWLockInfo local;
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
			ret = ksceGUIDGetObject(local.writeOwnerId, &owner);
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
int SceThreadmgrForDriver_E2B57231(int level, ThreadmgrRWLockInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrRWLockInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->rwLockClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->rwLockClass, level, vector, num, &got);
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* the same for the rw locks a process holds PUIDs of, with user-visible IDs */
int sceKernelGetRWLockInfoAllProc(SceUID pid, int level, ThreadmgrRWLockInfo *infos,
				  SceSize size, SceSize *count)
{
	struct ThreadmgrThreadObject *owner;
	ThreadmgrRWLockInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret, err;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid,
		s_threadmgrRuntime.manager->rwLockClass, level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid,
			s_threadmgrRuntime.manager->rwLockClass, level, vector, num, &got);
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
		if (getRWLockInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
			continue;
		info.rwLockId = vector[i];
		if (info.writeOwnerId != 0) {
			err = ksceGUIDGetObject(info.writeOwnerId, &owner);
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
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}
