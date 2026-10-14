/* DES bit permutations, shared by the encryption and decryption paths. */
#include "des_internal.h"

uint64_t DesInitialPermutation(uint64_t block)
{
	uint32_t low = block;
	uint32_t high = block >> 32;
	uint32_t exchange;

	exchange = (low ^ (high >> 4)) & 0x0f0f0f0f;
	low ^= exchange;
	high ^= exchange << 4;
	exchange = (low ^ (high >> 16)) & 0xffff;
	low ^= exchange;
	high ^= exchange << 16;
	exchange = (high ^ (low >> 2)) & 0x33333333;
	high ^= exchange;
	low ^= exchange << 2;
	exchange = (high ^ (low >> 8)) & 0x00ff00ff;
	high ^= exchange;
	low ^= exchange << 8;
	exchange = (low ^ (high >> 1)) & 0x55555555;
	low ^= exchange;
	high ^= exchange << 1;
	return ((uint64_t)high << 32) | low;
}

uint64_t DesFinalPermutation(uint64_t block)
{
	uint32_t low = block;
	uint32_t high = block >> 32;
	uint32_t exchange;

	exchange = (low ^ (high >> 1)) & 0x55555555;
	low ^= exchange;
	high ^= exchange << 1;
	exchange = (high ^ (low >> 8)) & 0x00ff00ff;
	high ^= exchange;
	low ^= exchange << 8;
	exchange = (high ^ (low >> 2)) & 0x33333333;
	high ^= exchange;
	low ^= exchange << 2;
	exchange = (low ^ (high >> 16)) & 0xffff;
	low ^= exchange;
	high ^= exchange << 16;
	exchange = (low ^ (high >> 4)) & 0x0f0f0f0f;
	low ^= exchange;
	high ^= exchange << 4;
	return ((uint64_t)high << 32) | low;
}
