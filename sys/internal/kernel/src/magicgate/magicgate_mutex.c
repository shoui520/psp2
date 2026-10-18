/* Descriptive file name: the original has no source-file debug records. */
#include <threadmgr/threadmgr.h>
#include "magicgate_private.h"

MagicGateDriver magicGateDriver;

int createDriverMutex(void)
{
	int result = sceKernelCreateMutex("SceMagicGateDrv", 2, 0, NULL);

	if (result >= 0) {
		magicGateDriver.mutex = result;
		result = 0;
	}
	return result;
}

int deleteDriverMutex(void)
{
	int result = magicGateDriver.mutex;

	if (result > 0) {
		result = sceKernelDeleteMutex(result);
		magicGateDriver.mutex = 0;
	}
	return result;
}

int SceMagicGateForDriver_A49B9D70(void)
{
	int result = sceKernelLockMutex(magicGateDriver.mutex, 1, NULL);

	if (result == (int)0x80024504)
		result = (int)0x80501001;
	else
		result &= result >> 31;
	return result;
}

int SceMagicGateForDriver_43518F19(void)
{
	int result = sceKernelTryLockMutex(magicGateDriver.mutex, 1);

	if (result == (int)0x80028145)
		return (int)0x80502001;
	if (result == (int)0x80024504)
		return (int)0x80501001;
	return result & (result >> 31);
}

int SceMagicGateForDriver_492EE950(void)
{
	return sceKernelUnlockMutex(magicGateDriver.mutex, 1);
}

int SceMagicGateForDriver_38AE3ACA(void)
{
	return clearSlotState();
}

int SceMagicGateForDriver_46A28FAA(void)
{
	int result = sceKernelLockMutex(magicGateDriver.mutex, 1, NULL);

	if (result == (int)0x80024504)
		return (int)0x80501001;
	if (result >= 0) {
		if (magicGateDriver.references <= 0) {
			magicGateDriver.references = 1;
			initializeSlots();
			result = loadDriverKeyTable();
			sceKernelUnlockMutex(magicGateDriver.mutex, 1);
			return result & (result >> 31);
		}
		++magicGateDriver.references;
		sceKernelUnlockMutex(magicGateDriver.mutex, 1);
		return 0;
	}
	return result;
}

void terminateDriver(void)
{
	clearSlotState();
	magicGateDriver.references = 0;
	sceKernelUnlockMutex(magicGateDriver.mutex, 1);
}

int SceMagicGateForDriver_6D3B42E6(void)
{
	int result = sceKernelLockMutex(magicGateDriver.mutex, 1, NULL);

	if (result < 0)
		return result;
	if (magicGateDriver.references > 1) {
		--magicGateDriver.references;
		return sceKernelUnlockMutex(magicGateDriver.mutex, 1);
	}
	clearSlotState();
	magicGateDriver.references = 0;
	return sceKernelUnlockMutex(magicGateDriver.mutex, 1);
}
