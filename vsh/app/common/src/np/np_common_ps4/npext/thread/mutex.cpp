/*
 * SceNpCommonPs4: mutexes and lightweight mutexes.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

SceUID SceNpCommonPs4_EDBFFFE9(SceUID *pMutex, const char *name, char recursive)
{
	SceUID result;

	if ((recursive & 1) != 0)
		result = sceKernelCreateMutex(name, 2, 0, NULL);
	else
		result = sceKernelCreateMutex(name, 0, 0, NULL);
	if (result < 0)
		return result;
	*pMutex = result;
	return 0;
}

int SceNpCommonPs4_2D74359F(SceUID *pMutex)
{
	return sceKernelDeleteMutex(*pMutex);
}

int SceNpCommonPs4_D37A4868(SceUID *pMutex)
{
	int result;

	result = sceKernelLockMutex(*pMutex, 1, NULL);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_950060DC(SceUID *pMutex)
{
	int result;

	result = sceKernelTryLockMutex(*pMutex, 1);
	if (result == SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN)
		return NP_COMMON_PS4_ERROR_BUSY;
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_262D1ACE(SceUID *pMutex)
{
	int result;

	result = sceKernelUnlockMutex(*pMutex, 1);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_4858EFB6(SceKernelLwMutexWork *pWork, const char *name, char recursive)
{
	int result;

	if ((recursive & 1) != 0)
		result = sceKernelCreateLwMutex(pWork, name, 2, 0, NULL);
	else
		result = sceKernelCreateLwMutex(pWork, name, 0, 0, NULL);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_29895EBB(SceKernelLwMutexWork *pWork)
{
	return sceKernelDeleteLwMutex(pWork);
}

int SceNpCommonPs4_F5840D6E(SceKernelLwMutexWork *pWork)
{
	int result;

	result = sceKernelLockLwMutex(pWork, 1, NULL);
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_9846B55A(SceKernelLwMutexWork *pWork)
{
	int result;

	result = sceKernelTryLockLwMutex(pWork, 1);
	if (result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN)
		return NP_COMMON_PS4_ERROR_BUSY;
	if (result < 0)
		return result;
	return 0;
}

int SceNpCommonPs4_4A1C1C3C(SceKernelLwMutexWork *pWork)
{
	int result;

	result = sceKernelUnlockLwMutex(pWork, 1);
	if (result < 0)
		return result;
	return 0;
}

}
