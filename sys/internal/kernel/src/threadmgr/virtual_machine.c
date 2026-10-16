/*
 * SceKernelThreadMgr: support for virtual machines (the PSP and other
 * emulators: threads with attr 0x4000000, or any thread of the titles listed
 * below). The VM can suspend and resume its threads (stateFlags 0x100, on
 * the manager's VM list), read and write their registers while suspended,
 * and wait for the exceptions its threads raise (each process has one
 * SceUIDExceptionObject that a single thread may wait on).
 */

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "mutex.h"
#include "syscall_context.h"

#define THREAD_ATTR_USER		0x80000000
#define THREAD_ATTR_VM			0x04000000	/* PLACEHOLDER: a VM's thread */
#define THREAD_CPU_NONE			0x7fff
#define WAIT_TYPE_THREAD_SUSPEND	0x00000008	/* PLACEHOLDER: woken when the thread is suspended */
#define WAIT_TYPE_EXCEPTION		0x01000000
#define THREADMGR_ERROR_WAIT_CANCEL	0x800280a2	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_WAIT_DELETE	0x80028006
#define SCE_KERNEL_ERROR_ILLEGAL_ARG	0x80020008	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_MASK	0x8002000c	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_THREAD_DELETED	0x8002802a	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NOT_VM_SUSPENDED	0x8002802e	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_VM_NOT_SUSPENDED	0x80028033	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_EXCEPTION_BUSY	0x80028321	/* PLACEHOLDER: name */
#define EXCEPTION_MASK_VALID		0x00000301
#define CPSR_DEBUGGER_MASK		0xf90ffe20
#define CPSR_KEEP_MASK			0x06f001df
#define USER_TLS_VM_WORD0		0x7f8	/* below the user TLS: two words the VM context carries */
#define USER_TLS_VM_WORD1		0x7f4

#define THREAD_CLASS()		(s_threadmgrRuntime.manager->threadClass)
#define EXCEPTION_CLASS()	(s_threadmgrRuntime.manager->exceptionClass)

/* the VM's view of a thread's registers (0x58) */
typedef struct ThreadmgrVmCpuContext {
	SceSize size;
	uint32_t cpsr;
	union {
		uint32_t r[15];			/* 0x08: r0-r14 */
		struct {
			uint32_t gpr[13];
			uint32_t sp;		/* 0x3c */
			uint32_t lr;
		};
	};
	uint32_t pc;				/* 0x44 */
	uint32_t tpidrurw;			/* 0x48 */
	uint32_t reserved;
	uint32_t tlsWord[2];			/* 0x50: the user TLS -0x7f8 and -0x7f4 */
} ThreadmgrVmCpuContext;

/* and its VFP registers (0x108) */
typedef struct ThreadmgrVmVfpContext {
	SceSize size;
	uint32_t fpscr;
	uint8_t d[0x100];			/* 0x08: d0-d31 */
} ThreadmgrVmVfpContext;

_Static_assert(sizeof(struct ThreadmgrException) == 0x38 &&
	       sizeof(ThreadmgrVmCpuContext) == 0x58 && sizeof(ThreadmgrVmVfpContext) == 0x108,
	       "virtual machine layouts");

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceGUIDGetName(SceUID guid, const char **name);
extern int ksceKernelIsIntrAllowedInCurrentContext(void);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int snprintf(char *str, SceSize size, const char *format, ...);
int callCallbackAll(struct ThreadCB *thread);
int sceKernelCheckThreadUserStackSelf(void);
int waitWaitableObject(SceUID waitId, uintptr_t waitable, uint32_t waitType,
		       const uint64_t *wakeup, void *timeoutOutput, int *waitCount, int *locked);
void getWakeupTime(uint64_t *wakeup, uint32_t timeout);
bool wakeupAndDistributeThread(struct ThreadCB *thread, uintptr_t waitable,
			       int result);
uint32_t wakeupWaitObjectAll(uintptr_t waitable, int result, uint32_t *count);
void runningThreadPreemptByIntr(struct ThreadCB *context, uint32_t reason);
void issueSuspendThreadForVMIntrRequest(unsigned int cpuId, SceUID threadId, int *result,
					SceKernelSpinlock *done);

static inline void dataMemoryBarrier(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

/* the exception object's release method: the end of one wait */
int uidException_ReleaseMethod(void *cls, void *object, struct ThreadCB *thread)
{
	struct ThreadmgrException *exception = object;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1447);
	SCE_DBG_ASSERT_CHECK(exception->lock != 0, 1450);
	if (thread->waitResult != (int)THREADMGR_ERROR_WAIT_CANCEL) {
		SCE_DBG_ASSERT_CHECK(exception->owner == thread->threadId, 1454);
		if (--exception->waiters == 0) {
			exception->owner = -1;
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
						    THREAD_FLAG_EXCEPTION_WAITER);
		}
	}
	SCE_DBG_ASSERT_CHECK(exception->waiting != 0, 1463);
	exception->waiting--;
	return 0;
}

/* SceUIDExceptionClass's destructor: its waiter wakes with WAIT_DELETE */
int uidException_Destructor(struct ThreadmgrException *exception)
{
	uint32_t wake;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1410);
	SCE_DBG_ASSERT_CHECK((struct ThreadmgrWaitNode *)&exception->next == exception->next, 1413);
	SCE_DBG_ASSERT_CHECK(exception->pendingNext ==
			     (struct ThreadmgrExceptionEntry *)&exception->pendingNext, 1415);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&exception->lock);
	wake = wakeupWaitObjectAll((uintptr_t)&exception->next, SCE_KERNEL_ERROR_WAIT_DELETE, NULL);
	ksceKernelSpinlockLowUnlock(&exception->lock);
	SCE_DBG_ASSERT_CHECK(!(wake & 1), 1423);
	if ((wake & 1) && currentThreadContext() != NULL) {
		switchThread((intr >> 7) & 1);
		ksceKernelCpuResumeIntr(intr);
		return 0;
	}
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/* SceUIDExceptionClass's constructor */
static int uidException_Constructor(void *object)
{
	struct ThreadmgrException *exception = object;

	memset(&exception->pendingNext, 0,
	       sizeof(*exception) - offsetof(struct ThreadmgrException, pendingNext));
	return 0;
}

/* whether the thread's process is one of the titles that run as VMs */
int isVirtualMachineTitle(struct ThreadCB *thread)
{
	const char *titleId = thread->process->TitleId;

	return strcmp(titleId, "PCSC90014") == 0 || strcmp(titleId, "PCSA00066") == 0 ||
	       strcmp(titleId, "PCSF00143") == 0 || strcmp(titleId, "PCSD00036") == 0 ||
	       strcmp(titleId, "PCSC00012") == 0 || strcmp(titleId, "PCSA00004") == 0 ||
	       strcmp(titleId, "PCSF00023") == 0 || strcmp(titleId, "PCSD00016") == 0 ||
	       strcmp(titleId, "PCSA22147") == 0 || strcmp(titleId, "PCSD00042") == 0 ||
	       strcmp(titleId, "PCSA00026") == 0;
}

/* the thread goes on the manager's VM-suspended list */
static inline void listVmSuspended(struct ThreadCB *thread)
{
	struct SceKernelThreadmgrPrivate *manager;
	uintptr_t self = (uintptr_t)thread;

	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	manager = s_threadmgrRuntime.manager;
	thread->next = (uintptr_t)&manager->suspendedNext;
	thread->prev = manager->suspendedPrev;
	manager->suspendedPrev = self;
	*(uintptr_t *)thread->prev = self;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
}

/* the thread leaves the list it is on under the manager's lock */
static inline void unlistThread(struct ThreadCB *thread)
{
	struct ThreadmgrWaitNode *self = (struct ThreadmgrWaitNode *)thread;
	struct SceKernelThreadmgrPrivate *manager;

	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	manager = s_threadmgrRuntime.manager;
	self->next->prev = self->prev;
	self->prev->next = self->next;
	ksceKernelSpinlockLowUnlock(&manager->threadMgrCBLock);
}

/*
 * Suspends a VM thread (nesting); returns the new count or an error. A
 * thread inside a syscall or waiting only gets the request flag and stops on
 * its way out. A thread running on another CPU is reached with SGI kind 0xa:
 * from a call (done NULL) this waits for the answer, from the SGI side
 * (sceKernelSuspendThreadForVMIntr) it forwards the request and returns 0.
 */
int suspendThreadForVM(SceUID threadId, int *result, SceKernelSpinlock *done)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *self, *node, *next, *head;
	SceKernelSpinlock wait;
	uint32_t state, request, oldState, wake;
	int intr, cpuId, count, ret;

	intr = ksceKernelCpuSuspendIntr();
	cpuId = ksceKernelCpuId();
	cpu = currentCpuState();
	ret = ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (!(thread->currentAttr & THREAD_ATTR_VM) && !isVirtualMachineTitle(thread)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARG;
		goto unlock;
	}
	count = thread->vmSuspendCount;
	request = thread->flags;
	if (count > 0 || (request & THREAD_FLAG_REQUEST_MASK)) {
		ret = thread->vmSuspendCount = count + 1;
		goto unlock;
	}
	state = thread->status;
	if ((state & THREAD_STATE_QUEUED) == 0) {
		if (state & SCE_KERNEL_THREAD_STATUS_STAGNANT) {
			ret = thread->vmSuspendCount = count + 1;
			if (thread->userFrame != NULL && !(request & THREAD_FLAG_SELF_STOPPED))
				goto later;
			wake = 0;
			unlistThread(thread);
			oldState = (uint16_t)(thread->status >> 16);
			goto suspended;
		}
		if (!(state & THREAD_STATE_OFF_CPU)) {
			if (state & SCE_KERNEL_THREAD_STATUS_WAITING) {
				ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
							   THREAD_FLAG_REQUEST_VM_SUSPEND);
				ret = ++thread->vmSuspendCount;
			} else if (state & SCE_KERNEL_THREAD_STATUS_DORMANT) {
				ret = SCE_KERNEL_ERROR_DORMANT;
			} else if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
				ret = SCE_KERNEL_ERROR_THREAD_DELETED;
			} else {
				if (!(state & SCE_KERNEL_THREAD_STATUS_SUSPENDED)) {
					SCE_PRINTF_WITH_INFO(0, 639, "error: unexpected thread status\n");
					SCE_PANIC(640);
				}
				ret = thread->vmSuspendCount = count + 1;
			}
			goto unlock;
		}
		/* debug-suspended off the CPUs: from the debugger's list to the VM's */
		ret = thread->vmSuspendCount = count + 1;
		if (thread->userFrame != NULL && !(request & THREAD_FLAG_SELF_STOPPED))
			goto later;
		wake = 0;
		unlistThread(thread);
		thread->status = (thread->status & THREAD_STATE_DEBUG_SUSPENDED) |
				     SCE_KERNEL_THREAD_STATUS_SUSPENDED;
		listVmSuspended(thread);
		goto wakeWaiters;
	}
	if (thread->currentCpuId != cpuId && thread->currentCpuId != THREAD_CPU_NONE) {
		if (done == NULL) {
			wait = 1;
			dataMemoryBarrier();
			issueSuspendThreadForVMIntrRequest(thread->currentCpuId, threadId, &ret, &wait);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ksceKernelCpuResumeIntr(0);
			ksceKernelSpinlockLowLockCpuSuspendIntr(&wait);
		} else {
			issueSuspendThreadForVMIntrRequest(thread->currentCpuId, threadId, result, done);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			ret = 0;
		}
		goto release;
	}
	ret = thread->vmSuspendCount = count + 1;
	if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
		SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 499);
		SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_SELF_STOPPED), 500);
		if (thread->userFrame != NULL)
			goto later;
		runningThreadPreemptByIntr(thread, 2);
		oldState = (uint16_t)(thread->status >> 16);
		wake = 1;
		thread->currentCpuId = -1;
	} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
		if (thread->userFrame != NULL && !(request & THREAD_FLAG_SELF_STOPPED))
			goto later;
		removeReady(thread, thread->currentPriority);
		oldState = (uint16_t)(thread->status >> 16);
		wake = 0;
		thread->currentCpuId = -1;
	} else {
		if (thread->userFrame != NULL && !(request & THREAD_FLAG_SELF_STOPPED))
			goto later;
		wake = 0;
		self = (struct ThreadmgrWaitNode *)thread;
		ksceKernelSpinlockLowLock(&cpu->standbyLock);
		self->next->prev = self->prev;
		self->prev->next = self->next;
		ksceKernelSpinlockLowUnlock(&cpu->standbyLock);
		oldState = (uint16_t)(thread->status >> 16);
		thread->currentCpuId = -1;
	}
suspended:
	thread->status = (oldState << 16) | SCE_KERNEL_THREAD_STATUS_SUSPENDED;
	listVmSuspended(thread);
wakeWaiters:
	/* the threads waiting for it to stop */
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowLock(&object->waitLock);
	head = (struct ThreadmgrWaitNode *)&object->waitNext;
	for (node = head->next; node != head; node = next) {
		next = node->next;
		SCE_DBG_ASSERT_CHECK(((struct ThreadCB *)node)->magic ==
				     THREAD_CB_MAGIC, 658);
		ksceKernelSpinlockLowLock(THREAD_LOCK((struct ThreadCB *)node));
		if (((struct ThreadCB *)node)->waitType == WAIT_TYPE_THREAD_SUSPEND)
			wake |= wakeupAndDistributeThread((struct ThreadCB *)node,
							  (uintptr_t)head, 0);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK((struct ThreadCB *)node));
	}
	ksceKernelSpinlockLowUnlock(&object->waitLock);
	ksceGUIDReleaseObject(threadId);
	if ((wake & 1) && currentThreadContext() != NULL)
		switchThread((intr >> 7) & 1);
	goto resume;
later:
	ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_VM_SUSPEND);
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
release:
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* undoes one sceKernelSuspendThreadForVM; at zero the thread runs again */
int resumeThreadForVM(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrException *exception;
	struct ThreadmgrExceptionEntry *entry;
	uint32_t state;
	int intr, ret, wake;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 719, "*** error: illegal context. threadId=0x%08x[%s]\n",
				      threadId, ksceGUIDName(threadId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (threadId == 0 || currentThreadContext()->threadId == threadId)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	if (!(thread->currentAttr & THREAD_ATTR_VM) && !isVirtualMachineTitle(thread)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARG;
		goto release;
	}
	exception = thread->process->exception;
	ksceKernelSpinlockLowLock(&exception->lock);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	state = thread->status;
	if ((state & 0xffff) == SCE_KERNEL_THREAD_STATUS_DORMANT) {
		ret = SCE_KERNEL_ERROR_DORMANT;
	} else if (thread->vmSuspendCount == 0) {
		ret = SCE_KERNEL_ERROR_VM_NOT_SUSPENDED;
	} else {
		ret = --thread->vmSuspendCount;
		if (ret < 1) {
			if (!(thread->flags & THREAD_FLAG_REQUEST_VM_SUSPEND)) {
				SCE_DBG_ASSERT_CHECK(state & SCE_KERNEL_THREAD_STATUS_SUSPENDED, 783);
				if (thread->flags & THREAD_FLAG_EXCEPTION) {
					/* its exception was not taken: it goes */
					entry = thread->exceptionEntry;
					entry->next->prev = entry->prev;
					entry->prev->next = entry->next;
					ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
								    THREAD_FLAG_EXCEPTION);
				}
				ksceKernelSpinlockLowUnlock(
					&thread->process->exception->lock);
				if (thread->flags & THREAD_FLAG_SUSPEND_COUNTED) {
					ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
					goto release;
				}
				unlistThread(thread);
				wake = distributeThread(selectExecCpu(thread), thread);
				ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
				ksceGUIDReleaseObject(threadId);
				if ((wake & 1) && currentThreadContext() != NULL)
					switchThread((intr >> 7) & 1);
				goto resume;
			}
			SCE_DBG_ASSERT_CHECK(!(state & SCE_KERNEL_THREAD_STATUS_SUSPENDED), 790);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_REQUEST_VM_SUSPEND);
		}
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceKernelSpinlockLowUnlock(&thread->process->exception->lock);
release:
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/*
 * Waits for an exception of a kind in mask raised by a thread of the
 * caller's process; the first waiter owns the process's exception object.
 */
int waitException(uint32_t mask, ThreadmgrExceptionInfo *info, SceUInt32 *timeout, int callbacks)
{
	struct ThreadCB *thread =
		currentThreadContext();
	struct ThreadmgrExceptionEntry *entry;
	struct ThreadmgrException *exception;
	uint64_t wakeup;
	uint32_t saved;
	int intr, ret, cbRet, locked = 0;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (ksceKernelIsIntrAllowedInCurrentContext()) {
		SCE_DBG_ASSERT_PRINTF(0, 161, "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (mask & ~EXCEPTION_MASK_VALID) {
		SCE_DBG_ASSERT_FAIL(168);
		ret = SCE_KERNEL_ERROR_ILLEGAL_MASK;
		goto restore;
	}
	if (info != NULL && info->size > sizeof(*info)) {
		SCE_DBG_ASSERT_FAIL(175);
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	if (intr & 0x80) {
		SCE_DBG_ASSERT_PRINTF(0, 183, "*** error: irq suspended. processId=0x%08x[%s]\n",
				      thread->processId, ksceGUIDName(thread->processId));
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if (currentCpuState()->disabledDispatch != 0) {
		ret = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	if ((thread->currentAttr & THREAD_ATTR_USER) && !sceKernelCheckThreadUserStackSelf())
		debugSuspendThreadSelf(DEBUG_SUSPEND_USER_STACK, 0, NULL);
	ret = ksceGUIDReferObjectWithClass(
		thread->process->exceptionId, EXCEPTION_CLASS(),
		(SceObjectBase **)&exception);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(203);
		goto resume;
	}
	ksceKernelSpinlockLowLock(&exception->lock);
	if (exception->owner == -1) {
		exception->owner = thread->threadId;
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_EXCEPTION_WAITER);
	} else if (exception->owner != thread->threadId) {
		ret = SCE_KERNEL_ERROR_EXCEPTION_BUSY;
		goto unlock;
	}
	exception->waiters++;
	if (timeout != NULL)
		getWakeupTime(&wakeup, *timeout);
	for (;;) {
		if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
			ksceKernelSpinlockLowUnlock(&exception->lock);
			ret = callCallbackAll(thread);
			if (ret < 0)
				goto release;
			ksceKernelSpinlockLowLock(&exception->lock);
		}
		for (entry = exception->pendingNext;
		     entry != (struct ThreadmgrExceptionEntry *)&exception->pendingNext;
		     entry = entry->next) {
			if (!(mask & entry->kind))
				continue;
			/* taken: the thread no longer has one pending */
			ksceKernelSpinlockLowLock(THREAD_LOCK(entry->thread));
			SCE_DBG_ASSERT_CHECK((entry->thread->flags >> 9) & 1, 258);
			entry->next->prev = entry->prev;
			entry->prev->next = entry->next;
			ksceKernelAtomicClearMask32((SceInt32 *)&entry->thread->flags,
						    THREAD_FLAG_EXCEPTION);
			if (info != NULL) {
				info->kind = entry->kind;
				info->value = entry->value;
				if (entry->kind == 1)
					info->value2 = entry->value2;
			}
			ret = 0;
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(entry->thread));
			if (--exception->waiters == 0) {
				exception->owner = -1;
				ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags,
							    THREAD_FLAG_EXCEPTION_WAITER);
				ret = 0;
			}
			goto unlock;
		}
		ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
		thread->waitArg0 = mask;
		thread->waitArg1 = (uintptr_t)info;
		if (!callbacks) {
			ret = waitWaitableObject(thread->process->exceptionId,
						 (uintptr_t)&exception->next, WAIT_TYPE_EXCEPTION,
						 &wakeup, timeout, NULL, &locked);
			ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
			ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			break;
		}
		ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ret = waitWaitableObject(thread->process->exceptionId,
					 (uintptr_t)&exception->next, WAIT_TYPE_EXCEPTION, &wakeup,
					 timeout, NULL, &locked);
		ksceKernelAtomicClearMask32((SceInt32 *)&thread->flags, THREAD_FLAG_WAKE_PENDING);
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		if (!locked || ret != (int)THREADMGR_ERROR_WAIT_CANCEL)
			break;
	}
	SCE_DBG_ASSERT_CHECK(locked != 0, 308);
	ksceKernelSpinlockLowUnlock(&exception->lock);
	ksceGUIDReleaseObject(thread->process->exceptionId);
	if (callbacks && (thread->flags & THREAD_FLAG_CALLBACKS)) {
		cbRet = callCallbackAll(thread);
		if (cbRet < 0)
			ret = cbRet;
	}
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(&exception->lock);
release:
	ksceGUIDReleaseObject(thread->process->exceptionId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceUIDExceptionClass */
void startException(void)
{
	struct ThreadmgrWaitableClass *cls;

	cls = ksceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(352);
	if (uidExtendedClassInit(cls, "SceUIDExceptionClass",
			s_threadmgrRuntime.manager->waitableClass, 0x80,
			uidException_Constructor, (SceClassCallback)uidException_Destructor,
			uidDefaultCancelWait, uidDefault_RevalidateMethod,
			uidException_ReleaseMethod) < 0)
		SCE_PANIC_FUNC(1385, "uidExceptionClassInit");
	s_threadmgrRuntime.manager->exceptionClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 368, " sizeof(SceUIDExceptionObject)=%d\n",
			     sizeof(struct ThreadmgrException));
}

/* a new process's exception object ("excp: <process name>") */
int createException(SceProcessObject *process)
{
	UIDCreateOpt opt;
	struct ThreadmgrException *exception;
	const char *processName;
	char name[32];
	int ret;

	ret = ksceGUIDGetName(process->pid, &processName);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 386, "*** error: sceKernelThreadMgrProcessCreate() -> "
			       "sceGUIDGetName(0x%08X)=0x%08X\n", process->pid, ret);
	snprintf(name, sizeof(name), "excp: %s", processName);
	opt = (UIDCreateOpt){ 0 };
	process->exceptionId = ksceGUIDKernelCreateWithOpt(EXCEPTION_CLASS(), name,
		(SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)&process->exception);
	if (process->exceptionId < 0) {
		SCE_DBG_ASSERT_FAIL(393);
		return ret;
	}
	exception = process->exception;
	exception->owner = -1;
	exception->waiters = 0;
	exception->pendingNext = (struct ThreadmgrExceptionEntry *)&exception->pendingNext;
	exception->pendingPrev = (struct ThreadmgrExceptionEntry *)&exception->pendingNext;
	ksceKernelSpinlockLowUnlock(&exception->lock);
	return ret;
}

int deleteException(SceProcessObject *process)
{
	int intr, ret;

	if (process->exceptionId == -1)
		return 0;
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDClose(process->exceptionId);
	ksceKernelCpuResumeIntr(intr);
	SCE_DBG_ASSERT_CHECK(ret >= 0, 419);
	process->exception = NULL;
	return ret;
}

/* SGI kind 0xa: suspendThreadForVM for a thread on this CPU */
void suspendThreadForVMIntr(SceUID threadId, int *result, SceKernelSpinlock *done)
{
	int ret;

	SCE_DBG_ASSERT_CHECK(result != NULL, 686);
	SCE_DBG_ASSERT_CHECK(done != NULL, 687);
	ret = suspendThreadForVM(threadId, result, done);
	if (ret != 0) {
		*result = ret;
		ksceKernelSpinlockLowUnlock(done);
	}
}

int sceKernelSuspendThreadForVM(SceUID threadId)
{
	uint32_t saved;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 860, "*** error: illegal context. threadId=0x%08x[%s]\n",
				      threadId, ksceGUIDName(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else if (threadId == 0 || currentThreadContext()->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	} else {
		ret = suspendThreadForVM(threadId, NULL, NULL);
		SCE_DBG_ASSERT_CHECK(ret != 0, 880);
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* writes a VM-suspended thread's registers (as for the debugger's mode 1) */
int sceKernelSetThreadContextForVM(SceUID threadId, const ThreadmgrVmCpuContext *cpu,
				   const ThreadmgrVmVfpContext *vfp)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	uintptr_t vfpContext;
	int intr, ret, i;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 950, "*** error: illegal context. threadId=0x%08x[%s]\n",
				      threadId, ksceGUIDName(threadId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (threadId == 0 || currentThreadContext()->threadId == threadId)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if (cpu == NULL && vfp == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if ((cpu != NULL && cpu->size != sizeof(*cpu)) || (vfp != NULL && vfp->size != sizeof(*vfp)))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (!(thread->currentAttr & THREAD_ATTR_VM)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARG;
		goto unlock;
	}
	ret = SCE_KERNEL_ERROR_NOT_VM_SUSPENDED;
	if (thread->vmSuspendCount == 0)
		goto unlock;
	if (cpu != NULL) {
		frame = thread->userFrame;
		if (thread->flags & THREAD_FLAG_IN_CALLBACK) {
			regs = thread->pRegisterContext;
			regs->callbackContext->cpsr = (cpu->cpsr & CPSR_DEBUGGER_MASK) |
						      (regs->callbackContext->cpsr & CPSR_KEEP_MASK);
			for (i = 0; i < 15; i++)
				regs->callbackContext->r[i] = cpu->r[i];
			regs->callbackContext->pc = cpu->pc;
			regs->callbackContext->tpidrurw = cpu->tpidrurw;
		} else if (frame == NULL) {
			regs = thread->pRegisterContext;
			regs->cpsr = (regs->cpsr & CPSR_KEEP_MASK) | (cpu->cpsr & CPSR_DEBUGGER_MASK);
			for (i = 0; i < 15; i++)
				regs->r[i] = cpu->r[i];
			regs->pc = cpu->pc;
			regs->tpidrurw = cpu->tpidrurw;
		} else {
			frame->cpsr = (frame->cpsr & CPSR_KEEP_MASK) | (cpu->cpsr & CPSR_DEBUGGER_MASK);
			for (i = 0; i < 13; i++)
				frame->r[i] = cpu->r[i];
			frame->sp = cpu->sp;
			frame->lr = cpu->lr;
			frame->pc = cpu->pc;
			frame->tpidrurw = cpu->tpidrurw;
			ksceKernelAtomicOrAndGet32((SceInt32 *)&thread->flags,
						   THREAD_FLAG_FRAME_CHANGED);
		}
		ret = ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VM_WORD0), &cpu->tlsWord[0],
					   sizeof(uint32_t));
		if (ret < 0)
			goto unlock;
		ret = ksceKernelCopyToUser((void *)(thread->tls - USER_TLS_VM_WORD1), &cpu->tlsWord[1],
					   sizeof(uint32_t));
		if (ret < 0)
			goto unlock;
	}
	ret = 0;
	if (vfp != NULL) {
		vfpContext = thread->pVfpRegisterContext;
		if (thread->userFrame == NULL)
			*(uint32_t *)(vfpContext + 0x100) = vfp->fpscr;
		else
			thread->userFrame->fpscr = vfp->fpscr;
		ret = 0;
		memcpy((void *)vfpContext, vfp->d, sizeof(vfp->d));
	}
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* reads a VM-suspended thread's registers */
int sceKernelGetThreadContextForVM(SceUID threadId, ThreadmgrVmCpuContext *cpu,
				   ThreadmgrVmVfpContext *vfp)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadRegisterContext *regs;
	ThreadSyscallFrame *frame;
	uintptr_t vfpContext;
	int intr, ret, i;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 1121, "*** error: illegal context. threadId=0x%08x[%s]\n",
				      threadId, ksceGUIDName(threadId));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (threadId == 0 || currentThreadContext()->threadId == threadId)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if ((cpu != NULL && cpu->size != sizeof(*cpu)) || (vfp != NULL && vfp->size != sizeof(*vfp)))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	intr = ksceKernelCpuSuspendIntr();
	if (ksceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (!(thread->currentAttr & THREAD_ATTR_VM) && !isVirtualMachineTitle(thread)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARG;
		goto unlock;
	}
	ret = SCE_KERNEL_ERROR_NOT_VM_SUSPENDED;
	if (thread->vmSuspendCount == 0)
		goto unlock;
	if (cpu != NULL) {
		frame = thread->userFrame;
		if (thread->flags & THREAD_FLAG_IN_CALLBACK) {
			regs = thread->pRegisterContext;
			cpu->cpsr = regs->callbackContext->cpsr;
			for (i = 0; i < 15; i++)
				cpu->r[i] = regs->callbackContext->r[i];
			cpu->pc = regs->callbackContext->pc;
			cpu->tpidrurw = regs->callbackContext->tpidrurw;
		} else if (frame == NULL) {
			regs = thread->pRegisterContext;
			cpu->cpsr = regs->cpsr;
			for (i = 0; i < 15; i++)
				cpu->r[i] = regs->r[i];
			cpu->pc = regs->pc;
			cpu->tpidrurw = regs->tpidrurw;
		} else {
			cpu->cpsr = frame->cpsr;
			for (i = 0; i < 13; i++)
				cpu->r[i] = frame->r[i];
			cpu->sp = frame->sp;
			cpu->lr = frame->lr;
			cpu->pc = frame->pc;
			cpu->tpidrurw = frame->tpidrurw;
		}
		ret = ksceKernelCopyFromUser(&cpu->tlsWord[0],
					     (const void *)(thread->tls - USER_TLS_VM_WORD0),
					     sizeof(uint32_t));
		if (ret < 0)
			goto unlock;
		ret = ksceKernelCopyFromUser(&cpu->tlsWord[1],
					     (const void *)(thread->tls - USER_TLS_VM_WORD1),
					     sizeof(uint32_t));
		if (ret < 0)
			goto unlock;
	}
	ret = 0;
	if (vfp != NULL) {
		vfpContext = thread->pVfpRegisterContext;
		ret = 0;
		vfp->fpscr = *(uint32_t *)(vfpContext + 0x100);
		memcpy(vfp->d, (const void *)vfpContext, sizeof(vfp->d));
	}
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* the user syscalls (and their cores) take the caller's PUIDs */
int sceKernelSuspendThreadForVMForUser(SceUID threadId)
{
	SceUID guid = kscePUIDtoGUID(0, threadId);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return sceKernelSuspendThreadForVM(guid);
}

int sceKernelResumeThreadForVM(SceUID threadId)
{
	SceUID guid = kscePUIDtoGUID(0, threadId);
	uint32_t saved;
	int ret;

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	ret = resumeThreadForVM(guid);
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* PLACEHOLDER: name; the user syscall's core (threadmgr_for_user.c copies the contexts) */
int setThreadContextForVMForUser(SceUID threadId, const ThreadmgrVmCpuContext *cpu,
				    const ThreadmgrVmVfpContext *vfp)
{
	SceUID guid = kscePUIDtoGUID(0, threadId);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return sceKernelSetThreadContextForVM(guid, cpu, vfp);
}

/* PLACEHOLDER: name */
int getThreadContextForVMForUser(SceUID threadId, ThreadmgrVmCpuContext *cpu,
				    ThreadmgrVmVfpContext *vfp)
{
	SceUID guid = kscePUIDtoGUID(0, threadId);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return sceKernelGetThreadContextForVM(guid, cpu, vfp);
}

/* PLACEHOLDER: name; the user syscalls' cores */
int waitExceptionForUser(uint32_t mask, ThreadmgrExceptionInfo *info, SceUInt32 *timeout)
{
	return waitException(mask, info, timeout, 0);
}

int waitExceptionCBForUser(uint32_t mask, ThreadmgrExceptionInfo *info, SceUInt32 *timeout)
{
	return waitException(mask, info, timeout, 1);
}

/* SceThreadmgrForKernel_A7E5C83D: the DTrace thread callback switch (manager +0x28ac) */
int SceThreadmgrForKernel_A7E5C83D(int *enable)
{
	*enable = s_threadmgrRuntime.manager->dtraceThreadCBEnable;
	return 0;
}
