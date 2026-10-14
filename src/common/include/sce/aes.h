/* SceKernelUtils compact AES context, owned by SceSysmem.
 *
 * ksceAesInit2/3 select the 0x1f0-byte single-schedule representation;
 * ksceAesInit1 selects the 0x3d0-byte two-schedule representation.
 * Field names are inferred from the initializer's writes: key/block bit
 * lengths are divided by 32, and rounds is their maximum plus six.
 */
#pragma once

#include <stdint.h>

typedef struct SceAesCompactContext {
	uint32_t size;
	uint32_t keyWords;
	uint32_t blockWords;
	uint32_t rounds;
	uint32_t roundKeys[120];
} SceAesCompactContext;

typedef struct SceAesFullContext {
	SceAesCompactContext encryption;
	uint32_t decryptionRoundKeys[120];
} SceAesFullContext;

_Static_assert(sizeof(SceAesCompactContext) == 0x1f0,
	       "compact AES context ABI");
_Static_assert(sizeof(SceAesFullContext) == 0x3d0,
	       "full AES context ABI");
