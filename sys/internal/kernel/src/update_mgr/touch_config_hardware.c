#include "spkg_peripherals.h"

uint32_t TouchUtilGetConfigVersionByHardware(uint64_t hardwareInfo)
{
	unsigned int port = (uint32_t)hardwareInfo & 15;
	unsigned int hardware, version;

	if (port == 0) {
		hardware = updateSysconTouchDeviceInfo[0];
		version = updateSysconTouchDeviceInfo[1];
		if ((hardware & 0xf000) == 0x8000)
			goto shiftedVersion;
	} else if (port == 1) {
		hardware = updateSysconTouchDeviceInfo[2];
		version = updateSysconTouchDeviceInfo[3];
		if ((hardware & 0xf000) == 0x8000)
			goto shiftedVersion;
	} else {
		return 0;
	}

checkRecovery:
	return (hardware & 0xc0c0) == 0xc0c0 ? 0 : version;
shiftedVersion:
	version >>= 8;
	goto checkRecovery;
}
