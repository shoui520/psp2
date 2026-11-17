/*
 * SceNpCommonPs4: threads.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_04EC8796(SceUID *pThread, SceKernelThreadEntry entry, SceSize argSize, void *pArgp,
	int initPriority, SceSize stackSize, int cpuAffinity, const char *name)
{
	SceUInt32 affinityMask;
	SceUID thread;
	int result;

	affinityMask = 0;
	if (cpuAffinity == 255)
		affinityMask = 0x70000;
	thread = sceKernelCreateThread(name, entry, initPriority, stackSize, 0, affinityMask, NULL);
	result = thread;
	if (thread >= 0) {
		*pThread = thread;
		result = sceKernelStartThread(thread, argSize, pArgp);
		if (result < 0)
			sceKernelDeleteThread(*pThread);
	}
	return result;
}

int SceNpCommonPs4_CFB92A12(SceUID thread, int *pExitStatus)
{
	int result;

	result = sceKernelWaitThreadEnd(thread, pExitStatus, NULL);
	if (result >= 0) {
		result = sceKernelDeleteThread(thread);
		if (result >= 0)
			return 0;
	}
	return result;
}

int SceNpCommonPs4_5395A6B1(SceUID *pThreadId)
{
	int result;

	if (pThreadId == NULL)
		return NP_COMMON_PS4_ERROR_80558009;
	result = sceKernelGetThreadId();
	if (result < 0)
		return result;
	*pThreadId = result;
	return 0;
}

}
