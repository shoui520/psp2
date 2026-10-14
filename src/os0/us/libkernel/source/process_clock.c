#include <sce/libkernel_backends.h>
#include <psp2/kernel/processmgr.h>
#include "lifecycle.h"
#include <sce/error.h>


SceUInt32 sceKernelGetProcessTimeLow(void)
{
	struct ProcessTimerRegisters *timer = libkernelState.timer;

	if (!timer)
		return sceKernelGetProcessTimeLowCore();
	return timer->low - libkernelState.processTimeBase.words.low;
}

SceUInt64 sceKernelGetProcessTimeWide(void)
{
	struct ProcessTimerRegisters *timer = libkernelState.timer;
	uint32_t high, low, nextHigh;

	if (!timer)
		return sceKernelGetProcessTimeWideCore();
	do {
		high = timer->high;
		low = timer->low;
		nextHigh = timer->high;
	} while (high != nextHigh);
	return (((uint64_t)high << 32) | low) - libkernelState.processTimeBase.clock;
}

int sceKernelGetProcessTime(SceKernelSysClock *clock)
{
	struct ProcessTimerRegisters *timer = libkernelState.timer;
	uint32_t high, low, nextHigh;

	if (!timer)
		return sceKernelGetProcessTimeCore(clock);
	if (!clock)
		return SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS;
	do {
		high = timer->high;
		low = timer->low;
		nextHigh = timer->high;
	} while (high != nextHigh);
	*clock = (((uint64_t)high << 32) | low) - libkernelState.processTimeBase.clock;
	return 0;
}
