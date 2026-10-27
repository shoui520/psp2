/*
 * string.c: Shared freestanding memset and memcpy primitives for PlayStation
 * Vita os0:sm secure modules (Toshiba MeP C5).
 */

#include "sm_common.h"

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

/* (name ours) */
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

/* (name ours) */
void *byteMemset(void *dst, int val, size_t len)
{
	uint8_t *p = (uint8_t *)dst;
	uint8_t b = (uint8_t)val;
	size_t i;

	for (i = 0u; i < len; i++)
		p[i] = b;
	return dst;
}
