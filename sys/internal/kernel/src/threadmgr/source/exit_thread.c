/*
 * SceKernelThreadMgr (threadmgr.c): the end of a thread. exitDeleteThread
 * runs on the thread itself: it tells the debugger and the tracers, lets go
 * of everything the thread holds, wakes the threads waiting for its end and
 * leaves the CPU for good, dormant or (deleted) for the cleaner thread.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "event_flag.h"
#include "mutex.h"
#include "priority_change.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREAD_ATTR_USER		0x80000000
#define THREAD_EVENT_EXIT		0x00000008	/* callThreadEventHandlers */
#define WAIT_TYPE_THREAD_END		0x00000002

/* the debugger's thread events (debugHandlers[group][DEBUG_HANDLER_THREAD_EVENT]) */
#define DEBUG_THREAD_EVENT_DELETE	1
#define DEBUG_THREAD_EVENT_EXIT		3	/* the exit status comes with it */

/* the tracer's events (s_threadmgrRuntime.threadTraceHook) */
#define TRACE_EVENT_THREAD_EXIT		3
#define TRACE_EVENT_THREAD_EXIT_DELETE	4

#define THREAD_STATE_HOOK_EXIT		3

typedef void (*ThreadmgrStateHook)(SceSize size, void *message);
typedef void (*ThreadmgrLwpExitProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				      SceUID threadId, SceUID processId);
typedef void (*ThreadmgrExitProbe)(const struct ThreadmgrSchedProbeDescriptor *probe,
				   int status);

/* what the thread state hook (paHooks[0]) hears */
struct ThreadmgrThreadStateMessage {
	SceUID processId;
	SceUID threadId;
	uint32_t state;				/* THREAD_STATE_HOOK_* */
	int priority;
};

_Static_assert(offsetof(struct SceKernelThreadmgrPrivate, deletedNext) == 0x2218 &&
	       offsetof(struct SceKernelThreadmgrPrivate, dormantNext) == 0x2230 &&
	       offsetof(struct SceKernelThreadmgrPrivate, cleanerThread) == 0x2260 &&
	       offsetof(SceProcessObject, exitThread) == 0x70,
	       "thread end lists");

int callThreadEventHandlers(uint32_t mask);
void deleteAllCallback(struct ThreadCB *thread);
uint32_t releaseAllFastMutex(struct ThreadCB *thread);
uint32_t releaseAllRWLock(struct ThreadCB *thread);
int releaseThreadTlsSlots(struct ThreadCB *thread);
int syslibtraceThreadCBExit(struct ThreadCB *thread);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);

static uint32_t s_procLwpExitProbeId;
volatile uintptr_t s_procLwpExitHandler;
extern const char s_procLwpExitCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procLwpExitDescriptor = {
	.probeId = &s_procLwpExitProbeId,
	.provider = "__proc_",
	.name = "lwp__exit",
	.codeSite = s_procLwpExitCodeSite,
	.handler = &s_procLwpExitHandler,
};

static uint32_t s_procExitProbeId;
volatile uintptr_t s_procExitHandler;
extern const char s_procExitCodeSite[];
const struct ThreadmgrSchedProbeDescriptor s_procExitDescriptor = {
	.probeId = &s_procExitProbeId,
	.provider = "__proc_",
	.name = "exit",
	.codeSite = s_procExitCodeSite,
	.handler = &s_procExitHandler,
};

void notifyDebugThreadEvent(SceUID processId, SceUID threadId, uint32_t event, int *status);

/* the thread goes at the end of one of the manager's lists */
static inline void listThread(struct ThreadCB *thread, uintptr_t *list)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	struct ThreadmgrWaitNode *head = (struct ThreadmgrWaitNode *)list;

	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
}

/* wakes the cleaner thread to delete what is on the deleted list */
static inline void activateCleanerThread(void)
{
	struct ThreadCB *cleaner;

	SCE_DBG_ASSERT_CHECK_FUNC(ThreadmgrIntrDisabled(), 1770, "activateCleanerThread");
	cleaner = s_threadmgrRuntime.manager->cleanerThread;
	ksceKernelSpinlockLowLock(THREAD_LOCK(cleaner));
	if (cleaner->status & SCE_KERNEL_THREAD_STATUS_WAITING)
		distributeThread(selectExecCpu(cleaner), cleaner);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(cleaner));
}

/* ends the running thread with status; delete: it is deleted too */
void exitDeleteThread(int delete, int intr, int status)
{
	struct ThreadCB *thread;
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrWaitNode *head, *node;
	struct ThreadmgrThreadStateMessage message;
	ThreadmgrLwpExitProbe lwpExit;
	ThreadmgrExitProbe exit;
	ThreadmgrStateHook stateHook;
	ThreadmgrTraceHook trace;
	SceProcessObject *process;
	SceUID threadId;
	int debugStatus;

	ksceKernelCpuSuspendIntr();
	thread = currentThreadContext();
	SCE_DBG_ASSERT_LEVEL(0, thread != NULL, 4944);
	callThreadEventHandlers(THREAD_EVENT_EXIT);
	debugStatus = status;
	notifyDebugThreadEvent(thread->processId, thread->userThreadId, DEBUG_THREAD_EVENT_EXIT,
			       &debugStatus);
	/* the DTrace probe site __proc_:lwp__exit */
	__asm__ volatile(".global s_procLwpExitCodeSite\ns_procLwpExitCodeSite:");
	lwpExit = (ThreadmgrLwpExitProbe)s_procLwpExitHandler;
	if (lwpExit != NULL) {
		threadId = thread->userThreadId;
		if (threadId < 0)
			threadId = thread->threadId;
		lwpExit(&s_procLwpExitDescriptor, threadId,
			thread->process->pid);
	}
	process = thread->process;
	if (process->exitThread == thread->threadId) {
		/* the DTrace probe site __proc_:exit */
		__asm__ volatile(".global s_procExitCodeSite\ns_procExitCodeSite:");
		exit = (ThreadmgrExitProbe)s_procExitHandler;
		if (exit != NULL)
			exit(&s_procExitDescriptor, status);
	}
	if (delete)
		notifyDebugThreadEvent(thread->processId, thread->userThreadId,
				       DEBUG_THREAD_EVENT_DELETE, NULL);
	stateHook = (ThreadmgrStateHook)s_threadmgrRuntime.manager->paHooks[0];
	if (stateHook != NULL) {
		message.processId = thread->processId;
		message.threadId = thread->threadId;
		message.state = THREAD_STATE_HOOK_EXIT;
		message.priority = thread->currentPriority;
		stateHook(sizeof(message), &message);
	}

	/* let go of what it holds */
	releaseAllEventFlag(thread);
	releaseAllMutex(thread);
	releaseAllFastMutex(thread);
	releaseAllRWLock(thread);
	deleteAllCallback(thread);

	object = (struct ThreadmgrThreadObject *)((uintptr_t)thread -
		offsetof(struct ThreadmgrThreadObject, context));
	ksceKernelSpinlockLowLock(&object->waitLock);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	thread->exitStatus = status;
	releaseThreadTlsSlots(thread);
	if (thread->restartWaitMultipleEvents != NULL) {
		SCE_DBG_ASSERT_CHECK((thread->flags & THREAD_FLAG_EXITING) != 0, 4991);
		SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 4992);
		SceSysrootForKernel_7968F091(thread->processId, thread->restartWaitMultipleEvents);
	}
	thread->restartWaitMultipleEvents = NULL;
	if (delete) {
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		if (thread->syslibtraceState != NULL)
			syslibtraceThreadCBExit(thread);
		listThread(thread, (uintptr_t *)&s_threadmgrRuntime.manager->deletedNext);
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	} else {
		thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DORMANT;
		ksceKernelSpinlockLowLock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		listThread(thread, (uintptr_t *)&s_threadmgrRuntime.manager->dormantNext);
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
	}
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), 5019);
	thread->currentCpuId = -1;
	thread->waitId = -1;

	/* the threads waiting for its end */
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	for (node = head->next; node != head; node = head->next) {
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, 5026);
		ksceKernelSpinlockLowLock(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_END) {
			if (((struct ThreadCB *)node)->waitArg0 != 0)
				*(int *)((struct ThreadCB *)node)->waitArg0 = status;
			wakeupAndDistributeThread((struct ThreadCB *)node,
						  (uintptr_t)head, 0);
		}
		ksceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	ksceKernelSpinlockLowUnlock(&object->waitLock);

	if (delete) {
		activateCleanerThread();
		trace = s_threadmgrRuntime.threadTraceHook;
		if (trace != NULL)
			trace(TRACE_EVENT_THREAD_EXIT_DELETE, 3, status, thread->processId,
			      thread->threadId);
	} else {
		trace = s_threadmgrRuntime.threadTraceHook;
		if (trace != NULL)
			trace(TRACE_EVENT_THREAD_EXIT, 3, status, thread->processId,
			      thread->threadId);
	}
	currentCpuState()->nextRunningThread = 0;
	switchThread(2);
	SCE_DBG_PRINTF(0xb, 5068, " fatal error: do not reach here.(threadId=0x%08X)\n",
		       thread->threadId);
	__asm__ volatile("bkpt #0");
	SCE_PANIC(SCE_LINE(5065, 5071));
}
