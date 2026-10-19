/* Bluetooth crypto adapters; the original source filename is unavailable. */
#include <sysmem/aes.h>
#include "bt_internal.h"
#include <sysmem/utils.h>

extern const uint8_t gBtRandomKey[16];

SceAesFullContext gBtRandomContext;

int BtGenerateRandomBlock(uint8_t *destination)
{
	uint8_t input[16] = { 0 };
	SceInt64 time = BtGetSystemTimeWide();

	BtPack(input, 8, "8", time);
	SceKernelUtilsForDriver_F12B6451(&gBtRandomContext, 128, 128, gBtRandomKey);
	return SceKernelUtilsForDriver_C2A61770(&gBtRandomContext, input, destination);
}
