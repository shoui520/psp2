/*
 * SceNpCommonPs4: event flags.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

SceUID SceNpCommonPs4_85B12451(SceUID *pEventFlag, const char *name, SceUInt32 attr, int reserved, SceUInt32 initPattern)
{
	SceUID result;

	result = sceKernelCreateEventFlag(name, attr, initPattern, NULL);
	if (result < 0)
		return result;
	*pEventFlag = result;
	return 0;
}

int SceNpCommonPs4_E42B3A09(SceUID eventFlag)
{
	int result;

	result = sceKernelDeleteEventFlag(eventFlag);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_BF4855F2(SceUID *pEventFlag, const char *name)
{
	SceUID result;

	result = sceKernelOpenEventFlag(name);
	if (result < 0)
		return result;
	*pEventFlag = result;
	return 0;
}

int SceNpCommonPs4_73E77BE0(SceUID eventFlag)
{
	int result;

	result = sceKernelCloseEventFlag(eventFlag);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_78905979(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
	SceUInt64 *pResult, SceUInt32 *pTimeout)
{
	SceUInt32 outBits;
	SceUInt32 timeout;
	int result;

	outBits = (SceUInt32)*pResult;
	timeout = *pTimeout;
	result = sceKernelWaitEventFlag(eventFlag, bitPattern, waitMode, &outBits, &timeout);
	if (result == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		result = NP_COMMON_PS4_ERROR_TIMEOUT;
	} else if (result >= 0) {
		result = 0;
		*pResult = outBits;
		*pTimeout = timeout;
	}
	return result;
}

int SceNpCommonPs4_2559AAB6(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
	SceUInt64 *pResult)
{
	SceUInt32 outBits;
	int result;

	outBits = (SceUInt32)*pResult;
	result = sceKernelPollEventFlag(eventFlag, bitPattern, waitMode, &outBits);
	if (result == SCE_KERNEL_ERROR_EVF_COND) {
		result = NP_COMMON_PS4_ERROR_8055800E;
	} else if (result >= 0) {
		*pResult = outBits;
		result = 0;
	}
	return result;
}

int SceNpCommonPs4_48C115EA(SceUID eventFlag, int reserved, SceUInt32 bitPattern)
{
	int result;

	result = sceKernelSetEventFlag(eventFlag, bitPattern);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_84EA4F2A(SceUID eventFlag, int reserved, SceUInt32 bitPattern)
{
	int result;

	result = sceKernelClearEventFlag(eventFlag, bitPattern);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_F2753B27(SceUID eventFlag, int reserved0, SceUInt32 setPattern, int reserved1, SceUInt32 *pNumWaitThreads)
{
	SceUInt32 numWaitThreads;
	int result;

	numWaitThreads = *pNumWaitThreads;
	result = sceKernelCancelEventFlag(eventFlag, setPattern, &numWaitThreads);
	if (result >= 0) {
		result = 0;
		*pNumWaitThreads = numWaitThreads;
	}
	return result;
}

}
