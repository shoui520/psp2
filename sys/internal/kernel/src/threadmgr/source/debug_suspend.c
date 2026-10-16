/*
 * SceKernelThreadMgr (threadmgr.c): a thread suspending itself for the
 * debugger. debugSuspendThreadSelf marks a user thread that hit a problem
 * (a broken user stack, a stray sceKernelExitCallback, ...), tells the
 * debugger's handlers, and asks for its process's suspension through the
 * process's debug event flag; _debugSuspendThreadSelf takes the running
 * thread off its CPU on the way out of the kernel.
 */

#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "fast_mutex.h"
#include "scheduler.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREAD_ATTR_USER		0x80000000

/* why a thread suspends itself, and the debugger's handler slot for each */

/* what the debugger's handler decides */
#define DEBUG_SUSPEND_ACTION_RETURN	0	/* PLACEHOLDER: name */
#define DEBUG_SUSPEND_ACTION_PROCESS	1	/* the process is suspended */
#define DEBUG_SUSPEND_ACTION_IGNORE	2

#define USER_TLS_STACK_RANGE		0x7f8


extern int ksceKernelSetEventFlag(SceUID evfId, uint32_t pattern);
extern uint64_t SceSysrootForKernel_E20F6FC8(void);
void issueCallPAScheduleHookIntrRequest(unsigned int cpuId, SceUID processId, SceUID threadId, uint32_t event,
		  int priority);

/* the user stack left below the syscall's sp, 0 if it is out of the stack */
int sceKernelCheckThreadUserStackSelf(void)
{
	struct ThreadCB *thread =
		currentThreadContext();
	uint32_t range[2];			/* the stack's top and bottom, from the user TLS */
	uint32_t sp;

	if (thread == NULL) {
		SCE_DBG_ASSERT_FAIL(7654);
		return 0;
	}
	if (!(thread->currentAttr & THREAD_ATTR_USER)) {
		SCE_DBG_ASSERT_FAIL(7660);
		return 0;
	}
	SCE_DBG_ASSERT_CHECK(thread->userFrame != NULL, 7665);
	sp = thread->userFrame->sp;
	if (ksceKernelCopyFromUser(range, (const void *)(thread->tls - USER_TLS_STACK_RANGE),
				   sizeof(range)) >= 0 && sp <= range[0] && range[1] < sp)
		return sp - range[1];
	return 0;
}

/*
 * The current user thread stops for the debugger (reason, with arg and the
 * reason's info): the handlers of the debugger's slot for the reason are
 * told; unless one decides otherwise, the process is suspended through its
 * debug event flag (once).
 */
int debugSuspendThreadSelf(uint32_t reason, SceSize size, void *message)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process;
	ThreadmgrDebugHandler notify;
	ThreadmgrDebugHandler decide;
	uint32_t kind;
	int intr, ret, action;

	SCE_DBG_ASSERT_CHECK(currentThreadContext() != NULL, 11306);
	SCE_DBG_ASSERT_CHECK(thread->currentAttr >> 31, 11308);
	process = thread->process;
	intr = ksceKernelCpuSuspendIntr();
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (thread->flags >> 30 != 0) {
		/* it is exiting */
		ret = 0;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		goto resume;
	}
	thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
	thread->debugSuspendReason |= reason;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	switch (reason) {
	case DEBUG_SUSPEND_EXIT_CALLBACK:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_EXIT_CALLBACK];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_EXIT_CALLBACK];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 5;
			goto decided;
		}
		kind = 5;
		break;
	case DEBUG_SUSPEND_USER_STACK:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_STACK_OVERFLOW];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_STACK_OVERFLOW];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 4;
			goto decided;
		}
		kind = 4;
		break;
	case DEBUG_SUSPEND_UNK80000:
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_UNK80000];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_UNK80000];
		if (decide != NULL) {
			action = decide(size, message);
			kind = 7;
			goto decided;
		}
		kind = 7;
		break;
	case DEBUG_SUSPEND_UNK100000:
		kind = *(uint32_t *)message;
		notify = s_threadmgrRuntime.manager->debugHandlers[0][DEBUG_HANDLER_UNK100000];
		if (notify != NULL)
			notify(size, message);
		decide = s_threadmgrRuntime.manager->debugHandlers[1][DEBUG_HANDLER_UNK100000];
		if (decide != NULL) {
			action = decide(size, message);
			goto decided;
		}
		break;
	default:
		SCE_PANIC(SCE_LINE(11406, 11411));
	}
	goto suspend;
decided:
	if (action != DEBUG_SUSPEND_ACTION_PROCESS) {
		if (action == DEBUG_SUSPEND_ACTION_IGNORE)
			goto none;
		if (action != DEBUG_SUSPEND_ACTION_RETURN)
			SCE_PANIC(SCE_LINE(11431, 11436));
		SCE_DBG_ASSERT_CHECK(reason == DEBUG_SUSPEND_CRITICAL_ERROR, 11417);
		ret = 0;
		goto resume;
	}
suspend:
	if (ksceKernelAtomicCompareAndSet32((SceInt32 *)&process->abortRequested, 0, 1) != 0)
		goto none;
	process->abortCode = kind;
	process->abortThread = thread->threadId;
	ret = ksceKernelSetEventFlag(process->abortEventFlag, 1);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 11427,
			"*** error: debugSuspendThreadSelf() -> sceKernelSetEventFlag(0x%08X)=0x%08X\n",
			process->abortEventFlag, ret);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
none:
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/*
 * The running thread leaves its CPU debug-suspended for reason (interrupts
 * off, on the way out of the kernel): its fast mutexes' locks are dropped,
 * its run time is accounted and the CPU is left idle.
 */
void _debugSuspendThreadSelf(uint32_t reason)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadmgrFastMutex *mutex;
	ThreadmgrTraceHook hook;
	uint64_t ran;
	uint32_t mpidr;
	uintptr_t self = (uintptr_t)thread;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 9498);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() != NULL, 9500);
	SCE_DBG_ASSERT_CHECK(thread == currentThreadContext(), 9501);
	for (mutex = (struct ThreadmgrFastMutex *)thread->ownedFastMutexNext;
	     (uintptr_t)mutex != (uintptr_t)&thread->ownedFastMutexNext; mutex = mutex->next) {
		SCE_DBG_ASSERT_CHECK(mutex->magic == FAST_MUTEX_MAGIC, 9509);
		ksceKernelSpinlockLowUnlock(&mutex->lock);
	}
	ran = thread->runClock;
	thread->runClock = SceSysrootForKernel_E20F6FC8() + (ran - cpu->lastThreadSwitchTime);
	thread->threadReleaseCount++;
	hook = s_threadmgrRuntime.switchHook;
	if (hook != NULL)
		hook(2, 5, thread->threadId, thread->currentPriority, -1, -1, 8);
	if (s_threadmgrRuntime.manager->paHooks[0] != 0) {
		__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpidr));
		issueCallPAScheduleHookIntrRequest(mpidr & 3, thread->processId, thread->threadId, 2,
			     thread->currentPriority);
	}
	notifyOffCpu(8);
	manager = s_threadmgrRuntime.manager;
	cpu->priority = 0xfe;
	cpu->processId = -1;
	cpu->threadId = -1;
	cpu->runningThread = 0;
	thread->status = ((thread->status << 16) & 0x7fff0000) | THREAD_STATE_DEBUG_SUSPENDED |
			     THREAD_STATE_OFF_CPU;
	thread->debugSuspendReason |= reason;
	thread->currentCpuId = -1;
	ksceKernelSpinlockLowLock(&manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
	thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
	s_threadmgrRuntime.manager->debugSuspendedPrev = self;
	*(uintptr_t *)thread->prev = self;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}
