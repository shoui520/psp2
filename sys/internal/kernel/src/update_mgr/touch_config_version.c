#include "update_internal.h"

int TouchUtilGetConfigVersion(unsigned int port, uint32_t *version)
{
	unsigned int current;

	if (!updateUseTouchService)
		return (int)0x800f0202;
	if (port == 0) {
		current = updateSysconTouchDeviceInfo[1];
		if ((updateSysconTouchDeviceInfo[0] & 0xf000) == 0x8000)
			goto shiftedVersion;
	} else {
		current = updateSysconTouchDeviceInfo[3];
		if ((updateSysconTouchDeviceInfo[2] & 0xf000) == 0x8000)
			goto shiftedVersion;
	}
	*version = current;
	return 0;
shiftedVersion:
	*version = current >> 8;
	return 0;
}
