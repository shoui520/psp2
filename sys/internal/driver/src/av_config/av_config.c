/* SceAVConfig settings and notifications. */

#include <skeleton.h>
#include "av_config_internal.h"
#include <threadmgr/threadmgr.h>
#include <regmgr/regmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/suspend.h>
#include <power/power.h>
#include <codec/codec.h>
#include <intrmgr/syscall.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <hdmi/hdmi.h>
#include <hpremote/hpremote.h>
#include <audioin/audioin.h>
#include <audio/audio.h>
#include <bt/bt_audio.h>
#include <bt/bt_registered.h>
#include <bt/bt_event.h>
#include <threadmgr/threadmgr.h>
#include <compat/compat.h>
#include <display/display.h>
#include <idstorage/idstorage.h>
#include <bt/bt_hid.h>
#include <ctrl/ctrl.h>
#include <sysmem/sysroot.h>
#include <iofilemgr/iofilemgr.h>
#include <stddef.h>
#include <string.h>

#pragma weak SceHdmiForDriver_0F12364A

int avConfigAvlsEnabled = 0;
int avConfigDisplayStarted = 0;
#if FW_VERSION >= 0x04000000
int avConfigCtrlRegistration = 0;
int avConfigHasCodec = 0;
SceUInt32 avConfigSavedOutputDevices = 0;
SceUInt64 avConfigAvlsStartTime = 0;
SceUID avConfigPowerCallback = 0;
int avConfigResumed = 0;
int avConfigAudioDevicesEnabled = 0;
int avConfigResumeActive = 0;
#else
int avConfigHasCodec = 0;
int avConfigAudioDevicesEnabled = 0;
int avConfigResumeActive = 0;
SceUID avConfigPowerCallback = 0;
int avConfigCtrlRegistration = 0;
SceUInt64 avConfigAvlsStartTime = 0;
SceUInt32 avConfigSavedOutputDevices = 0;
int avConfigResumed = 0;
#endif
AvConfigConfiguration avConfigConfiguration = {0};
AvConfigAvlsTimer avConfigAvlsTimer = {0};
AvConfigState avConfigState = {0};
int avConfigBtButtonState = 128;

const char avConfigMasterVolumeCategory[] = "/CONFIG/SOUND";
const char avConfigMasterVolumeKey[] = "master_volume";
static const char avConfigSystemVolumeCategory[] = "/CONFIG/SOUND";
static const char avConfigSystemVolumeKey[] = "main_volume";
static const char avConfigSpeakerMuteKey[] = "speaker_mute";
static const char avConfigSoundOutputKey[] = "sound_output";
static const char avConfigBtCategory[] = "/CONFIG/BT";
static const char avConfigBtMicKey[] = "bt_mic";
static const char avConfigAvlsKey[] = "avls";
static const char avConfigAvlsTimerKey[] = "avls_timer";
static const char avConfigDisplayCategory[] = "/CONFIG/DISPLAY";
static const char avConfigBrightnessKey[] = "brightness";
static const char avConfigPowerSavingCategory[] = "/CONFIG/POWER_SAVING";
static const char avConfigSuspendIntervalKey[] = "suspend_interval";
static const char avConfigDimmingIntervalKey[] = "dimming_interval";
static const char avConfigColorSpaceModeKey[] = "color_space_mode";
static const unsigned int avConfigMasterVolumeGain[31] = {
	0, 0x48a, 0x518, 0x5b7, 0x66a, 0x732, 0x813, 0x90f,
	0xa2a, 0xb68, 0xccc, 0xe5c, 0x101d, 0x1214, 0x1449, 0x16c3,
	0x198a, 0x1ca7, 0x2026, 0x2413, 0x287a, 0x2d6a, 0x32f5, 0x392c,
	0x4026, 0x47fa, 0x50c3, 0x5a9d, 0x65ac, 0x7214, 0x8000
};

int _sceAVConfigThread(SceSize arglen, void *argp);
int _sceAVConfigResumePhase1Main(int resume, int eventId, void *args, void *opt);
void avConfigResetConfiguration(void);
void avConfigGetSettings(AvConfigSettings *settings);
void avConfigRefreshAudioDevices(void);
void avConfigEnableAudioDevices(int enabled);
void avConfigInitializeConfiguration(int enableAvls, int shutterVolume);
void avConfigHpremoteCallback(int input, int multiType, int headphones,
			     int microphones, int value, void *argument);
void avConfigHdmiConnected(void);
void avConfigHdmiDisconnected(void);
void avConfigCompleteDisplayUpdate(void);
int SceAVConfigForDriver_0E60701A(unsigned int active);
void avConfigEnableAvls(void);

int _sceAVConfigPowerIntr(int notifyId, int notifyCount, int flags, void *userData)
{
	int result;
	const void *lr = __builtin_return_address(0);

	(void)notifyId;
	(void)notifyCount;
	(void)userData;
	SCE_DBG_ASSERT_CHECK_LR(SceSblAIMgrForDriver_71608CA3() != 0, 1364, lr);
	if (flags & 0x800000) {
		result = SceHdmiForDriver_B04F5B6A();
		if (result < 0) {
			SCE_DBG_ASSERT_CHECK_LR(0, 1372, lr);
		} else {
			result = SceHdmiForDriver_EA486A37(1);
			if (result < 0) {
				SCE_DBG_ASSERT_CHECK_LR(0, 1379, lr);
			} else {
				avConfigHdmiPowered = 1;
				avConfigHdmiUpdateMonitor();
				result = avConfigHdmiConfigureCec();
				if (result < 0)
					SCE_DBG_ASSERT_CHECK_LR(0, 1392, lr);
			}
		}
		return result;
	}
	if (flags & 0x400000) {
		result = SceHdmiForDriver_D929E4E5();
		if (result != (int)0x8037000e && result < 0)
			SCE_DBG_ASSERT_CHECK_LR(0, 1404, lr);
		result = SceHdmiForDriver_EA486A37(0);
		if (result < 0) {
			SCE_DBG_ASSERT_CHECK_LR(0, 1410, lr);
			return result;
		}
		avConfigHdmiPowered = 0;
		return result;
	}
	return 0;
}

int avConfigBtCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)userData;
	avConfigReadBtEvents(&avConfigState);
	return 0;
}

void avConfigCtrlButtonCallback(uint32_t buttons, uint32_t previous, void *argument)
{
	(void)argument;
	if (((previous ^ buttons) & 0x10000) != 0 && (buttons & 0x10000) != 0)
		sceHdmiCecCmdOneTouchPlay(2);
}

void avConfigHdmiEventCallback(int event)
{
	if (event & 2) {
		if ((ScePowerForDriver_9F26222A() & 0x7f) != 28)
			sceHdmiCecCmdOneTouchPlay(0);
	}
}

int _sceAVConfigResumePhase1Main(int resume, int eventId, void *args, void *opt)
{
	SceKernelIntrStatus intr;
	unsigned int brightness;
	unsigned int maximum;
	unsigned int elapsed;
	int retry;
	int result;
	const void *lr = __builtin_return_address(0);

	(void)args;
	(void)opt;
	if (resume != 0) {
		if (resume != 1)
			return 0;
		if (eventId != 0x100001) {
			if (eventId == 0x400000) {
				avConfigResumeActive = 1;
				avConfigEnableAudioDevices(1);
				avConfigRefreshAudioDevices();
				avConfigGetSettings(&avConfigState.settings);
				brightness = avConfigState.settings.brightness;
				maximum = ScePowerForDriver_D8759B55();
				if (maximum >= brightness)
					maximum = brightness;
				ScePowerForDriver_43D5CE1D(maximum);
				sceKernelSetEventFlag(avConfigState.eventFlag, 16);
				avConfigResumeActive = resume;
				avConfigEnableAudioDevices(resume);
				intr = sceKernelSpinlockLowLockCpuSuspendIntr(
					&avConfigAvlsTimer.lock);
				avConfigAvlsTimer.busy = 0;
				sceKernelSpinlockLowUnlockCpuResumeIntr(
					&avConfigAvlsTimer.lock, intr);
				return 0;
			}
			if (eventId == 0x1000f) {
				avConfigResetConfiguration();
				avConfigGetSettings(&avConfigState.settings);
				memset(&avConfigState, 0, offsetof(AvConfigState, settings));
				avConfigSavedOutputDevices = avConfigState.previousOutputDevices[0];
				avConfigState.previousOutputDevices[0] = 0;
				avConfigState.previousOutputDevices[1] = 0;
				avConfigState.previousInputDevice = 0;
				avConfigState.outputDevices[0] = 0;
				avConfigState.outputDevices[1] = 0;
				avConfigState.inputDevice = 0;
				avConfigState.previousRoutingChanges = 0;
				avConfigState.routingChanges = 0;
				avConfigResumed = 0;
			}
			return 0;
		}
		if (avConfigResumed != 0)
			return 0;
		result = 0;
		if (avConfigHdmiAvailable == 1) {
			if (SceSblAIMgrForDriver_4273B97B() != 0) {
				if (ScePowerForDriver_394DE492() != 0) {
					result = SceHdmiForDriver_B04F5B6A();
					if (result < 0)
						SCE_DBG_ASSERT_CHECK_LR(0, 1086, lr);
					else
						result = 0;
				}
			} else if (ScePowerForDriver_394DE492() == 0) {
				result = SceHdmiForDriver_EA486A37(0);
				if (result < 0)
					SCE_DBG_ASSERT_CHECK_LR(0, 1097, lr);
				else
					avConfigHdmiPowered = 0;
			} else {
				result = SceHdmiForDriver_B04F5B6A();
				if (result < 0) {
					SCE_DBG_ASSERT_CHECK_LR(0, 1108, lr);
				} else {
					result = SceHdmiForDriver_EA486A37(1);
					if (result < 0) {
						SCE_DBG_ASSERT_CHECK_LR(0, 1115, lr);
					} else {
						avConfigHdmiPowered = 1;
						result = avConfigHdmiConfigureCec();
						if (result < 0)
							SCE_DBG_ASSERT_CHECK_LR(0, 1126, lr);
					}
				}
			}
		}
		avConfigResumed = 1;
		return result;
	}
	if (eventId != 0x200)
		return 0;
	avConfigStopBtSend();
	avConfigStopBtReceive();
	if ((unsigned char)avConfigState.settings.connectedDevices == 17) {
		if (avConfigState.muted != 0) {
			if (avConfigState.callbackMask & 16)
				sceKernelNotifyCallback(avConfigState.callback, 16);
		} else if (avConfigState.settings.displayBlanked != 1) {
			avConfigState.muted = 1;
			avConfigState.savedVolume = avConfigState.settings.systemVolume;
			if (avConfigHasCodec != 0)
				SceCodecForDriver_8932F886(0);
			if (avConfigState.callbackMask & 16)
				sceKernelNotifyCallback(avConfigState.callback, 16);
		}
	}
	if (avConfigState.avlsTimerEnabled == 1) {
		for (;;) {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
			if (avConfigAvlsTimer.busy == 0)
				break;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
			sceKernelDelayThread(40000);
		}
		avConfigAvlsTimer.busy = 1;
		retry = 5;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
		elapsed = avConfigAvlsTimer.elapsed;
		for (; retry != 0; --retry) {
			if (sceRegMgrSetKeyInt(avConfigMasterVolumeCategory,
					    avConfigAvlsTimerKey, elapsed) == 0)
				break;
			sceKernelDelayThread(40000);
		}
	}
	avConfigResumeActive = 0;
	avConfigEnableAudioDevices(0);
	return 0;
}

int _sceAVConfigThread(SceSize arglen, void *argp)
{
	SceKernelIntrStatus intr;
	SceUInt8 idStorageValue;
	SceUInt64 now;
	AvConfigScalingRatio scaling;
	unsigned int bits, timeout, volume, devices, previousOutputs;
	unsigned int outputs, outputAdded, outputRemoved;
	unsigned int inputAdded, inputRemoved, oldBtOutput, oldBtInput;
	unsigned int newBtOutput, newBtInput, removedBtOutput, removedBtInput;
	unsigned int codecOutputs;
	int (*initializeHdmi)(void);
	int retries, result, registryValue, counter;
	int speakerEnabled, headphonesEnabled, multiEnabled, codecChanged;
	int deviceChanged, stopBt;
	const void *lr = __builtin_return_address(0);

	(void)arglen;
	(void)argp;
	avConfigState.hasVideoHardware = (SceSysconForDriver_CBD6D8BC() & 0x80) != 0;
	avConfigHasCodec = SceSblAIMgrForDriver_71608CA3() == 0;
	if (sceIdStorageLookup(0x118, 0, &idStorageValue, 1) == 0 &&
	    idStorageValue == 1) {
		for (retries = 5; retries != 0; --retries) {
			if (sceRegMgrGetKeyInt(avConfigMasterVolumeCategory,
					      avConfigAvlsTimerKey, &registryValue) == 0)
				break;
			sceKernelDelayThread(40000);
		}
		if (retries == 0)
			registryValue = 0;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
		avConfigAvlsTimer.elapsed = registryValue;
		avConfigAvlsTimer.tracking = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
		avConfigState.avlsTimerEnabled = 1;
	} else {
		avConfigState.avlsTimerEnabled = 0;
	}
	if (sceIdStorageLookup(0x11b, 0, &idStorageValue, 1) >= 0 &&
	    idStorageValue == 1)
		avConfigState.settings.shutterVolume = 26;
	else
		avConfigState.settings.shutterVolume = 30;
	avConfigInitializeConfiguration(avConfigState.avlsTimerEnabled,
					avConfigState.settings.shutterVolume);
	avConfigGetSettings(&avConfigState.settings);
	volume = avConfigState.settings.brightness;
	bits = ScePowerForDriver_D8759B55();
	if (bits < volume)
		volume = bits;
	ScePowerForDriver_43D5CE1D(volume);
	avConfigSetEventFlag(16);
	avConfigResumeActive = 1;
	avConfigEnableAudioDevices(1);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
	avConfigAvlsTimer.busy = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
	volume = avConfigState.settings.masterVolume;
	if (SceSblAIMgrForDriver_71608CA3() != 0 && volume <= 30)
		SceAudioForDriver_15FB30DF(avConfigMasterVolumeGain[volume]);
	SceHpremoteForDriver_D22913DB(avConfigHpremoteCallback, NULL);
	result = sceKernelCreateCallback("SceAVConfigBt", 0, avConfigBtCallback, NULL);
	SceBtForDriver_120AC1F7(result, 0x100be060ULL);
	if (avConfigHdmiAvailable == 1) {
		SceHdmiForDriver_0F12364A(1, avConfigHdmiConnected);
		SceHdmiForDriver_0F12364A(0, avConfigHdmiDisconnected);
		SceHdmiForDriver_0F12364A(2, sceAVConfigHdmiHdcpDoneIntr);
		SceHdmiForDriver_0F12364A(3, sceAVConfigHdmiHdcpDoneIntr);
		if (SceSblAIMgrForDriver_71608CA3() != 0) {
			if (SceSysrootForDriver_834439A7() == 0 &&
			    SceSysrootForDriver_B0E1FC67() == 0 &&
			    sceIoRemove("ur0:safemode/force_480p") == 0)
				sceRegMgrSetKeyInt(avConfigDisplayCategory,
						   "hdmi_resolution_mode", 3);
			avConfigHdmiAudioMode = 1;
			registryValue = 0;
			if (sceRegMgrSystemParamGetInt(2265410, &registryValue) >= 0 &&
			    (unsigned int)registryValue < 2)
				SceHdmiForDriver_47894820(registryValue);
			avConfigPowerCallback = sceKernelCreateCallback("SceAVConfigDisplay",
						0, _sceAVConfigPowerIntr, NULL);
			if (avConfigPowerCallback < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1584, lr);
			result = scePowerRegisterCallback(avConfigPowerCallback);
			if (result < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1592, lr);
			result = sceHdmiCecRegisterEventCallback(2, avConfigHdmiEventCallback);
			if (result < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1597, lr);
			result = SceHdmiForDriver_B04F5B6A();
			if (result < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1604, lr);
			result = SceHdmiForDriver_EA486A37(1);
			if (result < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1610, lr);
			avConfigHdmiPowered = 1;
			result = avConfigHdmiConfigureCec();
			if (result < 0)
				SCE_DBG_ASSERT_CHECK_LR(0, 1618, lr);
			scaling.size = sizeof(scaling);
			if (sceRegMgrGetKeyInt(avConfigDisplayCategory,
					      "hdmi_out_scaling_ratio", &registryValue) != 0 ||
			    (unsigned int)registryValue - 800000000u > 400000000u)
				scaling.ratio = 900000000;
			else
				scaling.ratio = registryValue;
			if (SceSysrootForDriver_834439A7() != 0 || SceSysrootForDriver_B0E1FC67() != 0)
				scaling.ratio = 900000000;
			result = avConfigApplyHdmiScalingRatio(&scaling);
			if (result < 0) {
				SCE_DBG_ASSERT_CHECK_FUNC(0, 201, "_initHdmiScalingRatio");
				SCE_DBG_ASSERT_CHECK_LR(0, 1623, lr);
			}
			avConfigCtrlRegistration = SceCtrlForDriver_FF9D4DD4(0x10000,
					avConfigCtrlButtonCallback, NULL, NULL);
		} else if (sceRegMgrGetKeyInt("/DEVENV/TOOL", "hdmi_audio_mode",
					    &avConfigHdmiAudioMode) != 0) {
			avConfigHdmiAudioMode = 1;
		}
		avConfigHdmiUpdateMonitor();
	}
	avConfigSetSuspendInterval(avConfigState.settings.suspendInterval);
	avConfigSetDimmingInterval(avConfigState.settings.dimmingInterval);
	counter = 1;
	avConfigSetDisplayColorSpaceMode(avConfigState.settings.colorSpaceMode);
	for (;;) {
		timeout = 40000;
		sceKernelWaitEventFlag(avConfigState.eventFlag, ~0u, 5, &bits, &timeout);
		avConfigState.eventBits = bits;
		if (avConfigState.bt.deviceChanged == 0) {
			avConfigState.bt.operation = 0;
			sceKernelCheckCallback();
		}
		if (bits == 0 && avConfigState.bt.deviceChanged != 1)
			goto iterationComplete;
		avConfigState.bt.deviceChanged = 0;
		avConfigGetSettings(&avConfigState.settings);
		if (bits & 16) {
			for (retries = 10; retries != 0; --retries) {
				if (SceHpremoteForDriver_71B5FB67() == 1)
					break;
				sceKernelDelayThread(40000);
			}
			avConfigGetSettings(&avConfigState.settings);
		}
		if (bits & 2) {
			if (avConfigState.settings.displayBlanked == 1)
				goto iterationComplete;
			outputs = avConfigState.previousOutputDevices[0] |
				avConfigState.previousOutputDevices[1];
			if (outputs & 7) {
				if (avConfigState.muted == 1)
					avConfigState.muted = 0;
				if (avConfigHasCodec != 0)
					SceCodecForDriver_8932F886(avConfigState.settings.systemVolume);
			} else if (outputs & 16) {
				volume = 0;
				if ((unsigned int)avConfigState.settings.btVolume <= 30)
					volume = (127 * avConfigState.settings.btVolume + 15) / 30u;
				avConfigSendBtVolume(avConfigState.bt.active.address, volume);
			}
			goto iterationComplete;
		}
		if (bits & 1) {
			if (avConfigState.settings.volumeKeyCommand & 1) {
				if (avConfigBtButtonState & 128) {
					avConfigBtButtonState = 65;
					avConfigSendBtButton(&avConfigState.bt, 65);
				}
			} else if (avConfigState.settings.volumeKeyCommand & 2) {
				if (avConfigBtButtonState & 128) {
					avConfigBtButtonState = 66;
					avConfigSendBtButton(&avConfigState.bt, 66);
				}
			} else if (avConfigBtButtonState != 128) {
				avConfigBtButtonState = 128;
				avConfigSendBtButton(&avConfigState.bt, 128);
			}
			goto iterationComplete;
		}
		if (bits & 4) {
			if (((avConfigState.previousOutputDevices[0] |
			      avConfigState.previousOutputDevices[1]) & 7) != 0 &&
			    avConfigState.muted == 0 && avConfigState.settings.displayBlanked != 1) {
				avConfigState.muted = 1;
				avConfigState.savedVolume = avConfigState.settings.systemVolume;
				if (avConfigHasCodec != 0)
					SceCodecForDriver_8932F886(0);
			}
			goto iterationComplete;
		}
		if (bits & 32) {
			volume = avConfigState.settings.masterVolume;
			if (SceSblAIMgrForDriver_71608CA3() != 0 && volume <= 30)
				SceAudioForDriver_15FB30DF(avConfigMasterVolumeGain[volume]);
			goto iterationComplete;
		}
		if (bits & 0x10000000) {
			SceDisplayForDriver_9E3C6DC6(0, 0);
			goto iterationComplete;
		}
		if (bits & 0x20000000) {
			SceDisplayForDriver_9E3C6DC6(0, 2);
			goto iterationComplete;
		}
		if (bits & 0x40000000) {
			SceDisplayForDriver_9E3C6DC6(0, 0);
			goto iterationComplete;
		}
		initializeHdmi = avConfigInitializeHdmiOutput;
		if (SceSblAIMgrForDriver_71608CA3() != 0)
			initializeHdmi = avConfigHdmiSetInitialOutput;
		if (avConfigState.hasVideoHardware == 1 &&
		    avConfigState.settings.soundOutput == 1) {
			avConfigState.outputDevices[0] =
				avConfigConfiguration.settings.connectedDevices & 1;
			avConfigState.outputDevices[1] =
				avConfigConfiguration.settings.connectedDevices & 1;
		} else {
			devices = avConfigState.settings.connectedDevices;
			if (avConfigState.settings.hdcpState == 2) {
				devices &= ~8u;
				avConfigState.settings.connectedDevices &= ~8u;
			}
			if (avConfigState.settings.btAudioPolicy == 2 && (devices & 16)) {
				if (devices & 8)
					avConfigState.outputDevices[0] = 8;
				else if (devices & 4)
					avConfigState.outputDevices[0] = 4;
				else if (devices & 2)
					avConfigState.outputDevices[0] = 2;
				else
					avConfigState.outputDevices[0] = devices & 1;
				avConfigState.outputDevices[1] = 16;
			} else {
				if (devices & 16)
					avConfigState.outputDevices[0] = 16;
				else if (devices & 8)
					avConfigState.outputDevices[0] = 8;
				else if (devices & 4)
					avConfigState.outputDevices[0] = 4;
				else if (devices & 2)
					avConfigState.outputDevices[0] = 2;
				else
					avConfigState.outputDevices[0] = devices & 1;
			}
			if ((unsigned int)avConfigState.settings.displayMode - 1u <= 1) {
				avConfigState.outputDevices[0] =
					avConfigConfiguration.settings.connectedDevices & 1;
			} else if (avConfigState.settings.displayMode == 3) {
				if (devices & 16)
					avConfigState.outputDevices[0] = 16;
				else if (devices & 4)
					avConfigState.outputDevices[0] = 4;
				else
					avConfigState.outputDevices[0] = 0;
			} else if (avConfigState.settings.displayMode == 4) {
				avConfigState.outputDevices[0] = devices & 4;
				if (avConfigConfiguration.settings.connectedDevices & 1)
					avConfigState.outputDevices[0] |= 1;
			}
			if (avConfigState.settings.btAudioPolicy != 2 || (devices & 16) == 0)
				avConfigState.outputDevices[1] = avConfigState.outputDevices[0];
			if (avConfigState.outputDevices[0] == 8) {
				SceAudioForDriver_AC9383D0(0, 1);
				SceAudioForDriver_AC9383D0(1,
					avConfigState.outputDevices[1] == 8 ? 1 :
					avConfigState.outputDevices[1] == 16 ? 2 : 0);
			}
		}
		switch (bits & 0x300) {
		case 0x100:
			initializeHdmi();
			break;
		case 0x200:
			avConfigHdmiDisableHead();
			break;
		case 0x300:
			if (avConfigState.settings.hdcpState != 0) {
				avConfigHdmiDisableHead();
				initializeHdmi();
			} else {
				avConfigHdmiDisableHead();
			}
			break;
		}
		devices = avConfigState.settings.connectedDevices;
		if (devices & 0x810) {
			avConfigSelectBtProfile(&avConfigState.bt, &avConfigState.settings);
			devices = avConfigState.settings.connectedDevices;
		}
		if (avConfigState.settings.audioInputActive != 0 && (devices & 0x800) &&
		    avConfigState.settings.btMic == 1 &&
		    (avConfigState.bt.pending.profile & 0x20000004))
			avConfigState.inputDevice = 0x800;
		else if (devices & 0x400)
			avConfigState.inputDevice = 0x400;
		else if (devices & 0x200)
			avConfigState.inputDevice = 0x200;
		else if (devices & 0x100)
			avConfigState.inputDevice = 0x100;
		else
			avConfigState.inputDevice = avConfigConfiguration.settings.connectedDevices & 0x100;
		previousOutputs = avConfigState.previousOutputDevices[0] |
			avConfigState.previousOutputDevices[1];
		outputs = avConfigState.outputDevices[0] | avConfigState.outputDevices[1];
		outputAdded = outputs & ~previousOutputs;
		oldBtOutput = previousOutputs & 16;
		deviceChanged = avConfigState.bt.pending.address != avConfigState.bt.active.address ||
			avConfigState.bt.pending.profile != avConfigState.bt.active.profile;
		if (oldBtOutput != 0 && (outputs & 16) && deviceChanged)
			outputAdded |= 16;
		if ((previousOutputs & 8) && (outputs & 8) && avConfigState.settings.hdcpState != 4)
			outputAdded |= 8;
		outputRemoved = previousOutputs & ~outputs;
		if (oldBtOutput != 0 && (outputs & 16) && deviceChanged)
			outputRemoved |= 16;
		inputAdded = avConfigState.inputDevice & ~avConfigState.previousInputDevice;
		oldBtInput = avConfigState.previousInputDevice & 0x800;
		inputRemoved = avConfigState.previousInputDevice & ~avConfigState.inputDevice;
		if (oldBtInput != 0 && (avConfigState.inputDevice & 0x800) && deviceChanged) {
			inputAdded |= 0x800;
			inputRemoved |= 0x800;
		}
		if ((outputAdded & 8) && avConfigState.settings.hdcpState != 4 &&
		    avConfigHdmiWaitHdcp(&avConfigState.settings.hdcpState) != 0)
			goto displayComplete;
		newBtOutput = outputAdded & 16;
		newBtInput = inputAdded & 0x800;
		removedBtOutput = outputRemoved & 16;
		removedBtInput = inputRemoved & 0x800;
		if (avConfigState.btAudioRunning == 1) {
			if (removedBtOutput != 0) {
				avConfigStopBtSend();
				if (newBtOutput == 0 && (avConfigState.callbackMask & 1))
					sceKernelNotifyCallback(avConfigState.callback, 1);
			}
			if (removedBtInput != 0)
				avConfigStopBtReceive();
		}
		stopBt = removedBtOutput != 0 ?
			(removedBtInput != 0 || oldBtInput == 0) :
			(removedBtInput != 0 && oldBtOutput == 0);
		if (stopBt) {
			if (avConfigState.bt.disconnected == 0)
				avConfigStopBtAudio(&avConfigState.bt);
			avConfigState.btAudioRunning = 0;
		}
		codecChanged = (outputRemoved & 7) != 0;
		speakerEnabled = (outputRemoved & 1) == 0 && (previousOutputs & 1) != 0;
		headphonesEnabled = (outputRemoved & 4) == 0 && (previousOutputs & 4) != 0;
		multiEnabled = (outputRemoved & 2) == 0 && (previousOutputs & 2) != 0;
		if ((newBtOutput != 0 || newBtInput != 0) && avConfigState.btAudioRunning == 0) {
			if (avConfigStartBtAudio(&avConfigState.bt) != 0) {
				avConfigState.previousOutputDevices[0] &= ~16u;
				avConfigState.previousOutputDevices[1] &= ~16u;
				avConfigState.previousInputDevice &= ~0x800u;
				avConfigState.bt.active.address = 0;
				if ((avConfigState.settings.connectedDevices & 6) == 0) {
					if (avConfigState.muted != 0) {
						if (avConfigState.callbackMask & 16)
							sceKernelNotifyCallback(avConfigState.callback, 16);
					} else if (avConfigState.settings.displayBlanked != 1) {
						avConfigState.muted = 1;
						avConfigState.savedVolume = avConfigState.settings.systemVolume;
						if (avConfigHasCodec != 0)
							SceCodecForDriver_8932F886(0);
						if (avConfigState.callbackMask & 16)
							sceKernelNotifyCallback(avConfigState.callback, 16);
					}
				}
				goto displayComplete;
			}
			avConfigState.btAudioRunning = 1;
		}
		SceAudioForDriver_AC9383D0(0, avConfigState.outputDevices[0] == 8 ? 1 :
					avConfigState.outputDevices[0] == 16 ? 2 : 0);
		SceAudioForDriver_AC9383D0(1, avConfigState.outputDevices[1] == 8 ? 1 :
					avConfigState.outputDevices[1] == 16 ? 2 : 0);
		if (avConfigResumeActive == 1) {
			if (avConfigState.inputDevice == 0x400)
				sceAudioInSelectInput(1);
			else if (avConfigState.inputDevice == 0x800)
				sceAudioInSelectInput(16);
			else if (avConfigState.inputDevice != 0)
				sceAudioInSelectInput(0);
			else if (SceSblAIMgrForDriver_71608CA3() != 0)
				sceAudioInSelectInput((avConfigState.settings.connectedDevices & 0x800) &&
						(avConfigState.settings.btMic & 1) ? 16 : 32);
		}
		if (newBtOutput != 0) {
			avConfigState.muted = 0;
			if (avConfigHasCodec != 0)
				SceCodecForDriver_8932F886(avConfigState.settings.systemVolume);
			avConfigStartBtSend(avConfigState.bt.pending.address, avConfigState.bt.pending.profile);
		}
		if (newBtInput != 0)
			avConfigStartBtReceive(avConfigState.bt.pending.address,
					      avConfigState.bt.pending.profile,
					      avConfigState.bt.pending.sampleRate);
		if (outputAdded & 1) {
			codecChanged = 1;
			speakerEnabled = 1;
		}
		if (outputAdded & 4) {
			codecChanged = 1;
			headphonesEnabled = 1;
		}
		if (outputAdded & 2) {
			codecChanged = 1;
			multiEnabled = 1;
		}
		if (codecChanged) {
			codecOutputs = (speakerEnabled ? 0x1000 : 0) |
				(headphonesEnabled ? 0x100 : 0) | (multiEnabled ? 0x400 : 0);
			SceCodecForDriver_762BF716(codecOutputs);
			if (speakerEnabled && removedBtOutput != 0) {
				if (avConfigState.muted != 0) {
					if (avConfigState.callbackMask & 16)
						sceKernelNotifyCallback(avConfigState.callback, 16);
				} else if (avConfigState.settings.displayBlanked != 1) {
					avConfigState.muted = 1;
					avConfigState.savedVolume = avConfigState.settings.systemVolume;
					if (avConfigHasCodec != 0)
						SceCodecForDriver_8932F886(0);
					if (avConfigState.callbackMask & 16)
						sceKernelNotifyCallback(avConfigState.callback, 16);
				}
			}
			if (speakerEnabled && ((outputRemoved & 4) || (avConfigSavedOutputDevices & 4)) &&
			    avConfigState.settings.speakerMute == 1) {
				if (avConfigState.muted != 0) {
					if (avConfigState.callbackMask & 16)
						sceKernelNotifyCallback(avConfigState.callback, 16);
				} else if (avConfigState.settings.displayBlanked != 1) {
					avConfigState.muted = avConfigState.settings.speakerMute;
					avConfigState.savedVolume = avConfigState.settings.systemVolume;
					if (avConfigHasCodec != 0)
						SceCodecForDriver_8932F886(0);
					if (avConfigState.callbackMask & 16)
						sceKernelNotifyCallback(avConfigState.callback, 16);
				}
			} else if (avConfigState.muted == 0 && avConfigHasCodec != 0) {
				SceCodecForDriver_8932F886(avConfigState.settings.systemVolume);
			}
		}
		avConfigState.previousOutputDevices[0] = avConfigState.outputDevices[0];
		avConfigState.previousOutputDevices[1] = avConfigState.outputDevices[1];
		avConfigState.previousInputDevice = avConfigState.inputDevice;
		avConfigState.previousRoutingChanges = avConfigState.routingChanges;
		if (newBtOutput != 0 || newBtInput != 0)
			avConfigState.bt.active = avConfigState.bt.pending;
		else if (stopBt)
			avConfigState.bt.active.address = 0;
displayComplete:
		if (bits & 0x300000) {
			if (avConfigState.settings.displayBlanked == 1) {
				if (avConfigState.previousOutputDevices[0] == 16 &&
				    (avConfigState.bt.active.flags & 0x200))
					avConfigSendBtVolume(avConfigState.bt.active.address, 127);
				if (avConfigHasCodec != 0)
					SceCodecForDriver_8932F886(30);
			} else if (avConfigState.settings.displayBlanked == 0) {
				volume = 0;
				if ((unsigned int)avConfigState.settings.btVolume <= 30)
					volume = (127 * avConfigState.settings.btVolume + 15) / 30u;
				avConfigSendBtVolume(avConfigState.bt.pending.address, volume);
				if (avConfigState.muted == 0) {
					if (avConfigHasCodec != 0)
						SceCodecForDriver_8932F886(avConfigState.settings.systemVolume);
				} else if (avConfigState.muted == 1 && avConfigHasCodec != 0) {
					SceCodecForDriver_8932F886(0);
				}
			}
			avConfigCompleteDisplayUpdate();
		}
iterationComplete:
		result = SceAudioInForDriver_005A2A31() > 0;
		if (avConfigState.settings.audioInputActive != result)
			SceAVConfigForDriver_0E60701A(result);
		if (++counter != 5)
			continue;
		if (avConfigState.avlsTimerEnabled != 1) {
			counter = 0;
			continue;
		}
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
		if (avConfigAvlsTimer.busy != 0) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
			counter = 0;
			continue;
		}
		avConfigAvlsTimer.busy = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
		if (avConfigState.muted != 0 || avConfigState.previousOutputDevices[0] != 4 ||
		    (unsigned int)avConfigState.settings.systemVolume <= 21) {
			if (avConfigAvlsTimer.tracking == 1) {
				avConfigAvlsTimer.tracking = 0;
				++avConfigAvlsTimer.elapsed;
			}
		} else if (avConfigAvlsTimer.tracking != 0) {
			if ((SceUInt64)sceKernelGetSystemTimeWide() - avConfigAvlsStartTime > 1000000) {
				avConfigAvlsStartTime += 1000000;
				if ((int)++avConfigAvlsTimer.elapsed > 71999) {
					avConfigSetRoutingVolume(21);
					avConfigEnableAvls();
					avConfigAvlsTimer.elapsed = 0;
					avConfigAvlsTimer.tracking = 0;
					do {
						if (sceRegMgrSetKeyInt(avConfigSystemVolumeCategory,
								   avConfigSystemVolumeKey, 21) == 0)
							break;
						sceKernelDelayThread(40000);
					} while (--counter != 0);
					for (retries = 5; retries != 0; --retries) {
						if (sceRegMgrSetKeyInt(avConfigSystemVolumeCategory,
								   avConfigAvlsKey, 1) == 0)
							break;
						sceKernelDelayThread(40000);
					}
					if (avConfigState.callbackMask & 8)
						sceKernelNotifyCallback(avConfigState.callback, 8);
				}
			}
		} else {
			now = sceKernelGetSystemTimeWide();
			avConfigAvlsTimer.tracking = 1;
			avConfigAvlsStartTime = now;
		}
		counter = 0;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
		avConfigAvlsTimer.busy = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
	}
}

void avConfigSetAvlsTimeRemaining(unsigned int remaining)
{
	SceKernelIntrStatus intr;

	if (avConfigState.avlsTimerEnabled != 1)
		return;
	for (;;) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
		if (avConfigAvlsTimer.busy == 0)
			break;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
		sceKernelDelayThread(40000);
	}
	avConfigAvlsTimer.busy = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
	if (remaining <= 71999)
		remaining = 72000 - remaining;
	else
		remaining = 72000;
	avConfigAvlsTimer.elapsed = remaining;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigAvlsTimer.lock);
	avConfigAvlsTimer.busy = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigAvlsTimer.lock, intr);
}

int avConfigGetHdmiAudioMode(void)
{
	return avConfigHdmiAudioMode;
}

int module_start(SceSize arglen, const void *argp)
{
	int result;

	(void)arglen;
	(void)argp;
	memset(&avConfigState, 0, sizeof(avConfigState));
	result = avConfigInitializeBtTransfer();
	if (result < 0)
		return result;
	result = sceKernelCreateEventFlag("SceAVConfig", 0, 0, NULL);
	avConfigState.eventFlag = result;
	if (result < 0)
		return result;
	if (SceHdmiForDriver_0F12364A != NULL)
		avConfigHdmiAvailable = 1;
	result = sceKernelCreateThread("SceAVConfig", _sceAVConfigThread,
				      32, 0x2000, 0, 0x80000, NULL);
	avConfigState.thread = result;
	if (result < 0)
		return result;
	result = sceKernelStartThread(result, 0, NULL);
	if (result < 0)
		return result;
	sceKernelRegisterSysEventHandler("SceAVConfig",
					_sceAVConfigResumePhase1Main, NULL);
	return 0;
}

int sceAVConfigChangeReg(int setting, unsigned int value)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int result = (int)0x803a0101;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	if (setting == 0) {
		if (value > 1)
			goto out;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.btMic = value;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		avConfigSetEventFlag(0x1000000);
		result = 0;
	} else if (setting == 1) {
		if (value > 1)
			goto out;
		avConfigAvlsEnabled = value;
		if (value == 1)
			avConfigNotifyCallback(4);
		result = 0;
	} else if (setting == 2) {
		if (value > 1)
			goto out;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.soundOutput = value;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		avConfigSetEventFlag(16);
		result = 0;
	} else if (setting == 4) {
		if (value > 1)
			goto out;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.speakerMute = value;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		result = 0;
	} else if (setting == 3) {
		if (value > 71999)
			value = 72000;
		avConfigSetAvlsTimeRemaining(value);
		result = 0;
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetDisplayBrightness(unsigned int brightness)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	unsigned int maximum;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		maximum = ScePowerForDriver_D8759B55();
		if (maximum < brightness)
			brightness = maximum;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.brightness = brightness;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		ScePowerForDriver_43D5CE1D(brightness);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetDisplayMaxBrightness(unsigned int *brightness)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int maximum;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		maximum = ScePowerForDriver_D8759B55();
		if (sceKernelCopyToUser(brightness, &maximum, sizeof(maximum)) != 0)
			result = (int)0x803a0102;
		else
			result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetAcStatus(int *online)
{
	SceUInt32 syscall = sceSyscallEnter();
	int active;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		active = scePowerIsPowerOnline() != 0;
		if (sceKernelCopyToUser(online, &active, sizeof(active)) != 0)
			result = (int)0x803a0102;
		else
			result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetAutoSuspend2(unsigned int interval)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt64 timeout;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		timeout = (SceUInt64)interval * 1000000;
		ScePowerForDriver_6BC26FC7(0, timeout);
		if (SceSblAIMgrForDriver_71608CA3() == 0)
			ScePowerForDriver_6BC26FC7(1, timeout);
		ScePowerForDriver_6BC26FC7(2, timeout);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigClearAutoSuspend2(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	unsigned int interval;
	SceUInt64 suspendTime;
	SceUInt64 dimmingTime;
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	interval = avConfigConfiguration.settings.suspendInterval;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
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
	SceKernelSuspendForDriver_E0489831(0);
	ScePowerForDriver_6BC26FC7(0, suspendTime);
	if (SceSblAIMgrForDriver_71608CA3() == 0)
		ScePowerForDriver_6BC26FC7(1, dimmingTime);
	ScePowerForDriver_6BC26FC7(2, (SceUInt64)interval * 1000000);
	result = 0;
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetAutoSuspend(unsigned int interval)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	SceUInt64 suspendTime;
	SceUInt64 dimmingTime;
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
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
	SceKernelSuspendForDriver_E0489831(0);
	ScePowerForDriver_6BC26FC7(0, suspendTime);
	if (SceSblAIMgrForDriver_71608CA3() == 0)
		ScePowerForDriver_6BC26FC7(1, dimmingTime);
	ScePowerForDriver_6BC26FC7(2, (SceUInt64)interval * 1000000);
	result = 0;
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetAutoDisplayDimming(unsigned int interval)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		if (SceSblAIMgrForDriver_71608CA3() != 0) {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
			avConfigConfiguration.settings.dimmingInterval = interval;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
			ScePowerForDriver_6BC26FC7(1, (SceUInt64)interval * 1000000);
			result = 0;
		} else {
			result = (int)0x803a0109;
		}
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetDisplayColorSpaceMode(int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		if (SceSblAIMgrForDriver_71608CA3() != 0) {
			result = (int)0x803a0109;
		} else {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
			avConfigConfiguration.settings.colorSpaceMode = mode;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
			if (mode != 1) {
				result = SceDisplayForDriver_8D79D187(0, 0);
				SceCompatForDriver_6EBD9E01(0);
			} else {
				result = SceDisplayForDriver_8D79D187(0, 2);
				SceCompatForDriver_6EBD9E01(2);
			}
		}
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetVolCtrlEnable(int *mode, int *muted, int *avls)
{
	SceUInt32 syscall = sceSyscallEnter();
	int modeValue;
	int mutedValue;
	int avlsValue;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		avConfigGetVolumeControlState(&modeValue, &mutedValue);
		avlsValue = avConfigAvlsEnabled;
		if (sceKernelCopyToUser(mode, &modeValue, sizeof(modeValue)) != 0 ||
		    sceKernelCopyToUser(muted, &mutedValue, sizeof(mutedValue)) != 0 ||
		    (result = sceKernelCopyToUser(avls, &avlsValue, sizeof(avlsValue))) != 0)
			result = (int)0x803a0102;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetBtVol(int *notified, int *volume)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int notifiedValue;
	int volumeValue;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		volumeValue = avConfigConfiguration.settings.btVolume;
		notifiedValue = avConfigConfiguration.settings.btVolumeNotified;
		avConfigConfiguration.settings.btVolumeNotified = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		if (sceKernelCopyToUser(notified, &notifiedValue, sizeof(notifiedValue)) != 0 ||
		    (result = sceKernelCopyToUser(volume, &volumeValue, sizeof(volumeValue))) != 0)
			result = (int)0x803a0102;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetSystemVol(int *volume)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int volumeValue;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		volumeValue = avConfigConfiguration.settings.systemVolume;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		if (sceKernelCopyToUser(volume, &volumeValue, sizeof(volumeValue)) != 0)
			result = (int)0x803a0102;
		else
			result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSetSystemVol(int volume)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int mode;
	int muted;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		avConfigGetVolumeControlState(&mode, &muted);
		if ((unsigned int)(mode - 1) > 1) {
			result = (int)0x803a0105;
		} else {
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
			if (mode == 1)
				avConfigConfiguration.settings.systemVolume = volume;
			else if (mode == 2)
				avConfigConfiguration.settings.btVolume = volume;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
			avConfigSetEventFlag(2);
			result = 0;
		}
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigWriteRegSystemVol(int volume)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		sceRegMgrSetKeyInt(avConfigSystemVolumeCategory, avConfigSystemVolumeKey, volume);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigSendVolKey(int command)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.volumeKeyCommand = command;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		avConfigSetEventFlag(1);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigRegisterCallback(SceUID callback, SceUInt32 mask)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceUID guid;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		guid = SceSysmemForDriver_45D22597(0, callback);
		result = guid;
		if (guid >= 0) {
			avConfigSetNotificationCallback(guid, mask);
			result = 0;
		}
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigUnRegisterCallback(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		avConfigSetNotificationCallback(0, 0);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigMuteOn(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		avConfigSetEventFlag(4);
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigDisplayOn(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	SceUInt32 devices;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		if (avConfigDisplayStarted == 0) {
			if (avConfigAudioDevicesEnabled == 1) {
				devices = SceHpremoteForDriver_2229EF51() == 1 ? 2 : 0;
				if (SceHpremoteForDriver_219C58F1() == 1)
					devices |= 0x400;
				if (SceHpremoteForDriver_71B5FB67() == 1)
					devices |= 4;
				if (SceSblAIMgrForDriver_71608CA3() == 0)
					devices |= 1;
				intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
				avConfigConfiguration.settings.connectedDevices |= devices;
				sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
				avConfigSetEventFlag(0x10);
			}
			avConfigDisplayStarted = 1;
		}
		result = 0;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiCecEnable(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		result = (int)0x80010058;
	else if (avConfigIsHdmiAvailable() == 0)
		result = (int)0x803a0107;
	else if (SceSblAIMgrForDriver_71608CA3() == 0)
		result = (int)0x803a0109;
	else
		result = sceHdmiCecEnableCec();
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiCecDisable(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		result = (int)0x80010058;
	else if (avConfigIsHdmiAvailable() == 0)
		result = (int)0x803a0107;
	else if (SceSblAIMgrForDriver_71608CA3() == 0)
		result = (int)0x803a0109;
	else
		result = SceHdmiForDriver_291F000B();
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiCecCmdOneTouchPlay(int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		if (avConfigIsHdmiAvailable() == 0)
			result = (int)0x803a0107;
		else if (SceSblAIMgrForDriver_71608CA3() == 0)
			result = (int)0x803a0109;
		else if (mode == 2)
			result = sceHdmiCecCmdOneTouchPlay(mode);
		else
			result = (int)0x803a0101;
	} else {
		result = (int)0x80010058;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiSetResolution(int vic, int force)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		result = (int)0x80010058;
	else if (avConfigIsHdmiAvailable() == 0)
		result = (int)0x803a0107;
	else if (SceSblAIMgrForDriver_71608CA3() == 0)
		result = (int)0x803a0109;
	else
		result = hdmiSetOutput(vic, 3, 0x40000000 | (force == 1));
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiSetRgbRangeMode(unsigned int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		result = (int)0x80010058;
	else if (avConfigIsHdmiAvailable() == 0)
		result = (int)0x803a0107;
	else if (SceSblAIMgrForDriver_71608CA3() == 0)
		result = (int)0x803a0109;
	else
		result = hdmiSetOutput(-1, mode, 0x20000000);
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiSetOutScalingRatio(const AvConfigScalingRatio *ratio)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	if (ratio == NULL)
		return (int)0x803a0102;
	{
		AvConfigScalingRatio *value = __builtin_alloca(sizeof(*value));

		result = sceKernelCopyFromUser(value, ratio, sizeof(*value));
		if (result < 0)
			goto out;
		if (avConfigIsHdmiAvailable() != 0) {
			if (SceSblAIMgrForDriver_71608CA3() != 0)
				result = avConfigSetHdmiScalingRatio(value);
			else
				result = (int)0x803a0109;
		} else {
			result = (int)0x803a0107;
		}
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiGetOutScalingRatio(AvConfigScalingRatio *ratio)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	if (ratio == NULL)
		return (int)0x803a0102;
	{
		AvConfigScalingRatio *value = __builtin_alloca(sizeof(*value));

		result = sceKernelCopyFromUser(value, ratio, sizeof(*value));
		if (result < 0)
			goto out;
		if (avConfigIsHdmiAvailable() == 0) {
			result = (int)0x803a0107;
			goto out;
		}
		if (SceSblAIMgrForDriver_71608CA3() == 0) {
			result = (int)0x803a0109;
			goto out;
		}
		result = avConfigGetHdmiScalingRatio(value);
		if (result >= 0)
			result = sceKernelCopyToUser(ratio, value, sizeof(*value));
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiGetMonitorInfo(AvConfigMonitorInfo *info)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	if (info == NULL)
		return (int)0x803a0102;
	{
		AvConfigMonitorInfo *value = __builtin_alloca(sizeof(*value));

		result = sceKernelCopyFromUser(value, info, sizeof(*value));
		if (result < 0)
			goto out;
		if (avConfigIsHdmiAvailable() == 0) {
			result = (int)0x803a0107;
			goto out;
		}
		if (SceSblAIMgrForDriver_71608CA3() == 0) {
			result = (int)0x803a0109;
			goto out;
		}
		result = avConfigGetMonitorInfo(value);
		if (result >= 0)
			result = sceKernelCopyToUser(info, value, sizeof(*value));
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiGetCecInfo(CecStatistics *statistics)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1) {
		result = (int)0x80010058;
		goto out;
	}
	if (statistics == NULL)
		return (int)0x803a0102;
	{
		CecStatistics *value = __builtin_alloca(sizeof(*value));

		result = sceKernelCopyFromUser(value, statistics, sizeof(*value));
		if (result < 0)
			goto out;
		if (avConfigIsHdmiAvailable() == 0) {
			result = (int)0x803a0107;
			goto out;
		}
		if (SceSblAIMgrForDriver_71608CA3() == 0) {
			result = (int)0x803a0109;
			goto out;
		}
		result = avConfigGetCecInfo(value);
		if (result >= 0)
			result = sceKernelCopyToUser(statistics, value, sizeof(*value));
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigHdmiClearCecInfo(void)
{
	SceUInt32 syscall = sceSyscallEnter();
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		result = (int)0x80010058;
	else if (avConfigIsHdmiAvailable() == 0)
		result = (int)0x803a0107;
	else if (SceSblAIMgrForDriver_71608CA3() == 0)
		result = (int)0x803a0109;
	else
		result = avConfigClearCecInfo();
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetConnectedAudioDevice(SceUInt32 *devices)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceKernelIntrStatus intr;
	SceUInt32 connectedDevices;
	int result;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	connectedDevices = avConfigConfiguration.settings.connectedDevices;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	if (sceKernelCopyToUser(devices, &connectedDevices, sizeof(connectedDevices)) != 0)
		result = (int)0x803a0102;
	else
		result = 0;
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetShutterVol(void)
{
	return avConfigConfiguration.settings.shutterVolume;
}

int sceAVConfigSetMasterVol(unsigned int volume)
{
	SceUInt32 syscall;
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		return (int)0x80010058;
	syscall = sceSyscallEnter();
	if (SceSblAIMgrForDriver_71608CA3() != 0) {
		if (volume > 30) {
			result = (int)0x803a0103;
		} else {
			avConfigConfiguration.settings.masterVolume = volume;
			avConfigSetEventFlag(0x20);
			result = 0;
		}
	} else {
		result = (int)0x803a0109;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceAVConfigGetMasterVol(int *volume)
{
	int masterVolume = 0;
	SceUInt32 syscall;
	int result;

	if (sceSblACMgrIsSystemProgram(0) == 1) {
		syscall = sceSyscallEnter();
		if (SceSblAIMgrForDriver_71608CA3() != 0) {
			masterVolume = avConfigConfiguration.settings.masterVolume;
			if (sceKernelCopyToUser(volume, &masterVolume, sizeof(masterVolume)) != 0)
				result = (int)0x803a0102;
			else
				result = 0;
		} else {
			result = (int)0x803a0109;
		}
		sceSyscallLeave(syscall);
	} else {
		result = (int)0x80010058;
	}
	return result;
}

int sceAVConfigWriteMasterVol(void)
{
	SceUInt32 syscall;
	int result;

	if (sceSblACMgrIsSystemProgram(0) != 1)
		return (int)0x80010058;
	syscall = sceSyscallEnter();
	if (SceSblAIMgrForDriver_71608CA3() != 0)
		result = avConfigWriteMasterVolume(avConfigConfiguration.settings.masterVolume);
	else
		result = (int)0x803a0109;
	sceSyscallLeave(syscall);
	return result;
}

void avConfigEnableAvls(void)
{
	avConfigAvlsEnabled = 1;
}

void avConfigResetConfiguration(void)
{
	SceKernelIntrStatus intr;
	int suspendInterval;
	unsigned int brightness;
	int btMic, systemVolume, soundOutput, speakerMute;
	int dimmingInterval, colorSpaceMode, shutterVolume, masterVolume;

	avConfigDisplayStarted = 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	suspendInterval = avConfigConfiguration.settings.suspendInterval;
	brightness = avConfigConfiguration.settings.brightness;
	soundOutput = avConfigConfiguration.settings.soundOutput;
	btMic = avConfigConfiguration.settings.btMic;
	speakerMute = avConfigConfiguration.settings.speakerMute;
	dimmingInterval = avConfigConfiguration.settings.dimmingInterval;
	colorSpaceMode = avConfigConfiguration.settings.colorSpaceMode;
	shutterVolume = avConfigConfiguration.settings.shutterVolume;
	masterVolume = avConfigConfiguration.settings.masterVolume;
	systemVolume = avConfigConfiguration.settings.systemVolume;
	memset(&avConfigConfiguration, 0, offsetof(AvConfigConfiguration, settings));
	memset(&avConfigConfiguration.settings, 0, sizeof(avConfigConfiguration.settings));
	avConfigConfiguration.settings.suspendInterval = suspendInterval;
	avConfigConfiguration.settings.brightness = brightness;
	avConfigConfiguration.settings.btMic = btMic;
	avConfigConfiguration.settings.systemVolume = systemVolume;
	avConfigConfiguration.settings.soundOutput = soundOutput;
	avConfigConfiguration.settings.speakerMute = speakerMute;
	avConfigConfiguration.settings.dimmingInterval = dimmingInterval;
	avConfigConfiguration.settings.colorSpaceMode = colorSpaceMode;
	avConfigConfiguration.settings.shutterVolume = shutterVolume;
	avConfigConfiguration.settings.masterVolume = masterVolume;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
}

void avConfigRefreshAudioDevices(void)
{
	SceKernelIntrStatus intr;
	SceUInt32 devices;
	int online;
	int input;

	input = SceSblAIMgrForDriver_71608CA3();
	if (input != 0)
		input = 32;
	sceAudioInSelectInput(input);
	if (avConfigDisplayStarted == 1) {
		devices = SceSblAIMgrForDriver_71608CA3() == 0;
		if (SceHpremoteForDriver_71B5FB67() == 1)
			devices |= 4;
		if (SceHpremoteForDriver_2229EF51() == 1)
			devices |= 2;
	} else {
		devices = 0;
	}
	if (SceHpremoteForDriver_219C58F1() == 1)
		devices |= 0x400;
	if (SceSblAIMgrForDriver_71608CA3() == 0)
		devices |= 0x100;
	online = scePowerIsPowerOnline();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.acOnline = online != 0;
	avConfigConfiguration.settings.connectedDevices |= devices;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
}

void avConfigInitializeConfiguration(int enableAvls, int shutterVolume)
{
	SceKernelIntrStatus intr;
	SceUInt32 devices;
	int systemVolume, btMic, brightness, suspendInterval;
	int soundOutput, speakerMute, dimmingInterval, colorSpaceMode, masterVolume;
	int retries, input, online;

	memset(&avConfigConfiguration, 0, sizeof(avConfigConfiguration));
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigSystemVolumeCategory,
			avConfigSpeakerMuteKey, &speakerMute) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			speakerMute = 0;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigSystemVolumeCategory,
			avConfigSoundOutputKey, &soundOutput) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			soundOutput = 0;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigSystemVolumeCategory,
			avConfigSystemVolumeKey, &systemVolume) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			systemVolume = 21;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigBtCategory, avConfigBtMicKey, &btMic) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			btMic = 0;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigSystemVolumeCategory,
			avConfigAvlsKey, &avConfigAvlsEnabled) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			avConfigAvlsEnabled = 0;
			break;
		}
	}
	if (SceSblAIMgrForDriver_71608CA3() != 0) {
		brightness = ScePowerForDriver_D8759B55();
	} else {
		retries = 5;
		while (sceRegMgrGetKeyInt(avConfigDisplayCategory,
				avConfigBrightnessKey, &brightness) != 0) {
			sceKernelDelayThread(40000);
			if (--retries == 0) {
				brightness = 0x10000;
				goto readSuspendInterval;
			}
		}
		if (brightness <= 20) {
			brightness = 0x10000;
			sceRegMgrSetKeyInt(avConfigDisplayCategory,
				avConfigBrightnessKey, 0x10000);
		}
	}
readSuspendInterval:
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigPowerSavingCategory,
			avConfigSuspendIntervalKey, &suspendInterval) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			suspendInterval = 180;
			break;
		}
	}
	if (enableAvls == 1) {
		if (systemVolume > 21) {
			systemVolume = 21;
			retries = 5;
			while (sceRegMgrSetKeyInt(avConfigSystemVolumeCategory,
					avConfigSystemVolumeKey, systemVolume) != 0) {
				sceKernelDelayThread(40000);
				if (--retries == 0)
					break;
			}
		}
		if (avConfigAvlsEnabled == 0) {
			avConfigAvlsEnabled = 1;
			retries = 5;
			while (sceRegMgrSetKeyInt(avConfigSystemVolumeCategory,
					avConfigAvlsKey, avConfigAvlsEnabled) != 0) {
				sceKernelDelayThread(40000);
				if (--retries == 0)
					break;
			}
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigDisplayCategory,
			avConfigDimmingIntervalKey, &dimmingInterval) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			dimmingInterval = 900;
			break;
		}
	}
	retries = 5;
	while (sceRegMgrGetKeyInt(avConfigDisplayCategory,
			avConfigColorSpaceModeKey, &colorSpaceMode) != 0) {
		sceKernelDelayThread(40000);
		if (--retries == 0) {
			colorSpaceMode = 0;
			break;
		}
	}
	avConfigReadMasterVolume(&masterVolume);
	input = SceSblAIMgrForDriver_71608CA3();
	if (input != 0)
		input = 32;
	sceAudioInSelectInput(input);
	if (avConfigDisplayStarted == 1) {
		devices = SceSblAIMgrForDriver_71608CA3() == 0;
		if (SceHpremoteForDriver_71B5FB67() == 1)
			devices |= 4;
		if (SceHpremoteForDriver_2229EF51() == 1)
			devices |= 2;
	} else {
		devices = 0;
	}
	if (SceHpremoteForDriver_219C58F1() == 1)
		devices |= 0x400;
	if (SceSblAIMgrForDriver_71608CA3() == 0)
		devices |= 0x100;
	online = scePowerIsPowerOnline();
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.acOnline = online != 0;
	avConfigConfiguration.settings.connectedDevices = devices;
	avConfigConfiguration.settings.btMic = btMic;
	avConfigConfiguration.settings.suspendInterval = suspendInterval;
	avConfigConfiguration.settings.brightness = brightness;
	avConfigConfiguration.settings.shutterVolume = shutterVolume;
	avConfigConfiguration.settings.systemVolume = systemVolume;
	avConfigConfiguration.settings.soundOutput = soundOutput;
	avConfigConfiguration.settings.masterVolume = masterVolume;
	avConfigConfiguration.settings.speakerMute = speakerMute;
	avConfigConfiguration.settings.dimmingInterval = dimmingInterval;
	avConfigConfiguration.settings.colorSpaceMode = colorSpaceMode;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
}

void avConfigGetSettings(AvConfigSettings *settings)
{
	SceKernelIntrStatus intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	memcpy(settings, &avConfigConfiguration.settings, sizeof(*settings));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
}

void avConfigUpdateBtVolume(int notified, unsigned int volume)
{
	SceKernelIntrStatus intr;
	unsigned int converted;

	avConfigConvertBtVolume(volume, &converted);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.btVolumeNotified = notified;
	avConfigConfiguration.settings.btVolume = converted;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	if (notified == 1)
		avConfigNotifyCallback(2);
}

int SceAVConfigForDriver_0E60701A(unsigned int active)
{
	SceKernelIntrStatus intr;

	if (active > 1)
		return (int)0x803a0101;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.audioInputActive = active;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(0x1000);
	return 0;
}
int SceAVConfigForDriver_5662E799(unsigned int policy)
{
	SceKernelIntrStatus intr;

	if (policy > 2)
		return (int)0x803a0101;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.btAudioPolicy = policy;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(0x10000);
	return 0;
}

int SceAVConfigForDriver_2A2E9042(unsigned int index, unsigned int enabled,
		unsigned int mode, int blanked)
{
	AvConfigDisplayRequest *request;
	SceKernelIntrStatus intr;
	int pending;

	if (index > 15 || enabled > 1 || mode > 4)
		return (int)0x803a0101;
	if (index != 0 &&
		(avConfigConfiguration.displayRequests[0].active == 1 ||
		(index > 1 && (avConfigConfiguration.displayRequests[1].active == 1 ||
		(index > 2 && (avConfigConfiguration.displayRequests[2].active == 1 ||
		(index > 3 && (avConfigConfiguration.displayRequests[3].active == 1 ||
		(index > 4 && (avConfigConfiguration.displayRequests[4].active == 1 ||
		(index > 5 && (avConfigConfiguration.displayRequests[5].active == 1 ||
		(index > 6 && (avConfigConfiguration.displayRequests[6].active == 1 ||
		(index > 7 && (avConfigConfiguration.displayRequests[7].active == 1 ||
		(index > 8 && (avConfigConfiguration.displayRequests[8].active == 1 ||
		(index > 9 && (avConfigConfiguration.displayRequests[9].active == 1 ||
		(index > 10 && (avConfigConfiguration.displayRequests[10].active == 1 ||
		(index > 11 && (avConfigConfiguration.displayRequests[11].active == 1 ||
		(index > 12 && (avConfigConfiguration.displayRequests[12].active == 1 ||
		(index > 13 && (avConfigConfiguration.displayRequests[13].active == 1 ||
		(index > 14 && avConfigConfiguration.displayRequests[14].active == 1)
		))))))))))))))))))))))))))))
		goto queued;
	request = &avConfigConfiguration.displayRequests[index];
	if (request->active != 1) {
		request->active = 1;
		request->enabled = enabled;
		request->mode = mode;
		request->blanked = blanked;
		avConfigConfiguration.displayRequestCount++;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		avConfigConfiguration.settings.displayEnabled = enabled;
		avConfigConfiguration.settings.displayMode = mode;
		avConfigConfiguration.settings.displayBlanked = blanked == 1;
		avConfigConfiguration.settings.displayUpdatePending = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		avConfigSetEventFlag(0x100000);
		do {
			sceKernelDelayThread(40000);
			intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
			pending = avConfigConfiguration.settings.displayUpdatePending;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		} while (pending != 0);
	}
	return 0;
queued:
	request = &avConfigConfiguration.displayRequests[index];
	if (request->active != 1) {
		request->active = 1;
		avConfigConfiguration.displayRequestCount++;
	}
	request->blanked = blanked;
	request->enabled = enabled;
	request->mode = mode;
	return 0;
}

int SceAVConfigForDriver_3817CDA1(unsigned int index, unsigned int enabled,
		unsigned int mode)
{
	return SceAVConfigForDriver_2A2E9042(index, enabled, mode, 1);
}
int SceAVConfigForDriver_91792D6F(unsigned int index)
{
	AvConfigDisplayRequest *request;
	SceKernelIntrStatus intr;
	unsigned int priority;
	SceUInt32 event;
	int count, pending;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	request = &avConfigConfiguration.displayRequests[index];
	if (request->active != 1)
		goto invalid;
	count = avConfigConfiguration.displayRequestCount - 1;
	request->active = 0;
	avConfigConfiguration.displayRequestCount = count;
	if (count != 0) {
		if (index == 0) {
			priority = 1;
			goto selectNext;
		}
		if (avConfigConfiguration.displayRequests[0].active == 1)
			goto unchanged;
		if (index <= 1)
			goto next;
		if (avConfigConfiguration.displayRequests[1].active == 1)
			goto unchanged;
		if (index <= 2)
			goto next;
		if (avConfigConfiguration.displayRequests[2].active == 1)
			goto unchanged;
		if (index <= 3)
			goto next;
		if (avConfigConfiguration.displayRequests[3].active == 1)
			goto unchanged;
		if (index <= 4)
			goto next;
		if (avConfigConfiguration.displayRequests[4].active == 1)
			goto unchanged;
		if (index <= 5)
			goto next;
		if (avConfigConfiguration.displayRequests[5].active == 1)
			goto unchanged;
		if (index <= 6)
			goto next;
		if (avConfigConfiguration.displayRequests[6].active == 1)
			goto unchanged;
		if (index <= 7)
			goto next;
		if (avConfigConfiguration.displayRequests[7].active == 1)
			goto unchanged;
		if (index <= 8)
			goto next;
		if (avConfigConfiguration.displayRequests[8].active == 1)
			goto unchanged;
		if (index <= 9)
			goto next;
		if (avConfigConfiguration.displayRequests[9].active == 1)
			goto unchanged;
		if (index <= 10)
			goto next;
		if (avConfigConfiguration.displayRequests[10].active == 1)
			goto unchanged;
		if (index <= 11)
			goto next;
		if (avConfigConfiguration.displayRequests[11].active == 1)
			goto unchanged;
		if (index <= 12)
			goto next;
		if (avConfigConfiguration.displayRequests[12].active == 1)
			goto unchanged;
		if (index <= 13)
			goto next;
		if (avConfigConfiguration.displayRequests[13].active == 1)
			goto unchanged;
		if (index <= 14)
			goto next;
		if (avConfigConfiguration.displayRequests[14].active == 1)
			goto unchanged;
		if (index <= 15)
			goto next;
		if (avConfigConfiguration.displayRequests[15].active == 1)
			goto unchanged;
		goto invalid;
next:
		priority = index + 1;
		if (priority == 16)
			goto invalid;
selectNext:
		request = &avConfigConfiguration.displayRequests[priority];
		if (request->active == 1)
			goto selected;
		if (priority + 1 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 1];
		if (request->active == 1)
			goto selected;
		if (priority + 2 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 2];
		if (request->active == 1)
			goto selected;
		if (priority + 3 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 3];
		if (request->active == 1)
			goto selected;
		if (priority + 4 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 4];
		if (request->active == 1)
			goto selected;
		if (priority + 5 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 5];
		if (request->active == 1)
			goto selected;
		if (priority + 6 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 6];
		if (request->active == 1)
			goto selected;
		if (priority + 7 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 7];
		if (request->active == 1)
			goto selected;
		if (priority + 8 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 8];
		if (request->active == 1)
			goto selected;
		if (priority + 9 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 9];
		if (request->active == 1)
			goto selected;
		if (priority + 10 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 10];
		if (request->active == 1)
			goto selected;
		if (priority + 11 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 11];
		if (request->active == 1)
			goto selected;
		if (priority + 12 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 12];
		if (request->active == 1)
			goto selected;
		if (priority + 13 == 16)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[priority + 13];
		if (request->active == 1)
			goto selected;
		if (priority != 1 || avConfigConfiguration.displayRequests[15].active != 1)
			goto invalid;
		request = &avConfigConfiguration.displayRequests[15];
selected:
		avConfigConfiguration.settings.displayEnabled = request->enabled;
		avConfigConfiguration.settings.displayMode = request->mode;
		avConfigConfiguration.settings.displayBlanked = request->blanked == 1;
		avConfigConfiguration.settings.displayUpdatePending = 1;
		event = 0x100000;
	} else {
		avConfigConfiguration.settings.displayEnabled = 0;
		avConfigConfiguration.settings.displayMode = 0;
		avConfigConfiguration.settings.displayBlanked = 0;
		avConfigConfiguration.settings.displayUpdatePending = 1;
		event = 0x200000;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(event);
	do {
		sceKernelDelayThread(40000);
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		pending = avConfigConfiguration.settings.displayUpdatePending;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	} while (pending != 0);
	return 0;
unchanged:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	return 0;
invalid:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	return (int)0x803a0101;
}
int SceAVConfigForDriver_D8B7AE3D(unsigned int index, int blanked)
{
	AvConfigDisplayRequest *request;
	SceKernelIntrStatus intr;

	request = &avConfigConfiguration.displayRequests[index];
	if (request->active != 1)
		return (int)0x803a0101;
	request->blanked = blanked;
	if (index == 0)
		goto update;
	if (avConfigConfiguration.displayRequests[0].active == 1)
		return 0;
	if (index <= 1)
		goto update;
	if (avConfigConfiguration.displayRequests[1].active == 1)
		return 0;
	if (index <= 2)
		goto update;
	if (avConfigConfiguration.displayRequests[2].active == 1)
		return 0;
	if (index <= 3)
		goto update;
	if (avConfigConfiguration.displayRequests[3].active == 1)
		return 0;
	if (index <= 4)
		goto update;
	if (avConfigConfiguration.displayRequests[4].active == 1)
		return 0;
	if (index <= 5)
		goto update;
	if (avConfigConfiguration.displayRequests[5].active == 1)
		return 0;
	if (index <= 6)
		goto update;
	if (avConfigConfiguration.displayRequests[6].active == 1)
		return 0;
	if (index <= 7)
		goto update;
	if (avConfigConfiguration.displayRequests[7].active == 1)
		return 0;
	if (index <= 8)
		goto update;
	if (avConfigConfiguration.displayRequests[8].active == 1)
		return 0;
	if (index <= 9)
		goto update;
	if (avConfigConfiguration.displayRequests[9].active == 1)
		return 0;
	if (index <= 10)
		goto update;
	if (avConfigConfiguration.displayRequests[10].active == 1)
		return 0;
	if (index <= 11)
		goto update;
	if (avConfigConfiguration.displayRequests[11].active == 1)
		return 0;
	if (index <= 12)
		goto update;
	if (avConfigConfiguration.displayRequests[12].active == 1)
		return 0;
	if (index <= 13)
		goto update;
	if (avConfigConfiguration.displayRequests[13].active == 1)
		return 0;
	if (index <= 14)
		goto update;
	if (avConfigConfiguration.displayRequests[14].active == 1)
		return 0;
	if (index <= 15)
		goto update;
	if (avConfigConfiguration.displayRequests[15].active == 1)
		return 0;
update:
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.displayBlanked = blanked == 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
	avConfigSetEventFlag(0x100000);
	return 0;
}
void avConfigCompleteDisplayUpdate(void)
{
	SceKernelIntrStatus intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
	avConfigConfiguration.settings.displayUpdatePending = 0;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
}

void avConfigHpremoteCallback(int input, int multiType, int headphones,
		int remoteStatus, int microphone, void *argument)
{
	SceKernelIntrStatus intr;
	SceUInt32 devices;

	(void)input;
	(void)remoteStatus;
	(void)argument;
	if (avConfigAudioDevicesEnabled == 1 && avConfigDisplayStarted == 1) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);
		devices = avConfigConfiguration.settings.connectedDevices;
		if (headphones == 1) {
			if (microphone == 1)
				devices |= 0x400;
			devices |= 4;
		} else {
			devices &= ~0x404u;
		}
		if (multiType == 1)
			devices |= 2;
		else
			devices &= ~2u;
		avConfigConfiguration.settings.connectedDevices = devices;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, intr);
		avConfigSetEventFlag(0x10);
	}
}

void avConfigEnableAudioDevices(int enabled)
{
	avConfigAudioDevicesEnabled = enabled;
}

void avConfigHdmiConnected(void)
{
	int state = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);

	if (avConfigGetHdmiAudioMode() != 0)
		avConfigConfiguration.settings.connectedDevices |= 8;
	avConfigConfiguration.settings.hdcpState = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
	avConfigSetEventFlag(0x100);
}

void avConfigHdmiDisconnected(void)
{
	int state = sceKernelSpinlockLowLockCpuSuspendIntr(&avConfigConfiguration.lock);

	avConfigConfiguration.settings.hdcpState = 0;
	avConfigConfiguration.settings.connectedDevices &= ~8u;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&avConfigConfiguration.lock, state);
	avConfigSetEventFlag(0x200);
}

