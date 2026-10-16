/*
 * SceKernelThreadMgr (threadmgr.c): the thread manager's part in a
 * process's life. sceKernelThreadMgrProcessCreate gives a new process its
 * lists, its msg pipe lock, its worker thread (which suspends it for the
 * debugger and keeps its object caches filled) and its exception object;
 * the two delete hooks take everything down again.
 */

#include <psp2common/kernel/threadmgr.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "timer.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define WORKER_EVENT_SUSPEND		0x00000001	/* debugEventFlag: suspend the process */
#define WORKER_EVENT_EXIT		0x80000000	/* debugEventFlag: the worker ends */
#define WORKER_WAIT_MODE		3		/* OR, clear */
#define WORKER_IDLE_USEC		16000
#define WORKER_PRIORITY			0xfe
#define WORKER_STACK_SIZE		0x1000
#define WORKER_CPU_AFFINITY		0xf
#define PERMISSION_WORKER		0x80		/* PLACEHOLDER: the permission it is created with */
#define PROCESS_DEBUG_SUSPEND_EXCEPTION	0x11		/* PLACEHOLDER: SceSysrootForKernel_1247A825's reason */
#define SCE_KERNEL_ERROR_WAIT_TIMEOUT	0x80028005
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_DORMANT	0x80028027
#define SCE_KERNEL_ERROR_NOT_SUSPENDED	0x8002802e
#define SCE_KERNEL_ERROR_UNKNOWN_UID	0x80028001	/* PLACEHOLDER: name */

typedef void (*ThreadmgrProcessDebugSuspendHook)(SceUID pid, SceUID threadId, int kind,
						 uintptr_t arg);

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern int ksceGUIDGetName(SceUID guid, const char **name);
extern int ksceGUIDGetObject(SceUID guid, SceObjectBase **object);
extern int ksceGUIDReferObjectWithSubclass(SceUID uid, SceClass *cls, SceObjectBase **object);
extern int SceSysrootForKernel_1247A825(SceUID pid, uint32_t reason);
extern int snprintf(char *str, SceSize size, const char *format, ...);
int stopThread(SceUID threadId, int priority, uint32_t request);
int deleteThread(struct ThreadCB *thread);
int waitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout, int callbacks);
int startThread(SceUID threadId, SceSize argSize, const void *arg, int checkSource);
SceUID ksceKernelCreateThread(const char *name, uintptr_t entry, int initPriority, SceSize stackSize,
			      uint32_t attr, uint32_t cpuAffinityMask, const SceKernelThreadOptParam *option);
int ksceKernelDeleteThread(SceUID threadId);
int ksceKernelInitializeFastMutex(void *mutex, const char *name, int attr, const void *option);
SceUID ksceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				 const void *option);
int ksceKernelSetEventFlag(SceUID evfId, uint32_t pattern);
int ksceKernelDeleteEventFlag(SceUID evfId);
int ksceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			    SceUInt32 *timeout);
int createException(SceProcessObject *process);
int deleteException(SceProcessObject *process);
int uidLwMutex_DeleteCache(SceProcessObject *process, int target);
int uidLwMutex_ManageCache(SceProcessObject *process, int high, int low, int target);
int eventFlagProcessExit(SceProcessObject *process);
int semaphoreProcessExit(SceProcessObject *process);
int condProcessExit(SceProcessObject *process);
int mutexProcessExit(SceProcessObject *process);
int rwLockProcessExit(SceProcessObject *process);
int lwCondProcessExit(SceProcessObject *process);
int lwMutexProcessExit(SceProcessObject *process);
int msgPipeProcessExit(SceProcessObject *process);
int simpleEventProcessExit(SceProcessObject *process);
int threadEventProcessExit(SceProcessObject *process);
int callbackProcessExit(SceProcessObject *process);

/* SceThreadmgrForKernel_9928DFBA: ends another thread */
int SceThreadmgrForKernel_9928DFBA(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_EXIT);
}

/* SceThreadmgrForKernel_800ACB9D: ends and deletes another thread */
int SceThreadmgrForKernel_800ACB9D(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_EXIT_DELETE);
}

/* SceThreadmgrForKernel_15D6357C: suspends another thread */
int SceThreadmgrForKernel_15D6357C(SceUID threadId, int priority)
{
	return stopThread(threadId, priority, THREAD_FLAG_REQUEST_SUSPEND);
}

/* SceThreadmgrForKernel_490DAF14: resumes a suspended thread */
int SceThreadmgrForKernel_490DAF14(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved;
	int intr, ret, wake;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (threadId == 0 ||
	    (currentThreadContext() != NULL && currentThreadContext()->threadId == threadId)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if ((uint16_t)thread->status == SCE_KERNEL_THREAD_STATUS_DORMANT) {
		ret = SCE_KERNEL_ERROR_DORMANT;
	} else if ((uint16_t)thread->status == SCE_KERNEL_THREAD_STATUS_SUSPENDED &&
		   (thread->flags & THREAD_FLAG_SUSPEND_COUNTED)) {
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_RESUME_CLEAR);
		thread->currentPriority = thread->basePriority < thread->boostPriority ?
			thread->basePriority : thread->boostPriority;
		if (thread->vmSuspendCount < 1) {
			/* runs again */
			ksceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			((struct ThreadmgrWaitNode *)thread->next)->prev =
				(struct ThreadmgrWaitNode *)thread->prev;
			((struct ThreadmgrWaitNode *)thread->prev)->next =
				(struct ThreadmgrWaitNode *)thread->next;
			ksceKernelSpinlockLowUnlock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			wake = distributeThread(selectExecCpu(thread), thread);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceGUIDReleaseObject(threadId);
			if ((wake & 1) && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			goto resume;
		}
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_NOT_SUSPENDED;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* sceKernelCountWaitingThreadsProc: how many threads of process pid wait on waitId */
int sceKernelCountWaitingThreadsProc(SceUID pid, SceUID waitId)
{
	struct ThreadmgrWaitQueue *queue;
	struct ThreadmgrWaitNode *node;
	struct ThreadCB *thread;
	SceObjectBase *processObject;
	int intr, ret;

	ret = ksceGUIDGetObject(pid, &processObject);
	if (ret < 0)
		return ret;
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithSubclass(waitId,
		s_threadmgrRuntime.manager->waitableClass, (SceObjectBase **)&queue);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_UID;
		goto resume;
	}
	queue = (struct ThreadmgrWaitQueue *)((uintptr_t)queue + 0x10);
	ksceKernelSpinlockLowLock(&queue->lock);
	ret = 0;
	for (node = queue->next; node != (struct ThreadmgrWaitNode *)queue; node = node->next) {
		thread = (node->owner & 1) ? (struct ThreadCB *)node :
			(struct ThreadCB *)node->owner;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 10004);
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		if (thread->processId == pid)
			ret++;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	ksceKernelSpinlockLowUnlock(&queue->lock);
	ksceGUIDReleaseObject(waitId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/*
 * A process's worker thread: suspends the process when a thread asks for
 * it (debugEventFlag), and while idle keeps its object caches filled.
 */
int processWorkerThread(SceSize argSize, void *arg)
{
	SceProcessObject *process = *(SceProcessObject **)arg;
	volatile uint32_t *state = &process->status;
	ThreadmgrProcessDebugSuspendHook hook;
	uint32_t pattern;
	SceUInt32 timeout;
	int ret, intr;

	(void)argSize;
	for (;;) {
		pattern = 0;
		timeout = WORKER_IDLE_USEC;
		ret = ksceKernelWaitEventFlag(process->abortEventFlag,
					      WORKER_EVENT_EXIT | WORKER_EVENT_SUSPEND,
					      WORKER_WAIT_MODE, &pattern, &timeout);
		if (ret != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT && ret < 0)
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(10238, 10243),
				"*** fatal error: processWorkerThread() -> sceKernelWaitEventFlag(0x%08X)=0x%08X\n",
				process->abortEventFlag, ret);
		if (ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			/* idle: refill the caches */
			if ((*state & PROCESS_STATE_RUNNING) &&
			    !(*state & PROCESS_FLAG_EXIT)) {
				if (process->pid == THREADMGR_KERNEL_PID) {
					ret = uidDelay_ManageCache(0x60, 0x20, 0x40);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10277,
							"*** error: processWorkerThread() -> uidDelay_ManageCache()=0x%08X\n",
							ret);
					ret = uidAlarm_ManageCache(0x20, 0x10, 0x30);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10282,
							"*** error: processWorkerThread() -> uidAlarm_ManageCache()=0x%08X\n",
							ret);
				} else {
					ret = uidLwMutex_ManageCache(process, 0x80, 0x40, 0x60);
					if (ret < 0)
						SCE_DBG_PRINTF(0, 10271,
							"*** error: processWorkerThread()[id=0x%08X] -> uidLwMutex_ManageCache()=0x%08X\n",
							process->pid, ret);
				}
			}
			continue;
		}
		if (pattern & WORKER_EVENT_EXIT) {
			if (process->pid != THREADMGR_KERNEL_PID)
				uidLwMutex_DeleteCache(process, 0);
			return 0;
		}
		if (pattern & WORKER_EVENT_SUSPEND) {
			ret = SceSysrootForKernel_1247A825(process->pid, PROCESS_DEBUG_SUSPEND_EXCEPTION);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 10256,
					"(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n", process->pid,
					ret);
			} else if (s_threadmgrRuntime.manager->processDebugSuspendHook != 0) {
				intr = ksceKernelCpuSuspendIntr();
				hook = (ThreadmgrProcessDebugSuspendHook)
					s_threadmgrRuntime.manager->processDebugSuspendHook;
				hook(process->pid, process->abortThread, process->abortCode,
				     s_threadmgrRuntime.manager->processDebugSuspendArg);
				ksceKernelCpuResumeIntr(intr);
			}
		}
	}
}

/* sceKernelThreadMgrProcessCreate: a new process's thread manager state */
int sceKernelThreadMgrProcessCreate(SceProcessObject *process)
{
	struct ThreadCB *self;
	const char *processName;
	char name[32];
	int ret, permission;

	process->timers.next = &process->timers;
	process->timers.prev = &process->timers;
	process->threads.next = &process->threads;
	process->threads.prev = &process->threads;
	process->timerLock = 0;
	ret = ksceKernelInitializeFastMutex(&process->msgPipeLock, "SceKernelMsgPipeListLock", 0,
					    NULL);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(10353);
		return ret;
	}
	process->lwMutexCacheLock = 0;
	process->msgPipes.next = &process->msgPipes;
	process->msgPipes.prev = &process->msgPipes;
	process->lwMutexCache = NULL;
	process->lwMutexCacheCount = 0;
	ret = ksceGUIDGetName(process->pid, &processName);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 10368,
			"*** error: sceKernelThreadMgrProcessCreate() -> sceGUIDGetName(0x%08X)=0x%08X\n",
			process->pid, ret);
	snprintf(name, sizeof(name), "tworker: %s", processName);
	process->abortEventFlag = ksceKernelCreateEventFlag(name, 0, 0, NULL);
	if (process->abortEventFlag < 0) {
		SCE_DBG_ASSERT_FAIL(10380);
		return ret;
	}
	/* the worker is created with the kernel's permission */
	self = currentThreadContext();
	if (self == NULL) {
		process->workerThread = ksceKernelCreateThread(name,
			(uintptr_t)processWorkerThread, WORKER_PRIORITY, WORKER_STACK_SIZE, 0,
			WORKER_CPU_AFFINITY, NULL);
		self = currentThreadContext();
	} else {
		permission = self->permission;
		self->permission = PERMISSION_WORKER;
		process->workerThread = ksceKernelCreateThread(name,
			(uintptr_t)processWorkerThread, WORKER_PRIORITY, WORKER_STACK_SIZE, 0,
			WORKER_CPU_AFFINITY, NULL);
		self = currentThreadContext();
		if (permission >= 0 && self != NULL)
			self->permission = permission;
	}
	if (process->workerThread < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 10396,
			"*** error: sceKernelCreateThread(\"%s\")=0x%08X, caller processId=0x%08X\n",
			name, process->workerThread, currentThreadContext()->processId);
		return ret;
	}
	ret = startThread(process->workerThread, sizeof(process), &process, 0);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(10402);
		return ret;
	}
	ret = createException(process);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(10409);
	return ret;
}

/* a process's threads are deleted with it (its thread list locked) */
static inline void threadProcessExit(SceProcessObject *process)
{
	ListNode *node;
	struct ThreadCB *thread;
	SceObjectBase *object;

	for (node = process->threads.next; node != &process->threads; node = node->next) {
		thread = THREAD_FROM_PROCESS_LINK(node);
		if (ksceGUIDReferObjectWithClass(thread->threadId,
			s_threadmgrRuntime.manager->threadClass, &object) < 0)
			SCE_PANIC_FUNC(SCE_LINE(10310, 10315), "threadProcessExit");
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		deleteThread(thread);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (ksceGUIDReleaseObject(thread->threadId) < 0)
			SCE_PANIC_FUNC(SCE_LINE(10320, 10325), "threadProcessExit");
	}
}

/* sceKernelThreadMgrProcessDeleteBeforePUIDClose: the process's objects go */
int sceKernelThreadMgrProcessDeleteBeforePUIDClose(SceProcessObject *process)
{
	int intr, ret;

	ret = eventFlagProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10425);
	ret = semaphoreProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10427);
	ret = condProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10429);
	ret = mutexProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10431);
	ret = rwLockProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10433);
	ret = lwCondProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10436);
	ret = lwMutexProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10438);
	ret = msgPipeProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10441);
	ret = timerProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10443);
	ret = simpleEventProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10445);
	ret = threadEventProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10448);
	ret = callbackProcessExit(process);
	SCE_DBG_ASSERT_LEVEL(0, ret >= 0, 10450);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&process->lock);
	threadProcessExit(process);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&process->lock, intr);
	SCE_DBG_ASSERT_LEVEL(0, 1, 10452);
	return 0;
}

/* sceKernelThreadMgrProcessDeleteAfterPUIDClose: its exception object and worker go */
int sceKernelThreadMgrProcessDeleteAfterPUIDClose(SceProcessObject *process)
{
	int ret;

	ret = deleteException(process);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 10466);
	if (process->abortEventFlag == -1)
		return ret;
	ret = ksceKernelSetEventFlag(process->abortEventFlag, WORKER_EVENT_EXIT);
	SCE_DBG_ASSERT_PRINTF(ret >= 0, 10480,
		"Fatal Error: sceKernelSetEventFlag(0x%08x) failed 0x%08x\n",
		process->abortEventFlag, ret);
	if (process->workerThread != -1) {
		waitThreadEnd(process->workerThread, NULL, NULL, 0);
		ret = ksceKernelDeleteThread(process->workerThread);
		if (ret != 0)
			SCE_DBG_PRINTF(0, 10486,
				" k: error: sceKernelThreadMgrProcessDeleteAfterPUIDClose() -> sceKernelDeleteThread()=0x%08X\n",
				ret);
	}
	ret = ksceKernelDeleteEventFlag(process->abortEventFlag);
	if (ret != 0) {
		SCE_DBG_PRINTF(0, 10491,
			" k: error: sceKernelThreadMgrProcessDeleteAfterPUIDClose() -> sceKernelDeleteEventFlag()=0x%08X\n",
			ret);
		return ret;
	}
	return 0;
}
