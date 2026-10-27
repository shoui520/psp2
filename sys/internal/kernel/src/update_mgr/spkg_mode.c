#include <sysmem/sysclib.h>
#include "update_internal.h"
#include <syscon/syscon.h>
#include "spkg.h"

int SpkgUtilGetUpdateMode(SceUpdateMode *mode)
{
	SceUpdateMode value;
	int result;

	SceKernelSuspendForDriver_4DF40893(0);
	if (!mode) {
		SceKernelSuspendForDriver_2BB92967(0);
		return (int)0x800f0216;
	}
	if (!updateUseSysconNvsService) {
		SceKernelSuspendForDriver_2BB92967(0);
		return 0;
	}
	value = 0;
	result = sceSysconNvsReadData(1184, &value, 1);
	if (result) {
		SceKernelSuspendForDriver_2BB92967(0);
		return (int)0x800f0205;
	}
	*mode = value;
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int SpkgUtilSetUpdateMode(SceUpdateMode mode)
{
	SceUpdateMode value, readback;
	int tries, result;

	SceKernelSuspendForDriver_4DF40893(0);
	if (mode == 255)
		SpkgUtilApplyMBR();
	if (!updateUseSysconNvsService) {
		SceKernelSuspendForDriver_2BB92967(0);
		return 0;
	}
	tries = 5;
	for (;;) {
		value = 0;
		if (!sceSysconNvsReadData(1184, &value, 1)) {
			value = mode;
			if (!sceSysconNvsWriteData(1184, &value, 1)) {
				readback = 0;
				result = sceSysconNvsReadData(1184, &readback, 1);
				if (!result && !timingsafe_memcmp(&value, &readback, 1))
					break;
			}
		}
		if (!--tries) {
			result = (int)0x800f0205;
			break;
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}
