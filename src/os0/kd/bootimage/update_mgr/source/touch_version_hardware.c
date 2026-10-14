#include "spkg_peripherals.h"

uint32_t TouchUtilGetFirmwareVersionByHardware(uint64_t hardwareInfo)
{
	uint32_t version = 0;

	if (TouchUtilGetFirmwareVersion((uint32_t)hardwareInfo & 15, &version) < 0)
		return 0;
	return version;
}
