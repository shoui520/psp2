/*
 * SceKernelThreadMgr: lw_cond.c, the lightweight condition variables
 * (SceUIDLwCondClass), bound to a lw mutex the waiter gives up and relocks.
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
#include <threadmgr/threadmgr.h>

#define LW_COND_ITEM_SIZE		0x80	/* the class's object allocation size */
#define LW_COND_ATTR_MASK		0x00002000
#define LW_COND_ATTR_UNLIMITED		0x00004000
#define LW_COND_OPTION_MAX_SIZE		4
#define LW_COND_UID_ATTR		0x00002000	/* PLACEHOLDER: name */
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define WAIT_TYPE_LW_COND		0x00200000
#define WAIT_TYPE_LW_COND_MUTEX		0x00400000
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define SDK_VERSION_SIGNAL_TO_STRICT	0x02000000
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID 0x80028021
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID 0x80028022
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID 0x80028181
#define SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID 0x800281c1
#define SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX 0x800281c2
#define SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND 0x800281c3	/* PLACEHOLDER: name */

typedef struct ThreadmgrLwCondOptParam {
	SceSize size;
} ThreadmgrLwCondOptParam;

int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);

/* SceUIDLwCondClass's constructor */
static int uidLwCond_Constructor(void *object)
{
	ThreadmgrLwCond *cond = object;

	memset(&cond->option, 0, 0x20);
	return 0;
}

/*
 * SceUIDLwCondClass's destructor: threads relocking the lw mutex after this
 * cond wake with WAIT_DELETE_LW_COND, the cond's own waiters with WAIT_DELETE.
 */
static int uidLwCond_Destructor(void *object)
{
	ThreadmgrLwCond *cond = object;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	SceProcessObject *process;
	ThreadmgrLwMutex *lwMutex;
	SceUID guid = cond->guid, lwMutexId = cond->lwMutexId, pid;
	volatile int delay;
	uint32_t wake, wakeMutex;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1277);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(lwMutexId,
			s_threadmgrRuntime.manager->lwMutexClass,
			(SceObjectBase **)&lwMutex) < 0) {
		wakeMutex = 0;
		SceCpuForDriver_BF82DEB2(&cond->lock);
	} else {
		queue = (struct ThreadmgrWaitQueue *)&lwMutex->next;
		SceCpuForDriver_BF82DEB2(&lwMutex->lock);
		while (SceCpuForDriver_5AC9D394(&cond->lock) < 0) {
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			for (delay = 10; --delay != 0;)
				;
			SceCpuForDriver_BF82DEB2(&lwMutex->lock);
		}
		ListLinkRemove(&cond->lwMutexLink);
		wakeMutex = 0;
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
			waiter = (struct ThreadCB *)node;
			next = node->next;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1303);
			SceCpuForDriver_BF82DEB2(THREAD_LOCK(waiter));
			if (waiter->waitType == WAIT_TYPE_LW_COND_MUTEX && waiter->waitCondId == guid)
				wakeMutex |= wakeupAndDistributeThread(waiter, (uintptr_t)queue,
					SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		sceKernelSpinlockLowUnlock(&lwMutex->lock);
		sceGUIDReleaseObject(lwMutexId);
	}
	wake = wakeupWaitObjectAll((uintptr_t)&cond->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	sceKernelSpinlockLowUnlock(&cond->lock);
	pid = cond->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		SceCpuForDriver_BF82DEB2(&process->resourceLock);
		if (cond->attr & LW_COND_ATTR_UNLIMITED)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->lwCondCount--;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
		sceGUIDReleaseObject(pid);
	}
	if (((wakeMutex | wake) & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	memset(&cond->option, 0, 0x20);
	return 0;
}

/* a lw cond's information (the info exports' core) */
static int getLwCondInfo(SceUID condId, SceKernelLwCondInfo *info)
{
	ThreadmgrLwCond *cond;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(condId,
		s_threadmgrRuntime.manager->lwCondClass, (SceObjectBase **)&cond);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ? SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID | SYSCALL_ERROR_BIT :
					     SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;
	} else {
		info->uid = condId;
		strncpy(info->name, SceSysmemForDriver_E655852F(condId), sizeof(info->name) - 1);
		SceCpuForDriver_BF82DEB2(&cond->lock);
		info->attr = cond->attr;
		info->numWaitThreads = cond->waiting;
		info->pWork = cond->work;
		info->pLwMutex = cond->lwMutexWork;
		sceKernelSpinlockLowUnlock(&cond->lock);
		sceGUIDReleaseObject(condId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDLwCondClass */
void startLwCond(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(87);
	ret = uidExtendedClassInit(cls, "SceUIDLwCondClass",
		s_threadmgrRuntime.manager->waitableClass, LW_COND_ITEM_SIZE,
		uidLwCond_Constructor, (SceClassCallback)uidLwCond_Destructor,
		uidDefaultCancelWait, uidDefault_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1233, 1231), "uidLwCondClassInit");
	s_threadmgrRuntime.manager->lwCondClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 103, " sizeof(SceUIDLwCondObject)=%d\n", sizeof(ThreadmgrLwCond));
}

/* closes every lw cond PUID a process still holds */
int lwCondProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->lwCondClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 130);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->lwCondClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 148, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(149);
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 156);
	}
	return 0;
}

/* the lw cond create syscall's body: the cond joins the lw mutex's list */
int __sceKernelCreateLwCond(void *work, const char *name, SceUInt32 attr, SceUID lwMutexPuid,
			    const ThreadmgrLwCondOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process;
	UIDCreateOpt opt;
	ThreadmgrLwCondWork userWork;
	ThreadmgrLwMutex *lwMutex;
	ThreadmgrLwCond *cond;
	uint32_t saved;
	SceUID lwMutexId, uid;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 204, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (attr & ~LW_COND_ATTR_MASK) {
		SCE_DBG_PRINTF(0, 212, " k: error: __sceKernelCreateLwCond(%s), attr=0x%08X\n", name,
			       attr);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > LW_COND_OPTION_MAX_SIZE) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	lwMutexId = SceSysmemForDriver_45D22597(0, lwMutexPuid);
	if (lwMutexId < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID | SYSCALL_ERROR_BIT;
		goto restore;
	}
	opt.ownerPid = 0;
	opt.cls = NULL;
	opt.pid = thread->processId;
	opt.type = 0;
	if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
		attr |= LW_COND_ATTR_UNLIMITED;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
	process = thread->process;
	opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
	opt.attr = LW_COND_UID_ATTR;
	intr = SceCpuForDriver_D32ACE9E(&process->resourceLock);
	if (!(attr & LW_COND_ATTR_UNLIMITED)) {
		if (process->uidCount >= 0x800) {
			sceKernelSpinlockLowUnlock(&process->resourceLock);
			SCE_DBG_PRINTF(0xb, 259, "(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
				       thread->processId);
			ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
			goto resume;
		}
		if (++process->uidCount > process->uidPeak)
			process->uidPeak = process->uidCount;
	} else if (++process->uidUnlimitedCount > process->uidUnlimitedPeak) {
		process->uidUnlimitedPeak = process->uidUnlimitedCount;
	}
	if (++process->lwCondCount > process->lwCondPeak)
		process->lwCondPeak = process->lwCondCount;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
	ret = sceGUIDReferObjectWithClass(lwMutexId,
		s_threadmgrRuntime.manager->lwMutexClass, (SceObjectBase **)&lwMutex);
	if (ret >= 0) {
		SceCpuForDriver_BF82DEB2(&lwMutex->lock);
		uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->lwCondClass,
			name, &opt, (SceObjectBase **)&cond);
		ret = uid;
		if (uid >= 0) {
			cond->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
			cond->attr = attr;
			cond->option = option;
			cond->lwMutexId = lwMutexId;
			cond->guid = uid;
			cond->work = work;
			cond->lwMutexWork =
				((ThreadmgrLwMutex *)SceSysmemForKernel_FA6C098C(lwMutexId))->work;
			cond->lwMutexLink.next = &lwMutex->conds;
			cond->lwMutexLink.prev = lwMutex->conds.prev;
			lwMutex->conds.prev = &cond->lwMutexLink;
			cond->lwMutexLink.prev->next = &cond->lwMutexLink;
			sceKernelSpinlockLowUnlock(&cond->lock);	/* the class hands it over locked */
			sceKernelSpinlockLowUnlock(&lwMutex->lock);
			sceGUIDReleaseObject(lwMutexId);
			userWork.uid = SceSysmemForDriver_CED1547B(0, uid, 1);
			userWork.reserved[0] = 0;
			userWork.reserved[1] = 0;
			userWork.lwMutexId = lwMutexPuid;
			ret = sceKernelCopyToUser(work, &userWork, sizeof(userWork));
			if (ret < 0 && SceSysmemForDriver_84A4AF5E(0, userWork.uid) < 0)
				SCE_PANIC(SCE_LINE(324, 322));
			goto resume;
		}
		sceKernelSpinlockLowUnlock(&lwMutex->lock);
		sceGUIDReleaseObject(lwMutexId);
	}
	SceCpuForDriver_BF82DEB2(&process->resourceLock);
	if (!(attr & LW_COND_ATTR_UNLIMITED))
		process->uidCount--;
	else
		process->uidUnlimitedCount--;
	process->lwCondCount--;
	sceKernelSpinlockLowUnlock(&process->resourceLock);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The lw cond wait syscall's body, like waitCond. Sony re-enables interrupts
 * (sceKernelCpuResumeIntr(0)) around the relock and disables them again
 * after it; kept.
 */
int __sceKernelWaitLwCond(SceUID condId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrLwCond *cond;
	SceUID lwMutexId;
	uint64_t wakeup;
	uint32_t saved, status, keepMutex;
	int intr, ret, pending, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 394, "*** error: illegal context. lwCondId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 402, "*** error: irq suspended. lwCondId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
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
	ret = sceGUIDReferObjectWithClass(condId,
		s_threadmgrRuntime.manager->lwCondClass, (SceObjectBase **)&cond);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	SceCpuForDriver_BF82DEB2(&cond->lock);
	lwMutexId = cond->lwMutexId;
	keepMutex = 0;
	if (SceThreadmgrForDriver_20C228E4()) {
		status = thread->flags;
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
			THREAD_FLAG_LW_COND_KEEP_MUTEX | THREAD_FLAG_LW_COND_RELOCK);
		if (status & THREAD_FLAG_LW_COND_RELOCK) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			sceGUIDReleaseObject(condId);
			sceKernelCpuResumeIntr(intr);
			ret = lockLwMutexForLwCond(lwMutexId, 1, NULL, callbacks, condId);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 453, "warning: can not lock mutex in WaitCond() [syscall "
					"retry], mutex GUID = 0x%08X, ret = 0x%08X\n", lwMutexId, ret);
			goto restore;
		}
		keepMutex = (status & THREAD_FLAG_LW_COND_KEEP_MUTEX) != 0;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	if (!keepMutex) {
		ret = unlockLwMutexForLwCond(lwMutexId, 1);
		if (ret < 0) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			sceGUIDReleaseObject(condId);
			goto resume;
		}
	}
	do {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			ret = callCallbackAll(thread);
			if (ret < 0) {
				sceGUIDReleaseObject(condId);
				goto resume;
			}
			SceCpuForDriver_BF82DEB2(&cond->lock);
		}
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
		thread->waitMode = 0;
		thread->waitCondId = condId;
		thread->waitMutexId = cond->lwMutexId;
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(condId, (uintptr_t)&cond->next, WAIT_TYPE_LW_COND, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			if (ret == (int)THREADMGR_ERROR_WAIT_CANCEL)
				goto next;
			if (ret == SCE_KERNEL_ERROR_WAIT_DELETE)
				ret = SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND;
			else if (ret != SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX &&
				 (uint32_t)ret - 0x8002802c > 1)
				SCE_DBG_ASSERT_FAIL(537);
			if (!locked)
				goto done;
			break;
		}
		/* woken (or timed out) on the cond: take the lw mutex back */
		if (locked) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			sceKernelCpuResumeIntr(0);
			pending = lockLwMutexForLwCond(lwMutexId, 1, NULL, callbacks, condId);
			if (pending < 0) {
				if (pending == SCE_KERNEL_ERROR_WAIT_DELETE)
					pending = SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX;
				SceCpuForDriver_821FC0EE();
				sceGUIDReleaseObject(condId);
				ret = pending;
				goto resume;
			}
			SceCpuForDriver_821FC0EE();
			SceCpuForDriver_BF82DEB2(&cond->lock);
		}
next:
		if (!callbacks) {
			if (!locked)
				goto done;
			break;
		}
		if (!locked)
			goto done;
	} while (ret == (int)THREADMGR_ERROR_WAIT_CANCEL);
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
done:
	if (callbacks && ret == SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND &&
	    (thread->flags & THREAD_FLAG_CALLBACKS)) {
		pending = thread->flags & THREAD_FLAG_CALLBACK_RESULT;
		ret = callCallbackAll(thread);
		if (ret >= 0)
			ret = 0;
		if (!pending)
			ret = SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND;
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* a signal from a thread with a suspend/exit request pending only marks it */
static inline int LwCondSignalDeferred(struct ThreadCB *thread)
{
	return (thread->flags & THREAD_FLAG_REQUEST_MASK) != 0;
}

/* wakes the first waiter */
int __sceKernelSignalLwCond(SceUID condId)
{
	struct ThreadCB *thread, *first;
	ThreadmgrLwCond *cond;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (LwCondSignalDeferred(thread)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto resume;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ret = sceGUIDReferObjectWithClass(condId,
		s_threadmgrRuntime.manager->lwCondClass, (SceObjectBase **)&cond);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	SceCpuForDriver_BF82DEB2(&cond->lock);
	first = (struct ThreadCB *)cond->next;
	if ((struct ThreadmgrWaitQueue *)first == (struct ThreadmgrWaitQueue *)&cond->next) {
		sceKernelSpinlockLowUnlock(&cond->lock);
		sceGUIDReleaseObject(condId);
		ret = 0;
		goto resume;
	}
	SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 656);
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(first));
	wake = wakeupAndDistributeThread(first, (uintptr_t)&cond->next, 0);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* wakes every waiter */
int __sceKernelSignalLwCondAll(SceUID condId)
{
	struct ThreadCB *thread, *first;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrLwCond *cond;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (LwCondSignalDeferred(thread)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto resume;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ret = sceGUIDReferObjectWithClass(condId,
		s_threadmgrRuntime.manager->lwCondClass, (SceObjectBase **)&cond);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)&cond->next;
	SceCpuForDriver_BF82DEB2(&cond->lock);
	if (queue->next == (struct ThreadmgrWaitNode *)queue) {
		sceKernelSpinlockLowUnlock(&cond->lock);
		sceGUIDReleaseObject(condId);
		ret = 0;
		goto resume;
	}
	wake = 0;
	do {
		first = (struct ThreadCB *)queue->next;
		SCE_DBG_ASSERT_CHECK(first->magic == THREAD_CB_MAGIC, 738);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(first));
		wake |= wakeupAndDistributeThread(first, (uintptr_t)queue, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	} while (queue->next != (struct ThreadmgrWaitNode *)queue);
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
	ret = 0;
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
resume:
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the SDK version __sceKernelSignalLwCondTo judges the calling process by */
static inline SceUInt32 LwCondCallerSdkVersion(struct ThreadCB *thread,
					       SceUInt32 *version)
{
	if (thread->processId == THREADMGR_KERNEL_PID)
		*version = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(0, version) < 0)
		*version = SDK_VERSION_SIGNAL_TO_STRICT;
	return *version;
}

/*
 * Wakes one given waiter. Before SDK 0x02000000 a thread that is not
 * waiting gives -1 (Sony; kept), from it ILLEGAL_THREAD_ID.
 */
int __sceKernelSignalLwCondTo(SceUID condId, SceUID threadId)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrLwCond *cond;
	SceObjectBase *object;
	SceUInt32 sdkVersion;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (threadId == 0 || thread->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	if (sceGUIDGetObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
				       &object) < 0 &&
	    LwCondCallerSdkVersion(thread, &sdkVersion) >= SDK_VERSION_SIGNAL_TO_STRICT) {
		SCE_DBG_PRINTF(0, 810, "*** error: __sceKernelSignalLwCondTo(0x%08X[%s], 0x%08X) "
			"returns SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID\n", condId, SceSysmemForDriver_E655852F(condId),
			threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	SceCpuForDriver_BF82DEB2(THREAD_LOCK(thread));
	if (LwCondSignalDeferred(thread)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto resume;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ret = sceGUIDReferObjectWithClass(condId,
		s_threadmgrRuntime.manager->lwCondClass, (SceObjectBase **)&cond);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)&cond->next;
	SceCpuForDriver_BF82DEB2(&cond->lock);
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		waiter = (struct ThreadCB *)node;
		next = node->next;
		SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 849);
		SceCpuForDriver_BF82DEB2(THREAD_LOCK(waiter));
		if (waiter->threadId == threadId) {
			wake = wakeupAndDistributeThread(waiter, (uintptr_t)queue, 0);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
			sceKernelSpinlockLowUnlock(&cond->lock);
			sceGUIDReleaseObject(condId);
			ret = 0;
			if ((wake & 1) && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			goto resume;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
	}
	if (LwCondCallerSdkVersion(thread, &sdkVersion) >= SDK_VERSION_SIGNAL_TO_STRICT) {
		SCE_DBG_PRINTF(0, 876, "*** error: __sceKernelSignalLwCondTo(0x%08X[%s], 0x%08X[%s]) "
			"returns SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID\n", condId, SceSysmemForDriver_E655852F(condId),
			threadId, SceSysmemForDriver_E655852F(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	} else {
		ret = -1;
	}
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* a lw cond's information, copied out */
int sceKernelGetLwCondInfo(SceUID condId, SceKernelLwCondInfo *info)
{
	SceKernelLwCondInfo local;
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
		ret = getLwCondInfo(condId, &local);
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

int SceThreadmgrForDriver_C2AFFBD0(SceUID condId, SceKernelLwCondInfo *info)
{
	return sceKernelGetLwCondInfo(condId, info);
}

/* SceThreadmgrForDriver_738FF63D: the information of every lw cond visible at level */
int SceThreadmgrForDriver_738FF63D(int level, SceKernelLwCondInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelLwCondInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->lwCondClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->lwCondClass, level, vector, num, &got);
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
		if (getLwCondInfo(vector[i], &info) != 0)
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

/* SceThreadmgrForDriver_09346D1A: the same for a process's lw cond PUIDs */
int SceThreadmgrForDriver_09346D1A(SceUID pid, int level, SceKernelLwCondInfo *infos,
				   SceSize size, SceSize *count)
{
	SceKernelLwCondInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid,
		s_threadmgrRuntime.manager->lwCondClass, level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid,
			s_threadmgrRuntime.manager->lwCondClass, level, vector, num, &got);
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
		if (getLwCondInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		ret++;
		info.uid = vector[i];
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
