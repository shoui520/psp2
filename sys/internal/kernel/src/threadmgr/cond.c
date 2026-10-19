/*
 * SceKernelThreadMgr: cond.c, the condition variables (SceUIDCondClass).
 * A cond is bound to a mutex at creation; a waiter gives the mutex up
 * (unlockMutexForCond), sleeps on the cond and relocks the mutex
 * (lockMutexForCond) before it returns.
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
#include <threadmgr/threadmgr.h>

#if FW_DEBUG
/* Native data retains this record without an executable reference. */
static const SceKernelDebugInfo s_openCondDebugInfo
__attribute__((section(".sce_dbginfo"), used)) = {
	SCE_DBGINFO_FIELDS(484, "sceKernelOpenCond")
};

static const SceKernelDebugInfoSource s_openCondDebugSource
__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&s_openCondDebugInfo, __FILE_NAME__, 484, "sceKernelOpenCond",
};
#endif

#define COND_ITEM_SIZE			0x80	/* the class's object allocation size */
#define COND_KERNEL_ATTR_MASK		0x0000a080	/* what kernel callers may pass */
#define COND_USER_ATTR_MASK		0x00002080	/* what a syscall may pass */
#define COND_ATTR_LW			0x00000080
#define COND_ATTR_UNINTERRUPTIBLE	0x00008000	/* PLACEHOLDER: name */
#define COND_ATTR_UNLIMITED		0x00004000
#define COND_OPTION_MAX_SIZE		4
#define COND_UID_ATTR_LW		0x0000c000	/* PLACEHOLDER: name */
#define SDK_VERSION_LW_COND_END		0x02100000	/* lw conds through this syscall end here */
#define SDK_VERSION_SIGNAL_TO_STRICT	0x02000000
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define THREAD_ATTR_CHECK_STACK		0x80000000
#define WAIT_TYPE_COND			0x100
#define WAIT_TYPE_COND_MUTEX		0x200
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */

/* SceUIDCondClass's constructor */
static int uidCond_Constructor(void *object)
{
	ThreadmgrCond *cond = object;

	memset(&cond->option, 0, 0x18);
	return 0;
}

/*
 * SceUIDCondClass's destructor: threads relocking the mutex after this cond
 * wake with WAIT_DELETE_COND, the cond's own waiters with WAIT_DELETE.
 */
static int uidCond_Destructor(void *object)
{
	ThreadmgrCond *cond = object;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadCB *waiter;
	struct ThreadmgrWaitQueue *queue;
	SceProcessObject *process;
	ThreadmgrMutex *mutex;
	SceUID mutexId = cond->mutexId, waitCondId = cond->waitCondId, pid;
	volatile int delay;
	uint32_t wake, wakeMutex;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1531);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(mutexId, s_threadmgrRuntime.manager->mutexClass,
					 (SceObjectBase **)&mutex) < 0) {
		wakeMutex = 0;
		sceKernelSpinlockLowLock(&cond->lock);
	} else {
		queue = (struct ThreadmgrWaitQueue *)&mutex->next;
		sceKernelSpinlockLowLock(&mutex->lock);
		while (SceCpuForDriver_5AC9D394(&cond->lock) < 0) {
			sceKernelSpinlockLowUnlock(&mutex->lock);
			for (delay = 10; --delay != 0;)
				;
			sceKernelSpinlockLowLock(&mutex->lock);
		}
		ListLinkRemove(&cond->mutexLink);
		wakeMutex = 0;
		for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
			waiter = (struct ThreadCB *)node;
			next = node->next;
			SCE_DBG_ASSERT_CHECK(waiter->magic == THREAD_CB_MAGIC, 1557);
			sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
			if (waiter->waitType == WAIT_TYPE_COND_MUTEX && waiter->waitCondId == waitCondId)
				wakeMutex |= wakeupAndDistributeThread(waiter, (uintptr_t)queue,
					SCE_KERNEL_ERROR_WAIT_DELETE_COND);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(waiter));
		}
		sceKernelSpinlockLowUnlock(&mutex->lock);
		sceGUIDReleaseObject(mutexId);
	}
	wake = wakeupWaitObjectAll((uintptr_t)&cond->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	sceKernelSpinlockLowUnlock(&cond->lock);
	pid = cond->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (cond->attr & COND_ATTR_LW)
				process->uidLwCount--;
			else if (cond->attr & COND_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->condCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	if (((wakeMutex | wake) & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	sceKernelCpuResumeIntr(intr);
	memset(&cond->option, 0, 0x18);
	return 0;
}

/*
 * Waits on the cond with its mutex given up, then relocks the mutex. A
 * retried syscall (SceThreadmgrForDriver_20C228E4) may already have
 * released the mutex or finished the wait (thread status bits 22/23).
 */
int waitCond(SceUID condId, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread;
	ThreadmgrCond *cond;
	SceUID mutexId;
	uint64_t wakeup;
	uint32_t saved, status, keepMutex;
	int intr, ret, pending, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 571, "*** error: illegal context. condId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 579, "*** error: irq suspended. condId=0x%08x[%s]\n",
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
	ret = sceGUIDReferObjectWithClass(condId, s_threadmgrRuntime.manager->condClass,
					   (SceObjectBase **)&cond);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 605,
				"*** error: unknown condvar id. condId=0x%08x\n", condId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&cond->lock);
	mutexId = cond->mutexId;
	keepMutex = 0;
	if (SceThreadmgrForDriver_20C228E4()) {
		status = thread->flags;
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags,
			THREAD_FLAG_COND_KEEP_MUTEX | THREAD_FLAG_COND_RELOCK);
		if (status & THREAD_FLAG_COND_RELOCK) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			sceGUIDReleaseObject(condId);
			ret = lockMutexForCond(mutexId, 1, NULL, callbacks, condId);
			goto resume;
		}
		keepMutex = (status & THREAD_FLAG_COND_KEEP_MUTEX) != 0;
	}
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	if (!keepMutex) {
		ret = unlockMutexForCond(mutexId, 1);
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
			sceKernelSpinlockLowLock(&cond->lock);
		}
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitMode = 0;
		thread->waitCondId = condId;
		thread->waitMutexId = cond->mutexId;
		if (callbacks)
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(condId, (uintptr_t)&cond->next, WAIT_TYPE_COND, &wakeup,
					 timeout, NULL, &locked);
		SceCpuForDriver_4AE1BCC0((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (ret != 0 && ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			if (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK)
				goto next;
			if (ret == SCE_KERNEL_ERROR_WAIT_DELETE) {
				ret = SCE_KERNEL_ERROR_WAIT_DELETE_COND;
			} else if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL) {
				SCE_DBG_ASSERT_FAIL(710);
				ret = SCE_KERNEL_ERROR_WAIT_CANCEL_COND;
			} else if (ret != SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX &&
				   (uint32_t)ret - 0x8002802c > 1) {
				SCE_DBG_ASSERT_FAIL(720);
			}
			if (!locked)
				goto done;
			break;
		}
		/* woken (or timed out) on the cond: take the mutex back */
		if (locked) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			pending = lockMutexForCond(mutexId, 1, NULL, callbacks, condId);
			if (pending < 0) {
				if (pending == SCE_KERNEL_ERROR_WAIT_DELETE)
					pending = SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX;
				else if (pending == SCE_KERNEL_ERROR_WAIT_CANCEL)
					pending = SCE_KERNEL_ERROR_WAIT_CANCEL_MUTEX;
				SceCpuForDriver_821FC0EE();
				sceGUIDReleaseObject(condId);
				ret = pending;
				goto resume;
			}
			sceKernelSpinlockLowLock(&cond->lock);
		}
next:
		if (!callbacks) {
			if (!locked)
				goto done;
			break;
		}
		if (!locked)
			goto done;
	} while (ret == (int)SCE_KERNEL_ERROR_NOTIFY_CALLBACK);
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
done:
	if (callbacks && ret == SCE_KERNEL_ERROR_WAIT_DELETE_COND &&
	    (thread->flags & THREAD_FLAG_CALLBACKS)) {
		pending = thread->flags & THREAD_FLAG_CALLBACK_RESULT;
		ret = callCallbackAll(thread);
		if (ret >= 0)
			ret = 0;
		if (!pending)
			ret = SCE_KERNEL_ERROR_WAIT_DELETE_COND;
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* a cond's information (the info exports' core) */
int getCondInfo(SceUID condId, SceKernelCondInfo *info)
{
	ThreadmgrCond *cond;
	int intr, ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(condId, s_threadmgrRuntime.manager->condClass,
					   (SceObjectBase **)&cond);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF(0, 1180,
				"*** error: unknown condvar id. condId=0x%08x\n", condId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID;
		}
	} else {
		info->condId = condId;
		strncpy(info->name, SceSysmemForDriver_E655852F(condId), sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(&cond->lock);
		info->attr = cond->attr;
		info->mutexId = cond->mutexId;
		info->numWaitThreads = cond->waiting;
		sceKernelSpinlockLowUnlock(&cond->lock);
		sceGUIDReleaseObject(condId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDCondClass */
void startCond(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(manager->heapId, 0x38);
	if (cls == NULL)
		SCE_PANIC(103);
	ret = uidExtendedClassInit(cls, "SceUIDCondClass",
		s_threadmgrRuntime.manager->waitableClass, COND_ITEM_SIZE,
		uidCond_Constructor, (SceClassCallback)uidCond_Destructor,
		uidDefaultCancelWait, uidDefault_RevalidateMethod,
		uidDefault_ReleaseMethod);
	if (ret < 0)
		SCE_PANIC_FUNC(SCE_LINE(1499, 1492), "uidCondClassInit");
	s_threadmgrRuntime.manager->condClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 119, " sizeof(SceUIDCondObject)=%d\n", sizeof(ThreadmgrCond));
}

/* closes every cond PUID a process still holds */
int condProcessExit(SceProcessObject *process)
{
	SceUID *vector;
	SceSize got = 0;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->condClass, PUID_VECTOR_LEVEL, NULL, 0, NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 146);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->condClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		intr = SceCpuForDriver_821FC0EE();
		ret = SceSysmemForDriver_84A4AF5E(process->pid, vector[i]);
		sceKernelCpuResumeIntr(intr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 164, "()->scePUIDClose(0x%08X, puid=0x%08X)=0x%08X\n",
				       process->pid, vector[i], ret);
			SCE_PANIC(165);
		}
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 172);
	}
	return 0;
}

static inline SceUID OpenByGUID(SceUID pid, SceUID guid)
{
	SceUID puid = scePUIDOpenByGUIDWithFlags(pid, guid, 1);

	SCE_DBG_PRINTF_LEVEL_FUNC(2, 3, 66, "OpenByGUID", "0x%08x=scePUIDOpenByGUID(0x%08x)\n",
				  puid, guid);
	return puid;
}

/*
 * Inside a syscall the result is the caller's PUID. Sony's PUID failure
 * path keeps the mutex locked and referenced; kept.
 */
SceUID sceKernelCreateCond(const char *name, SceUInt32 attr, SceUID mutexId,
			   const SceKernelCondOptParam *option)
{
	struct ThreadCB *thread;
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrMutex *mutex;
	ThreadmgrCond *cond;
	SceUInt32 sdkVersion;
	uint32_t saved;
	SceUID uid, puid;
	int intr;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 219, "sceKernelCreateCond", "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (!ThreadmgrInSyscall()) {
		if (attr & ~COND_KERNEL_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 251, "sceKernelCreateCond", " k: error: sceKernelCreateCond(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else {
		if (attr & ~COND_USER_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 229, "sceKernelCreateCond", " k: error: sceKernelCreateCond(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
		if (attr & COND_ATTR_LW) {
			SCE_DBG_ASSERT_CHECK_FUNC(thread->processId != THREADMGR_KERNEL_PID, 236, "sceKernelCreateCond");
			if (SceSysrootForDriver_CAE820B2(thread->processId, &sdkVersion) >= 0 &&
			    sdkVersion >= SDK_VERSION_LW_COND_END) {
				SCE_DBG_PRINTF_FUNC(0, 241, "sceKernelCreateCond", " k: error: sceKernelCreateCond(%s), "
					"attr=0x%08X\n", name, attr);
				uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
				goto restore;
			}
		}
	}
	if (option != NULL && option->size > COND_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (attr & COND_ATTR_LW) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = COND_UID_ATTR_LW;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			opt.pid = thread->processId;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
				attr |= COND_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		}
	}
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&process->resourceLock);
		if (attr & COND_ATTR_LW) {
			if (process->uidLwCount >= 0x100) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 304, "sceKernelCreateCond", "sceKernelCreateCond(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & COND_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 321, "sceKernelCreateCond",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->condCount > process->condPeak)
			process->condPeak = process->condCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	thread = currentThreadContext();
	uid = sceGUIDReferObjectWithClass(mutexId, s_threadmgrRuntime.manager->mutexClass,
					   (SceObjectBase **)&mutex);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			uid = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 347, "sceKernelCreateCond",
				"*** error: unknown mutex id. mutexId=0x%08x\n", mutexId);
			uid = SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID;
		}
		goto unaccount;
	}
	sceKernelSpinlockLowLock(&mutex->lock);
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->condClass, name,
		&opt, (SceObjectBase **)&cond);
	if (uid < 0) {
		sceKernelSpinlockLowUnlock(&mutex->lock);
		sceGUIDReleaseObject(mutexId);
		goto unaccount;
	}
	cond->ownerPid = ThreadmgrInSyscall() ? thread->processId : THREADMGR_KERNEL_PID;
	cond->option = option;
	cond->mutexId = mutexId;
	cond->mutexLink.next = &mutex->core.conds;
	cond->mutexLink.prev = mutex->core.conds.prev;
	cond->waitCondId = -1;
	cond->attr = attr;
	mutex->core.conds.prev = &cond->mutexLink;
	cond->mutexLink.prev->next = &cond->mutexLink;
	if (ThreadmgrInSyscall()) {
		puid = OpenByGUID(currentThreadContext()->processId, uid);
		SCE_DBG_ASSERT_CHECK_FUNC(puid >= 0, 378, "sceKernelCreateCond");
		if (puid < 0) {
			sceKernelSpinlockLowUnlock(&cond->lock);
			uid = sceGUIDClose(uid);
			SCE_DBG_ASSERT_CHECK_FUNC(uid >= 0, 383, "sceKernelCreateCond");
			uid = puid;
			goto unaccount;
		}
		cond->puid = puid;
		uid = puid;
	}
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceKernelSpinlockLowUnlock(&mutex->lock);
	sceGUIDReleaseObject(mutexId);
	goto resume;
unaccount:
	if (ThreadmgrInSyscall()) {
		sceKernelSpinlockLowLock(&process->resourceLock);
		if (attr & COND_ATTR_LW)
			process->uidLwCount--;
		else if (attr & COND_ATTR_UNLIMITED)
			process->uidUnlimitedCount--;
		else
			process->uidCount--;
		process->condCount--;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

int sceKernelDeleteCond(SceUID condId)
{
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 442, "sceKernelDeleteCond", "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (SceSysmemForKernel_368F1C36(condId) == s_threadmgrRuntime.manager->condClass)
		ret = sceGUIDClose(condId);
	else
		ret = SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	sceKernelCpuResumeIntr(intr);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

int sceKernelWaitCond(SceUID condId, SceUInt32 *timeout)
{
	return waitCond(condId, timeout, 0);
}

int waitCondCB(SceUID condId, SceUInt32 *timeout)
{
	return waitCond(condId, timeout, 1);
}

/*
 * Whether a signal from thread must wait: it has a suspend/exit request
 * pending and is inside a syscall, unless the request is a plain suspend of
 * an interruptible cond. Such a signal only marks the thread.
 */
static inline int CondSignalDeferred(struct ThreadCB *thread, ThreadmgrCond *cond)
{
	uint32_t status = thread->flags;

	if (!(status >> 28))
		return 0;
	if (((status & THREAD_FLAG_REQUEST_VM_SUSPEND) || !(cond->attr & COND_ATTR_UNINTERRUPTIBLE)) &&
	    !ThreadmgrInSyscall())
		return 0;
	return 1;
}

/* wakes the first waiter */
int sceKernelSignalCond(SceUID condId)
{
	struct ThreadCB *thread, *first;
	ThreadmgrCond *cond;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 845, "sceKernelSignalCond", "*** error: illegal context. condId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(condId, s_threadmgrRuntime.manager->condClass,
					   (SceObjectBase **)&cond);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 860, "sceKernelSignalCond",
				"*** error: unknown condvar id. condId=0x%08x\n", condId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&cond->lock);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (CondSignalDeferred(thread, cond)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
		goto unlock;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	first = (struct ThreadCB *)cond->next;
	ret = 0;
	if ((struct ThreadmgrWaitQueue *)first == (struct ThreadmgrWaitQueue *)&cond->next)
		goto unlock;
	SCE_DBG_ASSERT_CHECK_FUNC(first->magic == THREAD_CB_MAGIC, 887, "sceKernelSignalCond");
	sceKernelSpinlockLowLock(THREAD_LOCK(first));
	wake = wakeupAndDistributeThread(first, (uintptr_t)&cond->next, 0);
	sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* wakes every waiter */
int sceKernelSignalCondAll(SceUID condId)
{
	struct ThreadCB *thread, *first;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrCond *cond;
	uint32_t saved, wake;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 938, "sceKernelSignalCondAll", "*** error: illegal context. condId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(condId, s_threadmgrRuntime.manager->condClass,
					   (SceObjectBase **)&cond);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 953, "sceKernelSignalCondAll",
				"*** error: unknown condvar id. condId=0x%08x\n", condId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&cond->lock);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (CondSignalDeferred(thread, cond)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ret = 0;
		goto unlock;
	}
	queue = (struct ThreadmgrWaitQueue *)&cond->next;
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ret = 0;
	if (queue->next == (struct ThreadmgrWaitNode *)queue)
		goto unlock;
	wake = 0;
	do {
		first = (struct ThreadCB *)queue->next;
		SCE_DBG_ASSERT_CHECK_FUNC(first->magic == THREAD_CB_MAGIC, 979, "sceKernelSignalCondAll");
		sceKernelSpinlockLowLock(THREAD_LOCK(first));
		wake |= wakeupAndDistributeThread(first, (uintptr_t)queue, 0);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(first));
	} while (queue->next != (struct ThreadmgrWaitNode *)queue);
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the SDK version SignalCondTo judges the calling process by */
static inline SceUInt32 CondCallerSdkVersion(struct ThreadCB *thread,
					     SceUInt32 *version)
{
	if (thread->processId == THREADMGR_KERNEL_PID)
		*version = SDK_VERSION_KERNEL;
	else if (SceSysrootForDriver_CAE820B2(0, version) < 0)
		*version = SDK_VERSION_SIGNAL_TO_STRICT;
	return *version;
}

/*
 * Wakes one given waiter. From SDK 0x02000000 on an unknown or non-waiting
 * thread is an error (two kernel conds of the I/O scheduler and PFS are
 * exempt from the message).
 */
int sceKernelSignalCondTo(SceUID condId, SceUID threadId)
{
	struct ThreadCB *thread, *waiter;
	struct ThreadmgrWaitNode *node, *next;
	struct ThreadmgrWaitQueue *queue;
	ThreadmgrCond *cond;
	SceUInt32 sdkVersion;
	const char *name;
	uint32_t saved, wake;
	SceObjectBase *object;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1036, "sceKernelSignalCondTo", "*** error: illegal context. condId=0x%08x[%s]\n",
				      condId, SceSysmemForDriver_E655852F(condId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || thread->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	if (sceGUIDGetObjectWithClass(threadId, s_threadmgrRuntime.manager->threadClass,
				       &object) < 0 &&
	    CondCallerSdkVersion(thread, &sdkVersion) >= SDK_VERSION_SIGNAL_TO_STRICT) {
		SCE_DBG_PRINTF_FUNC(0, 1056, "sceKernelSignalCondTo", "*** error: sceKernelSignalCondTo(0x%08X[%s], 0x%08X) "
			"returns SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID\n", condId, SceSysmemForDriver_E655852F(condId),
			threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(condId, s_threadmgrRuntime.manager->condClass,
					   (SceObjectBase **)&cond);
	if (ret < 0) {
		if (ThreadmgrInSyscall()) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID | SYSCALL_ERROR_BIT;
		} else {
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1072, "sceKernelSignalCondTo",
				"*** error: unknown condvar id. condId=0x%08x\n", condId);
			ret = SCE_KERNEL_ERROR_UNKNOWN_COND_ID;
		}
		goto resume;
	}
	sceKernelSpinlockLowLock(&cond->lock);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (CondSignalDeferred(thread, cond)) {
		SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto unlock;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	queue = (struct ThreadmgrWaitQueue *)&cond->next;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = next) {
		waiter = (struct ThreadCB *)node;
		next = node->next;
		SCE_DBG_ASSERT_CHECK_FUNC(waiter->magic == THREAD_CB_MAGIC, 1100, "sceKernelSignalCondTo");
		sceKernelSpinlockLowLock(THREAD_LOCK(waiter));
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
	/* the thread does not wait on this cond */
	if (CondCallerSdkVersion(thread, &sdkVersion) >= SDK_VERSION_SIGNAL_TO_STRICT) {
		name = NULL;
		if (sceGUIDGetName(condId, &name) < 0 ||
		    (strncmp("SceIoScheduler", name, 31) != 0 &&
		     strncmp("ScePfsCryptEngineDoneCnd", name, 31) != 0))
			SCE_DBG_PRINTF_FUNC(0, 1131, "sceKernelSignalCondTo", "*** error: sceKernelSignalCondTo(0x%08X[%s], "
				"0x%08X[%s]) returns SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID\n", condId,
				name, threadId, SceSysmemForDriver_E655852F(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	}
unlock:
	sceKernelSpinlockLowUnlock(&cond->lock);
	sceGUIDReleaseObject(condId);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_D1FBDFA6: a cond's information; a syscall sees the mutex's PUID */
int SceThreadmgrForDriver_D1FBDFA6(SceUID condId, SceKernelCondInfo *info)
{
	SceKernelCondInfo local;
	ThreadmgrMutex *mutex;
	uint32_t saved;
	SceSize size;
	SceUID mutexId;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getCondInfo(condId, &local);
		if (ret >= 0) {
			mutexId = local.mutexId;
			if (ThreadmgrInSyscall()) {
				if (sceGUIDReferObjectWithClass(mutexId,
						s_threadmgrRuntime.manager->mutexClass,
						(SceObjectBase **)&mutex) < 0) {
					local.mutexId = -1;
				} else {
					local.mutexId = mutex->core.puid;
					sceGUIDReleaseObject(mutexId);
				}
			}
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_67022B18: the information of every cond visible at level */
int SceThreadmgrForDriver_67022B18(int level, SceKernelCondInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelCondInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(s_threadmgrRuntime.manager->condClass, level,
					  NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->condClass, level, vector, num, &got);
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
		if (getCondInfo(vector[i], &info) != 0)
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

/* SceThreadmgrForDriver_CA704239: the same for a process's cond PUIDs (mutexes as GUID PUIDs) */
int SceThreadmgrForDriver_CA704239(SceUID pid, int level, SceKernelCondInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelCondInfo info;
	ThreadmgrMutex *mutex;
	SceSize got = 0;
	SceUID *vector, mutexId;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, s_threadmgrRuntime.manager->condClass,
					  level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid,
			s_threadmgrRuntime.manager->condClass, level, vector, num, &got);
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
		if (getCondInfo(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.condId = vector[i];
		mutexId = info.mutexId;
		if (sceGUIDReferObjectWithClass(mutexId,
				s_threadmgrRuntime.manager->mutexClass,
				(SceObjectBase **)&mutex) < 0) {
			info.mutexId = -1;
		} else {
			info.mutexId = mutex->core.puid;
			sceGUIDReleaseObject(mutexId);
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
	return ret;
}
