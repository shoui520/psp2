/* The debugger's shell event count: one semaphore count per event the debugger waits for. */
#pragma once

#include <fw.h>
#include <scetypes.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include <appmgr/appmgr.h>
#include "appmgr_device.h"

extern SceUID g_appmgrDebugShellEventSemaphore;

static const SceKernelDebugInfo debuggerShellEventCountWaitPanicInfo = {
	.fileHash = 0x38fc9b3e,
	.lineHash = 0xc965bbb3,
	.funcHash = 0x668765d6,
#if FW_DEBUG
	.file = "appmgr_core_internal.h",
	.line = 168,
	.func = "waitSemaForDebuggerShellEventCount",
#endif
};

/* Takes a count without waiting; SCE_KERNEL_ERROR_WAIT_TIMEOUT when there is none. */
static inline int waitSemaForDebuggerShellEventCount(void)
{
	SceUInt32 timeout = 0;
	int result;

	result = sceKernelWaitSema(g_appmgrDebugShellEventSemaphore, 1, &timeout);
	if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT || result == 0)
		return result;
#if FW_DEBUG
	sceAppMgrDump();
	dumpBgmState();
	if (sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_FD753E7A(1, 0, &debuggerShellEventCountWaitPanicInfo,
				     __builtin_return_address(0));
#else
	SceDebugForDriver_391B5B74(&debuggerShellEventCountWaitPanicInfo,
		       __builtin_return_address(0));
#endif
	return result;
}

static const SceKernelDebugInfo debuggerShellEventCountPanicInfo = {
	.fileHash = 0x38fc9b3e,
	.lineHash = 0xe4bf4f91,
	.funcHash = 0x7c920bfa,
#if FW_DEBUG
	.file = "appmgr_core_internal.h",
	.line = 176,
	.func = "signalSemaForDebuggerShellEventCount",
#endif
};

static inline void signalSemaForDebuggerShellEventCount(void)
{
	if (sceKernelSignalSema(g_appmgrDebugShellEventSemaphore, 1) == 0)
		return;
#if FW_DEBUG
	sceAppMgrDump();
	dumpBgmState();
	if (sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_FD753E7A(1, 0, &debuggerShellEventCountPanicInfo,
				     __builtin_return_address(0));
#else
	SceDebugForDriver_391B5B74(&debuggerShellEventCountPanicInfo,
		       __builtin_return_address(0));
#endif
}
