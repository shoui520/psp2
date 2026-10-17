#include <psp2kern/power.h>
#include <sysmem/suspend.h>
#include <psp2kern/lowio/gpio.h>
#include "update_internal.h"
#include <syscon/syscon.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/dipsw.h>
#include <power/power.h>

extern int updateSysconShutdownWorkaround;
extern int updateSysconFirmwareUpdateStarted;
int UpdateUtilLockPupHeader(void);
int UpdateUtilUnlockPupHeader(void);
int SpkgUtilCpReboot(void);
int SpkgUtilCpShutdown(int mode);
int SceDeci4pCpupForDriver_BBB991CD(uint32_t commandId);
int ksceLedSetMode(int led, int mode, void *param) __attribute__((weak));

int SpkgUtilPowerControl(int operation, int flags)
{
	int result;
	int (*ledControl)(int kind, int enabled, void *argument) = ksceLedSetMode;

	UpdateUtilLockPupHeader();
	switch (operation) {
	case 0:
		result = (int)0x800f0216;
		if (flags & 2)
			result = SpkgUtilCpReboot();
		if (!(flags & 5))
			break;
		if (updateCpupConnected) {
			result = SpkgUtilCpReboot();
			break;
		}
		result = 0;
		if (!updateUseSysconPowerControl)
			break;
		if (updateSysconFirmwareUpdateStarted && !updateSysconShutdownWorkaround)
			kscePowerRequestShutdown(1);
		else
			scePowerRequestColdReset();
		break;
	case 1:
		result = (int)0x800f0216;
		if (flags & 2)
			result = SpkgUtilCpShutdown(0);
		if (!(flags & 5))
			break;
		if (updateCpupConnected) {
			result = SpkgUtilCpShutdown(0);
			break;
		}
		result = 0;
		if (!updateUseSysconPowerControl)
			break;
		if (updateSysconFirmwareUpdateStarted && !updateSysconShutdownWorkaround)
			kscePowerRequestShutdown(0);
		else
			scePowerRequestStandby();
		break;
	case 2:
		result = updateUseCpupService;
		if (result)
			result = SceDeci4pCpupForDriver_BBB991CD(0);
		if (flags & 1)
			result = SceKernelSuspendForDriver_4DF40893(0);
		break;
	case 3:
		result = updateUseCpupService;
		if (result)
			result = SceDeci4pCpupForDriver_BBB991CD(1);
		if (flags & 1)
			result = SceKernelSuspendForDriver_2BB92967(0);
		break;
	case 4:
		if (ledControl) {
			result = ledControl(2, 1, 0);
		} else {
			ksceSysconCtrlLED(64, 1);
			ksceGpioPortSet(0, 7);
			result = 0;
		}
		break;
	case 5:
		if (ledControl) {
			result = ledControl(2, 0, 0);
		} else {
			ksceSysconCtrlLED(64, 0);
			ksceGpioPortClear(0, 7);
			result = 0;
		}
		break;
	case 6:
		result = ksceGpioSetPortMode(0, 7, 0);
		break;
	case 7:
		result = SceKernelSuspendForDriver_E0489831(flags);
		break;
	case 8:
		result = 0;
		if (!SceSblAIMgrForDriver_274663A0() && !SceSblAIMgrForDriver_3B638885())
			break;
		result = (int)0x800f0233;
		if (SceDipswForDriver_A98FC2FD(194))
			result = 0;
		break;
	default:
		UpdateUtilUnlockPupHeader();
		return (int)0x800f0216;
	}
	UpdateUtilUnlockPupHeader();
	return result;
}
