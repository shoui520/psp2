/*
 * SceKernelThreadMgr: semaphore.c, the semaphores (SceUIDSemaphoreClass).
 * PLACEHOLDER: only part of the file is C so far; the rest is in threadmgr.c's
 * stubs (agent/match-notes/os0/kd/threadmgr.txt).
 */

#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "uid_waitable.h"
#include "syscall_context.h"

#define SEMA_ATTR_PRIORITY		0x00002000
#define SEMA_ITEM_SIZE			0x80	/* the class's object allocation size */

/* SceUIDSemaphoreObject (0x38) */
typedef struct ThreadmgrSemaphore {
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
	const void *option;			/* 0x28 */
	int count;				/* 0x2c */
	int maxCount;				/* 0x30 */
	int initCount;				/* 0x34 */
} ThreadmgrSemaphore;

_Static_assert(sizeof(ThreadmgrSemaphore) == 0x38, "SceUIDSemaphoreObject size");

/* ksceKernelGetSemaInfo */
typedef struct ThreadmgrSemaInfo {
	SceSize size;				/* 0x3c */
	SceUID semaId;
	char name[32];
	uint32_t attr;				/* 0x28 */
	int initCount;
	int currentCount;
	int maxCount;
	int numWaitThreads;			/* 0x38 */
} ThreadmgrSemaInfo;

extern const char *ksceGUIDName(SceUID guid);
extern char *strncpy(char *dst, const char *src, SceSize n);
extern void *memset(void *dst, int c, SceSize n);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
int uidCacheConstructor(void *object);
int uidCacheDestructor(void *object);

/* SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID, with bit 30 set inside a user syscall */
#define SEMA_ERROR_UNKNOWN_ID()		((ThreadmgrUserRoWord() & USERRO_IN_SYSCALL) ? \
					 0xc0028101 : 0x80028101)

/* SceUIDSemaphoreClass's revalidation: waiters get the count while it lasts, in queue order */
int uidSemaphore_RevalidateMethod(void *cls, void *object)
{
	ThreadmgrSemaphore *sema = object;
	struct ThreadmgrWaitQueue *queue = (struct ThreadmgrWaitQueue *)&sema->next;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *thread;
	int wake = 0;

#if FW_DEBUG
	if (ksceKernelGetAssertLevel() > 0) {
		SCE_DBG_ASSERT_LEVEL(1, ThreadmgrIntrDisabled(), 1418);
		if (ksceKernelGetAssertLevel() > 0)
			SCE_DBG_ASSERT_LEVEL(1, sema->lock != 0, 1421);
	}
#endif
	if (sema->waiting == 0)
		return 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		thread = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1431);
		next = node->next;
		ksceKernelSpinlockLowLock(&thread->lock);
		SCE_DBG_ASSERT_CHECK(thread->waitId >= 0, 1434);
		if (sema->count < thread->waitMode) {
			ksceKernelSpinlockLowUnlock(&thread->lock);
			return wake;
		}
		sema->count -= thread->waitMode;
		wake |= wakeupAndDistributeThread(thread, (uintptr_t)queue, 0);
		ksceKernelSpinlockLowUnlock(&thread->lock);
	}
	return wake;
}

/* SceUIDSemaphoreClass's constructor */
static int uidSemaphore_Constructor(void *object)
{
	ThreadmgrSemaphore *sema = object;

	memset(&sema->option, 0, 0x10);
	return 0;
}

#define SEMA_ATTR_LW			0x00000080	/* PLACEHOLDER: name */
#define SEMA_ATTR_UNLIMITED		0x00004000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006

/* SceUIDSemaphoreClass's destructor: the waiters wake with WAIT_DELETE, the owner's count drops */
static int uidSemaphore_Destructor(void *object)
{
	ThreadmgrSemaphore *sema = object;
	SceProcessObject *process;
	uint32_t wake;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1366);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&sema->lock);
	wake = wakeupWaitObjectAll((uintptr_t)&sema->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	ksceKernelSpinlockLowUnlock(&sema->lock);
	pid = sema->ownerPid;
	if (ksceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (sema->attr & SEMA_ATTR_LW)
				process->uidLwCount--;
			else if (sema->attr & SEMA_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->semaCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		ksceGUIDReleaseObject(pid);
	}
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	memset(&sema->option, 0, 0x10);
	return 0;
}

/* a semaphore's information (ksceKernelGetSemaInfo's core) */
int getSemaInfo(SceUID semaId, ThreadmgrSemaInfo *info)
{
	ThreadmgrSemaphore *sema;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(semaId,
		s_threadmgrRuntime.manager->semaphoreClass, (SceObjectBase **)&sema);
	if (ret < 0) {
		ret = SEMA_ERROR_UNKNOWN_ID();
	} else {
		info->semaId = semaId;
		strncpy(info->name, ksceGUIDName(semaId), sizeof(info->name) - 1);
		ksceKernelSpinlockLowLock(&sema->lock);
		info->attr = sema->attr;
		info->initCount = sema->initCount;
		info->currentCount = sema->count;
		info->maxCount = sema->maxCount;
		info->numWaitThreads = sema->waiting;
		ksceKernelSpinlockLowUnlock(&sema->lock);
		ksceGUIDReleaseObject(semaId);
		ret = 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* whether a thread of priority must queue behind the waiters already there */
bool isWaitOnly(struct ThreadmgrWaitQueue *queue, int priority)
{
	struct ThreadCB *first;

	if (queue->waiting == 0)
		return false;
	if (!(queue->attributes & SEMA_ATTR_PRIORITY))
		return true;
	first = (struct ThreadCB *)queue->next;
	if ((struct ThreadmgrWaitQueue *)first == queue)
		SCE_PANIC(SCE_LINE(119, 112));
	SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 119);
	return first->currentPriority <= priority;
}

int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);

#define WAIT_TYPE_SEMAPHORE		0x20
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_COUNT	0x8002000f

/* takes needCount from the semaphore, waiting for it (optionally running callbacks) */
int waitSema(SceUID semaId, int needCount, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrSemaphore *sema;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelPrintfAssertLevel(1, 0, SCE_DBGINFO(149), __builtin_return_address(0),
				"*** error: illegal context. semaId=0x%08x[%s]\n", semaId,
				ksceGUIDName(semaId));
#endif
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (needCount < 1) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
#if FW_DEBUG
		if (ksceKernelGetAssertLevel() > 0)
			ksceKernelPrintfAssertLevel(1, 0, SCE_DBGINFO(162), __builtin_return_address(0),
				"*** error: irq suspended. semaId=0x%08x[%s]\n", semaId,
				ksceGUIDName(semaId));
#endif
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
	ret = ksceGUIDReferObjectWithClass(semaId,
		s_threadmgrRuntime.manager->semaphoreClass, (SceObjectBase **)&sema);
	if (ret < 0) {
		if (!(ThreadmgrUserRoWord() & USERRO_IN_SYSCALL)) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelPrintfAssertLevel(1, 0, SCE_DBGINFO(188),
					__builtin_return_address(0),
					"*** error: unknown semaphore id. semaId=0x%08x\n", semaId);
#endif
			ret = 0x80028101;
		} else {
			ret = 0xc0028101;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&sema->lock);
	if (sema->maxCount < needCount) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	queue = (struct ThreadmgrWaitQueue *)&sema->next;
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&sema->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&sema->lock);
		}
		if (!isWaitOnly(queue, thread->currentPriority) && needCount <= sema->count) {
			sema->count -= needCount;
			ret = 0;
			goto unlock;
		}
		ksceKernelSpinlockLowLock(&thread->lock);
		thread->waitMode = needCount;
		if (!callbacks) {
			ret = waitWaitableObject(semaId, (uintptr_t)queue, WAIT_TYPE_SEMAPHORE, &wakeup,
						 timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(&thread->lock);
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(semaId, (uintptr_t)queue, WAIT_TYPE_SEMAPHORE, &wakeup, timeout,
					 NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(&thread->lock);
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&sema->lock);
	ksceGUIDReleaseObject(semaId);
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
	ksceKernelSpinlockLowUnlock(&sema->lock);
release:
	ksceGUIDReleaseObject(semaId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

extern SceClass *SceSysmemForKernel_368F1C36(SceUID guid);
extern int ksceGUIDClose(SceUID guid);
extern void *memcpy(void *dst, const void *src, SceSize n);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr);

#define SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS	0x80028002
#define SCE_KERNEL_ERROR_WAIT_CANCEL		0x80028007
#define SCE_KERNEL_ERROR_NO_MEMORY		0x8002710b
#define SCE_KERNEL_ERROR_SEMA_OVF		0x80028103
#define SDK_VERSION_NO_NEGATIVE_SIGNAL		0x02000000	/* from here a signal must not lower the count */

extern int kscePUIDClose(SceUID pid, SceUID puid);

#define SEMA_KERNEL_ATTR_MASK		0x0000a080	/* what kernel callers may pass */
#define SEMA_USER_ATTR_MASK		0x00002080	/* what a syscall may pass */
#define SEMA_OPTION_MAX_SIZE		4
#define UID_MAX_PER_PROCESS		0x800
#define UID_LW_MAX_PER_PROCESS		0x100
#define SEMA_UID_ATTR_LW		0x0000c000	/* PLACEHOLDER: name */
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */

typedef struct ThreadmgrSemaOptParam {
	SceSize size;
} ThreadmgrSemaOptParam;

/* closes every semaphore PUID a process still holds */
int semaphoreProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid,
		s_threadmgrRuntime.manager->semaphoreClass, PUID_VECTOR_LEVEL, NULL, 0,
		NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 344);
		num = SceSysmemForKernel_08C05493(process->pid,
			s_threadmgrRuntime.manager->semaphoreClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = ksceKernelCpuSuspendIntr();
		ret = kscePUIDClose(process->pid, vector[i]);
		ksceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 362, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(363);
		}
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 370);
	}
	return 0;
}

/* creates the object, counting it against the calling process's limits */
static inline __attribute__((always_inline)) SceUID sceKernelCreateSemaCore(const char *name,
	SceUInt32 attr, int initCount, int maxCount, const ThreadmgrSemaOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrSemaphore *sema;
	SceUID uid;
	int intr;

	thread = currentThreadContext();
	if (attr & SEMA_ATTR_LW) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = SEMA_UID_ATTR_LW;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = 0;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
				attr |= SEMA_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		}
	}
	if (!ThreadmgrInSyscall()) {
		intr = ksceKernelCpuSuspendIntr();
	} else {
		process = thread->process;
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & SEMA_ATTR_LW) {
			if (process->uidLwCount >= UID_LW_MAX_PER_PROCESS) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 451, "sceKernelCreateSemaCore",
					"sceKernelCreateSemaCore(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, "
					"processId=0x%08X\n", thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & SEMA_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= UID_MAX_PER_PROCESS) {
				ksceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 468, "sceKernelCreateSemaCore",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->semaCount > process->semaPeak)
			process->semaPeak = process->semaCount;
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->semaphoreClass,
		name, (SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)&sema);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			ksceKernelSpinlockLowLock(&process->resourceLock);
			if (attr & SEMA_ATTR_LW)
				process->uidLwCount--;
			else if (attr & SEMA_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->semaCount--;
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		sema->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
		sema->attr = attr;
		sema->option = option;
		sema->count = initCount;
		sema->initCount = initCount;
		sema->maxCount = maxCount;
		ksceKernelSpinlockLowUnlock(&sema->lock);	/* the class hands it over locked */
	}
resume:
	ksceKernelCpuResumeIntr(intr);
	return uid;
}

/*
 * The early errors return with TPIDRURO still shifted, as Sony's code does
 * (both create functions).
 */
SceUID ksceKernelCreateSema(const char *name, SceUInt32 attr, int initCount, int maxCount,
			    const ThreadmgrSemaOptParam *option)
{
	uint32_t saved;
	SceUID uid;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 551, "sceKernelCreateSema", "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~SEMA_USER_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 560, "sceKernelCreateSema", " k: error: sceKernelCreateSema(%s), attr=0x%08X\n",
				       name, attr);
			return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		}
	} else if (attr & ~SEMA_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF_FUNC(0, 568, "sceKernelCreateSema", " k: error: sceKernelCreateSema(%s), attr=0x%08X\n", name,
			       attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}
	if (maxCount < 1 || initCount < 0) {
		SCE_DBG_PRINTF_FUNC(0, 575, "sceKernelCreateSema",
			" k: error: sceKernelCreateSema(%s), initCount=%d maxCount=%d\n", name,
			initCount, maxCount);
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	}
	if (initCount > maxCount)
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	if (option != NULL && option->size > SEMA_OPTION_MAX_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	uid = sceKernelCreateSemaCore(name, attr, initCount, maxCount, option);
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

/* the create of 1.6x SDKs: maxCount 0 allowed, no syscall attribute check */
SceUID sceKernelCreateSema_16XX(const char *name, SceUInt32 attr, int initCount, int maxCount,
				const ThreadmgrSemaOptParam *option)
{
	uint32_t saved;
	SceUID uid;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 612, "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (attr & ~SEMA_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF(0, 621, " k: error: sceKernelCreateSema(%s), attr=0x%08X\n", name,
			       attr);
		return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
	}
	if (option != NULL && option->size > SEMA_OPTION_MAX_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	if ((initCount | maxCount) < 0 || initCount > maxCount)
		return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	uid = sceKernelCreateSemaCore(name, attr, initCount, maxCount, option);
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int ksceKernelDeleteSema(SceUID semaId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 661, "sceKernelDeleteSema", "*** error: illegal context. semaId=0x%08x[%s]\n",
				      semaId, ksceGUIDName(semaId));
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (SceSysmemForKernel_368F1C36(semaId) ==
	    s_threadmgrRuntime.manager->semaphoreClass)
		ret = ksceGUIDClose(semaId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForKernel_EFD7439D, SceThreadmgrForDriver_3C8B55A9 */
int ksceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	return waitSema(semaId, needCount, timeout, 0);
}

/* ksceKernelWaitSemaCB */
int SceThreadmgrForDriver_F55E4D86(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	return waitSema(semaId, needCount, timeout, 1);
}

int ksceKernelPollSema(SceUID semaId, int needCount)
{
	struct ThreadCB *thread;
	ThreadmgrSemaphore *sema;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (needCount < 1) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(semaId,
		s_threadmgrRuntime.manager->semaphoreClass, (SceObjectBase **)&sema);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = 0xc0028101;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 760, "sceKernelPollSema",
				"*** error: unknown semaphore id. semaId=0x%08x\n", semaId);
			ret = 0x80028101;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&sema->lock);
	if (sema->maxCount < needCount) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	} else {
		thread = currentThreadContext();
		if (!isWaitOnly((struct ThreadmgrWaitQueue *)&sema->next,
				thread != NULL ? thread->currentPriority : 1) &&
		    needCount <= sema->count) {
			sema->count -= needCount;
			ret = 0;
		} else {
			ret = SCE_KERNEL_ERROR_SEMA_ZERO;
		}
	}
	ksceKernelSpinlockLowUnlock(&sema->lock);
	ksceGUIDReleaseObject(semaId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelSignalSema(SceUID semaId, int signalCount)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrSemaphore *sema;
	uint32_t saved, wake;
	SceUInt32 sdkVersion;
	int intr, ret, newCount;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	thread = currentThreadContext();
	if (signalCount < 0) {
		ThreadmgrCallerSdkVersion(thread, &sdkVersion);
		if (sdkVersion >= SDK_VERSION_NO_NEGATIVE_SIGNAL) {
			SCE_DBG_PRINTF_FUNC(0, 836, "sceKernelSignalSema", "*** error: sceKernelSignalSema(0x%08X[%s])=0x%08X, "
				"signalCount=0x%08X\n", semaId, ksceGUIDName(semaId),
				SCE_KERNEL_ERROR_ILLEGAL_COUNT, signalCount);
			if (ThreadmgrInSyscall()) {
				ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT | SYSCALL_ERROR_BIT;
			} else {
				SCE_DBG_ASSERT_CHECK_FUNC(0, 840, "sceKernelSignalSema");
				ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
			}
			goto restore;
		}
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(semaId,
		s_threadmgrRuntime.manager->semaphoreClass, (SceObjectBase **)&sema);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = 0xc0028101;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 855, "sceKernelSignalSema",
				"*** error: unknown semaphore id. semaId=0x%08x\n", semaId);
			ret = 0x80028101;
		}
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)&sema->next;
	ksceKernelSpinlockLowLock(&sema->lock);
	newCount = sema->count + signalCount;
	if (newCount > sema->maxCount) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_SEMA_OVF | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 872, "sceKernelSignalSema");
			ret = SCE_KERNEL_ERROR_SEMA_OVF;
		}
		goto unlock;
	}
	if (newCount < 0) {
		ThreadmgrCallerSdkVersion(thread, &sdkVersion);
		if (sdkVersion >= SDK_VERSION_NO_NEGATIVE_SIGNAL) {
			SCE_DBG_PRINTF_FUNC(0, 886, "sceKernelSignalSema", "*** error: sceKernelSignalSema(0x%08X[%s])=0x%08X, "
				"newCount=0x%08X\n", semaId, ksceGUIDName(semaId),
				SCE_KERNEL_ERROR_SEMA_OVF, newCount);
			if (ThreadmgrInSyscall()) {
				ret = SCE_KERNEL_ERROR_SEMA_OVF | SYSCALL_ERROR_BIT;
			} else {
				SCE_DBG_ASSERT_CHECK_FUNC(0, 890, "sceKernelSignalSema");
				ret = SCE_KERNEL_ERROR_SEMA_OVF;
			}
			goto unlock;
		}
	}
	sema->count = newCount;
	ret = 0;
	if (queue->next == (struct ThreadmgrWaitNode *)queue)
		goto unlock;
	wake = 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		waiter = (struct ThreadCB *)node;
		SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 901, "sceKernelSignalSema");
		next = node->next;
		ksceKernelSpinlockLowLock(&waiter->lock);
		if (sema->count < waiter->waitMode) {
			ksceKernelSpinlockLowUnlock(&waiter->lock);
			break;
		}
		sema->count -= waiter->waitMode;
		wake |= wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
		ksceKernelSpinlockLowUnlock(&waiter->lock);
	}
	ksceKernelSpinlockLowUnlock(&sema->lock);
	ksceGUIDReleaseObject(semaId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(&sema->lock);
	ksceGUIDReleaseObject(semaId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* wakes every waiter with WAIT_CANCEL and sets the count (negative: back to initCount) */
int sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads)
{
	ThreadmgrSemaphore *sema;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(semaId,
		s_threadmgrRuntime.manager->semaphoreClass, (SceObjectBase **)&sema);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = 0xc0028101;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 974,
				"*** error: unknown semaphore id. semaId=0x%08x\n", semaId);
			ret = 0x80028101;
		}
		goto resume;
	}
	ksceKernelSpinlockLowLock(&sema->lock);
	if (sema->maxCount < setCount) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		ksceKernelSpinlockLowUnlock(&sema->lock);
		ksceGUIDReleaseObject(semaId);
		goto resume;
	}
	wake = wakeupWaitObjectAll((uintptr_t)&sema->next, SCE_KERNEL_ERROR_WAIT_CANCEL,
				   (uint32_t *)numWaitThreads);
	if (setCount < 0)
		sema->count = sema->initCount;
	else
		sema->count = setCount;
	ksceKernelSpinlockLowUnlock(&sema->lock);
	ksceGUIDReleaseObject(semaId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	ksceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int ksceKernelGetSemaInfo(SceUID semaId, ThreadmgrSemaInfo *info)
{
	ThreadmgrSemaInfo local;
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
		ret = getSemaInfo(semaId, &local);
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

/* the information of every semaphore visible at level */
int SceThreadmgrForDriver_C6741986(int level, ThreadmgrSemaInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrSemaInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->semaphoreClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->semaphoreClass, level, vector, num, &got);
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
		if (getSemaInfo(vector[i], &info) != 0)
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

/* the same for the semaphores a process holds PUIDs of */
int SceThreadmgrForDriver_477F7C8A(SceUID pid, int level, ThreadmgrSemaInfo *infos,
				     SceSize size, SceSize *count)
{
	ThreadmgrSemaInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid,
		s_threadmgrRuntime.manager->semaphoreClass, level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid,
			s_threadmgrRuntime.manager->semaphoreClass, level, vector, num, &got);
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
		if (getSemaInfo(kscePUIDtoGUID(pid, vector[i]), &info) != 0)
			continue;
		info.semaId = vector[i];
		if (scePUIDGetAttr(pid, vector[i], &attr) >= 0 && (attr & PUID_ATTR_OPENED))
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

/* SceUIDSemaphoreClass */
void startSemaphore(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = ksceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(301);
	ret = uidExtendedClassInit(cls, "SceUIDSemaphoreClass",
		s_threadmgrRuntime.manager->waitableClass, SEMA_ITEM_SIZE,
		uidSemaphore_Constructor, (SceClassCallback)uidSemaphore_Destructor,
		uidDefaultCancelWait, uidSemaphore_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1342, 1336), "uidSemaphoreClassInit");
	s_threadmgrRuntime.manager->semaphoreClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 317, " sizeof(SceUIDSemaphoreObject)=%d\n",
			     sizeof(ThreadmgrSemaphore));
}
