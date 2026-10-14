/* Bluetooth audio device state and event handling. */

#include "av_config_internal.h"
#include <sce/bt_registered.h>
#include <sce/bt_event.h>
#include <sce/bt_audio.h>
#include <sce/threadmgr.h>
#include <psp2/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/cpu/spinlock.h>

void avConfigReadBtDevice(AvConfigBtAudioState *bt, SceUInt64 address)
{
	SceBtRegisteredInfo info;
	int state;

	if (ksceBtGetRegisteredInfo(address, &info, 1) != 1)
		return;
	info.profiles &= 0x20000008;
	if (info.profiles == 0)
		return;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	if (info.profiles & 0x20000000) {
		avConfigConfiguration.settings.connectedDevices |= 0x810;
	} else {
		if (info.profiles & 8)
			avConfigConfiguration.settings.connectedDevices |= 0x10;
		else
			avConfigConfiguration.settings.connectedDevices &= ~0x10u;
		if (info.profiles & 4)
			avConfigConfiguration.settings.connectedDevices |= 0x800;
		else
			avConfigConfiguration.settings.connectedDevices &= ~0x800u;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
	bt->pending.address = address;
	bt->pending.profiles = info.profiles;
	bt->pending.flags = info.flags;
	if (info.profiles & 4)
		bt->pending.sampleRate = ksceBtFreqAudio(address);
	bt->deviceChanged = 1;
}

int avConfigReadBtEvents(AvConfigState *state)
{
	AvConfigBtAudioState *bt = &state->bt;
	SceBtEvent event;
	SceUInt64 address;
	int lockState;
	int volume;

	while (ksceBtReadEvent(&event, 1) > 0) {
		address = ((SceUInt64)event.mac1 << 32) | event.mac0;
		if (event.id == 5) {
			if (event.status == 0) {
				bt->disconnected = 0;
				avConfigReadBtDevice(bt, address);
			}
			continue;
		}
		if (event.id == 6) {
			if (bt->operation != 0) {
				if (address != bt->pending.address)
					continue;
				bt->disconnected = 1;
				lockState = ksceKernelSpinlockLowLockCpuSuspendIntr(
					&avConfigConfiguration.lock);
				avConfigConfiguration.settings.connectedDevices &= ~0x810u;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(
					&avConfigConfiguration.lock, lockState);
				bt->pending.address = 0;
				bt->deviceChanged = 1;
				bt->completionFlags |= 3;
				continue;
			}
			if (address != bt->active.address && address != bt->pending.address)
				continue;
			bt->disconnected = 1;
			goto clearPending;
		}
		if (event.id == 28) {
			if (address == bt->pending.address)
				bt->pending.sampleRate = event.argument;
			continue;
		}
		if (event.id == 19 && event.status == 7) {
			if ((bt->operation == 0 && address == bt->active.address) ||
			    address == bt->pending.address)
				goto clearPending;
		} else {
			if (event.id == 16) {
				if (bt->operation == 1)
					bt->completionFlags |= event.status != 0 ? 2 : 1;
				continue;
			}
			if (event.id == 17) {
				if (bt->operation == 2)
					bt->completionFlags |= 1;
				continue;
			}
			if (event.id == 14) {
				if (address != bt->active.address && address != bt->pending.address)
					avConfigReadBtDevice(bt, address);
				if (event.status != 0) {
					if (event.status == 1)
						bt->volumeNotification = 1;
				} else {
					avConfigUpdateBtVolume(bt->volumeNotification, event.argument);
					bt->volumeNotification = 0;
				}
				continue;
			}
			if (event.id == 15) {
				bt->buttonCompleted = 1;
				continue;
			}
			if (event.id != 13 || address == bt->active.address)
				continue;
		}
		volume = ksceBtAvrcpReadVolume(address);
		if (volume >= 0)
			avConfigUpdateBtVolume(bt->volumeNotification, volume);
		avConfigReadBtDevice(bt, ((SceUInt64)event.mac1 << 32) | event.mac0);
		continue;

clearPending:
		lockState = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.connectedDevices &= ~0x810u;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, lockState);
		bt->pending.address = 0;
		bt->deviceChanged = 1;
	}
	return 0;
}

void avConfigSelectBtProfile(AvConfigBtAudioState *bt, const AvConfigSettings *settings)
{
	SceUInt32 profiles = bt->pending.profiles;
	SceUInt32 profile = profiles & 0xc;

	if (profile != 0xc) {
		if ((profiles & 0x2000000c) == 0x20000008) {
			if (settings->audioInputActive != 1 || settings->btMic != 1) {
				bt->pending.profile = 8;
				return;
			}
		} else if ((profiles & 0x2000000c) != 0x20000004) {
			bt->pending.profile = profiles;
			return;
		}
		bt->pending.profile = 0x20000000;
		return;
	}
	bt->pending.profile = 8;
	if (settings->audioInputActive == 1 && settings->btMic == 1)
		bt->pending.profile = profile;
}

int avConfigStartBtAudio(AvConfigBtAudioState *bt)
{
	SceUInt64 address;
	int state;

	if (ksceBtGetConnectingInfo(bt->pending.address) == 5)
		return 0;
	if (ksceBtStartAudio(bt->pending.address, bt->pending.profile) < 0) {
		state = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.connectedDevices &= ~0x810u;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
		bt->deviceChanged = 1;
		bt->pending.address = 0;
		return -2;
	}
	bt->completionFlags = 0;
	bt->operation = 1;
	do {
		sceKernelDelayThread(40000);
		sceKernelCheckCallback();
	} while (bt->completionFlags == 0);
	if (bt->completionFlags == 1) {
		if ((bt->pending.profile & 4) && bt->pending.sampleRate == 0)
			bt->pending.sampleRate = 48000;
		return 0;
	}
	if (bt->completionFlags != 2)
		return -1;
	address = bt->active.address;
	if (address != 0) {
		if (address == bt->pending.address) {
			bt->pending.profiles &= ~bt->pending.profile;
		} else {
			bt->pending.profiles = bt->active.profiles;
			bt->pending.address = address;
		}
		state = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		if (bt->pending.profiles & 0x20000000)
			avConfigConfiguration.settings.connectedDevices |= 0x810;
		else
			avConfigConfiguration.settings.connectedDevices =
				(avConfigConfiguration.settings.connectedDevices & ~0x800u) | 0x10;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
		bt->deviceChanged = 1;
		bt->active.address = 0;
		return -2;
	}
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.connectedDevices &= ~0x810u;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
	bt->pending.address = 0;
	bt->deviceChanged = 1;
	return -2;
}

int avConfigStopBtAudio(AvConfigBtAudioState *bt)
{
	if (ksceBtStopAudio(bt->active.address, bt->active.profile) < 0)
		ksceBtStartDisconnect((SceUInt32)bt->active.address,
				      (SceUInt32)(bt->active.address >> 32));
	bt->completionFlags = 0;
	bt->operation = 2;
	do {
		sceKernelCheckCallback();
		sceKernelDelayThread(40000);
	} while (bt->completionFlags == 0);
	return bt->completionFlags == 3 ? -1 : 0;
}

int avConfigSendBtVolume(SceUInt64 address, unsigned int volume)
{
	ksceBtAvrcpSendVolume(address, (SceUInt8)volume);
	return 0;
}

int avConfigSendBtButton(AvConfigBtAudioState *bt, unsigned int button)
{
	SceUInt64 address = bt->active.address;
	int retries = 101;

	bt->buttonCompleted = 0;
	ksceBtAvrcpSendButton(address, (SceUInt8)button);
	do {
		if (bt->buttonCompleted != 0)
			break;
		sceKernelCheckCallback();
		sceKernelDelayThread(40000);
		--retries;
	} while (retries != 0);
	return 0;
}
