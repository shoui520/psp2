#include "spkg_peripherals.h"
#include "touch_update.h"
#include "snvs.h"

extern uint8_t updateSnvsFailureStage;

int TouchUtilCheckFirmwareType(uint64_t hardwareInfo, unsigned int kind)
{
	uint16_t panelHardware;
	uint16_t storedHardware = 0;
	uint16_t storedVersion = 0;
	uint16_t panelVersion = 0;
	uint8_t flags = 0;
	unsigned int platform = (uint32_t)hardwareInfo & 0xffffff00;
	unsigned int requested = hardwareInfo >> 32;
	unsigned int port, currentPlatform;
	unsigned int versionHigh, versionLow;
	unsigned int failure;

	if (updateSysconDownloader || !updateUseTouchService || kind > 1)
		return (int)0x800f0202;
	currentPlatform = SceSysconForDriver_CBD6D8BC();
	if (platform && (currentPlatform & 0xffffff00) != platform)
		return (int)0x800f0202;
	port = (uint32_t)hardwareInfo & 15;
	if (port == 0)
		panelHardware = updateSysconTouchDeviceInfo[0];
	else if (port == 1)
		panelHardware = updateSysconTouchDeviceInfo[2];
	else
		return (int)0x800f0202;
	versionLow = updateTouchDeviceInfo[port].detail[1];
	if (!versionLow)
		versionLow = 100;
	versionHigh = updateTouchDeviceInfo[port].detail[0];
	if (!versionHigh)
		versionHigh = 100;
	panelVersion = (versionHigh << 8) | versionLow;
	if (SpkgInfoUtilGetSNVSValue(9, 2, port, &storedHardware, 2, 1, -1)) {
		failure = 0x0a001016;
		goto snvsFailure;
	}
	if (SpkgInfoUtilGetSNVSValue(9, 3, port, &storedVersion, 2, 1, -1)) {
		failure = 0x0a001017;
		goto snvsFailure;
	}
	if ((panelHardware & 0xc0c0) != 0xc0c0 && kind == 1) {
		if (TouchUtilGetConfigUpdateFlags(port, panelHardware, &flags)) {
			updateLastResult = (port << 16) | 0x0a002011;
			return (int)0x800f0205;
		}
	}
	if ((panelHardware & 0xc0c0) == 0xc0c0 || (flags & 1)) {
		updateTouchRecoveryMode = 1;
		panelHardware = storedHardware;
		versionHigh = storedVersion >> 8;
		versionLow = (uint8_t)storedVersion;
	} else {
		if ((uint16_t)(panelHardware - 1) > 0xfffd)
			return (int)0x800f0202;
		if (storedHardware != panelHardware || storedVersion != panelVersion) {
			if (SpkgInfoUtilSetSNVSValue(9, 2, port, &panelHardware,
				2, 1, -1)) {
				failure = 0x0a001018;
				goto snvsFailure;
			}
			if (SpkgInfoUtilSetSNVSValue(9, 3, port, &panelVersion,
				2, 1, -1)) {
				failure = 0x0a001019;
				goto snvsFailure;
			}
		}
	}
	if (panelHardware != requested >> 16)
		return (int)0x800f0202;
	return versionHigh == (uint8_t)requested &&
		versionLow == (uint16_t)(requested & 0xff00) >> 8 ?
		0 : (int)0x800f0202;

snvsFailure:
	updateLastResult = failure | (updateSnvsFailureStage << 8) | (port << 16);
	return (int)0x800f0205;
}
