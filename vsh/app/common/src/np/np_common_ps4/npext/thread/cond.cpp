/*
 * SceNpCommonPs4: condition variables.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

int SceNpCommonPs4_E47C64E8(SceUID *pCond)
{
	return sceKernelWaitCond(*pCond, NULL);
}

__attribute__((noinline)) SceUID SceNpCommonPs4_34C15C21(SceUID *pCond, SceUID *pMutex, const char *name)
{
	SceUID result;

	result = sceKernelCreateCond(name, 0, *pMutex, (const SceKernelCondOptParam *)NULL);
	if (result >= 0)
		*pCond = result;
	return result;
}

__attribute__((noinline)) int SceNpCommonPs4_AC89B1E7(SceUID *pCond)
{
	if (pCond != NULL) {
		if (*pCond != 0) {
			sceKernelDeleteCond(*pCond);
			*pCond = 0;
		}
	}
	return 0;
}

__attribute__((noinline)) int SceNpCommonPs4_69E858EA(SceUID *pCond, SceUInt32 timeout)
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

__attribute__((noinline)) int SceNpCommonPs4_3D73D77E(SceUID *pCond)
{
	return sceKernelSignalCond(*pCond);
}

__attribute__((noinline)) int SceNpCommonPs4_62A77FA7(SceUID *pCond)
{
	return sceKernelSignalCondAll(*pCond);
}

extern const void *g_condVtable[];		/* 0x8102ab68 */

struct NpCommonPs4Cond *npCommonPs4CondConstruct(struct NpCommonPs4Cond *cond)
{
	cond->vtable = NP_COMMON_PS4_VTABLE(g_condVtable);
	cond->mutex = NULL;
	sceClibMemset(&cond->cond, 0, 4);
	return cond;
}

struct NpCommonPs4Cond *npCommonPs4CondDestruct(struct NpCommonPs4Cond *cond)
{
	if (cond->mutex != NULL) {
		cond->vtable = NP_COMMON_PS4_VTABLE(g_condVtable);
		SceNpCommonPs4_AC89B1E7(&cond->cond);
	}
	cond->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return cond;
}

int npCommonPs4CondDelete(struct NpCommonPs4Cond *cond)
{
	if (cond->mutex != NULL) {
		cond->vtable = NP_COMMON_PS4_VTABLE(g_condVtable);
		SceNpCommonPs4_AC89B1E7(&cond->cond);
	}
	cond->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(cond);
}

int npCommonPs4CondIsInit(struct NpCommonPs4Cond *cond)
{
	return cond->mutex != NULL;
}

int npCommonPs4CondInit(struct NpCommonPs4Cond *cond, const char *name, struct NpCommonPs4Mutex *mutex)
{
	static const char functionName[] = "Init";
	SceUID *pMutex;
	int result;

	if (cond->mutex != NULL)
		return NP_COMMON_PS4_ERROR_80559E03;
	pMutex = &mutex->mutexId;
	result = SceNpCommonPs4_34C15C21(&cond->cond, pMutex, name);
	if (result >= 0) {
		cond->mutex = pMutex;
		NP_COMMON_PS4_ASSERT(pMutex != NULL, functionName, "npext\\thread\\cond.cpp", 73, "IsInit()");
		return 0;
	}
	return result;
}

int npCommonPs4CondDestroy(struct NpCommonPs4Cond *cond)
{
	if (cond->mutex != NULL)
		SceNpCommonPs4_AC89B1E7(&cond->cond);
	sceClibMemset(&cond->cond, 0, 4);
	cond->mutex = NULL;
	return 0;
}

int npCommonPs4CondWait(struct NpCommonPs4Cond *cond, SceUInt32 timeout)
{
	int result;

	result = SceNpCommonPs4_69E858EA(&cond->cond, timeout);
	if (result >= 0)
		return 0;
	return result;
}

int npCommonPs4CondSignal(struct NpCommonPs4Cond *cond)
{
	int result;

	result = SceNpCommonPs4_3D73D77E(&cond->cond);
	if (result >= 0)
		return 0;
	return result;
}

int npCommonPs4CondBroadcast(struct NpCommonPs4Cond *cond)
{
	int result;

	result = SceNpCommonPs4_62A77FA7(&cond->cond);
	if (result >= 0)
		return 0;
	return result;
}

const void *g_condVtable[] = {			/* 0x8102ab68 */
	NULL,
	NULL,
	(const void *)npCommonPs4CondDestruct,
	(const void *)npCommonPs4CondDelete,
};

}
