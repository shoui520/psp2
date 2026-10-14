/*
 * The C library's memory copy and fill, a byte at a time (PU1's, FW2's codec's).
 */
#include <vip.h>

void *memcpy(void *destination, const void *source, size_t size)
{
	uint8_t *to = destination;
	const uint8_t *from = source;

	while (size--)
		*to++ = *from++;
	return destination;
}

void *memset(void *destination, int value, size_t size)
{
	uint8_t *to = destination;

	while (size--)
		*to++ = value;
	return destination;
}
