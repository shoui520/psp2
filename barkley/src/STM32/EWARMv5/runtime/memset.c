/*
 * memset: the AEABI's fill (its arguments in another order).
 */

#include <string.h>

void __aeabi_memset(void *dst, size_t n, int c);

void *memset(void *dst, int c, size_t n)
{
	__aeabi_memset(dst, n, c);
	return dst;
}
