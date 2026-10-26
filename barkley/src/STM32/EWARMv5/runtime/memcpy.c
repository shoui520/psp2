/*
 * memcpy: the AEABI's copy.
 */

#include <string.h>

void __aeabi_memcpy(void *dst, const void *src, size_t n);

void *memcpy(void *dst, const void *src, size_t n)
{
	__aeabi_memcpy(dst, src, n);
	return dst;
}
