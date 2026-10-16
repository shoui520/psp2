#include "spkg_peripherals.h"

int TouchUtilRequiresNewerFirmware(uint64_t hardwareInfo)
{
	unsigned int port = (uint32_t)hardwareInfo & 15;

	if (port == 0)
		return updateTouchDeviceInfo[0].detail[0] > 143;
	if (port == 1)
		return updateTouchDeviceInfo[1].detail[0] > 143;
	return 0;
}
