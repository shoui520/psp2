#include <sysmem/suspend.h>
#include <lowio/gpio.h>
#include "update_internal.h"
#include <syscon/syscon.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/dipsw.h>
#include <power/power.h>
#include "spkg.h"

extern int updateSysconShutdownWorkaround;
extern int updateSysconFirmwareUpdateStarted;
int SceDeci4pCpupForDriver_BBB991CD(uint32_t commandId);
int SceLedForDriver_EA24BE03(int led, int mode, void *param) __attribute__((weak));

int SpkgUtilPowerControl(int operation, int flags)
{
	int result;
	int (*ledControl)(int kind, int enabled, void *argument) = SceLedForDriver_EA24BE03;

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
			ScePowerForDriver_8921A7A0(1);
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
			ScePowerForDriver_8921A7A0(0);
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
			SceSysconForDriver_04EC7579(64, 1);
			SceGpioForDriver_D454A584(0, 7);
			result = 0;
		}
		break;
	case 5:
		if (ledControl) {
			result = ledControl(2, 0, 0);
		} else {
			SceSysconForDriver_04EC7579(64, 0);
			SceGpioForDriver_F6310435(0, 7);
			result = 0;
		}
		break;
	case 6:
		result = SceGpioForDriver_372022A4(0, 7, 0);
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
