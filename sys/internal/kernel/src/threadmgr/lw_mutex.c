/*
 * SceKernelThreadMgr: lw_mutex.c, the lightweight mutexes (SceUIDLwMutexClass).
 * User code takes an uncontended lw mutex in its work area alone; the kernel
 * is entered only to wait or to hand the lock to a waiter, through the
 * SceCpu work-area primitives (trylock, unlock, transfer).
 */

#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "lw_mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>

#define LW_MUTEX_ITEM_SIZE		0x80	/* the class's object allocation size */
#define LW_MUTEX_ATTR_MASK		0x00002002
#define LW_MUTEX_OPTION_MAX_SIZE	4
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define WAIT_TYPE_LW_MUTEX		0x00100000
#define WAIT_TYPE_LW_COND_MUTEX		0x00400000	/* a lw cond waiter relocking its lw mutex */
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define USER_THREAD_ID_KERNEL_FLAG	0x40000000	/* PLACEHOLDER: an owner that is no user thread */
#define SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT 0x80020007
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_ILLEGAL_COUNT	0x8002000f
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID 0x80028181
#define SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE 0x80028182
#define SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF 0x80028183
#define SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF 0x80028184
#define SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED 0x80028186
#define SCE_KERNEL_ERROR_LW_MUTEX_WORK	0x80028187	/* PLACEHOLDER: name; the work area is unreadable */
#define SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX 0x800281c2
#define UID_CREATE_OPT_PID		0x00000008	/* PLACEHOLDER: name; opt.pid owns the object */

typedef struct ThreadmgrLwMutexOptParam {
	SceSize size;
} ThreadmgrLwMutexOptParam;

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern SceUID SceSysmemForKernel_03A2CAF0(void *entry);		/* PLACEHOLDER: reuse a GUID entry */
extern int SceSysmemForKernel_FF464A49(SceUID guid, void **entry);	/* sceGUIDGetEntry */
extern int SceCpuForKernel_75A85059(ThreadmgrLwMutexWork *work, SceUID owner);	/* trylockLwMutex */
extern int SceCpuForKernel_FE9ED689(ThreadmgrLwMutexWork *work);			/* unlockLwMutex */
extern int SceCpuForKernel_9B071FD3(ThreadmgrLwMutexWork *work, SceUID owner, int waiters,
				    int count);					/* transferLwMutex */
int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

/* SceUIDLwMutexClass's constructor */
static int uidLwMutex_Constructor(void *object)
{
	return 0;
}

/* SceUIDLwMutexClass's destructor */
static int uidLwMutex_Destructor(void *object)
{
	ThreadmgrLwMutex *lwMutex = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1920);
	memset(&lwMutex->guid, 0, sizeof(*lwMutex) - offsetof(ThreadmgrLwMutex, guid));
	return 0;
}

/* a lw mutex's information, its count and owner read from the work area */
static int getLwMutexInfo(SceUID lwMutexId, ThreadmgrLwMutexInfo *info)
{
	ThreadmgrLwMutexWork work;
	ThreadmgrLwMutex *lwMutex;
	SceUID owner;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, (SceObjectBase **)&lwMutex);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT :
					     SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
		goto resume;
	}
	if (lwMutex->cacheNext != &lwMutex->uidEntry) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
		goto release;
	}
	info->uid = lwMutexId;
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	info->numWaitThreads = lwMutex->waiting;
	info->initCount = lwMutex->initCount;
	info->work = lwMutex->work;
	info->attr = lwMutex->attr;
	strncpy(info->name, lwMutex->name, sizeof(info->name));
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ret = ksceKernelCopyFromUserProc(lwMutex->pid, &work, lwMutex->work, sizeof(work));
	if (ret < 0) {
		info->currentCount = -1;
		info->currentOwnerId = -1;
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_WORK | SYSCALL_ERROR_BIT;
		} else {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_WORK;
			info->work = (ThreadmgrLwMutexWork *)((uintptr_t)info->work | 1);
		}
	} else {
		if (work.owner == 0) {
			info->currentOwnerId = 0;
		} else {
			owner = kscePUIDtoGUID(lwMutex->pid, work.owner);
			info->currentOwnerId = owner < 0 ? work.owner : owner;
		}
		ret = 0;
		info->currentCount = work.lockCount;
	}
release:
	ksceGUIDReleaseObject(lwMutexId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDLwMutexClass's revalidation: nothing to hand over here (the work area decides) */
static int uidLwMutex_RevalidateMethod(void *cls, void *object)
{
	ThreadmgrLwMutex *lwMutex = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1944);
	SCE_DBG_ASSERT_CHECK(lwMutex->lock != 0, 1947);
	return 0;
}

/* SceUIDLwMutexClass */
void startLwMutex(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = ksceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(92);
	ret = uidExtendedClassInit(cls, "SceUIDLwMutexClass",
		s_threadmgrRuntime.manager->waitableClass, LW_MUTEX_ITEM_SIZE,
		uidLwMutex_Constructor, (SceClassCallback)uidLwMutex_Destructor,
		uidDefaultCancelWait, uidLwMutex_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(1890, "uidLwMutexClassInit");
	s_threadmgrRuntime.manager->lwMutexClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 108, " sizeof(SceUIDLwMutexObject)=%d\n",
			     sizeof(ThreadmgrLwMutex));
}

/* closes every lw mutex a process still holds, PUID and GUID */
int lwMutexProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	SceUID puid, guid;
	int num, i, intr, ret;

	num = SceSysmemForKernel_08C05493(process->pid,
		s_threadmgrRuntime.manager->lwMutexClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 143);
		num = SceSysmemForKernel_08C05493(process->pid,
			s_threadmgrRuntime.manager->lwMutexClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		puid = vector[i];
		guid = kscePUIDtoGUID(process->pid, puid);
		if (guid < 0) {
			SCE_DBG_PRINTF(0xb, 159, "()->scePUIDtoGUID(0x%08X, 0x%08X)=0x%08X\n",
				       process->pid, puid, guid);
			continue;
		}
		intr = ksceKernelCpuSuspendIntr();
		ret = kscePUIDClose(process->pid, puid);
		ksceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 166, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, puid, ret);
			SCE_PANIC(167);
		}
		intr = ksceKernelCpuSuspendIntr();
		ret = ksceGUIDClose(guid);
		ksceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 174, "()->sceGUIDClose(guid=0x%08X)=0x%08X\n", guid, ret);
			SCE_PANIC(175);
		}
	}
	if (vector != NULL) {
		ret = ksceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 182);
	}
	return 0;
}

/* returns an object to its process's cache (both locks free of it) */
static inline void LwMutexCachePush(SceProcessObject *process, ThreadmgrLwMutex *lwMutex)
{
	ksceKernelSpinlockLowLock(&process->lwMutexCacheLock);
	lwMutex->cacheNext = process->lwMutexCache;
	process->lwMutexCache = lwMutex;
	process->lwMutexCacheCount++;
	ksceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
}

/*
 * The lw mutex create syscall's body: an object is taken from the process's
 * cache (or made: uidLwMutex_AllocObject, inlined) and the work area is
 * initialised. Sony returns the name copy's length when the PUID cannot be
 * opened (after suspending the thread for the debugger); kept.
 */
int _sceKernelCreateLwMutexForUser(ThreadmgrLwMutexWork *work, const char *name, SceUInt32 attr,
				   int initCount, const ThreadmgrLwMutexOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process, *cacheProcess;
	UIDCreateOpt opt;
	ThreadmgrLwMutexWork userWork;
	ThreadmgrLwMutex *lwMutex, *old;
	char newName[32];
	volatile int delay;
	uint32_t saved;
	SceUID guid, uid;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 238, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		goto restore;
	}
	if ((uintptr_t)work & 7) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT | SYSCALL_ERROR_BIT;
		goto restore;
	}
	if (attr & ~LW_MUTEX_ATTR_MASK) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > LW_MUTEX_OPTION_MAX_SIZE) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (initCount < 0 || (initCount > 1 && !(attr & LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	if (name == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	process = thread->process;
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceKernelCopyFromUser(&userWork, work, sizeof(userWork));
	if (ret < 0) {
		ret |= SYSCALL_ERROR_BIT;
		goto resume;
	}
	guid = SceSysmemForKernel_188C8742(0, userWork.uid);
	if (guid >= 0 && ksceGUIDReferObjectWithClass(guid,
			s_threadmgrRuntime.manager->lwMutexClass,
			(SceObjectBase **)&old) >= 0) {
		ThreadmgrLwMutexWork *oldWork = old->work;

		ksceGUIDReleaseObject(guid);
		if (work == oldWork) {
			if (ksceKernelStrncpyFromUser(newName, name, sizeof(newName)) < 0)
				newName[0] = '\0';
			SCE_PRINTF_WITH_INFO(0, 314, "*** error: sceKernelCreateLwMutex(), Illegal reuse "
				"of \"pWork\" address.\n***      :  processId=0x%08X, pWork=%p, old "
				"uid=0x%08X, old name=[%s], new name=[%s]\n", thread->processId, work,
				userWork.uid, old->name, newName);
		}
	}
	ksceKernelSpinlockLowLock(&process->resourceLock);
	if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) {
		attr |= LW_MUTEX_ATTR_UNLIMITED;
		if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
			process->uidUnlimitedPeak = process->uidUnlimitedCount;
	} else {
		if (process->uidCount >= 0x800) {
			ksceKernelSpinlockLowUnlock(&process->resourceLock);
			SCE_DBG_PRINTF(0xb, 334, "(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
				       thread->processId);
			ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
			goto resume;
		}
		if (++process->uidCount > process->uidPeak)
			process->uidPeak = process->uidCount;
	}
	if (++process->lwMutexCount > process->lwMutexPeak)
		process->lwMutexPeak = process->lwMutexCount;
	ksceKernelSpinlockLowUnlock(&process->resourceLock);

	/* uidLwMutex_AllocObject */
	SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 2099, "uidLwMutex_AllocObject");
	cacheProcess = thread->process;
	for (;;) {
		ksceKernelSpinlockLowLock(&cacheProcess->lwMutexCacheLock);
		if (cacheProcess->lwMutexCacheCount <= 0)
			break;
		lwMutex = cacheProcess->lwMutexCache;
		if (ksceKernelSpinlockLowTrylock(&lwMutex->lock) >= 0) {
			cacheProcess->lwMutexCache = lwMutex->cacheNext;
			cacheProcess->lwMutexCacheCount--;
			ksceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
			lwMutex->cacheNext = &lwMutex->uidEntry;
			uid = SceSysmemForKernel_03A2CAF0(lwMutex->uidEntry);
			if (uid >= 0)
				goto allocated;
			ret = uid;
			goto unaccount;
		}
		ksceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
		for (delay = 10; --delay != 0;)
			;
	}
	ksceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
	opt.flags = UID_CREATE_OPT_PID;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.pid = cacheProcess->pid;
	opt.heapKind = 0;
	opt.type = 0;
	opt.attr = 0;
	opt.ownerPid = 0;
	opt.cls = NULL;
	uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->lwMutexClass,
		"SceKernelLwMutexCache", (SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)&lwMutex);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0, 2139, "uidLwMutex_AllocObject", "*** error: "
			"uidLwMutex_AllocObject() -> sceGUIDKernelCreateWithAttr()=0x%08X\n", uid);
		ret = uid;
		goto unaccount;
	}
	lwMutex->ownerPid = cacheProcess->pid;
	ret = SceSysmemForKernel_FF464A49(uid, &lwMutex->uidEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0, 2150, "uidLwMutex_AllocObject", "*** error: "
			"uidLwMutex_AllocObject() -> sceGUIDGetEntry(0x%08X)=0x%08X\n", uid, ret);
		ksceKernelSpinlockLowUnlock(&lwMutex->lock);
		if (ksceGUIDClose(uid) < 0)
			SCE_PANIC_FUNC(2153, "uidLwMutex_AllocObject");
		goto unaccount;
	}
	SCE_DBG_ASSERT_CHECK_FUNC(lwMutex->uidEntry != NULL, 2158, "uidLwMutex_AllocObject");
allocated:
	lwMutex->attr = attr;
	lwMutex->guid = uid;
	lwMutex->conds.next = &lwMutex->conds;
	lwMutex->conds.prev = &lwMutex->conds;
	lwMutex->initCount = initCount;
	lwMutex->work = work;
	lwMutex->pid = thread->processId;
	ret = ksceKernelStrncpyFromUser(lwMutex->name, name, sizeof(lwMutex->name));
	if (ret < 0)
		goto recache;
	lwMutex->name[sizeof(lwMutex->name) - 1] = '\0';
	userWork.owner = initCount != 0 ? thread->userThreadId : 0;
	userWork.reserved0 = 0;
	userWork.lockCount = initCount;
	userWork.attr = attr;
	userWork.uid = kscePUIDOpenByGUID(0, uid);
	if (userWork.uid < 0) {
		LwMutexCachePush(process, lwMutex);
		ksceKernelSpinlockLowUnlock(&lwMutex->lock);
		ksceKernelSpinlockLowLock(&process->resourceLock);
		if (attr & LW_MUTEX_ATTR_UNLIMITED)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->lwMutexCount--;
		ksceKernelSpinlockLowUnlock(&process->resourceLock);
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		goto resume;
	}
	userWork.reserved1[0] = 0;
	userWork.reserved1[1] = 0;
	userWork.reserved1[2] = 0;
	ret = ksceKernelCopyToUser(work, &userWork, sizeof(userWork));
	if (ret < 0) {
		if (kscePUIDClose(0, userWork.uid) >= 0) {
			ret |= SYSCALL_ERROR_BIT;
			goto recache;
		}
		ksceKernelSpinlockLowUnlock(&lwMutex->lock);
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		goto resume;
	}
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ret = 0;
	goto resume;
recache:
	LwMutexCachePush(process, lwMutex);
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
unaccount:
	ksceKernelSpinlockLowLock(&process->resourceLock);
	if (attr & LW_MUTEX_ATTR_UNLIMITED)
		process->uidUnlimitedCount--;
	else
		process->uidCount--;
	process->lwMutexCount--;
	ksceKernelSpinlockLowUnlock(&process->resourceLock);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Deletes a lw mutex by its work area: bound lw conds' waiters wake with
 * WAIT_DELETE_LW_MUTEX, its own with WAIT_DELETE, and the object goes back
 * to the process's cache.
 */
int sceKernelDeleteLwMutexForUser(ThreadmgrLwMutexWork *work)
{
	ListNode *link, *next;
	SceProcessObject *process;
	ThreadmgrLwCond *cond;
	ThreadmgrLwMutex *lwMutex;
	struct ThreadmgrWaitQueue *queue;
	volatile int delay;
	uint32_t saved, attr, wake, wakeOwn;
	SceUID puid, guid;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 543, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = ksceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 553, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &puid, &work->uid, sizeof(puid), ret);
		goto restore;
	}
	guid = kscePUIDtoGUID(0, puid);
	if (guid < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   (SceObjectBase **)&lwMutex);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		ksceGUIDReleaseObject(guid);
		goto resume;
	}
	wake = 0;
retry:
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	for (link = lwMutex->conds.next; link != &lwMutex->conds; link = next) {
		next = link->next;
		cond = LW_COND_FROM_MUTEX_LINK(link);
		if (ksceKernelSpinlockLowTrylock(&cond->lock) < 0) {
			/* the cond holds its lock while it takes ours: back off */
			ksceKernelSpinlockLowUnlock(&lwMutex->lock);
			for (delay = 10; --delay != 0;)
				;
			goto retry;
		}
		wake |= wakeupWaitObjectAll((uintptr_t)&cond->next,
					    SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX, NULL);
		cond->lwMutexId = -1;
		ksceKernelSpinlockLowUnlock(&cond->lock);
	}
	queue = (struct ThreadmgrWaitQueue *)&lwMutex->next;
	ret = kscePUIDClose(0, puid);
	wakeOwn = wakeupWaitObjectAll((uintptr_t)queue, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	process = currentThreadContext()->process;
	LwMutexCachePush(process, lwMutex);
	attr = lwMutex->attr;
	lwMutex->waiting = 0;
	lwMutex->next = queue;
	lwMutex->attr = 0;
	lwMutex->reserved = 0;
	lwMutex->prev = queue;
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ksceKernelSpinlockLowLock(&process->resourceLock);
	if (attr & LW_MUTEX_ATTR_UNLIMITED)
		process->uidUnlimitedCount--;
	else
		process->uidCount--;
	process->lwMutexCount--;
	ksceKernelSpinlockLowUnlock(&process->resourceLock);
	ksceGUIDReleaseObject(guid);
	if ((wake | wakeOwn) & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* whether a thread may try the work area now (the queue is only looked at) */
#define LW_MUTEX_PEEK_FIRST(lwMutex, line) do {						\
	struct ThreadCB *__first;						\
											\
	if ((lwMutex)->waiting != 0) {							\
		__first = (struct ThreadCB *)(lwMutex)->next;		\
		SCE_DBG_ASSERT_CHECK(__first->magic == THREAD_CB_MAGIC, line);	\
		if ((lwMutex)->attr & LW_MUTEX_ATTR_PRIORITY) {				\
			ksceKernelSpinlockLowLock(THREAD_LOCK(__first));		\
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(__first));		\
		}									\
	}										\
} while (0)

/* the lw mutex lock syscall's body: the work area is tried, else the thread waits */
int _sceKernelLockLwMutex(ThreadmgrLwMutexWork *work, int count, SceUInt32 *timeout,
			  int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrLwMutexWork userWork;
	ThreadmgrLwMutex *lwMutex;
	uint64_t wakeup;
	uint32_t saved;
	SceUID puid, guid, self;
	int intr, ret, cbRet, locked = 0;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 715, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = ksceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 724, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &puid, &work->uid, sizeof(puid), ret);
		goto restore;
	}
	guid = kscePUIDtoGUID(0, puid);
	if (guid < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	self = currentThreadContext()->userThreadId;
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 742, "*** error: irq suspended. pWork=%p\n", work);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_CHECK_STACK) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = ksceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   (SceObjectBase **)&lwMutex);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		goto release;
	}
	ret = ksceKernelCopyFromUser(&userWork, work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 779, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (userWork.owner == self) {
		if (!(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE)) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		} else if ((count += userWork.lockCount) < 0) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		} else {
			ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 812, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			else
				ret = 0;
		}
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&lwMutex->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&lwMutex->lock);
			if (lwMutex->cacheNext != &lwMutex->uidEntry) {
				ret = SCE_KERNEL_ERROR_WAIT_DELETE;
				goto unlock;
			}
		}
		LW_MUTEX_PEEK_FIRST(lwMutex, 862);
		ret = SceCpuForKernel_75A85059(lwMutex->work, self);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 880, "trylockLwMutex(%p, 0x%08x): error 0x%08x\n",
				       lwMutex->work, self, ret);
			goto unlock;
		}
		if (ret == 0) {
			ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 890, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			goto unlock;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		if (!callbacks) {
			ret = waitWaitableObject(guid, (uintptr_t)&lwMutex->next, WAIT_TYPE_LW_MUTEX,
						 &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(guid, (uintptr_t)&lwMutex->next, WAIT_TYPE_LW_MUTEX, &wakeup,
					 timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ksceGUIDReleaseObject(guid);
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
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	ksceGUIDReleaseObject(guid);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The lw mutex unlock syscall's body: at zero the work area is unlocked, or
 * handed straight to the first waiter (transferLwMutex) with the waiter
 * count that remains.
 */
int _sceKernelUnlockLwMutex(ThreadmgrLwMutexWork *work, int count)
{
	struct ThreadCB *first;
	ThreadmgrLwMutexWork userWork;
	ThreadmgrLwMutex *lwMutex;
	uint32_t saved, wake;
	SceUID puid, guid, self, newOwner;
	int intr, ret, waiters, firstCount;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 988, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = ksceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0)
		goto restore;
	guid = kscePUIDtoGUID(0, puid);
	if (guid < 0) {
		SCE_DBG_PRINTF(0, 1004, "*** error: _sceKernelUnlockLwMutex(%p, 0x%08X)="
			"SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID, lwMutexPuid=0x%08X\n", work, count, puid);
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	self = currentThreadContext()->userThreadId;
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 1013, "*** error: irq suspended. pWork=%p\n", work);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	ret = ksceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   (SceObjectBase **)&lwMutex);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		goto release;
	}
	ret = ksceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1038, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (userWork.owner != self) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
		goto unlock;
	}
	if (userWork.lockCount < count) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
		goto unlock;
	}
	userWork.lockCount -= count;
	if (userWork.lockCount > 0) {
		ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &userWork.lockCount,
					   sizeof(userWork.lockCount));
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 1072, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error 0x%08x\n",
				       &lwMutex->work->lockCount, &userWork.lockCount,
				       sizeof(userWork.lockCount), ret);
		else
			ret = 0;
		goto unlock;
	}
	first = (struct ThreadCB *)lwMutex->next;
	if ((struct ThreadmgrWaitQueue *)first == (struct ThreadmgrWaitQueue *)&lwMutex->next) {
		ret = SceCpuForKernel_FE9ED689(lwMutex->work);
		if (ret >= 0)
			ret = 0;
		else
			SCE_DBG_PRINTF(0xf, 1118, "unlockLwMutex(%p): error 0x%08x\n", lwMutex->work, ret);
		goto unlock;
	}
	SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 1083);
	ksceKernelSpinlockLowLock(THREAD_LOCK(first));
	newOwner = first->userThreadId;
	SCE_DBG_ASSERT_CHECK(newOwner >= 0, 1090);
	firstCount = first->waitMode;
	waiters = lwMutex->waiting - 1;
	SCE_DBG_ASSERT_CHECK(firstCount > 0, 1095);
	ret = SceCpuForKernel_9B071FD3(lwMutex->work, newOwner, waiters, firstCount);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1100, "transferLwMutex(%p, 0x%08x, 0x%08x, 0x%08x): error 0x%08x\n",
			       lwMutex->work, newOwner, waiters, firstCount, ret);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(first));
		goto unlock;
	}
	__sync_synchronize();
	wake = wakeupAndDistributeThread(first, (uintptr_t)&lwMutex->next, 0);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ksceGUIDReleaseObject(guid);
	ret = 0;
	if (wake & 1)
		switchThread(0);
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	ksceGUIDReleaseObject(guid);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Sony's sceKernelGetLwMutexInfoById: an unreadable work area still gives
 * the rest of the information.
 */
int ksceKernelGetLwMutexInfo(SceUID lwMutexId, ThreadmgrLwMutexInfo *info)
{
	struct ThreadmgrThreadObject *owner;
	ThreadmgrLwMutexInfo local;
	uint32_t saved;
	SceSize size;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	ret = getLwMutexInfo(lwMutexId, &local);
	if (ret != SCE_KERNEL_ERROR_LW_MUTEX_WORK) {
		if (ret < 0)
			goto restore;
		if (ThreadmgrInSyscall() && local.currentOwnerId > 0 &&
		    !(local.currentOwnerId & USER_THREAD_ID_KERNEL_FLAG)) {
			ret = ksceGUIDGetObject(local.currentOwnerId, &owner);
			if (ret < 0) {
				SCE_DBG_PRINTF_FUNC(0, 1287, "sceKernelGetLwMutexInfoById", "*** error: sceKernelGetLwMutexInfoById() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", local.currentOwnerId, ret);
				goto restore;
			}
			local.currentOwnerId = owner->context.userThreadId;
		}
	}
	size = info->size;
	if (size > sizeof(local))
		size = sizeof(local);
	memcpy(info, &local, size);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_C2DA6286: the information of every lw mutex visible at level */
int SceThreadmgrForDriver_C2DA6286(int level, ThreadmgrLwMutexInfo *infos, SceSize size,
				   SceSize *count)
{
	ThreadmgrLwMutexInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret, err;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = ksceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->lwMutexClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->lwMutexClass, level, vector, num, &got);
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
		err = getLwMutexInfo(vector[i], &info);
		if (err != 0 && err != SCE_KERNEL_ERROR_LW_MUTEX_WORK)
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

/* the same for the lw mutexes a process holds PUIDs of, with user-visible IDs */
int sceKernelGetLwMutexInfoAllProc(SceUID pid, int level, ThreadmgrLwMutexInfo *infos,
				   SceSize size, SceSize *count)
{
	struct ThreadmgrThreadObject *owner;
	ThreadmgrLwMutexInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret, err;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = SceSysmemForKernel_08C05493(pid,
		s_threadmgrRuntime.manager->lwMutexClass, level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = SceSysmemForKernel_08C05493(pid,
			s_threadmgrRuntime.manager->lwMutexClass, level, vector, num, &got);
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
		err = getLwMutexInfo(kscePUIDtoGUID(pid, vector[i]), &info);
		if (err != 0 && err != SCE_KERNEL_ERROR_LW_MUTEX_WORK)
			continue;
		info.uid = vector[i];
		if (info.currentOwnerId > 0 && !(info.currentOwnerId & USER_THREAD_ID_KERNEL_FLAG)) {
			err = ksceGUIDGetObject(info.currentOwnerId, &owner);
			if (err < 0)
				SCE_DBG_PRINTF(0, 1475, "*** error: sceKernelGetLwMutexInfoAllProc() -> "
					"sceGUIDGetObject(0x%08X)=0x%08X\n", info.currentOwnerId, err);
			else
				info.currentOwnerId = owner->context.userThreadId;
		}
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
 * A lw cond waiter relocking its lw mutex. Sony returns with interrupts
 * still off and the object referenced when the work area cannot be read;
 * kept.
 */
int lockLwMutexForLwCond(SceUID lwMutexId, int count, SceUInt32 *timeout, int callbacks,
			 SceUID condId)
{
	struct ThreadCB *thread;
	ThreadmgrLwMutexWork userWork;
	ThreadmgrLwMutex *lwMutex;
	uint64_t wakeup;
	SceUID self;
	int intr, ret, locked = 0;

	thread = currentThreadContext();
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 1537, "*** error: illegal context. lwMutexId=0x%08x[%s]\n",
				      lwMutexId, ksceGUIDName(lwMutexId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	self = currentThreadContext()->userThreadId;
	intr = ksceKernelCpuSuspendIntr();
	if ((intr & 0x80) || currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	ret = ksceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, (SceObjectBase **)&lwMutex);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	ret = ksceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1570, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		return ret;
	}
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (userWork.owner == self) {
		if (!(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE)) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		} else if ((count += userWork.lockCount) < 0) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		} else {
			ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 1601, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			else
				ret = 0;
		}
		goto unlock;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&lwMutex->lock);
			callCallbackAll(thread);
			ksceKernelSpinlockLowLock(&lwMutex->lock);
		}
		LW_MUTEX_PEEK_FIRST(lwMutex, 1640);
		ret = SceCpuForKernel_75A85059(lwMutex->work, self);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1658, "trylockLwMutex(%p, 0x%08x): error 0x%08x\n",
				       lwMutex->work, self, ret);
			goto unlock;
		}
		if (ret == 0) {
			ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 1668, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			goto unlock;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		thread->waitCondId = condId;
		thread->waitMutexId = lwMutexId;
		if (!callbacks) {
			ret = waitWaitableObject(lwMutexId, (uintptr_t)&lwMutex->next,
				WAIT_TYPE_LW_COND_MUTEX, &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(lwMutexId, (uintptr_t)&lwMutex->next, WAIT_TYPE_LW_COND_MUTEX,
					 &wakeup, timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto deleted;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ksceGUIDReleaseObject(lwMutexId);
deleted:
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE && callbacks &&
	    (thread->flags & THREAD_FLAG_CALLBACKS))
		callCallbackAll(thread);
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
	ksceGUIDReleaseObject(lwMutexId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* a lw cond waiter giving up its lw mutex before it sleeps (interrupts already off) */
int unlockLwMutexForLwCond(SceUID lwMutexId, int count)
{
	struct ThreadCB *first;
	ThreadmgrLwMutexWork userWork;
	ThreadmgrLwMutex *lwMutex;
	SceUID self, newOwner;
	int ret, waiters, firstCount;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1752);
	self = currentThreadContext()->userThreadId;
	ret = ksceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, (SceObjectBase **)&lwMutex);
	if (ret < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
	ret = ksceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1770, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	ksceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	} else if (userWork.owner != self) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
	} else if (userWork.lockCount < count) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
	} else if ((userWork.lockCount -= count) > 0) {
		ret = ksceKernelCopyToUser(&lwMutex->work->lockCount, &userWork.lockCount,
					   sizeof(userWork.lockCount));
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 1802, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error 0x%08x\n",
				       &lwMutex->work->lockCount, &userWork.lockCount,
				       sizeof(userWork.lockCount), ret);
		else
			ret = 0;
	} else if ((struct ThreadmgrWaitQueue *)lwMutex->next ==
		   (struct ThreadmgrWaitQueue *)&lwMutex->next) {
		ret = SceCpuForKernel_FE9ED689(lwMutex->work);
		if (ret >= 0)
			ret = 0;
		else
			SCE_DBG_PRINTF(0xf, 1848, "unlockLwMutex(%p): error 0x%08x\n", lwMutex->work,
				       ret);
	} else {
		first = (struct ThreadCB *)lwMutex->next;
		SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 1813);
		ksceKernelSpinlockLowLock(THREAD_LOCK(first));
		newOwner = first->userThreadId;
		SCE_DBG_ASSERT_CHECK(newOwner >= 0, 1820);
		firstCount = first->waitMode;
		waiters = lwMutex->waiting - 1;
		SCE_DBG_ASSERT_CHECK(firstCount > 0, 1825);
		ret = SceCpuForKernel_9B071FD3(lwMutex->work, newOwner, waiters, firstCount);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1830, "transferLwMutex(%p, 0x%08x, 0x%08x, 0x%08x): error "
				"0x%08x\n", lwMutex->work, newOwner, waiters, firstCount, ret);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(first));
		} else {
			__sync_synchronize();
			wakeupAndDistributeThread(first, (uintptr_t)&lwMutex->next, 0);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(first));
			ret = 0;
		}
	}
	ksceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	ksceGUIDReleaseObject(lwMutexId);
	return ret;
}

/* fills a process's cache with fresh objects up to target */
int uidLwMutex_CreateCache(SceProcessObject *process, int target)
{
	UIDCreateOpt opt;
	ThreadmgrLwMutex *lwMutex;
	bool more = true;
	SceUID uid;
	int intr, ret;

	do {
		opt.pid = process->pid;
		opt.flags = UID_CREATE_OPT_PID;
		opt.object = NULL;
		opt.heapGuid = 0;
		opt.heapKind = 0;
		opt.type = 0;
		opt.attr = 0;
		opt.ownerPid = 0;
		opt.cls = NULL;
		intr = ksceKernelCpuSuspendIntr();
		uid = ksceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->lwMutexClass,
			"SceKernelLwMutexCache", (SceGUIDKernelCreateOpt *)&opt,
			(SceObjectBase **)&lwMutex);
		if (uid < 0) {
			ksceKernelCpuResumeIntr(intr);
			SCE_DBG_PRINTF(0, 2013, "*** error: uidLwMutex_CreateCache() -> "
				"sceGUIDKernelCreateWithAttr()=0x%08X\n", uid);
			return uid;
		}
		lwMutex->ownerPid = process->pid;
		ret = SceSysmemForKernel_FF464A49(uid, &lwMutex->uidEntry);
		if (ret < 0) {
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&lwMutex->lock, intr);
			SCE_DBG_PRINTF(0, 2026, "*** error: uidLwMutex_CreateCache() -> "
				"sceGUIDGetEntry(0x%08X)=0x%08X\n", uid, ret);
			if (ksceGUIDClose(uid) < 0)
				SCE_PANIC(2028);
		} else {
			SCE_DBG_ASSERT_CHECK(lwMutex->uidEntry != NULL, 2032);
			ksceKernelSpinlockLowLock(&process->lwMutexCacheLock);
			lwMutex->cacheNext = process->lwMutexCache;
			process->lwMutexCache = lwMutex;
			if (++process->lwMutexCacheCount >= target)
				more = false;
			ksceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&lwMutex->lock, intr);
		}
	} while (more);
	return ret;
}

/* trims a process's cache down to target */
int uidLwMutex_DeleteCache(SceProcessObject *process, int target)
{
	ThreadmgrLwMutex *lwMutex;
	volatile int delay;
	int intr, ret;

	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
		if (process->lwMutexCacheCount <= target)
			break;
		for (;;) {
			lwMutex = process->lwMutexCache;
			if (ksceKernelSpinlockLowTrylock(&lwMutex->lock) < 0)
				goto backoff;
			process->lwMutexCache = lwMutex->cacheNext;
			process->lwMutexCacheCount--;
			ksceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
			ksceKernelSpinlockLowUnlock(&lwMutex->lock);
			ret = ksceGUIDClose(SceSysmemForKernel_03A2CAF0(lwMutex->uidEntry));
			SCE_DBG_ASSERT_CHECK(ret >= 0, 2075);
			ksceKernelCpuResumeIntr(intr);
			intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
			if (process->lwMutexCacheCount <= target)
				goto done;
		}
backoff:
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
		for (delay = 10; --delay != 0;)
			;
	}
done:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
	return 0;
}

/* keeps a process's cache between low and high (refilled or trimmed to target) */
int uidLwMutex_ManageCache(SceProcessObject *process, int high, int low, int target)
{
	int intr, count, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
	count = process->lwMutexCacheCount;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
	if (count < low) {
		ret = uidLwMutex_CreateCache(process, target);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 1976, "*** error: uidLwMutex_ManageCache() -> "
				"uidLwMutex_CreateCache()=0x%08X\n", ret);
	} else if (high < count) {
		ret = uidLwMutex_DeleteCache(process, target);
		if (ret < 0)
			SCE_DBG_PRINTF(0, 1983, "*** error: uidLwMutex_ManageCache() -> "
				"uidLwMutex_DeleteCache()=0x%08X\n", ret);
	} else {
		ret = 0;
	}
	return ret;
}
