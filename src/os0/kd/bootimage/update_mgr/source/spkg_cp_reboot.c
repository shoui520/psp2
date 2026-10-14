#include "update_internal.h"

extern int updateCpupContext, updateCpupUpdateTriggered;
int SceDeci4pCpupForDriver_BBB991CD(uint32_t commandId);
int SceDeci4pCpupForDriver_2BAD7B39(int context);

int SpkgUtilCpReboot(void)
{
	if (updateCpupConnected && updateCpupUpdateTriggered) {
		SceDeci4pCpupForDriver_BBB991CD(5);
		if (updateUseCpupExtendedService)
			SceDeci4pCpupForDriver_BBB991CD(6);
		SceDeci4pCpupForDriver_2BAD7B39(updateCpupContext);
		return 0;
	}
	if (updateUseCpupPowerService) {
		if (updateUseCpupExtendedService)
			SceDeci4pCpupForDriver_BBB991CD(6);
		SceDeci4pCpupForDriver_BBB991CD(4);
		return 0;
	}
	if (!updateUseCpupService)
		return 0;
	if (updateUseCpupExtendedService)
		SceDeci4pCpupForDriver_BBB991CD(6);
	SceDeci4pCpupForDriver_BBB991CD(2);
	return 0;
}
