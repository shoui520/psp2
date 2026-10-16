#include "spkg_peripherals.h"

int SysconUtilGetFirmwareVersion(uint32_t *version)
{
	if (!updateUseMotionService)
		return (int)0x800f0202;
	*version = updateSysconFirmwareVersion;
	return 0;
}

int SysconUtilGetDownloaderVersion(uint32_t *version)
{
	if (!updateUseSysconUpdateService)
		return (int)0x800f0202;
	*version = updateSysconDownloaderVersion & 0xffff00;
	return 0;
}
