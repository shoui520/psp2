#include "spkg_peripherals.h"

int SysconUtilCheckFirmwareType(uint64_t hardwareInfo, int kind)
{
	int result;
	uint32_t hardwareType = (uint32_t)(hardwareInfo >> 32) & 0xffffff00u;

	result = ((SceSysconForDriver_CBD6D8BC() & 0xffffff00u) ==
		hardwareType) ? 0 : (int)0x800f0202;
	if (kind && updateSysconDownloader)
		return (int)0x800f0202;
	return result;
}
