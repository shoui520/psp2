/*
 * SceKernelThreadMgr (threadmgr.c): starting a dormant thread. Its start
 * argument is copied to the top of its stack, its contexts are reset, and a
 * user thread also gets its user context, TLS words and VFP area on its
 * kernel stack; then it is queued to run threadEntry.
 */

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "uid_waitable.h"
#include "thread_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define DEBUG_THREAD_EVENT_START	2
#define TRACE_EVENT_THREAD_START	2
#define REGISTER_POISON			0xdeadbeef
#define CPACR_VFP_FULL			0x00f00000
#define CPSR_START_MASKED		0xff	/* SYS mode, Thumb, IRQ and FIQ masked */
#define CPSR_USR_ARM			0x50	/* USR mode, FIQ masked */
#define CPSR_USR_THUMB			0x70
#define BOOST_PRIORITY_NONE		0xfe
#define USER_TLS_STACK_RANGE		0x7f8	/* below the user TLS: the stack top and base */
#define USER_TLS_PRIORITY		0x7e0	/* the initial priority */
#define USER_TLS_AFFINITY		0x7dc	/* the initial CPU affinity mask << 16 */
#define USER_CONTEXT_SIZE		0x100
#define USER_CONTEXT_ALIGN		0x40
#define ACCESS_KERNEL			0x10	/* PLACEHOLDER: ksceKernelIsAccessibleRange: kernel memory */
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000
#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_NOT_DORMANT	0x80028028
#define SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT	0x80020005	/* PLACEHOLDER: name */


extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
extern const char *ksceGUIDName(SceUID guid);
extern int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize size);
extern int ksceKernelCopyFromToUser(void *dst, const void *src, SceSize size);
extern int ksceKernelIsAccessibleRange(SceUInt32 access, const void *addr, SceSize size);
extern uintptr_t SceSysrootForKernel_5F2CE289(SceProcessObject *process);
void initVfpContext(uintptr_t vfp);
void __attribute__((noreturn)) threadEntry(void);
SceUID sceKernelStartThreadForUser(SceUID threadId, SceSize argSize, const void *arg);

/* what a kernel argument copied to a user stack is replaced with (cleared at boot) */
uint8_t s_zeroBlock[0x80];

/*
 * Starts the dormant thread threadId with argSize bytes at arg; checkSource:
 * an argument in kernel memory is not given to a user thread (zeros are).
 */
int startThread(SceUID threadId, SceSize argSize, const void *arg, int checkSource)
{
	struct ThreadCB *self = currentThreadContext();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadRegisterContext *regs, *user;
	struct ThreadmgrDebugThreadEvent message;
	SceProcessObject *process;
	ThreadmgrDebugHandler handler;
	ThreadmgrTraceHook trace;
	uintptr_t sp, vfp, stackRange[2];
	SceSize stackSize, done, chunk;
	SceUID userThreadId, processId;
	SceUInt32 version, mask;
	uint32_t saved;
	int intr, ret, wake, group;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4454, "_sceKernelStartThread",
					   "*** error: illegal context\n");
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (self->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 4462, "_sceKernelStartThread",
			"*** error: illegal context(thread event handler). threadId=0x%08x[%s]\n",
			threadId, ksceGUIDName(threadId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (threadId == 0 || self->threadId == threadId) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
		goto restore;
	}
	intr = ksceKernelCpuSuspendIntr();
	ksceKernelSpinlockLowLock(THREAD_LOCK(self));
	if (self->threadEventMask != 0) {
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto resume;
	}
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(self));
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = ThreadmgrInSyscall() ?
			(int)(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT) :
			(int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	userThreadId = thread->userThreadId;
	if (thread->threadEventMask != 0) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto unlock;
	}
	if (!(thread->status & SCE_KERNEL_THREAD_STATUS_DORMANT)) {
		ret = SCE_KERNEL_ERROR_NOT_DORMANT;
		goto unlock;
	}
	stackSize = (int)thread->currentAttr < 0 ? thread->uStackSize : thread->kStackSize;
	if (stackSize < argSize) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
		goto unlock;
	}
	/* the thread ID marks the bottom of its stacks */
	*(SceUID *)thread->pKStack = thread->threadId;
	if ((int)thread->currentAttr < 0) {
		sp = thread->pUStack;
		ret = ksceKernelCopyToUserProc(thread->processId, (void *)sp, &thread->threadId,
					       sizeof(thread->threadId));
		if (ret < 0)
			goto unlock;
		sp += thread->uStackSize;
	} else {
		sp = thread->pKStack + thread->kStackSize;
	}
	/* the argument at the top of the stack */
	if (arg == NULL || argSize == 0) {
		thread->argSize = 0;
		thread->pArgBlock = 0;
	} else {
		thread->argSize = argSize;
		sp -= (argSize + 15) & ~15;
		thread->pArgBlock = sp;
		if ((int)thread->currentAttr >= 0) {
			memcpy((void *)sp, arg, argSize);
		} else if (ThreadmgrInSyscall()) {
			ret = ksceKernelCopyFromToUser((void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		} else if (!checkSource) {
			ret = ksceKernelCopyToUserProc(thread->processId, (void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		} else if (ksceKernelIsAccessibleRange(ACCESS_KERNEL, arg, argSize)) {
			/* kernel memory is not handed to a user thread */
			for (done = 0; done != argSize; done += chunk) {
				chunk = argSize - done;
				if (chunk > sizeof(s_zeroBlock))
					chunk = sizeof(s_zeroBlock);
				ret = ksceKernelCopyToUser((void *)(sp + done), s_zeroBlock, chunk);
				if (ret < 0)
					goto unlock;
			}
		} else {
			ret = ksceKernelCopyFromToUser((void *)sp, arg, argSize);
			if (ret < 0)
				goto unlock;
		}
	}
	/* a fresh start */
	regs = thread->pRegisterContext;
	thread->currentCpuAffinityMask = thread->initCpuAffinityMask;
	thread->exitStatus = SCE_KERNEL_ERROR_NOT_DORMANT;
	thread->currentAttr = thread->initAttr;
	thread->basePriority = thread->initPriority;
	thread->currentPriority = thread->initPriority;
	thread->ioFlags = 0;
	thread->boostPriority = BOOST_PRIORITY_NONE;
	thread->permission = thread->initPermission;
	thread->tlsProcessId = thread->processId;
	initRegisterContext(regs);
	thread->initSp = sp;
	regs->cpacr = CPACR_VFP_FULL;
	initVfpContext(thread->pVfpRegisterContext);
	process = thread->process;
	regs = thread->pRegisterContext;
	regs->pmuserenr = process->defaultPmuserenr;
	regs->pmcr = process->defaultPmcr;
	if ((int)thread->currentAttr >= 0) {
		thread->flags = 0;
		thread->kernelStackTop = 0;
		goto queue;
	}
	/* a user thread: its TLS words */
	SCE_DBG_ASSERT_LEVEL_FUNC(0, thread->userFrame != NULL, 4623, "_sceKernelStartThread");
	stackRange[1] = thread->pUStack;
	thread->userFrame = NULL;
	thread->flags = THREAD_FLAG_IN_CALLBACK;
	stackRange[0] = thread->uStackSize + stackRange[1];
	ret = ksceKernelCopyToUserProc(thread->processId,
				       (void *)(thread->tls - USER_TLS_STACK_RANGE), stackRange,
				       sizeof(stackRange));
	if (ret < 0)
		goto unlock;
	ret = ksceKernelCopyToUserProc(thread->processId, (void *)(thread->tls - USER_TLS_PRIORITY),
				       &thread->initPriority, sizeof(thread->initPriority));
	if (ret < 0)
		goto unlock;
	if (thread->initCpuAffinityMask != 0) {
		mask = thread->initCpuAffinityMask << 16;
	} else if (self->processId == THREADMGR_KERNEL_PID ||
		   (SceSysrootForDriver_CAE820B2(self->processId, &version) >= 0 &&
		    version >= SDK_VERSION_DEFAULT_AFFINITY)) {
		mask = 0;
	} else {
		mask = thread->process->cpuAffinity << 16;
	}
	ret = ksceKernelCopyToUserProc(thread->processId, (void *)(thread->tls - USER_TLS_AFFINITY),
				       &mask, sizeof(mask));
	if (ret < 0)
		goto unlock;
	/* its user context and VFP area on its kernel stack, entered through threadEntry */
	user = (struct ThreadRegisterContext *)
		((thread->kernelStackTop - USER_CONTEXT_SIZE) & ~(USER_CONTEXT_ALIGN - 1));
	memset(user, 0, USER_CONTEXT_SIZE);
	initRegisterContext(user);
	user->full = 1;
	user->r[0] = thread->argSize;
	user->r[1] = thread->pArgBlock;
	user->sp = thread->initSp;
	user->pc = thread->entry & ~1;
	user->cpsr = (thread->entry & 1) ? CPSR_USR_THUMB : CPSR_USR_ARM;
	user->lr = SceSysrootForKernel_5F2CE289(thread->process);
	user->cpacr = CPACR_VFP_FULL;
	user->tpidruro = thread->tls;
	regs = thread->pRegisterContext;
	user->pmuserenr = process->defaultPmuserenr;
	user->pmcr = process->defaultPmcr;
	user->ttbr1 = regs->ttbr1;
	user->contextidr = regs->contextidr;
	user->kernelDacr = regs->kernelDacr;
	user->userDacr = regs->userDacr;
	SCE_DBG_ASSERT_CHECK_FUNC(regs->callbackContext == NULL, 4701, "_sceKernelStartThread");
	regs->callbackContext = user;
	SCE_DBG_ASSERT_CHECK_FUNC(regs->callbackVfp == 0, 4704, "_sceKernelStartThread");
	vfp = (uintptr_t)user - sizeof(struct ThreadmgrVfpContext) - 0x20;
	memset((void *)vfp, 0, sizeof(struct ThreadmgrVfpContext));
	initVfpContext(vfp);
	thread->pRegisterContext->callbackVfp = vfp;
	thread->kernelStackTop = vfp;
	regs = thread->pRegisterContext;
queue:
	regs->full = 1;
	regs->sp = (int)thread->currentAttr < 0 ? thread->kernelStackTop : thread->initSp;
	regs->lr = 0;
	regs->pc = (uintptr_t)threadEntry;
	regs->cpsr |= CPSR_START_MASKED;
	ksceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	((struct ThreadmgrWaitNode *)thread->next)->prev = (struct ThreadmgrWaitNode *)thread->prev;
	((struct ThreadmgrWaitNode *)thread->prev)->next = (struct ThreadmgrWaitNode *)thread->next;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	wake = distributeThread(selectExecCpu(thread), thread);
	processId = thread->processId;
	trace = s_threadmgrRuntime.threadTraceHook;
	if (trace != NULL)
		trace(TRACE_EVENT_THREAD_START, 4, processId, threadId, argSize, arg);
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
	for (group = 0; group < 2; group++) {
		handler = s_threadmgrRuntime.manager->debugHandlers[group][0];
		if (handler == NULL)
			continue;
		message.processId = processId;
		message.threadId = userThreadId;
		message.event = DEBUG_THREAD_EVENT_START;
		handler(sizeof(message), &message);
	}
	ret = 0;
	if (wake)
		switchThread((intr >> 7) & 1);
	goto resume;
unlock:
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* _sceKernelStartThread */
int _sceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg)
{
	return sceKernelStartThreadForUser(threadId, argSize, arg);
}
