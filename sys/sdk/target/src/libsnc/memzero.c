/*
 * memzero.c: __memzero (libsnc memzero.o), which SNC calls to clear an object.
 */
#include <string.h>

void __memzero(void *p, size_t n)
{
	memset(p, 0, n);
}
