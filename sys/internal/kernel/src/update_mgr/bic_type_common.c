#include "spkg_peripherals.h"
#include "snvs.h"

extern int updateSysconDownloader;
extern unsigned int updateSysconHardwareInfo;

int BicUtilCheckFirmwareTypeCommon(uint64_t hardwareInfo, unsigned int kind)
{
	uint32_t firmwareVersion = updateBatteryFirmwareVersion;
	uint32_t batteryHardwareInfo = updateBatteryHardwareInfo;
	uint32_t storedHardwareInfo = 0, storedFirmwareVersion = 0;
	uint32_t batteryType = (uint32_t)(hardwareInfo >> 32);
	uint32_t requestedHardwareInfo;
	int downloader = updateSysconDownloader;

	if (downloader)
		return (int)0x800f0202;
	if (kind > 1)
		return (int)0x800f0216;
	requestedHardwareInfo = batteryType >> 16;
	if (SpkgInfoUtilGetSNVSValue(10, 2, downloader, &storedHardwareInfo,
			sizeof(storedHardwareInfo), 1, -1) ||
	    SpkgInfoUtilGetSNVSValue(10, 0, 0, &storedFirmwareVersion,
			sizeof(storedFirmwareVersion), 1, -1))
		return (int)0x800f0205;
	if (batteryHardwareInfo != 0xfffe) {
		if ((batteryHardwareInfo != storedHardwareInfo &&
		     SpkgInfoUtilSetSNVSValue(10, 2, 0, &batteryHardwareInfo,
				sizeof(batteryHardwareInfo), 1, -1)) ||
		    (firmwareVersion != storedFirmwareVersion &&
		     SpkgInfoUtilSetSNVSValue(10, 0, 0, &firmwareVersion,
				sizeof(firmwareVersion), 1, -1)))
			return (int)0x800f0205;
	} else {
		batteryHardwareInfo = storedHardwareInfo;
		firmwareVersion = storedFirmwareVersion;
	}
	if (requestedHardwareInfo != batteryHardwareInfo)
		return (int)0x800f0202;
	if ((updateSysconHardwareInfo & 0xffffff00u) !=
	    ((uint32_t)hardwareInfo & 0xffffff00u))
		return (int)0x800f0202;
	if (kind && firmwareVersion != (uint16_t)batteryType)
		return (int)0x800f0202;
	return 0;
}
