#include "update_internal.h"
#include <deci4p_cpup/cpup.h>
#include "spkg.h"

extern int updateCpupContext, updateCpupUpdateTriggered;

int SpkgUtilCpShutdown(int mode)
{
	if (mode) {
		if (mode == 1)
			return SpkgUtilCpReboot();
		return (int)0x800f0216;
	}
	if (updateCpupConnected && updateCpupUpdateTriggered) {
		if (updateUseCpupExtendedService)
			sceDeci4pCpupExecCommand(6);
		SceDeci4pCpupForDriver_2BAD7B39(updateCpupContext);
		return 0;
	}
	if (updateUseCpupService) {
		if (updateUseCpupExtendedService)
			sceDeci4pCpupExecCommand(6);
		sceDeci4pCpupExecCommand(2);
		return 0;
	}
	return updateUseCpupService;
}
