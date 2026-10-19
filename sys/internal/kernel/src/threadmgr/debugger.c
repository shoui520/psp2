/*
 * SceKernelThreadMgr: the debugger's view of threads. A thread can be
 * debug-suspended for several reasons at once (thread +0x70); while it is,
 * its registers can be read and written (the switched-out context, the
 * callback's saved context, or the user registers a syscall saved). A thread
 * running on another CPU is reached with an SGI request the caller waits
 * on. Also the thread information and name lists for the debugger, the
 * hardware breakpoint words and the debugger's handler slots.
 */

#include <string.h>
#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr_debugger.h>
#include "scheduler.h"
#include "mutex.h"
#include "lw_mutex.h"
#include "syscall_context.h"
#include <sysmem/dipsw.h>
#include <sysmem/uid_class.h>
#include <threadmgr/threadmgr.h>
#include "affinity.h"

#define THREAD_ATTR_USER		0x80000000
#define THREAD_ATTR_UNK10000000		0x10000000	/* PLACEHOLDER: its stack is only measured with DIPSW 0xd8 */
#define THREAD_CPU_NONE			0x7fff	/* PLACEHOLDER: cpuId of a thread on no CPU */

#define DEBUG_SUSPEND_REASONS		0x000f7f03	/* the reasons a debugger can give */
#define DIPSW_THBP			0xe4
#define DIPSW_STACK_CHECK		0xd8	/* PLACEHOLDER: name */
#define THBP_SELECTOR			5

#define CPSR_DEBUGGER_MASK		0xf90ffe20	/* the cpsr bits the debugger may change */
#define CPSR_KEEP_MASK			0x06f001df
#define STACK_FILL_WORD			0xffffffff	/* a stack's unused words */
#define USER_TLS_STACK_RANGE		0x7f8	/* below the user TLS: the stack's top and bottom */

#define WAIT_TYPE_DELAY			0x00000001
#define WAIT_TYPE_UNK4			0x00000004
#define WAIT_TYPE_EVENT_FLAG		0x00000010
#define WAIT_TYPE_SEMAPHORE		0x00000020
#define WAIT_TYPE_MUTEX			0x00000040
#define WAIT_TYPE_RW_LOCK		0x00000080
#define WAIT_TYPE_COND			0x00000100
#define WAIT_TYPE_COND_MUTEX		0x00000200
#define WAIT_TYPE_EVENT			0x00010000
#define WAIT_TYPE_MULTIPLE_EVENTS	0x00020000
#define WAIT_TYPE_MSG_PIPE		0x00040000
#define WAIT_TYPE_LW_MUTEX		0x00100000
#define WAIT_TYPE_LW_COND		0x00200000
#define WAIT_TYPE_LW_COND_MUTEX		0x00400000
#define WAIT_TYPE_EXCEPTION		0x01000000
#define WAIT_TYPE_UNK4000000		0x04000000
#define WAIT_TYPE_USER			0x80000000	/* PLACEHOLDER: the same wait from a user object */
#define WAIT_ID_MULTIPLE_EVENTS		((SceUID)0x80000000)

#define THREAD_CLASS()		(s_threadmgrRuntime.manager->threadClass)

#define DEBUG_REG_PC			15	/* mask bits: r0-r14, then these */
#define DEBUG_REG_CPSR			16
#define DEBUG_REG_FPSCR			17

_Static_assert(sizeof(ThreadmgrThreadInfoForDebugger) == 0x128 &&
	       offsetof(ThreadmgrThreadInfoForDebugger, timeout) == 0xf4 &&
	       offsetof(ThreadmgrThreadInfoForDebugger, waitDetail) == 0x108,
	       "thread info for debugger layout");

/* getThreadNameListProc's entries (0x24) */
typedef struct ThreadmgrThreadName {
	SceUID threadId;
	char name[32];
} ThreadmgrThreadName;

/* the VFP save area's FPSCR (initVfpContext's layout) */
#define VFP_FPSCR(vfp)		(*(uint32_t *)((uintptr_t)(vfp) + 0x100))
#define VFP_Q(vfp, n)		((uint32_t *)((uintptr_t)(vfp) + (n) * 0x10))

static inline void dataMemoryBarrier(void)
{
	__asm__ volatile("dmb sy" : : : "memory");
}

/* the thread is the caller, or none: the debugger may not touch it */
static inline int isSelfOrNone(SceUID threadId)
{
	return threadId == 0 || (currentThreadContext() != NULL &&
				 sceKernelGetThreadId() == threadId);
}

/*
 * Writes the masked registers of a debug-suspended thread: mode 0 its
 * switched-out context, mode 1 what it returns to (a callback's saved
 * context, else the syscall's user frame, else the context).
 */
void setCpuRegisterForDebuggerRaw(struct ThreadCB *thread,
				  const ThreadmgrDebugRegisters *regs, uint32_t mask, int mode)
{
	struct ThreadRegisterContext *context;
	ThreadSyscallFrame *frame;
	uintptr_t vfp;
	int i;

	if (mode == 0) {
		for (i = 0; i < 15; i++)
			if (mask & (1u << i))
				thread->pRegisterContext->r[i] = ((const uint32_t *)regs)[i];
		if (mask & (1u << DEBUG_REG_PC))
			thread->pRegisterContext->pc = regs->pc;
		if (mask & (1u << DEBUG_REG_CPSR))
			thread->pRegisterContext->cpsr = regs->cpsr;
	} else if (mode == 1) {
		frame = thread->userFrame;
		if (thread->flags & THREAD_FLAG_IN_CALLBACK) {
			for (i = 0; i < 15; i++)
				if (mask & (1u << i))
					thread->pRegisterContext->callbackContext->r[i] =
						((const uint32_t *)regs)[i];
			if (mask & (1u << DEBUG_REG_PC))
				thread->pRegisterContext->callbackContext->pc = regs->pc;
			if (mask & (1u << DEBUG_REG_CPSR)) {
				context = thread->pRegisterContext->callbackContext;
				context->cpsr = (regs->cpsr & CPSR_DEBUGGER_MASK) |
						(context->cpsr & CPSR_KEEP_MASK);
			}
			if (!(mask & (1u << DEBUG_REG_FPSCR)))
				return;
			vfp = thread->pRegisterContext->callbackVfp;
			if (vfp == 0)
				return;
			VFP_FPSCR(vfp) = regs->fpscr;
			return;
		}
		if (frame != NULL) {
			for (i = 0; i < 13; i++)
				if (mask & (1u << i))
					frame->r[i] = regs->r[i];
			if (mask & (1u << 13))
				frame->sp = regs->sp;
			if (mask & (1u << 14))
				frame->lr = regs->lr;
			if (mask & (1u << DEBUG_REG_PC))
				frame->pc = regs->pc;
			if (mask & (1u << DEBUG_REG_CPSR))
				frame->cpsr = (regs->cpsr & CPSR_DEBUGGER_MASK) |
					      (frame->cpsr & CPSR_KEEP_MASK);
			if ((mask & (1u << DEBUG_REG_FPSCR)) && thread->pVfpRegisterContext != 0)
				frame->fpscr = regs->fpscr;
			SceCpuForDriver_BC248C30((SceInt32 *)&thread->flags,
						   THREAD_FLAG_FRAME_CHANGED);
			return;
		}
		for (i = 0; i < 15; i++)
			if (mask & (1u << i))
				thread->pRegisterContext->r[i] = ((const uint32_t *)regs)[i];
		if (mask & (1u << DEBUG_REG_PC))
			thread->pRegisterContext->pc = regs->pc;
		if (mask & (1u << DEBUG_REG_CPSR))
			thread->pRegisterContext->cpsr = (regs->cpsr & CPSR_DEBUGGER_MASK) |
						  (thread->pRegisterContext->cpsr & CPSR_KEEP_MASK);
	} else {
		SCE_DBG_ASSERT_FAIL(758);
		return;
	}
	if (!(mask & (1u << DEBUG_REG_FPSCR)) || thread->pVfpRegisterContext == 0)
		return;
	VFP_FPSCR(thread->pVfpRegisterContext) = regs->fpscr;
}

/* the thread's information; called with nothing locked */
int getThreadInfoForDebugger(SceUID threadId, SceUInt32 level, uint32_t flags,
			     ThreadmgrThreadInfoForDebugger *info)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadRegisterContext *context;
	ThreadSyscallFrame *frame;
	uint32_t userStack[2];			/* the stack's top and bottom, from the user TLS */
	uint32_t state, status, waiting, waitType, sp, words;
	SceUID waitId;
	int intr, ret, nest, i;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClassLevel(threadId, THREAD_CLASS(), level,
					      (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	strncpy(info->name, SceSysmemForDriver_E655852F(threadId), sizeof(info->name) - 1);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (thread->status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		SCE_DBG_PRINTF(0, 1674, "error: getThreadInfoForDebugger(0x%08X), deleted thread\n",
			       threadId);
		goto unlock;
	}
	info->threadId = threadId;
	info->processId = thread->processId;
	info->initAttr = thread->initAttr;
	info->processCpuAffinityMask =
		thread->process->cpuAffinity;
	info->initCpuAffinityMask = thread->initCpuAffinityMask;
	info->initPriority = thread->initPriority;
	info->lastCpuId = thread->lastExecutedCpuId;
	info->currentPriority = thread->currentPriority;
	info->cpuAffinityMask = thread->currentCpuAffinityMask;
	info->registers = (uintptr_t)thread->pRegisterContext;
	info->vfpContext = thread->pVfpRegisterContext;
	info->userFrame = (uintptr_t)thread->userFrame;
	info->tls = thread->tls;
	info->tlsSlotMemory = thread->tlsSlotMemory;
	info->reserved1 = 0;
	state = thread->status;
	if (level == 5) {
		status = thread->flags;
		waiting = state & SCE_KERNEL_THREAD_STATUS_WAITING;
		info->status = state;
		info->flags = status;
	} else {
		if (state & THREAD_STATE_OFF_CPU) {
			/* off the CPUs: the state it had */
			if (((state >> 16) & 0x7fff & 7) == 0) {
				info->status = SCE_KERNEL_THREAD_STATUS_READY;
				SCE_DBG_ASSERT_CHECK(thread->currentCpuId == -1, 1717);
				waiting = info->status & SCE_KERNEL_THREAD_STATUS_WAITING;
			} else {
				info->status = (state >> 16) & 0x7fff;
				waiting = info->status & SCE_KERNEL_THREAD_STATUS_WAITING;
			}
		} else {
			waiting = state & SCE_KERNEL_THREAD_STATUS_WAITING;
			info->status = state & 0xffff;
		}
		status = thread->flags;
		info->flags = 0;
	}
	info->userStackBase = thread->pUStack;
	info->entry = thread->entry;
	info->kernelStackTop = thread->kernelStackTop;
	info->stackBase = thread->pKStack;
	info->pArgBlock = thread->pArgBlock;
	info->reserved0[0] = 0;
	info->argSize = thread->argSize;
	info->cpuId = thread->currentCpuId;
	waitType = thread->waitType;
	if (waiting) {
		if (status & THREAD_FLAG_WAKE_PENDING)
			info->waitType = waitType | 0x80000000;
		else
			info->waitType = waitType;
	}
	waitId = thread->waitId;
	info->exitStatus = thread->exitStatus;
	if (waitId != WAIT_ID_MULTIPLE_EVENTS && waitId != -1)
		info->waitId = waitId;
	info->callbacksPending = (status >> 1) & 1;
	info->signalPending = (status >> 4) & 1;
	if (thread->delayId != -1 && thread->timeoutOutput != 0) {
		if (waitType == WAIT_TYPE_MSG_PIPE) {
			if (sceKernelCopyFromUserProc(thread->timeoutProcessId, &info->timeout,
						       (const void *)thread->timeoutOutput, 4) < 0)
				info->timeout = -1;
		} else {
			info->timeout = *(const uint32_t *)thread->timeoutOutput;
		}
		waitType = thread->waitType;
	}
	switch (waitType) {
	case WAIT_TYPE_DELAY:
	case WAIT_TYPE_UNK4:
	case WAIT_TYPE_EVENT_FLAG:
	case WAIT_TYPE_UNK4000000:
	case WAIT_TYPE_USER | WAIT_TYPE_DELAY:
	case WAIT_TYPE_USER | WAIT_TYPE_UNK4:
	case WAIT_TYPE_USER | WAIT_TYPE_EVENT_FLAG:
	case WAIT_TYPE_USER | WAIT_TYPE_UNK4000000:
		info->waitDetail[0] = thread->waitArg0;
		info->waitDetail[1] = thread->waitArg1;
		break;
	case WAIT_TYPE_COND:
	case WAIT_TYPE_COND_MUTEX:
	case WAIT_TYPE_LW_COND:
	case WAIT_TYPE_LW_COND_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_COND:
	case WAIT_TYPE_USER | WAIT_TYPE_COND_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_LW_COND:
	case WAIT_TYPE_USER | WAIT_TYPE_LW_COND_MUTEX:
		info->waitDetail[0] = thread->waitArg0;
		info->waitDetail[1] = thread->waitArg1;
		info->waitDetail[2] = thread->waitArg2;
		break;
	case WAIT_TYPE_SEMAPHORE:
	case WAIT_TYPE_MUTEX:
	case WAIT_TYPE_RW_LOCK:
	case WAIT_TYPE_EVENT:
	case WAIT_TYPE_LW_MUTEX:
	case WAIT_TYPE_EXCEPTION:
	case WAIT_TYPE_USER | WAIT_TYPE_SEMAPHORE:
	case WAIT_TYPE_USER | WAIT_TYPE_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_RW_LOCK:
	case WAIT_TYPE_USER | WAIT_TYPE_EVENT:
	case WAIT_TYPE_USER | WAIT_TYPE_LW_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_EXCEPTION:
		info->waitDetail[0] = thread->waitArg0;
		break;
	case WAIT_TYPE_MULTIPLE_EVENTS:
	case WAIT_TYPE_USER | WAIT_TYPE_MULTIPLE_EVENTS:
		info->waitDetail[0] = (uintptr_t)
			((const uint32_t *)thread->waitMultipleEvents)[10];	/* its events */
		info->waitDetail[2] = ((const uint32_t *)thread->waitMultipleEvents)[3];
		info->waitDetail[3] =
			((const uint32_t *)thread->waitMultipleEvents)[2] == 1 ? 1 : 2;
		info->waitDetail[1] = thread->waitArg1;
		break;
	case WAIT_TYPE_MSG_PIPE:
	case WAIT_TYPE_USER | WAIT_TYPE_MSG_PIPE:
		info->waitDetail[0] = thread->waitArg0;
		info->waitDetail[1] = thread->waitArg1;
		info->waitDetail[2] = thread->waitOptions;
		break;
	}
	info->intrPreemptCount = thread->intrPreemptCount;
	info->userStackSize = thread->uStackSize;
	info->threadPreemptCount = thread->threadPreemptCount;
	info->attr = thread->currentAttr;
	info->stackSize = thread->kStackSize;
	info->threadReleaseCount = thread->threadReleaseCount;
	info->changeCpuCount = thread->changeCpuCount;
	info->reserved0[1] = 0;
	info->runClocksLow = (uint32_t)thread->runClock;
	info->debugSuspendReason = thread->debugSuspendReason;
	info->reserved0[3] = 0;
	info->runClocksHigh = (uint32_t)(thread->runClock >> 32);
	info->callbackNest = thread->callbackNest;
	info->reserved0[2] = 0;
	info->context = (uintptr_t)thread;

	/* the stacks in use: the kernel stack down to the innermost saved context */
	if (thread->currentAttr & THREAD_ATTR_USER) {
		frame = thread->userFrame;
		ret = sceKernelCopyFromUserProc(thread->processId, userStack,
			(const void *)(thread->tls - USER_TLS_STACK_RANGE), sizeof(userStack));
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1876, " k: error: sceKernelCopyFromUserProc(0x%08X, %p, "
				"0x%08X, 0x%08X)=0x%08X -> SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID\n",
				thread->processId, userStack, thread->tls - USER_TLS_STACK_RANGE,
				sizeof(userStack), ret);
			ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			goto unlock;
		}
		if (thread->flags & THREAD_FLAG_IN_CALLBACK) {
			info->stackUsed = thread->pKStack - thread->kernelStackTop + thread->kStackSize;
			info->userStackUsed = thread->pUStack + thread->uStackSize -
					      thread->pRegisterContext->callbackContext->sp;
			goto measured;
		}
		nest = thread->callbackNest;
		if (frame == NULL) {
			if (nest < 1) {
				info->stackUsed = thread->pKStack - thread->kernelStackTop +
						  thread->kStackSize;
			} else {
				context = thread->pRegisterContext->callbackContext;
				SCE_DBG_ASSERT_CHECK(context != NULL, 1919);
				for (; nest > 1; nest--) {
					context = context->callbackContext;
					SCE_DBG_ASSERT_CHECK(context != NULL, 1922);
				}
				info->stackUsed = thread->pKStack + thread->kStackSize - context->sp;
			}
			sp = thread->pRegisterContext->sp;
		} else {
			if (nest < 1) {
				info->stackUsed = thread->pKStack + thread->kStackSize -
						  thread->pRegisterContext->sp;
			} else {
				context = thread->pRegisterContext->callbackContext;
				SCE_DBG_ASSERT_CHECK(context != NULL, 1893);
				for (; nest > 1; nest--) {
					context = context->callbackContext;
					SCE_DBG_ASSERT_CHECK(context != NULL, 1896);
				}
				info->stackUsed = thread->pKStack + thread->kStackSize - context->sp;
			}
			sp = frame->sp;
		}
		if (sp <= userStack[0] && userStack[1] < sp)
			info->userStackUsed = userStack[0] - sp;
		else
			info->userStackUsed = 0;
	} else {
		nest = thread->callbackNest;
		if (nest < 1) {
			info->userStackUsed = 0;
			info->stackUsed = thread->pKStack + thread->kStackSize - thread->pRegisterContext->sp;
		} else {
			context = thread->pRegisterContext->callbackContext;
			SCE_DBG_ASSERT_CHECK(context != NULL, 1946);
			for (; nest > 1; nest--) {
				context = context->callbackContext;
				SCE_DBG_ASSERT_CHECK(context != NULL, 1949);
			}
			info->userStackUsed = 0;
			info->stackUsed = thread->pKStack + thread->kStackSize - context->sp;
		}
	}
measured:
	if (!(flags & 1) || ((thread->currentAttr & THREAD_ATTR_UNK10000000) &&
			     !sceKernelCheckDipsw(DIPSW_STACK_CHECK))) {
		ret = 0;
		info->stackMaxUsed = 0;
		info->userStackMaxUsed = 0;
		goto unlock;
	}
	/* the deepest use: the stacks are filled with STACK_FILL_WORD */
	words = thread->kStackSize / 4;
	for (i = 1; i < (int)words && ((const uint32_t *)thread->pKStack)[i] == STACK_FILL_WORD; i++)
		;
	info->stackMaxUsed = thread->kStackSize - i * 4;
	if ((thread->currentAttr & THREAD_ATTR_USER) &&
	    thread->pUStack + thread->uStackSize == userStack[0] &&
	    thread->pUStack == userStack[1]) {
		ret = sceKernelCountFillValueFromUserProc(thread->processId,
			(const void *)(thread->pUStack + 4), STACK_FILL_WORD,
			thread->uStackSize - 4);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 1980,
				" k: error: sceKernelCountFillValueFromUserProc(%p)=0x%08X\n",
				(void *)(thread->pUStack + 4), ret);
		} else {
			info->userStackMaxUsed = thread->uStackSize - 4 - ret;
			ret = 0;
		}
		goto unlock;
	}
	ret = 0;
	info->userStackMaxUsed = 0;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* the information with kernel IDs (SceThreadmgrForKernel_C976719E / ForDriver_555CA6BB) */
int getThreadInfoForDebuggerByGUID(SceUID threadId, SceUInt32 level, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info)
{
	ThreadmgrThreadInfoForDebugger local;
	SceSize size;
	int ret;

	if (threadId == 0 && currentThreadContext() == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (info->size > sizeof(local))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	ret = getThreadInfoForDebugger(threadId, level, flags, &local);
	if (ret == 0) {
		size = info->size;
		if (size > sizeof(local))
			size = sizeof(local);
		memcpy(info, &local, size);
	}
	return ret;
}

/* a lw object's ID as its process sees it: the PUID in its user work area */
static inline void convertWaitObjectsLw(ThreadmgrThreadInfoForDebugger *info)
{
	ThreadmgrLwMutexWork work;
	ThreadmgrLwCond *lwCond;
	ThreadmgrLwMutex *lwMutex;
	SceUID uid;

	uid = info->waitDetail[1];
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->lwCondClass,
					 (SceObjectBase **)&lwCond) < 0) {
		info->waitDetail[1] = -1;
	} else {
		if (sceKernelCopyFromUserProc(info->processId, &work, lwCond->work, 0x10) < 0)
			work.owner = -1;
		info->waitDetail[1] = work.owner;
		sceGUIDReleaseObject(uid);
	}
	uid = info->waitDetail[2];
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->lwMutexClass,
					 (SceObjectBase **)&lwMutex) < 0) {
		info->waitDetail[2] = -1;
	} else {
		if (sceKernelCopyFromUserProc(info->processId, &work, lwMutex->work,
					       sizeof(work)) < 0)
			work.uid = -1;
		info->waitDetail[2] = work.uid;
		sceGUIDReleaseObject(uid);
	}
}

/* a cond wait's mutex and cond as PUIDs */
static inline void convertWaitObjectsCond(ThreadmgrThreadInfoForDebugger *info)
{
	ThreadmgrMutex *mutex;
	ThreadmgrCond *cond;
	SceUID uid;

	uid = info->waitDetail[2];
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->mutexClass,
					 (SceObjectBase **)&mutex) < 0) {
		info->waitDetail[2] = -1;
	} else {
		info->waitDetail[2] = mutex->core.puid;
		sceGUIDReleaseObject(uid);
	}
	uid = info->waitDetail[1];
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->condClass,
					 (SceObjectBase **)&cond) < 0) {
		info->waitDetail[1] = -1;
	} else {
		info->waitDetail[1] = cond->puid;
		sceGUIDReleaseObject(uid);
	}
}

/* the information in the IDs its process uses (SceThreadmgrForDriver_DD8D9429) */
int getThreadInfoForDebuggerForUser(SceUID threadId, SceUInt32 level, uint32_t flags,
				    ThreadmgrThreadInfoForDebugger *info)
{
	ThreadmgrThreadInfoForDebugger local;
	struct ThreadmgrThreadObject *object;
	SceUID puid;
	SceSize size, got;
	int ret;

	if (threadId == 0 && currentThreadContext() == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (info->size > sizeof(local))
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	ret = getThreadInfoForDebugger(threadId, level, flags, &local);
	if (ret != 0)
		return ret;
	local.initCpuAffinityMask <<= 16;
	local.cpuAffinityMask <<= 16;
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		local.threadId = -1;
	} else {
		local.threadId = object->context.userThreadId;
		sceGUIDReleaseObject(threadId);
	}
	if (local.waitId != 0) {
		if (PUIDGetPUIDVectorByGUID(local.processId, local.waitId, level, &puid, 4,
					    &got) < 0 || got == 0)
			local.waitId = -1;
		else
			local.waitId = puid;
	}
	switch (local.waitType) {
	case WAIT_TYPE_LW_COND:
	case WAIT_TYPE_LW_COND_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_LW_COND:
	case WAIT_TYPE_USER | WAIT_TYPE_LW_COND_MUTEX:
		convertWaitObjectsLw(&local);
		break;
	case WAIT_TYPE_COND:
	case WAIT_TYPE_COND_MUTEX:
	case WAIT_TYPE_USER | WAIT_TYPE_COND:
	case WAIT_TYPE_USER | WAIT_TYPE_COND_MUTEX:
		convertWaitObjectsCond(&local);
		break;
	}
	size = info->size;
	if (size > sizeof(local))
		size = sizeof(local);
	memcpy(info, &local, size);
	return ret;
}

/* the information of every thread visible at level */
int getThreadInfoListForDebugger(uint32_t flags, SceUInt32 level,
				 ThreadmgrThreadInfoForDebugger *infos, SceSize size, SceSize *count)
{
	ThreadmgrThreadInfoForDebugger info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(THREAD_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(THREAD_CLASS(), level, vector, num, &got);
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
		if (getThreadInfoForDebugger(vector[i], level, flags, &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		memcpy(&infos[got++], &info, sizeof(info));
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/* the same for a process's thread PUIDs, in its IDs (level 5 keeps the kernel's) */
int getThreadInfoListProcForDebugger(SceUID pid, uint32_t flags, SceUInt32 level,
				     ThreadmgrThreadInfoForDebugger *infos, SceSize size,
				     SceSize *count)
{
	ThreadmgrThreadInfoForDebugger info;
	SceSize got = 0, found;
	SceUID *vector, puid;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, THREAD_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid, THREAD_CLASS(), level, vector, num, &got);
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
		if (getThreadInfoForDebugger(SceSysmemForDriver_45D22597(pid, vector[i]), level, flags,
					     &info) != 0)
			continue;
		info.threadId = vector[i];
		if (level != 5) {
			if (info.waitId != 0) {
				if (PUIDGetPUIDVectorByGUID(pid, info.waitId, level, &puid, 4,
							    &found) < 0 || found == 0)
					info.waitId = -1;
				else
					info.waitId = puid;
			}
			info.initCpuAffinityMask <<= 16;
			info.cpuAffinityMask <<= 16;
			switch (info.waitType) {
			case WAIT_TYPE_LW_COND:
			case WAIT_TYPE_LW_COND_MUTEX:
			case WAIT_TYPE_USER | WAIT_TYPE_LW_COND:
			case WAIT_TYPE_USER | WAIT_TYPE_LW_COND_MUTEX:
				convertWaitObjectsLw(&info);
				break;
			case WAIT_TYPE_COND:
			case WAIT_TYPE_COND_MUTEX:
			case WAIT_TYPE_USER | WAIT_TYPE_COND:
			case WAIT_TYPE_USER | WAIT_TYPE_COND_MUTEX:
				convertWaitObjectsCond(&info);
				break;
			}
		}
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		memcpy(&infos[got++], &info, sizeof(info));
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	return ret;
}

/*
 * Reads a debug-suspended thread's registers: entry 0 what it was switched
 * out at (or its callback's saved context), entry 1 the syscall's user
 * frame (zero outside a syscall).
 */
void getCpuRegisterForDebuggerRaw(struct ThreadCB *thread,
				  ThreadmgrDebugRegisters *regs)
{
	struct ThreadRegisterContext *context = thread->pRegisterContext;
	ThreadSyscallFrame *frame;
	uintptr_t vfp;
	int i;

	if (!(thread->flags & THREAD_FLAG_IN_CALLBACK)) {
		for (i = 0; i < 15; i++)
			((uint32_t *)&regs[0])[i] = context->r[i];
		if (context->full == 0) {
			regs[0].pc = context->lr;
			regs[0].cpsr = context->status;
		} else {
			regs[0].pc = context->pc;
			regs[0].cpsr = context->cpsr;
		}
		vfp = thread->pVfpRegisterContext;
	} else {
		for (i = 0; i < 15; i++)
			((uint32_t *)&regs[0])[i] = context->callbackContext->r[i];
		if (context->full == 0) {
			regs[0].pc = context->callbackContext->lr;
			regs[0].cpsr = context->callbackContext->status;
		} else {
			regs[0].pc = context->callbackContext->pc;
			regs[0].cpsr = context->callbackContext->cpsr;
		}
		vfp = context->callbackVfp;
	}
	if (vfp == 0) {
		frame = thread->userFrame;
		regs[0].fpscr = 0;
	} else {
		frame = thread->userFrame;
		regs[0].fpscr = VFP_FPSCR(vfp);
	}
	if (frame == NULL) {
		for (i = 0; i < 16; i++)
			((uint32_t *)&regs[1])[i] = 0;
		regs[1].cpsr = 0;
		regs[1].fpscr = 0;
		return;
	}
	for (i = 0; i < 13; i++)
		regs[1].r[i] = frame->r[i];
	regs[1].sp = frame->sp;
	regs[1].lr = frame->lr;
	regs[1].pc = frame->pc;
	regs[1].cpsr = frame->cpsr;
	regs[1].fpscr = frame->fpscr;
}

int _sceKernelSetCpuRegisterForDebugger(SceUID threadId, const ThreadmgrDebugRegisters *regs,
					uint32_t mask, int mode)
{
	ThreadmgrDebugSetRequest request;
	struct ThreadmgrThreadObject *object;
	SceKernelSpinlock done;
	int result, cpu, intr;

	if (regs == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	result = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (result < 0) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		SCE_DBG_PRINTF(0, 878,
			"error: _sceKernelSetCpuRegisterForDebugger(0x%08X), deleted thread\n",
			threadId);
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		cpu = sceKernelCpuId();
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    cpu != object->context.currentCpuId) {
			/* still on its CPU: that CPU does it */
			request.result = &result;
			request.done = &done;
			done = 1;
			dataMemoryBarrier();
			request.threadId = threadId;
			request.regs = regs;
			request.mask = mask;
			request.mode = mode;
			issueSetCpuRegisterIntrRequest(object->context.currentCpuId, &request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			sceKernelCpuResumeIntr(intr);
			sceKernelSpinlockLowLock(&done);
			return result;
		}
		setCpuRegisterForDebuggerRaw(&object->context, regs, mask, mode);
		result = 0;
	} else {
		result = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return result;
}

int _sceKernelGetCpuRegisterForDebugger(SceUID threadId, ThreadmgrDebugRegisters *regs, int mode)
{
	ThreadmgrDebugGetRequest request;
	struct ThreadmgrThreadObject *object;
	SceKernelSpinlock done;
	int result, cpu, intr;

	if (regs == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	result = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (result < 0) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		SCE_DBG_PRINTF(0, 1119,
			"error: _sceKernelGetCpuRegisterForDebugger(0x%08X), deleted thread\n",
			threadId);
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		cpu = sceKernelCpuId();
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    cpu != object->context.currentCpuId) {
			request.result = &result;
			request.done = &done;
			done = 1;
			dataMemoryBarrier();
			request.threadId = threadId;
			request.regs = regs;
			request.mode = mode;
			issueGetCpuRegisterIntrRequest(object->context.currentCpuId, &request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			sceKernelCpuResumeIntr(intr);
			sceKernelSpinlockLowLock(&done);
			return result;
		}
		getCpuRegisterForDebuggerRaw(&object->context, regs);
		result = 0;
	} else {
		result = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return result;
}

/* writes the masked q registers of a debug-suspended thread (mode 1: not inside a syscall) */
int _sceKernelSetVfpRegisterForDebugger(SceUID threadId, const uint32_t *vfp, uint32_t mask,
					int mode)
{
	ThreadmgrDebugSetRequest request;
	struct ThreadmgrThreadObject *object;
	SceKernelSpinlock done;
	uint32_t *q;
	int result, cpu, intr, i;

	if (vfp == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	result = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (result < 0) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		SCE_DBG_PRINTF(0, 1306,
			"error: _sceKernelSetVfpRegisterForDebugger(0x%08X), deleted thread\n",
			threadId);
	} else if (mode != 0 && object->context.userFrame != NULL) {
		result = SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		cpu = sceKernelCpuId();
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    cpu != object->context.currentCpuId) {
			request.result = &result;
			request.done = &done;
			done = 1;
			dataMemoryBarrier();
			request.threadId = threadId;
			request.regs = vfp;
			request.mask = mask;
			request.mode = mode;
			issueSetVfpRegisterIntrRequest(object->context.currentCpuId, &request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			sceKernelCpuResumeIntr(intr);
			sceKernelSpinlockLowLock(&done);
			return result;
		}
		for (i = 0; i < 16; i++, vfp += 4) {
			if (!(mask & (1u << i)))
				continue;
			q = VFP_Q(object->context.pVfpRegisterContext, i);
			q[0] = vfp[0];
			q[1] = vfp[1];
			q[2] = vfp[2];
			q[3] = vfp[3];
		}
		result = 0;
	} else {
		result = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return result;
}

int _sceKernelGetVfpRegisterForDebugger(SceUID threadId, uint32_t *vfp, int mode)
{
	ThreadmgrDebugGetRequest request;
	struct ThreadmgrThreadObject *object;
	SceKernelSpinlock done;
	int result, cpu, intr, i, j;

	if (vfp == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	result = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (result < 0) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		SCE_DBG_PRINTF(0, 1511,
			"error: _sceKernelGetVfpRegisterForDebugger(0x%08X), deleted thread\n",
			threadId);
	} else if (mode != 0 && object->context.userFrame != NULL) {
		result = SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		cpu = sceKernelCpuId();
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    cpu != object->context.currentCpuId) {
			request.result = &result;
			request.done = &done;
			done = 1;
			dataMemoryBarrier();
			request.threadId = threadId;
			request.regs = vfp;
			request.mode = mode;
			issueGetVfpRegisterIntrRequest(object->context.currentCpuId, &request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			sceKernelCpuResumeIntr(intr);
			sceKernelSpinlockLowLock(&done);
			return result;
		}
		for (i = 0; i < 16; i++)
			for (j = 0; j < 4; j++)
				vfp[i * 4 + j] = VFP_Q(object->context.pVfpRegisterContext, i)[j];
		result = 0;
	} else {
		result = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return result;
}

/* the IDs and names of a process's threads visible at level; returns how many it has */
int getThreadNameListProc(SceUID pid, SceUInt32 level, ThreadmgrThreadName *names, SceSize size,
			  SceSize *count)
{
	struct ThreadmgrThreadObject *object;
	ThreadmgrThreadName entry;
	const char *name;
	SceSize got = 0, need;
	SceUID *vector;
	int num, total, found, i, intr, ret;

	if (size >= sizeof(entry) && names == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(THREAD_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(THREAD_CLASS(), level, vector, num, &got);
		if (total > (int)got) {
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			num = total;
			continue;
		}
		if (total < 1) {
			found = 0;
			got = 0;
		} else {
			/* keep the process's threads, referenced */
			found = 0;
			for (i = 0; i < total; i++) {
				if (sceGUIDReferObjectWithClass(vector[i], THREAD_CLASS(),
						(SceObjectBase **)&object) < 0)
					continue;
				if (object->context.processId == pid) {
					vector[found++] = vector[i];
				} else {
					intr = SceCpuForDriver_821FC0EE();
					ret = sceGUIDReleaseObject(vector[i]);
					sceKernelCpuResumeIntr(intr);
					SCE_DBG_ASSERT_CHECK(ret >= 0, 2710);
				}
			}
			got = 0;
			for (i = 0, need = sizeof(entry); i < found; i++, need += sizeof(entry)) {
				if (need <= size) {
					ret = sceGUIDGetName(vector[i], &name);
					SCE_DBG_ASSERT_CHECK(ret == 0, 2721);
					entry.threadId = vector[i];
					strncpy(entry.name, name, sizeof(entry.name));
					names[got++] = entry;
				}
				intr = SceCpuForDriver_821FC0EE();
				sceGUIDReleaseObject(vector[i]);
				sceKernelCpuResumeIntr(intr);
			}
		}
		if (count != NULL)
			*count = got;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		return found;
	}
	return SCE_KERNEL_ERROR_NO_MEMORY;
}

/*
 * Takes a debug-suspended thread off the CPUs: off whatever queue it is on
 * (running, ready or standby), onto the manager's debug-suspended list. The
 * thread is locked, on this CPU or none.
 */
static inline void takeThreadOffCpu(struct ThreadCB *thread, uint32_t reason)
{
	struct SceKernelThreadmgrPrivate *manager;
	uintptr_t self = (uintptr_t)thread;

	thread->debugSuspendReason = reason;
	thread->currentCpuId = -1;
	thread->status = ((thread->status & 0x7fff) << 16) | THREAD_STATE_DEBUG_SUSPENDED |
			     THREAD_STATE_OFF_CPU;
	manager = s_threadmgrRuntime.manager;
	sceKernelSpinlockLowLock(&manager->threadMgrCBLock);
	thread->next = (uintptr_t)&s_threadmgrRuntime.manager->debugSuspendedNext;
	thread->prev = s_threadmgrRuntime.manager->debugSuspendedPrev;
	s_threadmgrRuntime.manager->debugSuspendedPrev = self;
	*(uintptr_t *)thread->prev = self;
	sceKernelSpinlockLowUnlock(&manager->threadMgrCBLock);
}

/* SGI kind 8: sceKernelDebugSuspendThread for a thread on this CPU */
int debugSuspendThreadIntr(SceUID threadId, uint32_t reason, int *result,
			    SceKernelSpinlock *done)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *self;
	uint32_t status, state;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 76);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 78);
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto done;
	}
	thread = &object->context;
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	status = thread->debugSuspendReason;
	if (reason & status) {
		ret = SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED;
	} else if (status != 0) {
		thread->debugSuspendReason = reason | status;
		ret = 0;
	} else if ((thread->status & (SCE_KERNEL_THREAD_STATUS_RUNNING | SCE_KERNEL_THREAD_STATUS_READY |
					   SCE_KERNEL_THREAD_STATUS_STANDBY)) == 0) {
		thread->debugSuspendReason = reason;
		thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
	} else if (thread->currentCpuId != sceKernelCpuId() && thread->currentCpuId != THREAD_CPU_NONE) {
		/* it moved on: follow it */
		issueDebugSuspendThreadIntrRequest(thread->currentCpuId, threadId, reason, result, done);
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		sceGUIDReleaseObject(threadId);
		return ret;
	} else if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
		state = thread->status;
		if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
			SCE_DBG_ASSERT_CHECK(!(thread->flags & THREAD_FLAG_SELF_STOPPED), 114);
			runningThreadPreemptByIntr(thread, 2);
		} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
			removeReady(thread, thread->currentPriority);
		} else {
			if (!(state & SCE_KERNEL_THREAD_STATUS_STANDBY))
				SCE_PANIC(126);
			self = (struct ThreadmgrWaitNode *)thread;
			sceKernelSpinlockLowLock(&cpu->standbyLock);
			self->next->prev = self->prev;
			self->prev->next = self->next;
			sceKernelSpinlockLowUnlock(&cpu->standbyLock);
		}
		takeThreadOffCpu(thread, reason);
	} else {
		/* in a syscall: it stops on the way out */
		thread->debugSuspendReason = reason;
		thread->status |= THREAD_STATE_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
done:
	*result = ret;
	sceKernelSpinlockLowUnlock(done);
	return ret;
}

/* sceKernelDebugSuspendThread: suspends the thread for reason (added to any others) */
int sceKernelDebugSuspendThread(SceUID threadId, uint32_t reason)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadMgrCpuCB *cpu;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *self;
	SceKernelSpinlock done;
	uint32_t status, state;
	int result, cpuId, intr, i;

	if (!(reason & DEBUG_SUSPEND_REASONS))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	if (intr & 0x80) {
		result = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
		goto resume;
	}
	manager = s_threadmgrRuntime.manager;
	for (i = 0; i < 4; i++)
		if (manager->cpuCB[i].idleThread->threadId == threadId)
			goto refused;
	if (manager->cleanerThread->threadId == threadId) {
refused:
		result = SCE_KERNEL_ERROR_CAN_NOT_SUSPEND;
		goto resume;
	}
	cpuId = sceKernelCpuId();
	cpu = currentCpuState();
	result = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (result < 0) {
		result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	status = thread->debugSuspendReason;
	if (reason & status) {
		result = SCE_KERNEL_ERROR_ALREADY_DEBUG_SUSPENDED;
		goto unlock;
	}
	if (status != 0) {
		thread->debugSuspendReason = status | reason;
		goto suspended;
	}
	state = thread->status;
	if (state & THREAD_STATE_QUEUED) {
		if (thread->currentCpuId != cpuId && thread->currentCpuId != THREAD_CPU_NONE) {
			/* on another CPU: that CPU does it */
			done = 1;
			dataMemoryBarrier();
			issueDebugSuspendThreadIntrRequest(thread->currentCpuId, threadId, reason, &result,
							   &done);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceGUIDReleaseObject(threadId);
			sceKernelCpuResumeIntr(intr);
			sceKernelSpinlockLowLock(&done);
			return result;
		}
		if (thread->userFrame == NULL || (thread->flags & THREAD_FLAG_SELF_STOPPED)) {
			if (state & SCE_KERNEL_THREAD_STATUS_RUNNING) {
				SCE_DBG_ASSERT_CHECK_FUNC(currentThreadContext() == NULL, 265,
							  "sceKernelDebugSuspendThread");
				SCE_DBG_ASSERT_CHECK_FUNC(!(thread->flags & THREAD_FLAG_SELF_STOPPED), 266,
							  "sceKernelDebugSuspendThread");
				runningThreadPreemptByIntr(thread, 2);
			} else if (state & SCE_KERNEL_THREAD_STATUS_READY) {
				removeReady(thread, thread->currentPriority);
			} else {
				if (!(state & SCE_KERNEL_THREAD_STATUS_STANDBY))
					SCE_PANIC_FUNC(278, "sceKernelDebugSuspendThread");
				self = (struct ThreadmgrWaitNode *)thread;
				sceKernelSpinlockLowLock(&cpu->standbyLock);
				self->next->prev = self->prev;
				self->prev->next = self->next;
				sceKernelSpinlockLowUnlock(&cpu->standbyLock);
			}
			takeThreadOffCpu(thread, reason);
			goto suspended;
		}
	}
	thread->debugSuspendReason = reason;
	thread->status = state | THREAD_STATE_DEBUG_SUSPENDED;
suspended:
	result = 0;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return result;
}

/* takes reason off the thread's suspend reasons; with none left it runs again */
int sceKernelDebugResumeThread(SceUID threadId, uint32_t reason)
{
	struct SceKernelThreadmgrPrivate *manager;
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	struct ThreadmgrWaitNode *self;
	uint32_t status, state;
	int intr, ret, wake;

	if (!(reason & DEBUG_SUSPEND_REASONS))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto resume;
	}
	thread = &object->context;
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (!(reason & thread->debugSuspendReason)) {
		ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
		goto unlock;
	}
	status = thread->debugSuspendReason & ~reason;
	thread->debugSuspendReason = status;
	if (status == 0) {
		state = thread->status;
		thread->status = state & 0xffff;
		if (state & THREAD_STATE_OFF_CPU) {
			manager = s_threadmgrRuntime.manager;
			self = (struct ThreadmgrWaitNode *)thread;
			sceKernelSpinlockLowLock(
				&s_threadmgrRuntime.manager->threadMgrCBLock);
			self->next->prev = self->prev;
			self->prev->next = self->next;
			sceKernelSpinlockLowUnlock(&manager->threadMgrCBLock);
			wake = distributeThread(selectExecCpu(thread), thread);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
			sceGUIDReleaseObject(threadId);
			if ((wake & 1) && currentThreadContext() != NULL)
				switchThread((intr >> 7) & 1);
			ret = 0;
			goto resume;
		}
	}
	ret = 0;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	sceGUIDReleaseObject(threadId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* replaces a debug-suspended thread's reasons; returns the old ones */
int SceThreadmgrForDriver_04C6764B(SceUID threadId, uint32_t reason)
{
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	if (!(reason & DEBUG_SUSPEND_REASONS))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (currentThreadContext() == NULL && threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
			ret = object->context.debugSuspendReason;
			object->context.debugSuspendReason = reason;
		} else {
			ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
		}
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* the thread's debug suspend reasons */
int SceThreadmgrForDriver_A0B1AB21(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	if (currentThreadContext() == NULL && threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = object->context.debugSuspendReason;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForDriver_D26BFF26: whether a debug-suspended thread stopped inside a syscall */
int SceThreadmgrForDriver_D26BFF26(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	if (isSelfOrNone(threadId))
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	intr = SceCpuForDriver_821FC0EE();
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		if (!(object->context.status & THREAD_STATE_DEBUG_SUSPENDED))
			ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
		else if (object->context.userFrame != NULL)
			ret = 1;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SGI kind 0xb: _sceKernelSetCpuRegisterForDebugger for a thread on this CPU */
int setCpuRegisterForDebuggerIntr(ThreadmgrDebugSetRequest *request)
{
	SceUID threadId = request->threadId;
	struct ThreadmgrThreadObject *object;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 776);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 778);
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 783, "*** error: unknown thread id. threadId=0x%08x\n",
				      threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto done;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF(0, 793,
			"error: setCpuRegisterForDebuggerIntr(0x%08X), deleted thread\n", threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    object->context.currentCpuId != sceKernelCpuId()) {
			issueSetCpuRegisterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return ret;
		}
		ret = 0;
		setCpuRegisterForDebuggerRaw(&object->context, request->regs, request->mask,
					     request->mode);
	} else {
		ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
	return ret;
}

int SceThreadmgrForKernel_5075AE5E(SceUID threadId, const ThreadmgrDebugRegisters *regs,
				   uint32_t mask, int mode)
{
	return _sceKernelSetCpuRegisterForDebugger(threadId, regs, mask, mode);
}

/* the driver entry always writes what the thread returns to */
int SceThreadmgrForDriver_64E89DE9(SceUID threadId, const ThreadmgrDebugRegisters *regs,
				   uint32_t mask)
{
	return _sceKernelSetCpuRegisterForDebugger(threadId, regs, mask, 1);
}

/* SGI kind 0xc: _sceKernelGetCpuRegisterForDebugger for a thread on this CPU */
int getCpuRegisterForDebuggerIntr(ThreadmgrDebugGetRequest *request)
{
	SceUID threadId = request->threadId;
	struct ThreadmgrThreadObject *object;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1018);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 1020);
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 1025, "*** error: unknown thread id. threadId=0x%08x\n",
				      threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto done;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF(0, 1035,
			"error: getCpuRegisterForDebuggerIntr(0x%08X), deleted thread\n", threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    object->context.currentCpuId != sceKernelCpuId()) {
			issueGetCpuRegisterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return ret;
		}
		getCpuRegisterForDebuggerRaw(&object->context, request->regs);
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
	return ret;
}

int SceThreadmgrForKernel_C4735A7E(SceUID threadId, ThreadmgrDebugRegisters *regs)
{
	return _sceKernelGetCpuRegisterForDebugger(threadId, regs, 0);
}

int SceThreadmgrForDriver_5022689D(SceUID threadId, ThreadmgrDebugRegisters *regs)
{
	return _sceKernelGetCpuRegisterForDebugger(threadId, regs, 1);
}

/* SGI kind 0xd: _sceKernelSetVfpRegisterForDebugger for a thread on this CPU */
int setVfpRegisterForDebuggerIntr(ThreadmgrDebugSetRequest *request)
{
	SceUID threadId = request->threadId;
	struct ThreadmgrThreadObject *object;
	const uint32_t *src;
	uint32_t *q;
	int ret, i;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1193);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 1195);
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 1200, "*** error: unknown thread id. threadId=0x%08x\n",
				      threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto done;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF(0, 1210,
			"error: setVfpRegisterForDebuggerIntr(0x%08X), deleted thread\n", threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    object->context.currentCpuId != sceKernelCpuId()) {
			issueSetVfpRegisterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return ret;
		}
		for (i = 0; i < 16; i++) {
			if (!(request->mask & (1u << i)))
				continue;
			src = (const uint32_t *)request->regs + i * 4;
			q = VFP_Q(object->context.pVfpRegisterContext, i);
			q[0] = src[0];
			q[1] = src[1];
			q[2] = src[2];
			q[3] = src[3];
		}
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
	return ret;
}

int SceThreadmgrForKernel_AC726660(SceUID threadId, const uint32_t *vfp, uint32_t mask, int mode)
{
	return _sceKernelSetVfpRegisterForDebugger(threadId, vfp, mask, mode);
}

int SceThreadmgrForDriver_49A0B679(SceUID threadId, const uint32_t *vfp, uint32_t mask)
{
	return _sceKernelSetVfpRegisterForDebugger(threadId, vfp, mask, 1);
}

/* SGI kind 0xe: _sceKernelGetVfpRegisterForDebugger for a thread on this CPU */
int getVfpRegisterForDebuggerIntr(ThreadmgrDebugGetRequest *request)
{
	SceUID threadId = request->threadId;
	struct ThreadmgrThreadObject *object;
	uint32_t *dst;
	int ret, i, j;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1399);
	SCE_DBG_ASSERT_CHECK(currentThreadContext() == NULL, 1401);
	ret = sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object);
	if (ret < 0) {
		SCE_DBG_ASSERT_PRINTF(0, 1406, "*** error: unknown thread id. threadId=0x%08x\n",
				      threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		goto done;
	}
	sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
	if (object->context.status & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF(0, 1416,
			"error: getVfpRegisterForDebuggerIntr(0x%08X), deleted thread\n", threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else if (object->context.status & THREAD_STATE_DEBUG_SUSPENDED) {
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    object->context.currentCpuId != sceKernelCpuId()) {
			issueGetVfpRegisterIntrRequest(object->context.currentCpuId, request);
			sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
			sceGUIDReleaseObject(threadId);
			return ret;
		}
		dst = request->regs;
		for (i = 0; i < 16; i++)
			for (j = 0; j < 4; j++)
				dst[i * 4 + j] = VFP_Q(object->context.pVfpRegisterContext, i)[j];
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_NOT_DEBUG_SUSPENDED;
	}
	sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
	sceGUIDReleaseObject(threadId);
done:
	*request->result = ret;
	sceKernelSpinlockLowUnlock(request->done);
	return ret;
}

int SceThreadmgrForKernel_444CE648(SceUID threadId, uint32_t *vfp)
{
	return _sceKernelGetVfpRegisterForDebugger(threadId, vfp, 0);
}

int SceThreadmgrForDriver_5CDE387A(SceUID threadId, uint32_t *vfp)
{
	return _sceKernelGetVfpRegisterForDebugger(threadId, vfp, 1);
}

/* the process and thread this CPU runs (from an exception handler) */
int SceThreadmgrForKernel_6C1F092F(ThreadCBInfo *info)
{
	struct ThreadMgrCpuCB *cpu;
	int intr;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = SceCpuForDriver_821FC0EE();
	cpu = currentCpuState();
	info->processId = cpu->runningProcessId;
	info->threadId = cpu->runningThreadId;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int SceThreadmgrForKernel_C976719E(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info)
{
	return getThreadInfoForDebuggerByGUID(threadId, 5, flags, info);
}

int SceThreadmgrForDriver_555CA6BB(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info)
{
	return getThreadInfoForDebuggerByGUID(threadId, 3, flags, info);
}

int SceThreadmgrForKernel_BFF892B4(SceUID threadId, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *info)
{
	return getThreadInfoForDebuggerForUser(threadId, 5, flags, info);
}

int SceThreadmgrForDriver_DD8D9429(SceUID threadId, uint32_t flags,
				       ThreadmgrThreadInfoForDebugger *info)
{
	return getThreadInfoForDebuggerForUser(threadId, 3, flags, info);
}

int SceThreadmgrForKernel_B011EA69(uint32_t flags, ThreadmgrThreadInfoForDebugger *infos,
				   SceSize size, SceSize *count)
{
	return getThreadInfoListForDebugger(flags, 5, infos, size, count);
}

int SceThreadmgrForDriver_D7455187(uint32_t flags, ThreadmgrThreadInfoForDebugger *infos,
				   SceSize size, SceSize *count)
{
	return getThreadInfoListForDebugger(flags, 3, infos, size, count);
}

int SceThreadmgrForKernel_6351A13A(SceUID pid, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *infos, SceSize size,
				   SceSize *count)
{
	return getThreadInfoListProcForDebugger(pid, flags, 5, infos, size, count);
}

int SceThreadmgrForDriver_F2C9DC97(SceUID pid, uint32_t flags,
				   ThreadmgrThreadInfoForDebugger *infos, SceSize size,
				   SceSize *count)
{
	return getThreadInfoListProcForDebugger(pid, flags, 3, infos, size, count);
}

int SceThreadmgrForKernel_A4D270F9(SceUID pid, ThreadmgrThreadName *names, SceSize size,
				   SceSize *count)
{
	return getThreadNameListProc(pid, 5, names, size, count);
}

int SceThreadmgrForDriver_FD87586C(SceUID pid, ThreadmgrThreadName *names, SceSize size,
				   SceSize *count)
{
	return getThreadNameListProc(pid, 3, names, size, count);
}

SceUID SceThreadmgrForDriver_F311808F(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	int intr;
	SceUID ret;

	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		ret = object->context.userThreadId;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* the threads waiting on a waitable object, in queue order; returns how many wait */
int sceKernelGetWaitingThreadList(SceUID waitId, ThreadmgrWaitingThread *list, SceSize size,
				  SceSize *count)
{
	struct ThreadmgrWaitableObject *object;
	struct ThreadmgrWaitNode *node, *head;
	struct ThreadCB *thread;
	ThreadmgrWaitingThread *out;
	SceSize got;
	int intr, ret;

	if (size >= sizeof(*list) && list == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithSubclass(waitId,
			s_threadmgrRuntime.manager->waitableClass,
			(SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_UID;
		goto resume;
	}
	sceKernelSpinlockLowLock(&object->lock);
	head = (struct ThreadmgrWaitNode *)&object->next;
	ret = 0;
	got = 0;
	out = list;
	for (node = head->next; node != head; node = node->next) {
		ret++;
		if ((uintptr_t)out > (uintptr_t)list + size - sizeof(*out))
			continue;
		/* a thread, or a WME node pointing at one */
		thread = (struct ThreadCB *)node;
		if (!(node->owner & 1))
			thread = (struct ThreadCB *)node->owner;
		SCE_DBG_ASSERT_CHECK(thread->magic == THREAD_CB_MAGIC, 2893);
		got++;
		sceKernelSpinlockLowLock(THREAD_LOCK(thread));
		out->threadId = thread->threadId;
		out->processId = thread->processId;
		out->userThreadId = thread->userThreadId;
		out->reserved = 0;
		out++;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	}
	sceKernelSpinlockLowUnlock(&object->lock);
	if (count != NULL)
		*count = got;
	sceGUIDReleaseObject(waitId);
resume:
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* the thread's hardware breakpoint words (selector 5 only); its CPU reloads them */
int sceKernelSetTHBP(SceUID threadId, int selector, uint32_t value0, uint32_t value1)
{
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	if (!sceKernelCheckDipsw(DIPSW_THBP))
		return SCE_KERNEL_ERROR_CP14_DISABLED;
	if (selector != THBP_SELECTOR)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		SCE_DBG_ASSERT_CHECK(object->context.thbp != NULL, 3076);
		object->context.thbp[0] = value0;
		object->context.thbp[1] = value1;
		if ((object->context.status & SCE_KERNEL_THREAD_STATUS_RUNNING) &&
		    object->context.currentCpuId != sceKernelCpuId())
			SceThreadmgrForKernel_8D53CA9B(object->context.currentCpuId);
		ret = 0;
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

int sceKernelGetTHBP(SceUID threadId, int selector, uint32_t *value0, uint32_t *value1)
{
	struct ThreadmgrThreadObject *object;
	int intr, ret;

	if (!sceKernelCheckDipsw(DIPSW_THBP))
		return SCE_KERNEL_ERROR_CP14_DISABLED;
	if (selector != THBP_SELECTOR)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (value0 == NULL || value1 == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(threadId, THREAD_CLASS(), (SceObjectBase **)&object) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	} else {
		sceKernelSpinlockLowLock(THREAD_LOCK(&object->context));
		SCE_DBG_ASSERT_CHECK(object->context.thbp != NULL, 3147);
		ret = 0;
		*value0 = object->context.thbp[0];
		*value1 = object->context.thbp[1];
		sceKernelSpinlockLowUnlock(THREAD_LOCK(&object->context));
		sceGUIDReleaseObject(threadId);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForDriver_6C2E3A49: installs the debugger's handler group/index (2 x 8) */
int SceThreadmgrForDriver_6C2E3A49(unsigned int group, unsigned int index,
				   ThreadmgrDebugHandler handler)
{
	struct SceKernelThreadmgrPrivate *manager;
	int intr, ret;

	if (group >= 2 || index >= 8)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	manager = s_threadmgrRuntime.manager;
	if (manager->debugHandlers[group][index] == NULL) {
		manager->debugHandlers[group][index] = handler;
		ret = 0;
	} else {
		ret = SCE_KERNEL_ERROR_ALREADY_REGISTERED;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&manager->threadMgrCBLock,
						 intr);
	return ret;
}

int SceThreadmgrForDriver_47F26712(unsigned int group, unsigned int index)
{
	if (group < 2 && index < 8) {
		s_threadmgrRuntime.manager->debugHandlers[group][index] = NULL;
		return 0;
	}
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

/* SceThreadmgrForDriver_1378F6EF: claims one of three slots (manager +0x23dc) */
int SceThreadmgrForDriver_1378F6EF(unsigned int index, uint32_t value)
{
	if (index >= 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (SceCpuForDriver_CDA96E81(
		    (SceInt32 *)&s_threadmgrRuntime.manager->paHooks[index], 0, value) != 0)
		return SCE_KERNEL_ERROR_ALREADY_REGISTERED;
	return 0;
}

int SceThreadmgrForDriver_0D0644DA(unsigned int index)
{
	if (index >= 3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_threadmgrRuntime.manager->paHooks[index] = 0;
	return 0;
}
