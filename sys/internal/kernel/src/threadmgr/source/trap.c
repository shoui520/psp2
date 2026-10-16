/*
 * SceKernelThreadMgr (threadmgr.c): the trap on the way out of a syscall.
 * The exception handlers call sceKernelTrapThreadAfterSyscall when a thread
 * leaves the kernel with something pending: a suspend or exit request, a
 * syscall the process misused, or a debugger suspension. A suspended
 * thread stops here and goes on running from here when it is resumed.
 */

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "uid_waitable.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define WAIT_TYPE_THREAD_SUSPEND	0x00000008	/* PLACEHOLDER: woken when the thread is suspended */
#define SCE_KERNEL_ERROR_THREAD_STOPPED	0x8002802c

/* what the exception handler asks for, besides the request bits above */
#define TRAP_REQUEST_CRITICAL_ERROR	0x01000000	/* the syscall's misuse is in the frame */
#define TRAP_REQUEST_DEBUG_SUSPEND	0x02000000	/* only the debugger's suspension */

#define CRITICAL_ERROR_MESSAGE		0x80000000	/* criticalError: | the message */
#define SDK_VERSION_CRITICAL_ERROR	0x01800000	/* reported for SDKs since 1.80 */
#define DIPSW_CRITICAL_ERROR		0xce

/* what the debugger's handler decides */
#define CRITICAL_ERROR_ACTION_CONTINUE	0
#define CRITICAL_ERROR_ACTION_SUSPEND	2


/* what the debugger's handlers hear of a critical error */
struct ThreadmgrCriticalErrorMessage {
	SceUID processId;
	SceUID threadId;
	uint32_t r[4];				/* 0x08: r0-r3 */
	uint32_t r12;				/* 0x18 */
	uint32_t sp;				/* 0x1c */
	uint32_t lr;
	uint32_t pc;
};

_Static_assert(sizeof(struct ThreadmgrCriticalErrorMessage) == 0x28, "critical error message");

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceKernelCheckDipsw(unsigned int bit);
extern int SceDebugForKernel_BCC8D0B2(SceUID processId, ThreadSyscallFrame *frame,
				      uint32_t *result);
extern int sceKernelSysrootBacktrace(SceUID processId, char *buffer, SceSize size,
					SceSize *length, int kind);
extern int sceKernelSysrootPrintBacktrace(SceUID processId, const char *buffer, SceSize length,
					int flags);
void __attribute__((noreturn)) _sceKernelExitCallback(int result);
void __attribute__((noreturn)) exitDeleteThread(int status, int intr, int error);
void _debugSuspendThreadSelf(uint32_t reason);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);

/* the thread object a context is part of */
static inline struct ThreadmgrThreadObject *threadObject(struct ThreadCB *thread)
{
	return (struct ThreadmgrThreadObject *)((uintptr_t)thread -
		offsetof(struct ThreadmgrThreadObject, context));
}

/* takes the running thread off its CPU (its lock held) until it is resumed */
static inline void stopSelf(struct ThreadCB *thread, uint32_t reason)
{
	currentCpuState()->nextRunningThread = 0;
	ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_SELF_STOPPED);
	switchThread(reason);
	ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_SELF_STOPPED);
}

/* an exit request: out of a callback first, then the thread ends */
static inline void __attribute__((noreturn, always_inline))
exitOnRequest(struct ThreadCB *thread, int delete)
{
	if (thread->callbackNest > 0 || thread->threadEventHandler != NULL)
		_sceKernelExitCallback(0);
	exitDeleteThread(delete, 0, SCE_KERNEL_ERROR_THREAD_STOPPED);
}

/* the thread goes on the manager's suspended list */
static inline void listSuspended(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread, *head;

	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	head = (struct ThreadmgrWaitNode *)&s_threadmgrRuntime.manager->suspendedNext;
	self->next = head;
	self->prev = head->prev;
	head->prev = self;
	self->prev->next = self;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/*
 * Returns 1 when the syscall returns as it is, 0 when a request broke its
 * wait and it starts again.
 */
int sceKernelTrapThreadAfterSyscall(ThreadSyscallFrame *frame, uint32_t *result,
				    uint32_t request)
{
	struct ThreadCB *thread = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadmgrWaitNode *head, *node, *next;
	struct ThreadmgrCriticalErrorMessage message;
	ThreadmgrDebugHandler handler;
	SceUInt32 sdkVersion;
	SceSize length;
	uint32_t status;
	char buffer[0x200];
	int action, ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), SCE_LINE(9571, 9576));
	SCE_DBG_ASSERT_CHECK(thread != NULL, SCE_LINE(9573, 9578));

	switch (request) {
	case THREAD_FLAG_REQUEST_VM_SUSPEND:
	case THREAD_FLAG_REQUEST_SUSPEND:
	case THREAD_FLAG_REQUEST_VM_SUSPEND | THREAD_FLAG_REQUEST_SUSPEND:
		break;
	case TRAP_REQUEST_CRITICAL_ERROR:
		if (frame == NULL || result == NULL) {
			ret = 1;
			goto debugSuspend;
		}
		if (SceSysrootForDriver_CAE820B2(0, &sdkVersion) >= 0 &&
		    sdkVersion >= SDK_VERSION_CRITICAL_ERROR &&
		    ksceKernelCheckDipsw(DIPSW_CRITICAL_ERROR)) {
			if ((int)frame->apiDeclare < 0)
				SCE_PRINTF_WITH_INFO(0, SCE_LINE(9717, 9722), "\n*** %s\n",
					(const char *)(frame->apiDeclare & ~CRITICAL_ERROR_MESSAGE));
			else
				SCE_PRINTF_WITH_INFO(0, SCE_LINE(9719, 9724), "\n*** Syscall Critical Usage Error\n");
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(9721, 9726), "ProcessID: 0x%08X [%s]\n", thread->processId,
					     ksceGUIDName(thread->processId));
			SCE_PRINTF_WITH_INFO(0, SCE_LINE(9722, 9727), "ThreadID:  0x%08X [%s]\n", thread->threadId,
					     ksceGUIDName(thread->threadId));
			SceDebugForKernel_BCC8D0B2(thread->processId, frame, result);
			memset(buffer, 0, sizeof(buffer));
			if (sceKernelSysrootBacktrace(0, buffer, sizeof(buffer), &length, 2) >= 0)
				sceKernelSysrootPrintBacktrace(0, buffer, length, 0);
			message.processId = thread->processId;
			message.threadId = thread->threadId;
			message.r[0] = frame->r[0];
			message.r12 = frame->r[12];
			message.r[1] = frame->r[1];
			message.r[2] = frame->r[2];
			message.r[3] = frame->r[3];
			message.sp = frame->sp;
			message.lr = frame->lr;
			message.pc = frame->pc;
			handler = s_threadmgrRuntime.manager->
				debugHandlers[0][DEBUG_HANDLER_CRITICAL_ERROR];
			if (handler != NULL)
				handler(sizeof(message), &message);
			handler = s_threadmgrRuntime.manager->
				debugHandlers[1][DEBUG_HANDLER_CRITICAL_ERROR];
			action = CRITICAL_ERROR_ACTION_CONTINUE;
			if (handler != NULL)
				action = handler(sizeof(message), &message);
			if (action == CRITICAL_ERROR_ACTION_SUSPEND) {
				ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
				_debugSuspendThreadSelf(DEBUG_SUSPEND_CRITICAL_ERROR);
				stopSelf(thread, 4);
				status = thread->flags;
				if (status >> 30)
					goto exiting;
			} else {
				SCE_DBG_ASSERT_LEVEL(0, action == CRITICAL_ERROR_ACTION_CONTINUE, SCE_LINE(9786, 9791));
			}
		}
		if (frame->apiDeclare > 1) {
			if ((int)frame->apiDeclare < 0) {
				ret = 1;
				goto debugSuspend;
			}
			SCE_PANIC(SCE_LINE(9803, 9808));
		}
		*result &= ~SYSCALL_ERROR_BIT;
		ret = 1;
		goto debugSuspend;
	case TRAP_REQUEST_DEBUG_SUSPEND:
		ret = 1;
		goto debugSuspend;
	case THREAD_FLAG_REQUEST_EXIT_DELETE:
		exitOnRequest(thread, 1);
	case THREAD_FLAG_REQUEST_EXIT:
		exitOnRequest(thread, 0);
	default:
		SCE_DBG_PRINTF(0, SCE_LINE(9812, 9817), "error: unexpected exitInst(%p, 0x%08X)\n", thread, request);
		SCE_PANIC(SCE_LINE(9813, 9818));
	}

	/* the threads waiting for it to stop */
	object = threadObject(thread);
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	ksceKernelSpinlockLowLock(&object->waitLock);
	for (node = head->next; node != head; node = next) {
		next = node->next;
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, SCE_LINE(9611, 9616));
		ksceKernelSpinlockLowLock(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wakeupAndDistributeThread((struct ThreadCB *)node,
						  (uintptr_t)head, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&object->waitLock);

	if (request == THREAD_FLAG_REQUEST_VM_SUSPEND) {
		if (!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND)) {
			/* resumed before it stopped */
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_REQUEST_MASK);
			if (thread->flags & THREAD_FLAG_SIGNAL_DEFERRED) {
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_SIGNAL_DEFERRED);
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
							   THREAD_FLAG_RESUMED_WAIT);
				ret = 0;
			} else {
				ret = 1;
			}
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			goto debugSuspend;
		}
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_MASK);
	} else {
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_MASK);
		if (request & THREAD_FLAG_REQUEST_SUSPEND) {
			thread->currentCpuAffinityMask = thread->savedCpuAffinityMask;
			thread->savedCpuAffinityMask = 0;
			thread->currentPriority = thread->basePriority < thread->boostPriority ?
				thread->basePriority : thread->boostPriority;
		}
	}

	/* suspended: off the CPUs until it is resumed */
	SCE_DBG_ASSERT_CHECK(thread->currentCpuId == (int)ksceKernelCpuId(), SCE_LINE(9656, 9661));
	thread->currentCpuId = -1;
	thread->waitId = -1;
	thread->status = (thread->status >> 16) << 16 | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
	listSuspended(thread);
	status = thread->flags;
	ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_SIGNAL_DEFERRED);
	stopSelf(thread, 2);
	if (thread->flags & THREAD_FLAG_EXITING) {
		status = thread->flags;
		goto exiting;
	}
	if (status & THREAD_FLAG_SIGNAL_DEFERRED) {
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_RESUMED_WAIT);
		ret = 0;
	} else {
		ret = 1;
	}

debugSuspend:
	if (thread->debugSuspendReason != 0) {
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		_debugSuspendThreadSelf(thread->debugSuspendReason);
		stopSelf(thread, 4);
		SCE_DBG_ASSERT_PRINTF(!(thread->flags & (THREAD_FLAG_REQUEST_VM_SUSPEND |
							 THREAD_FLAG_REQUEST_SUSPEND)),
				      9840, "threadId=0x%08X, flags=0x%08X, debugSuspendReason=0x%08X\n",
				      thread->threadId, thread->flags, thread->debugSuspendReason);
		status = thread->flags;
		if (status >> 30)
			goto exiting;
	}
	SCE_DBG_ASSERT_PRINTF(thread->debugSuspendReason == 0 &&
			      !(thread->flags & THREAD_FLAG_REQUEST_MASK),
			      9857, "threadId=0x%08X, flags=0x%08X, debugSuspendReason=0x%08X\n",
			      thread->threadId, thread->flags, thread->debugSuspendReason);
	return ret;

exiting:
	exitOnRequest(thread, (status & THREAD_FLAG_REQUEST_EXIT_DELETE) != 0);
}
