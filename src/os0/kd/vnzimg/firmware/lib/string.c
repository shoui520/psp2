/*
 * The C library's pieces (0x924f28-0x924f8e), byte at a time. The
 * soft-float and 64-bit helpers after them are libgcc's.
 */
#include "vnz.h"

/* 0x924f28 */
void *memset(void *destination, int value, size_t size)
{
	uint8_t *p = destination;

	while (size--)
		*p++ = value;
	return destination;
}

/* 0x924f3c */
int abs(int value)
{
	return value < 0 ? -value : value;
}

/* 0x924f48 */
void *memcpy(void *destination, const void *source, size_t size)
{
	uint8_t *to = destination;
	const uint8_t *from = source;

	while (size--)
		*to++ = *from++;
	return destination;
}

/* 0x924f5e: forwards unless the destination lies above the source */
void *memmove(void *destination, const void *source, size_t size)
{
	uint8_t *to = destination;
	const uint8_t *from = source;

	if (size == 0)
		return destination;
	if (to < from) {
		while (size--)
			*to++ = *from++;
	} else {
		to += size;
		from += size;
		while (size--)
			*--to = *--from;
	}
	return destination;
}
