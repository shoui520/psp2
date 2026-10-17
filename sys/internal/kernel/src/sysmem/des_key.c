/* DES key expansion. No source-name record survives for these routines. */
#include <sysmem/des.h>
#include "des_internal.h"

int SceKernelUtilsForDriver_AE6C45FB(DesContext *context, uint64_t key)
{
	uint32_t low = key;
	uint32_t high = key >> 32;
	uint32_t left = 0;
	uint32_t right = 0;
	uint32_t rotations;
	uint32_t (*roundKey)[2] = context->roundKeys;

#if FW_SINCE(0x04000000)
	int count;

	for (count = 7; count != 0; count--) {
		uint32_t selected;

		high >>= 1;
		low >>= 1;
		selected = ((low & 0x01010101) + ((high & 0x01010101) << 4))
			* 0x01020408;
		left = (left << 8) + (right >> 24);
		right = (right << 8) + (selected >> 24);
	}
	left = __builtin_bswap32((left << 8) + (right >> 28));
	rotations = desKeyRotationSequence;
#else
	uint32_t position = 57;

	do {
		uint32_t selected;

		right = (right << 1) | (right >> 31);
		left = ((left << 1) & ~16u) | ((right & 1) << 4);
		selected = high >> (position & 31);
		if (!(position & 32))
			selected = low >> (position & 31);
		right = (right & ~16u) | ((selected & 1) << 4);
		position -= 8;
		position = (position + ((position >> 6) & 1)) & 63;
	} while (position != 0);
	left = __builtin_bswap32(left);
	left = ((left & 0xf000000f) | ((left & 0x00ffffff) << 4)) >> 4;
	right >>= 4;
	rotations = 0x6aa9aaa5;
#endif
	do {
		const uint16_t *selection = desKeySelection + 24;
		uint32_t shift = rotations & 3;

		left = (left & 0x0fffffff) | (left << 28);
		right = (right & 0x0fffffff) | (right << 28);
#if FW_SINCE(0x04000000)
		left >>= shift;
		right >>= shift;
#else
		left = (left >> shift) | (left << (32 - shift));
		right = (right >> shift) | (right << (32 - shift));
#endif
		do {
			uint32_t bit = *--selection;
			uint32_t lowShift = bit & 255;
			uint32_t highShift = bit >> 8;

#if FW_SINCE(0x04000000)
			lowShift--;
			highShift--;
#endif
			high = (high << 1) + ((right >> lowShift) & 1);
			low = (low << 1) + ((left >> highShift) & 1);
		} while (selection != desKeySelection);
#if FW_SINCE(0x04000000)
		{
			uint32_t packed = high << 8;
			uint32_t upper = low >> 12;

			packed = (packed & ~0x00fff000u) | ((high & 0xfff) << 12);
			packed = (packed & ~0x0000fff0u) | ((upper & 0xfff) << 4);
			high = (high & ~0x000003f0u) | ((high & 63) << 4);
			high <<= 14;
			high = (high & ~0x0000fc00u) | ((upper & 63) << 10);
			high = (high & ~0x000000fcu) | ((low & 63) << 2);
			packed = (packed & ~0x000000fcu) | (((low >> 6) & 63) << 2);
			roundKey[0][0] = packed;
			roundKey[0][1] = high;
		}
#else
		roundKey[0][0] = low;
		roundKey[0][1] = high;
#endif
		roundKey++;
		rotations >>= 2;
	} while (rotations != 0);
#if FW_SINCE(0x04000000)
	return 0;
#else
	return (uintptr_t)roundKey;
#endif
}
