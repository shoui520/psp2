/*
 * string.c: freestanding memory and string utilities (strncmp, strnlen,
 * word-aligned memcpy, overlapping memmove, and constant-time memory compare)
 * (0x80b258..0x80b32b).
 *
 * Function names follow standard C / BSD conventions; sl* aliases are ours.
 */

#include "second_loader.h"

/* 0x80b258 */
int slStrncmp(const char *s1, const char *s2, size_t n)
{
	while (n != 0) {
		char c1 = *s1;
		char c2 = *s2;

		if (c2 != c1)
			return ((uint8_t)c1 < (uint8_t)c2) ? -1 : 1;
		if (c2 == '\0')
			return 0;
		s1++;
		s2++;
		n--;
	}
	return 0;
}

/* 0x80b284 */
size_t slStrnlen(const char *s, size_t maxlen)
{
	size_t len = 0;

	if (s == NULL)
		return 0;

	while (maxlen != 0) {
		if (*s == '\0')
			break;
		len++;
		s++;
		maxlen--;
	}
	return len;
}

/* 0x80b29e */
void *memcpy(void *dst, const void *src, size_t n)
{
	size_t i;

	if ((((uintptr_t)src | (uintptr_t)dst | n) & 3u) == 0) {
		uint32_t *d32 = (uint32_t *)dst;
		const uint32_t *s32 = (const uint32_t *)src;
		size_t words = n >> 2;

		for (i = 0; i < words; i++)
			d32[i] = s32[i];
	} else {
		uint8_t *d8 = (uint8_t *)dst;
		const uint8_t *s8 = (const uint8_t *)src;

		for (i = 0; i < n; i++)
			d8[i] = s8[i];
	}
	return dst;
}

void *slMemcpy(void *dst, const void *src, size_t n) __attribute__((alias("memcpy")));

/* 0x80b2da */
void *slMemmove(void *dst, const void *src, size_t n)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	if (n != 0) {
		if (d < s) {
			do {
				*d++ = *s++;
				n--;
			} while (n != 0);
		} else if (s < d) {
			d += n;
			s += n;
			do {
				*--d = *--s;
				n--;
			} while (n != 0);
		}
	}
	return dst;
}

/* 0x80b312 */
int slTimingsafeBcmp(const void *b1, const void *b2, size_t n)
{
	const uint8_t *p1 = (const uint8_t *)b1;
	const uint8_t *p2 = (const uint8_t *)b2;
	uint32_t diff = 0;

	while (n != 0) {
		diff |= (uint32_t)((uint8_t)(*p1++ ^ *p2++));
		n--;
	}
	return (int)diff;
}
