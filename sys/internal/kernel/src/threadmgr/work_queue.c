/*
 * SceKernelThreadMgr: work queues. A queue owns a few worker threads that
 * wait on its semaphore and run the queue's tasks (SceUIDWorkTaskObject) in
 * order (by priority with attr 0x2000), on the scheduling thread's
 * permissions; a user task runs in user mode on the syscall's user stack.
 * Every task gets the next sequence number; waiting on the queue waits for
 * every task scheduled so far, waiting on a task for that task.
 */

#include <string.h>
#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "mutex.h"
#include "syscall_context.h"

#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_USER		0x80000000
#define TRAP_REQUEST_UNK2000000		0x02000000	/* PLACEHOLDER: name */
#define WAIT_TYPE_WORK_QUEUE		0x04000000
#define WAIT_TYPE_WORK_TASK		0x08000000
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID	0x80028301	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID	0x80028302	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_ILLEGAL_COUNT	0x8002000f

#define WORK_QUEUE_ATTR_THREADS		0x00000001	/* the option's thread count */
#define WORK_QUEUE_ATTR_PRIORITY	0x00000002	/* the option's thread priority */
#define WORK_QUEUE_ATTR_STACK_SIZE	0x00000004
#define WORK_QUEUE_ATTR_CPU_AFFINITY	0x00000008
#define WORK_QUEUE_ATTR_KERNEL		0x00000080	/* PLACEHOLDER: a kernel object even from a syscall */
#define WORK_QUEUE_ATTR_TASK_PRIORITY	0x00002000	/* tasks run by the scheduler's priority */
#define WORK_QUEUE_ATTR_VALID		0x0000200f
#define WORK_QUEUE_MAX_THREADS		4
#define WORK_QUEUE_THREAD_PRIORITY	0xbf
#define WORK_QUEUE_THREAD_STACK		0x2000
#define GLOBAL_WORK_QUEUE_UID		0x10023	/* SceKernelGlobalWorkQueue's fixed UID */

#define SAVE_ALIGN			64
#define SAVE_ALIGNED(buffer)		(((uintptr_t)(buffer) + SAVE_ALIGN - 1) & ~(SAVE_ALIGN - 1))
#define TASK_VFP_SAVE_SIZE		0x48
#define PSR_T				0x00000020
#define PSR_KERNEL_CALLBACK		0x0000005f
#define PSR_USER_CALLBACK		0x00000050
#define CPACR_CP10_CP11_FULL		0x00f00000
#define VFP_EXCEPTION_KERNEL		0x00000087	/* PLACEHOLDER: name */
#define USER_TLS_VFP_EXCEPTION		0x7f0

#define WORK_QUEUE_CLASS()	(s_threadmgrRuntime.manager->workQueueClass)
#define WORK_TASK_CLASS()	(s_threadmgrRuntime.manager->workTaskClass)

/* SceUIDWorkQueueObject (0x68) */
typedef struct ThreadmgrWorkQueue {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	void *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitNode *next;		/* 0x10: the threads waiting for it to drain */
	struct ThreadmgrWaitNode *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;
	uint32_t attributes;			/* 0x20 */
	uint32_t reserved0;			/* 0x24: zeroed */
	ListNode pending;	/* 0x28: the tasks to run (ThreadmgrWorkTaskCB.link) */
	ListNode running;	/* 0x30: the tasks being run */
	uint64_t scheduled;			/* 0x38: the last task's sequence number */
	uint64_t completed;			/* 0x40: the highest finished task's */
	SceUID creatorPid;			/* 0x48 */
	SceUID uid;				/* 0x4c: PLACEHOLDER: Sony stores the last thread created */
	SceUID reservedId;			/* 0x50: always -1; reported by the info */
	SceUID sema;				/* 0x54: counts the pending tasks */
	uint32_t attr;				/* 0x58 */
	uint32_t threadCount;			/* 0x5c */
	SceUID *threads;			/* 0x60 */
	uint32_t reserved1;
} ThreadmgrWorkQueue;

/* a task's control block (SceUIDWorkTaskObject +0x28) */
typedef struct ThreadmgrWorkTaskCB {
	ThreadmgrWorkQueue *queue;		/* 0x00: NULL once the queue is gone */
	SceUID creatorPid;			/* 0x04 */
	ListNode link;		/* 0x08: in its queue's pending or running list */
	SceUID guid;				/* 0x10 */
	SceUID puid;				/* 0x14 */
	uint64_t sequence;			/* 0x18 */
	uint32_t done;				/* 0x20 */
	uint32_t closed;			/* 0x24: its UID is closed */
	uint32_t user;				/* 0x28: runs in user mode */
	SceUID threadId;			/* 0x2c: the worker running it */
	SceUID queueId;				/* 0x30 */
	uintptr_t entry;			/* 0x34 */
	void *arg;				/* 0x38 */
	SceUID creatorThreadId;			/* 0x3c */
	const void *caller;			/* 0x40 */
	int priority;				/* 0x44: the scheduler's */
	int permission;				/* 0x48: the scheduler's, for the run */
	SceUID tlsProcessId;			/* 0x4c */
	uint32_t ioFlags;			/* 0x50: the creator's, for the run */
	uint32_t reserved1;
} ThreadmgrWorkTaskCB;

/* SceUIDWorkTaskObject (0x80) */
typedef struct ThreadmgrWorkTask {
	SceUID ownerPid;
	void *cls;
	void *uidEntry;				/* 0x08: the GUID entry */
	void *cacheNext;			/* 0x0c: the next cached object; &uidEntry while in use */
	struct ThreadmgrWaitNode *next;		/* 0x10: the threads waiting for it */
	struct ThreadmgrWaitNode *prev;
	SceKernelSpinlock lock;			/* 0x18 */
	uint32_t waiting;
	uint32_t attributes;			/* 0x20 */
	uint32_t reserved0;			/* 0x24: zeroed */
	ThreadmgrWorkTaskCB cb;			/* 0x28 */
} ThreadmgrWorkTask;

_Static_assert(sizeof(ThreadmgrWorkQueue) == 0x68 && sizeof(ThreadmgrWorkTask) == 0x80 &&
	       offsetof(ThreadmgrWorkTask, cb.sequence) == 0x40 &&
	       offsetof(ThreadmgrWorkTask, cb.priority) == 0x6c,
	       "work queue object layouts");

#define TASK_FROM_LINK(l) \
	((ThreadmgrWorkTaskCB *)((uintptr_t)(l) - offsetof(ThreadmgrWorkTaskCB, link)))

/* sceKernelCreateWorkQueue's option (0x14) */
typedef struct ThreadmgrWorkQueueOpt {
	SceSize size;
	uint32_t threadCount;
	int priority;
	SceSize stackSize;
	uint32_t cpuAffinityMask;
} ThreadmgrWorkQueueOpt;

/* SceThreadmgrForDriver_B92709C4 (0x40) */
typedef struct ThreadmgrWorkQueueInfo {
	SceSize size;
	SceUID workQueueId;
	char name[32];				/* 0x08 */
	uint32_t attr;				/* 0x28 */
	uint32_t threadCount;
	uint64_t scheduled;			/* 0x30 */
	uint32_t pendingCount;			/* 0x38 */
	uint32_t reserved;
} ThreadmgrWorkQueueInfo;

/* the information lists' entries: their threads follow ((threadCount + 0x12) * 4) */
typedef struct ThreadmgrWorkQueueInfoFull {
	SceSize size;
	SceUID workQueueId;
	SceUID creatorPid;
	SceUID reservedId;			/* 0x0c: the queue's */
	char name[32];				/* 0x10 */
	uint32_t attr;				/* 0x30 */
	uint32_t threadCount;
	uint64_t scheduled;			/* 0x38 */
	uint32_t pendingCount;			/* 0x40 */
	SceUID threads[];			/* 0x44 */
} ThreadmgrWorkQueueInfoFull;

/* the work task information (0x58) */
typedef struct ThreadmgrWorkTaskInfo {
	SceSize size;
	SceUID workTaskId;
	SceUID creatorPid;
	SceUID puid;
	char name[32];				/* 0x10 */
	uintptr_t entry;			/* 0x30 */
	void *arg;
	SceUID workQueueId;			/* 0x38 */
	SceUID threadId;
	SceUID creatorThreadId;			/* 0x40 */
	const void *caller;
	int priority;				/* 0x48 */
	int permission;
	SceUID tlsProcessId;
	uint32_t ioFlags;			/* 0x54 */
} ThreadmgrWorkTaskInfo;

/* SceThreadmgrForDriver_0B604A3C (0x34) */
typedef struct ThreadmgrWorkTaskInfoShort {
	SceSize size;
	SceUID workTaskId;
	char name[32];				/* 0x08 */
	uintptr_t entry;			/* 0x28 */
	void *arg;
	SceUID workQueueId;			/* 0x30 */
} ThreadmgrWorkTaskInfoShort;

_Static_assert(sizeof(ThreadmgrWorkQueueInfo) == 0x40 && sizeof(ThreadmgrWorkTaskInfo) == 0x58 &&
	       sizeof(ThreadmgrWorkTaskInfoShort) == 0x34 &&
	       offsetof(ThreadmgrWorkQueueInfoFull, threads) == 0x44,
	       "work queue info layouts");

extern void SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags,
					 uintptr_t vfp) __attribute__((noreturn));
extern int SceIntrmgrForKernel_71C5F42F(struct ThreadRegisterContext *context, uintptr_t vfp)
	__attribute__((returns_twice));
extern uintptr_t SceSysrootForKernel_AD74BEB3(SceProcessObject *process);
extern int SceSysmemForKernel_BF275C5A(SceUID uid, SceClass *cls, const char *name, void *object);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern int ksceKernelFreeHeapMemory(SceUID heap, void *entry);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector,
				   SceSize num, SceSize *count);
extern int snprintf(char *str, SceSize size, const char *format, ...);
SceUID ksceKernelCreateSema(const char *name, SceUInt32 attr, int initCount, int maxCount,
			    const void *option);
int ksceKernelDeleteSema(SceUID semaId);
int ksceKernelSignalSema(SceUID semaId, int signalCount);
int ksceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout);
SceUID ksceKernelCreateThread(const char *name, int (*entry)(SceSize, void *), int priority,
			      SceSize stackSize, SceUInt32 attr, int cpuAffinityMask,
			      const void *option);
int ksceKernelStartThread(SceUID threadId, SceSize argSize, void *argp);
int ksceKernelDeleteThread(SceUID threadId);
SceUID ksceKernelGetThreadId(void);
SceUID sceKernelGetProcessId(void);
int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);
void getWakeupTime(uint64_t *wakeup, uint32_t timeout);
int releaseThreadWaitState(struct ThreadCB *thread, uintptr_t waitable, int result);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);
int revalidateWaitState(uintptr_t waitable);
uint32_t wakeupWaitObjectAll(uintptr_t waitable, int result, uint32_t *count);
void initRegisterContext(struct ThreadRegisterContext *regs);
void initVfpContext(uintptr_t vfp);
int uidWaitableConstructor(void *object);
void returnFromWorkTaskEntry(int result) __attribute__((noreturn));

/* a short pause between lock attempts (lock order: task before queue) */
static inline void spinDelay(void)
{
	volatile int spin = 10;

	do
		spin--;
	while (spin != 0);
}

/* whether every task up to target has finished: none of them still runs */
static inline int isWorkQueueDone(const ThreadmgrWorkQueue *queue, uint64_t target)
{
	const ListNode *link;
	uint64_t oldest;

	if (target > queue->completed)
		return 0;
	oldest = queue->completed + 1;
	for (link = queue->running.next; link != &queue->running; link = link->next)
		if (TASK_FROM_LINK(link)->sequence < oldest)
			oldest = TASK_FROM_LINK(link)->sequence;
	return oldest > target;
}

/* SceUIDWorkTaskClass's destructor: its waiters wake with WAIT_DELETE */
int uidWorkTask_Destructor(ThreadmgrWorkTask *task)
{
	ThreadmgrWorkQueue *queue;
	uint32_t wake;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2402);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&task->lock);
	wake = wakeupWaitObjectAll((uintptr_t)&task->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	ksceKernelSpinlockLowUnlock(&task->lock);
	if (task->cb.queue != NULL) {
		ksceKernelSpinlockLowLock(&task->cb.queue->lock);
		queue = task->cb.queue;
		ListLinkRemove(&task->cb.link);
		ksceKernelSpinlockLowUnlock(&queue->lock);
	}
	if ((wake & 1) && currentThreadContext() != NULL) {
		switchThread((intr >> 7) & 1);
		ksceKernelCpuResumeIntr(intr);
		return 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/* SceUIDWorkQueueClass's constructor */
static int uidWorkQueue_Constructor(void *object)
{
	ThreadmgrWorkQueue *queue = object;

	memset(&queue->pending, 0, sizeof(*queue) - offsetof(ThreadmgrWorkQueue, pending));
	queue->pending.next = &queue->pending;
	queue->pending.prev = &queue->pending;
	queue->running.next = &queue->running;
	queue->running.prev = &queue->running;
	return 0;
}

/* SceUIDWorkTaskClass's constructor */
static int uidWorkTask_Constructor(void *object)
{
	memset(&((ThreadmgrWorkTask *)object)->cb, 0, sizeof(ThreadmgrWorkTaskCB));
	return 0;
}

/* the queue's waiters wake in order while what they wait for is done */
int uidWorkQueue_RevalidateMethod(void *cls, void *object)
{
	ThreadmgrWorkQueue *queue = object;
	struct ThreadmgrWaitNode *head = (struct ThreadmgrWaitNode *)&queue->next;
	struct ThreadCB *thread, *next;
	uint32_t wake;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2299);
	SCE_DBG_ASSERT_CHECK(queue->lock != 0, 2302);
	if (queue->waiting == 0)
		return 0;
	wake = 0;
	for (thread = (struct ThreadCB *)queue->next;
	     (struct ThreadmgrWaitNode *)thread != head; thread = next) {
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2312);
		next = (struct ThreadCB *)thread->next;
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		SCE_DBG_ASSERT_CHECK(thread->waitId >= 0, 2315);
		if (!isWorkQueueDone(queue, ((uint64_t)thread->waitArg1 << 32) | thread->waitArg0)) {
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			return wake;
		}
		wake |= wakeupAndDistributeThread(thread, (uintptr_t)head, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	return wake;
}

/* SceUIDWorkQueueClass's destructor: its tasks are dropped, its workers deleted */
int uidWorkQueue_Destructor(ThreadmgrWorkQueue *queue)
{
	ListNode *link, *next;
	ThreadmgrWorkTaskCB *task;
	unsigned int i;
	uint32_t wake;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 2225);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&queue->lock);
	for (link = queue->running.next; link != &queue->running; link = next) {
		next = link->next;
		TASK_FROM_LINK(link)->queue = NULL;
		ListLinkRemove(link);
	}
	for (link = queue->pending.next; link != &queue->pending; link = next) {
		task = TASK_FROM_LINK(link);
		next = link->next;
		ListLinkRemove(link);
		task->closed = 1;
		if (ksceGUIDClose(task->guid) < 0)
			SCE_PANIC(SCE_LINE(2256, 2253));
	}
	wake = wakeupWaitObjectAll((uintptr_t)&queue->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	ksceKernelDeleteSema(queue->sema);
	for (i = 0; i < queue->threadCount; i++)
		ksceKernelDeleteThread(queue->threads[i]);
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, queue->threads);
	ksceKernelSpinlockLowUnlock(&queue->lock);
	if ((wake & 1) && currentThreadContext() != NULL) {
		switchThread((intr >> 7) & 1);
		ksceKernelCpuResumeIntr(intr);
		return 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/* waits until every task scheduled on the queue so far has finished */
int waitWorkQueue(SceUID workQueueId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrWorkQueue *queue;
	uint64_t wakeup, target;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 744, "*** error: illegal context. workQueueId=0x%08x[%s]\n",
				      workQueueId, ksceGUIDName(workQueueId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 752, "*** error: irq suspended. workQueueId=0x%08x[%s]\n",
				      workQueueId, ksceGUIDName(workQueueId));
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
	if (ksceGUIDReferObjectWithClass(workQueueId, WORK_QUEUE_CLASS(),
					 (SceObjectBase **)&queue) < 0) {
		if (!ThreadmgrInSyscall()) {
			SCE_DBG_ASSERT_PRINTF(0, 778,
				"*** error: unknown workqueue id. workQueueId=0x%08x\n", workQueueId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID;
		} else {
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID | SYSCALL_ERROR_BIT;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&queue->lock);
	target = queue->scheduled;
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&queue->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&queue->lock);
		}
		if (isWorkQueueDone(queue, target)) {
			ret = 0;
			ksceKernelSpinlockLowUnlock(&queue->lock);
			goto release;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitArg0 = (uint32_t)target;
		thread->waitArg1 = (uint32_t)(target >> 32);
		if (!callbacks) {
			ret = waitWaitableObject(workQueueId, (uintptr_t)&queue->next,
						 WAIT_TYPE_WORK_QUEUE, &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(workQueueId, (uintptr_t)&queue->next, WAIT_TYPE_WORK_QUEUE,
					 &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&queue->lock);
	ksceGUIDReleaseObject(workQueueId);
	if (!callbacks)
		goto resume;
callbacks:
	if (thread->flags & THREAD_FLAG_CALLBACKS) {
		cbRet = callCallbackAll(thread);
		if (cbRet < 0)
			ret = cbRet;
	}
	goto resume;
release:
	ksceGUIDReleaseObject(workQueueId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* waits until the task has run (WAIT_DELETE if it was dropped) */
int waitWorkTask(SceUID workTaskId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrWorkTask *task;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 901, "*** error: illegal context. workTaskId=0x%08x[%s]\n",
				      workTaskId, ksceGUIDName(workTaskId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 909, "*** error: irq suspended. workTaskId=0x%08x[%s]\n",
				      workTaskId, ksceGUIDName(workTaskId));
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
	if (ksceGUIDReferObjectWithClass(workTaskId, WORK_TASK_CLASS(),
					 (SceObjectBase **)&task) < 0) {
		if (!ThreadmgrInSyscall()) {
			SCE_DBG_ASSERT_PRINTF(0, 935,
				"*** error: unknown worktask id. workTaskId=0x%08x\n", workTaskId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID;
		} else {
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID | SYSCALL_ERROR_BIT;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&task->lock);
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&task->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&task->lock);
		}
		while (ksceKernelSpinlockLowTrylock(&task->cb.queue->lock) < 0) {
			ksceKernelSpinlockLowUnlock(&task->lock);
			spinDelay();
			ksceKernelSpinlockLowLock(&task->lock);
		}
		if (task->cb.done) {
			ret = 0;
			ksceKernelSpinlockLowUnlock(&task->cb.queue->lock);
			ksceKernelSpinlockLowUnlock(&task->lock);
			goto release;
		}
		if (task->cb.closed) {
			ret = SCE_KERNEL_ERROR_WAIT_DELETE;
			ksceKernelSpinlockLowUnlock(&task->cb.queue->lock);
			ksceKernelSpinlockLowUnlock(&task->lock);
			goto release;
		}
		ksceKernelSpinlockLowUnlock(&task->cb.queue->lock);
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (!callbacks) {
			ret = waitWaitableObject(workTaskId, (uintptr_t)&task->next, WAIT_TYPE_WORK_TASK,
						 &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(workTaskId, (uintptr_t)&task->next, WAIT_TYPE_WORK_TASK,
					 &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&task->lock);
	ksceGUIDReleaseObject(workTaskId);
	if (!callbacks)
		goto resume;
callbacks:
	if (thread->flags & THREAD_FLAG_CALLBACKS) {
		cbRet = callCallbackAll(thread);
		if (cbRet < 0)
			ret = cbRet;
	}
	goto resume;
release:
	ksceGUIDReleaseObject(workTaskId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

static inline uint32_t countPendingTasks(const ThreadmgrWorkQueue *queue)
{
	const ListNode *link;
	uint32_t n = 0;

	for (link = queue->pending.next; link != &queue->pending; link = link->next)
		n++;
	return n;
}

/* the queue's information */
int getWorkQueueInfo(SceUID workQueueId, ThreadmgrWorkQueueInfo *info)
{
	ThreadmgrWorkQueue *queue;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(workQueueId, WORK_QUEUE_CLASS(),
					 (SceObjectBase **)&queue) < 0) {
		ret = ThreadmgrInSyscall() ?
			(SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID | SYSCALL_ERROR_BIT) :
			SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID;
	} else {
		info->workQueueId = workQueueId;
		strncpy(info->name, ksceGUIDName(workQueueId), sizeof(info->name) - 1);
		ksceKernelSpinlockLowLock(&queue->lock);
		info->attr = queue->attr;
		info->threadCount = queue->threadCount;
		info->scheduled = queue->scheduled;
		info->pendingCount = countPendingTasks(queue);
		ksceKernelSpinlockLowUnlock(&queue->lock);
		ksceGUIDReleaseObject(workQueueId);
		ret = 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* the queue's information with its worker threads; info->size is the room given */
int getWorkQueueInfoFull(SceUID workQueueId, ThreadmgrWorkQueueInfoFull *info)
{
	ThreadmgrWorkQueue *queue;
	int intr, ret;

	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(workQueueId, WORK_QUEUE_CLASS(),
					 (SceObjectBase **)&queue) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID;
		goto resume;
	}
	ksceKernelSpinlockLowLock(&queue->lock);
	if (info->size < (queue->threadCount + 0x12) * 4) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		goto resume;
	}
	memset(info, 0, 0x48);
	info->workQueueId = workQueueId;
	info->size = (queue->threadCount + 0x12) * 4;
	strncpy(info->name, ksceGUIDName(workQueueId), sizeof(info->name) - 1);
	info->creatorPid = queue->creatorPid;
	info->reservedId = queue->reservedId;
	info->attr = queue->attr;
	info->threadCount = queue->threadCount;
	info->scheduled = queue->scheduled;
	info->pendingCount = countPendingTasks(queue);
	ret = 0;
	memcpy(info->threads, queue->threads, queue->threadCount * sizeof(SceUID));
	ksceKernelSpinlockLowUnlock(&queue->lock);
	ksceGUIDReleaseObject(workQueueId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* the task's information, read under its queue's lock */
int getWorkTaskInfo(SceUID workTaskId, ThreadmgrWorkTaskInfo *info)
{
	ThreadmgrWorkTask *task;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(workTaskId, WORK_TASK_CLASS(),
					 (SceObjectBase **)&task) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID;
	} else {
		info->workTaskId = workTaskId;
		ret = 0;
		info->creatorPid = task->cb.creatorPid;
		info->puid = task->cb.puid;
		strncpy(info->name, ksceGUIDName(workTaskId), sizeof(info->name) - 1);
		ksceKernelSpinlockLowLock(&task->cb.queue->lock);
		info->arg = task->cb.arg;
		info->threadId = task->cb.threadId;
		info->creatorThreadId = task->cb.creatorThreadId;
		info->caller = task->cb.caller;
		info->priority = task->cb.priority;
		info->permission = task->cb.permission;
		info->tlsProcessId = task->cb.tlsProcessId;
		info->entry = task->cb.entry;
		info->ioFlags = task->cb.ioFlags;
		info->workQueueId = task->cb.queueId;
		ksceKernelSpinlockLowUnlock(&task->cb.queue->lock);
		ksceGUIDReleaseObject(workTaskId);
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* where a kernel task returns to: its result in waitResult */
void __attribute__((noreturn)) returnFromWorkTask(int result)
{
	struct ThreadCB *thread;
	struct ThreadRegisterContext *regs;

	ksceKernelCpuSuspendIntr();
	thread = currentThreadContext();
	thread->waitResult = result;
	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 188);
	regs = thread->pRegisterContext;
	regs->callbackContext->pmuserenr = regs->pmuserenr;
	regs->callbackContext->pmcr = regs->pmcr;
	SceIntrmgrForKernel_08FDB0CE(regs->callbackContext, 0, regs->callbackVfp);
}

/*
 * Runs the task's entry(arg) on the worker, on the scheduler's permissions;
 * a user task in user mode on the stack of the syscall that runs the worker.
 * Returns when it has (returnFromWorkTask, or the process's return stub).
 */
void callWorkTaskEntry(ThreadmgrWorkTaskCB *task, void *arg)
{
	struct ThreadCB *thread =
		currentThreadContext();
	uint8_t contextBuffer[sizeof(struct ThreadRegisterContext) + SAVE_ALIGN - 4];
	uint8_t vfpBuffer[TASK_VFP_SAVE_SIZE + SAVE_ALIGN - 4];
	struct ThreadRegisterContext *context, *regs;
	ThreadSyscallFrame *frame, *savedUserFrame;
	volatile uint32_t *vfpException;
	uint32_t savedUnk30, savedVfpException, savedUnk100, request;
	uintptr_t entry = task->entry, vfp, stack;
	int savedPermission, i;
	SceUID savedTlsProcessId;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 430);
	context = (struct ThreadRegisterContext *)SAVE_ALIGNED(contextBuffer);
	memset(context, 0, sizeof(*context));
	if (thread->pVfpRegisterContext != 0) {
		vfp = SAVE_ALIGNED(vfpBuffer) | 1;
		memset((void *)vfp, 0, TASK_VFP_SAVE_SIZE);
	} else {
		vfp = 0;
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	savedUnk30 = thread->kernelStackTop;
	savedUserFrame = thread->userFrame;
	savedVfpException = thread->vfpException;
	savedPermission = thread->permission;
	savedTlsProcessId = thread->tlsProcessId;
	savedUnk100 = thread->ioFlags;
	if (SceIntrmgrForKernel_71C5F42F(context, vfp) != 0) {
		/* back from the task */
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		regs = thread->pRegisterContext;
		regs->callbackVfp = regs->callbackContext->callbackVfp;
		regs->callbackContext = regs->callbackContext->callbackContext;
		thread->kernelStackTop = savedUnk30;
		thread->userFrame = savedUserFrame;
		thread->vfpException = savedVfpException;
		thread->permission = savedPermission;
		thread->tlsProcessId = savedTlsProcessId;
		thread->ioFlags = savedUnk100;
		vfpException = currentVfpException();
		SCE_DBG_ASSERT_CHECK(vfpException != NULL, 468);
		*vfpException = thread->vfpException;
		__asm__ volatile("dsb sy" : : : "memory");
		if (task->user && ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
						       &thread->vfpException, sizeof(uint32_t)) < 0)
			SCE_DBG_ASSERT_FAIL(475);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		return;
	}

	regs = thread->pRegisterContext;
	context->callbackContext = regs->callbackContext;
	context->callbackVfp = regs->callbackVfp;
	regs->callbackVfp = vfp;
	regs->callbackContext = context;
	thread->tlsProcessId = task->tlsProcessId;
	thread->ioFlags = task->ioFlags;
	thread->permission = task->permission;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	stack = context->sp;
	if (task->user) {
		request = thread->flags & THREAD_FLAG_REQUEST_MASK;
		if (request == 0 && thread->debugSuspendReason != 0)
			request = TRAP_REQUEST_UNK2000000;
		if (request != 0) {
			sceKernelTrapThreadAfterSyscall(0, 0, request);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_RESUMED_WAIT);
		}
		frame = thread->userFrame;
		stack = frame->sp;
		thread->pRegisterContext->tpidrurw = frame->tpidrurw;
		thread->kernelStackTop = context->sp;
		thread->pRegisterContext->syscallAux = (uintptr_t)frame;
		thread->userFrame = NULL;
	}
	initRegisterContext(thread->pRegisterContext);
	regs = thread->pRegisterContext;
	regs->r[1] = 0;
	regs->r[2] = 0;
	regs->full = 1;
	regs->pc = entry & ~1;
	regs->r[3] = 0;
	regs->r[0] = (uintptr_t)arg;
	regs->sp = stack;
	if (entry & 1)
		regs->cpsr = PSR_T;
	if (!task->user) {
		regs->lr = (uintptr_t)returnFromWorkTaskEntry;
		regs->cpsr |= PSR_KERNEL_CALLBACK;
		thread->vfpException = VFP_EXCEPTION_KERNEL;
	} else {
		regs->cpsr |= PSR_USER_CALLBACK;
		regs->lr = SceSysrootForKernel_AD74BEB3(thread->process);
		thread->pRegisterContext->tpidruro = thread->tls;
		thread->vfpException = 0;
		if (ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VFP_EXCEPTION),
					 &thread->vfpException, sizeof(uint32_t)) < 0)
			SCE_DBG_ASSERT_FAIL(559);
	}
	thread->pRegisterContext->cpacr = CPACR_CP10_CP11_FULL;
	initVfpContext(thread->pVfpRegisterContext);
	regs = thread->pRegisterContext;
	regs->pmccntr = 0;
	for (i = 0; i < 6; i++) {
		regs->pmevent[i].eventType = 0;
		regs->pmevent[i].count = 0;
	}
	SceIntrmgrForKernel_08FDB0CE(thread->pRegisterContext, 0, thread->pVfpRegisterContext);
}

/* a worker: runs the queue's tasks as its semaphore counts them */
int workQueueThread(SceSize argSize, void *argp)
{
	ThreadmgrWorkQueue *queue = *(ThreadmgrWorkQueue **)argp;
	ListNode *link;
	ThreadmgrWorkTaskCB *cb;
	ThreadmgrWorkTask *task;
	uint32_t wake, revalidated;
	int intr;

	for (;;) {
		if (ksceKernelWaitSema(queue->sema, 1, NULL) < 0)
			return 0;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&queue->lock);
		while ((link = queue->pending.next) != &queue->pending) {
			cb = TASK_FROM_LINK(link);
			if (ksceGUIDReferObjectWithClass(cb->guid, WORK_TASK_CLASS(),
							 (SceObjectBase **)&task) < 0)
				SCE_PANIC(638);
			ListLinkRemove(link);
			link->next = link;
			link->prev = link;
			if (cb->closed) {
				ksceKernelSpinlockLowUnlock(&queue->lock);
				ksceGUIDReleaseObject(cb->guid);
			} else {
				cb->threadId = ksceKernelGetThreadId();
				link->next = &queue->running;
				link->prev = queue->running.prev;
				queue->running.prev = link;
				link->prev->next = link;
				ksceKernelSpinlockLowUnlock(&queue->lock);
				callWorkTaskEntry(cb, cb->arg);
				ksceKernelSpinlockLowLock(&task->lock);
				while (ksceKernelSpinlockLowTrylock(&queue->lock) < 0) {
					ksceKernelSpinlockLowUnlock(&task->lock);
					spinDelay();
					ksceKernelSpinlockLowLock(&task->lock);
				}
				cb->done = 1;
				if (!cb->closed) {
					cb->closed = 1;
					if (ksceGUIDClose(cb->guid) < 0)
						SCE_PANIC(SCE_LINE(683, 681));
				}
				wake = wakeupWaitObjectAll((uintptr_t)&task->next, 0, NULL);
				ksceKernelSpinlockLowUnlock(&task->lock);
				ListLinkRemove(link);
				link->next = link;
				link->prev = link;
				if (cb->sequence > queue->completed)
					queue->completed = cb->sequence;
				revalidated = revalidateWaitState((uintptr_t)&queue->next);
				ksceKernelSpinlockLowUnlock(&queue->lock);
				ksceGUIDReleaseObject(cb->guid);
				if ((revalidated | wake) & 1)
					switchThread((intr >> 7) & 1);
			}
			ksceKernelCpuResumeIntr(intr);
			intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&queue->lock);
		}
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&queue->lock, intr);
	}
}

/* SceUIDWorkQueueClass and SceUIDWorkTaskClass */
void startWorkQueue(void)
{
	struct ThreadmgrWaitableClass *cls;

	cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(340);
	if (uidExtendedClassInit(cls, "SceUIDWorkQueueClass",
			s_threadmgrRuntime.manager->waitableClass, 0x80,
			uidWorkQueue_Constructor, (SceClassCallback)uidWorkQueue_Destructor,
			uidDefaultCancelWait, uidWorkQueue_RevalidateMethod,
			uidDefault_ReleaseMethod) < 0)
		SCE_PANIC_FUNC(SCE_LINE(2192, 2189), "uidWorkQueueClassInit");
	s_threadmgrRuntime.manager->workQueueClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 356, " sizeof(SceUIDWorkQueueObject)=%d\n",
			     sizeof(ThreadmgrWorkQueue));
	cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(361);
	if (uidExtendedClassInit(cls, "SceUIDWorkTaskClass",
			s_threadmgrRuntime.manager->waitableClass, 0x80,
			uidWorkTask_Constructor, (SceClassCallback)uidWorkTask_Destructor,
			uidDefaultCancelWait, uidDefault_RevalidateMethod,
			uidDefault_ReleaseMethod) < 0)
		SCE_PANIC_FUNC(SCE_LINE(2374, 2371), "uidWorkTaskClassInit");
	s_threadmgrRuntime.manager->workTaskClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 377, " sizeof(SceUIDWorkTaskObject)=%d\n",
			     sizeof(ThreadmgrWorkTask));
}

/* SceKernelGlobalWorkQueue (UID 0x10023): four kernel workers */
int createGlobalWorkQueue(void)
{
	ThreadmgrWorkQueue *queue;
	char name[32];
	SceUID sema, thread, *threads;
	int intr, ret, i;

	snprintf(name, sizeof(name), "wqueue: %s", "SceKernelGlobalWorkQueue");
	sema = ksceKernelCreateSema(name, 0, 0, 0x7fffffff, NULL);
	if (sema < 0) {
		SCE_DBG_PRINTF(0xb, 249, " error: sceKernelCreateSema(0x%08X)\n", sema);
		SCE_PANIC(250);
	}
	threads = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
					    WORK_QUEUE_MAX_THREADS * sizeof(SceUID));
	if (threads == NULL) {
		SCE_DBG_PRINTF(0xb, 257, " error: sceKernelAllocHeapMemory()\n");
		SCE_PANIC(259);
	}
	for (i = 0; i < WORK_QUEUE_MAX_THREADS; i++) {
		thread = ksceKernelCreateThread(name, workQueueThread, WORK_QUEUE_THREAD_PRIORITY,
						WORK_QUEUE_THREAD_STACK, 0, 0xf, NULL);
		if (thread < 0) {
			SCE_DBG_PRINTF(0xb, 272, " error: sceKernelCreateThread(0x%08X)\n", thread);
			threads[i] = -1;
			SCE_PANIC(274);
		}
		threads[i] = thread;
	}
	intr = ksceKernelCpuSuspendIntr();
	queue = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*queue));
	if (queue == NULL)
		SCE_PANIC(285);
	if (SceSysmemForKernel_BF275C5A(GLOBAL_WORK_QUEUE_UID, WORK_QUEUE_CLASS(),
					"SceKernelGlobalWorkQueue", queue) < 0)
		SCE_PANIC(292);
	uidWorkQueue_Constructor(queue);
	uidWaitableConstructor(queue);
	queue->attr = 0;
	queue->threadCount = WORK_QUEUE_MAX_THREADS;
	queue->uid = GLOBAL_WORK_QUEUE_UID;
	queue->threads = threads;
	queue->sema = sema;
	queue->creatorPid = THREADMGR_KERNEL_PID;
	queue->reservedId = -1;
	for (i = 0; i < WORK_QUEUE_MAX_THREADS; i++) {
		ret = ksceKernelStartThread(threads[i], sizeof(queue), &queue);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 311, " error: sceKernelStartThread(0x%08X)\n", ret);
			SCE_PANIC(312);
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&queue->lock, intr);
	return 0;
}

SceUID sceKernelCreateWorkQueue(const char *name, SceUInt32 attr,
				const ThreadmgrWorkQueueOpt *option)
{
	UIDCreateOpt opt;
	struct ThreadCB *thread;
	ThreadmgrWorkQueue *queue;
	char threadName[32];
	SceUID sema, uid, *threads;
	uint32_t saved, count, objectAttr;
	unsigned int i;
	SceSize stackSize;
	int priority, cpuMask, intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	thread = currentThreadContext();
	if (currentThreadContext() == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (attr & ~WORK_QUEUE_ATTR_VALID) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option == NULL) {
		count = WORK_QUEUE_MAX_THREADS;
		priority = WORK_QUEUE_THREAD_PRIORITY;
		stackSize = WORK_QUEUE_THREAD_STACK;
		cpuMask = 0;
	} else {
		if (option->size > sizeof(*option)) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
			goto restore;
		}
		if (attr & WORK_QUEUE_ATTR_THREADS) {
			count = option->threadCount;
			if (count > WORK_QUEUE_MAX_THREADS) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
				goto restore;
			}
		} else {
			count = WORK_QUEUE_MAX_THREADS;
		}
		priority = (attr & WORK_QUEUE_ATTR_PRIORITY) ? option->priority :
							       WORK_QUEUE_THREAD_PRIORITY;
		stackSize = (attr & WORK_QUEUE_ATTR_STACK_SIZE) ? option->stackSize :
								  WORK_QUEUE_THREAD_STACK;
		cpuMask = 0;
		if (attr & WORK_QUEUE_ATTR_CPU_AFFINITY)
			cpuMask = option->cpuAffinityMask;
	}
	snprintf(threadName, sizeof(threadName), "wqueue: %s", name);
	sema = ksceKernelCreateSema(threadName, 0, 0, 0x7fffffff, NULL);
	if (sema < 0) {
		SCE_DBG_PRINTF(0xb, 1114, " error: sceKernelCreateSema(0x%08X)\n", sema);
		ret = sema;
		goto restore;
	}
	threads = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
					    count * sizeof(SceUID));
	if (threads == NULL) {
		SCE_DBG_PRINTF(0xb, 1122, " error: sceKernelAllocHeapMemory()\n");
		ret = SCE_KERNEL_ERROR_NO_MEMORY;
		goto deleteSema;
	}
	ret = sema;
	for (i = 0; i < count; i++) {
		ret = ksceKernelCreateThread(threadName, workQueueThread, priority, stackSize, 0,
					     cpuMask, NULL);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1137, " error: sceKernelCreateThread(0x%08X)\n", ret);
			threads[i] = -1;
			goto deleteThreads;
		}
		threads[i] = ret;
	}
	opt = (UIDCreateOpt){ .flags = UID_CREATE_OPT_ATTR };
	objectAttr = attr;
	if (ThreadmgrInSyscall() && !(attr & WORK_QUEUE_ATTR_KERNEL)) {
		opt.pid = thread->processId;
		opt.flags |= UID_CREATE_OPT_PROCESS_HEAP;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
			objectAttr = attr | THREAD_ATTR_UNLIMITED_OBJECTS;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
	}
	intr = ksceKernelCpuSuspendIntr();
	uid = ksceGUIDKernelCreateWithOpt(WORK_QUEUE_CLASS(), name, (SceGUIDKernelCreateOpt *)&opt,
					  (SceObjectBase **)&queue);
	queue->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
	queue->uid = ret;
	queue->attr = objectAttr;
	queue->creatorPid = sceKernelGetProcessId();
	queue->threadCount = count;
	queue->threads = threads;
	queue->reservedId = -1;
	queue->sema = sema;
	for (i = 0; i < count; i++) {
		ret = ksceKernelStartThread(threads[i], sizeof(queue), &queue);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1187, " error: sceKernelStartThread(0x%08X)\n", ret);
			if (ksceGUIDClose(uid) < 0)
				SCE_PANIC(SCE_LINE(1192, 1189));
			goto deleteThreads;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&queue->lock, intr);
	ret = uid;
	goto restore;
deleteThreads:
	if (threads[0] != -1) {
		for (i = 0; i < count; ) {
			ksceKernelDeleteThread(threads[i]);
			if (++i >= count || threads[i] == -1)
				break;
		}
	}
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, threads);
deleteSema:
	ksceKernelDeleteSema(sema);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_154F2C2A(SceUID workQueueId, SceUInt32 *timeout)
{
	return waitWorkQueue(workQueueId, timeout, 0);
}

int SceThreadmgrForDriver_48C06CD6(SceUID workQueueId, SceUInt32 *timeout)
{
	return waitWorkQueue(workQueueId, timeout, 1);
}

/* sceKernelScheduleWorkTask: queues entry(arg) as a task named name; returns the task's UID */
SceUID ksceKernelEnqueueWorkQueue(SceUID workQueueId, const char *name, uintptr_t entry, void *arg)
{
	UIDCreateOpt opt;
	struct ThreadCB *thread =
		currentThreadContext();
	ListNode *pos;
	ThreadmgrWorkQueue *queue;
	ThreadmgrWorkTask *task;
	SceUID creatorPid, sema;
	uint32_t saved, user;
	int intr, ret, uid;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceGUIDReferObjectWithClass(workQueueId, WORK_QUEUE_CLASS(),
					 (SceObjectBase **)&queue) < 0) {
		if (!ThreadmgrInSyscall()) {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1281, "sceKernelScheduleWorkTask",
				"*** error: unknown workqueue id. workQueueId=0x%08x\n", workQueueId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID;
		} else {
			ret = SCE_KERNEL_ERROR_UNKNOWN_WORK_QUEUE_ID | SYSCALL_ERROR_BIT;
		}
		goto restore;
	}
	opt = (UIDCreateOpt){ .flags = UID_CREATE_OPT_ATTR };
	user = ThreadmgrInSyscall();
	if (user) {
		creatorPid = thread->processId;
		opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		opt.pid = creatorPid;
	} else {
		creatorPid = THREADMGR_KERNEL_PID;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDKernelCreateWithOpt(WORK_TASK_CLASS(), name, (SceGUIDKernelCreateOpt *)&opt,
					  (SceObjectBase **)&task);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 1311, "sceKernelScheduleWorkTask",
				    " error: sceGUIDKernelCreate(0x%08X)\n", ret);
		ksceGUIDReleaseObject(workQueueId);
		goto resume;
	}
	task->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
	task->cb.user = user;
	task->cb.guid = ret;
	task->cb.entry = entry;
	task->cb.creatorPid = creatorPid;
	task->cb.queueId = workQueueId;
	task->cb.arg = arg;
	task->cb.puid = -1;
	task->cb.threadId = -1;
	task->cb.closed = 0;
	task->cb.creatorThreadId = ksceKernelGetThreadId();
	task->cb.caller = __builtin_return_address(0);
	if (thread == NULL) {
		task->cb.ioFlags = 0;
		task->cb.priority = 0x10000100;
		task->cb.permission = 0x80;
		task->cb.tlsProcessId = THREADMGR_KERNEL_PID;
	} else {
		task->cb.priority = thread->currentPriority;
		task->cb.permission = thread->permission;
		task->cb.tlsProcessId = thread->tlsProcessId;
		task->cb.ioFlags = thread->ioFlags;
	}
	ksceKernelSpinlockLowUnlock(&task->lock);
	ksceKernelSpinlockLowLock(&queue->lock);
	if (queue->attr & WORK_QUEUE_ATTR_TASK_PRIORITY) {
		/* after the tasks of the same or a higher priority */
		pos = queue->pending.next;
		if (pos != &queue->pending)
			while (TASK_FROM_LINK(pos)->priority <= task->cb.priority &&
			       (pos = pos->next) != &queue->pending)
				;
		task->cb.link.next = pos;
		task->cb.link.prev = pos->prev;
		pos->prev = &task->cb.link;
		task->cb.link.prev->next = &task->cb.link;
	} else {
		task->cb.link.next = &queue->pending;
		task->cb.link.prev = queue->pending.prev;
		queue->pending.prev = &task->cb.link;
		task->cb.link.prev->next = &task->cb.link;
	}
	SCE_DBG_ASSERT_CHECK_FUNC((queue->scheduled >> 32) != 0 ||
				  (uint32_t)queue->scheduled != 0xffffffff, 1377,
				  "sceKernelScheduleWorkTask");
	sema = queue->sema;
	uid = task->cb.guid;
	task->cb.sequence = ++queue->scheduled;
	task->cb.queue = queue;
	ksceKernelSpinlockLowUnlock(&queue->lock);
	ksceGUIDReleaseObject(workQueueId);
	ret = ksceKernelSignalSema(sema, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 1391, "sceKernelScheduleWorkTask",
				    " error: sceKernelSignalSema(0x%08X)\n", ret);
	else
		ret = uid;
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int SceThreadmgrForDriver_12FC0FAB(SceUID workTaskId, SceUInt32 *timeout)
{
	return waitWorkTask(workTaskId, timeout, 0);
}

int SceThreadmgrForDriver_B247EC4B(SceUID workTaskId, SceUInt32 *timeout)
{
	return waitWorkTask(workTaskId, timeout, 1);
}

/* SceThreadmgrForDriver_B92709C4: the queue's information */
int SceThreadmgrForDriver_B92709C4(SceUID workQueueId, ThreadmgrWorkQueueInfo *info)
{
	ThreadmgrWorkQueueInfo local;
	SceSize size;
	uint32_t saved;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getWorkQueueInfo(workQueueId, &local);
		if (ret >= 0) {
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * SceThreadmgrForDriver_920EA1CA: the queues visible at level with their
 * threads, packed; without room only the size they need goes to count.
 */
int SceThreadmgrForDriver_920EA1CA(int level, ThreadmgrWorkQueueInfoFull *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrWorkQueueInfo info;
	ThreadmgrWorkQueueInfoFull *out;
	SceSize got = 0, need;
	SceUID *vector;
	int num, total, found, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(WORK_QUEUE_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(WORK_QUEUE_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	found = 0;
	got = 0;
	need = 0;
	out = infos;
	for (i = 0; i < total; i++) {
		if (size < 0x48) {
			info.size = size;
			ret = getWorkQueueInfo(vector[i], &info);
			if (ret == 0)
				need += (info.threadCount + 0x12) * 4;
			out = NULL;
		} else {
			out->size = size;
			ret = getWorkQueueInfoFull(vector[i], out);
		}
		if (ret == 0) {
			found++;
			if (out != NULL) {
				size -= out->size;
				out = (ThreadmgrWorkQueueInfoFull *)((uintptr_t)out + out->size);
				got++;
			}
		}
	}
	if (infos == NULL)
		got = need;
	if (count != NULL)
		*count = got;
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return found;
}

/* SceThreadmgrForDriver_8C8A76EF: the same for a process's queue PUIDs */
int SceThreadmgrForDriver_8C8A76EF(SceUID pid, int level, ThreadmgrWorkQueueInfoFull *infos,
				   SceSize size, SceSize *count)
{
	ThreadmgrWorkQueueInfo info;
	ThreadmgrWorkQueueInfoFull *out;
	SceSize got = 0, need;
	SceUID *vector;
	int num, total, found, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid, WORK_QUEUE_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid, WORK_QUEUE_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	found = 0;
	got = 0;
	if (total < 1) {
		need = 0;
	} else {
		need = 0;
		out = infos;
		for (i = 0; i < total; i++) {
			if (size < 0x48) {
				info.size = size;
				ret = getWorkQueueInfo(kscePUIDtoGUID(pid, vector[i]), &info);
				if (ret == 0)
					need += (info.threadCount + 0x12) * 4;
				out = NULL;
			} else {
				out->size = size;
				ret = getWorkQueueInfoFull(kscePUIDtoGUID(pid, vector[i]), out);
			}
			if (ret == 0) {
				found++;
				if (out != NULL) {
					size -= out->size;
					out->workQueueId = vector[i];
					out = (ThreadmgrWorkQueueInfoFull *)((uintptr_t)out + out->size);
					got++;
				}
			}
		}
	}
	if (infos == NULL)
		got = need;
	if (count != NULL)
		*count = got;
	ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return found;
}

/* SceThreadmgrForDriver_0B604A3C: the task's entry, argument and queue (a syscall gets PUIDs) */
int SceThreadmgrForDriver_0B604A3C(SceUID workTaskId, ThreadmgrWorkTaskInfoShort *info)
{
	ThreadmgrWorkTaskInfoShort local;
	ThreadmgrWorkTask *task;
	SceUID puid, queueId;
	SceSize size, got;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	local = (ThreadmgrWorkTaskInfoShort){ .size = sizeof(local) };
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(workTaskId, WORK_TASK_CLASS(),
					 (SceObjectBase **)&task) < 0) {
		ret = ThreadmgrInSyscall() ?
			(SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID | SYSCALL_ERROR_BIT) :
			SCE_KERNEL_ERROR_UNKNOWN_WORK_TASK_ID;
		ksceKernelCpuResumeIntr(intr);
		goto restore;
	}
	local.workTaskId = workTaskId;
	strncpy(local.name, ksceGUIDName(workTaskId), sizeof(local.name) - 1);
	ksceKernelSpinlockLowLock(&task->cb.queue->lock);
	queueId = task->cb.queueId;
	local.entry = task->cb.entry;
	local.arg = task->cb.arg;
	local.workQueueId = queueId;
	ksceKernelSpinlockLowUnlock(&task->cb.queue->lock);
	ksceGUIDReleaseObject(workTaskId);
	ksceKernelCpuResumeIntr(intr);
	if (ThreadmgrInSyscall()) {
		if (PUIDGetPUIDVectorByGUID(0, workTaskId, 3, &puid, 4, &got) < 0 || got == 0)
			local.workTaskId = -1;
		else
			local.workTaskId = puid;
		if (PUIDGetPUIDVectorByGUID(0, queueId, 3, &puid, 4, &got) < 0 || got == 0)
			local.workQueueId = -1;
		else
			local.workQueueId = puid;
	}
	size = info->size;
	ret = 0;
	if (size > sizeof(local))
		size = sizeof(local);
	memcpy(info, &local, size);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_E605ED7A: the information of every task visible at level */
int SceThreadmgrForDriver_E605ED7A(int level, ThreadmgrWorkTaskInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrWorkTaskInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(WORK_TASK_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(WORK_TASK_CLASS(), level, vector, num, &got);
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
		if (getWorkTaskInfo(vector[i], &info) != 0)
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

/* SceThreadmgrForDriver_B4CED111: the same for a process's task PUIDs, in its IDs */
int SceThreadmgrForDriver_B4CED111(SceUID pid, int level, ThreadmgrWorkTaskInfo *infos,
				   SceSize size, SceSize *count)
{
	ThreadmgrWorkTaskInfo info;
	SceSize got = 0, found;
	SceUID *vector, puid;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid, WORK_TASK_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid, WORK_TASK_CLASS(), level, vector, num, &got);
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
		if (getWorkTaskInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
			continue;
		info.workTaskId = vector[i];
		if (PUIDGetPUIDVectorByGUID(pid, info.workQueueId, level, &puid, 4, &found) < 0 ||
		    found == 0)
			info.workQueueId = -1;
		else
			info.workQueueId = puid;
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
