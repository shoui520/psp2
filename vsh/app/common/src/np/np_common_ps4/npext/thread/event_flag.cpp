/*
 * SceNpCommonPs4: event flags.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

__attribute__((noinline)) SceUID SceNpCommonPs4_85B12451(SceUID *pEventFlag, const char *name, SceUInt32 attr, int reserved, SceUInt32 initPattern)
{
	SceUID result;

	result = sceKernelCreateEventFlag(name, attr, initPattern, NULL);
	if (result < 0)
		return result;
	*pEventFlag = result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_E42B3A09(SceUID eventFlag)
{
	int result;

	result = sceKernelDeleteEventFlag(eventFlag);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_BF4855F2(SceUID *pEventFlag, const char *name)
{
	SceUID result;

	result = sceKernelOpenEventFlag(name);
	if (result < 0)
		return result;
	*pEventFlag = result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_73E77BE0(SceUID eventFlag)
{
	int result;

	result = sceKernelCloseEventFlag(eventFlag);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_78905979(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
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

__attribute__((noinline)) int SceNpCommonPs4_2559AAB6(SceUID eventFlag, int reserved0, SceUInt32 bitPattern, int reserved1, SceUInt32 waitMode,
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

__attribute__((noinline)) int SceNpCommonPs4_48C115EA(SceUID eventFlag, int reserved, SceUInt32 bitPattern)
{
	int result;

	result = sceKernelSetEventFlag(eventFlag, bitPattern);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_84EA4F2A(SceUID eventFlag, int reserved, SceUInt32 bitPattern)
{
	int result;

	result = sceKernelClearEventFlag(eventFlag, bitPattern);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_F2753B27(SceUID eventFlag, int reserved0, SceUInt32 setPattern, int reserved1, SceUInt32 *pNumWaitThreads)
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

extern const void *g_eventFlagVtable[];		/* 0x8102a9e4 */

struct NpCommonPs4EventFlag *SceNpCommonPs4_945AC0FC(struct NpCommonPs4EventFlag *ef)
{
	ef->vtable = NP_COMMON_PS4_VTABLE(g_eventFlagVtable);
	ef->state = 0;
	return ef;
}

struct NpCommonPs4EventFlag *SceNpCommonPs4_DEB8B7CA(struct NpCommonPs4EventFlag *ef)
{
	ef->vtable = NP_COMMON_PS4_VTABLE(g_eventFlagVtable);
	ef->state = 0;
	return ef;
}

__attribute__((noinline)) void SceNpCommonPs4_9131ACDF(struct NpCommonPs4EventFlag *ef)
{
	if (ef->state > 1) {
		if (ef->state <= 2)
			SceNpCommonPs4_73E77BE0(ef->eventFlag);
	} else if (ef->state != 0) {
		SceNpCommonPs4_E42B3A09(ef->eventFlag);
	}
}

struct NpCommonPs4EventFlag *SceNpCommonPs4_4C3E58CA(struct NpCommonPs4EventFlag *ef)
{
	ef->vtable = NP_COMMON_PS4_VTABLE(g_eventFlagVtable);
	SceNpCommonPs4_9131ACDF(ef);
	ef->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ef;
}

int SceNpCommonPs4_B5EA1FBB(struct NpCommonPs4EventFlag *ef)
{
	ef->vtable = NP_COMMON_PS4_VTABLE(g_eventFlagVtable);
	SceNpCommonPs4_9131ACDF(ef);
	ef->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(ef);
}

struct NpCommonPs4EventFlag *SceNpCommonPs4_1F290BAE(struct NpCommonPs4EventFlag *ef)
{
	ef->vtable = NP_COMMON_PS4_VTABLE(g_eventFlagVtable);
	SceNpCommonPs4_9131ACDF(ef);
	ef->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return ef;
}

int SceNpCommonPs4_BA957ABE(struct NpCommonPs4EventFlag *ef, const char *name, SceUInt32 attr)
{
	int result;

	if (ef->state != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_85B12451(&ef->eventFlag, name, attr, 0, 0);
	if (result >= 0)
		ef->state = 1;
	return result;
}

int SceNpCommonPs4_3C9F833F(struct NpCommonPs4EventFlag *ef)
{
	return ef->state != 0;
}

int SceNpCommonPs4_909FBD24(struct NpCommonPs4EventFlag *ef, const char *name)
{
	int result;

	if (ef->state != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_BF4855F2(&ef->eventFlag, name);
	if (result >= 0)
		ef->state = 2;
	return result;
}

struct NpCommonPs4EventFlag *SceNpCommonPs4_82F5AC22(struct NpCommonPs4EventFlag *ef)
{
	ef->state = 0;
	return ef;
}

int SceNpCommonPs4_E7D29E99(struct NpCommonPs4EventFlag *ef)
{
	SceNpCommonPs4_9131ACDF(ef);
	ef->state = 0;
	return 0;
}

int SceNpCommonPs4_0F1ED1EB(struct NpCommonPs4EventFlag *ef, int reserved, SceUInt32 bitPattern)
{
	static const char functionName[] = "Set";

	NP_COMMON_PS4_ASSERT(ef->state != 0, functionName, "npext\\thread\\event_flag.cpp", 134, "IsInit()");
	return SceNpCommonPs4_48C115EA(ef->eventFlag, reserved, bitPattern);
}

int SceNpCommonPs4_D6F2A0EC(struct NpCommonPs4EventFlag *ef, int reserved0, SceUInt32 bitPattern, int reserved1,
	SceUInt32 waitMode, SceUInt64 *pResult)
{
	static const char functionName[] = "Poll";
	int result;

	NP_COMMON_PS4_ASSERT(ef->state != 0, functionName, "npext\\thread\\event_flag.cpp", 150, "IsInit()");
	result = SceNpCommonPs4_2559AAB6(ef->eventFlag, reserved0, bitPattern, reserved1, waitMode, pResult);
	if (result >= 0)
		return 0;
	return result;
}

int SceNpCommonPs4_6661735E(struct NpCommonPs4EventFlag *ef, int reserved0, SceUInt32 bitPattern, int reserved1,
	SceUInt32 waitMode, SceUInt64 *pResult, SceUInt32 timeout)
{
	static const char functionName[] = "Wait";

	NP_COMMON_PS4_ASSERT(ef->state != 0, functionName, "npext\\thread\\event_flag.cpp", 171, "IsInit()");
	if (timeout != 0)
		return SceNpCommonPs4_78905979(ef->eventFlag, (int)&timeout, bitPattern, reserved1, waitMode, pResult, &timeout);
	return SceNpCommonPs4_78905979(ef->eventFlag, 0, bitPattern, reserved1, waitMode, pResult, NULL);
}

int SceNpCommonPs4_8D8FD4D4(struct NpCommonPs4EventFlag *ef, int reserved, SceUInt32 bitPattern)
{
	static const char functionName[] = "Clear";

	NP_COMMON_PS4_ASSERT(ef->state != 0, functionName, "npext\\thread\\event_flag.cpp", 193, "IsInit()");
	return SceNpCommonPs4_84EA4F2A(ef->eventFlag, reserved, bitPattern);
}

int SceNpCommonPs4_64895237(struct NpCommonPs4EventFlag *ef, int reserved0, SceUInt32 setPattern, int reserved1)
{
	static const char functionName[] = "Cancel";
	SceUInt32 numWaitThreads;

	NP_COMMON_PS4_ASSERT(ef->state != 0, functionName, "npext\\thread\\event_flag.cpp", 205, "IsInit()");
	numWaitThreads = 0;
	return SceNpCommonPs4_F2753B27(ef->eventFlag, ef->eventFlag, setPattern, reserved1, &numWaitThreads);
}

const void *g_eventFlagVtable[] = {		/* 0x8102a9e4 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_4C3E58CA,
	(const void *)SceNpCommonPs4_B5EA1FBB,
};

}
