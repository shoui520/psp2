#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/sysroot.h>
#include <syscon/syscon.h>
#include "update_internal.h"

int ksceSblAimgrIsGenuineDolce(void);
int ksceMotionDevIsReady(void);
int SceTouchForDriver_63DCF2FE(void);
int SceDeci4pCpupForDriver_C0AB956B(unsigned int *protocol,
				 unsigned int *board, unsigned int *version)
	__attribute__((weak));

int sceSblUpdateMgrInitPeripheralsPhase2(SceSize arglen, void *argp)
{
	unsigned int hardware, firmware;
	union {
		unsigned int dataFlashVersion;
		uint16_t touchDeviceInfo[4];
	} information;
	struct touchDeviceInfo touchInfo[2];
	uint32_t status, flags;
	int (*useCpup)(unsigned int *, unsigned int *, unsigned int *);
	int result;

	if (updateUseSysconNvsService && ksceSysconNvsSetRunMode(0) == 0) {
		if (updateUseSysconSnvs)
			result = SpkgInfoUtilGetSNVSFlagStatus(&status, &flags, 1, -1);
		else
			result = SpkgInfoUtilGetNVSFlagStatus(&status, &flags);
		if (result == 0 && (flags & 1))
			SpkgInfoUtilInitForUpdater(&status, &flags);
	}
	hardware = 0;
	firmware = 0;
	information.dataFlashVersion = 0;
	useCpup = SceDeci4pCpupForDriver_C0AB956B;
	if (useCpup) {
		if (SceDeci4pCpupForDriver_C0AB956B(&hardware, &firmware,
						 &information.dataFlashVersion) != 0) {
			updateCpupConnected = 0;
			updateUseCpupService = 0;
			updateUseCpupPowerService = 0;
			updateUseCpupExtendedService = 0;
		} else {
			updateCpupConnected = 1;
			updateCpupBoardVersion = firmware;
			updateCpupProtocolVersion = hardware;
			if (hardware > 0x10001ff) {
				updateUseCpupService = 1;
				if (hardware > 0x10002ff) {
					updateUseCpupPowerService = 1;
					if (hardware > 0x10006ff)
						updateUseCpupExtendedService = 1;
				}
			}
		}
	}
	if (!ksceSblAimgrIsGenuineDolce() && updateMotionPollCount > 0) {
		do {
			if (ksceMotionDevIsReady()) {
				if (ksceMotionDevGetDeviceInfo(&updateMotionDeviceInfo) == 0 &&
				    updateUseMotionService) {
					updateMotionFirmwareVersion = updateMotionDeviceInfo.firmwareVersion;
					updateMotionConnected = 1;
					updateMotionHardwareType = updateMotionDeviceInfo.hardwareInfo >> 8;
					break;
				}
			} else {
				sceKernelDelayThread(100000);
			}
		} while (--updateMotionPollCount > 0);
	}
	hardware = 0;
	firmware = 0;
	information.dataFlashVersion = 0;
	if (!ksceSblAimgrIsGenuineDolce()) {
		if (ksceSysconGetBatteryVersion(&hardware, &firmware,
					      &information.dataFlashVersion) != 0) {
			updateUseBatteryService = 0;
		} else {
			updateBatteryFirmwareVersion = firmware;
			updateBatteryHardwareInfo = hardware;
			updateBatteryDataFlashVersion = information.dataFlashVersion;
			if (updateSysconFirmwareVersion <= 0x70600 ||
			    BicUtilIsDisconnected() || BicUtilIsUnsupportedHardware())
				updateUseBatteryService = 0;
			else
				updateUseBatteryService = 1;
		}
	}
	if (!ksceSblAimgrIsGenuineDolce() && updateTouchPollCount > 0) {
		while (!SceTouchForDriver_63DCF2FE()) {
			sceKernelDelayThread(20000);
			if (--updateTouchPollCount <= 0)
				goto finish;
		}
		if (ksceSysconGetTouchpanelDeviceInfo(information.touchDeviceInfo) != 0) {
			updateUseTouchService = 0;
		} else {
			__builtin_memcpy(updateSysconTouchDeviceInfo,
					 information.touchDeviceInfo,
					 sizeof(updateSysconTouchDeviceInfo));
			if (updateSysconFirmwareVersion > 0x90205) {
				updateUseTouchService = 1;
				TouchUtilGetFirmwareVersion(0, &firmware);
				TouchUtilGetFirmwareVersion(1, &firmware);
			} else {
				updateUseTouchService = 0;
			}
		}
		__builtin_memset(updateTouchDeviceInfo, 0, sizeof(updateTouchDeviceInfo));
		ksceTouchGetDeviceInfo(0, &touchInfo[0]);
		ksceTouchGetDeviceInfo(1, &touchInfo[1]);
		__builtin_memcpy(updateTouchDeviceInfo, touchInfo,
				 sizeof(updateTouchDeviceInfo));
	}
finish:
	SpkgInfoUtilInitSystemSwVersion();
	updateInitialized = 1;
	ksceKernelSysrootSetGetSystemSwVersionFunc(SpkgInfoUtilGetSystemSwVersion);
	result = updateInitializeAsync;
	if (result)
		result = sceKernelExitDeleteThread(0);
	return result;
}
