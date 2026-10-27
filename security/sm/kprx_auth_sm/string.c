/*
 * string.c: freestanding memory, constant-time byte comparison, and libgcc
 * integer multiply/divide/modulo primitives (0x8102aa..0x810438).
 *
 * Internal names timingsafeBcmp and byteMemset are ours.
 */

#include "kprx_auth_sm.h"

/* 0x8102aa */
void *memset(void *dst, int val, size_t len)
{
	uint8_t byteVal = (uint8_t)val;
	uint8_t *endPtr = (uint8_t *)dst + len;
	uint8_t *curPtr = (uint8_t *)dst;

	if (len > 7u) {
		uint16_t halfVal = ((uint16_t)byteVal << 8) | (uint16_t)byteVal;
		uint32_t wordVal = ((uint32_t)halfVal << 16) | (uint32_t)halfVal;
		uint16_t *halfPtr = (uint16_t *)(((uintptr_t)dst + 1u) & ~1u);
		uint32_t *wordPtr;
		uint32_t *wordEnd = (uint32_t *)((uintptr_t)endPtr & ~3u);

		*curPtr = byteVal;
		*halfPtr = halfVal;
		for (wordPtr = (uint32_t *)(((uintptr_t)(halfPtr + 1) & ~3u));
		     wordPtr < wordEnd;
		     wordPtr++) {
			*wordPtr = wordVal;
		}
		curPtr = (uint8_t *)wordPtr;
	}

	while (curPtr < endPtr)
		*curPtr++ = byteVal;

	return dst;
}

/* 0x810300 */
void *memcpy(void *dst, const void *src, size_t len)
{
	if ((((uintptr_t)src | (uintptr_t)dst | (uintptr_t)len) & 3u) == 0u) {
		uint32_t *dWords = (uint32_t *)dst;
		const uint32_t *sWords = (const uint32_t *)src;
		size_t wordCount = len >> 2;
		size_t i;

		for (i = 0u; i < wordCount; i++)
			dWords[i] = sWords[i];
	} else {
		uint8_t *dBytes = (uint8_t *)dst;
		const uint8_t *sBytes = (const uint8_t *)src;
		size_t i;

		for (i = 0u; i < len; i++)
			dBytes[i] = sBytes[i];
	}
	return dst;
}

/* 0x81033c (name ours) */
int timingsafeBcmp(const void *b1, const void *b2, size_t len)
{
	const uint8_t *p1 = (const uint8_t *)b1;
	const uint8_t *p2 = (const uint8_t *)b2;
	uint32_t diff = 0u;
	size_t i;

	for (i = 0u; i < len; i++)
		diff |= (uint32_t)(p1[i] ^ p2[i]);
	return (int)diff;
}

/* 0x810356 (name ours) */
void *byteMemset(void *dst, int val, size_t len)
{
	uint8_t *p = (uint8_t *)dst;
	uint8_t b = (uint8_t)val;
	size_t i;

	for (i = 0u; i < len; i++)
		p[i] = b;
	return dst;
}

/* 0x81036a */
uint32_t __mulsi3(uint32_t a, uint32_t b)
{
	uint32_t res = 0u;

	while (a != 0u) {
		if ((a & 1u) != 0u)
			res += b;
		a >>= 1;
		b <<= 1;
	}
	return res;
}

/* 0x810384 */
uint32_t udivmodsi4(uint32_t num, uint32_t den, int modwanted)
{
	uint32_t bit = 1u;
	uint32_t res = 0u;

	while (den < num && bit != 0u && (den & (1u << 31)) == 0u) {
		den <<= 1;
		bit <<= 1;
	}
	while (bit != 0u) {
		if (num >= den) {
			num -= den;
			res |= bit;
		}
		bit >>= 1;
		den >>= 1;
	}
	return modwanted ? num : res;
}

/* 0x8103ca */
int32_t __divsi3(int32_t a, int32_t b)
{
	int neg = 0;
	int32_t res;

	if (a < 0) {
		a = -a;
		neg = 1;
	}
	if (b < 0) {
		b = -b;
		neg ^= 1;
	}
	res = (int32_t)udivmodsi4((uint32_t)a, (uint32_t)b, 0);
	if (neg)
		res = -res;
	return res;
}

/* 0x8103fc */
int32_t __modsi3(int32_t a, int32_t b)
{
	int neg = 0;
	int32_t res;

	if (a < 0) {
		a = -a;
		neg = 1;
	}
	if (b < 0)
		b = -b;
	res = (int32_t)udivmodsi4((uint32_t)a, (uint32_t)b, 1);
	if (neg)
		res = -res;
	return res;
}

/* 0x81042c */
uint32_t __udivsi3(uint32_t a, uint32_t b)
{
	return udivmodsi4(a, b, 0);
}

/* 0x810432 */
uint32_t __umodsi3(uint32_t a, uint32_t b)
{
	return udivmodsi4(a, b, 1);
}
