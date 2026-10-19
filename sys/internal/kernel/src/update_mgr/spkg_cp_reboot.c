#include "update_internal.h"
#include <deci4p_cpup/cpup.h>

extern int updateCpupContext, updateCpupUpdateTriggered;

int SpkgUtilCpReboot(void)
{
	if (updateCpupConnected && updateCpupUpdateTriggered) {
		sceDeci4pCpupExecCommand(5);
		if (updateUseCpupExtendedService)
			sceDeci4pCpupExecCommand(6);
		SceDeci4pCpupForDriver_2BAD7B39(updateCpupContext);
		return 0;
	}
	if (updateUseCpupPowerService) {
		if (updateUseCpupExtendedService)
			sceDeci4pCpupExecCommand(6);
		sceDeci4pCpupExecCommand(4);
		return 0;
	}
	if (!updateUseCpupService)
		return 0;
	if (updateUseCpupExtendedService)
		sceDeci4pCpupExecCommand(6);
	sceDeci4pCpupExecCommand(2);
	return 0;
}
