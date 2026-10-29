/* Signal counters, lockup notifications and salvage registration. */
#include <stdint.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include "../../gpu_es4/srvkm/gpu-dump.h"
#include "../../gpu_es4/srvkm/lockup-history.h"
#include <sysmem/debug.h>
#include <sysmem/sblaimgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysclib.h>
#include <gpu/lockup.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/lockup/signal-lockup.c"

int signalMutex = -1;
int signalCond = -1;
unsigned signalCounter;
int lockupMutex = -1;
int lockupCond = -1;
int registerDumpMutex = -1;
int salvageUid = -1;
unsigned registerDumpSize;
int lockupEventUid = -1;
int lockupThreadUid = -1;
unsigned lockupThreadRunning;
unsigned gpuHalted;
unsigned lockupRecoveryCount;
GpuLockupBeginCallback lockupBeginCallback;
GpuLockupEndCallback lockupEndCallback;

/* Internal names are unknown; these names describe the recovered operations. */
int CreateSignalWaitObjects(void)
{
	int ret;

	ret = sceKernelCreateMutex("SceGpuSignalMutex", 0, 0, 0);
	signalMutex = ret;
	if (ret < 0) {
		signalMutex = -1;
		return ret;
	}
	ret = sceKernelCreateCond("SceGpuSignalCond", 0, signalMutex, 0);
	signalCond = ret;
	if (ret < 0) {
		sceKernelDeleteMutex(signalMutex);
		ret = signalCond;
		signalMutex = -1;
		signalCond = -1;
		return ret;
	}
	return 0;
}

int SignalWaitCounter(void)
{
	int ret;

	if (signalMutex < 0 || signalCond < 0)
		return (int)0x804c0007;
	ret = sceKernelLockMutex(signalMutex, 1, 0);
	if (ret < 0)
		return ret;
	signalCounter++;
	sceKernelUnlockMutex(signalMutex, 1);
	return sceKernelSignalCondAll(signalCond);
}

int WaitSignalCounter(unsigned *counter, unsigned timeout, int user)
{
	unsigned context, observed, *waitTimeout;
	int ret;

	if (signalMutex < 0 || signalCond < 0)
		return (int)0x804c0007;
	if (!counter)
		return (int)0x804c0000;
	context = sceSyscallEnter();
	ret = sceKernelLockMutex(signalMutex, 1, 0);
	if (ret >= 0) {
		if (user) {
			ret = sceKernelCopyFromUser(&observed, counter, 4);
			if (ret < 0)
				goto unlock;
		} else {
			observed = *counter;
		}
		if (observed == signalCounter) {
			waitTimeout = timeout ? &timeout : 0;
			ret = sceKernelWaitCond(signalCond, waitTimeout);
		}
		if (ret == 0) {
			if (user)
				ret = sceKernelCopyToUser(counter, &signalCounter, 4);
			else
				*counter = signalCounter;
		}
	unlock:
		sceKernelUnlockMutex(signalMutex, 1);
	}
	sceSyscallLeave(context);
	return ret;
}

int sceGpuSignalWait(unsigned *counter, unsigned timeout)
{
	return WaitSignalCounter(counter, timeout, 1);
}

int WaitSignalCounterKernel(unsigned *counter, unsigned timeout)
{
	return WaitSignalCounter(counter, timeout, 0);
}

int sceGpuSignalWaitLockup(void)
{
	unsigned context, timeout = 100000;
	int ret;

	if (lockupMutex < 0 || lockupCond < 0)
		return (int)0x804c0007;
	context = sceSyscallEnter();
	ret = sceKernelLockMutex(lockupMutex, 1, 0);
	if (ret >= 0) {
		ret = sceKernelWaitCond(lockupCond, &timeout);
		sceKernelUnlockMutex(lockupMutex, 1);
	}
	sceSyscallLeave(context);
	return ret;
}

static int LockRegisterDump(void)
{
	unsigned timeout;

	if (registerDumpMutex < 0)
		return (int)0x804c0007;
	timeout = 100000;
	return sceKernelLockMutex(registerDumpMutex, 1, &timeout);
}

static int UnlockRegisterDump(void)
{
	if (registerDumpMutex < 0)
		return (int)0x804c0007;
	return sceKernelUnlockMutex(registerDumpMutex, 1);
}

unsigned GetRegisterDumpAllocationSize(void);
unsigned StoreRegisterDump(const void *data);
int LockupThread(SceSize args, void *argp);

int CreateLockupWaitObjects(void)
{
	unsigned dumpSize;
	int ret;

	dumpSize = GetRegisterDumpAllocationSize();
	registerDumpSize = dumpSize;
	salvageUid = -1;
	ret = StoreRegisterDump(NULL);
	if (ret < 0)
		return ret;
	ret = sceKernelCreateMutex("SceGpuLockupMutex", 0, 0, NULL);
	lockupMutex = ret;
	if (ret < 0) {
		lockupMutex = -1;
		return ret;
	}
	ret = sceKernelCreateCond("SceGpuLockupCond", 0, lockupMutex, NULL);
	lockupCond = ret;
	if (ret < 0)
		goto deleteCond;
	ret = sceKernelCreateEventFlag("SceGpuLockupEventFlag", 0, 0, NULL);
	lockupEventUid = ret;
	if (ret < 0)
		goto deleteLockupMutex;
	ret = sceKernelCreateMutex("SceGpuLockupMutex2", 0, 0, NULL);
	registerDumpMutex = ret;
	if (ret < 0)
		goto deleteRegisterMutex;
	ret = sceKernelCreateThread("SceGpuLockupThread", LockupThread, 20,
			((dumpSize + 0x1fff) & ~0x1fffU) + 0x2000, 0, 0x80000, NULL);
	lockupThreadUid = ret;
	if (ret < 0)
		goto deleteEvent;
	lockupThreadRunning = 1;
	ret = sceKernelStartThread(lockupThreadUid, 0, NULL);
	if (ret >= 0)
		return 0;
	sceKernelDeleteThread(lockupThreadUid);
	lockupThreadUid = -1;
deleteEvent:
	sceKernelDeleteEventFlag(lockupEventUid);
	lockupEventUid = -1;
deleteRegisterMutex:
	sceKernelDeleteMutex(registerDumpMutex);
	registerDumpMutex = -1;
deleteLockupMutex:
	sceKernelDeleteMutex(lockupMutex);
	lockupMutex = -1;
deleteCond:
	sceKernelDeleteCond(lockupCond);
	lockupCond = -1;
	lockupThreadRunning = 0;
	return ret;
}

SceUID GetLockupEventUid(void)
{
	return lockupEventUid;
}

int sceGpuRegisterSalvage(int eventUid)
{
	unsigned context;
	int ret;

	if (!lockupThreadRunning)
		return (int)0x804c000c;
	if (eventUid == -1)
		return (int)0x804c0000;
	context = sceSyscallEnter();
	ret = LockRegisterDump();
	if (ret == 0) {
		if (salvageUid == -1)
			salvageUid = eventUid;
		else
			ret = (int)0x804c000e;
	}
	UnlockRegisterDump();
	sceSyscallLeave(context);
	return ret;
}

int sceGpuUnregisterSalvage(int eventUid)
{
	unsigned context;
	int ret;

	if (!lockupThreadRunning)
		return (int)0x804c000c;
	if (eventUid == -1)
		return (int)0x804c0000;
	context = sceSyscallEnter();
	ret = LockRegisterDump();
	if (ret == 0) {
		if (salvageUid == eventUid)
			salvageUid = -1;
		else
			ret = (int)0x804c0000;
	}
	UnlockRegisterDump();
	sceSyscallLeave(context);
	return ret;
}

int SceGpuEs4ForDriver_14ECCFCD(void *destination, unsigned size)
{
	const SceGpuRegisterDump *dump;
	int ret;

	if (!destination)
		return (int)0x804c0000;
	memset(destination, 0, size);
	ret = LockRegisterDump();
	if (ret == 0) {
		dump = GetRegisterDumpBuffer();
		if (!dump || size != registerDumpSize)
			ret = (int)0x804c0000;
		else
			memcpy(destination, dump, size);
	}
	UnlockRegisterDump();
	return ret;
}

/* Native LockupThread calls begin() before collecting the dump and end(token)
 * after analysis when recovery is not required. These are not IRQ callbacks.
 */
void SceGpuEs4ForDriver_26B03DCE(GpuLockupBeginCallback begin,
			       GpuLockupEndCallback end)
{
	lockupBeginCallback = begin;
	lockupEndCallback = end;
}

unsigned SceGpuEs4ForDriver_F001AE90(void)
{
	return registerDumpSize;
}

unsigned SceGpuEs4ForDriver_18C90B42(void)
{
	return lockupRecoveryCount;
}

PVRSRV_DEVICE_NODE *GetGpuDeviceNode(void);
PVRSRV_ERROR OSDisableTimer(IMG_HANDLE hTimer);
PVRSRV_ERROR OSEnableTimer(IMG_HANDLE hTimer);
int sceGpuRegisterDumpCollect(SceGpuRegisterDump *dump, unsigned size,
		unsigned flags);
int sceGpuCollectFrameNumSceneIndexInfo(SceGpuRegisterDump *dump);
int CollectRegisterDumpContexts(SceGpuRegisterDump *dump);
int sceGpuRegisterDumpAnalysis(const SceGpuRegisterDump *dump,
		unsigned *analysis, int *pid, unsigned *detail);
IMG_VOID HWRecoveryResetSGX(PVRSRV_DEVICE_NODE *psDeviceNode,
		unsigned callerId, int lockupId);
void RecordGpuLockup(SceGpuRegisterDump *dump, unsigned reason,
		SceUID processUid, unsigned analysisDetail);

int LockupThread(SceSize args, void *argp)
{
	SceGpuRegisterDump *dump = __builtin_alloca(registerDumpSize);
	struct GpuLockupHistory *history = __builtin_alloca(sizeof(*history));
	struct GpuLockupHistory *ordered = __builtin_alloca(sizeof(*ordered));
	PVRSRV_DEVICE_NODE *node;
	PVRSRV_SGXDEV_INFO *device;
	unsigned eventPattern, token, analysis, detail;
	int processUid, recovered, salvage, result;

	lockupHistory = history;
	orderedLockupHistory = ordered;
	memset(history, 0, sizeof(*history));
	memset(ordered, 0, sizeof(*ordered));
	history->processCount = 0;
	history->size = sizeof(*history);
	history->identifier = 101;
	ordered->identifier = 101;
	ordered->size = sizeof(*ordered);
	while (lockupThreadRunning) {
		eventPattern = 0;
		result = sceKernelWaitEventFlag(lockupEventUid, 3,
				SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				&eventPattern, NULL);
		if (!lockupThreadRunning)
			break;
		if (result)
			continue;
		node = GetGpuDeviceNode();
		device = node ? node->pvDevice : NULL;
		OSDisableTimer(device->hTimer);
		token = 0;
		if (lockupBeginCallback)
			token = lockupBeginCallback();
		result = sceGpuRegisterDumpCollect(dump, registerDumpSize, 7);
		if (result)
			SCE_DBG_PRINTF(0, 151,
					"sceGpuRegisterDumpCollect returned 0x%08x\n", result);
		result = sceGpuCollectFrameNumSceneIndexInfo(dump);
		if (result)
			SCE_DBG_PRINTF(0, 159,
					"sceGpuCollectFrameNumSceneIndexInfo returned 0x%08x\n", result);
		if (!LockRegisterDump()) {
			result = StoreRegisterDump(dump);
			if (result)
				SCE_DBG_PRINTF(0, 175,
						"sceGpuRegisterDumpSave returned 0x%08x\n", result);
			UnlockRegisterDump();
		}
		CollectRegisterDumpContexts(dump);
		if (!LockRegisterDump()) {
			result = StoreRegisterDump(dump);
			if (result)
				SCE_DBG_PRINTF(0, 190,
						"sceGpuRegisterDumpSave returned 0x%08x\n", result);
			UnlockRegisterDump();
		}
		result = sceGpuRegisterDumpAnalysis(dump, &analysis, &processUid, &detail);
		if (result < 0) {
			SCE_DBG_PRINTF(0, 204,
					"sceGpuRegisterDumpAnalysis returned 0x%08x\n", result);
			analysis = 0;
		}
		recovered = 0;
		if (analysis > 2 && analysis <= 4) {
			if (SceSblAIMgrForDriver_274663A0() && SceSblACMgrForDriver_E87D1777()) {
				if (lockupEndCallback)
					lockupEndCallback(token);
				SceDebugForDriver_391B74B7("GPU coredump analysis: GPU may have been halted by a known clipper hardware bug.\n");
				SceDebugForDriver_391B74B7("GPU coredump analysis: Please refer to https://psvita.scedev.net/technotes/view/274\n");
			} else {
				HWRecoveryResetSGX(node, 0, -1);
				RecordGpuLockup(dump, analysis, processUid, detail);
				lockupRecoveryCount++;
				recovered = 1;
			}
		} else if (lockupEndCallback) {
			lockupEndCallback(token);
		}
		if (!LockRegisterDump()) {
			salvage = salvageUid;
			UnlockRegisterDump();
			if (salvage != -1) {
				sceKernelSignalCondAll(lockupCond);
				break;
			}
		}
		if (recovered) {
			OSEnableTimer(device->hTimer);
		} else {
			gpuHalted = 1;
			sceKernelSignalCondAll(lockupCond);
			threadmgrGpuExceptionHandler(0x10005);
		}
	}
	sceKernelExitDeleteThread(0);
	return 0;
}

int sceGpuIsHalted(void)
{
	return gpuHalted;
}
