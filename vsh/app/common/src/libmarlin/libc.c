/*
 * The memset the compiler calls from SceLibMarlin's code: a byte loop of the library's own,
 * linked after the SDK libraries (the vectorizer peels to alignment and stores 16-byte blocks).
 */
#include <stddef.h>

void *memset(void *dest, int value, size_t size)
{
	unsigned char *to = dest;

	while (size--)
		*to++ = value;
	return dest;
}
