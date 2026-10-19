/* vip_process.c */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <dmacmgr/dmacmgr.h>
#include "../venezia_internal.h"
#include <vnz_wrapper/venezia_debug.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>

int lockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex);
int unlockVeneziaVipProcess(struct ThreadmgrFastMutex *mutex);

extern int waitVeneziaVipProcessReady(void);
extern VeneziaVipProcessStorage *_sceVipGetVipProcessResource(SceUID pid);

int g_veneziaVipProcessStorageKey = -1;

/* name unknown: allocate the VIP process-storage key and start its driver. */
int initializeVeneziaVipProcesses(void)
{
	int ret;

	ret = sceKernelRegisterKPLS("SceVip", sizeof(VeneziaVipProcessStorage));
	if (ret >= 0) {
		g_veneziaVipProcessStorageKey = ret;
		ret = initializeVeneziaVipResources();
		if (ret >= 0)
			ret = startVeneziaVipDriver(0);
	}
	return ret;
}

int _sceHwd0ProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	VeneziaVipProcessStorage *storage = NULL;
	char name[32];
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_veneziaVipProcessStorageKey,
		(void **)&storage, 1);
	if (ret >= 0) {
#if FW_DEBUG
		sceKernelAssert(storage != NULL,
			SCE_DBGINFO_FUNC(98, "_sceHwd0ProcessCreateEntry"), __builtin_return_address(0));
#endif
		memset(storage, 0, sizeof(*storage));
		snprintf(name, sizeof(name), "SceVipSuspend%08x", pid);
		ret = sceKernelCreateSema(name, 0x8000, 1, 1, NULL);
		if (ret >= 0) {
			storage->suspendSemaphore = ret;
			ret = initializeVeneziaVipProcessStorage(storage);
		}
	}
	return ret;
}

/* name unknown: cancel waiters and delete the process's VIP suspend semaphore. */
int releaseVeneziaVipProcessStorage(SceUID pid, SceUInt32 type)
{
	VeneziaVipProcessStorage *storage = NULL;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_veneziaVipProcessStorageKey,
		(void **)&storage, 0);
	if (ret >= 0) {
		ret = 0;
		if (storage && storage->suspendSemaphore >= 0) {
			sceKernelCancelSema(storage->suspendSemaphore, -1, NULL);
			sceKernelDeleteSema(storage->suspendSemaphore);
			storage->suspendSemaphore = -1;
		}
	}
	return ret;
}

VeneziaVipProcessStorage *_sceVipGetVipProcessResource(SceUID pid)
{
	VeneziaVipProcessStorage *storage = NULL;

	if (pid) {
		if (SceProcessmgrForDriver_AF80F39C(pid, g_veneziaVipProcessStorageKey,
			(void **)&storage, 0) < 0)
			storage = NULL;
	} else {
		storage = SceProcessmgrForDriver_EE694840(g_veneziaVipProcessStorageKey);
	}
#if FW_DEBUG
	sceKernelAssert(storage != NULL, SCE_DBGINFO_FUNC(170, "_sceVipGetVipProcessResource"),
		__builtin_return_address(0));
#endif
	return storage;
}

/* name unknown: wait until this process's VIP suspend semaphore is available. */
int waitVeneziaVipProcessReady(void)
{
	VeneziaVipProcessStorage *storage;
	int ret, signal;

	storage = SceProcessmgrForDriver_EE694840(g_veneziaVipProcessStorageKey);
#if FW_DEBUG
	sceKernelAssert(storage != NULL, SCE_DBGINFO_FUNC(170, "_sceVipGetVipProcessResource"),
		__builtin_return_address(0));
#endif
	if (!storage)
		return -2141060856;
	ret = sceKernelWaitSema(storage->suspendSemaphore, 1, NULL);
	if (ret >= 0) {
		signal = sceKernelSignalSema(storage->suspendSemaphore, 1);
		if (signal < 0)
			ret = signal;
	}
	return ret;
}

int _sceHwd0StartProcessSuspend(SceUID pid)
{
	VeneziaVipProcessStorage *storage = NULL;
	VeneziaVipProcessStorage *process;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_veneziaVipProcessStorageKey,
		(void **)&storage, 0);
	if (ret >= 0) {
#if FW_DEBUG
		sceKernelAssert(storage != NULL, SCE_DBGINFO_FUNC(225, "_sceHwd0StartProcessSuspend"),
			__builtin_return_address(0));
#endif
		process = storage;
		ret = sceKernelWaitSema(storage->suspendSemaphore, 1, NULL);
		if (ret >= 0) {
			lockVeneziaVipProcess(g_veneziaVipState.processLock);
			if (process->state & 0x100)
				ret = dispatchVeneziaVipProcessSuspend(process, 3);
		}
	}
	unlockVeneziaVipProcess(g_veneziaVipState.processLock);
	return ret;
}

/* name unknown: this firmware reports every Hwd process suspended. */
int isHwdProcessSuspended(SceUID pid)
{
	(void)pid;
	return 1;
}

int _sceHwd0ProcessResume(SceUID pid)
{
	VeneziaVipProcessStorage *storage = NULL;
	VeneziaVipProcessStorage *process;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_veneziaVipProcessStorageKey,
		(void **)&storage, 0);
	if (ret >= 0) {
#if FW_DEBUG
		sceKernelAssert(storage != NULL, SCE_DBGINFO_FUNC(281, "_sceHwd0ProcessResume"),
			__builtin_return_address(0));
#endif
		process = storage;
		lockVeneziaVipProcess(g_veneziaVipState.processLock);
		if (process->state & 0x100)
			ret = dispatchVeneziaVipProcessResume(process);
		else
			ret = dispatchVeneziaVipProcessReset(process);
		unlockVeneziaVipProcess(g_veneziaVipState.processLock);
		if (ret >= 0)
			ret = sceKernelSignalSema(process->suspendSemaphore, 1);
	}
	return ret;
}
