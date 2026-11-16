/*
 * SceNpCommerce2: the shell slots. A request that waits for the shell (the store's own screen)
 * gets one of sixteen slots; a slot holds the semaphore the request waits on and the shell's
 * request it waits for. The shell's events signal the slots of their kind.
 */

#include "np_commerce2_internal.h"

NpCommerce2ShellSlot g_shellSlots[NP_COMMERCE2_SHELL_SLOT_NUM];	/* 0x810270D8 */

int npCommerce2CreateShellSlot(int type, int *slotId)
{
	NpCommerce2ShellSlot *slot;
	int ret;
	int i;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		slot = g_shellSlots;
		for (i = 0; i < NP_COMMERCE2_SHELL_SLOT_NUM; i++) {
			if (slot->used == 0)
				break;
			slot++;
		}
		if (i == NP_COMMERCE2_SHELL_SLOT_NUM) {
			ret = NP_COMMERCE2_ERROR_NO_SHELL_SLOT;
		} else {
			slot = &g_shellSlots[i];
			sceClibMemset(slot, 0, sizeof(*slot));
			slot->type = type;
			ret = sceKernelCreateSema("SceNpCommerce2Ipc", 0, 0, 1, NULL);
			if (ret >= 0) {
				slot->semaphore = ret;
				slot->used = 1;
				*slotId = i + 1;
			}
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}

int npCommerce2DeleteShellSlot(int slotId)
{
	NpCommerce2ShellSlot *slot;
	int ret;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId <= 0 || slotId > NP_COMMERCE2_SHELL_SLOT_NUM)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		slot = &g_shellSlots[slotId - 1];
		if (slot->used == 1) {
			if (slot->semaphore >= 0) {
				sceKernelDeleteSema(slot->semaphore);
				slot->semaphore = -1;
			}
			slot->used = 0;
		} else {
			ret = NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT;
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}

int npCommerce2AbortShellSlot(int slotId)
{
	NpCommerce2ShellSlot *slot;
	int ret;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId <= 0 || slotId > NP_COMMERCE2_SHELL_SLOT_NUM)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		slot = &g_shellSlots[slotId - 1];
		if (slot->used == 1) {
			slot->aborted = 1;
			sceKernelDeleteSema(slot->semaphore);
			slot->semaphore = -1;
			if (slot->shellRequest > 0)
				ret = npCommerce2_8101EAE6(SceShellSvc_B31E7F1C(), slot->shellRequest);
		} else {
			ret = NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT;
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}

int npCommerce2WaitShellSlot(int slotId)
{
	NpCommerce2ShellSlot *slot;
	SceUID semaphore;
	int ret;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId <= 0 || slotId > NP_COMMERCE2_SHELL_SLOT_NUM)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		slot = &g_shellSlots[slotId - 1];
		if (slot->used != 1) {
			ret = NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT;
		} else if (slot->aborted == 1) {
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		} else {
			semaphore = slot->semaphore;
			sceKernelUnlockLwMutex(&g_shellLock, 1);
			return sceKernelWaitSema(semaphore, 1, NULL);
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}
