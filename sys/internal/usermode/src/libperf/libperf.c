/*
 * ScePerf: module entry points and timebase API.
 */

#include "libperf_internal.h"

int module_start(SceSize args, const void *argp)
{
	SceUInt32 ioBase;

	(void)args;
	(void)argp;

	razorCpuInit(SCE_PERF_ARM_PMON_PMCNT_NUM);
	ioBase = sceKernelPaGetIoBaseAddress();
	g_paIoBase = (volatile SceUInt32 *)ioBase;
	if (ioBase == 0)
		return SCE_KERNEL_START_FAILED;
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize args, const void *argp)
{
	(void)args;
	(void)argp;
	return SCE_KERNEL_STOP_SUCCESS;
}

SceUInt64 scePerfGetTimebaseValue(void)
{
	volatile SceUInt32 *base = g_paIoBase;
	SceUInt32 hi;
	SceUInt32 lo;
	SceUInt32 hi2;

	do {
		hi = base[0x8C / 4];
		lo = base[0x88 / 4];
		hi2 = base[0x8C / 4];
	} while (hi != hi2);

	return ((SceUInt64)hi << 32) | (SceUInt64)lo;
}

SceUInt32 scePerfGetTimebaseFrequency(void)
{
	return sceKernelPaGetTimebaseFrequency();
}
