/*
 * The C library's ARM string routines (__aeabi_memmove 0x5228c, strncpy 0x523b0, strcmp 0x52420).
 * memmove copies from the end when the destination overlaps the source from above, and hands every
 * other case to memcpy. The filename is inferred.
 */
#include <stdint.h>
#include "robin/rom.h"
#include "robin/runtime.h"

#define WORD_ONES	0x01010101u
#define WORD_HAS_ZERO(w)	((((w) - WORD_ONES) & ~(w)) & (WORD_ONES << 7))

void __aeabi_memmove(void *dest, const void *source, size_t size)
{
	uint8_t *d;
	const uint8_t *s;

	if ((uintptr_t)dest < (uintptr_t)source ||
	    size <= (uintptr_t)dest - (uintptr_t)source) {
		__aeabi_memcpy(dest, source, size);
		return;
	}
	d = (uint8_t *)dest + size;
	s = (const uint8_t *)source + size;
	if (size > 3) {
		while ((uintptr_t)d & 3) {
			*--d = *--s;
			size--;
		}
		if (((uintptr_t)s & 3) == 0) {
			/* Both aligned: four words, then a word, then the bytes. */
			while (size >= 16) {
				d -= 16;
				s -= 16;
				((uint32_t *)d)[3] = ((const uint32_t *)s)[3];
				((uint32_t *)d)[2] = ((const uint32_t *)s)[2];
				((uint32_t *)d)[1] = ((const uint32_t *)s)[1];
				((uint32_t *)d)[0] = ((const uint32_t *)s)[0];
				size -= 16;
			}
			while (size >= 4) {
				d -= 4;
				s -= 4;
				*(uint32_t *)d = *(const uint32_t *)s;
				size -= 4;
			}
		} else {
			/* The source is not aligned: each word is made of two aligned source words. */
			unsigned int shift = ((uintptr_t)s & 3) * 8;
			const uint32_t *w = (const uint32_t *)((uintptr_t)s & ~3u);
			uint32_t high = *w;

			while (size >= 4) {
				uint32_t low = *--w;

				d -= 4;
				*(uint32_t *)d = (high << (32 - shift)) | (low >> shift);
				high = low;
				size -= 4;
			}
			s = (const uint8_t *)w + shift / 8;
		}
	}
	while (size != 0) {
		*--d = *--s;
		size--;
	}
}

char *strncpy(char *destination, const char *source, size_t size)
{
	char *d = destination;
	int left = (int)size;

	if ((((uintptr_t)destination | (uintptr_t)source) & 3) == 0) {
		const uint32_t *s = (const uint32_t *)source;

		while ((left -= 4) >= 0) {
			uint32_t word = *s;

			if (WORD_HAS_ZERO(word))
				break;
			*(uint32_t *)d = word;
			d += 4;
			s++;
		}
		left += 4;
		source = (const char *)s;
	}
	while (--left >= 0) {
		char c = *source++;

		*d++ = c;
		if (c == 0) {
			__aeabi_memclr(d, (size_t)left);
			break;
		}
	}
	return destination;
}

int strcmp(const char *left, const char *right)
{
	const uint8_t *a = (const uint8_t *)left;
	const uint8_t *b = (const uint8_t *)right;
	unsigned int ca, cb;

	if ((((uintptr_t)a | (uintptr_t)b) & 3) == 0) {
		uint32_t wa, wb;

		do {
			wa = *(const uint32_t *)a;
			wb = *(const uint32_t *)b;
			a += 4;
			b += 4;
		} while (!WORD_HAS_ZERO(wa) && wa == wb);
		/* The first byte that differs or ends the string decides, as in the byte loop. */
		a -= 4;
		b -= 4;
	}
	do {
		ca = *a++;
		cb = *b++;
	} while (ca != 0 && ca == cb);
	return (int)(ca - cb);
}
