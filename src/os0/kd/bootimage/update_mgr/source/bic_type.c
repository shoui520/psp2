#include "spkg_peripherals.h"

int BicUtilCheckFirmwareType(uint64_t hardwareInfo)
{
	return BicUtilCheckFirmwareTypeCommon(hardwareInfo, 0);
}

int BicUtilCheckDataFlashType(uint64_t hardwareInfo)
{
	return BicUtilCheckFirmwareTypeCommon(hardwareInfo, 1);
}
