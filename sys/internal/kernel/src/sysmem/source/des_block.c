/* DES block operations, using the original two-round loop. */
#include <sce/des.h>
#include "des_internal.h"

uint64_t SceKernelUtilsForDriver_A0D45D25(const DesContext *context, uint64_t block)
{
	const uint32_t (*roundKey)[2] = context->roundKeys;
	const uint32_t (*end)[2] = context->roundKeys + 16;
	uint32_t left;
	uint32_t right;

	block = DesInitialPermutation(block);
	left = block;
	right = block >> 32;
	do {
#if FW_SINCE(0x04000000)
		uint32_t first = roundKey[0][0];
		uint32_t second = roundKey[0][1];

		roundKey += 2;
		first ^= (left >> 1) | (left << 31);
		second ^= (left >> 29) | (left << 3);
		right ^= DesRound(first, second);
		first = roundKey[-1][0] ^ ((right >> 1) | (right << 31));
		second = roundKey[-1][1] ^ ((right >> 29) | (right << 3));
		left ^= DesRound(first, second);
#else
		right ^= DesRound(left, roundKey[0]);
		left ^= DesRound(right, roundKey[1]);
		roundKey += 2;
#endif
	} while (roundKey != end);
	return DesFinalPermutation(((uint64_t)left << 32) | right);
}

uint64_t SceKernelUtilsForDriver_801D0ED4(const DesContext *context, uint64_t block)
{
	const uint32_t (*roundKey)[2] = context->roundKeys + 16;
	uint32_t left;
	uint32_t right;

	block = DesInitialPermutation(block);
	left = block;
	right = block >> 32;
	do {
#if FW_SINCE(0x04000000)
		uint32_t first;
		uint32_t second;
#endif

		roundKey -= 2;
#if FW_SINCE(0x04000000)
		first = roundKey[1][0] ^ ((left >> 1) | (left << 31));
		second = roundKey[1][1] ^ ((left >> 29) | (left << 3));

		right ^= DesRound(first, second);
		first = roundKey[0][0] ^ ((right >> 1) | (right << 31));
		second = roundKey[0][1] ^ ((right >> 29) | (right << 3));
		left ^= DesRound(first, second);
#else
		right ^= DesRound(left, roundKey[1]);
		left ^= DesRound(right, roundKey[0]);
#endif
	} while (roundKey != context->roundKeys);
	return DesFinalPermutation(((uint64_t)left << 32) | right);
}
