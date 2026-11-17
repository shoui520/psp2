/*
 * SceNpCommonPs4: condition variables.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_E47C64E8(SceUID *pCond)
{
	return sceKernelWaitCond(*pCond, NULL);
}

SceUID SceNpCommonPs4_34C15C21(SceUID *pCond, SceUID *pMutex, const char *name)
{
	SceUID result;

	result = sceKernelCreateCond(name, 0, *pMutex, (const SceKernelCondOptParam *)NULL);
	if (result >= 0)
		*pCond = result;
	return result;
}

int SceNpCommonPs4_AC89B1E7(SceUID *pCond)
{
	if (pCond != NULL) {
		if (*pCond != 0) {
			sceKernelDeleteCond(*pCond);
			*pCond = 0;
		}
	}
	return 0;
}

int SceNpCommonPs4_69E858EA(SceUID *pCond, SceUInt32 timeout)
{
	SceUInt32 waitTime;
	int result;

	if (timeout != 0) {
		waitTime = timeout;
		result = sceKernelWaitCond(*pCond, &waitTime);
		if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			result = NP_COMMON_PS4_ERROR_TIMEOUT;
		else if (result >= 0)
			result = 0;
	} else {
		result = sceKernelWaitCond(*pCond, NULL);
	}
	return result;
}

int SceNpCommonPs4_3D73D77E(SceUID *pCond)
{
	return sceKernelSignalCond(*pCond);
}

int SceNpCommonPs4_62A77FA7(SceUID *pCond)
{
	return sceKernelSignalCondAll(*pCond);
}

}
