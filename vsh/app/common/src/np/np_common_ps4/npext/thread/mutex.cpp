/*
 * SceNpCommonPs4: mutexes and lightweight mutexes.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

__attribute__((noinline)) SceUID SceNpCommonPs4_EDBFFFE9(SceUID *pMutex, const char *name, char recursive)
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

__attribute__((noinline)) int SceNpCommonPs4_2D74359F(SceUID *pMutex)
{
	return sceKernelDeleteMutex(*pMutex);
}

__attribute__((noinline)) int SceNpCommonPs4_D37A4868(SceUID *pMutex)
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

__attribute__((noinline)) int SceNpCommonPs4_262D1ACE(SceUID *pMutex)
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

__attribute__((noinline)) int SceNpCommonPs4_BB5282C8(void *object)
{
	void *const *pool;

	if (object == NULL)
		return 0;
	pool = (void *const *)((char *)object - 16);
	return ((int (*)(void *, void *))((void *const *)*pool)[2])((char *)object - 16, ((void *const *)*pool)[3]);
}

struct NpCommonPs4Mutex *SceNpCommonPs4_4266D870(struct NpCommonPs4Mutex *mutex)
{
	mutex->vtable = g_mutexVtable;
	mutex->initialized = 0;
	sceClibMemset(&mutex->mutexId, 0, 4);
	return mutex;
}

struct NpCommonPs4Mutex *SceNpCommonPs4_4C7BD335(struct NpCommonPs4Mutex *mutex)
{
	mutex->vtable = g_mutexVtable;
	mutex->initialized = 0;
	sceClibMemset(&mutex->mutexId, 0, 4);
	return mutex;
}

int SceNpCommonPs4_AB8AE01C(struct NpCommonPs4Mutex *mutex)
{
	if (mutex->initialized != 0)
		return SceNpCommonPs4_2D74359F(&mutex->mutexId);
	return (int)mutex;
}

struct NpCommonPs4Mutex *SceNpCommonPs4_294F0B1A(struct NpCommonPs4Mutex *mutex)
{
	if (mutex->initialized != 0) {
		mutex->vtable = g_mutexVtable;
		SceNpCommonPs4_2D74359F(&mutex->mutexId);
	}
	mutex->vtable = g_baseVtable;
	return mutex;
}

int SceNpCommonPs4_E0ED3312(struct NpCommonPs4Mutex *mutex)
{
	if (mutex->initialized != 0) {
		mutex->vtable = g_mutexVtable;
		SceNpCommonPs4_2D74359F(&mutex->mutexId);
	}
	mutex->vtable = g_baseVtable;
	return SceNpCommonPs4_BB5282C8(mutex);
}

struct NpCommonPs4Mutex *SceNpCommonPs4_3FE4AAB6(struct NpCommonPs4Mutex *mutex)
{
	if (mutex->initialized != 0) {
		mutex->vtable = g_mutexVtable;
		SceNpCommonPs4_2D74359F(&mutex->mutexId);
	}
	mutex->vtable = g_baseVtable;
	return mutex;
}

int SceNpCommonPs4_C78F5DF5(struct NpCommonPs4Mutex *mutex)
{
	return mutex->initialized;
}

int SceNpCommonPs4_8AA7E995(struct NpCommonPs4Mutex *mutex, const char *name, char recursive)
{
	int result;

	if (mutex->initialized != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = SceNpCommonPs4_EDBFFFE9(&mutex->mutexId, name, (recursive & 1) ? 1 : 0);
	if (result >= 0) {
		mutex->initialized = 1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_6EA40010(struct NpCommonPs4Mutex *mutex)
{
	mutex->initialized = 0;
	return (int)mutex;
}

int SceNpCommonPs4_63EED478(struct NpCommonPs4Mutex *mutex)
{
	if (mutex->initialized != 0)
		SceNpCommonPs4_2D74359F(&mutex->mutexId);
	mutex->initialized = 0;
	return 0;
}

int SceNpCommonPs4_A2A8493B(struct NpCommonPs4Mutex *mutex)
{
	int result;
	int printed;

	result = SceNpCommonPs4_D37A4868(&mutex->mutexId);
	if (result != 0) {
		sceClibPrintf("Mutex lock failed. ret=%x\n", result);
		printed = sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "Lock", NP_COMMON_PS4_SOURCE("npext\\thread\\mutex.cpp"), 124, "HALT_ASSERT", "ret == 0");
		return SceNpCommonPs4_F50BD96A(printed);
	}
	return result;
}

int SceNpCommonPs4_6BA5A164(struct NpCommonPs4Mutex *mutex)
{
	int result;
	int printed;

	result = SceNpCommonPs4_262D1ACE(&mutex->mutexId);
	if (result != 0) {
		sceClibPrintf("Mutex unlock failed. ret=%x\n", result);
		printed = sceClibPrintf("Assertion in %s(%s:%d): %s(%s)\n", "Unlock", NP_COMMON_PS4_SOURCE("npext\\thread\\mutex.cpp"), 140, "HALT_ASSERT", "ret == 0");
		return SceNpCommonPs4_F50BD96A(printed);
	}
	return result;
}

const void *g_mutexVtable[] = {		/* 0x8102aa0c */
	(const void *)SceNpCommonPs4_294F0B1A,
	(const void *)SceNpCommonPs4_E0ED3312,
	NULL,
	NULL,
};

}
