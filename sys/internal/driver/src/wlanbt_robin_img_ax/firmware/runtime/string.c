/*
 * The C library's Thumb string routines (memcmp 0x52234, strcpy 0x52360): a word at a time while both
 * pointers are aligned, a byte at a time otherwise. The filename is inferred.
 */
#include <stdint.h>
#include "robin/runtime.h"

/* A word has a zero byte: some byte borrows when 0x01 is taken from each byte. */
#define WORD_ONES	0x01010101u
#define WORD_HAS_ZERO(w)	((((w) - WORD_ONES) & ~(w)) & (WORD_ONES << 7))

int memcmp(const void *left, const void *right, size_t size)
{
	const uint8_t *a = left;
	const uint8_t *b = right;

	if (size >= 4 && (((uintptr_t)a | (uintptr_t)b) & 3) == 0) {
		uint32_t wa, wb;

		do {
			wa = *(const uint32_t *)a;
			wb = *(const uint32_t *)b;
			a += 4;
			b += 4;
			if (wa != wb)
				break;
			size -= 4;
		} while (size >= 4);
		if (wa != wb) {
			a -= 4;
			b -= 4;
		}
	}
	while (size != 0) {
		if (*a != *b)
			return *a - *b;
		a++;
		b++;
		size--;
	}
	return 0;
}

char *strcpy(char *destination, const char *source)
{
	char *d = destination;

	if ((((uintptr_t)destination | (uintptr_t)source) & 3) == 0) {
		const uint32_t *s = (const uint32_t *)source;
		uint32_t word;

		for (;;) {
			word = *s++;
			if (WORD_HAS_ZERO(word))
				break;
			*(uint32_t *)d = word;
			d += 4;
		}
		for (;;) {
			*d++ = (char)word;
			if ((uint8_t)word == 0)
				break;
			word >>= 8;
		}
		return destination;
	}
	while ((*d++ = *source++) != 0)
		;
	return destination;
}
