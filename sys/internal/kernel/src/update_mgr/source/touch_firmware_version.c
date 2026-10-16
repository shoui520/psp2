#include "update_internal.h"
#include "touch_update.h"
#include "touch_dispatch.h"

int TouchUtilGetFirmwareVersion(unsigned int port, unsigned int *version)
{
	unsigned int hardware, current;
	uint8_t flags = 0;

	if (!updateUseTouchService)
		return (int)0x800f0202;
	if (port)
		hardware = updateSysconTouchDeviceInfo[2];
	else
		hardware = updateSysconTouchDeviceInfo[0];
	if (TouchUtilGetConfigUpdateFlags(port, hardware, &flags)) {
		updateLastResult = ((uint8_t)port << 16) | 0x0a002011;
		return (int)0x800f0205;
	}
	if ((hardware & 0xc0c0) == 0xc0c0 ||
	    ((flags & 1) | (uint8_t)updateTouchRecoveryMode)) {
		*version = 0;
		return 0;
	}
	if (touchPanelOperations[port].getFirmwareVersion(&current) < 0)
		*version = 0;
	else
		*version = (uint16_t)(((current >> 8) & 255) | (current << 8));
	return 0;
}
