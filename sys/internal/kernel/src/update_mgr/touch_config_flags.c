#include "update_internal.h"
#include "touch_dispatch.h"

int TouchUtilGetConfigUpdateFlags(int port, unsigned int hardwareInfo,
	uint8_t *flags)
{
	uint8_t value = 0;
	int result;

	if (updateSysconFirmwareVersion > 0x01030100)
		return touchPanelOperations[port].getConfigUpdateFlags(flags);
	if (hardwareInfo == 0x800a) {
		result = touchPanelOperations[port].readConfigData(101, &value, 1);
		if (result == 0 && (value & 8))
			*flags |= 1;
	} else {
		result = 0;
		*flags = 0;
	}
	return result;
}
