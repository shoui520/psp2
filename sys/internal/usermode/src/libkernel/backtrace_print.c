#include <stddef.h>
#include <psp2common/kernel/modulemgr.h>

#include "backtrace.h"
#include "lifecycle.h"
#include <kernel/error.h>

extern SceUID sceKernelGetProcessId(void);
extern int sceKernelGetModuleInfoByAddr(const void *address,
		SceKernelModuleInfo *info);
extern int sceClibPrintf(const char *format, ...);

_Static_assert(offsetof(SceKernelModuleInfo, module_name) == 12,
		"backtrace module name");
_Static_assert(offsetof(SceKernelModuleInfo, segments[0].vaddr) == 0x15c,
		"backtrace first segment address");

int SceLibKernel_20E2D4B7(SceUID processId, const SceKernelCallFrame *frames,
		SceUInt32 count)
{
	SceKernelModuleInfo info;
	SceUInt32 index;
	SceUInt32 frameSize;
	int result;

	if (!libkernelState.backtraceAvailable)
		return SCE_KERNEL_ERROR_ILLEGAL_PERMISSION;
	if (processId && sceKernelGetProcessId() != processId)
		return SCE_KERNEL_ERROR_INVALID_PID;
	if (!frames)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

	sceClibPrintf("=== Print Backtrace =================\n");
	for (index = 0; index != count; index++) {
		info.size = sizeof(info);
		__builtin_prefetch((const char *)&frames[index] + 28);
		result = sceKernelGetModuleInfoByAddr((const void *)frames[index].pc,
				&info);
		frameSize = index == count - 1 ? 0 :
				frames[index + 1].sp - frames[index].sp;
		if (result < 0) {
			sceClibPrintf("(%d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [no module info]\n",
					index, frames[index].pc, frames[index].sp, frameSize);
		} else {
			sceClibPrintf("(%d) PC:0x%08X, SP:0x%08X(size=0x%08X byte) [%s + 0x%08X]\n",
					index, frames[index].pc, frames[index].sp, frameSize,
					info.module_name, frames[index].pc -
					(SceUIntVAddr)info.segments[0].vaddr);
		}
	}
	sceClibPrintf("=====================================\n");
	return 0;
}

int sceKernelPrintBacktrace(const SceKernelCallFrame *frames, SceUInt32 count)
{
	return SceLibKernel_20E2D4B7(0, frames, count);
}
