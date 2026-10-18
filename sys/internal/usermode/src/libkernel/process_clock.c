#include "libkernel_backends.h"
#include <kernel/processmgr.h>
#include "lifecycle.h"
#include <kernel/error.h>


SceUInt32 sceKernelGetProcessTimeLow(void)
{
	SceLongTimerRegisters *timer = libkernelState.timer;

	if (!timer)
		return sceKernelGetProcessTimeLowCore();
	return timer->countLo - libkernelState.processTimeBase.words.low;
}

SceUInt64 sceKernelGetProcessTimeWide(void)
{
	SceLongTimerRegisters *timer = libkernelState.timer;
	uint32_t high, low, nextHigh;

	if (!timer)
		return sceKernelGetProcessTimeWideCore();
	do {
		high = timer->countHi;
		low = timer->countLo;
		nextHigh = timer->countHi;
	} while (high != nextHigh);
	return (((uint64_t)high << 32) | low) - libkernelState.processTimeBase.clock;
}

int sceKernelGetProcessTime(SceKernelSysClock *clock)
{
	SceLongTimerRegisters *timer = libkernelState.timer;
	uint32_t high, low, nextHigh;

	if (!timer)
		return sceKernelGetProcessTimeCore(clock);
	if (!clock)
		return SCE_KERNEL_ERROR_INVALID_MEMORY_ACCESS;
	do {
		high = timer->countHi;
		low = timer->countLo;
		nextHigh = timer->countHi;
	} while (high != nextHigh);
	clock->quad = (((uint64_t)high << 32) | low) - libkernelState.processTimeBase.clock;
	return 0;
}
