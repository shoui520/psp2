/*
 * The per-process slots: one UlobjProcCB for each process that started
 * supporting user-level objects, all guarded by one spinlock.
 */
#include <string.h>
#include <sysmem/cpu.h>
#include "ulobjmgr_internal.h"
#include <sysmem/barrier.h>

UlobjInstance g_ulobjInstance;

static inline void dataMemoryBarrier(void)
{
	dmb();
}

static inline UlobjProcCB *searchUlobjProcCB(SceUID pid)
{
	int i;

	for (i = 0; i < ULOBJ_PROC_CB_COUNT; i++)
		if (g_ulobjInstance.procCB[i].pid == pid)
			return &g_ulobjInstance.procCB[i];
	return NULL;
}

int initializeUlobjProcCB(void)
{
	int i, ret;

	memset(&g_ulobjInstance, 0, sizeof(g_ulobjInstance));
	for (i = 0; i < ULOBJ_PROC_CB_COUNT; i++) {
		UlobjProcCB *procCB = &g_ulobjInstance.procCB[i];

		SCE_DBG_ASSERT(i < ULOBJ_PROC_CB_COUNT, 85);
		procCB->pid = -1;
		procCB->driverMemBlock = -1;
		procCB->userMemBlock = -1;
	}
	ret = registerUlobjProcessEvents();
	if (ret < 0)
		return ret;
	dataMemoryBarrier();
	return 0;
}

static inline __attribute__((always_inline))
UlobjProcCB *getAndLockUlobjInfoArrayImpl(SceUID pid, SceKernelIntrStatus *pLockState)
{
	UlobjProcCB *procCB;

	SCE_DBG_ASSERT(pLockState != NULL, 117);
	*pLockState = sceKernelSpinlockLowLockCpuSuspendIntr(&g_ulobjInstance.lock);
	procCB = searchUlobjProcCB(pid);
	if (procCB == NULL)
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ulobjInstance.lock, *pLockState);
	return procCB;
}

UlobjProcCB *getAndLockUlobjInfoArray(SceUID pid, SceKernelIntrStatus *pLockState)
{
	SCE_DBG_ASSERT(pLockState != NULL, 246);
	return getAndLockUlobjInfoArrayImpl(pid, pLockState);
}

UlobjProcCB *getAndLockUlobjInfoArrayForKernel(SceUID pid)
{
	UlobjProcCB *procCB;

	sceKernelSpinlockLowLock(&g_ulobjInstance.lock);
	procCB = searchUlobjProcCB(pid);
	if (procCB == NULL)
		sceKernelSpinlockLowUnlock(&g_ulobjInstance.lock);
	return procCB;
}

static inline __attribute__((always_inline))
int allocateAndLockUlobjInfoArrayImpl(SceUID pid, UlobjProcCB **pProcCB, SceKernelIntrStatus *pLockState)
{
	UlobjProcCB *procCB;

	SCE_DBG_ASSERT(pProcCB != NULL, 151);
	SCE_DBG_ASSERT(pLockState != NULL, 152);
	*pLockState = sceKernelSpinlockLowLockCpuSuspendIntr(&g_ulobjInstance.lock);
	if (searchUlobjProcCB(pid) != NULL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ulobjInstance.lock, *pLockState);
		return SCE_KERNEL_ERROR_ALREADY_REGISTERED;
	}
	/* no check for a full table: Sony stores through NULL then */
	procCB = searchUlobjProcCB(-1);
	procCB->pid = pid;
	dataMemoryBarrier();
	*pProcCB = procCB;
	return 0;
}

int allocateAndLockUlobjInfoArray(SceUID pid, UlobjProcCB **pProcCB, SceKernelIntrStatus *pLockState)
{
	SCE_DBG_ASSERT(pProcCB != NULL, 264);
	SCE_DBG_ASSERT(pLockState != NULL, 265);
	return allocateAndLockUlobjInfoArrayImpl(pid, pProcCB, pLockState);
}

void unlockUlobjInfoArrayImpl(UlobjProcCB *procCB, SceKernelIntrStatus lockState)
{
	SCE_DBG_ASSERT(procCB != NULL, 138);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ulobjInstance.lock, lockState);
}

void unlockUlobjInfoArrayForKernelImpl(UlobjProcCB *procCB)
{
	SCE_DBG_ASSERT(procCB != NULL, 229);
	sceKernelSpinlockLowUnlock(&g_ulobjInstance.lock);
}

void freeAndUnlockUlobjInfoArrayImpl(UlobjProcCB *procCB, SceKernelIntrStatus lockState)
{
	SCE_DBG_ASSERT(procCB != NULL, 181);
	dataMemoryBarrier();
	procCB->pid = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_ulobjInstance.lock, lockState);
}

void freeAndUnlockUlobjInfoArrayForKernelImpl(UlobjProcCB *procCB)
{
	SCE_DBG_ASSERT(procCB != NULL, 194);
	dataMemoryBarrier();
	procCB->pid = -1;
	sceKernelSpinlockLowUnlock(&g_ulobjInstance.lock);
}

/* name unknown: look the slot up without keeping the lock */
UlobjProcCB *findUlobjInfoArray(SceUID pid)
{
	UlobjProcCB *procCB;

	sceKernelSpinlockLowLock(&g_ulobjInstance.lock);
	procCB = searchUlobjProcCB(pid);
	sceKernelSpinlockLowUnlock(&g_ulobjInstance.lock);
	return procCB;
}
