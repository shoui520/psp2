#include "update_internal.h"
#include "spkg_peripherals.h"

int MotionUtilCheckFirmwareType(uint64_t hardwareInfo)
{
	if (updateSysconDownloader || !updateMotionConnected)
		return (int)0x800f0202;
	if ((((unsigned int)(hardwareInfo >> 32) ^
	      updateMotionHardwareType) & 0xffu) == 0)
		return 0;
	return (int)0x800f0202;
}
