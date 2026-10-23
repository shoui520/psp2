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

uint32_t processSdkVersion;
SceLongTimerRegisters *processTimer;
union ProcessTimeBase processTimeBase;
unsigned int backtraceAvailable;
uint32_t pmUserEnable;
uint32_t exitStarted;
uint32_t __stack_chk_guard = 0;
uint32_t SceKernelStackChkGuard = 0;
extern uint32_t sceLibSspStackChkGuard __attribute__((alias("__stack_chk_guard")));

/*
 * The exit entries are where a thread or callback returns to. Neither they nor the two nops
 * before each can be unwound, so an unwinder that looks up the return address (or the
 * instruction before it) stops at the thread's last frame.
 */
#define EXIT_ENTRY_PAD(name)						\
	__asm__("\t.text\n\t.thumb\n\t.type " #name ", %function\n"	\
		"\t.thumb_func\n" #name ":\n\t.fnstart\n\tnop\n\tnop\n"	\
		"\t.cantunwind\n\t.fnend\n\t.size " #name ", .-" #name)
#define EXIT_ENTRY	__attribute__((optimize("no-unwind-tables", "no-section-anchors")))

EXIT_ENTRY_PAD(ExitThreadEntryPad);

EXIT_ENTRY void ExitThreadEntry(int status)
{
	SceThreadmgrCoredumpTime_sceKernelExitThread(status);
	__breakpoint(BKPT_ABORT);
}

EXIT_ENTRY_PAD(ExitDeleteThreadEntryPad);

EXIT_ENTRY void ExitDeleteThreadEntry(int status)
{
	sceKernelExitDeleteThread(status);
	__breakpoint(BKPT_ABORT);
}

EXIT_ENTRY_PAD(ExitCallbackEntryPad);

EXIT_ENTRY void ExitCallbackEntry(int status)
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
	struct LibkernelAddresses addresses;
	const struct ProcessParamHeader *parameter;
	SceKernelCallFrame frames[1];
	SceUInt32 count;
	int result;

	addresses.size = sizeof(addresses);
	addresses.exitThread = ExitThreadEntry;
	addresses.exitDeleteThread = ExitDeleteThreadEntry;
	addresses.exitCallback = ExitCallbackEntry;
	addresses.invokeEntry = InvokeThreadEntry;
	addresses.processTimeBase = &processTimeBase.clock;
	addresses.pmUserEnable = &pmUserEnable;
	_sceKernelRegisterLibkernelAddresses(&addresses);
	_sceKernelGetTimer5Reg(&processTimer);
	result = sceKernelBacktrace(sceKernelGetThreadId(), frames, sizeof(frames),
				    &count, 0);
	backtraceAvailable = (uint32_t)~result >> 31;
	SceKernelStackChkGuard = *(uint32_t *)sceKernelGetTLSAddr(7);
	__stack_chk_guard = SceKernelStackChkGuard;
	parameter = sceKernelGetProcessParam();
	if (!parameter || parameter->magic != SCE_PROCESS_PARAM_MAGIC || !parameter->version)
		processSdkVersion = 0;
	else
		processSdkVersion = parameter->sdkVersion;
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	for (;;)
		;
}

void SceLibKernel_023EAA62(void)
{
}

uint32_t sceKernelGetPMUSERENR(void)
{
	return pmUserEnable;
}

uint32_t SceLibKernel_9F793F84(void)
{
	return processSdkVersion;
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
	if (sceKernelAtomicCompareAndSet32(&exitStarted, 0, 1)) {
		SceThreadmgrCoredumpTime_sceKernelExitThread(status);
		return 0;
	}
	sceKernelCallModuleExit(0x80);
	_sceKernelExitProcessForUser(status);
	return 0;
}
