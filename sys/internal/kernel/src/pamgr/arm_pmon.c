#include "pamgr_internal.h"
#include <psp2kern/kernel/sysmem/uid_puid.h>
#include <psp2kern/kernel/threadmgr/debugger.h>
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <excpmgr/excpmgr_cpu.h>
#include <kernel/error.h>

int ksceKernelSetPermission(int level);
void *ksceKernelAlloc(SceSize size);
void ksceKernelFree(void *memory);
int pmonCpuSetEventSelf(SceUID thread, unsigned int counter, unsigned int event);
int sceKernelPerfArmPmonOpen(void);
int sceKernelPerfArmPmonClose(void);
int sceKernelPerfArmPmonReset(SceUID thread);
int sceKernelPerfArmPmonSelectEvent(SceUID thread, unsigned int counter,
				  unsigned int event);
int sceKernelPerfArmPmonStart(SceUID thread);
int sceKernelPerfArmPmonStop(SceUID thread);
int sceKernelPerfArmPmonSetCounterValue(SceUID thread, unsigned int counter,
				      uint32_t value);

int ScePamgrForDriver_0A942B36(uint32_t counters)
{
	if (counters & ~ARM_PMON_COUNTER_MASK)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	writePmswinc(counters);		/* software increment */
	return 0;
}

int armPmonResetAllCounters(SceUID thread)
{
	unsigned int counter;
	int result;

	for (counter = 0; counter < ARM_PMON_COUNTERS; counter++) {
		result = SceThreadmgrForKernel_7B3368F1(thread, counter, 0);
		if (result != 0) {
			SCE_DBG_PRINTF(0, 51,
				"[Error] sceKernelPMonThreadSetCounter (ret = 0x%08x)\n",
				result);
			return result;
		}
	}
	result = SceThreadmgrForKernel_7B3368F1(thread, ARM_PMON_CYCLE_COUNTER, 0);
	if (result != 0)
		SCE_DBG_PRINTF(0, 59,
			"[Error] sceKernelPMonThreadSetCounter (ret = 0x%08x)\n",
			result);
	return result;
}

int ScePamgrForDriver_ADDE4C07(SceUID thread, unsigned int counter,
			     uint32_t *value)
{
	if ((counter >= ARM_PMON_COUNTERS && counter != ARM_PMON_CYCLE_COUNTER) ||
	    thread == -1 || thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return SceThreadmgrForKernel_170F69D6(thread, counter, value);
}

int armPmonExecAllThread(unsigned int operation, unsigned int counter,
			 uint32_t value)
{
	SceUID process = sceKernelGetProcessId();
	SceUID *threads;
	SceSize bytes;
	unsigned int i;
	int count, result;

	result = ksceKernelGetThreadIdList(process, NULL, 0, NULL);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 84,
			"[Error] sceKernelGetThreadIdList (ret = 0x%08x)\n", result);
		return result;
	}
	bytes = (uint32_t)result * sizeof(*threads);
	threads = ksceKernelAlloc(bytes);
	if (threads == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	result = ksceKernelGetThreadIdList(process, threads, bytes, &count);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 97,
			"[Error] sceKernelGetThreadIdList (ret = 0x%08x)\n", result);
		goto freeThreads;
	}
	for (i = 0; i < (uint32_t)count; i++) {
		switch (operation) {
		case ARM_PMON_OP_RESET:
			result = armPmonResetAllCounters(threads[i]);
			if (result != 0)
				SCE_DBG_PRINTF(0, 110,
					"[Error] armPmonResetAllCounters (ret = 0x%08x)\n",
					result);
			break;
		case ARM_PMON_OP_SET_EVENT:
			result = pmonCpuSetEventSelf(threads[i], counter, (uint8_t)value);
			if (result != 0)
				SCE_DBG_PRINTF(0, 121,
					"[Error] sceKernelPMonThreadSetEvent (ret = 0x%08x)\n",
					result);
			break;
		case ARM_PMON_OP_START:
			result = SceThreadmgrForKernel_7F831213(threads[i], ARM_PMON_ENABLE_ALL);
			if (result != 0)
				SCE_DBG_PRINTF(0, 132,
					"[Error] sceKernelPMonThreadSetEnableCounter (ret = 0x%08x)\n",
					result);
			break;
		case ARM_PMON_OP_STOP:
			result = SceThreadmgrForKernel_1D2A6815(threads[i], ARM_PMON_ENABLE_ALL);
			if (result != 0)
				SCE_DBG_PRINTF(0, 143,
					"[Error] sceKernelPMonThreadClearEnableCounter (ret = 0x%08x)\n",
					result);
			break;
		case ARM_PMON_OP_SET_COUNTER:
			result = SceThreadmgrForKernel_7B3368F1(threads[i], counter, value);
			if (result != 0)
				SCE_DBG_PRINTF(0, 154,
					"[Error] sceKernelPMonThreadSetCounter (ret = 0x%08x)\n",
					result);
			break;
		default:
			SCE_DBG_PRINTF(0, 161,
				"[Error] armPmonExecAllThread : Unknown function code.\n");
			SCE_DBG_ASSERT(0, 162);
			continue;
		}
		if (result != 0)
			goto freeThreads;
	}
	result = 0;
freeThreads:
	ksceKernelFree(threads);
	return result;
}

int sceKernelPerfArmPmonReset(SceUID thread)
{
	int result;

	if (thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID || thread == -1) {
		result = armPmonExecAllThread(ARM_PMON_OP_RESET, 0, 0);
		if (result != 0)
			SCE_DBG_PRINTF(0, 264,
				"[Error] armPmonExecAllThread (ret = 0x%08x)\n", result);
	} else {
		result = armPmonResetAllCounters(thread);
		if (result != 0)
			SCE_DBG_PRINTF(0, 270,
				"[Error] armPmonResetAllCounters (ret = 0x%08x)\n", result);
	}
	return result;
}

int sceKernelPerfArmPmonSelectEvent(SceUID thread, unsigned int counter,
				  unsigned int event)
{
	int result;

	if (counter >= ARM_PMON_COUNTERS)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (SceSysrootForKernel_403B509E() <= 0x1f &&
	    (uint8_t)(event + 0x60) <= 5)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID || thread == -1) {
		result = armPmonExecAllThread(ARM_PMON_OP_SET_EVENT, counter, event);
		if (result != 0)
			SCE_DBG_PRINTF(0, 310,
				"[Error] armPmonExecAllThread (ret = 0x%08x)\n", result);
	} else {
		result = pmonCpuSetEventSelf(thread, counter, event);
		if (result != 0)
			SCE_DBG_PRINTF(0, 316,
				"[Error] sceKernelPMonThreadSetEvent (ret = 0x%08x)\n", result);
	}
	return result;
}

int sceKernelPerfArmPmonStart(SceUID thread)
{
	int result;

	if (thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID || thread == -1) {
		result = armPmonExecAllThread(ARM_PMON_OP_START, 0, 0);
		if (result != 0)
			SCE_DBG_PRINTF(0, 340,
				"[Error] armPmonExecAllThread (ret = 0x%08x)\n", result);
	} else {
		result = SceThreadmgrForKernel_7F831213(thread, ARM_PMON_ENABLE_ALL);
		if (result != 0)
			SCE_DBG_PRINTF(0, 346,
				"[Error] sceKernelPMonThreadSetEnableCounter (ret = 0x%08x)\n",
				result);
	}
	return result;
}

int sceKernelPerfArmPmonStop(SceUID thread)
{
	int result;

	if (thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID || thread == -1) {
		result = armPmonExecAllThread(ARM_PMON_OP_STOP, 0, 0);
		if (result != 0)
			SCE_DBG_PRINTF(0, 368,
				"[Error] armPmonExecAllThread (ret = 0x%08x)\n", result);
	} else {
		result = SceThreadmgrForKernel_1D2A6815(thread, ARM_PMON_ENABLE_ALL);
		if (result != 0)
			SCE_DBG_PRINTF(0, 374,
				"[Error] sceKernelPMonThreadClearEnableCounter (ret = 0x%08x)\n",
				result);
	}
	return result;
}

int sceKernelPerfArmPmonSetCounterValue(SceUID thread, unsigned int counter,
				      uint32_t value)
{
	int result;

	if (counter >= ARM_PMON_COUNTERS && counter != ARM_PMON_CYCLE_COUNTER)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (thread == SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID || thread == -1) {
		result = armPmonExecAllThread(ARM_PMON_OP_SET_COUNTER, counter, value);
		if (result != 0)
			SCE_DBG_PRINTF(0, 416,
				"[Error] armPmonExecAllThread (ret = 0x%08x)\n", result);
	} else {
		result = SceThreadmgrForKernel_7B3368F1(thread, counter, value);
		if (result != 0)
			SCE_DBG_PRINTF(0, 422,
				"[Error] sceKernelPMonThreadSetCounter (ret = 0x%08x)\n", result);
	}
	return result;
}

int sceKernelPerfArmPmonOpen(void)
{
	int result;

	result = SceProcessmgrForDriver_61B9B6FA(0, ARM_PMON_PMCR);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 180,
			"[Error] sceKernelSetInitialPMCR (ret = 0x%08x)\n", result);
		return result;
	}
	result = sceKernelPMonSetControlRegister(0, ARM_PMON_PMCR);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 186,
			"[Error] sceKernelPMonSetControlRegister (ret = 0x%08x)\n",
			result);
		return result;
	}
	result = SceProcessmgrForDriver_B1C3EFCA(0, 1);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 192,
			"[Error] sceKernelSetInitialPMUSERENR (ret = 0x%08x)\n",
			result);
		return result;
	}
	result = sceKernelPMonSetUserEnableRegister(0, 1);
	if (result != 0)
		SCE_DBG_PRINTF(0, 199,
			"[Error] sceKernelPMonSetUserEnableRegister (ret = 0x%08x)\n",
			result);
	return result;
}

int sceKernelPerfArmPmonClose(void)
{
	int result;

	result = SceProcessmgrForDriver_61B9B6FA(0, 0);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 221,
			"[Error] sceKernelSetInitialPMCR (ret = 0x%08x)\n", result);
		return result;
	}
	result = sceKernelPMonSetControlRegister(0, 0);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 227,
			"[Error] sceKernelPMonSetControlRegister (ret = 0x%08x)\n",
			result);
		return result;
	}
	result = SceProcessmgrForDriver_B1C3EFCA(0, 0);
	if (result != 0) {
		SCE_DBG_PRINTF(0, 233,
			"[Error] sceKernelSetInitialPMUSERENR (ret = 0x%08x)\n",
			result);
		return result;
	}
	result = sceKernelPMonSetUserEnableRegister(0, 0);
	if (result != 0)
		SCE_DBG_PRINTF(0, 240,
			"[Error] sceKernelPMonSetUserEnableRegister (ret = 0x%08x)\n",
			result);
	return result;
}

/* Inlined user-thread translation; special selectors pass through. */
static inline __attribute__((always_inline)) int translatePmonThread(SceUID *thread)
{
	if (*thread != 0 && *thread != SCE_KERNEL_THREAD_ID_PROCESS_ALL_ID && *thread != -1) {
		*thread = kscePUIDtoGUID(0, *thread);
		if (*thread < 0)
			return *thread | 0x40000000;
	}
	return 0;
}

int sceKernelPerfArmPmonOpenForUser(void)
{
	int permission = ksceKernelSetPermission(PAMGR_ACCESS_LEVEL);
	int result;

	if (permission < 0) {
		SCE_DBG_PRINTF(0, 510,
			"[Error]  sceKernelSetThreadAccessLevel(ret = 0x%08x)\n",
			permission);
		return permission;
	}
	result = sceKernelPerfArmPmonOpen();
	ksceKernelSetPermission(permission);
	return result;
}

int sceKernelPerfArmPmonCloseForUser(void)
{
	int permission = ksceKernelSetPermission(PAMGR_ACCESS_LEVEL);
	int result;

	if (permission < 0) {
		SCE_DBG_PRINTF(0, 533,
			"[Error]  sceKernelSetThreadAccessLevel(ret = 0x%08x)\n",
			permission);
		return permission;
	}
	result = sceKernelPerfArmPmonClose();
	ksceKernelSetPermission(permission);
	return result;
}

/* ForUser suffixes distinguish the user exports from the driver functions. */
int sceKernelPerfArmPmonResetForUser(SceUID thread)
{
	int result = translatePmonThread(&thread);

	if (result < 0)
		return result;
	return sceKernelPerfArmPmonReset(thread);
}

int sceKernelPerfArmPmonSelectEventForUser(SceUID thread, unsigned int counter,
					 unsigned int event)
{
	int result = translatePmonThread(&thread);

	if (result < 0)
		return result;
	return sceKernelPerfArmPmonSelectEvent(thread, counter, event);
}

int sceKernelPerfArmPmonStartForUser(SceUID thread)
{
	int result = translatePmonThread(&thread);

	if (result < 0)
		return result;
	return sceKernelPerfArmPmonStart(thread);
}

int sceKernelPerfArmPmonStopForUser(SceUID thread)
{
	int result = translatePmonThread(&thread);

	if (result < 0)
		return result;
	return sceKernelPerfArmPmonStop(thread);
}

int sceKernelPerfArmPmonSetCounterValueForUser(SceUID thread, unsigned int counter,
					     uint32_t value)
{
	int result = translatePmonThread(&thread);

	if (result < 0)
		return result;
	return sceKernelPerfArmPmonSetCounterValue(thread, counter, value);
}
