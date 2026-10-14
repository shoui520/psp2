/* Private AV configuration operations. */

#include "av_config_internal.h"
#include <sce/regmgr.h>
#include <sce/power.h>
#include <sce/display.h>
#include <sce/compat.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <stddef.h>

int avConfigSetEventFlag(unsigned int bits)
{
	return ksceKernelSetEventFlag(avConfigState.eventFlag, bits);
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
		return ksceKernelNotifyCallback(avConfigState.callback, events);
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

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.suspendInterval = interval;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
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
	ksceKernelPowerTick(0);
	kscePowerSetIdleTimer(0, suspendTime);
	if (ksceSblAimgrIsDolce() == 0)
		kscePowerSetIdleTimer(1, dimmingTime);
	kscePowerSetIdleTimer(2, (SceUInt64)interval * 1000000);
	return 0;
}

int avConfigSetDimmingInterval(unsigned int interval)
{
	SceKernelIntrStatus intr;

	if (ksceSblAimgrIsDolce() == 0)
		return (int)0x803a0109;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.dimmingInterval = interval;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	kscePowerSetIdleTimer(1, (SceUInt64)interval * 1000000);
	return 0;
}

int avConfigSetDisplayColorSpaceMode(int mode)
{
	SceKernelIntrStatus intr;
	int result;

	if (ksceSblAimgrIsDolce() != 0)
		return (int)0x803a0109;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.colorSpaceMode = mode;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	if (mode == 1) {
		result = ksceDisplaySetColorSpaceMode(0, 2);
		SceCompatForDriver_6EBD9E01(2);
	} else {
		result = ksceDisplaySetColorSpaceMode(0, 0);
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
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	if (mode == 1)
		avConfigConfiguration.settings.systemVolume = volume;
	else if (mode == 2)
		avConfigConfiguration.settings.btVolume = volume;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(2);
	return 0;
}

int avConfigApplyHdmiScalingRatio(const AvConfigScalingRatio *ratio)
{
	if (avConfigIsHdmiAvailable() == 0)
		return (int)0x803a0107;
	if (ksceSblAimgrIsDolce() != 0)
		return avConfigSetHdmiScalingRatio(ratio);
	return (int)0x803a0109;
}

