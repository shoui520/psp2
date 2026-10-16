#include "update_internal.h"

int MotionUtilGetFirmwareVersion(uint32_t *version)
{
	if (!updateMotionConnected)
		return (int)0x800f0202;
	if (updateMotionFirmwareVersion == 0xffff)
		*version = 0;
	else
		*version = updateMotionFirmwareVersion;
	return 0;
}

int MotionMagnUtilGetFirmwareVersion(uint32_t *version)
{
	if (!updateMotionConnected)
		return (int)0x800f0202;
	if (updateMotionFirmwareVersion == 0xffff)
		*version = 0;
	else
		*version = updateMotionFirmwareVersion;
	return 0;
}
