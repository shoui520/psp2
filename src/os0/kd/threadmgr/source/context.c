/* SceKernelThreadMgr: the running thread's context queries and setters. */
#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/error.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

SceUID ksceKernelGetThreadId(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->threadId : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

SceUID sceKernelGetProcessId(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->processId : 0x10005;
}

int SceThreadmgrForDriver_332E127C(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->permission : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

int sceKernelIsUserModeThread(void)
{
	struct ThreadCB *thread;
	SceUInt32 state;
	int ret;

	ENTER_SYSCALL(state);
	if (!currentThreadContext()) {
		EXIT_SYSCALL(state);
		return 0;
	}
	thread = currentThreadContext();
	SCE_DBG_ASSERT_CHECK(thread != NULL, 7596);
	ret = thread->currentAttr >> 31;
	EXIT_SYSCALL(state);
	return ret;
}

/* SceThreadmgrForDriver_AB977C72: the calling thread's attributes */
int SceThreadmgrForDriver_AB977C72(SceUInt32 *attributes)
{
	struct ThreadCB *thread = currentThreadContext();
	SceUInt32 state;
	int intr, ret;

	ENTER_SYSCALL(state);
	if (attributes == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (currentThreadContext() == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	} else {
		intr = ksceKernelCpuSuspendIntr();
		ksceKernelSpinlockLowLock(&thread->lock);
		*attributes = thread->currentAttr;
		ksceKernelSpinlockLowUnlock(&thread->lock);
		ksceKernelCpuResumeIntr(intr);
		ret = 0;
	}
	EXIT_SYSCALL(state);
	return ret;
}

int ksceKernelGetThreadCurrentPriority(void)
{
	struct ThreadCB *thread;
	SceUInt32 state;
	int priority;

	ENTER_SYSCALL(state);
	if (!currentThreadContext()) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 6389, "sceKernelGetThreadCurrentPriority",
					   "*** error: illegal context\n");
		EXIT_SYSCALL(state);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	thread = currentThreadContext();
	priority = thread->currentPriority;
	EXIT_SYSCALL(state);
	return priority;
}

int sceKernelCheckWaitableStatus(void)
{
	SceKernelIntrStatus state;
	int result = 0;

	if (!currentThreadContext())
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	state = ksceKernelCpuSuspendIntr();
	if ((state & 0x80) || currentCpuState()->disabledDispatch)
		result = SCE_KERNEL_ERROR_CAN_NOT_WAIT;
	ksceKernelCpuResumeIntr(state);
	return result;
}

void *ksceKernelGetTLSAddr(unsigned int index)
{
	struct ThreadCB *thread = currentThreadContext();
	uintptr_t tls;

	if (index > 0xff || !thread)
		return NULL;
	tls = thread->tls;
	if (!tls)
		return NULL;
	return (void *)(tls - 0x800 + index * 4);
}

int SceThreadmgrForDriver_20C228E4(void)
{
	if (!currentThreadContext())
		return 0;
	return (currentThreadContext()->flags >> 15) & 1;
}

int ksceKernelSetPermission(int permission)
{
	struct ThreadCB *thread = currentThreadContext();
	int previous;

	if (permission < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->permission;
	thread->permission = permission;
	return previous;
}

SceUID ksceKernelGetProcessIdFromTLS(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->tlsProcessId : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

SceUID ksceKernelSetProcessIdToTLS(SceUID pid)
{
	struct ThreadCB *thread = currentThreadContext();
	SceUID previous;

	if (pid < 0)
		return SCE_KERNEL_ERROR_INVALID_UID;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->tlsProcessId;
	thread->tlsProcessId = pid;
	return previous;
}

int SceThreadmgrForDriver_56C18B11(void)
{
	struct ThreadCB *thread = currentThreadContext();

	return thread ? thread->ioFlags : SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
}

int SceThreadmgrForDriver_84EB1EA4(int value)
{
	struct ThreadCB *thread = currentThreadContext();
	int previous;

	if (value < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!thread)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	previous = thread->ioFlags;
	thread->ioFlags = value;
	return previous;
}

int SceThreadmgrForKernel_BB56D42F(void)
{
	struct ThreadMgrCpuCB *cpu = currentCpuState();
	uintptr_t current;

	if (cpu->disabledDispatch)
		return 0;
	current = cpu->runningThread;
	if (current)
		return current != cpu->nextRunningThread;
	return 1;
}
