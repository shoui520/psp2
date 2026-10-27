/* Private AV configuration operations. */

#include "av_config_internal.h"
#include <regmgr/regmgr.h>
#include <power/power.h>
#include <display/display.h>
#include <compat/compat.h>
#include <sysmem/cpu.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/suspend.h>
#include <threadmgr/threadmgr.h>
#include <stddef.h>

int avConfigSetEventFlag(unsigned int bits)
{
	return sceKernelSetEventFlag(avConfigState.eventFlag, bits);
}

void avConfigGetVolumeControlState(int *mode, int *muted)
{
	SceUInt32 devices;

	*muted = avConfigState.muted;
	if (avConfigState.settings.displayBlanked == 1) {
		*mode = 0;
		return;
	}
	devices = avConfigState.previousOutputDevices[0] |
		avConfigState.previousOutputDevices[1];
	if (devices & 5) {
		*mode = 1;
		return;
	}
	if (devices & 0x10) {
		if (avConfigState.bt.active.profile == 0x20000000)
			*mode = 4;
		else if (avConfigState.bt.active.flags & 0x200)
			*mode = 2;
		else if (avConfigState.bt.active.flags & 0x100)
			*mode = 3;
		else
			*mode = 4;
		return;
	}
	if (devices & 0xa)
		*mode = 0;
}

int avConfigConvertBtVolume(unsigned int volume, unsigned int *converted)
{
	if (volume > 127)
		return (int)0x803a0103;
	*converted = (30 * volume + 63) / 127;
	return 0;
}

int avConfigSetNotificationCallback(SceUID callback, SceUInt32 mask)
{
	avConfigState.callback = callback;
	avConfigState.callbackMask = mask;
	return 0;
}

int avConfigNotifyCallback(int events)
{
	int result = avConfigState.callbackMask & events;

	if (result != 0)
		return sceKernelNotifyCallback(avConfigState.callback, events);
	return result;
}

int avConfigIsHdmiAvailable(void)
{
	return avConfigHdmiAvailable == 1;
}

int avConfigGetHdmiPowerState(void)
{
	return avConfigHdmiPowered;
}

int avConfigWriteMasterVolume(unsigned int volume)
{
	if (volume > 30)
		return (int)0x803a0103;
	return sceRegMgrSetKeyInt(avConfigMasterVolumeCategory, avConfigMasterVolumeKey, volume);
}

int avConfigReadMasterVolume(int *volume)
{
	int value;
	int result;

	if (volume == NULL)
		return (int)0x803a0101;
	result = sceRegMgrGetKeyInt(avConfigMasterVolumeCategory, avConfigMasterVolumeKey, &value);
	if (result == 0 && value <= 30)
		*volume = value;
	else
		*volume = 20;
	return result;
}

int avConfigSetSuspendInterval(unsigned int interval)
{
	SceKernelIntrStatus intr;
	SceUInt64 suspendTime;
	SceUInt64 dimmingTime;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.suspendInterval = interval;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	if (interval == 0) {
		suspendTime = 0;
		dimmingTime = 0;
	} else if (interval == 60) {
		suspendTime = 75000000;
		dimmingTime = 45000000;
	} else {
		suspendTime = (SceUInt64)(interval + 15) * 1000000;
		dimmingTime = 60000000;
	}
	sceKernelPowerTick(0);
	ScePowerForDriver_6BC26FC7(0, suspendTime);
	if (SceSblAIMgrForDriver_71608CA3() == 0)
		ScePowerForDriver_6BC26FC7(1, dimmingTime);
	ScePowerForDriver_6BC26FC7(2, (SceUInt64)interval * 1000000);
	return 0;
}

int avConfigSetDimmingInterval(unsigned int interval)
{
	SceKernelIntrStatus intr;

	if (SceSblAIMgrForDriver_71608CA3() == 0)
		return (int)0x803a0109;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.dimmingInterval = interval;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	ScePowerForDriver_6BC26FC7(1, (SceUInt64)interval * 1000000);
	return 0;
}

int avConfigSetDisplayColorSpaceMode(int mode)
{
	SceKernelIntrStatus intr;
	int result;

	if (SceSblAIMgrForDriver_71608CA3() != 0)
		return (int)0x803a0109;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.colorSpaceMode = mode;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	if (mode == 1) {
		result = SceDisplayForDriver_8D79D187(0, 2);
		SceCompatForDriver_6EBD9E01(2);
	} else {
		result = SceDisplayForDriver_8D79D187(0, 0);
		SceCompatForDriver_6EBD9E01(0);
	}
	return result;
}

int avConfigSetRoutingVolume(int volume)
{
	int mode;
	int muted;
	SceKernelIntrStatus intr;

	avConfigGetVolumeControlState(&mode, &muted);
	if ((unsigned int)(mode - 1) > 1)
		return (int)0x803a0105;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	if (mode == 1)
		avConfigConfiguration.settings.systemVolume = volume;
	else if (mode == 2)
		avConfigConfiguration.settings.btVolume = volume;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(2);
	return 0;
}

int avConfigApplyHdmiScalingRatio(const AvConfigScalingRatio *ratio)
{
	if (avConfigIsHdmiAvailable() == 0)
		return (int)0x803a0107;
	if (SceSblAIMgrForDriver_71608CA3() != 0)
		return avConfigSetHdmiScalingRatio(ratio);
	return (int)0x803a0109;
}

