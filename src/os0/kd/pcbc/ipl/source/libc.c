/*
 * C library subset (loadcore's clib): NULL-tolerant string functions and
 * memset/memcpy with word fast paths.
 */

#include "ipl.h"

char *strncpy(char *dst, const char *src, SceSize n)
{
	int len = n;
	int i;

	if (dst == NULL || src == NULL)
		return NULL;

	for (i = 0; i < len; i++) {
		char c = src[i];

		dst[i] = c;
		if (c == '\0') {
			while (++i < len)
				dst[i] = '\0';
			break;
		}
	}
	return dst;
}

int memcmp(const void *a, const void *b, SceSize n)
{
	const u8 *p = a, *q = b;
	int len = n;

	for (; len > 0; len--, p++, q++) {
		if (*p != *q)
			return *q > *p ? -1 : 1;
	}
	return 0;
}

static void *memsetWords(u32 *dst, u32 v, SceSize n)
{
	u32 *p = dst, *end = (u32 *)((u8 *)dst + (n & ~3));

	if ((n & ~3) == 0)
		return dst;
	if (n & 4) {
		*p++ = v;
		if (p == end)
			return dst;
	}
	do {
		p[0] = v;
		p[1] = v;
		p += 2;
	} while (p != end);
	return dst;
}

void *memset(void *dst, int c, SceSize n)
{
	u8 *p = dst;
	int len = n;

	if (dst == NULL)
		return NULL;
	if (c == 0 && (((u32)dst | n) & 3) == 0)
		return memsetWords(dst, 0, n);

	for (; len > 0; len--)
		*p++ = c;
	return dst;
}

static void *memcpyWords(u32 *dst, const u32 *src, SceSize n)
{
	u32 *p = dst, *end = (u32 *)((u8 *)dst + (n & ~3));

	if ((n & ~3) == 0)
		return dst;
	if (n & 0xC) {
		u32 *head = (u32 *)((u8 *)dst + (n & 0xC));

		while (p != head)
			*p++ = *src++;
		if (p == end)
			return dst;
	}
	do {
		u32 a = src[0], b = src[1], c = src[2], d = src[3];

		p[0] = a;
		p[1] = b;
		p[2] = c;
		p[3] = d;
		p += 4;
		src += 4;
	} while (p != end);
	return dst;
}

void *memcpy(void *dst, const void *src, SceSize n)
{
	u8 *p = dst;
	const u8 *q = src;

	if (dst == NULL)
		return NULL;
	if ((((u32)dst | (u32)src | n) & 3) == 0)
		return memcpyWords(dst, src, n);

	for (; n != 0; n--)
		*p++ = *q++;
	return dst;
}

int strcmp(const char *a, const char *b)
{
	if (a == NULL || b == NULL) {
		if (a == b)
			return 0;
		return a == NULL ? -1 : 1;
	}
	for (; *a == *b; a++, b++) {
		if (*a == '\0')
			return 0;
	}
	return *a - *b;
}

