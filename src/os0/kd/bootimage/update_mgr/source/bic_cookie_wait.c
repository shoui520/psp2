#include "update_internal.h"

extern unsigned int updateSysconFirmwareVersion;
int SceSysconForDriver_DFB024C4(uint32_t *status);

int BicUtilWaitCookieStatus(void)
{
	uint32_t status = 0;
	int tries, result;

	if (updateSysconFirmwareVersion <= 0x1020006)
		return sceKernelDelayThread(4000000);
	for (tries = 4; tries; tries--) {
		sceKernelDelayThread(1000000);
		result = SceSysconForDriver_DFB024C4(&status);
		if (status & 0x40)
			break;
	}
	return result;
}
