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
#define USER_THREAD_ID_KERNEL_FLAG	0x40000000	/* PLACEHOLDER: an owner that is no user thread */
#define UID_CREATE_OPT_PID		0x00000008	/* PLACEHOLDER: name; opt.pid owns the object */

/* SceUIDLwMutexClass's constructor */
static int uidLwMutex_Constructor(void *object)
{
	return 0;
}

/* SceUIDLwMutexClass's destructor */
static int uidLwMutex_Destructor(void *object)
{
	SceUIDLwMutexObject *lwMutex = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1920);
	memset(&lwMutex->guid, 0, sizeof(*lwMutex) - offsetof(SceUIDLwMutexObject, guid));
	return 0;
}

/* a lw mutex's information, its count and owner read from the work area */
static int getLwMutexInfo(SceUID lwMutexId, SceKernelLwMutexInfo *info)
{
	ThreadmgrLwMutexWork work;
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
	SceUID owner;
	int intr, ret;

	*info = (SceKernelLwMutexInfo){ .size = sizeof(*info) };
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, &pObject);
	if (ret < 0) {
		ret = !ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID :
			SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	lwMutex = (SceUIDLwMutexObject *)pObject;
	if (lwMutex->cacheNext != &lwMutex->uidEntry) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
		goto release;
	}
	info->uid = lwMutexId;
	sceKernelSpinlockLowLock(&lwMutex->lock);
	info->numWaitThreads = lwMutex->waiting;
	info->initCount = lwMutex->initCount;
	info->pWork = (SceKernelLwMutexWork *)lwMutex->work;
	info->attr = lwMutex->attr;
	strncpy(info->name, lwMutex->name, sizeof(info->name));
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	ret = sceKernelCopyFromUserProc(lwMutex->pid, &work, lwMutex->work, sizeof(work));
	if (ret < 0) {
		info->currentCount = -1;
		info->currentOwnerId = -1;
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_CAN_NOT_ACCESS | SYSCALL_ERROR_BIT;
		} else {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_CAN_NOT_ACCESS;
			info->pWork = (SceKernelLwMutexWork *)((uintptr_t)info->pWork | 1);
		}
	} else {
		if (work.owner == 0) {
			info->currentOwnerId = 0;
		} else {
			owner = SceSysmemForDriver_45D22597(lwMutex->pid, work.owner);
			info->currentOwnerId = owner < 0 ? work.owner : owner;
		}
		ret = 0;
		info->currentCount = work.lockCount;
	}
release:
	sceGUIDReleaseObject(lwMutexId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDLwMutexClass's revalidation: nothing to hand over here (the work area decides) */
static int uidLwMutex_RevalidateMethod(void *cls, void *object)
{
	SceUIDLwMutexObject *lwMutex = object;

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

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(92);
	ret = uidWaitableClassInit(cls, "SceUIDLwMutexClass",
		s_threadmgrRuntime.manager->waitableClass, LW_MUTEX_ITEM_SIZE,
		uidLwMutex_Constructor, (SceClassCallback)uidLwMutex_Destructor,
		uidDefaultCancelWait, uidLwMutex_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(1890, "uidLwMutexClassInit");
	s_threadmgrRuntime.manager->lwMutexClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 108, " sizeof(SceUIDLwMutexObject)=%d\n",
			     sizeof(SceUIDLwMutexObject));
}

/* closes every lw mutex a process still holds, PUID and GUID */
int lwMutexProcessExit(SceProcessObject *process)
{
	SceSize got = 0;
	SceUID *vector;
	SceUID puid, guid;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->lwMutexClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 143);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->lwMutexClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		puid = vector[i];
		guid = SceSysmemForDriver_45D22597(process->pid, puid);
		if (guid < 0) {
			SCE_DBG_PRINTF(0xb, 159, "()->scePUIDtoGUID(0x%08X, 0x%08X)=0x%08X\n",
				       process->pid, puid, guid);
			continue;
		}
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, puid);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 166, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, puid, ret);
			SCE_PANIC(167);
		}
		intr = SceCpuForDriver_821FC0EE();
		ret = sceGUIDClose(guid);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 174, "()->sceGUIDClose(guid=0x%08X)=0x%08X\n", guid, ret);
			SCE_PANIC(175);
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 182);
	}
	return 0;
}

/* returns an object to its process's cache (both locks free of it) */
static inline __attribute__((always_inline)) void LwMutexCachePush(SceProcessObject *process, SceUIDLwMutexObject *lwMutex)
{
	sceKernelSpinlockLowLock(&process->lwMutexCacheLock);
	lwMutex->cacheNext = process->lwMutexCache;
	process->lwMutexCache = lwMutex;
	process->lwMutexCacheCount++;
	sceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
}

/*
 * The lw mutex create syscall's body: an object is taken from the process's
 * cache (or made: uidLwMutex_AllocObject, inlined) and the work area is
 * initialised. Sony returns the name copy's length when the PUID cannot be
 * opened (after suspending the thread for the debugger); kept.
 */
int _sceKernelCreateLwMutexForUser(ThreadmgrLwMutexWork *work, const char *name, SceUInt32 attr,
				   int initCount, const SceKernelLwMutexOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process, *cacheProcess;
	UIDCreateOpt opt;
	ThreadmgrLwMutexWork userWork __attribute__((aligned(8)));
	SceUIDLwMutexObject *lwMutex, *old;
	char newName[32];
	volatile int delay;
	uint32_t saved;
	SceUID guid, uid;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ThreadmgrNoContext()) {
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
	if (initCount < 0 || (initCount > 1 && !(attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto restore;
	}
	if (name == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	process = thread->process;
	intr = SceCpuForDriver_821FC0EE();
	ret = sceKernelCopyFromUser(&userWork, work, sizeof(userWork));
	if (ret < 0) {
		ret |= SYSCALL_ERROR_BIT;
		goto resume;
	}
	guid = SceSysmemForKernel_188C8742(0, userWork.uid);
	if (guid >= 0 && sceGUIDReferObjectWithClass(guid,
			s_threadmgrRuntime.manager->lwMutexClass,
			(SceObjectBase **)&old) >= 0) {
		ThreadmgrLwMutexWork *oldWork = old->work;

		sceGUIDReleaseObject(guid);
		if (work == oldWork) {
			if (sceKernelStrncpyFromUser(newName, name, sizeof(newName)) < 0)
				newName[0] = '\0';
			SCE_PRINTF_WITH_INFO(0, 314, "*** error: sceKernelCreateLwMutex(), Illegal reuse "
				"of \"pWork\" address.\n***      :  processId=0x%08X, pWork=%p, old "
				"uid=0x%08X, old name=[%s], new name=[%s]\n", thread->processId, work,
				userWork.uid, old->name, newName);
		}
	}
	sceKernelSpinlockLowLock(&process->resourceLock);
	if (thread->currentAttr & THREAD_ATTR_UNLIMITED) {
		attr |= LW_MUTEX_ATTR_UNLIMITED;
		if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
			process->uidUnlimitedPeak = process->uidUnlimitedCount;
	} else {
		if (process->uidCount >= 0x800) {
			sceKernelSpinlockLowUnlock(&process->resourceLock);
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
	sceKernelSpinlockLowUnlock(&process->resourceLock);

	/* uidLwMutex_AllocObject */
	SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 2099, "uidLwMutex_AllocObject");
	cacheProcess = thread->process;
	for (;;) {
		sceKernelSpinlockLowLock(&cacheProcess->lwMutexCacheLock);
		if (cacheProcess->lwMutexCacheCount <= 0)
			break;
		lwMutex = cacheProcess->lwMutexCache;
		if (SceCpuForDriver_5AC9D394(&lwMutex->lock) >= 0) {
			cacheProcess->lwMutexCache = lwMutex->cacheNext;
			cacheProcess->lwMutexCacheCount--;
			sceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
			lwMutex->cacheNext = &lwMutex->uidEntry;
			uid = SceSysmemForKernel_03A2CAF0(lwMutex->uidEntry);
			if (uid >= 0)
				goto allocated;
			ret = uid;
			goto unaccount;
		}
		sceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
		for (delay = 10; delay != 0; delay--)
			;
	}
	sceKernelSpinlockLowUnlock(&cacheProcess->lwMutexCacheLock);
	opt.flags = UID_CREATE_OPT_PID;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.pid = cacheProcess->pid;
	opt.heapKind = 0;
	opt.type = 0;
	opt.attr = 0;
	opt.ownerPid = 0;
	opt.cls = NULL;
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->lwMutexClass,
		"SceKernelLwMutexCache", &opt, (SceObjectBase **)&lwMutex);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0, 2139, "uidLwMutex_AllocObject", "*** error: "
			"uidLwMutex_AllocObject() -> sceGUIDKernelCreateWithAttr()=0x%08X\n", uid);
		ret = uid;
		goto unaccount;
	}
	lwMutex->ownerPid = cacheProcess->pid;
	ret = sceGUIDGetEntry(uid, &lwMutex->uidEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0, 2150, "uidLwMutex_AllocObject", "*** error: "
			"uidLwMutex_AllocObject() -> sceGUIDGetEntry(0x%08X)=0x%08X\n", uid, ret);
		sceKernelSpinlockLowUnlock(&lwMutex->lock);
		if (sceGUIDClose(uid) < 0)
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
	ret = sceKernelStrncpyFromUser(lwMutex->name, name, sizeof(lwMutex->name));
	if (ret < 0)
		goto recache;
	lwMutex->name[sizeof(lwMutex->name) - 1] = '\0';
	userWork.owner = initCount != 0 ? thread->userThreadId : 0;
	userWork.reserved0 = 0;
	userWork.lockCount = initCount;
	userWork.attr = attr;
	userWork.uid = SceSysmemForDriver_BF209859(0, uid);
	if (userWork.uid < 0) {
		LwMutexCachePush(process, lwMutex);
		sceKernelSpinlockLowUnlock(&lwMutex->lock);
		sceKernelSpinlockLowLock(&process->resourceLock);
		if (attr & LW_MUTEX_ATTR_UNLIMITED)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->lwMutexCount--;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		goto resume;
	}
	userWork.reserved1[0] = 0;
	userWork.reserved1[1] = 0;
	userWork.reserved1[2] = 0;
	ret = sceKernelCopyToUser(work, &userWork, sizeof(userWork));
	if (ret < 0) {
		if (SceSysmemForDriver_84A4AF5E(0, userWork.uid) >= 0) {
			ret |= SYSCALL_ERROR_BIT;
			goto recache;
		}
		sceKernelSpinlockLowUnlock(&lwMutex->lock);
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		goto resume;
	}
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	ret = 0;
	goto resume;
recache:
	LwMutexCachePush(process, lwMutex);
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
unaccount:
	sceKernelSpinlockLowLock(&process->resourceLock);
	if (attr & LW_MUTEX_ATTR_UNLIMITED)
		process->uidUnlimitedCount--;
	else
		process->uidCount--;
	process->lwMutexCount--;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
resume:
	sceKernelCpuResumeIntr(intr);
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
	SceUIDLwCondObject *cond;
	SceUIDLwMutexObject *lwMutex;
	SceUID puid;
	SceObjectBase *pObject;
	volatile int delay;
	struct ThreadmgrWaitQueue *queue;
	uint32_t saved, attr, wake, count;
	uint8_t wakeOwn;
	SceUID guid;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 543, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = sceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 553, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &puid, &work->uid, sizeof(puid), ret);
		goto restore;
	}
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	lwMutex = (SceUIDLwMutexObject *)pObject;
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		sceGUIDReleaseObject(guid);
		goto resume;
	}
	wake = 0;
retry:
	sceKernelSpinlockLowLock(&lwMutex->lock);
	for (link = lwMutex->conds.next; link != &lwMutex->conds; link = next) {
		next = link->next;
		cond = LW_COND_FROM_MUTEX_LINK(link);
		if (SceCpuForDriver_5AC9D394(&cond->lock) < 0) {
			/* the cond holds its lock while it takes ours: back off */
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			for (delay = 10; delay != 0; --delay)
				;
			goto retry;
		}
		wake |= wakeupWaitObjectAll((uintptr_t)&cond->next,
					    SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX, NULL);
		cond->lwMutexId = -1;
		sceKernelSpinlockLowUnlock(&cond->lock);
	}
	queue = (struct ThreadmgrWaitQueue *)&lwMutex->next;
	ret = SceSysmemForDriver_84A4AF5E(0, puid);
	wakeOwn = wake | wakeupWaitObjectAll((uintptr_t)queue, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	process = currentThreadContext()->process;
	sceKernelSpinlockLowLock(&process->lwMutexCacheLock);
	count = process->lwMutexCacheCount + 1;
	lwMutex->cacheNext = process->lwMutexCache;
	process->lwMutexCache = (SceUIDLwMutexObject *)pObject;
	process->lwMutexCacheCount = count;
	sceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
	attr = lwMutex->attr;
	lwMutex->waiting = 0;
	lwMutex->next = queue;
	lwMutex->attr = 0;
	lwMutex->reserved = 0;
	lwMutex->prev = queue;
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	sceKernelSpinlockLowLock(&process->resourceLock);
	if (attr & LW_MUTEX_ATTR_UNLIMITED)
		process->uidUnlimitedCount--;
	else
		process->uidCount--;
	process->lwMutexCount--;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
	sceGUIDReleaseObject(guid);
	if (wakeOwn & 1)
		switchThread(0);
resume:
	sceKernelCpuResumeIntr(intr);
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
		if ((lwMutex)->attr & SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO) {	\
			sceKernelSpinlockLowLock(THREAD_LOCK(__first));		\
			sceKernelSpinlockLowUnlock(THREAD_LOCK(__first));		\
		}									\
	}										\
} while (0)

/* the lw mutex lock syscall's body: the work area is tried, else the thread waits */
int _sceKernelLockLwMutex(ThreadmgrLwMutexWork *work, int count, SceUInt32 *timeout,
			  int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrLwMutexWork userWork __attribute__((aligned(8)));
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
	int locked = 0;
	SceUID puid;
	uint64_t wakeup;
	uint32_t saved;
	SceUID guid, self;
	int intr, ret, cbRet;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 715, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = sceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 724, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &puid, &work->uid, sizeof(puid), ret);
		goto restore;
	}
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	self = currentThreadContext()->userThreadId;
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 742, "*** error: irq suspended. pWork=%p\n", work);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_USER) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = sceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	lwMutex = (SceUIDLwMutexObject *)pObject;
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		goto release;
	}
	ret = sceKernelCopyFromUser(&userWork, work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 779, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	sceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (userWork.owner == self) {
		if (!(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE)) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		} else if ((count += userWork.lockCount) < 0) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		} else {
			ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
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
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			sceKernelSpinlockLowLock(&lwMutex->lock);
			if (((SceUIDLwMutexObject *)pObject)->cacheNext !=
			    &((SceUIDLwMutexObject *)pObject)->uidEntry) {
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
			ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 890, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			goto unlock;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		if (!callbacks) {
			ret = waitWaitableObject(guid, (uintptr_t)&lwMutex->next, SCE_KERNEL_WAITTYPE_LW_MUTEX,
						 &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(guid, (uintptr_t)&lwMutex->next, SCE_KERNEL_WAITTYPE_LW_MUTEX, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto callbacks;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	sceGUIDReleaseObject(guid);
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
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	sceGUIDReleaseObject(guid);
resume:
	sceKernelCpuResumeIntr(intr);
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
	ThreadmgrLwMutexWork userWork __attribute__((aligned(8)));
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
	uint32_t saved, wake;
	SceUID puid, guid, self, newOwner;
	int intr, ret, waiters, firstCount;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 988, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	ret = sceKernelCopyFromUser(&puid, &work->uid, sizeof(puid));
	if (ret < 0)
		goto restore;
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid < 0) {
		SCE_DBG_PRINTF(0, 1004, "*** error: _sceKernelUnlockLwMutex(%p, 0x%08X)="
			"SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID, lwMutexPuid=0x%08X\n", work, count, puid);
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	self = currentThreadContext()->userThreadId;
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 1013, "*** error: irq suspended. pWork=%p\n", work);
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(guid, s_threadmgrRuntime.manager->lwMutexClass,
					   &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	lwMutex = (SceUIDLwMutexObject *)pObject;
	if (lwMutex->work != work) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT;
		goto release;
	}
	ret = sceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1038, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	sceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE))) {
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
		ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &userWork.lockCount,
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
	sceKernelSpinlockLowLock(THREAD_LOCK(first));
	newOwner = first->userThreadId;
	SCE_DBG_ASSERT_CHECK(newOwner >= 0, 1090);
	firstCount = first->waitMode;
	waiters = lwMutex->waiting - 1;
	SCE_DBG_ASSERT_CHECK(firstCount > 0, 1095);
	ret = SceCpuForKernel_9B071FD3(lwMutex->work, newOwner, waiters, firstCount);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1100, "transferLwMutex(%p, 0x%08x, 0x%08x, 0x%08x): error 0x%08x\n",
			       lwMutex->work, newOwner, waiters, firstCount, ret);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
		goto unlock;
	}
	__sync_synchronize();
	wake = wakeupAndDistributeThread(first, (uintptr_t)&lwMutex->next, 0);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	sceGUIDReleaseObject(guid);
	ret = 0;
	if (wake & 1)
		switchThread(0);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	sceGUIDReleaseObject(guid);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * Sony's sceKernelGetLwMutexInfoById: an unreadable work area still gives
 * the rest of the information.
 */
int sceKernelGetLwMutexInfoById(SceUID lwMutexId, SceKernelLwMutexInfo *info)
{
	struct SceUIDThreadObject *owner;
	SceKernelLwMutexInfo local;
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
	if (ret != SCE_KERNEL_ERROR_LW_MUTEX_CAN_NOT_ACCESS) {
		if (ret < 0)
			goto restore;
		if (ThreadmgrInSyscall() && local.currentOwnerId > 0 &&
		    !(local.currentOwnerId & USER_THREAD_ID_KERNEL_FLAG)) {
			ret = sceGUIDGetObject(local.currentOwnerId, &owner);
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
int SceThreadmgrForDriver_C2DA6286(int level, SceKernelLwMutexInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	SceKernelLwMutexInfo info;
	SceUID *vector;
	int total, i, ret, err;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->lwMutexClass,
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
			s_threadmgrRuntime.manager->lwMutexClass, level, vector, ret, &got);
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
		err = getLwMutexInfo(vector[i], &info);
		if (err != SCE_KERNEL_ERROR_LW_MUTEX_CAN_NOT_ACCESS && err != 0)
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

/* the same for the lw mutexes a process holds PUIDs of, with user-visible IDs */
int sceKernelGetLwMutexInfoAllProc(SceUID pid, int level, SceKernelLwMutexInfo *infos,
				   SceSize size, SceSize *count)
{
	SceSize got = 0;
	SceKernelLwMutexInfo info;
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
		s_threadmgrRuntime.manager->lwMutexClass, level, NULL, 0, NULL);
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
			s_threadmgrRuntime.manager->lwMutexClass, level, vector, ret, &got);
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
		err = getLwMutexInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info);
		if (err != SCE_KERNEL_ERROR_LW_MUTEX_CAN_NOT_ACCESS && err != 0)
			continue;
		info.uid = vector[i];
		if (info.currentOwnerId > 0 && !(info.currentOwnerId & USER_THREAD_ID_KERNEL_FLAG)) {
			err = sceGUIDGetObject(info.currentOwnerId, &owner);
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
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
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
	ThreadmgrLwMutexWork userWork __attribute__((aligned(8)));
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
	uint64_t wakeup;
	SceUID self;
	int intr, ret, locked = 0;

	thread = currentThreadContext();
	if (ThreadmgrNoContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 1537, "*** error: illegal context. lwMutexId=0x%08x[%s]\n",
				      lwMutexId, SceSysmemForDriver_E655852F(lwMutexId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	self = currentThreadContext()->userThreadId;
	intr = SceCpuForDriver_821FC0EE();
	if ((intr & 0x80) || currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	ret = sceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, &pObject);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	lwMutex = (SceUIDLwMutexObject *)pObject;
	ret = sceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1570, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		return ret;
	}
	sceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
		goto unlock;
	}
	if (userWork.owner == self) {
		if (!(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE)) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
		} else if ((count += userWork.lockCount) < 0) {
			ret = SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
		} else {
			ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
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
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			callCallbackAll(thread);
			sceKernelSpinlockLowLock(&lwMutex->lock);
		}
		LW_MUTEX_PEEK_FIRST(lwMutex, 1640);
		ret = SceCpuForKernel_75A85059(lwMutex->work, self);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1658, "trylockLwMutex(%p, 0x%08x): error 0x%08x\n",
				       lwMutex->work, self, ret);
			goto unlock;
		}
		if (ret == 0) {
			ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &count, sizeof(count));
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 1668, "sceKernelCopyToUser(0x%08x, %p, 0x%08x): error "
					"0x%08x\n", &lwMutex->work->lockCount, &count, sizeof(count), ret);
			goto unlock;
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = count;
		thread->waitCondId = condId;
		thread->waitMutexId = lwMutexId;
		if (!callbacks) {
			ret = waitWaitableObject(lwMutexId, (uintptr_t)&lwMutex->next,
				SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX, &wakeup, timeout, NULL, &locked);
			SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			if (!locked)
				goto resume;
			break;
		}
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(lwMutexId, (uintptr_t)&lwMutex->next, SCE_KERNEL_WAITTYPE_LW_COND_LW_MUTEX,
					 &wakeup, timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked)
			goto deleted;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	sceGUIDReleaseObject(lwMutexId);
deleted:
	if (ret == SCE_KERNEL_ERROR_WAIT_DELETE && callbacks &&
	    (thread->flags & THREAD_FLAG_CALLBACKS))
		callCallbackAll(thread);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
	sceGUIDReleaseObject(lwMutexId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* a lw cond waiter giving up its lw mutex before it sleeps (interrupts already off) */
int unlockLwMutexForLwCond(SceUID lwMutexId, int count)
{
	struct ThreadCB *first;
	ThreadmgrLwMutexWork userWork __attribute__((aligned(8)));
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
	SceUID self, newOwner;
	int ret, waiters, firstCount;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1752);
	self = currentThreadContext()->userThreadId;
	ret = sceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, &pObject);
	if (ret < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
	lwMutex = (SceUIDLwMutexObject *)pObject;
	ret = sceKernelCopyFromUser(&userWork, lwMutex->work, sizeof(userWork));
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 1770, "sceKernelCopyFromUser(%p, 0x%08x, 0x%08x): error 0x%08x\n",
			       &userWork, lwMutex->work, sizeof(userWork), ret);
		goto release;
	}
	sceKernelSpinlockLowLock(&lwMutex->lock);
	if (count < 1 || (count != 1 && !(lwMutex->attr & SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE))) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
	} else if (userWork.owner != self) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
	} else if (userWork.lockCount < count) {
		ret = SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
	} else if ((userWork.lockCount -= count) > 0) {
		ret = sceKernelCopyToUser(&lwMutex->work->lockCount, &userWork.lockCount,
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
		sceKernelSpinlockLowLock(THREAD_LOCK(first));
		newOwner = first->userThreadId;
		SCE_DBG_ASSERT_CHECK(newOwner >= 0, 1820);
		firstCount = first->waitMode;
		waiters = lwMutex->waiting - 1;
		SCE_DBG_ASSERT_CHECK(firstCount > 0, 1825);
		ret = SceCpuForKernel_9B071FD3(lwMutex->work, newOwner, waiters, firstCount);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1830, "transferLwMutex(%p, 0x%08x, 0x%08x, 0x%08x): error "
				"0x%08x\n", lwMutex->work, newOwner, waiters, firstCount, ret);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
		} else {
			__sync_synchronize();
			wakeupAndDistributeThread(first, (uintptr_t)&lwMutex->next, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
			ret = 0;
		}
	}
	sceKernelSpinlockLowUnlock(&lwMutex->lock);
release:
	sceGUIDReleaseObject(lwMutexId);
	return ret;
}

/* fills a process's cache with fresh objects up to target */
int uidLwMutex_CreateCache(SceProcessObject *process, int target)
{
	UIDCreateOpt opt;
	SceUIDLwMutexObject *lwMutex;
	SceObjectBase *pObject;
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
		intr = SceCpuForDriver_821FC0EE();
		uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->lwMutexClass,
			"SceKernelLwMutexCache", &opt,
			&pObject);
		if (uid < 0) {
			sceKernelCpuResumeIntr(intr);
			SCE_DBG_PRINTF(0, 2013, "*** error: uidLwMutex_CreateCache() -> "
				"sceGUIDKernelCreateWithAttr()=0x%08X\n", uid);
			return uid;
		}
		lwMutex = (SceUIDLwMutexObject *)pObject;
		lwMutex->ownerPid = process->pid;
		ret = sceGUIDGetEntry(uid, &lwMutex->uidEntry);
		if (ret < 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&lwMutex->lock, intr);
			SCE_DBG_PRINTF(0, 2026, "*** error: uidLwMutex_CreateCache() -> "
				"sceGUIDGetEntry(0x%08X)=0x%08X\n", uid, ret);
			if (sceGUIDClose(uid) < 0)
				SCE_PANIC(2028);
		} else {
			SCE_DBG_ASSERT_CHECK(lwMutex->uidEntry != NULL, 2032);
			sceKernelSpinlockLowLock(&process->lwMutexCacheLock);
			lwMutex->cacheNext = process->lwMutexCache;
			process->lwMutexCache = lwMutex;
			if (++process->lwMutexCacheCount >= target)
				more = false;
			sceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&lwMutex->lock, intr);
		}
	} while (more);
	return ret;
}

/* trims a process's cache down to target */
int uidLwMutex_DeleteCache(SceProcessObject *process, int target)
{
	SceUIDLwMutexObject *lwMutex;
	volatile int delay;
	int intr, ret;

	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
		if (process->lwMutexCacheCount <= target)
			break;
		for (;;) {
			lwMutex = process->lwMutexCache;
			if (SceCpuForDriver_5AC9D394(&lwMutex->lock) < 0)
				goto backoff;
			process->lwMutexCache = lwMutex->cacheNext;
			process->lwMutexCacheCount--;
			sceKernelSpinlockLowUnlock(&process->lwMutexCacheLock);
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			ret = sceGUIDClose(SceSysmemForKernel_03A2CAF0(lwMutex->uidEntry));
			SCE_DBG_ASSERT_CHECK(ret >= 0, 2075);
			sceKernelCpuResumeIntr(intr);
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
			if (process->lwMutexCacheCount <= target)
				goto done;
		}
backoff:
		sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
		for (delay = 10; delay != 0; delay--)
			;
	}
done:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
	return 0;
}

/* keeps a process's cache between low and high (refilled or trimmed to target) */
int uidLwMutex_ManageCache(SceProcessObject *process, int high, int low, int target)
{
	int intr, count, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->lwMutexCacheLock);
	count = process->lwMutexCacheCount;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&process->lwMutexCacheLock, intr);
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
