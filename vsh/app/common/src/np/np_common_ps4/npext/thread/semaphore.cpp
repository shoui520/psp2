/*
 * SceNpCommonPs4: semaphores.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

__attribute__((noinline)) SceUID SceNpCommonPs4_1C1B177A(SceUID *pSema, int initCount, int maxCount, const char *name)
{
	SceUID result;

	result = sceKernelCreateSema(name, 0, initCount, maxCount, NULL);
	if (result < 0)
		return result;
	*pSema = result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_908EDA3A(SceUID sema)
{
	int result;

	result = sceKernelDeleteSema(sema);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) SceUID SceNpCommonPs4_EB92EB4E(SceUID *pSema, const char *name)
{
	SceUID result;

	result = sceKernelOpenSema(name);
	if (result < 0)
		return result;
	*pSema = result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_7272B106(SceUID sema)
{
	int result;

	result = sceKernelCloseSema(sema);
	if (result < 0)
		return result;
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_2387BCED(SceUID sema, SceUInt32 *pTimeout)
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

__attribute__((noinline)) int SceNpCommonPs4_364483E2(SceUID sema)
{
	int result;

	result = sceKernelSignalSema(sema, 1);
	if (result < 0)
		return result;
	return 0;
}

extern const void *g_semaphoreVtable[];		/* 0x8102aa14 */

struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_90A8B2FA(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->vtable = NP_COMMON_PS4_VTABLE(g_semaphoreVtable);
	sema->state = 0;
	return sema;
}

struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_3C6221A8(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->vtable = NP_COMMON_PS4_VTABLE(g_semaphoreVtable);
	sema->state = 0;
	return sema;
}

__attribute__((noinline)) void SceNpCommonPs4_FFDF8860(struct NpCommonPs4SemaphoreObject *sema)
{
	if (sema->state > 1) {
		if (sema->state <= 2)
			SceNpCommonPs4_7272B106(sema->semaphore);
	} else if (sema->state != 0) {
		SceNpCommonPs4_908EDA3A(sema->semaphore);
	}
}

struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_CD40FE5E(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->vtable = NP_COMMON_PS4_VTABLE(g_semaphoreVtable);
	SceNpCommonPs4_FFDF8860(sema);
	sema->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return sema;
}

int SceNpCommonPs4_CA3B328B(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->vtable = NP_COMMON_PS4_VTABLE(g_semaphoreVtable);
	SceNpCommonPs4_FFDF8860(sema);
	sema->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(sema);
}

struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_15DDA799(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->vtable = NP_COMMON_PS4_VTABLE(g_semaphoreVtable);
	SceNpCommonPs4_FFDF8860(sema);
	sema->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return sema;
}

int SceNpCommonPs4_6BE664A1(struct NpCommonPs4SemaphoreObject *sema)
{
	return sema->state != 0;
}

int SceNpCommonPs4_47268A8C(struct NpCommonPs4SemaphoreObject *sema, int initCount, int maxCount, const char *name)
{
	int result;

	if (sema->state != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_1C1B177A(&sema->semaphore, initCount, maxCount, name);
	if (result >= 0) {
		sema->state = 1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_08B4BC67(struct NpCommonPs4SemaphoreObject *sema, const char *name)
{
	int result;

	if (sema->state != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_EB92EB4E(&sema->semaphore, name);
	if (result >= 0) {
		sema->state = 2;
		return 0;
	}
	return result;
}

struct NpCommonPs4SemaphoreObject *SceNpCommonPs4_144B99F3(struct NpCommonPs4SemaphoreObject *sema)
{
	sema->state = 0;
	return sema;
}

int SceNpCommonPs4_16A644A5(struct NpCommonPs4SemaphoreObject *sema)
{
	SceNpCommonPs4_FFDF8860(sema);
	sema->state = 0;
	return 0;
}

int SceNpCommonPs4_1D3964FF(struct NpCommonPs4SemaphoreObject *sema, SceUInt32 timeout)
{
	static const char functionName[] = "Wait";
	int result;

	NP_COMMON_PS4_ASSERT(sema->state != 0, functionName, "npext\\thread\\semaphore.cpp", 118, "IsInit()");
	if (timeout != 0)
		result = SceNpCommonPs4_2387BCED(sema->semaphore, &timeout);
	else
		result = SceNpCommonPs4_2387BCED(sema->semaphore, NULL);
	if (result >= 0)
		result = 0;
	return result;
}

int SceNpCommonPs4_B764816E(struct NpCommonPs4SemaphoreObject *sema)
{
	static const char functionName[] = "Signal";
	int result;

	NP_COMMON_PS4_ASSERT(sema->state != 0, functionName, "npext\\thread\\semaphore.cpp", 139, "IsInit()");
	result = SceNpCommonPs4_364483E2(sema->semaphore);
	if (result >= 0)
		return 0;
	return result;
}

const void *g_semaphoreVtable[] = {		/* 0x8102aa14 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_CD40FE5E,
	(const void *)SceNpCommonPs4_CA3B328B,
};

}
