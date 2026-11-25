/*
 * ScePerf: ARM performance monitor user-mode API.
 */

#include "libperf_internal.h"

int scePerfArmPmonReset(SceUID threadId)
{
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (threadId != SCE_PERF_ARM_PMON_THREAD_ID_SELF) {
		ret = sceKernelPerfArmPmonReset(threadId);
		if (ret == SCE_KERNEL_ERROR_INVALID_UID)
			return SCE_PERF_ERROR_INVALID_ARGUMENT;
		return ret;
	}

	{
		SceUInt32 pmcr;

		__asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(pmcr));
		pmcr |= 6;
		__asm__ volatile("mcr p15, 0, %0, c9, c12, 0" :: "r"(pmcr));
	}
	return 0;
}

int scePerfArmPmonSelectEvent(SceUID threadId, SceUInt32 counter, SceUInt8 eventCode)
{
	int ret;

	if (counter >= SCE_PERF_ARM_PMON_PMCNT_NUM)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	if (!((eventCode <= 0x07U) ||
	      (eventCode >= 0x09U && eventCode <= 0x0DU) ||
	      (eventCode >= 0x0FU && eventCode <= 0x12U) ||
	      (eventCode >= 0x40U && eventCode <= 0x42U) ||
	      (eventCode >= 0x50U && eventCode <= 0x51U) ||
	      (eventCode >= 0x60U && eventCode <= 0x68U) ||
	      (eventCode == 0x6EU) ||
	      (eventCode >= 0x70U && eventCode <= 0x74U) ||
	      (eventCode >= 0x80U && eventCode <= 0x86U) ||
	      (eventCode >= 0x8AU && eventCode <= 0x8BU) ||
	      (eventCode >= 0x90U && eventCode <= 0x93U) ||
	      (eventCode >= 0xA0U && eventCode <= 0xA5U))) {
		return SCE_PERF_ERROR_INVALID_ARGUMENT;
	}

	ret = sceKernelPerfArmPmonSelectEvent(threadId, counter, eventCode);
	if (ret == SCE_KERNEL_ERROR_INVALID_UID)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;
	return ret;
}

int scePerfArmPmonStart(SceUID threadId)
{
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (threadId != SCE_PERF_ARM_PMON_THREAD_ID_SELF) {
		ret = sceKernelPerfArmPmonStart(threadId);
		if (ret == SCE_KERNEL_ERROR_INVALID_UID)
			return SCE_PERF_ERROR_INVALID_ARGUMENT;
		return ret;
	}

	__asm__ volatile("mcr p15, 0, %0, c9, c12, 1" :: "r"(0x8000003FU));
	return 0;
}

int scePerfArmPmonStop(SceUID threadId)
{
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (threadId != SCE_PERF_ARM_PMON_THREAD_ID_SELF) {
		ret = sceKernelPerfArmPmonStop(threadId);
		if (ret == SCE_KERNEL_ERROR_INVALID_UID)
			return SCE_PERF_ERROR_INVALID_ARGUMENT;
		return ret;
	}

	__asm__ volatile("mcr p15, 0, %0, c9, c12, 2" :: "r"(0x8000003FU));
	return 0;
}

int scePerfArmPmonGetCounterValue(SceUID threadId, SceUInt32 counter, SceUInt32 *pValue)
{
	int ret;

	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (pValue == NULL)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	if (counter >= SCE_PERF_ARM_PMON_PMCNT_NUM && counter != SCE_PERF_ARM_PMON_CYCLE_COUNTER)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	if (threadId != SCE_PERF_ARM_PMON_THREAD_ID_SELF) {
		if (threadId == SCE_UID_THREAD_ID_PROCESS_ALL || threadId == (SceUID)-1)
			return SCE_PERF_ERROR_INVALID_ARGUMENT;
		ret = sceKernelPMonThreadGetCounter(threadId, counter, pValue);
		if (ret == SCE_KERNEL_ERROR_INVALID_UID)
			return SCE_PERF_ERROR_INVALID_ARGUMENT;
		return ret;
	}

	if (counter < SCE_PERF_ARM_PMON_PMCNT_NUM) {
		SceUInt32 val;

		__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" :: "r"(counter));
		__asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(val));
		*pValue = val;
	} else if (counter == SCE_PERF_ARM_PMON_CYCLE_COUNTER) {
		SceUInt32 val;

		__asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(val));
		*pValue = val;
	}

	return 0;
}

int scePerfArmPmonSetCounterValue(SceUID threadId, SceUInt32 counter, SceUInt32 value)
{
	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if (counter >= SCE_PERF_ARM_PMON_PMCNT_NUM && counter != SCE_PERF_ARM_PMON_CYCLE_COUNTER)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	if (threadId != SCE_PERF_ARM_PMON_THREAD_ID_SELF)
		return sceKernelPerfArmPmonSetCounterValue(threadId, counter, value);

	if (counter < SCE_PERF_ARM_PMON_PMCNT_NUM) {
		__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" :: "r"(counter));
		__asm__ volatile("mcr p15, 0, %0, c9, c13, 2" :: "r"(value));
	} else if (counter == SCE_PERF_ARM_PMON_CYCLE_COUNTER) {
		__asm__ volatile("mcr p15, 0, %0, c9, c13, 0" :: "r"(value));
	}

	return 0;
}

int scePerfArmPmonSoftwareIncrement(SceUInt32 mask)
{
	if (sceKernelGetPMUSERENR() == 0)
		return SCE_PERF_ERROR_NOT_INITIALIZED;

	if ((mask & ~SCE_PERF_ARM_PMON_COUNTER_MASK_ALL) != 0)
		return SCE_PERF_ERROR_INVALID_ARGUMENT;

	__asm__ volatile("mcr p15, 0, %0, c9, c12, 4" :: "r"(mask));
	return 0;
}
