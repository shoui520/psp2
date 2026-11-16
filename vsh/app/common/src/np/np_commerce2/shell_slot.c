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
				ret = npCommerce2IpcOp1C((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), slot->shellRequest);
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

/* the first shell request of the slot's type, started with the request given; when the shell already has
 * one of that kind waiting for another slot, the slot's semaphore is signalled instead */
int npCommerce2StartShellSlot(int slotId, void *request)
{
	NpCommerce2ShellSlot *slot;
	NpCommerce2Ipc *shell;
	int ret;
	int result;
	int shellRequest;
	int i;
	int unused1;
	int unused2;
	int unused3;
	int unused4;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId <= 0 || slotId > NP_COMMERCE2_SHELL_SLOT_NUM)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		if (request == NULL) {
			ret = SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;
		} else if (g_shellSlots[slotId - 1].used != 1) {
			ret = NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT;
		} else {
			slot = &g_shellSlots[slotId - 1];
			if (slot->aborted == 1) {
				ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
			} else {
				switch (slot->type) {
				case 1: case 5: case 6: case 7: case 8: case 0xb: case 0xc:
					result = NP_COMMERCE2_ERROR_NO_SHELL_QUERY;
					goto answered;
				case 2:
					shell = (NpCommerce2Ipc *)SceShellSvc_B31E7F1C();
					ret = npCommerce2IpcOp08((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), &result, &unused1, NULL, 0, &unused2, NULL, 0,
								 &unused3, &unused4);
					if (ret >= 0)
						goto answered;
					break;
				case 3:
					shell = (NpCommerce2Ipc *)SceShellSvc_B31E7F1C();
					ret = npCommerce2IpcOp0B((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), &result, &unused1, NULL, 0, &unused2, NULL, 0,
								 &unused3, &unused4);
					if (ret >= 0)
						goto answered;
					break;
				case 4:
					shell = (NpCommerce2Ipc *)SceShellSvc_B31E7F1C();
					ret = npCommerce2IpcOp0D((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), &result, &unused1, NULL, 0, &unused2, NULL, 0,
								 &unused3, &unused4);
					if (ret >= 0)
						goto answered;
					break;
				case 9:
					shell = (NpCommerce2Ipc *)SceShellSvc_B31E7F1C();
					ret = npCommerce2IpcOp0F((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), &result, &unused1, NULL, 0, &unused2, NULL, 0,
								 &unused3, &unused4);
					if (ret >= 0)
						goto answered;
					break;
				case 0xa:
					shell = (NpCommerce2Ipc *)SceShellSvc_B31E7F1C();
					ret = npCommerce2IpcOp11((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), &result, &unused1, NULL, 0, &unused2, NULL, 0,
								 &unused3, &unused4);
					if (ret >= 0)
						goto answered;
					break;
				default:
answered:
					if (result < 0) {
						for (i = 0; i < NP_COMMERCE2_SHELL_SLOT_NUM; i++) {
							if (g_shellSlots[i].used == 1 && g_shellSlots[i].waiting == 1 &&
							    g_shellSlots[i].notified == 0)
								break;
						}
						if (i == NP_COMMERCE2_SHELL_SLOT_NUM) {
							switch (slot->type) {
							case 1: ret = npCommerce2IpcOp04((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 2: ret = npCommerce2IpcOp05((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 0xb: ret = npCommerce2IpcOp06((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 3: ret = npCommerce2IpcOp0A((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 4: ret = npCommerce2IpcOp0C((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 9: ret = npCommerce2IpcOp0E((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 0xa: ret = npCommerce2IpcOp10((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 5: ret = npCommerce2IpcOp12((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 6: ret = npCommerce2IpcOp14((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 7: ret = npCommerce2IpcOp16((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 0xc: ret = npCommerce2IpcOp18((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							case 8: ret = npCommerce2IpcOp1A((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), request); break;
							default: goto started;
							}
							if (ret < 0)
								break;
started:
							slot->shellRequest = ret;
							ret = 0;
							slot->waiting = 1;
						}
					} else {
						sceKernelSignalSema(slot->semaphore, 1);
					}
					break;
				}
			}
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}

/* gives the buffers of the shell request of the slot's type the results of the operation */
int npCommerce2GetShellSlotResult(int slotId, int *a2, int *a3, int *a4, int a5, int *a6, int *a7, int a8, int *a9)
{
	NpCommerce2ShellSlot *slot;
	NpCommerce2Ipc *shell;
	int unused;
	int ret;

	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	if (slotId <= 0 || slotId > NP_COMMERCE2_SHELL_SLOT_NUM || a2 == NULL || a3 == NULL || a6 == NULL)
		return SCE_NP_COMMERCE2_ERROR_INVALID_ARGUMENT;

	ret = sceKernelLockLwMutex(&g_shellLock, 1, NULL);
	if (ret >= 0) {
		slot = &g_shellSlots[slotId - 1];
		if (slot->used != 1) {
			ret = NP_COMMERCE2_ERROR_NO_SUCH_SHELL_SLOT;
		} else if (slot->aborted == 1) {
			ret = SCE_NP_COMMERCE2_ERROR_ABORTED;
		} else {
			switch (slot->type) {
			case 1: ret = npCommerce2IpcOp07((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 2: ret = npCommerce2IpcOp08((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 0xb: ret = npCommerce2IpcOp09((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 3: ret = npCommerce2IpcOp0B((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 4: ret = npCommerce2IpcOp0D((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 9: ret = npCommerce2IpcOp0F((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 0xa: ret = npCommerce2IpcOp11((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 5: ret = npCommerce2IpcOp13((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 6: ret = npCommerce2IpcOp15((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 7: ret = npCommerce2IpcOp17((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 0xc: ret = npCommerce2IpcOp19((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a3, a4, a5, a6, a7, a8, &unused, a9); break;
			case 8: ret = npCommerce2IpcOp1B((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), a2, a4, a5, a6); break;
			default: break;
			}
		}
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return ret;
}

/* the shell's event of a kind of request: signals the slots of that kind that wait for it, unless the
 * shell says it still has the request */
int npCommerce2NotifyShellSlots(int event, int enabled)
{
	NpCommerce2ShellSlot *slot;
	int type;
	int answer;
	int i;

	if (sceKernelLockLwMutex(&g_shellLock, 1, NULL) >= 0) {
		switch (event) {
		case 24: case 25: type = 1; break;
		case 26: case 27: type = 2; break;
		case 28: case 29: type = 11; break;
		case 30: case 31: type = 3; break;
		case 32: case 33: type = 4; break;
		case 34: case 35: type = 9; break;
		case 36: case 37: type = 10; break;
		case 38: case 39: type = 5; break;
		case 40: case 41: type = 6; break;
		case 42: case 43: type = 7; break;
		case 44: case 45: type = 12; break;
		case 46: case 47: type = 8; break;
		default: goto done;
		}
		slot = g_shellSlots;
		for (i = 0; i < NP_COMMERCE2_SHELL_SLOT_NUM; i++, slot++) {
			if (slot->used == 1 && slot->type == type && slot->semaphore >= 0 && slot->waiting == 1 &&
			    slot->notified == 0) {
				answer = 0;
				if (npCommerce2IpcOp1D((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), slot->shellRequest,
						       &answer) < 0 || answer != 1) {
					sceKernelSignalSema(slot->semaphore, 1);
					slot->notified = 1;
				}
			}
		}
done:
		sceKernelUnlockLwMutex(&g_shellLock, 1);
	}
	return 0;
}

int npCommerce2ShellEnabled(int event)
{
	return npCommerce2NotifyShellSlots(event, 1);
}

int npCommerce2ShellDisabled(int event)
{
	return npCommerce2NotifyShellSlots(event, 0);
}

int npCommerce2ShellOp00(void)
{
	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2IpcOp00((NpCommerce2Ipc *)SceShellSvc_B31E7F1C());
}

int npCommerce2ShellOp01(int a1)
{
	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2IpcOp01((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), (void *)a1);
}

int npCommerce2ShellOp02(int mode)
{
	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2IpcOp02((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), mode);
}

int npCommerce2ShellOp03(int slot, int *counter)
{
	if (g_shellReady == 0)
		return SCE_NP_COMMERCE2_ERROR_NOT_INITIALIZED;
	return npCommerce2IpcOp03((NpCommerce2Ipc *)SceShellSvc_B31E7F1C(), slot, counter);
}
