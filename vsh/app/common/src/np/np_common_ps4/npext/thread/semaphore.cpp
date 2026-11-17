/*
 * SceNpCommonPs4: semaphores.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

SceUID SceNpCommonPs4_1C1B177A(SceUID *pSema, int initCount, int maxCount, const char *name)
{
	SceUID result;

	result = sceKernelCreateSema(name, 0, initCount, maxCount, NULL);
	if (result < 0)
		return result;
	*pSema = result;
	return 0;
}

int SceNpCommonPs4_908EDA3A(SceUID sema)
{
	int result;

	result = sceKernelDeleteSema(sema);
	if (result < 0)
		return result;
	return 0;
}

SceUID SceNpCommonPs4_EB92EB4E(SceUID *pSema, const char *name)
{
	SceUID result;

	result = sceKernelOpenSema(name);
	if (result < 0)
		return result;
	*pSema = result;
	return 0;
}

int SceNpCommonPs4_7272B106(SceUID sema)
{
	int result;

	result = sceKernelCloseSema(sema);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_2387BCED(SceUID sema, SceUInt32 *pTimeout)
{
	SceUInt32 timeout;
	int result;

	timeout = *pTimeout;
	result = sceKernelWaitSema(sema, 1, &timeout);
	if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		result = NP_COMMON_PS4_ERROR_TIMEOUT;
	} else if (result >= 0) {
		result = 0;
		*pTimeout = timeout;
	}
	return result;
}

int SceNpCommonPs4_508877F1(SceUID sema)
{
	int result;

	result = sceKernelPollSema(sema, 1);
	if (result == SCE_KERNEL_ERROR_SEMA_ZERO) {
		return NP_COMMON_PS4_ERROR_8055800D;
	}
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_364483E2(SceUID sema)
{
	int result;

	result = sceKernelSignalSema(sema, 1);
	if (result < 0)
		return result;
	return 0;
}

}
