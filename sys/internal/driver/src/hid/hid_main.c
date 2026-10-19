/* Descriptive filename; this part has no source-file debug records. */
#include <bt/bt_hid.h>
#include <sysmem/debug.h>
#include <sysmem/cpu.h>
#include <sysmem/suspend.h>
#include <power/power.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include "hid_internal.h"

unsigned int hidFlags;
SceKernelSpinlock hidSpinlock;

int hidBluetoothCallback(int notifyId, int notifyCount, int notifyArg,
			 void *userData);
int hidDisplayCallback(int notifyId, int notifyCount, int notifyArg,
		       void *userData);

int hidBluetoothThread(SceSize args, void *argp)
{
	SceUID eventFlag;
	int result;
	unsigned int pattern;

	eventFlag = sceKernelCreateEventFlag("SceHid", 0x1000, 0, NULL);
	if (eventFlag < 0) {
		SceDebugForDriver_391B74B7("HID: sceKernelCreateEventFlag() failed! => 0x%x",
				 eventFlag);
		SceDebugForDriver_391B74B7("!!\n");
		return eventFlag;
	}
	result = sceKernelCreateCallback("SceBtHid", 0,
					 hidBluetoothCallback, NULL);
	if (result < 0)
		goto callbackError;
	result = SceBtForDriver_120AC1F7(result, UINT64_C(0x1c60));
	if (result < 0)
		goto registerError;
	result = sceKernelCreateCallback("SceBtHidDisp", 0,
					 hidDisplayCallback, NULL);
	if (result < 0)
		goto callbackError;
	result = scePowerRegisterCallback(result);
	if (result < 0)
		goto registerError;
	for (;;)
		SceThreadmgrForDriver_8A35F714(eventFlag, 1, 5, &pattern, NULL);

registerError:
	SceDebugForDriver_391B74B7("HID: sceBtRegisterCallback() failed! => 0x%x", result);
	SceDebugForDriver_391B74B7("!!\n");
	goto out;
callbackError:
	SceDebugForDriver_391B74B7("HID: sceKernelCreateCallback(Bt) failed! => 0x%x", result);
	SceDebugForDriver_391B74B7("!!\n");
out:
	sceKernelDeleteEventFlag(eventFlag);
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
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x1000;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	} else if ((notifyArg & 0x800000) != 0) {
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags &= ~0x1000U;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	}
	hidUnlock();
	return 0;
}

int hidSystemEvent(int resume, int eventId, void *args, void *opt)
{
	int intrState;

	if (resume != 0) {
		if (resume == 1 && eventId == 0x100000) {
			intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
			hidFlags &= ~0x20U;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		}
		return 0;
	}
	switch (eventId) {
	case 0x402:
		return (int)hidFlags < 0 ? -2 : 0;
	case 0x4000:
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x20;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags &= ~0x10U;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
		hidReleaseBluetoothDevices();
		return 0;
	case 0x200:
		intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
		hidFlags |= 0x10;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
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
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags |= 0x80000000U;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
#define HID_EVENT_ADDRESS() \
	(address.words[0] = event.mac0, address.words[1] = event.mac1, address.value)
	for (;;) {
		result = SceBtForDriver_5ABB9A9D(&event, 1);
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
			SceDebugForDriver_391B74B7("HID: Invalid event type!! (%d)", event.id);
			SceDebugForDriver_391B74B7("!!\n");
			break;
		}
	}
#undef HID_EVENT_ADDRESS
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags &= ~0x80000000U;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
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
	result = sceKernelCreateThread("SceHid", hidBluetoothThread, 60,
				       0x2000, 0, 0x80000, NULL);
	if (result < 0) {
		SceDebugForDriver_391B74B7("HID: sceKernelCreateThread() failed! => 0x%x", result);
		goto error;
	}
	result = sceKernelStartThread(result, args, (void *)argp);
	if (result < 0) {
		SceDebugForDriver_391B74B7("HID: sceKernelStartThread() failed! => 0x%x", result);
		goto error;
	}
	result = sceKernelRegisterSysEventHandler("SceHid", hidSystemEvent, NULL);
	if (result < 0) {
		SceDebugForDriver_391B74B7("HID: sceKernelRegisterSysEventHandler() failed! => 0x%x",
				 result);
		goto error;
	}
	result = sceKernelRegisterProcEventHandler("SceHid", &hidProcessHandlers, NULL);
	if (result < 0) {
		SceDebugForDriver_391B74B7("HID: sceKernelRegisterProcEventHandler() failed! => 0x%x",
				 result);
		goto error;
	}
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&hidSpinlock);
	hidFlags |= 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&hidSpinlock, intrState);
	SceSysrootForDriver_34281746(&hidSysrootOps);
	return 0;
error:
	SceDebugForDriver_391B74B7("!!\n");
	return 2;
}
