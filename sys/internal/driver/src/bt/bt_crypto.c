/* Bluetooth crypto adapters; the original source filename is unavailable. */
#include <sysmem/aes.h>
#include "bt_internal.h"

int ksceAesInit1(SceAesFullContext *context, unsigned int blockBits,
		 unsigned int keyBits, const void *key);
int ksceAesEncrypt1(SceAesFullContext *context, const void *source,
		    void *destination);
int BtPack(uint8_t *destination, int remaining, const char *format, ...);
extern const uint8_t gBtRandomKey[16];

SceAesFullContext gBtRandomContext;

int BtGenerateRandomBlock(uint8_t *destination)
{
	uint8_t input[16] = { 0 };
	SceInt64 time = BtGetSystemTimeWide();

	BtPack(input, 8, "8", time);
	ksceAesInit1(&gBtRandomContext, 128, 128, gBtRandomKey);
	return ksceAesEncrypt1(&gBtRandomContext, input, destination);
}
