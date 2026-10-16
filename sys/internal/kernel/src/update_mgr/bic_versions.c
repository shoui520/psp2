#include "update_internal.h"

int BicUtilGetFirmwareVersion(uint32_t *version)
{
	if (!updateUseBatteryService)
		return (int)0x800f0202;
	*version = updateBatteryFirmwareVersion;
	return 0;
}

int BicUtilGetDataFlashVersion(uint32_t *version)
{
	if (!updateUseBatteryService)
		return (int)0x800f0202;
	*version = updateBatteryDataFlashVersion;
	return 0;
}

int BicUtilIsDisconnected(void)
{
	return updateBatteryHardwareInfo == 0xffff;
}

int BicUtilIsUnsupportedHardware(void)
{
	unsigned int hardwareInfo = updateBatteryHardwareInfo;

	return (hardwareInfo & 0xff00u) == 0xff00u &&
		hardwareInfo - 0xfffcu > 2;
}
