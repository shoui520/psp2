/*
 * SceKernelThreadMgr: GPU exceptions. The GPU driver reports the process
 * whose GPU work faulted; the debugger's handlers decide, by default the
 * process is debug-suspended once.
 */

#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>

#define PROCESS_DEBUG_SUSPEND_GPU	1	/* the processDebugSuspendHook kind */

/* what the debugger's handler decides */
#define GPU_EXCEPTION_PANIC		0
#define GPU_EXCEPTION_SUSPEND_PROCESS	1
#define GPU_EXCEPTION_IGNORE		2
#define GPU_EXCEPTION_PANIC2		3	/* PLACEHOLDER: name */

typedef void (*ThreadmgrProcessDebugSuspendHook)(SceUID pid, SceUID threadId, int kind,
						 uintptr_t arg);

/* SceThreadmgrForDriver_37F51E91: called from a kernel thread with the faulting process */
int threadmgrGpuExceptionHandler(SceUID pid)
{
	struct SceKernelThreadmgrPrivate *manager;
	SceProcessObject *process;
	ThreadmgrDebugHandler handler;
	ThreadmgrProcessDebugSuspendHook hook;
	SceUID message;
	int action, ret;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_FAIL(58);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	if (currentThreadContext()->currentAttr & THREAD_ATTR_USER) {
		SCE_DBG_ASSERT_FAIL(65);
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	manager = s_threadmgrRuntime.manager;
	handler = manager->debugHandlers[0][DEBUG_HANDLER_GPU_EXCEPTION];
	if (handler != NULL) {
		message = pid;
		handler(sizeof(message), &message);
		manager = s_threadmgrRuntime.manager;
	}
	handler = manager->debugHandlers[1][DEBUG_HANDLER_GPU_EXCEPTION];
	if (handler == NULL) {
		ret = sceGUIDReferObjectWithClass(pid, manager->processClass,
						   (SceObjectBase **)&process);
		if (ret < 0)
			goto invalid;
	} else {
		message = pid;
		action = handler(sizeof(message), &message);
		ret = sceGUIDReferObjectWithClass(pid,
			s_threadmgrRuntime.manager->processClass, (SceObjectBase **)&process);
		if (ret < 0)
			goto invalid;
		switch (action) {
		case GPU_EXCEPTION_PANIC:
			SCE_PANIC(94);
		case GPU_EXCEPTION_SUSPEND_PROCESS:
			break;
		case GPU_EXCEPTION_IGNORE:
			goto release;
		case GPU_EXCEPTION_PANIC2:
			SCE_PANIC(116);
		default:
			SCE_PANIC(119);
		}
	}
	if (sceKernelAtomicCompareAndSet32((SceInt32 *)&process->abortRequested, 0, 1) == 0) {
		if (pid != SCE_KERNEL_PROCESS_ID) {
			ret = SceSysrootForKernel_1247A825(pid, PROCESS_DEBUG_SUSPEND_EXCEPTION);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xf, 102,
					"(): sceKernelProcessDebugSuspend(0x%08X)=0x%08X\n", pid, ret);
				SCE_DBG_ASSERT_FAIL(103);
			}
		}
		hook = (ThreadmgrProcessDebugSuspendHook)
			s_threadmgrRuntime.manager->processDebugSuspendHook;
		if (hook != NULL)
			hook(pid, -1, PROCESS_DEBUG_SUSPEND_GPU,
			     s_threadmgrRuntime.manager->processDebugSuspendArg);
	}
release:
	sceGUIDReleaseObject(pid);
	return ret;
invalid:
	SCE_DBG_PRINTF(0, 86, "error: threadmgrGpuExceptionHandler(), invalid processId(0x%08X)\n",
		       pid);
	SCE_DBG_ASSERT_FAIL(87);
	return ret;
}
