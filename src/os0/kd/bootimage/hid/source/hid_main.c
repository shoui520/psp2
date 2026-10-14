/* Descriptive filename; this part has no source-file debug records. */
#include <sce/bt_hid.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2/power.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <sce/sysroot.h>
#include "hid_internal.h"

unsigned int hidFlags;
SceKernelSpinlock hidSpinlock;

int ksceKernelSpinlockLowLockCpuSuspendIntr(SceKernelSpinlock *lock);
void ksceKernelSpinlockLowUnlockCpuResumeIntr(SceKernelSpinlock *lock,
					    int intrState);

int hidBluetoothCallback(int notifyId, int notifyCount, int notifyArg,
			 void *userData);
int hidDisplayCallback(int notifyId, int notifyCount, int notifyArg,
		       void *userData);
int hidLock(void);
int hidUnlock(void);
void hidReleaseBluetoothDevices(void);

int hidBluetoothThread(SceSize args, void *argp)
{
	SceUID eventFlag;
	int result;
	unsigned int pattern;

	eventFlag = ksceKernelCreateEventFlag("SceHid", 0x1000, 0, NULL);
	if (eventFlag < 0) {
		ksceKernelPrintf("HID: sceKernelCreateEventFlag() failed! => 0x%x",
				 eventFlag);
		ksceKernelPrintf("!!\n");
		return eventFlag;
	}
	result = ksceKernelCreateCallback("SceBtHid", 0,
					 hidBluetoothCallback, NULL);
	if (result < 0)
		goto callbackError;
	result = ksceBtRegisterCallback(result, UINT64_C(0x1c60));
	if (result < 0)
		goto registerError;
	result = ksceKernelCreateCallback("SceBtHidDisp", 0,
					 hidDisplayCallback, NULL);
	if (result < 0)
		goto callbackError;
	result = scePowerRegisterCallback(result);
	if (result < 0)
		goto registerError;
	for (;;)
		ksceKernelWaitEventFlagCB(eventFlag, 1, 5, &pattern, NULL);

registerError:
	ksceKernelPrintf("HID: sceBtRegisterCallback() failed! => 0x%x", result);
	ksceKernelPrintf("!!\n");
	goto out;
callbackError:
	ksceKernelPrintf("HID: sceKernelCreateCallback(Bt) failed! => 0x%x", result);
	ksceKernelPrintf("!!\n");
out:
	ksceKernelDeleteEventFlag(eventFlag);
	return result;
}

int hidDisplayCallback(int notifyId, int notifyCount, int notifyArg,
		       void *userData)
{
	int result;
	int intrState;

	result = hidLock();
	if (result != 0)
		return result;
	if ((notifyArg & 0x400000) != 0) {
		intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x1000;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	} else if ((notifyArg & 0x800000) != 0) {
		intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags &= ~0x1000U;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	}
	hidUnlock();
	return 0;
}

int hidSystemEvent(int resume, int eventId, void *args, void *opt)
{
	int intrState;

	if (resume != 0) {
		if (resume == 1 && eventId == 0x100000) {
			intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
			hidFlags &= ~0x20U;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		}
		return 0;
	}
	switch (eventId) {
	case 0x402:
		return (int)hidFlags < 0 ? -2 : 0;
	case 0x4000:
		intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x20;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags &= ~0x10U;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		hidReleaseBluetoothDevices();
		return 0;
	case 0x200:
		intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x10;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		return 0;
	default:
		return 0;
	}
}

int hidBluetoothCallback(int notifyId, int notifyCount, int notifyArg,
			 void *userData)
{
	SceBtEvent event;
	union {
		uint32_t words[2];
		uint64_t value;
	} address;
	uint64_t timestamp = sceKernelGetSystemTimeWide();
	int result = hidLock();
	int intrState;

	if (result != 0)
		return result;
	if ((hidFlags & 0x10) != 0) {
		hidUnlock();
		return 0;
	}
	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags |= 0x80000000U;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
#define HID_EVENT_ADDRESS() \
	(address.words[0] = event.mac0, address.words[1] = event.mac1, address.value)
	for (;;) {
		result = ksceBtReadEvent(&event, 1);
		/* SCE_BT_ERROR_CB_OVERFLOW in the SDK. */
		if (result == (int)0x802f0c04)
			continue;
		if (result <= 0)
			break;
		switch (event.id) {
		case 5:
			if (event.status == 0)
				hidConnectBluetoothDevice(HID_EVENT_ADDRESS());
			break;
		case 6:
			hidDisconnectBluetoothDevice(HID_EVENT_ADDRESS());
			break;
		case 10:
			hidReceiveBluetoothReport(HID_EVENT_ADDRESS(), event.status,
				(SceBtHidRequest *)(uintptr_t)event.argument, timestamp);
			break;
		case 11:
			hidCompleteBluetoothWrite(HID_EVENT_ADDRESS(), event.status,
				(SceBtHidRequest *)(uintptr_t)event.argument);
			break;
		case 12:
			break;
		default:
			ksceKernelPrintf("HID: Invalid event type!! (%d)", event.id);
			ksceKernelPrintf("!!\n");
			break;
		}
	}
#undef HID_EVENT_ADDRESS
	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags &= ~0x80000000U;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	hidUnlock();
	return 0;
}

unsigned int hidGetFlags(unsigned int mask)
{
	return mask & hidFlags;
}

int module_start(SceSize args, const void *argp)
{
	int result;
	int intrState;

	hidInitializeMemory();
	hidInitializeReportLists();
	hidInitializeProcessList();
	hidFlags = 0;
	hidSpinlock = 0;
	result = ksceKernelCreateThread("SceHid", hidBluetoothThread, 60,
				       0x2000, 0, 0x80000, NULL);
	if (result < 0) {
		ksceKernelPrintf("HID: sceKernelCreateThread() failed! => 0x%x", result);
		goto error;
	}
	result = ksceKernelStartThread(result, args, (void *)argp);
	if (result < 0) {
		ksceKernelPrintf("HID: sceKernelStartThread() failed! => 0x%x", result);
		goto error;
	}
	result = ksceKernelRegisterSysEventHandler("SceHid", hidSystemEvent, NULL);
	if (result < 0) {
		ksceKernelPrintf("HID: sceKernelRegisterSysEventHandler() failed! => 0x%x",
				 result);
		goto error;
	}
	result = ksceKernelRegisterProcEventHandler("SceHid", &hidProcessHandlers, 0);
	if (result < 0) {
		ksceKernelPrintf("HID: sceKernelRegisterProcEventHandler() failed! => 0x%x",
				 result);
		goto error;
	}
	intrState = ksceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags |= 1;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	SceSysrootForDriver_34281746(&hidSysrootOps);
	return 0;
error:
	ksceKernelPrintf("!!\n");
	return 2;
}
