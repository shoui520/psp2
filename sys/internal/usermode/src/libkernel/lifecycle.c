#include "libkernel_backends.h"
#include <scetypes.h>
#include <kernel/modulemgr.h>
#include <modulemgr/module_info.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "lifecycle.h"
#include "backtrace.h"
#include <kernel/error.h>
#include <sysmem/compiler.h>

struct ProcessParamHeader {
	uint32_t size;
	uint32_t magic;			/* SCE_PROCESS_PARAM_MAGIC */
	uint32_t version;
	uint32_t sdkVersion;
};

#define SCE_PROCESS_PARAM_MAGIC		0x32505350	/* "PSP2" */

/* the attributes sceKernelChangeCurrentThreadAttr accepts (and ignores) */
#define THREAD_ATTR_CHANGEABLE		0x81000000

struct ThreadEntryCall {
	void (*function)(void *argument);
	void *argument;
};

struct LibkernelAddresses {
	uint32_t size;
	void (*exitThread)(int status);
	void (*exitDeleteThread)(int status);
	void (*exitCallback)(int status);
	void (*invokeEntry)(SceSize size, const struct ThreadEntryCall *entry);
	uint64_t *processTimeBase;
	uint32_t *pmUserEnable;
};

struct LibkernelState libkernelState;
uint32_t __stack_chk_guard = 0;
uint32_t SceKernelStackChkGuard = 0;
extern uint32_t sceLibSspStackChkGuard __attribute__((alias("__stack_chk_guard")));

void ExitThreadEntry(int status)
{
	SceThreadmgrCoredumpTime_sceKernelExitThread(status);
	__breakpoint(BKPT_ABORT);
}

void ExitDeleteThreadEntry(int status)
{
	sceKernelExitDeleteThread(status);
	__breakpoint(BKPT_ABORT);
}

void ExitCallbackEntry(int status)
{
	_sceKernelExitCallback(status);
	__breakpoint(BKPT_ABORT);
}

void InvokeThreadEntry(SceSize size, const struct ThreadEntryCall *entry)
{
	entry->function(entry->argument);
}

int module_start(SceSize size, void *argument)
{
	struct LibkernelAddresses addresses = {
		sizeof(addresses), ExitThreadEntry, ExitDeleteThreadEntry,
		ExitCallbackEntry, InvokeThreadEntry,
		&libkernelState.processTimeBase.clock, &libkernelState.pmUserEnable
	};
	const struct ProcessParamHeader *parameter;
	SceKernelCallFrame frames[1];
	SceUInt32 count;
	int result;
	uint32_t guard;

	_sceKernelRegisterLibkernelAddresses(&addresses);
	_sceKernelGetTimer5Reg(&libkernelState.timer);
	result = sceKernelBacktrace(sceKernelGetThreadId(), frames, sizeof(frames),
				    &count, 0);
	libkernelState.backtraceAvailable = (uint32_t)~result >> 31;
	guard = *(uint32_t *)sceKernelGetTLSAddr(7);
	SceKernelStackChkGuard = guard;
	__stack_chk_guard = guard;
	parameter = sceKernelGetProcessParam();
	if (!parameter || parameter->magic != SCE_PROCESS_PARAM_MAGIC || !parameter->version)
		libkernelState.processSdkVersion = 0;
	else
		libkernelState.processSdkVersion = parameter->sdkVersion;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	for (;;)
		;
}

void SceLibKernel_023EAA62(void)
{
}

uint32_t sceKernelGetPMUSERENR(void)
{
	return libkernelState.pmUserEnable;
}

uint32_t SceLibKernel_9F793F84(void)
{
	return libkernelState.processSdkVersion;
}

int sceKernelChangeCurrentThreadAttr(uint32_t clear, uint32_t set)
{
	return ((clear | set) & ~THREAD_ATTR_CHANGEABLE) ? SCE_KERNEL_ERROR_ILLEGAL_ATTR : 0;
}

/* type is narrowed to the module list's 8-bit selector here, not by the caller */
void sceKernelCallModuleExit(int type)
{
	SceUID ids[128];
	SceSize count = 128;
	SceKernelModuleInfo info;
	SceSize index;

	if (sceKernelGetModuleList(type, ids, &count) || !count)
		return;
	sceClibMemset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	index = 0;
	do {
		if (!sceKernelGetModuleInfo(ids[index], &info) &&
		    info.exit_entry && info.state == SCE_KERNEL_MODULE_STATE_STARTED)
			((void (*)(void))info.exit_entry)();
	} while (++index < count);
}

int sceKernelExitProcess(int status)
{
	if (sceKernelAtomicCompareAndSet32(&libkernelState.exitStarted, 0, 1)) {
		SceThreadmgrCoredumpTime_sceKernelExitThread(status);
		return 0;
	}
	sceKernelCallModuleExit(0x80);
	_sceKernelExitProcessForUser(status);
	return 0;
}
