#include "update_internal.h"

extern int updateCpupContext, updateCpupUpdateTriggered;
int SceDeci4pCpupForDriver_BBB991CD(uint32_t commandId);
int SceDeci4pCpupForDriver_2BAD7B39(int context);
int SpkgUtilCpReboot(void);

int SpkgUtilCpShutdown(int mode)
{
	if (mode) {
		if (mode == 1)
			return SpkgUtilCpReboot();
		return (int)0x800f0216;
	}
	if (updateCpupConnected && updateCpupUpdateTriggered) {
		if (updateUseCpupExtendedService)
			SceDeci4pCpupForDriver_BBB991CD(6);
		SceDeci4pCpupForDriver_2BAD7B39(updateCpupContext);
		return 0;
	}
	if (updateUseCpupService) {
		if (updateUseCpupExtendedService)
			SceDeci4pCpupForDriver_BBB991CD(6);
		SceDeci4pCpupForDriver_BBB991CD(2);
		return 0;
	}
	return updateUseCpupService;
}
