/*
 * SceKernelThreadMgr (threadmgr.c): the life of a thread after it is
 * created. threadEntry is where a new thread first runs; it ends through
 * sceKernelExitThread / sceKernelExitDeleteThread (exitDeleteThread), is
 * deleted by another thread (deleteThread) or by the cleaner thread, and
 * its UID object's destructor frees what is left.
 */

#include <string.h>
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

#define THREAD_ATTR_USER		0x80000000
#define THREAD_ATTR_VM			0x04000000	/* PLACEHOLDER: a VM's thread */
#define THREAD_ATTR_UNK4000		0x00004000	/* PLACEHOLDER: counted apart in its process */
#define THREAD_EVENT_START		0x00000004	/* callThreadEventHandlers */
#define TRAP_REQUEST_DEBUG_SUSPEND	0x02000000
#define DEBUG_THREAD_EVENT_DELETE	1
#define TRACE_EVENT_THREAD_DELETE	1
#define CPSR_SYS_ARM			0x5f	/* SYS mode, FIQ masked */
#define CPSR_SYS_THUMB			0x7f
#define DIPSW_FREE_RETRY_LOG		0x9f
#define FREE_RETRY_LIMIT		100000
#define FREE_RETRY_LOG_EVERY		1000
#define SCE_KERNEL_ERROR_CAN_NOT_WAIT	0x80028004
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_WAIT_CANCEL	0x80028007
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_NOT_DORMANT	0x80028028


extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceGUIDClose(SceUID guid);
extern int kscePUIDClose(SceUID pid, SceUID puid);
extern int ksceKernelFreeMemBlock(SceUID uid);
extern int ksceKernelCheckDipsw(unsigned int bit);
extern int ksceKernelSpinlockLowTrylock(SceKernelSpinlock *lock);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);
extern uint32_t SceIntrmgrForKernel_08FDB0CE(struct ThreadRegisterContext *context, uint32_t flags, uintptr_t vfp);
int callThreadEventHandlers(uint32_t mask);
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request);
void __attribute__((noreturn)) exitDeleteThread(int delete, int intr, int status);
int releaseThreadTlsSlots(struct ThreadCB *thread);
int dtraceThreadCBExit(struct ThreadCB *thread);
int syslibtraceThreadCBExit(struct ThreadCB *thread);
int ksceKernelExitThread(int status);
void kernelThreadReturnEntry(int status);

/* the thread goes from the list it is on to the manager's closed list */
static inline void listClosed(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;

	self->next->prev = self->prev;
	self->prev->next = self->next;
	head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->closedNext;
	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
}

/* the thread leaves its process */
static inline void unlistFromProcess(struct ThreadCB *thread,
				     SceProcessObject *process)
{
	ListNode *next = thread->processLink.next;

	next->prev = thread->processLink.prev;
	thread->processLink.prev->next = next;
	if (thread->currentAttr & THREAD_ATTR_UNK4000)
		process->unlimitedThreadCount--;
	else
		process->threadCount--;
}

/*
 * Where a kernel thread's entry function returns to: the dispatch is
 * enabled again and the thread exits with the returned status.
 */
void kernelThreadReturn(int status)
{
	struct ThreadMgrCpuCB *cpu;
	int intr;

	intr = ksceKernelCpuSuspendIntr();
	cpu = currentCpuState();
	if (cpu->disabledDispatch != 0)
		cpu->disabledDispatch = 0;
	ksceKernelCpuResumeIntr(intr);
	ksceKernelExitThread(status);
}

/*
 * A new thread's first code: the thread event handlers, then its entry
 * function (a kernel thread) or its user context (a user thread).
 */
void threadEntry(void)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadRegisterContext *regs, *context;
	uintptr_t entry, vfp;
	uint32_t request;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 4821);
	if ((int)thread->currentAttr < 0)
		goto again;
	for (;;) {
		SCE_DBG_ASSERT_CHECK(thread->userFrame == NULL, 4827);
again:
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_IN_CALLBACK);
		callThreadEventHandlers(THREAD_EVENT_START);
		if ((int)thread->currentAttr < 0)
			break;
		/* a kernel thread: its entry function with the start argument */
		regs = thread->pRegisterContext;
		entry = thread->entry;
		regs->full = 1;
		regs->r[1] = thread->pArgBlock;
		regs->r[0] = thread->argSize;
		regs->pc = entry & ~1;
		regs->sp = thread->initSp;
		regs->lr = (uintptr_t)kernelThreadReturnEntry;
		regs->cpsr = (entry & 1) ? CPSR_SYS_THUMB : CPSR_SYS_ARM;
		SceIntrmgrForKernel_08FDB0CE(regs, 0, thread->pVfpRegisterContext);
		SCE_DBG_ASSERT_CHECK((thread->flags >> 16) & 1, 4825);
	}
	/* a user thread: the context _sceKernelStartThread prepared */
	thread->userFrame = NULL;
	thread->kernelStackTop = thread->pKStack + thread->kStackSize;
	vfp = thread->pRegisterContext->callbackVfp;
	context = thread->pRegisterContext->callbackContext;
	thread->pRegisterContext->callbackVfp = 0;
	thread->pRegisterContext->callbackContext = NULL;
	request = thread->flags & THREAD_FLAG_REQUEST_MASK;
	if (request == 0) {
		if (thread->debugSuspendReason == 0)
			goto load;
		request = TRAP_REQUEST_DEBUG_SUSPEND;
	}
	for (;;) {
		sceKernelTrapThreadAfterSyscall(NULL, NULL, request);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
					    THREAD_FLAG_RESUMED_WAIT);
load:
		request = SceIntrmgrForKernel_08FDB0CE(context, 0, vfp);
	}
}

/* ksceKernelExitThread: the running thread ends with status */
int ksceKernelExitThread(int status)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	uintptr_t current;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	current = (uintptr_t)currentThreadContext();
	if (current == 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 5089, "sceKernelExitThread",
					   "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	cpu = currentCpuState();
	if (thread->threadEventMask != 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(5097, 5103), "sceKernelExitThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			ksceGUIDName(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			SCE_PANIC_FUNC(SCE_LINE(5104, 5110), "sceKernelExitThread");
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (thread->callbackNest != 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0, SCE_LINE(5112, 5118), "sceKernelExitThread",
			"*** error: illegal context(callback). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			ksceGUIDName(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			goto panic;
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (cpu->disabledDispatch != 0) {
		ksceKernelCpuResumeIntr(intr);
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	exitDeleteThread(0, intr, status);
panic:
	SCE_PANIC_FUNC(SCE_LINE(5119, 5125), "sceKernelExitThread");
}

/* sceKernelExitDeleteThread: the running thread ends with status and is deleted */
int sceKernelExitDeleteThread(int status)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadMgrCpuCB *cpu;
	uint32_t saved;
	int intr;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 5168, "*** error: illegal context\n");
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = ksceKernelCpuSuspendIntr();
	cpu = currentCpuState();
	if (thread->threadEventMask != 0) {
		SCE_PRINTF_WITH_INFO(0, SCE_LINE(5176, 5182),
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			ksceGUIDName(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			SCE_PANIC(SCE_LINE(5183, 5189));
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (thread->callbackNest != 0) {
		SCE_PRINTF_WITH_INFO(0, SCE_LINE(5191, 5197),
			"*** error: illegal context(callback). threadId=0x%08x[%s]\n",
			currentThreadContext()->threadId,
			ksceGUIDName(currentThreadContext()->threadId));
		if ((int)thread->currentAttr >= 0)
			goto panic;
		debugSuspendThreadSelf(DEBUG_SUSPEND_EXIT_CALLBACK, 0, NULL);
		sceKernelTrapThreadAfterSyscall(NULL, NULL, TRAP_REQUEST_DEBUG_SUSPEND);
	}
	if (cpu->disabledDispatch != 0) {
		ksceKernelCpuResumeIntr(intr);
		ThreadmgrSetUserRoWord(saved);
		return SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	}
	exitDeleteThread(1, intr, status);
panic:
	SCE_PANIC(SCE_LINE(5198, 5204));
}

/* deletes a dormant thread (its process's thread list and its own lock held) */
int deleteThread(struct ThreadCB *thread)
{
	SceProcessObject *process = thread->process;
	SceUID processId = thread->processId, threadId = thread->threadId;
	ThreadmgrTraceHook trace;
	int ret;

	if (!(thread->status & (SCE_KERNEL_THREAD_STATUS_DORMANT | SCE_KERNEL_THREAD_STATUS_DEAD))) {
		ret = SCE_KERNEL_ERROR_NOT_DORMANT;
		goto out;
	}
	releaseThreadTlsSlots(thread);
	thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_DELETED;
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 4279);
	thread->currentCpuId = -1;
	if (thread->dtraceState != NULL)
		dtraceThreadCBExit(thread);
	if (thread->syslibtraceState != NULL)
		syslibtraceThreadCBExit(thread);
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	listClosed(thread);
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	if (thread->userThreadId != -1) {
		ret = kscePUIDClose(processId, thread->userThreadId);
		if (ret < 0) {
			SCE_DBG_ASSERT_LEVEL(0, 0, 4310);
			goto out;
		}
	}
	unlistFromProcess(thread, process);
	ret = ksceGUIDClose(thread->threadId);
	if (ret < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 4329);
		goto out;
	}
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_DELETE, 2, threadId, processId);
out:
	return ret;
}

/* ksceKernelDeleteThread: deletes another, dormant, thread */
int ksceKernelDeleteThread(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrDebugThreadEvent message;
	SceProcessObject *process;
	ThreadmgrDebugHandler handler;
	SceUID processId, userThreadId;
	uint32_t saved;
	int intr, ret, group;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4356, "sceKernelDeleteThread",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (currentThreadContext()->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4364, "sceKernelDeleteThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			threadId, ksceGUIDName(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || currentThreadContext()->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ?
			(int)(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT) :
			(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	userThreadId = object->context.userThreadId;
	process = object->context.process;
	processId = process->pid;
	ksceKernelSpinlockLowLock(&process->lock);
	ksceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	ret = deleteThread(&object->context);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	ksceKernelSpinlockLowUnlock(&process->lock);
	ksceGUIDReleaseObject(threadId);
	if (ret >= 0) {
		/* the debugger hears of the deletion */
		for (group = 0; group < 2; group++) {
			handler = s_threadmgrRuntime.manager->
				debugHandlers[group][0];
			if (handler == NULL)
				continue;
			message.processId = processId;
			message.threadId = userThreadId;
			message.event = DEBUG_THREAD_EVENT_DELETE;
			handler(sizeof(message), &message);
		}
	}
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * The cleaner thread: deletes the threads exitDeleteThread left on the
 * deleted list, and sleeps when there are none.
 */
void __attribute__((noreturn)) cleanerThread(void)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadCB *thread, *self;
	SceProcessObject *process;
	SceUID threadId;
	int intr, ret;

	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		manager = s_threadmgrRuntime.manager;
		thread = (struct ThreadCB *)manager->deletedNext;
		if ((uintptr_t)thread == (uintptr_t)&manager->deletedNext) {
			/* nothing to do: sleep until exitDeleteThread wakes us */
			ksceKernelSpinlockLowUnlockCpuResumeIntr(
				&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
			intr = ksceKernelCpuSuspendIntr();
			self = s_threadmgrRuntime.manager->cleanerThread;
			ksceKernelSpinlockLowLock(THREAD_LOCK(self));
			SCE_DBG_ASSERT_CHECK(self->currentCpuId == (int)ksceKernelCpuId(), 1985);
			self->currentCpuId = -1;
			self->waitId = -1;
			self->status = (self->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_WAITING;
			currentCpuState()->nextRunningThread = 0;
			switchThread(2);
			ksceKernelCpuResumeIntr(intr);
			continue;
		}
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 1911);
		threadId = thread->threadId;
		if (ksceKernelSpinlockLowTrylock(THREAD_LOCK(thread)) < 0)
			goto retry;
		process = thread->process;
		if (ksceKernelSpinlockLowTrylock(&process->lock) < 0) {
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto retry;
		}
		if (thread->dtraceState != NULL)
			dtraceThreadCBExit(thread);
		listClosed(thread);
		ksceKernelSpinlockLowUnlock(
			&s_threadmgrRuntime.manager->threadMgrCBLock);
		unlistFromProcess(thread, process);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceKernelSpinlockLowUnlock(&process->lock);
		if (thread->userThreadId != -1) {
			ret = kscePUIDClose(thread->processId, thread->userThreadId);
			if (ret < 0)
				SCE_DBG_PRINTF(0, 1966,
					"cleanerThread(): scePUIDClose=0x%08X, process(0x%08X) may be exited.\n",
					ret, thread->processId);
		}
		if (ksceGUIDClose(threadId) < 0)
			SCE_PANIC(SCE_LINE(1970, 1972));
		ksceKernelCpuResumeIntr(intr);
		continue;
retry:
		ksceKernelSpinlockLowUnlockCpuResumeIntr(
			&s_threadmgrRuntime.manager->threadMgrCBLock, intr);
	}
}


/* SceUIDThreadClass: a new thread object, dormant, its lists empty */
int uidThread_Constructor(struct ThreadmgrThreadObject *object)
{
	struct ThreadCB *thread = &object->context;

	memset(thread, 0, sizeof(*thread));
	thread->next = (uintptr_t)thread;
	thread->prev = (uintptr_t)thread;
	thread->status = SCE_KERNEL_THREAD_STATUS_DORMANT;
	thread->waitId = -1;
	thread->lock = 1;
	thread->callbackNext = (struct ThreadmgrCallbackCB *)&thread->callbackNext;
	thread->callbackPrev = (struct ThreadmgrCallbackCB *)&thread->callbackNext;
	thread->heldEventFlagNext = (uintptr_t)&thread->heldEventFlagNext;
	thread->heldEventFlagPrev = (uintptr_t)&thread->heldEventFlagNext;
	thread->ownedMutexNext = (uintptr_t)&thread->ownedMutexNext;
	thread->ownedMutexPrev = (uintptr_t)&thread->ownedMutexNext;
	thread->ownedFastMutexNext = (uintptr_t)&thread->ownedFastMutexNext;
	thread->ownedFastMutexPrev = (uintptr_t)&thread->ownedFastMutexNext;
	thread->ownedRWLockNext = (uintptr_t)&thread->ownedRWLockNext;
	thread->ownedRWLockPrev = (uintptr_t)&thread->ownedRWLockNext;
	return 0;
}

/* the thread object's destructor: its stacks, contexts and waiters */
int uidThread_Destructor(struct ThreadmgrThreadObject *object)
{
	struct ThreadCB *thread = &object->context;
	struct ThreadmgrExtendedStack *extended;
	SceUID memBlockId;
	int intr, ret, tries, wake;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 11736);
	if (!(thread->status & SCE_KERNEL_THREAD_STATUS_DELETED)) {
		SCE_DBG_PRINTF(0xb, 11743,
			" fatal error: thread status is not DELETED.(status = 0x%08X)\n",
			thread->status);
		SCE_PANIC(SCE_LINE(11739, 11744));
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(THREAD_LOCK(thread));
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	((struct ThreadmgrWaitNode *)thread->next)->prev = (struct ThreadmgrWaitNode *)thread->prev;
	((struct ThreadmgrWaitNode *)thread->prev)->next = (struct ThreadmgrWaitNode *)thread->next;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(THREAD_LOCK(thread), intr);
	for (extended = thread->extendedStacks; extended != NULL;
	     extended = thread->extendedStacks) {
		ret = ksceKernelFreeMemBlock(extended->memBlockId);
		SCE_DBG_PRINTF(0, 11762, "*** uidThread_Destructor(), free ExKernelStack(0x%08X)\n",
			       ret);
		thread->extendedStacks = thread->extendedStacks->next;
	}
	if (thread->kStackMemblkId != -1) {
		ret = ksceKernelFreeMemBlock(thread->kStackMemblkId);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11771,
				"*** error: uidThread_Destructor() -> sceKernelFreeMemBlock(0x%08X)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X\n",
				thread->kStackMemblkId, ret, thread, thread->magic);
			SCE_DBG_ASSERT(0, 11772);
			return 0;
		}
	}
	if ((int)thread->currentAttr < 0) {
		/* the user stack may still be in use for a while: retry */
		memBlockId = thread->uStackMemblkId;
		if (memBlockId != -1) {
			for (tries = 0; tries < FREE_RETRY_LIMIT; tries++) {
				ret = ksceKernelFreeMemBlock(memBlockId);
				if (ret >= 0)
					break;
				if (ksceKernelCheckDipsw(DIPSW_FREE_RETRY_LOG) &&
				    tries % FREE_RETRY_LOG_EVERY == 0)
					SCE_PRINTF_WITH_INFO(0, SCE_LINE(11786, 11791),
						"*** error: uidThread_Destructor() -> sceKernelFreeMemBlock(0x%08X[%s])=0x%08X, count=%d\n",
						thread->uStackMemblkId,
						ksceGUIDName(thread->uStackMemblkId), ret, tries);
				memBlockId = thread->uStackMemblkId;
			}
		}
		if (thread->userTlsMemBlockId != -1)
			ksceKernelFreeMemBlock(thread->userTlsMemBlockId);
		if (thread->exceptionEntry != NULL) {
			SCE_DBG_ASSERT_CHECK((thread->currentAttr >> 26) & 1, 11806);
			ret = SceSysrootForKernel_7968F091(thread->processId, thread->exceptionEntry);
			if (ret < 0) {
				SCE_DBG_PRINTF(0, 11810,
					"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pExceptionList\n",
					thread->processId, thread->exceptionEntry, ret, thread,
					thread->magic);
				SCE_DBG_ASSERT(0, 11811);
				return 0;
			}
		}
	}
	if (thread->pRegisterContext != NULL) {
		ret = SceSysrootForKernel_7968F091(thread->processId, thread->pRegisterContext);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11823,
				"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pRegisterContext\n",
				thread->processId, thread->pRegisterContext, ret, thread, thread->magic);
			SCE_DBG_ASSERT(0, 11824);
			return 0;
		}
	}
	if (thread->pVfpRegisterContext != 0) {
		ret = SceSysrootForKernel_7968F091(thread->processId, (void *)thread->pVfpRegisterContext);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 11835,
				"*** error: uidThread_Destructor() -> sceKernelSysrootFreeRemoteProcessHeap(0x%08X, %p)=0x%08X, pThreadCB=%p, pThreadCB->magic=0x%08X, pVfpRegisterContext\n",
				thread->processId, (void *)thread->pVfpRegisterContext, ret, thread,
				thread->magic);
			SCE_DBG_ASSERT(0, 11836);
			return 0;
		}
	}
	/* whoever still waits for this thread hears it is gone */
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&object->waitLock);
	wake = wakeupWaitObjectAll((uintptr_t)&object->waitNext, SCE_KERNEL_ERROR_WAIT_DELETE,
				   NULL);
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * PLACEHOLDER: name. The thread class's cancel method: whoever waits for
 * the thread is woken with SCE_KERNEL_ERROR_WAIT_CANCEL; returns how many.
 */
int uidThread_CancelWait(void *cls, void *uidObject, uint32_t a2)
{
	struct ThreadmgrThreadObject *object = uidObject;
	uint32_t count;
	int intr, wake;

	(void)cls;
	intr = ksceKernelCpuSuspendIntr();
	ksceKernelSpinlockLowLock(&object->waitLock);
	wake = wakeupWaitObjectAll((uintptr_t)&object->waitNext, SCE_KERNEL_ERROR_WAIT_CANCEL,
				   &count);
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	ksceKernelCpuResumeIntr(intr);
	return count;
}
